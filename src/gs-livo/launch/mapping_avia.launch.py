"""ROS2 launch file for Livox AVIA LiDAR, mirroring mapping_avia.launch (ROS1).

The ported rclcpp node (LIVMapper, see src/LIVMapper.cpp) registers itself
as "laser_mapping_node" (its constructor calls
rclcpp::Node("laser_mapping_node", options)), which is why the Node(name=...)
below and the top-level key in config/avia_ros2.yaml both use that name
instead of the ROS1 launch file's "laserMapping".

The two ROS1 config files (config/avia.yaml, config/camera_pinhole.yaml)
were both loaded flat into the same node's parameter namespace in the
original launch file (one via a global `rosparam load`, one via a
per-node `rosparam file` -- both end up as private parameters of the same
node in roscpp's ~private namespace convention). They are merged here into
a single config/avia_ros2.yaml under one `ros__parameters:` block for that
reason, rather than kept as two separate ROS2 params files.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_share = get_package_share_directory("fast_livo")

    rviz_arg = DeclareLaunchArgument(
        "rviz",
        default_value="true",
        description="Launch RViz alongside the mapping node",
    )

    params_file = os.path.join(pkg_share, "config", "avia_ros2.yaml")
    rviz_config = os.path.join(pkg_share, "rviz_cfg", "fast_livo2.rviz")

    laser_mapping_node = Node(
        package="fast_livo",
        executable="fastlivo_mapping",
        name="laser_mapping_node",
        output="screen",
        parameters=[params_file],
    )

    rviz_node = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2",
        arguments=["-d", rviz_config],
        prefix="nice",
        condition=IfCondition(LaunchConfiguration("rviz")),
    )

    # ROS2 equivalent of the ROS1 image_transport republish node.
    republish_node = Node(
        package="image_transport",
        executable="republish",
        name="republish",
        output="screen",
        arguments=["compressed", "raw"],
        remappings=[
            ("in", "/left_camera/image"),
            ("out", "/left_camera/image"),
        ],
        respawn=True,
    )

    return LaunchDescription(
        [
            rviz_arg,
            laser_mapping_node,
            rviz_node,
            republish_node,
        ]
    )
