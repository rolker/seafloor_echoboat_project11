---
issue: 18
---

# Issue #18 — nav2: re-enable sea_surface_layer for bizzy's four OAK cameras

## External Review
**Status**: complete
**When**: 2026-05-21
**By**: Claude Code Agent (Claude Opus 4.7 (1M context))

**PR**: #19 — 1 review (Copilot), 1 valid finding, 0 false positives
**CI**: all pass

### Actions
- [x] Add YAML comment documenting `sea_surface_segmentation` build dependency for the shared nav2_params (Copilot finding) — committed `d7c9e1a`
- [ ] Field-verify on bizzy: confirm `controller_server` doesn't crash with all 4 layer instances loaded
