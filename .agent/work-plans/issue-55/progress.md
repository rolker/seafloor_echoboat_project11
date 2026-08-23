---
issue: 55
---

# Issue #55 — Field import: seafloor_echoboat_project11 (2026-08-21)

## Local Review
**Status**: complete
**When**: 2026-08-23 00:15 -04:00
**By**: Claude Code Agent (Claude Opus)
**Verdict**: changes-requested

**PR**: #56 at `e491740`
**Mode**: post-PR
**Depth**: Deep (reason: new node on the nav/sounding vertical path + cross-repo lockstep coupling; 579 lines added)
**Must-fix**: 7 | **Suggestions**: 12

Specialists: Static Analysis (no C++ linter available on this host; compile + unit tests run instead — 15/15 pass, node syntax-checks clean against ROS jazzy), Governance, Claude Adversarial Lens A + Lens B, Copilot PR review (pre-existing on the PR, folded in). Local Adversarial skipped: Ollama server unreachable at http://localhost:11434. Plan Drift skipped: no work plan (field import).

Verified independently: mavros `global_position` and `gps_status` plugins both build their message from the same `GPS_RAW_INT` and both stamp with `uas->synchronized_header(..., time_usec)` — the cancellation argument holds and the two stamps are in fact identical, not "tens of microseconds apart". Cross-repo topic contract verified end to end: the node is launched in unh_echoboats#453 `core_launch.py` inside the `bizzy` namespace (sibling of the mavros group), so relative topics resolve to `/bizzy/mavros/...` and match `mru_transform`'s configured `mavros/global_position/global_ellipsoidal`. `mru_transform` has `sensor_names: [fcu, sbg]`, `sensor_timeout: 0.5` — the SBG failover the withholding design relies on does exist on this boat.

### Findings
- [ ] (must-fix) `GPSRAW.alt_ellipsoid` is MAVLink-v2-only and arrives as 0 when absent; no validity or plausibility gate, so correction becomes ≈ +25.8 m and the diagnostic still reports OK — `include/echo_helm/ellipsoidal_corrector.hpp:142`
- [ ] (must-fix) No fix-quality gate: neither `GPSRAW.fix_type` nor `NavSatFix.status.status` is checked, so a correction is latched from acquisition-time zeros and held for the full 30 s timeout — `src/ellipsoidal_fix_node.cpp:111-119`
- [ ] (must-fix) `correctionAge()` is unclamped and `hasCorrection()` accepts a negative age, so a backward clock step makes a stale correction permanently fresh — `include/echo_helm/ellipsoidal_corrector.hpp:82-91`
- [ ] (must-fix) `paired_` latches true and is never cleared, so `receiverUndulation()`/`mavrosUndulation()` mix samples across unpaired messages once the topics decouple — the change-detection signal silently drifts (also flagged by Copilot) — `include/echo_helm/ellipsoidal_corrector.hpp:100-116`
- [ ] (must-fix) Diagnostic level is derived only from the correction's age, never from whether output is flowing: a dead `input_topic` or a mavros respawn reports OK indefinitely — `src/ellipsoidal_fix_node.cpp:141-182`
- [ ] (must-fix, lockstep #453) No annunciator indicator for `GPS: ellipsoidal fix`, so the withholding state the operator most needs to see is invisible — `bizzyboat_project11/config/bizzyboat_annunciator.yaml`
- [ ] (must-fix, lockstep #453) The node now owns the FCU nav position but launches without `respawn`, unlike mavros and echo_helm around it; and `platform_sender` is left on uncorrected `mavros/global_position/global`, so operator-facing position and the nav solution differ by the 0.626 m being removed — `bizzyboat_project11/launch/core_launch.py`, `config/bizzyboat.yaml:72`
- [ ] (suggestion) Missing standard includes: `<limits>`/`<algorithm>` in the header, `<cstdio>`/`<cstdint>` in the node — compiles today via libstdc++ transitives only (Copilot + both lenses) — `include/echo_helm/ellipsoidal_corrector.hpp:9`
- [ ] (suggestion) Header rationale for `pair_tolerance` states mavros stamps the two topics "at publish time, tens of microseconds apart"; both plugins actually use `synchronized_header(..., time_usec)` from the same message, so the stamps are identical — fix the stated rationale — `include/echo_helm/ellipsoidal_corrector.hpp:44-48`
- [ ] (suggestion) Comment claims withholding "lets mru_transform's sensor_timeout fail over to another source" unconditionally; true only while the SBG (a temporary loaner) is fitted, and the generic `echo.yaml` configures a single sensor — `src/ellipsoidal_fix_node.cpp:126-128`
- [ ] (suggestion) Publish `pair_skew_s` and a rejected-pair count so a pairing failure is diagnosable instead of presenting as a permanent silent outage — `src/ellipsoidal_fix_node.cpp:141`
- [ ] (suggestion) Reject non-finite altitudes in `tryPair()`/`correct()`; a NaN correction passes `hasCorrection()` and publishes as a valid fix — `include/echo_helm/ellipsoidal_corrector.hpp:132-145`
- [ ] (suggestion) `.agents/README.md` not updated for a second `echo_helm` executable, its 8 parameters and 4 topics — `.agents/README.md:24-41`
- [ ] (suggestion) No startup guard against `output_topic` equal to any input topic — a field param edit creates a self-feeding loop that diverges 0.626 m per cycle — `src/ellipsoidal_fix_node.cpp:64-72`
- [ ] (suggestion) `correction_age_s`, a seconds value, is formatted by the helper named `metres()` — `src/ellipsoidal_fix_node.cpp:149-150`
- [ ] (suggestion) `install(DIRECTORY include/)` and `$<INSTALL_INTERFACE:include>` on an executable target with no `ament_export_include_directories` — export properly or drop — `CMakeLists.txt:21-31`
- [ ] (suggestion) Shared mutable state across three callbacks plus the timer is safe only under the default single-threaded spin; record the requirement or declare a callback group — `src/ellipsoidal_fix_node.cpp:183-202`
- [ ] (suggestion) `echo_helm`'s own launch file does not offer the node, so IzzyBoat and the ArduPilot sim get no correction; wiring lives only in the boat-instance repo — `launch/echo_helm_launch.py`
- [ ] (suggestion, lockstep #453) `/bizzy/mavros/global_position/global_ellipsoidal` is absent from the logger record list, so the position actually consumed is not in the data-of-record — `bizzyboat_project11/config/bizzyboat.yaml:662`
