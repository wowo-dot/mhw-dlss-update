# Scaled depth recorder startup — v1.23.6

## v1.23.5 live result

PID 42532 / creation 134350218727074430, 2026-09-28. DLL SHA
`d4fc891e90a62afef260ebd821e322fb8d841d773d618713e4afffe398c1ede1`.
The activation check worked: 20 unowned projections were skipped before a valid,
owned baseline in frame 20595. Projection fault stayed zero. No jitter injection
or TAA input pairing occurred because depth recording exhausted its 64-request
limit; 63 native depth consumers completed before cancellation. Observer fault
15 propagated depth fault 3. The 328 ms between recording start and installation
return allowed ordinary startup traffic to fill the recorder.

All 189 depth pins and 382 journal pins were released, pending callbacks drained,
hooks disabled and full scale restored after 544 overrides. User reported a
normal scene and normal exit. Process exit, unchanged settings and preserved
installed DLL were verified before sealing the run.

Evidence: `local/scaled-temporal-20260928/v1.23.5/`. Final log SHA
`a7425851d74ffb4be2600e5498062f69df602ea03ed7d59f5afe3801006d5fb6`;
515-file archive SHA
`40c5a97363ed2dca41517a8a341f4b0a142581fc5ccba28136de0cacc40b346b`.
The unchanged baseline again used a 2560x1440 projection viewport. The complete
scaled jitter sequence is still unverified.

## Correction

Only the scaled probe now requests a depth-start gate. Before reserving a depth
receipt, it requires the exact first selected projection frame, epoch, bank,
async mode, context and view. Pre-baseline native requests forward without
occupying capture slots or acquiring resource pins. Once opened, the gate stays
open until normal shutdown: every later native producer remains observable,
including writes that reuse earlier packet storage. There is no mid-capture
filter that could hide a newer generation from the consumer lookup.

The first matching stamp and number of ignored startup requests are logged.
The v1.23.6 reviewer requires this stamp to match both the first depth request
and the selected baseline projection. Unmatched frames cannot open the gate.
Existing deadline, 64-request capacity, exact consumer generation/state checks,
jitter restoration and cleanup remain unchanged. Other recorder modes retain
their existing admission policy. No hook, SDK call or saved-setting write added.

ABI, lifetime and executable identities remain those documented in
[the scaled projection contract](scaled-projection-capture.md) and
[the activation admission correction](scaled-projection-startup-result.md).
The gate only borrows already captured CPU identity fields synchronously;
resource ownership begins at the existing native consumer pins. All 33 native
byte gates are rechecked when preparing the build.

## Verification and live test

Warning-clean /W4 /WX build and 75 native tests passed, including the unchanged
non-scaled depth routes. Both retained combined WARP captures passed with clean
GPU validation (only the fixture's known clear-value performance warnings).
Five projection-review checks and ten combined review checks passed, with one
inapplicable branch skipped. The earlier accepted v1.23.3 capture still passes
the updated reviewer.

The combined WARP regression forwards 96 pre-baseline depth requests, exceeding
the recorder's capacity, without consuming a slot. It then rejects six requests
with a mismatched epoch or bank before accepting the real baseline. Normal
three-frame projection/input/depth capture and exact GPU readback must still
pass, with one scoped jitter write and restore. Reviewer corruption cases reject
missing gates, unopened gates, invalid counters and mismatched stamp fields.

Evidence/build root: `local/scaled-temporal-20260928/v1.23.6/`. Use the same
bare/offline MO2 entry, native AA/DLSS Off, windowed2560x1440, loaded save and
still camera. Verify the exact new process/header/hash before arming. Keep this
DLL installed afterward. No live NGX evaluation or displayed DLSS output yet.
