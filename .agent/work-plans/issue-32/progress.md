---
issue: 32
---

# Issue #32 — navigation_launch.py: eval-based use_composition condition crashes on lowercase booleans (use UnlessCondition)

## Local Review (Pre-Push)
**Status**: complete
**When**: 2026-05-27 20:16 -04:00
**By**: Claude Code Agent (Claude Opus 4.7 (1M context))
**Verdict**: approved

**Branch**: feature/issue-32 at `f60595d`
**Mode**: pre-push
**Depth**: Light (reason: 4 lines, 2 launch files, low risk)
**Must-fix**: 0 | **Suggestions**: 0

### Findings
- [ ] No issues found. LGTM.

Static analysis (flake8, ament max-line-length=99): findings only on
untouched pre-existing lines; touched lines clean; no F401 unused-import.
Fresh-context adversarial sub-agent: confirmed UnlessCondition is
semantically equivalent for all valid booleans, the composed/non-composed
halves remain mutually exclusive and exhaustive (line 131 vs 284), imports
consistent, and loud failure on truly-invalid input preserved. Behavior
verified empirically (false/False/true/True/0/1) and old eval path
reproduced the NameError.
