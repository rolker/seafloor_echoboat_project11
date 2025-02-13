from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.actions import GroupAction
from launch.actions import IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch.substitutions import PathJoinSubstitution
from launch.substitutions import TextSubstitution
from launch_ros.substitutions import FindPackageShare

# To launch the docker based simulator, in a separate terminal, run:

# roscd echo_helm/scripts
# ./start_ardupilot_docker_sim.bash 


def generate_launch_description():
  namespace = LaunchConfiguration('namespace')
  operator_namespace = LaunchConfiguration('operator_namespace')
  enable_bridge = LaunchConfiguration('enable_bridge')
  launch_robot = LaunchConfiguration('launch_robot')
  launch_operator = LaunchConfiguration('launch_operator')

  namespace_arg = DeclareLaunchArgument(
    "namespace", default_value=TextSubstitution(text="echo")
  )
  operator_namespace_arg = DeclareLaunchArgument(
    "operator_namespace", default_value=TextSubstitution(text="operator")
  )
  enable_bridge_arg = DeclareLaunchArgument(
    "enable_bridge", default_value=TextSubstitution(text="false")
  )
  launch_robot_arg = DeclareLaunchArgument(
    "launch_robot", default_value=TextSubstitution(text="true")
  )
  launch_operator_arg = DeclareLaunchArgument(
    "launch_operator", default_value=TextSubstitution(text="true")
  )

  launch_robot_include = IncludeLaunchDescription(
    PythonLaunchDescriptionSource(
      PathJoinSubstitution([
        FindPackageShare('echoboat_project11'),
        'launch',
        'echo_launch.py'
      ])
    ),
    condition=IfCondition(launch_robot),
    launch_arguments={
      'namespace': namespace,
      'enable_bridge': enable_bridge,
      'operator_host': 'localhost',
      'fcu_url': 'tcp://0.0.0.0:5760@',
      'gcs_url': 'udp://@127.0.0.1:14550',
      'is_simulator': 'true',
    }.items()
  )


  operator_group = GroupAction(
    actions=[
      IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
          PathJoinSubstitution([
            FindPackageShare('project11'),
            'launch',
            'operator_core_launch.py'
          ])
        ),
        launch_arguments={
          'namespace': operator_namespace,
          'robot_namespace': namespace,
          'enable_bridge': enable_bridge,
          'local_port': '4201'
        }.items()
      ),
      
      IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
          PathJoinSubstitution([
            FindPackageShare('project11'),
            'launch',
            'operator_ui_launch.py'
          ])
        ),
        launch_arguments={
          'namespace': operator_namespace,
          'robot_namespace': namespace,
        }.items()
      )
    ],
    condition=IfCondition(launch_operator)
  )

  return LaunchDescription([
    namespace_arg,
    operator_namespace_arg,
    enable_bridge_arg,
    launch_robot_arg,
    launch_operator_arg,
    launch_robot_include,
    operator_group
  ])

# <launch>
#   <group if="$(arg launchRobot)">
#     <rosparam param="udp_bridge/remotes/operator/connections/default/port" ns="$(arg namespace)" subst_value="True">4201</rosparam>
#     <rosparam param="udp_bridge/remotes/operator/connections/default/host" ns="$(arg namespace)" subst_value="True">localhost</rosparam>
#   </group>
# </launch>
