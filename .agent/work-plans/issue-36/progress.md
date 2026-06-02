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

## Local Review (Pre-Push)
**Status**: complete
**When**: 2026-06-02 10:46 -0400
**By**: Claude Code Agent (Claude Opus 4.8 (1M context))
**Verdict**: approved
**Branch**: feature/issue-36 at `afa93bb`
**Mode**: pre-push
**Depth**: Deep (reason: safety-critical cmd_vel routing + Collision Monitor gating / lifecycle)
**Must-fix**: 0 | **Suggestions**: 4

### Findings
- [x] (suggestion) enable_stamped_cmd_vel consistency untested — Twist/TwistStamped mismatch would silently break the chain — FIXED, added `test_stamped_cmd_vel_consistent_across_chain` — `test/test_launch_wiring.py`
- [ ] (suggestion, behavioral) `scale_velocities: true` couples axes: a large yaw-accel-limited turn scales surge DOWN too (slows on straightaway) — decide scale_velocities true vs false at on-water tune — `config/nav2_params.base.yaml:277`
- [ ] (suggestion, behavioral) smoother adds `velocity_timeout: 1.0`s hold + decel ramp on `cmd_vel_smoothed` after cmd_vel_nav goes silent — autonomy-stop now decays over ~1 s; confirm prompt stop on-water — `config/nav2_params.base.yaml:282`
- [ ] (note, pre-existing/out-of-scope) composition path lists `manda_coverage` in lifecycle_nodes but never loads the component (predates #36; composition unused in field, use_composition=False) — `launch/navigation_launch.py`
- [x] (suggestion) wiring test guards literal remaps only (misses SetRemap/shared-var) and len==2 exact — accepted scope limit (launch uses literal remaps throughout)

### Adversarial verification
- Claude adversarial verified topic names against installed nav2 binaries: velocity_smoother subscribes `cmd_vel` / publishes `cmd_vel_smoothed` (input remap correct), all 4 hops TwistStamped (types match), monitor sole helm publisher in both paths, 240 caps pass on_configure validation, OPEN_LOOP = no odom activation hazard.
- Copilot adversarial (cross-model) concurred; no double-publisher, no type mismatch.
