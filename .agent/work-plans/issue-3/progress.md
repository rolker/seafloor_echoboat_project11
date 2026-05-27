---
issue: 3
---

# Issue #3 — Add support for Echoboat 240

## Plan Authored
**Status**: complete
**When**: 2026-05-26 18:37 -04:00
**By**: Claude Code Agent (Claude Opus 4.7 (1M context))

**Plan**: `.agent/work-plans/issue-3/plan.md` at `4c32efa`
**PR**: https://github.com/rolker/seafloor_echoboat_project11/pull/29 (`[PLAN]` prefix)
**Phases**: 3 (seafloor split + model arg; instance includes pass model; optional 240 re-tuning follow-ups)

### Open questions
- [ ] Cross-repo sequencing: instance-launch changes land in `unh_echoboats_project11` — separate PR sequenced after this one (back-compat `240` default keeps instances working meanwhile)?
- [ ] Which never-revisited knobs (`robot_radius`, footprint, velocity limits) are genuinely wrong for the 240 and worth a re-tuning follow-up vs genuinely shared — decide after the Phase 0 delta table exists.

## Plan Review
**Status**: complete
**When**: 2026-05-26 18:46 -04:00
**By**: Claude Code Agent (Claude Opus 4.7 (1M context)) (in-context — author self-review)

**Plan**: `.agent/work-plans/issue-3/plan.md` at `f536179`
**PR**: https://github.com/rolker/seafloor_echoboat_project11/pull/29
**Verdict**: approve-with-suggestions

### Findings
- [ ] (suggestion) Model hook is `nav2_bringup_launch.py:118-119` (`config/nav2_params.yaml`); `navigation_launch.py`'s `params/nav2_params.yaml` default is vestigial — drop/clarify it in the plan's file list — `plan.md` Files table
- [ ] (suggestion) Consider splitting PR1 into structure+model-arg (regression-safe) vs duplicate-knob consolidation — `plan.md` Approach step 5 / Estimated Scope
- [ ] (suggestion) `sim_echo.yaml` is empty — note its disposition in the Phase 0 delta table — `plan.md` Approach step 1
- Confirmed independently: nav2_bringup wraps a single `source_file` in `ReplaceString`+`ParameterFile`, so the Phase 0 overlay spike is genuinely necessary (not a gap — validates the plan).

## Local Review (Pre-Push)
**Status**: complete
**When**: 2026-05-26 20:47 -04:00
**By**: Claude Code Agent (Claude Opus 4.7 (1M context))
**Verdict**: approved

**Branch**: feature/issue-3 at `27d3de9`
**Mode**: pre-push
**Depth**: Standard (reason: deployed boat's autonomous nav2 launch/params)
**Must-fix**: 0 | **Suggestions**: 3 (all addressed)

Step 1 implemented: model-based param composition + nav2_params base/160/240 split.
Adversarial wiring verified correct (Claude: lazy substitution resolution vs OpaqueFunction
ordering is sound; composed temp file reaches both composition + non-composition node paths).
Behavior preservation verified: merge(base,240) semantically identical to the pre-split
nav2_params.yaml (279 leaf params, zero diffs). Copilot's "byte-equivalence" High = false
positive (nav2 needs semantic, not byte, equivalence). Field watch-item resolved: gitcloud/jazzy
== origin/jazzy (c804dfa), no unmerged nav2_params.yaml edits to lose.

### Findings
- [x] (suggestion, cross-confirmed) tempfile NamedTemporaryFile(delete=False) leak — added atexit cleanup — `param_compose.py`
- [x] (suggestion, cross-confirmed) `navigation_launch.py` params_file default pointed at nonexistent `params/nav2_params.yaml` — repointed to `config/nav2_params.base.yaml` + clarifying comment — `navigation_launch.py:95`
- [x] (suggestion) `model` arg lacked validation — added `choices=['160','240']` — `nav2_bringup_launch.py`
- [ ] (carry-forward) field-reconciliation flow: when deleting generic config that field hosts also edit, confirm gitcloud is merged first (done here)

## Implementation Status (resume — 2026-05-26 21:44 -04:00)

> Not a review entry; a resume snapshot (per user request to persist state).

**Done — steps 1, 2a, 2** (all on `feature/issue-3` @ `11065bd`, PR #29; + cross-repo PR #185):

- **Mechanism (Phase 0 decision):** layered deep-merge composition, not full file copies.
  `nav2_params.base.yaml` (shared) ← `nav2_params.<model>.yaml` (hull) ← optional
  per-instance overlay (`instance_params`). Merge in `launch/param_compose.py`
  (pure, unit-tested); `nav2_bringup_launch.py` `model:=160|240` (default 240) +
  `instance_params` args drive an OpaqueFunction that composes → temp file → existing
  `ReplaceString`/`RewrittenYaml` chain. Wiring verified correct (lazy substitution vs
  OpaqueFunction ordering). 11 pytest cases (`test/test_param_compose.py`).
- **Step 1:** split `nav2_params.yaml` → base + 160 + 240. 240 ≡ today (verified). 160 is
  a placeholder copy of 240 (real values = step 3). Instances inherit `model=240` default.
- **Step 2a:** added the optional `instance_params` 3rd layer (back-compat).
- **Step 2 (cutover, atomic cross-repo):** base made rig-agnostic — sea_surface rig +
  collision reflex response removed (gating wiring kept). BizzyBoat re-adds them via
  `unh_echoboats_project11/bizzyboat_project11/config/nav2_overlay.yaml`; its
  `nav_launch.py` passes `model:=240` + `instance_params:=<overlay>`.
  → `unh_echoboats_project11#184`, branch `feature/issue-184` @ `2a04fad`, PR #185 (draft).
  **Verified:** `base + 240 + overlay` == today's `nav2_params.yaml` (zero param diffs).

**⚠ DEPLOY-TOGETHER:** PR #29 (seafloor base-reduction) and PR #185 (Bizzy overlay) must
merge **and reach gabby together**, else BizzyBoat loses its rig/reflex in the gap.

**Field entry points (verified):** Bizzy nav2 = `bizzyboat_project11/nav_launch.py` only
(`core_launch.py` references echoboat's mavros.yaml, not nav2). Izzy nav2 = via
`echo_launch.py` (gets no-rig base; `model` defaults to 240 but hull params are
placeholder-equal until step 3 — threading `model=160` to izzy is a step-3 item).

**Remaining:**
- [ ] `/review-code` on PR #29 **and** PR #185 (not yet run for the step-2 cutover; it
      relocates safety config — recommended before ready).
- [ ] Runtime smoke test (live `ros2 launch`, needs built nav2 stack): confirm Bizzy gets
      4 sea_surface layers + reflex polygons + intact gating; confirm Izzy's
      `collision_monitor` launches cleanly with empty `polygons`/`observation_sources`
      (pass-through) — **unverified at runtime**.
- [ ] **Step 3:** real 160 values into `nav2_params.160.yaml` from
      `izzyboat_project11/measurements.md` (turn radius 3–4 m, top ~2 m/s, accel ~0.6,
      rot ~1.5 rad/s, smaller footprint) + thread `model=160` to izzy via `echo_launch.py`.
- [ ] **Step 4:** 240 re-tune from `bizzyboat.urdf.xacro` geometry + measured dynamics
      (behavior change → on-water validation; timing vs June 4 freeze = open question).
- [ ] `echo.yaml` model-specific bits (platform dims, helm max_speed/yaw, navigator block,
      path_follower pid) + consolidate the duplicated hull-dim/turn-radius/pid homes (deferred).

**Open questions:** cross-repo sequencing (resolved: coordinated pair); 240-retune timing
(step 4, pre- vs post-freeze).

**Active worktrees (not removed):** `issue-seafloor_echoboat_project11-3`,
`issue-unh_echoboats_project11-184`.
