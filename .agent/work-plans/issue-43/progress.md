---
issue: 43
---

# Issue #43 — Configure & wire the CA safety node for EchoBoat 240

## Local Review (Pre-Push)
**Status**: complete
**When**: 2026-06-05 01:24 -04:00
**By**: Claude Code Agent (Claude Opus 4.8 (1M context))
**Verdict**: changes-requested → resolved

**Branch**: feature/issue-43 (seafloor PR #44 + companion unh_echoboats PR #227)
**Mode**: pre-push (both repos' diffs)
**Depth**: Deep (reason: deployed-boat safety config, replaces sole helm publisher, cross-repo)
**Must-fix**: 1 | **Suggestions**: 3

### Findings
- [x] (must-fix) Composition path launched the Collision Monitor unconditionally + never launched ca_safety → nav2_bringup's contradictory defaults (use_composition=True + use_ca_safety=true) silently downgraded ca_safety→CM (Claude+Copilot cross-confirmed). Fixed `2c14e23`: use_composition default→False + hard OpaqueFunction guard erroring on the use_ca_safety+composition combo.
- [x] (suggestion) Boat launcher had no one-line revert knob → `5df5e33` threads use_ca_safety through bizzy nav_launch.py.
- [x] (suggestion) base_frame=base_link relies on tf base_link←base_link_level — same dependency the CM already had; node fails safe on missing tf (catches TransformException → passthrough, verified in #64). No change.
- [x] (suggestion) viz defaults — publish_visualization defaults on; polygon topics match CAMP's existing reflex-zone overlay. No change.

### Notes
- Non-composition (field) path verified correct by both reviewers: mutually-exclusive helm publisher, consistent lifecycle-manager node lists, matching param block/topics/frames/odom, preserved CM fallback.
- test_launch_wiring.py passes (incl. new ca_safety assertions). Pre-existing, NOT from #43: test_param_compose::test_merged_240_hull_values fails (nav2_params.240.yaml yaw accel/decel z=3.0/-3.0 vs #36-intent 0.5) — flagged for separate triage.
