#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile

from nav_msgs.msg import Odometry
import numpy as np
import cv2
import math

class VoVisualizer(Node):
    def __init__(self):
        super().__init__("vo_visualizer")

        self.window_size = 800
        self.pad = 50
        self.path_points = []   # (x, y, yaw)

        qos = QoSProfile(depth=10)
        self.sub = self.create_subscription(
            Odometry,
            "/stereo_vo/odom",
            self.odom_callback,
            qos
        )

        cv2.namedWindow("VO Path", cv2.WINDOW_NORMAL)
        self.get_logger().info("VO Visualizer started.")

    # -------------------------------
    # Quaternion → yaw (2D heading)
    # -------------------------------
    def quat_to_yaw(self, q):
        # yaw (z-axis rotation)
        siny_cosp = 2.0 * (q.w * q.z + q.x * q.y)
        cosy_cosp = 1.0 - 2.0 * (q.y*q.y + q.z*q.z)
        return math.atan2(siny_cosp, cosy_cosp)

    def compute_yaw_from_motion(self):
        if len(self.path_points) < 2:
            return 0.0

        x1, y1, _ = self.path_points[-2]
        x2, y2, _ = self.path_points[-1]

        return math.atan2(y2 - y1, x2 - x1)

    # -------------------------------
    # Odometry callback
    # -------------------------------
    def odom_callback(self, msg):
        x = msg.pose.pose.position.x
        y = msg.pose.pose.position.y
        q = msg.pose.pose.orientation
        yaw = self.quat_to_yaw(q)

        self.path_points.append((x, y, yaw))
        self.draw_path()

    # -------------------------------
    # Draw path + robot heading arrow
    # -------------------------------
    def draw_path(self):
        if len(self.path_points) < 2:
            blank = np.zeros((self.window_size, self.window_size, 3), dtype=np.uint8)
            cv2.imshow("VO Path", blank)
            cv2.waitKey(1)
            return

        w = self.window_size
        pad = self.pad

        img = np.zeros((w, w, 3), dtype=np.uint8)

        # Convert to numpy array for min/max
        pts = np.array([(p[0], p[1]) for p in self.path_points], dtype=np.float32)

        min_x, max_x = pts[:,0].min(), pts[:,0].max()
        min_y, max_y = pts[:,1].min(), pts[:,1].max()

        sx = max_x - min_x or 1.0
        sy = max_y - min_y or 1.0

        scale = min((w - pad*2) / sx, (w - pad*2) / sy)

        # Draw trajectory lines
        for i in range(1, len(pts)):
            x1 = int((pts[i-1,0] - min_x) * scale + pad)
            y1 = w - int((pts[i-1,1] - min_y) * scale + pad)
            x2 = int((pts[i,0]   - min_x) * scale + pad)
            y2 = w - int((pts[i,1]   - min_y) * scale + pad)

            cv2.line(img, (x1,y1), (x2,y2), (0,255,0), 2)

        # -------------------------------
        # Draw orientation arrow
        # -------------------------------
        x, y, _ = self.path_points[-1]
        yaw = self.compute_yaw_from_motion()

        rx = int((x - min_x) * scale + pad)
        ry = w - int((y - min_y) * scale + pad)

        # Arrow length in pixels
        arrow_len = 40

        # Compute arrow endpoint
        arrow_x = int(rx + arrow_len * math.cos(yaw))
        arrow_y = int(ry - arrow_len * math.sin(yaw))  # y inverted for OpenCV

        # Draw the robot point
        cv2.circle(img, (rx, ry), 6, (0, 0, 255), -1)

        # Draw arrow for heading
        cv2.arrowedLine(
            img,
            (rx, ry),
            (arrow_x, arrow_y),
            (255, 0, 0),
            3,
            tipLength=0.3
        )

        cv2.imshow("VO Path", img)
        cv2.waitKey(1)


# -----------------------------------
# Main
# -----------------------------------
def main():
    rclpy.init()
    node = VoVisualizer()

    try:
        while rclpy.ok():
            rclpy.spin_once(node, timeout_sec=0.01)
    except KeyboardInterrupt:
        pass

    node.destroy_node()
    rclpy.shutdown()
    cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
