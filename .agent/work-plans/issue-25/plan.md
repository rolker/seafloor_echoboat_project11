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
     min_height: -2.0        # spans the projected plane (z≈0 in base_link_level)
     max_height:  2.0        # + pitch-induced z-shift of far points in base_link
     enabled: True
   ```
   The shared-params change is a no-op for izzy until it sets
   `publish_pointcloud: true` and remaps its reflex cloud to the same canonical
   topic — documented inline. **Done.**

2. **Tune polygons for boat momentum** — **DONE** (geometry is a starting
   point, refine in sim). Passive coast-down is ~10–15 m from ~1.5 m/s cruise
   (#124 §2 / PR #172), so favor slowdown over hard stop, forward-arc only.
   `FootprintApproach` (the old default, blind on the nonexistent `scan`) is
   **replaced** by two explicit forward-sector polygons:
   - `CollisionSlowdown` — x∈[0, 20] m, |y|≤3 m, `slowdown_ratio: 0.3`,
     `min_points: 4`. Bleeds speed well before stopping distance.
   - `CollisionStop` — x∈[0, 5] m, |y|≤2 m, `min_points: 5`. Last resort.

   `min_points` reflects the sparse reflex cloud (~tens of pts; Phase A measured
   ≤~53). **Reverse-action question resolved**: nav2_collision_monitor has no
   reverse action (stop/slowdown/limit/approach only), so the early-standoff
   slowdown zone *is* the momentum mechanism.

3. **Fix `base_frame_id`** — **DONE**. Was hardcoded `"base_footprint"` (no
   prefix, frame absent from the boat TF trees); changed to
   `<tf_prefix>/base_link`, matching every other frame in this file
   (costmaps + docking). The reflex cloud (`bizzy/base_link_level`) is
   TF-transformed into it; small pitch/roll error is folded into the height
   bracket + polygon-sizing budget.

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
| Polygon block (shared) | If bizzy/izzy tuning must diverge, add a per-boat override | Kept shared (revisit if needed) |
| `base_frame_id` | Use a frame present in both boats' TF trees | Done — `<tf_prefix>/base_link` |

## Open Questions

- [x] **Reverse vs stop** — resolved: CM has no reverse action; rely on the
  early-standoff slowdown zone. (Active crash-stop braking still uncharacterized
  — #88.)
- [x] **Shared vs per-boat polygons** — resolved for now: kept shared (izzy is a
  no-op until it opts in). Revisit only if bizzy/izzy tuning must diverge.
- [ ] **Polygon geometry + slowdown_ratio + min_points + height bracket** — the
  landed values (20 m slowdown / 5 m stop / ratio 0.3 / ±2 m height) are
  estimates. Finalize against **sim verification** and the Phase C offline
  trigger catalog (#169). This is the gate before the PR leaves draft.

## Implementation Notes

- Replaced the default `FootprintApproach` (approach-type, bound to the
  nonexistent `scan` source) with explicit `CollisionSlowdown` + `CollisionStop`
  polygons. Rationale: the issue calls for slowdown-first with a realistic
  standoff; explicit forward-sector slowdown/stop zones are easier to reason
  about and tune for boat momentum than approach-type time-to-collision on a
  sparse, forward-only cloud. No izzy regression — its monitor was already blind
  (same shared `scan` source it never publishes).

## Estimated Scope

Single PR against seafloor `jazzy`, but the polygon numbers are sim/field-tuned
— expect iteration. Pairs with Phase A (#178) and the Phase C trigger catalog.
