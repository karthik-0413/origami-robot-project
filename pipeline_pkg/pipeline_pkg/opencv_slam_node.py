#!/usr/bin/env python3
import rclpy
from rclpy.node import Node

from sensor_msgs.msg import Image, CameraInfo
from nav_msgs.msg import Odometry, Path
from geometry_msgs.msg import PoseStamped, TransformStamped
from tf2_ros import TransformBroadcaster

from cv_bridge import CvBridge
import cv2
import numpy as np
import math


class StereoVO(Node):
    def __init__(self):
        super().__init__('stereo_vo')

        # === ROS Subscribers ===
        self.create_subscription(
            Image,
            '/left_camera/image_rect',
            self.left_image_cb,
            10
        )

        self.create_subscription(
            Image,
            '/right_camera/image_rect',
            self.right_image_cb,
            10
        )

        self.create_subscription(
            CameraInfo,
            '/left_camera/camera_info_rect',
            self.left_info_cb,
            10
        )

        # Optional right camera info, not needed for this VO
        self.create_subscription(
            CameraInfo,
            '/right_camera/camera_info_rect',
            lambda msg: None,
            10
        )
        
        # TF Broadcaster
        self.tf_broadcaster = TransformBroadcaster(self)

        # === Publishers ===
        self.odom_pub = self.create_publisher(Odometry, '/stereo_vo/odom', 10)
        self.path_pub = self.create_publisher(Path, '/stereo_vo/path', 10)
        self.pose_pub = self.create_publisher(PoseStamped, '/stereo_vo/pose', 10)

        self.path_msg = Path()
        self.path_msg.header.frame_id = "map"

        self.bridge = CvBridge()

        # === Camera Intrinsics ===
        self.fx = None
        self.fy = None
        self.cx = None
        self.cy = None
        self.baseline = 0.06  # meters

        # === Image Buffers ===
        self.left_img = None
        self.right_img = None

        # === ORB Feature Extractor ===
        self.orb = cv2.ORB_create(2000)
        self.bf = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)

        # === VO State ===
        self.prev_left_gray = None
        self.prev_pts_3d = None
        self.prev_pts_2d = None

        # === World Pose (4x4) in OpenCV camera convention ===
        self.T_wc = np.eye(4, dtype=np.float32)
        
    # TransformBroadcaster helper
    def publish_tf(self, T, stamp):
        tf = TransformStamped()
        tf.header.stamp = stamp
        tf.header.frame_id = "odom"
        tf.child_frame_id = "base_link"

        tf.transform.translation.x = float(T[0, 3])
        tf.transform.translation.y = float(T[1, 3])
        tf.transform.translation.z = float(T[2, 3])

        qw = math.sqrt(max(0.0, 1.0 + T[0,0] + T[1,1] + T[2,2])) / 2.0
        qx = (T[2,1] - T[1,2]) / (4.0 * qw)
        qy = (T[0,2] - T[2,0]) / (4.0 * qw)
        qz = (T[1,0] - T[0,1]) / (4.0 * qw)

        tf.transform.rotation.x = float(qx)
        tf.transform.rotation.y = float(qy)
        tf.transform.rotation.z = float(qz)
        tf.transform.rotation.w = float(qw)

        self.tf_broadcaster.sendTransform(tf)


    # ======================================================
    # CALLBACKS
    # ======================================================

    def left_info_cb(self, msg: CameraInfo):
        # Load intrinsics once
        if self.fx is None:
            self.fx = msg.k[0]
            self.fy = msg.k[4]
            self.cx = msg.k[2]
            self.cy = msg.k[5]
            self.get_logger().info(
                f"Loaded intrinsics: fx={self.fx:.3f}, fy={self.fy:.3f}, "
                f"cx={self.cx:.3f}, cy={self.cy:.3f}"
            )

    def left_image_cb(self, msg: Image):
        self.left_img = self.bridge.imgmsg_to_cv2(msg, 'bgr8')
        self.process()

    def right_image_cb(self, msg: Image):
        self.right_img = self.bridge.imgmsg_to_cv2(msg, 'bgr8')

    # ======================================================
    # MAIN PROCESSING
    # ======================================================

    def process(self):
        if self.left_img is None or self.right_img is None:
            return
        if self.fx is None:
            return

        left_gray = cv2.cvtColor(self.left_img, cv2.COLOR_BGR2GRAY)
        right_gray = cv2.cvtColor(self.right_img, cv2.COLOR_BGR2GRAY)

        # First frame: bootstrap from stereo
        if self.prev_left_gray is None:
            self.bootstrap_stereo(left_gray, right_gray)
            return

        # Track features with optical flow
        next_pts, status, _ = cv2.calcOpticalFlowPyrLK(
            self.prev_left_gray,
            left_gray,
            self.prev_pts_2d.reshape(-1, 1, 2),
            None
        )

        good_prev_3d = self.prev_pts_3d[status.flatten() == 1]
        good_curr_2d = next_pts[status.flatten() == 1]

        if len(good_prev_3d) < 20:
            self.get_logger().warn("Tracking lost — reinitializing stereo bootstrap")
            self.bootstrap_stereo(left_gray, right_gray)
            return

        # Camera matrix
        K = np.array([
            [self.fx, 0.0,      self.cx],
            [0.0,     self.fy,  self.cy],
            [0.0,     0.0,      1.0]
        ], dtype=np.float32)

        # PnP to get relative motion
        ok, rvec, tvec, inliers = cv2.solvePnPRansac(
            good_prev_3d,
            good_curr_2d,
            K,
            None,
            flags=cv2.SOLVEPNP_ITERATIVE
        )

        if not ok:
            self.get_logger().warn("PnP failed — reinitializing stereo bootstrap")
            self.bootstrap_stereo(left_gray, right_gray)
            return

        # Convert rvec/tvec to transform T_prev_curr
        R, _ = cv2.Rodrigues(rvec)
        T_prev_curr = np.eye(4, dtype=np.float32)
        T_prev_curr[:3, :3] = R
        T_prev_curr[:3, 3] = tvec.flatten()

        # Integrate global pose in OpenCV camera convention
        self.T_wc = self.T_wc @ np.linalg.inv(T_prev_curr)

        # Alignment: OpenCV camera -> ROS (map/base_link) convention
        # OpenCV cam: X right, Y down, Z forward
        # ROS base_link/map (we'll treat pose as in map frame):
        # X forward, Y left, Z up
        R_align = np.array([
            [0, 0, 1],
            [-1, 0, 0],
            [0, -1, 0]
        ], dtype=np.float32)

        T = self.T_wc.copy()
        T_correct = np.eye(4, dtype=np.float32)
        T_correct[:3, :3] = R_align @ T[:3, :3]
        T_correct[:3, 3] = R_align @ T[:3, 3]

        # Publish corrected pose
        self.publish_odom(T_correct)
        self.publish_path(T_correct)
        self.publish_pose(T_correct)
        self.publish_tf(T_correct, self.left_img.header.stamp if hasattr(self.left_img, 'header') else self.get_clock().now().to_msg())

        # Re-bootstrap from current stereo pair
        self.bootstrap_stereo(left_gray, right_gray)

    # ======================================================
    # STEREO BOOTSTRAP (3D point initialization)
    # ======================================================

    def bootstrap_stereo(self, left_gray, right_gray):
        kpL, desL = self.orb.detectAndCompute(left_gray, None)
        kpR, desR = self.orb.detectAndCompute(right_gray, None)

        if desL is None or desR is None:
            return

        matches = self.bf.match(desL, desR)
        matches = sorted(matches, key=lambda m: m.distance)[:500]

        pts_3d = []
        pts_2d = []

        for m in matches:
            uL, vL = kpL[m.queryIdx].pt
            uR, vR = kpR[m.trainIdx].pt

            disparity = uL - uR
            if disparity <= 1.0:
                continue

            Z = self.fx * self.baseline / disparity
            X = (uL - self.cx) * Z / self.fx
            Y = (vL - self.cy) * Z / self.fy

            pts_3d.append([X, Y, Z])
            pts_2d.append([uL, vL])

        if len(pts_3d) < 20:
            return

        self.prev_left_gray = left_gray
        self.prev_pts_3d = np.array(pts_3d, dtype=np.float32)
        self.prev_pts_2d = np.array(pts_2d, dtype=np.float32)

    # ======================================================
    # PUBLISHERS
    # ======================================================

    def publish_odom(self, T: np.ndarray):
        odom = Odometry()
        odom.header.frame_id = "map"
        odom.child_frame_id = "base_link"
        odom.header.stamp = self.get_clock().now().to_msg()

        # Position
        odom.pose.pose.position.x = float(T[0, 3])
        odom.pose.pose.position.y = float(T[1, 3])
        odom.pose.pose.position.z = float(T[2, 3])

        # Orientation (rotation matrix → quaternion)
        qw = math.sqrt(max(0.0, 1.0 + T[0, 0] + T[1, 1] + T[2, 2])) / 2.0
        qx = (T[2, 1] - T[1, 2]) / (4.0 * qw)
        qy = (T[0, 2] - T[2, 0]) / (4.0 * qw)
        qz = (T[1, 0] - T[0, 1]) / (4.0 * qw)

        odom.pose.pose.orientation.x = float(qx)
        odom.pose.pose.orientation.y = float(qy)
        odom.pose.pose.orientation.z = float(qz)
        odom.pose.pose.orientation.w = float(qw)

        self.odom_pub.publish(odom)

    def publish_path(self, T: np.ndarray):
        pose = PoseStamped()
        pose.header.frame_id = "map"
        pose.header.stamp = self.get_clock().now().to_msg()

        pose.pose.position.x = float(T[0, 3])
        pose.pose.position.y = float(T[1, 3])
        pose.pose.position.z = float(T[2, 3])

        qw = math.sqrt(max(0.0, 1.0 + T[0, 0] + T[1, 1] + T[2, 2])) / 2.0
        qx = (T[2, 1] - T[1, 2]) / (4.0 * qw)
        qy = (T[0, 2] - T[2, 0]) / (4.0 * qw)
        qz = (T[1, 0] - T[0, 1]) / (4.0 * qw)

        pose.pose.orientation.x = float(qx)
        pose.pose.orientation.y = float(qy)
        pose.pose.orientation.z = float(qz)
        pose.pose.orientation.w = float(qw)

        self.path_msg.poses.append(pose)
        self.path_pub.publish(self.path_msg)

    def publish_pose(self, T: np.ndarray):
        pose = PoseStamped()
        pose.header.frame_id = "map"
        pose.header.stamp = self.get_clock().now().to_msg()

        pose.pose.position.x = float(T[0, 3])
        pose.pose.position.y = float(T[1, 3])
        pose.pose.position.z = float(T[2, 3])

        qw = math.sqrt(max(0.0, 1.0 + T[0, 0] + T[1, 1] + T[2, 2])) / 2.0
        qx = (T[2, 1] - T[1, 2]) / (4.0 * qw)
        qy = (T[0, 2] - T[2, 0]) / (4.0 * qw)
        qz = (T[1, 0] - T[0, 1]) / (4.0 * qw)

        pose.pose.orientation.x = float(qx)
        pose.pose.orientation.y = float(qy)
        pose.pose.orientation.z = float(qz)
        pose.pose.orientation.w = float(qw)

        self.pose_pub.publish(pose)


def main(args=None):
    rclpy.init(args=args)
    node = StereoVO()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
