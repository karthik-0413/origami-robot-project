from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'serial_pkg'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        
        (os.path.join('share', package_name), glob('launch/*')),
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
            'serial_test = serial_pkg.serial_test:main',
            'manual_control_node_new = serial_pkg.manual_control_new_node:main',
            'esp_serial_sender_node_new = serial_pkg.esp_serial_sender_node_new:main',
            'esp_serial_receiver_node_new = serial_pkg.esp_serial_receiver_node_new:main',
        ],
    },
)
