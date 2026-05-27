# Plan: Add support for Echoboat 240 (split generic config 160/240)

## Issue

https://github.com/rolker/seafloor_echoboat_project11/issues/3

## Context

`echoboat_project11` holds the **generic** EchoBoat config + launch; one shared set
serves both hulls, hand-disambiguated by comments. BizzyBoat (**240**) runs daily
(June 4 freeze); IzzyBoat (**160**) stays available. Per-instance config lives in
`unh_echoboats_project11/{bizzy,izzy}boat_project11`; instances `IncludeLaunchDescription`
the generic launches.

## Phase 0 findings (done — git archaeology + plumbing read)

- **Timeline maps to boats**: 2025 commits = Izzy(160) era; 2026 commits = Bizzy(240) era.
  Today the two boats run **nearly identical** generic config.
- **The real deltas are few, and split three ways:**
  1. **Sensor rig + reflex** (4 OAK `sea_surface_layer`s; `collision_monitor` momentum
     polygons + `reflex_cloud`) — driven by Bizzy's camera hardware + reflex deployment,
     **not** the hull. The 4 OAK *camera* configs already live in `bizzyboat_project11`.
     → **instance-level, move out of generic.**
  2. **General corrections** mislabeled as 240 changes (`minimum_turning_radius` 3.0→1.5
     #124; pid sign-flip) — both hulls want these; don't revert for 160.
  3. **True hull-model params** (footprint, `robot_radius`, velocity/accel/rotational
     limits, hover) — currently **identical**: the 240 inherited Izzy's 160 values, never
     re-tuned. Measured truth differs (160 `measurements.md`: top ~2 m/s, accel ~0.6,
     rot ~1.5 rad/s, **turn radius 3–4 m**; 240 geometry in `bizzyboat.urdf.xacro`).
- **Mechanism decided: layered param composition** (not full per-model file copies — the
  delta is ~a dozen lines; duplication would re-create the friction). nav2_bringup wraps a
  single `source_file` in `ReplaceString`→`RewrittenYaml`; we add a launch-time deep-merge
  (we're not bound to nav2's single-file layout) that composes
  `base + hull-model-overlay + instance-overlay` → one temp YAML → the existing chain.

## Target architecture

- `config/nav2_params.base.yaml` — shared nav2 algorithm/structure (BT, controller
  plugins, planner, smoother, costmap structure, frames, collision_monitor *structure*,
  docking). All echoboats.
- `config/nav2_params.{160,240}.yaml` — hull-model overlay: footprint, `robot_radius`,
  velocity/accel/rotational limits, hover, turning radius. **160** from `measurements.md`;
  **240** geometry from `bizzyboat.urdf.xacro` + dynamics from current Bizzy-tuned values.
- Instance overlay in `bizzyboat_project11` / `izzyboat_project11` — sensor rig
  (`sea_surface_layer`s ← OAK suite) + reflex/collision polygons + observation sources.
  Bizzy = 4-OAK + reflex; Izzy = forward-only / none.
- `echo.yaml` model-specific bits (platform dims, `helm_manager.max_speed/max_yaw_speed`,
  `navigator.robot`) follow the same base/hull-overlay split; reconcile the duplicated
  hull-dim/turn-radius/pid homes (3 inconsistent copies today) to one each.

## Approach (sequenced to protect the daily boat)

1. **Composition mechanism + base/hull split, 240 ≡ today.** Add `model:=160|240` arg;
   deep-merge base+overlay in `nav2_bringup_launch.py`; 240 overlay = today's values →
   **zero behavior change**, regression-safe. Lands first.
2. **Move rig/reflex to instance overlays** (behavior-neutral refactor; cross-repo to
   `unh_echoboats_project11`). Bizzy keeps its 4-OAK + reflex via its overlay; generic
   base loses them.
3. **160 overlay from measurements** (Izzy not running daily → low risk).
4. **240 re-tune from real specs** (footprint from URDF; dynamics review). **Behavior
   change to the deployed boat → on-water re-validation; timing vs June 4 is an open
   question below.**

## Files to Change

| File | Change |
|------|--------|
| `echoboat_project11/launch/nav2_bringup_launch.py` | `model` arg + deep-merge composition of base/hull/instance params |
| `echoboat_project11/launch/echo_launch.py` | `model` arg; select hull overlay for `echo.yaml` bits |
| `echoboat_project11/config/nav2_params.{base,160,240}.yaml` | Split from today's `nav2_params.yaml` |
| `echoboat_project11/config/echo*.yaml` | Base/hull split; consolidate dup hull-dim/turn-radius/pid |
| `unh_echoboats_project11/bizzyboat_project11/{launch,config}` | Pass `model:'240'`; own rig+reflex overlay |
| `unh_echoboats_project11/izzyboat_project11/{launch,config}` | Pass `model:'160'`; own rig overlay (forward-only) |

(Review finding folded in: `navigation_launch.py`'s `params/nav2_params.yaml` default is
vestigial — it's always overridden by the forwarded `params_file`; no model logic there.)

## Principles Self-Check

| Principle | Consideration |
|---|---|
| A change includes its consequences | Cross-repo arg threading + rig-overlay moves; consolidate the 3 duplicated hull-dim/turn-radius/pid copies rather than leave drift. |
| Improve incrementally | Sequenced so the regression-safe mechanism lands first; behavior-changing 240 re-tune is isolated + validated last. |
| Test what breaks | 240≡today equivalence check at step 1; on-water re-validation gates the step-4 re-tune. |
| Workspace vs. project separation | Hull-model = generic; sensor rig/reflex = instance — the boundary the data revealed. |
| Only what's needed | Layered overlay (small deltas) not full file duplication. |

## ADR Compliance

| ADR | Triggered | How addressed |
|---|---|---|
| Workspace ADR-0013 (progress.md) | Yes (procedural) | Plan Authored / Plan Review / Local / Integrated entries. |
| Project ADRs | No | seafloor has none. |

## Consequences

| If we change... | Also update... | Included? |
|---|---|---|
| `model` arg surface on generic launch | both instance includes (cross-repo) | Yes (steps 1–2) |
| Move rig/reflex out of generic base | Bizzy must re-add via its overlay or lose segmentation/reflex | Yes (step 2) |
| Config filenames/layout | `CMakeLists.txt` install() globs in all three packages | Yes (verify in impl) |
| 240 dynamics/geometry (step 4) | on-water behavior; survey-limit assumptions (#124) | Yes — validation gate |

## Open Questions

- **240 re-tune timing (step 4)**: it changes the daily boat's behavior and needs on-water
  re-validation. Land before June 4 (validate under freeze pressure) or defer the re-tune
  to after the class, keeping 240 ≡ today's values until then? Steps 1–3 are safe either way.
- Cross-repo PR sequencing: seafloor mechanism PR first (240≡today, back-compat default),
  then the `unh_echoboats_project11` instance-overlay PR. Confirm.

## Estimated Scope

4 PRs across 2 repos: (1) seafloor composition + base/hull split (regression-safe);
(2) `unh_echoboats` instance rig/reflex overlays + model arg; (3) 160 overlay from
measurements; (4) 240 re-tune (behavior change, validated). Steps 1–2 are the structural
core; 3–4 populate values.

## Implementation Notes

- Mechanism decision (Phase 0): layered deep-merge composition chosen over full per-model
  file sets because the real 160/240 delta is ~a dozen lines and full duplication would
  re-create shared-edit friction. Evidence: `git blame` shows today's config is ~95%
  Izzy(160)-origin; the few 2026 Bizzy changes are either general corrections or
  rig/reflex (instance-level). nav2_bringup's single-`source_file` `ReplaceString` chain
  is fed the merged temp file, so the substitution path is unchanged.
