from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.actions import GroupAction
from launch.actions import IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import AnyLaunchDescriptionSource
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch.substitutions import PathJoinSubstitution
from launch.substitutions import TextSubstitution
from launch_ros.actions import Node
from launch_ros.actions import PushROSNamespace
from launch_ros.actions import SetParameter
from launch_ros.actions import SetParametersFromFile
from launch_ros.actions import SetRemap
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
  namespace = LaunchConfiguration('namespace')

  frame_prefix = LaunchConfiguration('frame_prefix')

  frame_prefix_arg = DeclareLaunchArgument(
      "frame_prefix", default_value="echo/"
  )

  enable_bridge = LaunchConfiguration('enable_bridge')
  fcu_url = LaunchConfiguration('fcu_url')
  gcs_url = LaunchConfiguration('gcs_url')

  is_simulator = LaunchConfiguration('is_simulator')

  namespace_arg = DeclareLaunchArgument(
    "namespace", default_value=TextSubstitution(text="echo")
  )

  enable_bridge_arg = DeclareLaunchArgument(
    "enable_bridge", default_value=TextSubstitution(text="false")
  )

  fcu_url_arg = DeclareLaunchArgument(
    "fcu_url", default_value=TextSubstitution(text="/dev/ttyUSB0:57600")
  )

  gcs_url_arg = DeclareLaunchArgument(
    "gcs_url", default_value=TextSubstitution(text="")
  )

  is_simulator_arg = DeclareLaunchArgument(
    "is_simulator", default_value=TextSubstitution(text="false")
  )

  return LaunchDescription([
    namespace_arg,
    frame_prefix_arg,
    enable_bridge_arg,
    fcu_url_arg,
    gcs_url_arg,
    is_simulator_arg,
    GroupAction(
      actions=[
        PushROSNamespace(namespace),
        SetParametersFromFile(
          filename=PathJoinSubstitution([
            FindPackageShare('echoboat_project11'),
            'config',
            'echo.yaml'
          ])
        ),
        SetParametersFromFile(
          filename=PathJoinSubstitution([
            FindPackageShare('echoboat_project11'),
            'config',
            'sim_echo.yaml'
          ]),
          condition=IfCondition(is_simulator)
        ),
        SetRemap(
          src = '/tf',
          dst = ['/', namespace, '/tf']
        ),
        SetRemap(
          src = '/tf_static',
          dst = ['/', namespace, '/tf_static']
        ),
        Node(
          package='mru_transform',
          executable='mru_transform_node',
          name='mru_transform',
          parameters=[{
            "map_frame": [frame_prefix,"map"],
            "base_frame": [frame_prefix,"base_link"],
            "odom_frame": [frame_prefix, "odom"],
          }],
        ),
        SetParameter(
          name="sea_surface_frame",
          value=[frame_prefix, "map_tide"]
        ),
        IncludeLaunchDescription(
          PythonLaunchDescriptionSource(
            PathJoinSubstitution([
              FindPackageShare('mru_transform'),
              'launch',
              'sea_surface_estimator_launch.py'
            ])
          ),
          launch_arguments={
            'namespace': namespace,
            'enable_bridge': enable_bridge,
          }.items()
        ),
        IncludeLaunchDescription(
          PythonLaunchDescriptionSource(
            PathJoinSubstitution([
              FindPackageShare('project11'),
              'launch',
              'robot_core_launch.py'
            ])
          ),
          launch_arguments={
            'namespace': namespace,
            'enable_bridge': enable_bridge,
          }.items()
        ),
        IncludeLaunchDescription(
          AnyLaunchDescriptionSource(
            PathJoinSubstitution([
              FindPackageShare('mavros'),
              'launch',
              'apm.launch'
            ])
          ),
          launch_arguments={
            "fcu_url": fcu_url,
            "gcs_url": gcs_url,
            "namespace": 'mavros'
          }.items()
        ),
        IncludeLaunchDescription(
          PythonLaunchDescriptionSource(
            PathJoinSubstitution([
              FindPackageShare('echo_helm'),
              'launch',
              'echo_helm_launch.py'
            ])
          ),
        ),
      ]
    ),
    IncludeLaunchDescription(
      PythonLaunchDescriptionSource(
        PathJoinSubstitution([
          FindPackageShare('echoboat_project11'),
          'launch',
          'nav2_bringup_launch.py'
        ])
      ),
      launch_arguments={
        'namespace': namespace,
        'use_namespace': 'true',
        'use_composition': 'False',
      }.items()
    )

  ])

# <launch>
#   <group ns="$(arg namespace)">
#     <rosparam command="load" file="$(find echoboat_project11)/config/echo.yaml"/>

#     <param name="udp_bridge/remotes/operator/connections/default/host" value="$(arg operator_host)"/>

#   </group>
# </launch>
