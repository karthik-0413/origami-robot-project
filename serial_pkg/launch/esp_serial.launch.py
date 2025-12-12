from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    
    # Declare launch arguments
    arg_port = DeclareLaunchArgument(
        'port',
        default_value='/dev/ttyUSB0',
        description='Serial port for ESP device'
    )
    port = LaunchConfiguration('port')
    
    # Receive Serial Node
    receive_serial_node = Node(
        package='serial_pkg',
        executable='esp_serial_receiver_node_new',
        name='esp_serial_receiver_node_new',
        output='screen',
        parameters=[{
            'port': port,
            'baud': 115200
        }]
    )
    
    # Send Serial Node
    send_serial_node = Node(
        package='serial_pkg',
        executable='esp_serial_sender_node_new',
        name='esp_serial_sender_node_new',
        output='screen',
        parameters=[{
            'port': port,
            'baud': 115200
        }]
    )
    
    # Manual Control Node
    manual_control_node = Node(
        package='serial_pkg',
        executable='manual_control_node_new',
        name='manual_control_node_new',
        output='screen',
        parameters=[{
        }]
    )
    
    return LaunchDescription([
        receive_serial_node,
        send_serial_node,
        manual_control_node,
    ])