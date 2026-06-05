# Copyright (c) 2018 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, OpaqueFunction, SetEnvironmentVariable
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import LoadComposableNodes, SetParameter
from launch_ros.actions import Node
from launch_ros.actions import LifecycleNode
from launch_ros.descriptions import ComposableNode, ParameterFile
from nav2_common.launch import RewrittenYaml


def _guard_ca_safety_composition(context, *args, **kwargs):
    """ca_safety_node is a plain node, not a composable component, so it cannot
    run under composition. Fail loudly rather than silently downgrading to the
    Collision Monitor when both are requested (the composition path launches the
    CM, not ca_safety)."""
    def _truthy(name):
        return LaunchConfiguration(name).perform(context).lower() in ('true', '1')
    if _truthy('use_ca_safety') and _truthy('use_composition'):
        raise RuntimeError(
            'use_ca_safety:=true requires use_composition:=false (ca_safety_node is '
            'not a composable component). Set use_ca_safety:=false to run the '
            'Collision Monitor under composition.')
    return []


def generate_launch_description():
    # Get the launch directory
    bringup_dir = get_package_share_directory('echoboat_project11')

    namespace = LaunchConfiguration('namespace')
    use_sim_time = LaunchConfiguration('use_sim_time')
    autostart = LaunchConfiguration('autostart')
    params_file = LaunchConfiguration('params_file')
    use_composition = LaunchConfiguration('use_composition')
    container_name = LaunchConfiguration('container_name')
    container_name_full = (namespace, '/', container_name)
    use_respawn = LaunchConfiguration('use_respawn')
    log_level = LaunchConfiguration('log_level')
    # ca_safety replaces the nav2 Collision Monitor as the helm gate (#64). It is
    # the default; use_ca_safety:=false reverts to the proven Collision Monitor.
    # The two are mutually exclusive — exactly one publishes the helm topic.
    use_ca_safety = LaunchConfiguration('use_ca_safety')

    lifecycle_nodes = [
        'controller_server',
        'smoother_server',
        'planner_server',
        'behavior_server',
        # velocity_smoother re-enabled (#36), now upstream of the Collision Monitor
        # (cmd_vel_nav -> smoother -> cmd_vel_smoothed -> monitor) — see node block below
        'velocity_smoother',
        'collision_monitor',
        #'bt_navigator',
        'manda_coverage',
        'bt_task_navigator',
        'waypoint_follower',
        'docking_server',
    ]

    # When ca_safety is active the Collision Monitor is not launched, so it must
    # not be in the lifecycle manager's node list (else the manager blocks waiting
    # for a node that never appears).
    lifecycle_nodes_no_monitor = [n for n in lifecycle_nodes if n != 'collision_monitor']

    # Map fully qualified names to relative ones so the node's namespace can be prepended.
    # In case of the transforms (tf), currently, there doesn't seem to be a better alternative
    # https://github.com/ros/geometry2/issues/32
    # https://github.com/ros/robot_state_publisher/pull/30
    # TODO(orduno) Substitute with `PushNodeRemapping`
    #              https://github.com/ros2/launch_ros/issues/56
    remappings = []#('/tf', 'tf'), ('/tf_static', 'tf_static')]

    # Create our own temporary YAML files that include substitutions
    param_substitutions = {'autostart': autostart}

    configured_params = ParameterFile(
        RewrittenYaml(
            source_file=params_file,
            root_key=namespace,
            param_rewrites=param_substitutions,
            convert_types=True,
        ),
        allow_substs=True,
    )

    stdout_linebuf_envvar = SetEnvironmentVariable(
        'RCUTILS_LOGGING_BUFFERED_STREAM', '1'
    )

    declare_namespace_cmd = DeclareLaunchArgument(
        'namespace', default_value='', description='Top-level namespace'
    )

    declare_use_sim_time_cmd = DeclareLaunchArgument(
        'use_sim_time',
        default_value='false',
        description='Use simulation (Gazebo) clock if true',
    )

    declare_params_file_cmd = DeclareLaunchArgument(
        'params_file',
        # Normally overridden by nav2_bringup_launch.py, which passes the composed
        # base+per-model params (#3). This fallback points at the shared base only
        # (no hull-model overlay) for the rare standalone invocation.
        default_value=os.path.join(bringup_dir, 'config', 'nav2_params.base.yaml'),
        description='Full path to the ROS2 parameters file to use for all launched nodes',
    )

    declare_autostart_cmd = DeclareLaunchArgument(
        'autostart',
        default_value='true',
        description='Automatically startup the nav2 stack',
    )

    declare_use_composition_cmd = DeclareLaunchArgument(
        'use_composition',
        default_value='False',
        description='Use composed bringup if True',
    )

    declare_container_name_cmd = DeclareLaunchArgument(
        'container_name',
        default_value='nav2_container',
        description='the name of conatiner that nodes will load in if use composition',
    )

    declare_use_respawn_cmd = DeclareLaunchArgument(
        'use_respawn',
        default_value='False',
        description='Whether to respawn if a node crashes. Applied when composition is disabled.',
    )

    declare_log_level_cmd = DeclareLaunchArgument(
        'log_level', default_value='info', description='log level'
    )

    declare_use_ca_safety_cmd = DeclareLaunchArgument(
        'use_ca_safety',
        default_value='true',
        description='Use the marine CA safety node (marine_nav_ca_safety) as the helm '
        'gate (default). Set false to revert to the nav2 Collision Monitor.',
    )

    load_nodes = GroupAction(
        condition=UnlessCondition(use_composition),
        actions=[
            SetParameter('use_sim_time', use_sim_time),
            LifecycleNode(
                package='nav2_controller',
                executable='controller_server',
                name='controller_server',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                # Route controller output into the Collision Monitor via cmd_vel_nav,
                # not straight to the helm topic, so the monitor can gate it (#170).
                remappings=remappings + [('cmd_vel', 'cmd_vel_nav')],
                namespace="",
                emulate_tty=True
            ),
            LifecycleNode(
                package='nav2_smoother',
                executable='smoother_server',
                name='smoother_server',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                remappings=remappings,
                namespace="",
                emulate_tty=True
            ),
            LifecycleNode(
                package='nav2_planner',
                executable='planner_server',
                name='planner_server',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                remappings=remappings,
                namespace="",
                emulate_tty=True
            ),
            LifecycleNode(
                package='nav2_behaviors',
                executable='behavior_server',
                name='behavior_server',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                # Route recovery-behavior cmd_vel through the Collision Monitor too,
                # via cmd_vel_nav (#170).
                remappings=remappings + [('cmd_vel', 'cmd_vel_nav')],
                namespace="",
                emulate_tty=True
            ),
            LifecycleNode(
                package='manda_coverage',
                executable='manda_coverage_action_server',
                name='manda_coverage',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                remappings=remappings + [('soundings','sensors/deltat/soundings')],
                namespace="",
                emulate_tty=True,
            ),
            LifecycleNode(
                package='nav2_bt_navigator',
                executable='bt_navigator',
                name='bt_task_navigator',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                remappings=remappings,
                namespace="",
                emulate_tty=True
            ),
            LifecycleNode(
                package='nav2_waypoint_follower',
                executable='waypoint_follower',
                name='waypoint_follower',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                remappings=remappings,
                namespace="",
                emulate_tty=True
            ),
            # velocity_smoother re-enabled (#36), inserted UPSTREAM of the Collision
            # Monitor: it subscribes to the controller/behaviors output (cmd_vel_nav)
            # and publishes the Nav2-default cmd_vel_smoothed, which the monitor then
            # gates (collision_monitor cmd_vel_in_topic: cmd_vel_smoothed in
            # nav2_params.base.yaml) before emitting piloting_mode/autonomous/cmd_vel.
            # Output is deliberately left at cmd_vel_smoothed and NOT remapped to the
            # helm topic — remapping it there was the #27 double-publisher bug (two
            # publishers on piloting_mode/autonomous/cmd_vel: the smoother and the
            # monitor). The monitor stays the sole helm publisher.
            LifecycleNode(
                package='nav2_velocity_smoother',
                executable='velocity_smoother',
                name='velocity_smoother',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                # Input only: subscribe to the controller/behaviors output. Output
                # stays at the default cmd_vel_smoothed (do NOT route to the helm
                # topic — see comment above / #27).
                remappings=remappings + [('cmd_vel', 'cmd_vel_nav')],
                namespace="",
                emulate_tty=True
            ),
            # Helm gate, option A (fallback): the nav2 Collision Monitor.
            # Launched only when use_ca_safety:=false.
            LifecycleNode(
                package='nav2_collision_monitor',
                executable='collision_monitor',
                name='collision_monitor',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                remappings=remappings,
                namespace="",
                emulate_tty=True,
                condition=UnlessCondition(use_ca_safety),
            ),
            # Helm gate, option B (default): the marine CA safety node (#64),
            # which replaces the Collision Monitor (speed-scaled yaw-preserving
            # slowdown + reverse-assisted stop). A plain (non-lifecycle) node, so
            # it is NOT in the lifecycle manager's node list. Reads cmd_vel_smoothed
            # and publishes the helm topic via its params (sole helm publisher when
            # active). Requires non-composition (it is not a registered component).
            Node(
                package='marine_nav_ca_safety',
                executable='ca_safety_node',
                name='ca_safety',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                remappings=remappings,
                namespace="",
                emulate_tty=True,
                condition=IfCondition(use_ca_safety),
            ),
            LifecycleNode(
                package='opennav_docking',
                executable='opennav_docking',
                name='docking_server',
                output='screen',
                respawn=use_respawn,
                respawn_delay=2.0,
                parameters=[configured_params],
                arguments=['--ros-args', '--log-level', log_level],
                remappings=remappings,
                namespace="",
                emulate_tty=True
            ),
            # Two mutually-exclusive managers: the node list must match what is
            # actually launched. With ca_safety active, the Collision Monitor is
            # absent, so it is dropped from the managed list.
            Node(
                package='nav2_lifecycle_manager',
                executable='lifecycle_manager',
                name='lifecycle_manager_navigation',
                output='screen',
                arguments=['--ros-args', '--log-level', log_level],
                parameters=[{'autostart': autostart}, {'node_names': lifecycle_nodes}],
                emulate_tty=True,
                condition=UnlessCondition(use_ca_safety),
            ),
            Node(
                package='nav2_lifecycle_manager',
                executable='lifecycle_manager',
                name='lifecycle_manager_navigation',
                output='screen',
                arguments=['--ros-args', '--log-level', log_level],
                parameters=[{'autostart': autostart}, {'node_names': lifecycle_nodes_no_monitor}],
                emulate_tty=True,
                condition=IfCondition(use_ca_safety),
            ),
        ],
    )

    # NOTE (#36): this composition path mirrors the non-composition routing —
    # controller/behaviors publish cmd_vel_nav -> velocity_smoother (input remapped
    # to cmd_vel_nav, output at the default cmd_vel_smoothed) -> Collision Monitor
    # (cmd_vel_in_topic: cmd_vel_smoothed -> cmd_vel_out_topic:
    # piloting_mode/autonomous/cmd_vel, both from nav2_params.base.yaml). The smoother
    # is included here too and is present in lifecycle_nodes above. Its output is NOT
    # remapped to the helm topic (that was the #27 double-publisher bug). This path
    # stays UNUSED in the field (use_composition=False), but is kept consistent so
    # enabling composition cannot silently drop the smoother or reintroduce the bypass.
    load_composable_nodes = GroupAction(
        condition=IfCondition(use_composition),
        actions=[
            SetParameter('use_sim_time', use_sim_time),
            LoadComposableNodes(
                target_container=container_name_full,
                composable_node_descriptions=[
                    ComposableNode(
                        package='nav2_controller',
                        plugin='nav2_controller::ControllerServer',
                        name='controller_server',
                        parameters=[configured_params],
                        remappings=remappings + [('cmd_vel', 'cmd_vel_nav')],
                        # remappings=remappings + [('cmd_vel', 'piloting_mode/autonomous/cmd_vel')],
                    ),
                    ComposableNode(
                        package='nav2_smoother',
                        plugin='nav2_smoother::SmootherServer',
                        name='smoother_server',
                        parameters=[configured_params],
                        remappings=remappings,
                    ),
                    ComposableNode(
                        package='nav2_planner',
                        plugin='nav2_planner::PlannerServer',
                        name='planner_server',
                        parameters=[configured_params],
                        remappings=remappings,
                    ),
                    ComposableNode(
                        package='nav2_behaviors',
                        plugin='behavior_server::BehaviorServer',
                        name='behavior_server',
                        parameters=[configured_params],
                        remappings=remappings + [('cmd_vel', 'cmd_vel_nav')],
                    ),
                    ComposableNode(
                        package='nav2_bt_navigator',
                        plugin='nav2_bt_navigator::BtNavigator',
                        name='bt_navigator',
                        parameters=[configured_params],
                        remappings=remappings,
                    ),
                    ComposableNode(
                        package='nav2_bt_navigator',
                        plugin='nav2_bt_navigator::BtNavigator',
                        name='bt_task_navigator',
                        parameters=[configured_params],
                        remappings=remappings,
                    ),
                    ComposableNode(
                        package='nav2_waypoint_follower',
                        plugin='nav2_waypoint_follower::WaypointFollower',
                        name='waypoint_follower',
                        parameters=[configured_params],
                        remappings=remappings,
                    ),
                    # velocity_smoother re-enabled (#36), mirroring the non-composition
                    # path: input remapped to cmd_vel_nav, output left at the default
                    # cmd_vel_smoothed (NOT the helm topic — #27). The Collision Monitor
                    # remains the sole publisher on piloting_mode/autonomous/cmd_vel.
                    ComposableNode(
                        package='nav2_velocity_smoother',
                        plugin='nav2_velocity_smoother::VelocitySmoother',
                        name='velocity_smoother',
                        parameters=[configured_params],
                        remappings=remappings + [('cmd_vel', 'cmd_vel_nav')],
                    ),
                    ComposableNode(
                        package='nav2_collision_monitor',
                        plugin='nav2_collision_monitor::CollisionMonitor',
                        name='collision_monitor',
                        parameters=[configured_params],
                        remappings=remappings,
                    ),
                    ComposableNode(
                        package='opennav_docking',
                        plugin='opennav_docking::DockingServer',
                        name='docking_server',
                        parameters=[configured_params],
                        remappings=remappings,
                    ),
                    ComposableNode(
                        package='nav2_lifecycle_manager',
                        plugin='nav2_lifecycle_manager::LifecycleManager',
                        name='lifecycle_manager_navigation',
                        parameters=[
                            {'autostart': autostart, 'node_names': lifecycle_nodes}
                        ],
                    ),
                ],
            ),
        ],
    )

    # Create the launch description and populate
    ld = LaunchDescription()

    # Set environment variables
    ld.add_action(stdout_linebuf_envvar)

    # Declare the launch options
    ld.add_action(declare_namespace_cmd)
    ld.add_action(declare_use_sim_time_cmd)
    ld.add_action(declare_params_file_cmd)
    ld.add_action(declare_autostart_cmd)
    ld.add_action(declare_use_composition_cmd)
    ld.add_action(declare_container_name_cmd)
    ld.add_action(declare_use_respawn_cmd)
    ld.add_action(declare_log_level_cmd)
    ld.add_action(declare_use_ca_safety_cmd)
    # Reject the unsupported use_ca_safety + use_composition combo loudly.
    ld.add_action(OpaqueFunction(function=_guard_ca_safety_composition))
    # Add the actions to launch all of the navigation nodes
    ld.add_action(load_nodes)
    ld.add_action(load_composable_nodes)

    return ld
