from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.actions import GroupAction
from launch.actions import IncludeLaunchDescription
from launch.conditions import IfCondition
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch.substitutions import PathJoinSubstitution
from launch.substitutions import TextSubstitution
from launch_ros.actions import Node
from launch_ros.actions import PushROSNamespace
from launch_ros.actions import SetParametersFromFile
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
  namespace = LaunchConfiguration('namespace')
  enable_bridge = LaunchConfiguration('enable_bridge')
  fcu_url = LaunchConfiguration('fcu_url')
  gcs_url = LaunchConfiguration('gcs_url')
  tgt_system = LaunchConfiguration('tgt_system')
  tgt_component = LaunchConfiguration('tgt_component')
  pluginlists_yaml = LaunchConfiguration('pluginlists_yaml')
  config_yaml = LaunchConfiguration('config_yaml')
  fcu_protocol = LaunchConfiguration('fcu_protocol')

  is_simulator = LaunchConfiguration('is_simulator')

  namespace_arg = DeclareLaunchArgument(
    "namespace", default_value=TextSubstitution(text="echo")
  )

  enable_bridge_arg = DeclareLaunchArgument(
    "enable_bridge", default_value=TextSubstitution(text="true")
  )

  fcu_url_arg = DeclareLaunchArgument(
    "fcu_url", default_value=TextSubstitution(text="/dev/ttyUSB0:57600")
  )

  gcs_url_arg = DeclareLaunchArgument(
    "gcu_url", default_value=TextSubstitution(text="")
  )

  tgt_system_arg = DeclareLaunchArgument(
    "tgt_system", default_value=TextSubstitution(text="1")
  )

  tgt_component_arg = DeclareLaunchArgument(
    "tgt_component", default_value=TextSubstitution(text="1")
  )

  pluginlists_yaml_arg = DeclareLaunchArgument(
    "pluginlists_yaml",
    default_value=PathJoinSubstitution([
      FindPackageShare('mavros'),
      'launch',
      'apm_pluginlists.yaml'
    ])
  )

  config_yaml_arg = DeclareLaunchArgument(
    "config_yaml",
    default_value=PathJoinSubstitution([
      FindPackageShare('mavros'),
      'launch',
      'apm_config.yaml'
    ])
  )

  fcu_protocol_arg = DeclareLaunchArgument(
    "fcu_protocol", default_value=TextSubstitution(text="v2.0")
  )

  is_simulator_arg = DeclareLaunchArgument(
    "is_simulator", default_value=TextSubstitution(text="false")
  )


  return LaunchDescription([
    namespace_arg,
    enable_bridge_arg,
    fcu_url_arg,
    gcs_url_arg,
    tgt_system_arg,
    tgt_component_arg,
    pluginlists_yaml_arg,
    config_yaml_arg,
    fcu_protocol_arg,
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
      Node(
        package="mavros",
        executable='mavros_node',
        name="mavros",
        parameters=[{
          "fcu_url": fcu_url,
          "gcs_url": gcs_url,
          "target_system_id": tgt_system,
          "target_component_id": tgt_component,
          "fcu_protocol": fcu_protocol
        },
        pluginlists_yaml,
        config_yaml
        ]
      ),

      Node(
        package="echo_helm",
        executable="echo_helm_node",
        name="echo_helm"
      )
    ]
  )
  ])

# <launch>
#   <group ns="$(arg namespace)">
#     <rosparam command="load" file="$(find echoboat_project11)/config/echo.yaml"/>

#     <param name="udp_bridge/remotes/operator/connections/default/host" value="$(arg operator_host)"/>

#   </group>
# </launch>
