import socket
import struct
import cv2
import numpy as np
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image, CameraInfo, Imu
from cv_bridge import CvBridge

def get_camera_info(is_left=True):
    
    # Camera info object
    camera_info = CameraInfo()
    camera_info.width = 640
    camera_info.height = 480
    
    # Distortion coefficients
    camera_info.distortion_model = 'plumb_bob'
    camera_info.d = [0.0, 0.0, 0.0, 0.0, 0.0]
    
    # Intrinsic camera matrix
    fx, fy = 554.25, 415.69
    cx, cy = 320.0, 240.0
    Tx = -110.85 if not is_left else 0.0
    
    camera_info.k = [fx, 0.0, cx,
                     0.0, fy, cy,
                     0.0, 0.0, 1.0]
    
    # Rectification matrix
    camera_info.r = [1.0, 0.0, 0.0,
                     0.0, 1.0, 0.0,
                     0.0, 0.0, 1.0]
    
    # Projection matrix
    camera_info.p = [fx, 0.0, cx, Tx,
                     0.0, fy, cy, 0.0,
                     0.0, 0.0, 1.0, 0.0]
    
    # Header frame
    camera_info.header.frame_id = 'left_camera_frame' if is_left else 'right_camera_frame'
    
    return camera_info


def get_imu_covariance_matrix(sim=True):
    orient_var = float(3.0e-6)
    accel_var = float(1.0e-2)
    gyro_var = float(7.6e-5)
    
    g_cov = [
        gyro_var, 0.0, 0.0,
        0.0, gyro_var, 0.0,
        0.0, 0.0, gyro_var
    ]
    a_cov = [
        accel_var, 0.0, 0.0,
        0.0, accel_var, 0.0,
        0.0, 0.0, accel_var
    ]
    o_cov = [
        orient_var, 0.0, 0.0,
        0.0, orient_var, 0.0,
        0.0, 0.0, orient_var
    ]
    
    return o_cov, g_cov, a_cov



class StereoReceiverNode(Node):
    def __init__(self):
        super().__init__('stereo_receiver')
        self.declare_parameter('port', 5005)
        self.port = self.get_parameter('port').value
        
        # TCP socket setup
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.bind(('', self.port))
        self.sock.listen(1)
        self.get_logger().info(f'Listening for connections on port {self.port}...')
        self.conn, self.addr = self.sock.accept()
        self.get_logger().info(f'Connection established with {self.addr}')
        
        # Publishers
        self.bridge = CvBridge()
        self.left_pub = self.create_publisher(Image, '/left_camera/image_raw', 10)
        self.right_pub = self.create_publisher(Image, '/right_camera/image_raw', 10)
        self.left_info_pub = self.create_publisher(CameraInfo, '/left_camera/camera_info', 10)
        self.right_info_pub = self.create_publisher(CameraInfo, '/right_camera/camera_info', 10)
        self.imu_pub = self.create_publisher(Imu, '/visual_slam/imu', 10)
        
        # Camera info
        self.left_info = get_camera_info(is_left=True)
        self.right_info = get_camera_info(is_left=False)
        
        # IMU covariance
        self.o_cov, self.g_cov, self.a_cov = get_imu_covariance_matrix(sim=True)
        
        self.timer = self.create_timer(0.001, self.read_packet)
    
    def recv_exact(self, size):
        buf = b''
        while len(buf) < size:
            data = self.conn.recv(size - len(buf))
            if not data:
                return None
            buf += data
        return buf
    
    def read_packet(self):
        
        # Read packet header
        try:
            header = self.recv_exact(8)
            left_len, right_len = struct.unpack('!II', header)
        except Exception as e:
            self.get_logger().error(f'Error reading packet header: {e}')
            return
        
        # Read images
        try:
            left_data = self.recv_exact(left_len)
            right_data = self.recv_exact(right_len)
            left_img = cv2.imdecode(np.frombuffer(left_data, np.uint8), cv2.IMREAD_COLOR)
            right_img = cv2.imdecode(np.frombuffer(right_data, np.uint8), cv2.IMREAD_COLOR)
            left_img = cv2.cvtColor(left_img, cv2.COLOR_BGR2RGB)
            right_img = cv2.cvtColor(right_img, cv2.COLOR_BGR2RGB)
        except Exception as e:
            self.get_logger().error(f'Error reading image data: {e}')
            return
        
        if left_img is None or right_img is None:
            self.get_logger().error('Failed to decode images')
            return
    
        # Read IMU data
        try:
            imu_bytes = self.recv_exact(40)
            if imu_bytes is None:
                self.get_logger().error('Connection closed while reading IMU data')
                return
            ox, oy, oz, ow, ax, ay, az, gx, gy, gz = struct.unpack('<10f', imu_bytes)
        except Exception as e:
            self.get_logger().error(f'Error reading IMU data: {e}')
            return
        
        # Create sensor_msgs/Image
        now = self.get_clock().now().to_msg()
        left_msg = self.bridge.cv2_to_imgmsg(left_img, encoding='rgb8')
        right_msg = self.bridge.cv2_to_imgmsg(right_img, encoding='rgb8')
        left_msg.header.stamp = now
        right_msg.header.stamp = now
        left_msg.header.frame_id = 'left_camera_frame'
        right_msg.header.frame_id = 'right_camera_frame'
        self.left_info.header.stamp = now
        self.right_info.header.stamp = now
        
        # Create IMU message
        imu_msg = Imu()
        imu_msg.header.stamp = now
        imu_msg.header.frame_id = 'imu_link'
        imu_msg.orientation.x = ox
        imu_msg.orientation.y = oy
        imu_msg.orientation.z = oz
        imu_msg.orientation.w = ow
        imu_msg.linear_acceleration.x = ax
        imu_msg.linear_acceleration.y = ay
        imu_msg.linear_acceleration.z = az
        imu_msg.angular_velocity.x = gx
        imu_msg.angular_velocity.y = gy
        imu_msg.angular_velocity.z = gz
        imu_msg.orientation_covariance = self.o_cov
        imu_msg.linear_acceleration_covariance = self.a_cov
        imu_msg.angular_velocity_covariance = self.g_cov
        
        # Publish images
        self.left_pub.publish(left_msg)
        self.right_pub.publish(right_msg)
        
        # Publish camera info
        self.left_info_pub.publish(self.left_info)
        self.right_info_pub.publish(self.right_info)
        
        # Publish IMU
        self.imu_pub.publish(imu_msg)
        
# Main function
def main(args=None):
    rclpy.init(args=args)
    node = StereoReceiverNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()