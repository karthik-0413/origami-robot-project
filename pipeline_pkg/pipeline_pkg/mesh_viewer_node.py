#!/usr/bin/env python3
import os
import rclpy
from rclpy.node import Node
from launch import LaunchDescription
from launch_ros.actions import Node as LaunchNode
from ament_index_python.packages import get_package_share_directory
from launch_ros.actions import Node as LaunchNode
from launch import LaunchService

class NVBloxRvizLauncher(Node):
    def __init__(self):
        super().__init__("nvblox_rviz_launcher")

        self.declare_parameter("rviz_config_file", "")
        rviz_config = self.get_parameter("rviz_config_file").get_parameter_value().string_value
        
        self.get_logger().info(f"Starting RViz2 with config: {rviz_config}")

        if rviz_config == "":
            self.get_logger().error("No RViz config file provided")
            return
        
        if not os.path.exists(rviz_config):
            self.get_logger().error(f"RViz config file does not exist: {rviz_config}")
            return
        
        self.get_logger().info(f"Starting RViz2 with config: {rviz_config}")

        ld = LaunchDescription([
            LaunchNode(
                package="rviz2",
                executable="rviz2",
                name="rviz2",
                arguments=["-d", rviz_config]
            )
        ])

        ls = LaunchService()
        ls.include_launch_description(ld)
        ls.run()

def main(args=None):
    rclpy.init(args=args)
    node = NVBloxRvizLauncher()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == "__main__":
    main()
