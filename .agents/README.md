# Agent Guide: seafloor_echoboat_project11

> Generic project11 support for Seafloor Systems EchoBoat ASVs: launch files,
> nav2 configuration, and the `echo_helm` FCU interface node. Per-boat
> (instance) configuration lives in `unh_echoboats_project11`, not here.

## Workflow

**When this repo is checked out as part of a
[ROS 2 Agent Workspace](https://github.com/rolker/ros2_agent_workspace)**,
workflow rules (worktree vs. field mode, branch naming, etc.) are
defined in the workspace `AGENTS.md`. To determine the active mode
before editing, run the detection script **from the workspace root**:

```bash
.agent/scripts/field_mode.sh --describe layers/main/platforms_ws/src/seafloor_echoboat_project11
```

**Standalone use** (this repo cloned alone, outside the workspace): only
this repo's own conventions apply.

## Package Inventory

| Package | Language | Description |
|---------|----------|-------------|
| `echoboat_project11` | launch/config (ament_cmake, no compiled code) | Launch files and nav2/mavros/platform configuration for EchoBoat ASVs; pytest suite for the param-compose mechanism and launch wiring |
| `echo_helm` | C++ | `echo_helm` lifecycle node: bridges `marine/control/cmd_vel` to the ArduPilot FCU via mavros, manages arming/mode on standby transitions |

## Repository Layout

```
seafloor_echoboat_project11/
├── echoboat_project11/
│   ├── config/            # nav2_params.{base,160,240}.yaml, echo.yaml, mavros.yaml, sim_echo.yaml
│   ├── launch/            # echo_launch.py (entry), nav2_bringup_launch.py, param_compose.py, sim_echo_launch.py
│   ├── scripts/           # sim helper
│   └── test/              # test_param_compose.py, test_launch_wiring.py
└── echo_helm/
    ├── src/echo_helm_node.cpp
    ├── launch/echo_helm_launch.py
    └── scripts/           # ArduPilot docker sim helper
```

## Architecture Overview

**Fleet context**: BizzyBoat = EchoBoat **240** (vectored thrust, the deployed
daily boat); IzzyBoat = EchoBoat **160** (skid-steer/differential, the test
boat). Both run the same `echo_helm`; hull differences live in per-model nav2
overlays.

**Per-hull param composition** (repo issue #3): `nav2_bringup_launch.py`
imports `param_compose.py` and merges, lowest → highest precedence:

```
nav2_params.base.yaml → nav2_params.<model>.yaml → optional instance overlay
```

Dicts merge key-by-key; scalars and **lists/footprint strings replace
wholesale**. The `model` launch arg (`echo_launch.py`, choices `160`/`240`,
default `240`) selects the overlay; a boat-instance package can pass a third
per-instance layer.

**`echo_helm` node** (lifecycle; configure → activate):

| Interface | Name | Type |
|---|---|---|
| sub | `marine/control/cmd_vel` | `geometry_msgs/TwistStamped` |
| sub | `piloting_mode/standby/active` | `std_msgs/Bool` |
| sub | `mavros/state` | `mavros_msgs/State` |
| pub | `mavros/setpoint_velocity/cmd_vel` | `geometry_msgs/TwistStamped` |
| pub | `mavros/rc/override` | `mavros_msgs/OverrideRCIn` |
| pub | `marine/status/helm` | `marine_interfaces/Heartbeat` |
| client | `mavros/cmd/arming`, `mavros/set_mode`, `mavros/set_stream_rate` | |

- Parameter `rc_mode` (default `false`): `false` = forward cmd_vel to
  mavros velocity setpoints (GUIDED); `true` = synthesize RC override
  (scales 2 m/s → full throttle, 1.5 rad/s → full rudder, with deadband
  compensation) and use MANUAL.
- **Standby → active** transition: disarm → set mode (GUIDED or MANUAL per
  `rc_mode`) → re-arm. **Entering standby runs the same
  disarm → set MANUAL → re-arm chain** (`standbyCallback` reuses
  `armFromGuidedCallback`), so the net standby state is **armed, in
  MANUAL** — consistent with "Standby hands the boat to manual RC
  control" (an armed FCU is what makes RC driving live). While in
  standby, incoming cmd_vel is dropped.
- Heartbeat on `marine/status/helm` relays FCU state as key/values
  (`marine_autonomy_standby`, `connected`, `armed`, `guided`, `mode`) at the
  `mavros/state` rate; on activate the node requests a 10 Hz stream rate.

**Deployed consumer**: `unh_echoboats_project11` (BizzyBoat/IzzyBoat instance
repo) includes `echo_launch.py` from its platform launches and supplies the
namespace (`bizzy`), frame prefix, FCU URL, and instance overlays.

## Key Files to Read First

1. `echoboat_project11/launch/echo_launch.py` — entry point; declares
   `namespace`, `frame_prefix`, `model`, `is_simulator`, `enable_bridge`, FCU/GCS URLs
2. `echoboat_project11/launch/param_compose.py` — the base/overlay merge (unit-tested)
3. `echoboat_project11/config/nav2_params.240.yaml` — deployed-boat overlay,
   heavily annotated with tuning provenance
4. `echo_helm/src/echo_helm_node.cpp` — the FCU bridge (298 lines)
5. `echoboat_project11/config/echo.yaml` — platform_sender/mru_transform/helm_manager params

## Build & Test

```bash
# From the layer workspace directory (layers/main/platforms_ws/)
colcon build --symlink-install --packages-select echoboat_project11 echo_helm
source ../../../.agent/scripts/setup.bash && colcon test --packages-select echoboat_project11 && colcon test-result --verbose
```

- `echoboat_project11`'s pytest suite (`test_param_compose.py`,
  `test_launch_wiring.py`) needs `python3-yaml` only — no ROS graph.
- `echo_helm` builds against `marine_interfaces` (from
  `unh_marine_autonomy`, core layer) and mavros.

## Cross-Layer Dependencies

| Package | Depends On | Layer | What It Uses |
|---------|-----------|-------|--------------|
| `echo_helm` | `marine_interfaces` | core | `Heartbeat`/`KeyValue` msgs |
| `echo_helm` | `mavros`, `mavros_msgs` | system/underlay | FCU bridge topics + services |
| `echoboat_project11` | `marine_autonomy`, `marine_nav_ca_safety`, `nav2_bringup` | core / system | exec-time launch includes |

## Common Pitfalls

- **The `model` arg defaults to `240`, and Izzy's instance-launch wiring is a
  pending follow-up** — until it lands, an Izzy bringup that doesn't pass
  `model:=160` silently runs the 240 overlay (noted in `echo_launch.py`).
- **Tuning provenance is per-model and annotated in the overlays** — the 240
  overlay's footprint is URDF-derived (replacing the Izzy-inherited one) and
  its velocity_smoother yaw-accel was field-tuned 2026-06-02 **with an explicit
  snap-roll watch item**: sharp yaw reversals were NOT validated, and the sim
  doesn't model hull roll. Read the overlay comments before "cleaning them up"
  or changing values; they are the tuning record.
- **Lists replace wholesale in the param compose** — an overlay that sets
  `max_velocity` must restate the whole array, not one element.
- **`config/platform.yaml` is loaded by no launch file** in this repo (the
  live platform dimensions come from `echo.yaml`'s `platform_sender`) — treat
  it as legacy; don't extend it.
- **`nav2_bringup_launch.py` is adapted from upstream nav2_bringup** and
  carries its Apache-2.0/Intel header while the packages declare BSD — keep
  the header intact when editing.
- **Standby leaves the FCU armed in MANUAL** (disarm → MANUAL → re-arm; see
  `standbyCallback` / `armFromGuidedCallback` reuse in
  `echo_helm_node.cpp`). This makes RC driving live in standby — treat any
  change to the arming chain as safety-relevant and get on-water sign-off.
- **`echoboat_project11` declares `marine_autonomy` as `exec_depend` only**
  (launch-time include). A gratuitous `find_package(marine_autonomy
  REQUIRED)` was removed from its CMakeLists during onboarding — don't
  reintroduce build-time deps into this config-only package.
- The Nav2/behavior rates here are marine-vessel rates (~10 Hz class), not
  Nav2's small-indoor-robot defaults — don't bump them to match upstream
  examples (workspace knowledge: `ros2_development_patterns.md`).
