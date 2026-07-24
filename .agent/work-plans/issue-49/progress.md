---
issue: 49
---

# Issue #49 — docs: create .agents/README.md agent guide (none exists)

## Local Review (Pre-Push)
**Status**: complete
**When**: 2026-07-24 17:23 -04:00
**By**: Claude Code Agent (Claude Fable 5)
**Verdict**: approved (after fixes)

**Branch**: feature/issue-49 at `d7801dc`
**Mode**: pre-push
**Depth**: Standard-equivalent (onboarding: config + CI + source-verified guide)
**Must-fix**: 3 (all fixed pre-push) | **Suggestions**: 1 (fixed)
**Round**: 1 | **Ship**: recommended — all findings addressed; hooks green repo-wide; 26/26 pytest after reformat; YAML parse-structure verified unchanged

### Findings
- [x] (must-fix) CI would fail: gratuitous find_package(marine_autonomy REQUIRED) in config-only CMakeLists — removed at root cause
- [x] (must-fix) repo failed its own new black/flake8 hooks — sources brought green; vendored nav2 launch files excluded from formatters
- [x] (must-fix) guide claimed standby disarms; code re-arms in MANUAL — guide corrected (safety-relevant)
- [x] (suggestion) dep table mislabeled marine_autonomy — resolved by the CMake fix + pitfall note
