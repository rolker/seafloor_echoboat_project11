---
issue: 25
---

# Issue #25 — Wire reflex PointCloud2 into nav2 Collision Monitor + tune polygons (Phase B)

## Plan Authored
**Status**: complete
**When**: 2026-05-26 09:54 -04:00
**By**: Claude Code Agent (Claude Opus 4.7 (1M context))

**Plan**: `.agent/work-plans/issue-25/plan.md` at `3395573`
**PR**: https://github.com/rolker/seafloor_echoboat_project11/pull/26 (`[PLAN]` prefix)
**Phases**: single PR (part of #170; pairs with Phase A #178 and Phase C #169)

### Open questions
- [ ] Reverse/back-off action vs slowdown+stop for obstacles already inside ~15 m coast-down distance.
- [ ] Shared polygon block vs bizzy-specific override.
- [ ] Final polygon geometry numbers (sim + Phase C trigger catalog).
- [ ] base_frame_id ("base_footprint") correctness in bizzy's namespaced TF tree.

## Local Review (Pre-Push)
**Status**: complete
**When**: 2026-05-26 10:39 -04:00
**By**: Claude Code Agent (Claude Opus 4.7 (1M context))
**Verdict**: approved (draft held for sim verification)

**Branch**: feature/issue-25 at `584775b`
**Mode**: pre-push
**Depth**: Standard (reason: safety-critical collision-monitor behavior change)
**Must-fix**: 0 | **Suggestions**: 2

### Findings
- [x] (debunked) Copilot "YAML syntax error" on `base_frame_id: <tf_prefix>/base_link` — false positive; raw file parses, unquoted `<tf_prefix>` is the file-wide convention (10+ pre-existing lines).
- [ ] (tune) `min_points` direction contested across reviewers (too-high vs too-low) — resolve in sim/Phase-C; bias toward sensitivity (missed obstacles are the collision risk) — `nav2_params.yaml` CollisionSlowdown/CollisionStop
- [ ] (limitation) suddenly-appearing obstacle inside ~5 m still coasts past `stop`; consider graduated inner slowdown in tuning, not a larger stop zone (#88) — `nav2_params.yaml`
- [ ] (gate) sim-verify cmd_vel slow/stop on forward danger sector + RC path unaffected before PR leaves draft
