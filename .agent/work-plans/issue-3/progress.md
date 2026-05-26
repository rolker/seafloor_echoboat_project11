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
