# Drain admitted callbacks before closing the journal — v1.15.2

Live result: all 42 depth markers and dependent callback drains passed, but
journaling continued during hook removal and exhausted the 4,096-event budget.
No SDK evaluation ran. [Result and next correction](../research/dlaa-journal-capacity-result.md).
Normal exit, exact restoration and final archiving are complete. The procedure
below documents the tested version; no next diagnostic is installed.

The [v1.15.1 run](../research/dlaa-nine-frame-result.md) produced nine private
DLAA outputs, but the frozen full reviewer rejected two later depth consumers
without submission markers. An offline WARP test now reproduces the exact
cancellation interleaving: a valid depth callback is admitted, cancellation
closes the submission journal, and the callback subsequently cannot get its
marker despite forwarding its original copy successfully. The pre-fix fixture,
source and expected failed assertion are preserved under
`local/dlaa-sequence-live-20260925/v1.15.2/checks/before-fix/`.

## Correction

Cancellation stops TAA producer, TAA consumer, jitter and depth admission.
Previously admitted depth callbacks drain while the copy/barrier hooks and
submission journal remain available. The depth hooks are then disabled, TAA
consumers drain before their hooks are disabled, and submission stops last.
The two-second capture and drain bounds remain. A drain timeout rejects the
capture and retains references needed by the delayed callback; it does not
enable conversion or SDK evaluation. Trampolines remain resident.

Every valid admitted depth consumer now requires a submission marker at
runtime. Missing markers set depth fault 16 and disable the experiment while
the original callback is still forwarded. A per-consumer `marker_failure`
records why creation failed: 0 success, 1 unconfigured, 2 admission closed,
3 unknown list, 4 missing Reset, 5 closed list, 6 busy list, 7 missing allocator,
8 event budget/deadline. No marker is fabricated and none is waived as a tail.

The v1.15.2 header requires `journal_drain:true`; the reviewer requires
`drained_before_disable:true` for both dependent observers and integer-zero
marker failure for every depth consumer. Existing marker coverage, complete
selected-frame generations, GPU fences, raw copies, typed conversions and
SDK checks remain. Historical accepted/rejected records keep their outcomes.

No target address, signature, calling convention or native argument changes.
The existing [packet-generation lifetime contract](dlaa-packet-generations.md)
still applies. Host v0.0.0.0 SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The SDK implementation and pinned public runtime are unchanged.

## Verification and live procedure

Release builds use /W4 /WX. All 145 native temporal tests pass, including
deterministic stop-before-marker, a callback paused across cancellation and
resumed during drain, native exception propagation, bounded drain timeout and
forced missing-marker rejection. Distinct unknown/reset/closed/busy/stopped
marker reasons are exercised. Positive nine-frame WARP fixtures preserve exact
color/depth/motion conversion and report clean GPU validation; their expected
clear-value performance warnings are explicitly counted.

Adversarial reviewers require the drain evidence and reject missing markers
even on unselected tails. SDK validation reuses the sealed unchanged evaluator
fixtures; this version changes recorder shutdown, not SDK evaluation. The
previous nine-frame live run remains rejected by the full reviewer.

Use the same barebones MO2 test entry, DX12 On, DLSS Off and TAA On. Load a save,
keep the camera still (stationary character pose is useful), and reply from
the phone. Verify startup/process/build before arming. No DLSS toggle is needed.
The test still captures nine private outputs with seven scoped jitter samples,
history retained and fixed diagnostic frame timing; it does not display DLAA.

Versioned deployment/rollback is under
`local/dlaa-sequence-live-20260925/v1.15.2/`. The exact prior slot DLL and sidecar
are preserved. After normal exit, run `deploy.ps1 -Action Restore` from that
directory; restoration verifies the exact previous hash and four configurations.
No original game executable or asset is modified. The live result above
supersedes the original preparation status.
