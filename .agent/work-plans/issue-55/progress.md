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
- [x] (must-fix) `GPSRAW.alt_ellipsoid` is MAVLink-v2-only and arrives as 0 when absent; no validity or plausibility gate, so correction becomes ≈ +25.8 m and the diagnostic still reports OK — `include/echo_helm/ellipsoidal_corrector.hpp:142`
- [x] (must-fix) No fix-quality gate: neither `GPSRAW.fix_type` nor `NavSatFix.status.status` is checked, so a correction is latched from acquisition-time zeros and held for the full 30 s timeout — `src/ellipsoidal_fix_node.cpp:111-119`
- [x] (must-fix) `correctionAge()` is unclamped and `hasCorrection()` accepts a negative age, so a backward clock step makes a stale correction permanently fresh — `include/echo_helm/ellipsoidal_corrector.hpp:82-91`
- [x] (must-fix) `paired_` latches true and is never cleared, so `receiverUndulation()`/`mavrosUndulation()` mix samples across unpaired messages once the topics decouple — the change-detection signal silently drifts (also flagged by Copilot) — `include/echo_helm/ellipsoidal_corrector.hpp:100-116`
- [x] (must-fix) Diagnostic level is derived only from the correction's age, never from whether output is flowing: a dead `input_topic` or a mavros respawn reports OK indefinitely — `src/ellipsoidal_fix_node.cpp:141-182`
- [x] (must-fix, lockstep #453) No annunciator indicator for `GPS: ellipsoidal fix`, so the withholding state the operator most needs to see is invisible — `bizzyboat_project11/config/bizzyboat_annunciator.yaml` (deferred: lives in `unh_echoboats_project11`; a separate agent holds that repo — routed as a cross-repo item)
- [x] (must-fix, lockstep #453) The node now owns the FCU nav position but launches without `respawn`, unlike mavros and echo_helm around it; and `platform_sender` is left on uncorrected `mavros/global_position/global`, so operator-facing position and the nav solution differ by the 0.626 m being removed — `bizzyboat_project11/launch/core_launch.py`, `config/bizzyboat.yaml:72` (deferred: lives in `unh_echoboats_project11`; a separate agent holds that repo — routed as a cross-repo item)
- [x] (suggestion) Missing standard includes: `<limits>`/`<algorithm>` in the header, `<cstdio>`/`<cstdint>` in the node — compiles today via libstdc++ transitives only (Copilot + both lenses) — `include/echo_helm/ellipsoidal_corrector.hpp:9`
- [x] (suggestion) Header rationale for `pair_tolerance` states mavros stamps the two topics "at publish time, tens of microseconds apart"; both plugins actually use `synchronized_header(..., time_usec)` from the same message, so the stamps are identical — fix the stated rationale — `include/echo_helm/ellipsoidal_corrector.hpp:44-48`
- [x] (suggestion) Comment claims withholding "lets mru_transform's sensor_timeout fail over to another source" unconditionally; true only while the SBG (a temporary loaner) is fitted, and the generic `echo.yaml` configures a single sensor — `src/ellipsoidal_fix_node.cpp:126-128`
- [x] (suggestion) Publish `pair_skew_s` and a rejected-pair count so a pairing failure is diagnosable instead of presenting as a permanent silent outage — `src/ellipsoidal_fix_node.cpp:141`
- [x] (suggestion) Reject non-finite altitudes in `tryPair()`/`correct()`; a NaN correction passes `hasCorrection()` and publishes as a valid fix — `include/echo_helm/ellipsoidal_corrector.hpp:132-145`
- [x] (suggestion) `.agents/README.md` not updated for a second `echo_helm` executable, its 8 parameters and 4 topics — `.agents/README.md:24-41`
- [x] (suggestion) No startup guard against `output_topic` equal to any input topic — a field param edit creates a self-feeding loop that diverges 0.626 m per cycle — `src/ellipsoidal_fix_node.cpp:64-72`
- [x] (suggestion) `correction_age_s`, a seconds value, is formatted by the helper named `metres()` — `src/ellipsoidal_fix_node.cpp:149-150`
- [x] (suggestion) `install(DIRECTORY include/)` and `$<INSTALL_INTERFACE:include>` on an executable target with no `ament_export_include_directories` — export properly or drop — `CMakeLists.txt:21-31`
- [x] (suggestion) Shared mutable state across three callbacks plus the timer is safe only under the default single-threaded spin; record the requirement or declare a callback group — `src/ellipsoidal_fix_node.cpp:183-202`
- [x] (suggestion) `echo_helm`'s own launch file does not offer the node, so IzzyBoat and the ArduPilot sim get no correction; wiring lives only in the boat-instance repo — `launch/echo_helm_launch.py`
- [x] (suggestion, lockstep #453) `/bizzy/mavros/global_position/global_ellipsoidal` is absent from the logger record list, so the position actually consumed is not in the data-of-record — `bizzyboat_project11/config/bizzyboat.yaml:662` (deferred: lives in `unh_echoboats_project11`; a separate agent holds that repo — routed as a cross-repo item)

## Implementation
**Status**: complete
**When**: 2026-08-23 00:54 -04:00
**By**: Claude Code Agent (Claude Opus)

**Branch**: feature/issue-55 at `d41fe7a`
**PR**: #56
**Addressed**: `## Local Review` (post-PR, 2026-08-23 00:15 -04:00, PR #56 at `e491740`), verdict changes-requested — 7 must-fix, 12 suggestions
**Commits**: `80a39dc` `437f014` `a80e095` `f7de3df` `629426a` `1c8adeb` `d41fe7a`

**Verification**: clean `colcon build --symlink-install` of `echo_helm` against
the built layer chain (underlay → core → sensors → platforms), no warnings under
`-Wall -Wextra -Wpedantic`; `colcon test` **31 tests, 0 failures** (15 pre-existing
+ 16 new, all in `test_ellipsoidal_corrector.cpp`); `echoboat_project11` pytest
suite 26 passed (the launch change touches a file its wiring tests read);
`pre-commit run --all-files` all hooks pass. Built in a temporary worktree on
`feature/issue-55`; the main tree stayed on `jazzy` and untouched. Not pushed.

### Actions

**The vertical-path guard (findings 1–2)** — `alt_ellipsoid` is a MAVLink-v2
extension and mavros copies it unconditionally, so a v1 link delivered a
structurally valid `GPSRAW` carrying zero and the node published
`correction = 0 − (−25.8) = +25.8 m` as a good fix while the diagnostic said OK.
Three independent guards now stand between that and the output, because they fail
independently: the absent-field sentinel (`alt_ellipsoid == 0`, checked on the raw
integer where the corrector cannot see it, with a throttled warning naming the
cause), the fix-quality gate (`GPSRAW.fix_type >= GPS_FIX_TYPE_3D_FIX` and
`NavSatFix.status.status >= STATUS_FIX`), and `max_correction` (default 3 m) as a
plausibility bound on a physically sub-metre quantity. A rejected sample is
*discarded*, not ignored — left in place it would later pair with a good sample
from the other topic — and a rejected pair never disturbs a correction already
held; that one ages out on its own. Seven new tests cover it, including the
v1-link zero, the boundary at exactly `max_correction`, and the
bad-sample-then-good-sample ordering.

- [x] (must-fix) `alt_ellipsoid` absent arrives as 0; no validity or plausibility gate — `include/echo_helm/ellipsoidal_corrector.hpp` (`max_correction`, `tryPair`), `src/ellipsoidal_fix_node.cpp` (`gpsRawCallback`) — `80a39dc`, `a80e095`
- [x] (must-fix) No fix-quality gate on `fix_type` / `NavSatFix.status` — `include/echo_helm/ellipsoidal_corrector.hpp` (`quality_ok` on both setters), `src/ellipsoidal_fix_node.cpp:rawFixCallback,gpsRawCallback` — `80a39dc`, `a80e095`
- [x] (must-fix) `correctionAge()` unclamped, `hasCorrection()` accepts a negative age — `include/echo_helm/ellipsoidal_corrector.hpp:correctionAge` — `437f014`. A backward step beyond `pair_tolerance` now reports **infinite** age, not a clamped zero: clamping alone would not have fixed it, since zero is the freshest value there is. Skews inside the tolerance are ordinary stamp-vs-clock jitter and clamp to zero. Three tests, including recovery on the next coincident pair.
- [x] (must-fix) `paired_` latch mixes samples across unpaired messages — `include/echo_helm/ellipsoidal_corrector.hpp` — `80a39dc`. Both undulations are now snapshotted from the pair they describe, so the change-detection signal cannot drift silently. Latch removed entirely.
- [x] (must-fix) Diagnostic levels only on correction age, never on output flow — `src/ellipsoidal_fix_node.cpp:publishDiagnostic` — `f7de3df`. Input flow is tracked and outranks a healthy correction (nothing reaches the output either way); silence past `input_timeout` (default 3 s) or no input ever received reports ERROR naming the topic. The status also carries received/published/withheld counts, seconds since last input and last publish, the last rejection reason, the rejected-pair count and the pair skew.
- [x] (suggestion) Missing standard includes — `<algorithm>`, `<cmath>`, `<cstdint>`, `<limits>`, `<optional>` in the header; `<cstdint>`, `<cstdio>`, `<stdexcept>` in the node — `80a39dc`, `a80e095`
- [x] (suggestion) `pair_tolerance` rationale said mavros stamps the two topics "tens of microseconds apart" — `include/echo_helm/ellipsoidal_corrector.hpp` — `80a39dc`. Both plugins use `synchronized_header(..., time_usec)` from the same `GPS_RAW_INT`, so a genuine pair's stamps are **identical**. Rationale rewritten to say the tolerance absorbs clock-source differences and future divergence in how mavros stamps, not a known publish-time offset. The tolerance value and the pairing logic are unchanged; the test helper's comment was corrected to match, keeping its small non-zero skew so the pairing path is exercised rather than trivially satisfied.
- [x] (suggestion) Withholding comment overstated the `mru_transform` failover — `src/ellipsoidal_fix_node.cpp:inputCallback` — `a80e095`. Now says the failover is a property of the boat's config (a second source fitted and in `sensor_names` — the SBG is a loaner; the generic `echo.yaml` has one sensor), and that where there is no second source withholding is a deliberate diagnosed outage.
- [x] (suggestion) Publish `pair_skew_s` and a rejected-pair count — `f7de3df` (plus `pairSkew()` / `rejectedPairCount()` / `lastReject()` in `80a39dc`)
- [x] (suggestion) Reject non-finite altitudes — `include/echo_helm/ellipsoidal_corrector.hpp` — `80a39dc`; `correct()` also refuses a non-finite input altitude
- [x] (suggestion) `.agents/README.md` not updated for the second executable — `d41fe7a`; adds its 10 parameters, 5 topics, the derivation, the withhold-not-degrade rule, and two pitfalls (the v1 zero and its three guards; single-threaded by design)
- [x] (suggestion) No startup guard against `output_topic` equal to an input — `src/ellipsoidal_fix_node.cpp` — `a80e095`; throws, `main()` logs FATAL and exits non-zero so a respawn does not loop into the same bad state
- [x] (suggestion) `correction_age_s` formatted by a helper named `metres()` — renamed `fixed4()` — `f7de3df`
- [x] (suggestion) `install(DIRECTORY include/)` + `$<INSTALL_INTERFACE:include>` on an executable — `1c8adeb`; dropped both, include dir is now PRIVATE
- [x] (suggestion) Shared mutable state safe only under single-threaded spin — recorded as a file-header threading note in `a80e095` and in the agent guide (`d41fe7a`); no callback group declared, the requirement is documented instead
- [x] (suggestion) `echo_helm`'s own launch file does not offer the node — `launch/echo_helm_launch.py` — `629426a`; added under `enable_ellipsoidal_fix` (default true) with `respawn`, so IzzyBoat and the ArduPilot sim get the correction

### Cross-repo items for the host to route (`unh_echoboats_project11`)

Not actionable here — a separate agent holds that repo concurrently, so these
were **not** edited. All three are lockstep with #453 and should land with, or
before, this PR.

- **Annunciator tile for `GPS: ellipsoidal fix`** (`bizzyboat_project11/config/bizzyboat_annunciator.yaml`). The node's whole safety design is to withhold rather than degrade, and the operator currently has no way to see that it is withholding. The diagnostic now carries a much richer status to back a tile: level ERROR with the cause (`no correction established: …`, or input silence naming the topic), STALE when the correction ages out, plus `correction_m`, `correction_age_s`, `receiver_undulation_m`, `mavros_undulation_m`, `pair_skew_s`, `rejected_pairs`, `last_reject`, `received`, `published`, `withheld`, `since_last_input_s`, `since_last_publish_s`.
- **`respawn` on the node, and `platform_sender` still on the uncorrected topic** (`bizzyboat_project11/launch/core_launch.py`, `config/bizzyboat.yaml:72`). Operator-facing position and the nav solution otherwise differ by the 0.626 m being removed.
- **`/bizzy/mavros/global_position/global_ellipsoidal` absent from the logger record list** (`bizzyboat_project11/config/bizzyboat.yaml:662`) — the position actually consumed is not in the data-of-record.

**New lockstep item raised by this pass**: `629426a` adds `ellipsoidal_fix` to
`echo_helm_launch.py`, which `echo_launch.py` includes and the instance repo
includes in turn. `core_launch.py` in `unh_echoboats_project11` launches the same
node, so once both land there would be **two copies in the `bizzy` namespace**
publishing duplicate messages on the output topic. The instance launch must drop
its copy, or pass `enable_ellipsoidal_fix:=false`. The node belongs in the generic
repo: the defect is a property of mavros behind an ArduPilot FCU, which is every
EchoBoat, not of one hull. Noted in the launch file itself as a `LOCKSTEP:`
comment.

### Note on commit granularity

Findings 1, 2 and 4 landed in one corrector commit (`80a39dc`) rather than three.
They are a single rework of the same code path — `tryPair()` and the validity
model around it — and splitting them would have produced intermediate commits
that did not describe a coherent state. Findings 3 and 5, which are separable,
were kept as their own commits (`437f014`, `f7de3df`), and each was verified to
build and pass tests at its own point in the history.

### Next step

Lifecycle: **Implementation** → **review-code** (re-review the fixes)

    .agent/scripts/dispatch_subagent.sh --mode in-process --issue 55 --skill review-code
