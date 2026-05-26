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
