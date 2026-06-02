---
issue: 36
---

# Issue #36 — Re-enable velocity_smoother in the cmd_vel chain (disabled in #27)

## Plan Authored
**Status**: complete
**When**: 2026-06-02 09:40 -0400
**By**: Claude Code Agent (Claude Opus 4.8 (1M context))

**Plan**: `.agent/work-plans/issue-36/plan.md` at `49cc506`
**PR**: https://github.com/rolker/seafloor_echoboat_project11/pull/37 (`[PLAN]` prefix)
**Phases**: single

### Open questions
- [ ] Yaw cap value (240): keep existing 0.45 rad/s survey cap (~3.4 m turn radius at cruise) vs the 1.0 helm clamp?
- [ ] Enable the smoother for IzzyBoat (160) too — shared launch/lifecycle re-enables both hulls (160 is in testing)?
- [ ] On-water Collision-Monitor-interaction validation: which deployment slot before the June 4 dev freeze?
