from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode
from launch.actions import EmitEvent
from launch.events import Shutdown

def generate_launch_description():
    
    roll = -1.5708  # -90 degrees in radians
    pitch = 0.0
    yaw = -1.5708  # -90 degrees in radians
    
    
    # Declare launch arguments
    arg_sim = DeclareLaunchArgument(
        'sim',
        default_value='false',
        description='Set to true to run in unity simulation mode'
    )
    sim = LaunchConfiguration('sim')
    
    if sim == 'false':
        width = 320
        height = 240
        baseline = '0.00'
    else:
        width = 640
        height = 480
        baseline = '0.2'
    
    # Node for Nvblox 3D reconstruction
    nvblox_node = ComposableNode(
        name='nvblox_node',
        package='nvblox_ros',
        plugin='nvblox::NvbloxNode',
        parameters=[{
            'use_lidar': False,
            'voxel_size': 0.04,
            'global_frame': 'odom',
            'decay_tsdf_rate_hz': 0.0,
            'publish_esdf_distance_slice': False,
            'map_clearing_radius_m': 10.0,
            'max_back_projection_distance': 10.0,
        }],
        remappings=[
            ('camera_0/color/image', '/left_camera/image_rect'),
            ('camera_0/color/camera_info', '/left_camera/camera_info_rect'),
            ('camera_0/depth/image', '/left_camera/image_depth'),
            ('camera_0/depth/camera_info', '/left_camera/camera_info_rect'),
            ('pose', '/visual_slam/tracking/vo_pose'),
        ],
    )
    
    # tf2 base_link to left_camera_frame Node
    tf2_base_to_left_node = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='tf2_base_to_left_camera',
        arguments=['0', '0', '0', str(roll), str(pitch), str(yaw), 'base_link', 'left_camera_frame'],
        on_exit=[EmitEvent(event=Shutdown())]
    )
    
    # tf2 base_link to right_camera_frame Node
    tf2_base_to_right_node = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='tf2_base_to_right_camera',
        arguments=['0.06', '0', '0', str(roll), str(pitch), str(yaw), 'base_link', 'right_camera_frame'],
        on_exit=[EmitEvent(event=Shutdown())]
    )
    
    # tf2 base_link to imu_link Node
    tf2_base_to_imu_node = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='tf2_base_to_imu',
        #arguments=['0', '0', '0', str(roll), str(pitch), str(yaw), 'base_link', 'imu_link'],
        arguments=['0', '0', '0', '0', '0', '0', 'base_link', 'imu_link'],
        on_exit=[EmitEvent(event=Shutdown())]
    )
    
    # Node for ESP frame receiving
    esp_frame_receiver_node = Node(
        package='pipeline_pkg',
        executable='esp_frame_receiver_node',
        name='esp_frame_receiver_node',
        output='screen',
        parameters=[{
            'frame_width': 320,
            'frame_height': 240,
        }]
    )
    
    # Node for Unity simulation receiving
    stereo_receiver_node = Node(
        package='pipeline_pkg',
        executable='stereo_receiver_node',
        name='stereo_receiver_node',
        output='screen',
        parameters=[{
            'port': 5005
        }]
    )
    
    # Node to for left camera rectification
    rectify_left_node = ComposableNode(
        package = 'isaac_ros_image_proc',
        plugin = 'nvidia::isaac_ros::image_proc::RectifyNode',
        name = 'rectify_left_node',
        parameters = [{
            'output_width': 320,
            'output_height': 240,
        }],
        remappings = [
            ('image_raw', '/left_camera/image_raw'),
            ('camera_info', '/left_camera/camera_info'),
            ('image_rect', '/left_camera/image_rect'),
            ('camera_info_rect', '/left_camera/camera_info_rect'),
        ]
    )
    
    # Node to for right camera rectification
    rectify_right_node = ComposableNode(
        package = 'isaac_ros_image_proc',
        plugin = 'nvidia::isaac_ros::image_proc::RectifyNode',
        name = 'rectify_right_node',
        parameters = [{
            'output_width': 320,
            'output_height': 240,
        }],
        remappings = [
            ('image_raw', '/right_camera/image_raw'),
            ('camera_info', '/right_camera/camera_info'),
            ('image_rect', '/right_camera/image_rect'),
            ('camera_info_rect', '/right_camera/camera_info_rect'),
        ]
    )
    
    # Node for stereo disparity computation
    disparity_node = ComposableNode(
        package = 'isaac_ros_stereo_image_proc',
        plugin = 'nvidia::isaac_ros::stereo_image_proc::DisparityNode',
        name = 'disparity_node',
        parameters = [{
            'max_disparity': 64.0,
            'backend': 'CUDA',
            'num_passes': 2,
            'p1': 24,
            'p2': 96,
        }],
        remappings = [
            ('left/image_rect', '/left_camera/image_rect'),
            ('left/camera_info', '/left_camera/camera_info_rect'),
            ('right/image_rect', '/right_camera/image_rect'),
            ('right/camera_info', '/right_camera/camera_info_rect'),
            ('disparity', '/left_camera/image_disparity'),
        ],
    )
    
    # Node for stereo depth computation
    disparity_to_depth_node = ComposableNode(
        package = 'isaac_ros_stereo_image_proc',
        plugin = 'nvidia::isaac_ros::stereo_image_proc::DisparityToDepthNode',
        name = 'disparity_to_depth_node',
        remappings = [
            ('disparity', '/left_camera/image_disparity'),
            ('depth', '/left_camera/image_depth'),
        ]
    )
    # SLAM Node
    visual_slam_node = ComposableNode(
        package='isaac_ros_visual_slam',
        plugin='nvidia::isaac_ros::visual_slam::VisualSlamNode',
        name='visual_slam',
        parameters=[{
            'enable_imu_fusion': False,
            'image_jitter_threshold_ms': 5000.0,
            'imu_jitter_threshold_ms': 5000.0,
            'enable_ground_constraint_in_slam': True,
            'enable_ground_constraint_in_odometry': False,
            'enable_localization_n_mapping': True,
            'imu_frame': 'imu_link',
            'base_frame': 'base_link',
            'camera_optical_frames': ['left_camera_frame', 'right_camera_frame'],
            'verbosity': 1,
            'enable_debug_mode': False,
            'enable_slam_visualization': False,
            'enable_observations_view': False,
            'enable_landmarks_view': False,
            'enable_image_denoising': True,
        }],
        remappings=[
            ('/visual_slam/image_0', '/left_camera/image_rect'),
            ('/visual_slam/camera_info_0', '/left_camera/camera_info_rect'),
            ('/visual_slam/image_1', '/right_camera/image_rect'),
            ('/visual_slam/camera_info_1', '/right_camera/camera_info_rect'),
        ],
    )

    # Container to hold all composable nodes
    nvidia_container = ComposableNodeContainer(
        package = 'rclcpp_components',
        name = 'stereo_pipeline_container',
        namespace = '',
        executable = 'component_container_mt',
        composable_node_descriptions = [
            rectify_left_node,
            rectify_right_node,
            disparity_node,
            disparity_to_depth_node,
            visual_slam_node,
            nvblox_node,
        ],
        output = 'screen',
    )
    
    # Check whether to receive from ESP or Unity simulation
    # if sim == 'false':
    #     return LaunchDescription([
    #         nvidia_container,
    #         tf2_base_to_left_node,
    #         tf2_base_to_right_node,
    #         tf2_base_to_imu_node,
    #         esp_frame_receiver_node,
    #     ])
    # else:
    #     return LaunchDescription([
    #         nvidia_container,
    #         tf2_base_to_left_node,
    #         tf2_base_to_right_node,
    #         tf2_base_to_imu_node,
    #         stereo_receiver_node,
    #     ])
        
    return LaunchDescription([
            esp_frame_receiver_node,
            tf2_base_to_left_node,
            tf2_base_to_right_node,
            nvidia_container,
        ])