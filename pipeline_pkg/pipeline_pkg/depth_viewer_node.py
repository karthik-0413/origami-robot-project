import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2
import numpy as np

class DepthColorViewer(Node):
    def __init__(self):
        super().__init__('depth_color_viewer')
        self.bridge = CvBridge()

        # Subscribe to depth and left image
        self.sub_depth = self.create_subscription(
            Image, '/left_camera/image_depth', self.depth_callback, 10)
        self.sub_left = self.create_subscription(
            Image, '/left_camera/image_raw', self.left_callback, 10)

        self.left_image = None
        self.depth_colored = None

    def left_callback(self, msg):
        self.left_image = self.bridge.imgmsg_to_cv2(msg, desired_encoding='bgr8')
        self.update_display()

    def depth_callback(self, msg):
        depth = self.bridge.imgmsg_to_cv2(msg, desired_encoding='passthrough')
        depth = np.nan_to_num(depth, nan=0.0, posinf=0.0)

        # Clip to a visible range (adjust if your scene is farther/closer)
        max_depth_m = 10.0
        depth_clipped = np.clip(depth, 0.0, max_depth_m)

        # Normalize to [0, 255]
        norm = cv2.normalize(depth_clipped, None, 0, 255, cv2.NORM_MINMAX)
        norm = 255 - norm  # invert so near = high value (hot)
        norm = norm.astype(np.uint8)

        # Apply colormap: JET gives blue→red gradient
        self.depth_colored = cv2.applyColorMap(norm, cv2.COLORMAP_JET)
        self.update_display()

    def update_display(self):
        if self.left_image is None or self.depth_colored is None:
            return

        # Resize both to the same height
        h = min(self.left_image.shape[0], self.depth_colored.shape[0])
        left_resized = cv2.resize(self.left_image, (int(self.left_image.shape[1]*h/self.left_image.shape[0]), h))
        depth_resized = cv2.resize(self.depth_colored, (left_resized.shape[1], h))

        # Concatenate side by side
        combined = np.vstack((left_resized, depth_resized))
        cv2.imshow('Left vs Depth (hot=near, cool=far)', combined)
        cv2.waitKey(1)

def main(args=None):
    rclpy.init(args=args)
    node = DepthColorViewer()
    rclpy.spin(node)
    node.destroy_node()
    cv2.destroyAllWindows()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
