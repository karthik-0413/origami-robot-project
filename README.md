# ECE1896_TermProject
Repo for the Origami Robot Software module

# Developed on:
ROS2 Humble

Ubuntu Jammy (22.04)

Jetpack 6.2

Jetson Orin Nano 8-GB

Using isaac_ros-dev container

# Dependencies
**As of writing, Isaac ROS 4.0 is not available on Jetson Orin Nano.**

isaac_ros_image_proc (v3.2)

isaac_ros_stereo_image_proc (v3.2)

isaac_ros_nvblox (v3.2)

isaac_ros_visual_slam (v3.2)

Link to Isaac ROS 3.2: https://nvidia-isaac-ros.github.io/v/release-3.2/getting_started/index.html

# Troubleshooting
**Not every issue is listed, just a few that I remember**

If getting out of memory errors when running disparity and depth-to-disparity nodes, then close all other applications before launching. I ran into this error when having vscode open while attempting to run.

The issac_ros-dev container resets installed apt repositories during start up. I got around this by modifying the run_dev.sh file to pause the container upon exit, instead of removing it.
