# Qualified native TAA bypass diagnostic — v1.19.0

This focused test keeps native temporal preparation and output routing, replacing
each admitted main TAA resolve with DLAA. The tested DLL is the same build as the
[offline implementation](../research/native-temporal-resolve-implementation.md).
This is not yet the complete exclusive-AA mode: unqualified candidate dispatches
still forward native work, and stopping the diagnostic restores native rendering.
Menu integration remains deferred until CMFM exists.

## Setup and procedure

Use the bare offline MO2 profile and the existing **Monster Hunter: World - DLSS
1.2.110 test** entry. Leave native DLSS Off, Windowed 2560x1440 and DX12 On. The
deployment temporarily sets native AA to TAA solely to activate its preparation
stage. Its exact previous configuration is backed up with the existing DLL.

Load a save outside the sauna and wait for startup verification and arming. First
check the stationary scene and pan. Once the baseline counters advance without
fault, revisit the reproducible sauna route; then test an area load and focus
cycle if the earlier phases stay healthy. Record user actions separately from
timestamped checkpoints. Do not infer performance improvement from this test.

The frozen control tools and session evidence live under
`local/native-taa-bypass-20260927/v1.19.0/`. Use its `source-1.19.0/tools/`
arming/startup/final reviewers and `capture_phase.py`; do not update those copies
after sealing. Startup must pass for the same process generation and DLL before
arming. Named event controls make no process-memory writes. Send the stop event
after the focused test, verify GPU/jitter/hook closure, then exit normally.

## Acceptance

Verify successful SDK evaluations equal frames and reconstructed resolve counts,
with zero rejected, recovered, unrecoverable or shader-rejected results. A live
snapshot can contain an in-flight outcome: frame count may lead reconstructed by
at most `resolve.pending`. Final pending must be zero. Every candidate must be
accounted for as pending, unclaimed native or claimed; every claimed result must
have exactly one outcome. Report unclaimed native counts explicitly; these
counters cover observed candidates, not a census of every AA pass in the game.
An intentional stop after a claim may produce a complete unfiltered write.

Existing request, history, binding, GPU completion, retention, and jitter checks
still apply. A partial/faulted capture is retained but must not be marked a passing
run. Telemetry alone does not verify pixel quality or universal lifecycle safety.

## Exact rollback

With the game closed, run the session's `deploy.ps1 -Action Restore`. It verifies
the installed DLL and sidecar identities, restores `previous-slot.dll.disabled`
to the exact MO2 destination, and restores/removes the sidecar as recorded.
It restores the original AA configuration only if the file still equals the
prepared configuration. If the game/user changes another setting, preserve that
file and resolve the AA-only restoration separately; never overwrite those changes.
Mode, profile and mod list are archived and remain unchanged. No original game
executable, runtime DLL or asset is overwritten. Keep all generated evidence local.
