from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch.substitutions import PythonExpression
from launch_ros.actions import LifecycleNode
from launch_ros.actions import LifecycleTransition
from launch_ros.actions import Node

from lifecycle_msgs.msg import Transition

def generate_launch_description():

    return LaunchDescription([
        LifecycleNode(
            package='echo_helm',
            executable='echo_helm_node',
            name='echo_helm',
            namespace='',
            respawn=True,
            respawn_delay=2,
            emulate_tty=True
        ),
        LifecycleTransition(
            lifecycle_node_names=(
                PythonExpression(
                    expression = [
                        '"',
                        LaunchConfiguration("ros_namespace", default=''),
                        '" + "/echo_helm"'
                    ],
                ),
            ),
            transition_ids=(
                Transition.TRANSITION_CONFIGURE,
                Transition.TRANSITION_ACTIVATE,
            )
        ),
    ])
