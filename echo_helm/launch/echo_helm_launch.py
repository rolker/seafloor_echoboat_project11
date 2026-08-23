from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch.substitutions import PythonExpression
from launch_ros.actions import LifecycleNode
from launch_ros.actions import LifecycleTransition
from launch_ros.actions import Node

from lifecycle_msgs.msg import Transition


def generate_launch_description():

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "enable_ellipsoidal_fix",
                default_value="true",
                description=(
                    "Republish mavros/global_position/global with a true WGS-84 "
                    "ellipsoidal altitude on .../global_ellipsoidal. mavros "
                    "round-trips the receiver's MSL altitude through EGM96-5, "
                    "leaving a sub-metre error (0.626 m measured at the UNH "
                    "pier) in every consumer; see "
                    "echo_helm/ellipsoidal_corrector.hpp. Every EchoBoat behind "
                    "an ArduPilot FCU has this defect, so the correction is "
                    "offered here rather than only in a boat-instance repo."
                ),
            ),
            LifecycleNode(
                package="echo_helm",
                executable="echo_helm_node",
                name="echo_helm",
                namespace="",
                respawn=True,
                respawn_delay=2,
                emulate_tty=True,
            ),
            # Not a lifecycle node: it is a stateless republisher with no
            # resources to acquire, and it must be running before anything that
            # consumes navigation comes up.
            #
            # respawn matches echo_helm and mavros around it: this node now owns
            # the FCU navigation position that reaches mru_transform, so a crash
            # that left it down would be an open-ended vertical outage.
            #
            # LOCKSTEP: BizzyBoat's instance repo (unh_echoboats_project11)
            # launches this same node from its own core_launch.py. Two copies in
            # one namespace publish duplicate messages on the output topic, so
            # the instance launch must drop its copy when this lands -- or pass
            # enable_ellipsoidal_fix:=false. The node belongs here: the defect is
            # a property of mavros behind an ArduPilot FCU, which is every
            # EchoBoat, not of one hull.
            Node(
                package="echo_helm",
                executable="ellipsoidal_fix_node",
                name="ellipsoidal_fix",
                namespace="",
                condition=IfCondition(LaunchConfiguration("enable_ellipsoidal_fix")),
                respawn=True,
                respawn_delay=2,
                emulate_tty=True,
            ),
            LifecycleTransition(
                lifecycle_node_names=(
                    PythonExpression(
                        expression=[
                            '"',
                            LaunchConfiguration("ros_namespace", default=""),
                            '" + "/echo_helm"',
                        ],
                    ),
                ),
                transition_ids=(
                    Transition.TRANSITION_CONFIGURE,
                    Transition.TRANSITION_ACTIVATE,
                ),
            ),
        ]
    )
