# Plan: Add support for Echoboat 240 (split generic config 160/240)

## Issue

https://github.com/rolker/seafloor_echoboat_project11/issues/3

## Context

`echoboat_project11` holds the **generic** EchoBoat config + launch. A single shared
config set serves both hulls today, with inline comments hand-disambiguating boats —
every per-boat edit needs cross-boat reasoning. We split it by hull model (160/240),
selected by a launch arg. BizzyBoat (**240**) runs daily (June 4 freeze): the 240
profile must reproduce today's behavior exactly. IzzyBoat (**160**) stays launchable.
Per-instance config already lives in `unh_echoboats_project11/{bizzy,izzy}boat_project11`;
those instances `IncludeLaunchDescription` the generic launches.

## Approach

1. **Phase 0 — spike + decide mechanism (gates the rest).** Prototype loading two param
   files through Nav2's `RewrittenYaml` (shared base + per-model overlay) in a throwaway
   branch; confirm per-param override works for our cases (leaf scalars, the
   `velocity_smoother` arrays, nested `collision_monitor` polygon points, and a replaced
   `local_costmap.plugins` list). Produce the concrete 160-vs-240 **delta table**.
   - If the overlay plumbing is clean → **overlay** (base = today's values, so 240 = base
     + empty/near-empty overlay = byte-equivalent; 160 overlay carries deltas).
   - If fragile (RewrittenYaml + multi-file substitution misbehaves) → **full per-model
     sets**, 240/ = byte-identical copy of today.
   Record the decision + evidence in `## Implementation Notes`.
2. **Recover 160 values via git archaeology.** For each model-specific knob, `git
   log`/`blame` the value. Changed-for-240 knobs recover their 160 value (validated:
   `minimum_turning_radius` 160=`3.0`; 4 `sea_surface_layer`s added for Bizzy #18 → 160 =
   none/forward-only; `default_speed` 160=`0.75`). Never-changed-since-origin knobs
   (`robot_radius`, footprint, `velocity_smoother` limits — all from the 2025-03 origin)
   stay in shared base; flag any that look hull-wrong for the 240 as re-tuning follow-ups
   (do **not** change pre-freeze).
3. **Add `model:=160|240` arg** to the generic launch(es) (`echo_launch.py`,
   `nav2_bringup_launch.py`); select the per-model config/overlay from it. Default `240`
   (current behavior) to keep any direct callers safe.
4. **Thread the arg from instances.** `bizzyboat_project11` include passes `model: '240'`;
   `izzyboat_project11` passes `model: '160'`.
5. **Consolidate duplicated knobs** to one home per model: hull dims (today in
   `platform.yaml`, `echo.yaml`, footprint — 3 inconsistent values), turning radius
   (`minimum_turning_radius`/`turn_radius`/`radius`), PID (`nav2_params` vs `echo.yaml`).
6. **Verify 240 regression-safety**: diff effective loaded params (240 selection) against
   today's; assert byte/param equivalence before the PR is ready.

## Files to Change

| File | Change |
|------|--------|
| `echoboat_project11/launch/echo_launch.py` | Add `model` arg; select per-model config |
| `echoboat_project11/launch/nav2_bringup_launch.py` | Accept/forward `model`; load base + overlay (or per-model set) |
| `echoboat_project11/config/**` | Split into base + `160`/`240` (mechanism per Phase 0); consolidate dup knobs |
| `unh_echoboats_project11/bizzyboat_project11/launch/*.py` | Pass `model: '240'` on generic includes |
| `unh_echoboats_project11/izzyboat_project11/launch/*.py` | Pass `model: '160'` on generic includes |
| `echoboat_project11/README` / docs | Document selector + per-model values |

## Principles Self-Check

| Principle | Consideration |
|---|---|
| A change includes its consequences | Threads the arg through both instance packages (cross-repo); consolidates duplicated knobs rather than leaving new drift. |
| Improve incrementally | 240 unchanged (regression-safe); 160 recovered now but re-tuning candidates deferred as follow-ups, not forced pre-freeze. |
| Test what breaks | Phase 0 spike de-risks the launch plumbing; explicit 240 effective-param equivalence check before ready. |
| Workspace vs. project separation | Generic (echoboat) vs instance (bizzy/izzy) boundary preserved; model split lives in the generic package, instances only select. |
| Only what's needed | One generic launch + model arg, not duplicated launch files, unless Phase 0 finds launch-level (not just param) model differences. |

## ADR Compliance

| ADR | Triggered | How addressed |
|---|---|---|
| Workspace ADR-0013 (progress.md vocabulary) | Yes (procedural) | `## Plan Authored` entry; later `## Local Review`/`## Integrated Review`. |
| Project-level ADRs | No | seafloor has no `docs/decisions/`. (Onboarding gap tracked separately.) |

## Consequences

| If we change... | Also update... | Included? |
|---|---|---|
| Generic launch arg surface (`model`) | Both instance packages' includes (cross-repo, `unh_echoboats_project11`) | Yes (steps 4) |
| Config file layout/paths | `echoboat_project11/CMakeLists.txt` install() of config dir | Yes (verify during impl) |
| Footprint/dynamics homes (consolidation) | Any node reading the now-removed duplicate (e.g. `navigator` in `echo.yaml`) | Yes (step 5) |

## Open Questions

- Cross-repo PRs: the instance-launch changes land in `unh_echoboats_project11` — separate
  PR there, or sequence with this one? (Leaning: this seafloor PR first with a back-compat
  `240` default so instances keep working unchanged; instance PR follows.)
- Which never-revisited knobs (e.g. `robot_radius`, footprint) are genuinely wrong for the
  240 and worth a re-tuning follow-up vs genuinely shared — surface the candidates after
  the delta table exists.

## Estimated Scope

Multiple PRs: (1) seafloor split + `model` arg with `240` default (this issue, regression-safe);
(2) `unh_echoboats_project11` instances pass their model; (3) optional 240 re-tuning follow-ups.
