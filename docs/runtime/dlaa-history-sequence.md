# Nine-frame DLAA accumulation diagnostic — v1.15.0

Completed with a safe recorder rejection: [packet reuse on frame five](../research/dlaa-packet-reuse-result.md).
The previous DLL is restored. The next build is [v1.15.1](dlaa-packet-generations.md).
The following describes the frozen v1.15.0 experiment after the
[v1.14 image review](../research/dlaa-history-image-review.md).
Evidence and reversible deployment scripts are under
`local/dlaa-sequence-live-20260925/v1.15.0/`.

## Frame and ownership contract

Nine adjacent projection/TAA-associated frames feed one private DLAA feature:
unchanged baseline, seven distinct native jitter samples, unchanged recovery.
Only the baseline resets history. Each of the seven camera mutations is restored
within its original helper call, including exceptional unwind. Repeated sample
bits, skipped frames/epochs, changed identities or incomplete restoration reject
the capture. The first injected sample still requires the existing strong-pair
selection; subsequent samples must be finite, nonzero and within native bounds.

The eleven hook targets, Windows x64 calling conventions, code hashes and
ownership assumptions are unchanged from [v1.14](retained-history-dlaa.md) and
its linked native-recorder contracts. The projection helper remains
`+0x228f3b0`, caller `+0x228e834`, RCX=camera+0x50, RDX=renderer+0x710, AL return.
Tested host v0.0.0.0 SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
All 55 existing decoded code spans are revalidated before preparation.

Jitter is supplied to the SDK as input pixels `(native_x/2, -native_y/2)`.
This diagnostic retains the native table's half-range raster displacement;
it does not establish a final production sampling pattern or full phase
coverage. Motion scale remains (1,1), MVJittered false, full-resolution DLAA
mode 5, preset K and flags 75. Time remains an explicitly fixed diagnostic
16.666666 ms. Actual frame timing and scene-change reset policy remain unfinished.

All native hooks disable and drain before private conversion and SDK evaluation.
The accepted startup provider/device ownership is reused with no extra Init.
The limits are one CreateFeature, nine EvaluateFeature, one ReleaseFeature and
DestroyParameters; there is no borrowed-device Shutdown. SDK log schema 5 binds
all nine temporal receipts and completion fences to their input sets. Private
resources stay pinned until process exit, including on uncertain failure.

## Bounds and verification

The two-second capture window, 4096-event journal, 64 depth requests, method
validation and per-image size limits remain. Frame-dependent budgets grow to:

| Work/storage | Limit |
| --- | ---: |
| Native copy commands | 108 |
| Raw color/depth/motion copies | 9 each |
| Raw readback bytes | 452,984,832 |
| Owned input copies / descriptor writes | 27 / 54 |
| Owned executes / signals / dispatches | 9 each |
| Owned allocation per set | 314,572,800 bytes |
| Owned readback bytes | 1,056,964,608 |
| SDK output bytes | 603,979,776 |
| SDK private image allocation | 150,994,944 bytes |

The SDK-free diagnostic and historical fixtures retain their three-frame
contract. Production frame count is compile-time bounded; reviewers select
only the explicit versioned count, never a count inferred from captured data.

Release /W4 /WX build and 56 focused native tests pass. The first longer
combined fixture exposed a literal three-receipt limit in raw readback; it now
uses the same bounded capacity as the recorder. Late exceptions, duplicate
jitter and frame skips fail closed. WARP success validates all nine depth,
motion and color sets and exact conversion. Its intentional varying-depth
clear-value performance warnings are counted separately from errors.

Five RTX fixtures pass with no GPU validation errors or warnings: three-frame
reset control, nine-frame retained history, late frame-gap rejection, short
history rejection and invalid four-frame count rejection. The longer isolated
fixture repeats three archived input sets with synthetic temporal metadata;
it proves the transport and SDK contract, not nine-frame live image quality.
Full artifact review checks 99,532,800 RGB values and rejects a rehashed NaN.
Adversarial SDK, combined native and historical/startup reviewers are recorded
in the checks directory. The previous accepted live v1.14 capture also passes
the current reviewer without modifying its archive.

## Live procedure and rollback

Install only with the game closed, using this version's `deploy.ps1`. The exact
previous MO2 slot DLL and any output sidecar are preserved and hash-checked.
The graphics settings, barebones profile and mode file stay unchanged.

Launch the existing barebones entry with DX12 On, DLSS Off, TAA On. Load a save,
keep the camera still and preferably use a stationary character pose. Reply
from the phone. After startup/process validation, arm the bounded capture.
No DLSS toggle is needed. Native TAA remains the presented image; DLAA output
is saved privately. A brief pause is possible.

After evidence is saved, exit normally with DLSS Off. Run this version's
`deploy.ps1 -Action Restore` after process exit to restore the previous exact
DLL and remove/restore its sidecar, then seal the capture archive. No original
game executable or asset is modified. Sustained visible DLAA, lifecycle tests,
resolution-changing DLSS and frame generation remain later work.
