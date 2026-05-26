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
