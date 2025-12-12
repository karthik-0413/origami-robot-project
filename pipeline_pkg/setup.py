from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'pipeline_pkg'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        
        (os.path.join('share', package_name, 'launch'), glob('launch/*')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='or',
    maintainer_email='or@todo.todo',
    description='TODO: Package description',
    license='Apache-2.0',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'depth_viewer_node = pipeline_pkg.depth_viewer_node:main',
            'esp_frame_receiver_node = pipeline_pkg.esp_frame_receiver_node:main',
            'opencv_slam_viewer_node = pipeline_pkg.opencv_slam_viewer_node:main',
            'opencv_slam_node = pipeline_pkg.opencv_slam_node:main',
            'stereo_receiver_node = pipeline_pkg.stereo_receiver_node:main',
            'mesh_viewer_node = pipeline_pkg.mesh_viewer_node:main',
            'pose_correction_node = pipeline_pkg.pose_correction_node:main',
        ],
    },
)
