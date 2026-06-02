# Plan: Re-enable velocity_smoother in the cmd_vel chain (disabled in #27)

## Issue

https://github.com/rolker/seafloor_echoboat_project11/issues/36

## Context

During the Collision Monitor rework (#170 / #27) the Nav2 `velocity_smoother`
was pulled out of the active cmd_vel path. Today the chain is:

```
controller_server / behavior_server ──cmd_vel_nav──▶ collision_monitor ──▶ piloting_mode/autonomous/cmd_vel ──▶ helm
```

with **no acceleration or yaw smoothing**. The only active yaw governor is the
helm's `max_yaw_speed: 1.0` rad/s capability backstop, so cmd_vel steps reach the
thrusters unfiltered. Deployment #197 (2026-05-29) showed the failure mode: a
near-stop hard pivot + accel snapped yaw to the 1.0 cap as a step and induced
±30° hull roll.

The smoother caps already exist in the per-model overlays and just aren't running:
- `nav2_params.240.yaml`: yaw cap `0.45` rad/s, yaw accel `0.2` rad/s² (BizzyBoat)
- `nav2_params.160.yaml`: yaw cap `1.5` rad/s, yaw accel `1.2` rad/s² (IzzyBoat, skid-steer)

**Why it was removed (#27):** the *old* wiring had the smoother publish its output
(`cmd_vel_smoothed`) **directly to the helm topic** `piloting_mode/autonomous/cmd_vel`.
Once #170 added the Collision Monitor (which also publishes there via
`cmd_vel_out_topic`), that put **two publishers on the helm topic**. The fix here
inserts the smoother *upstream* of the monitor so the monitor stays the sole helm
publisher.

## Approach

Target chain (single helm publisher = the monitor; reflex stop still gates):

```
controller / behaviors ──cmd_vel_nav──▶ velocity_smoother ──cmd_vel_smoothed──▶ collision_monitor ──▶ piloting_mode/autonomous/cmd_vel ──▶ helm
```

1. **Re-add the `velocity_smoother` LifecycleNode** to `load_nodes` (non-composition
   path) in `navigation_launch.py`, package `nav2_velocity_smoother`, executable
   `velocity_smoother`. Remap **input only**: `('cmd_vel', 'cmd_vel_nav')` so it
   subscribes to the controller/behaviors output. Leave the output at its Nav2
   default `cmd_vel_smoothed` — **do NOT remap output to the helm topic** (that was
   the #27 double-publish bug). Place it between `behavior_server` and
   `collision_monitor` so the read order matches the data flow.
2. **Mirror the node into the composition path** (`load_composable_nodes`) as a
   `ComposableNode` with the same input remap, so a future `use_composition=True`
   can't silently drop the smoother (the #27 progress explicitly hardened this
   parity — keep it).
3. **Re-add `'velocity_smoother'`** to the shared `lifecycle_nodes` list (replacing
   the `# velocity_smoother removed ...` comment at line 49).
4. **Point the monitor at the smoother** in `nav2_params.base.yaml`:
   `collision_monitor.cmd_vel_in_topic: cmd_vel_nav → cmd_vel_smoothed` (and update
   the inline `# (#170)` comment to describe the smoother hop). `cmd_vel_out_topic`
   stays `piloting_mode/autonomous/cmd_vel`.
5. **Update the explanatory comments** in `navigation_launch.py` (the
   "intentionally removed / deliberately disconnected" blocks at lines ~49, ~229–236,
   ~275–282, ~340–342) to describe the re-enabled, smoother-upstream-of-monitor
   wiring and cite #36. Stale "deliberately disconnected" comments would actively
   mislead the next reader.
6. **Update the wiring test** `test_param_compose.py::test_base_has_no_sensor_rig_or_reflex`:
   the `cm['cmd_vel_in_topic'] == 'cmd_vel_nav'` assertion (line 189) becomes
   `'cmd_vel_smoothed'`.
7. **Add a regression test** that locks the re-enabled wiring so it can't silently
   regress to the #27 double-publisher: assert (a) `velocity_smoother` is in
   `lifecycle_nodes`, (b) the smoother's output is **not** remapped to the helm topic,
   and (c) `collision_monitor.cmd_vel_in_topic == 'cmd_vel_smoothed'` chains to the
   smoother's output. Implement by AST-parsing `navigation_launch.py` (the existing
   test already imports from `launch/`), matching the style of `test_param_compose.py`.
8. **Retest the Collision Monitor interaction in sim** — the reflex stop must still
   gate correctly with the smoother upstream (smoothed cmd_vel → monitor → helm).
   Then on-water at Lake Massabesic survey speeds before relying on it.

## Files to Change

| File | Change |
|------|--------|
| `echoboat_project11/launch/navigation_launch.py` | Re-add `velocity_smoother` node (non-composition + composition), input remap `cmd_vel→cmd_vel_nav`, re-add to `lifecycle_nodes`, refresh comments |
| `echoboat_project11/config/nav2_params.base.yaml` | `collision_monitor.cmd_vel_in_topic`: `cmd_vel_nav → cmd_vel_smoothed` + comment |
| `echoboat_project11/test/test_param_compose.py` | Update `cmd_vel_in_topic` assertion to `cmd_vel_smoothed` |
| `echoboat_project11/test/test_launch_wiring.py` (new) | Regression test locking smoother-in-lifecycle + no helm double-publish + monitor input chains to smoother |

## Principles Self-Check

| Principle | Consideration |
|---|---|
| Quality Standard — fix completely, add the test | Adds a regression test (step 7) that pins the exact failure mode (#27 double-publish), not just a config flip. Comments refreshed so the next reader isn't misled. |
| Robustness for open-water autonomy | The smoother is inserted **upstream** of the reflex Collision Monitor, so the binary safety floor still gates the final command — smoothing cannot bypass the reflex stop. Verified in sim before on-water (step 8). |
| Safety lifecycle transition | `velocity_smoother` returns to `lifecycle_nodes` so the lifecycle manager configures/activates it; if it fails to activate, the manager surfaces it rather than silently passing raw cmd_vel. |

## ADR Compliance

| ADR | Triggered | How addressed |
|---|---|---|
| ADR-0008 Follow ROS 2 official conventions | Yes | Uses the stock Nav2 `nav2_velocity_smoother` component and its default `cmd_vel_smoothed` output topic; no custom topic gymnastics. |
| ADR-0002 Worktree isolation | Yes | Work done in `feature/issue-36` worktree; this plan is the first commit on the branch. |

## Consequences

| If we change... | Also update... | Included in plan? |
|---|---|---|
| `lifecycle_nodes` + launch node block (shared, not per-model) | Both 160 (Izzy) and 240 (Bizzy) get the smoother re-enabled — Izzy's caps (yaw 1.5, accel 1.2) are already tuned, so this is consistent, but Izzy is in testing | Yes — flagged as Open Question |
| `collision_monitor.cmd_vel_in_topic` in `base.yaml` | `test_param_compose.py` assertion | Yes (step 6) |
| Re-enabled smoother sits upstream of the reflex monitor | The per-boat reflex polygons/observation_sources (supplied via the instance overlay, which lives in the deploying repo, not here) still gate the smoothed output — verify in the sim retest | Yes (step 8) |
| Smoother is a **flat per-axis limiter** (no speed-scheduling) | A single yaw cap applies at all speeds; "tight at low speed, gentle at speed" would need controller-side logic | No — out of scope, noted as separate concern in the issue |

## Open Questions

- **Yaw cap value (240):** keep the existing `0.45` rad/s survey cap (≈3.4 m turn
  radius at 1.52 m/s cruise, vs ≈1.5 m at the 1.0 helm clamp)? The issue recommends
  keeping `0.45` and leaving `max_yaw_speed: 1.0` as the helm capability backstop.
  Confirm before tuning, since it directly sets survey turn geometry.
- **Enable for IzzyBoat (160) too?** The shared launch/lifecycle list re-enables the
  smoother for both hulls. Izzy's caps are already tuned and distinct, but it's in
  testing — OK to enable now, or gate to 240 only (would require per-model launch
  logic)?
- **On-water validation window:** the sim retest is in-scope here; the on-water
  Collision-Monitor-interaction check needs a deployment slot before the June 4 dev
  freeze. Schedule under an existing deployment issue or a dedicated test run?

## Estimated Scope

Single PR (launch + base param + tests). The on-water validation (step 8) is a
follow-up field check tracked against a deployment, not a code change in this PR.
