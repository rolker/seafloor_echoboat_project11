# Plan: Wire reflex PointCloud2 into nav2 Collision Monitor + tune polygons (Phase B)

## Issue

https://github.com/rolker/seafloor_echoboat_project11/issues/25

## Context

Phase B of the BizzyBoat reflex safety layer (umbrella
`rolker/unh_echoboats_project11#170`; adapter `rolker/unh_marine_perception#17`;
Phase A `rolker/unh_echoboats_project11#178`).

`echoboat_project11/config/nav2_params.yaml` already defines `collision_monitor:`
correctly gated on the autonomous cmd_vel path (`cmd_vel_smoothed` →
`piloting_mode/autonomous/cmd_vel`, ahead of the mavros setpoint). Its only
`observation_sources` is `scan` (a LaserScan BizzyBoat never publishes) and its
only polygon is `FootprintApproach` (costmap-footprint approach type). So the
monitor is **blind** today. This params file is **shared** by bizzy and izzy
(bizzy's `nav_launch.py` passes no override).

Phase A publishes/records/bridges the reflex cloud on the canonical relative
topic `collision_monitor/pointcloud` (→ `/bizzy/collision_monitor/pointcloud`).

## Approach

1. **Swap the observation source** (`nav2_params.yaml:353-358`): replace
   `observation_sources: ["scan"]` + the `scan` block with a `pointcloud`
   source on the canonical relative topic `collision_monitor/pointcloud`:
   ```yaml
   observation_sources: ["reflex_cloud"]
   reflex_cloud:
     type: "pointcloud"
     topic: "collision_monitor/pointcloud"
     min_height: -0.5        # cloud is flat at plane_z≈0 in base_link_level;
     max_height:  0.5        # bracket it generously (hull-floor vs waterline)
     enabled: True
   ```
   (Keep `FootprintApproach` as-is, or see step 2.) The shared-params change is
   a no-op for izzy until it sets `publish_pointcloud: true` and remaps its
   reflex cloud to the same canonical topic — document inline.

2. **Tune polygons for boat momentum** (`nav2_params.yaml:343-352`). Passive
   coast-down is ~10–15 m from ~1.5 m/s cruise (#124 §2 / PR #172). Favor
   **slowdown/limit over hard stop**, forward-arc only. Starting geometry
   (forward +x, ±beam in y; tune in sim/field):
   - a **`slowdown`** (or `limit`) polygon covering the forward sector out to
     **well beyond 15 m** (e.g. x∈[0, 20] m, |y|≤ ~3 m) — scales velocity down
     on first detection so the boat bleeds speed within coast-down distance;
   - a close-in **`stop`** polygon (e.g. x∈[0, 3] m) as last resort.
   `min_points` set to the cloud's realistic presence threshold (the reflex
   cloud is sparse — ~tens of points; Phase A measured ≤~53). Whether to add a
   reverse/back-off action for obstacles already inside coast-down distance is
   an **open question** (see below).

3. **Fix `base_frame_id`** (`nav2_params.yaml:331`): currently hardcoded
   `"base_footprint"` with no `<tf_prefix>`. The monitor transforms
   observations into `base_frame_id`, so it must be a real frame in bizzy's TF
   tree. Verify and, if needed, switch to `<tf_prefix>/base_link` (the
   reflex cloud is in `bizzy/base_link_level`; confirm the chain resolves).

4. **Sim-verify before field** (non-negotiable per #170): bring up nav2 +
   collision_monitor in sim, inject/replay a forward reflex cloud, and confirm
   the monitor slows/stops `piloting_mode/autonomous/cmd_vel` when points fall
   in the forward danger sector — and passes through when clear. Confirm the
   manual RC-direct-to-FCU path is unaffected.

## Files to Change

| File | Change |
|------|--------|
| `echoboat_project11/config/nav2_params.yaml` | Swap `scan`→`pointcloud` observation source on `collision_monitor/pointcloud`; add slowdown + stop polygons tuned for momentum; verify/fix `base_frame_id` |

## Principles Self-Check

| Principle | Consideration |
|---|---|
| Robustness (boat on open water) | Slowdown-first + standoff beyond coast-down distance; stop as last resort; manual path untouched. |
| Test what breaks | Sim verification of actual cmd_vel gating is the acceptance gate, not just config parse. |
| A change includes its consequences | Shared-params effect on izzy documented; `base_frame_id` correctness folded in, not deferred. |
| Capture decisions | Slowdown-over-stop, canonical-topic source, reverse-action open question recorded. |

## ADR Compliance

| ADR | Triggered | How addressed |
|---|---|---|
| 0002 — Worktree isolation | Yes | `layers/worktrees/issue-seafloor_echoboat_project11-25`. |
| 0008 — ROS 2 conventions | Yes | nav2 plugin params; canonical relative topic; sensor_msgs/PointCloud2. |
| 0013 — progress.md vocabulary | Yes | `## Plan Authored` entry appended. |

## Consequences

| If we change... | Also update... | Included? |
|---|---|---|
| Shared `collision_monitor` observation source | izzy opts in later (publish_pointcloud + remap) — no-op until then | Documented inline |
| Polygon block (shared) | If bizzy/izzy tuning must diverge, add a per-boat override | Flagged — open question |
| `base_frame_id` | Confirm frame exists in bizzy TF tree | Step 3 |

## Open Questions

- [ ] **Reverse vs stop inside coast-down distance** — for an obstacle already
  within ~15 m, zero-throttle still coasts into it. Add a reverse/back-off
  action, or accept slowdown+stop and rely on the standoff zone catching
  obstacles earlier? (Active crash-stop braking is uncharacterized — #88.)
- [ ] **Shared vs per-boat polygons** — keep one shared polygon block, or does
  bizzy's momentum/size warrant a bizzy-specific override now?
- [ ] **Polygon geometry numbers** — starting values above are estimates;
  finalize against sim + the Phase C offline trigger catalog (#169).

## Estimated Scope

Single PR against seafloor `jazzy`, but the polygon numbers are sim/field-tuned
— expect iteration. Pairs with Phase A (#178) and the Phase C trigger catalog.
