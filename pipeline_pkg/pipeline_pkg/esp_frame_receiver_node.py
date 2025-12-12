import rclpy
from rclpy.node import Node

import socket
import time
import numpy as np
import cv2

from sensor_msgs.msg import Image, CameraInfo
from cv_bridge import CvBridge


UDP_PORT = 9999
UDP_IP = "0.0.0.0"
BUFFER_SIZE = 2048
HEADER_SIZE = 5
MAX_PAYLOAD = 1400 - HEADER_SIZE

def get_camera_info_left(width=320, height=240):
    
    # Camera info object
    camera_info = CameraInfo()
    camera_info.width = width
    camera_info.height = height
    
    # Distortion coefficients
    camera_info.distortion_model = 'plumb_bob'
    camera_info.d = [-1.58333822e-1, 2.23236758e00, -2.11866809e-03, 5.419668405e-03, -7.58967751e00]
    
    # Intrinsic camera matrix
    fx, fy = 387.00143897, 386.77189786
    cx, cy = 156.58800772, 130.62680217
    Tx = 0.0
    
    camera_info.k = [fx, 0.0, cx,
                     0.0, fy, cy,
                     0.0, 0.0, 1.0]
    
    # Rectification matrix
    camera_info.r = [0.8496307, 0.3993389, 0.34446497,
                     -0.41396962, 0.90967331, -0.03352048,
                     -0.32673662, -0.114118, 0.93820055]
    
    # Projection matrix
    # camera_info.p = [fx, 0.0, cx, Tx,
    #                  0.0, fy, cy, 0.0,
    #                  0.0, 0.0, 1.0, 0.0]
    camera_info.p = [
        6.52749886e03,  0.0,            3.32091868e01,  0.0,
        0.0,            6.52749886e03,  1.60702084e02,  0.0,
        0.0,            0.0,            1.0,            0.0
    ]
    
    # Header frame
    camera_info.header.frame_id = 'left_camera_frame'
    
    return camera_info

def get_camera_info_right(width=320, height=240):
    
    # Camera info object
    camera_info = CameraInfo()
    camera_info.width = width
    camera_info.height = height
    
    # Distortion coefficients
    camera_info.distortion_model = 'plumb_bob'
    camera_info.d = [1.94941322e-01, -1.39501516e-04, 4.22089445e-04, 7.76500182e-03, 1.78474881e00]
    
    # Intrinsic camera matrix
    fx, fy = 360.30109026, 359.77814513
    cx, cy = 183.5449637, 129.70467331
    #Tx = -33.255376
    
    camera_info.k = [fx, 0.0, cx,
                     0.0, fy, cy,
                     0.0, 0.0, 1.0]
    
    # Rectification matrix
    camera_info.r = [0.85948752, 0.43702521, 0.26512294,
                     -0.42509024, 0.89914856, -0.10406806,
                     -0.28386527, -0.02325597, 0.95858211]
    
    # Projection matrix
    camera_info.p = [6.52749886e03, 0.0,            3.32091868e01,  -7.77179741e02,
                     0.0,           6.52749886e03,  1.60702084e02,  0.0,
                     0.0,           0.0,            1.0,            0.0
        
    ]
    
    # Header frame
    camera_info.header.frame_id = 'right_camera_frame'
    
    return camera_info


class StereoUDPNode(Node):
    def __init__(self):
        super().__init__("stereo_udp_receiver")

        self.bridge = CvBridge()
        
        # Parameters
        self.declare_parameter("frame_width", 320)
        self.declare_parameter("frame_height", 240)
        width = self.get_parameter("frame_width").value
        height = self.get_parameter("frame_height").value
        
        # Publishers
        self.pub_left  = self.create_publisher(Image, "/left_camera/image_raw", 10)
        self.pub_right = self.create_publisher(Image, "/right_camera/image_raw", 10)
        self.pub_left_info  = self.create_publisher(CameraInfo, "/left_camera/camera_info", 10)
        self.pub_right_info = self.create_publisher(CameraInfo, "/right_camera/camera_info", 10)

        # Camera info
        self.left_camera_info  = get_camera_info_left(width, height)
        self.right_camera_info = get_camera_info_right(width, height)

        # Buffers and tracking
        self.frame_buffers = {1: {}, 2: {}}
        self.frame_sizes   = {1: 0, 2: 0}

        # Last ready frames
        self.last_left_frame  = None
        self.last_right_frame = None

        # FPS
        self.frame_times = {1: [], 2: []}

        # UDP Setup
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind((UDP_IP, UDP_PORT))
        self.sock.settimeout(0.005)

        # Loop timer
        self.timer = self.create_timer(0.001, self.poll_udp)

        self.get_logger().info("Stereo UDP Receiver with SYNCHRONIZED publishing started.")

    # --------------------------------------------------------------
    # FPS calculation
    # --------------------------------------------------------------
    def calculate_fps(self, cam):
        now = time.time()
        self.frame_times[cam].append(now)
        self.frame_times[cam] = [t for t in self.frame_times[cam] if now - t < 2]

        if len(self.frame_times[cam]) < 2:
            return 0.0

        dt = self.frame_times[cam][-1] - self.frame_times[cam][0]
        return (len(self.frame_times[cam]) - 1) / dt if dt > 0 else 0.0

    # --------------------------------------------------------------
    # Main network polling
    # --------------------------------------------------------------
    def poll_udp(self):
        try:
            data, _ = self.sock.recvfrom(BUFFER_SIZE)
            self.process_packet(data)
        except socket.timeout:
            return

    # --------------------------------------------------------------
    # Process packet with packet index & total packet count
    # --------------------------------------------------------------
    def process_packet(self, data):
        if len(data) < 5:
            return

        cam = data[0]
        size_hi = data[1]
        size_lo = data[2]
        packet_index = data[3]
        total_packets = data[4]
        chunk = data[5:]

        frame_size = (size_hi << 8) | size_lo

        # Detect new frame
        if self.frame_sizes[cam] != frame_size:
            self.frame_sizes[cam] = frame_size
            self.frame_buffers[cam] = {}

        # Insert chunk by packet index
        self.frame_buffers[cam][packet_index] = chunk

        # If not complete, return
        if len(self.frame_buffers[cam]) != total_packets:
            return

        # Frame complete → decode
        self.finish_frame(cam, total_packets)

    # --------------------------------------------------------------
    # Assemble a full frame and store it
    # --------------------------------------------------------------
    def finish_frame(self, cam, total_packets):
        # Reassemble JPEG data
        img_bytes = bytearray()

        for i in range(total_packets):
            if i not in self.frame_buffers[cam]:
                self.get_logger().warn(f"Packet {i} missing — dropping frame for camera {cam}")
                self.frame_buffers[cam] = {}
                return

            img_bytes.extend(self.frame_buffers[cam][i])

        self.frame_buffers[cam] = {}  # clear buffer

        # Decode JPEG
        img_arr = np.frombuffer(img_bytes, dtype=np.uint8)
        img_bgr = cv2.imdecode(img_arr, cv2.IMREAD_COLOR)

        if img_bgr is None:
            return

        img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
        ros_image = self.bridge.cv2_to_imgmsg(img_rgb, encoding="rgb8")

        # Store frame
        if cam == 1:
            self.last_left_frame = ros_image
        else:
            self.last_right_frame = ros_image

        # Try synchronized publish
        self.try_publish_pair()

    # --------------------------------------------------------------
    # Publish *only if both frames are ready*
    # --------------------------------------------------------------
    def try_publish_pair(self):
        if self.last_left_frame is None:
            return
        if self.last_right_frame is None:
            return

        # Add timestamps and camera info
        now = self.get_clock().now().to_msg()
        self.last_left_frame.header.stamp = now
        self.last_right_frame.header.stamp = now
        self.left_camera_info.header.stamp = now
        self.right_camera_info.header.stamp = now
        self.last_left_frame.header.frame_id = 'left_camera_frame'
        self.last_right_frame.header.frame_id = 'right_camera_frame'
        
        # Publish both together
        self.pub_left.publish(self.last_left_frame)
        self.pub_right.publish(self.last_right_frame)
        self.pub_left_info.publish(self.left_camera_info)
        self.pub_right_info.publish(self.right_camera_info)

        # FPS tracking
        self.fps_left = self.calculate_fps(1)
        self.fps_right = self.calculate_fps(2)

        self.get_logger().info(f"Published frames - Left FPS: {self.calculate_fps(1):.0f}, Right FPS: {self.calculate_fps(2):.0f}")

        # Clear after publishing
        self.last_left_frame = None
        self.last_right_frame = None


def main(args=None):
    rclpy.init(args=args)
    node = StereoUDPNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

