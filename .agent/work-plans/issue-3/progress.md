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
