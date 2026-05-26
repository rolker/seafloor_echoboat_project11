---
issue: 27
---

# Issue #27 — Collision Monitor stranded on dead input — route autonomous cmd_vel through the monitor

## Local Review
**Status**: complete
**When**: 2026-05-26 17:12 -04:00
**By**: Claude Code Agent (Claude Opus 4.7 (1M context))
**Verdict**: changes-requested (addressed in `b213dd0`)

**PR**: #28 at `b213dd0`
**Mode**: post-PR
**Depth**: Standard (reason: autonomous e-stop safety path)
**Must-fix**: 1 | **Suggestions**: 3

Active (non-composition, `use_composition=False`) path verified correct and
field-verified: controller/behaviors → `cmd_vel_nav` → collision_monitor →
`piloting_mode/autonomous/cmd_vel` → helm; no lifecycle-manager hang, no
double-publisher. Static analysis clean for the diff (remaining flake8 hits are
pre-existing untouched lines 51/64/196/293). Cross-model adversarial (Claude +
Copilot) both flagged the composition path.

### Findings
- [x] (must-fix) Composition path left half-migrated + inaccurate NOTE comment; mirrored the smoother removal into `load_composable_nodes` so a future `use_composition=True` can't reintroduce the gating bypass — `launch/navigation_launch.py:272` (fixed in `b213dd0`)
- [x] (suggestion) Safe routing depended on unenforced `use_composition=False` default — resolved by making both paths consistent — `launch/navigation_launch.py:277`
- [x] (suggestion) No-op `('cmd_vel', …)` remap on composition `collision_monitor` removed — `launch/navigation_launch.py:351`
- [ ] (suggestion) `velocity_smoother:` param block retained in `nav2_params.yaml` while the node is gone from both paths — harmless (unused params, intentional per "re-enable later" note); leave for a future cleanup
- [ ] (note) seafloor repo has no `.pre-commit-config.yaml` and no `.agents/README.md` — pre-existing governance gaps, out of scope for #27

## Integrated Review
**Status**: complete
**When**: 2026-05-26 17:21 -04:00
**By**: Claude Code Agent (Claude Opus 4.7 (1M context))

**PR**: #28 at `60a7825`
**Sources**: 2 (GitHub Copilot reviewer @ `ffbc57a`, Local Review @ `b213dd0`; CI rollup empty)
**Cross-source confirmations**: 1
**CI**: no checks configured on seafloor repo

### Findings
- [x] (cross-confirmed: GitHub Copilot @ `ffbc57a` + Local Review @ `b213dd0` + review-code Claude/Copilot adversarial) Composition-path NOTE inaccurate + path half-migrated; resolved in `b213dd0` (velocity_smoother removed from composition path, NOTE rewritten). GitHub Copilot review is stale (against pre-fix `ffbc57a`) — `launch/navigation_launch.py`
- [ ] (suggestion, Local Review) `velocity_smoother:` param block retained in `nav2_params.yaml` — harmless unused params, intentionally deferred
- [ ] (note) Optional: reply-to/dismiss the stale Copilot review on the PR to keep the conversation clean (no code action)

### False positives
- (none) — Copilot's comment was correct, just already addressed by `b213dd0` before it posted
