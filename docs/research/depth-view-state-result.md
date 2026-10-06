# Actual depth-view provenance and earlier history — run 1544

2026-09-24, [recorder v0.4.0](../runtime/depth-view-state-recorder.md).
The actual D3D12 depth-view descriptor and its native view lifetime now match
the selected projection-consuming draw. Earlier transitions and clears for
the same resource were also observed, on other command lists. The selected
list's state at the draw remains unqualified because this recorder did not
observe queue submissions. The game exited normally; the previous diagnostic
DLL is restored and all four configuration hashes are unchanged.

## Accepted observations

PID `1544`, creation time `134347305314677188`; executable version `0.0.0.0`,
SHA-256 `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256: `93A161877B3C6CDFCAE6977102F31E7622B1002C60582578E57A9919898B12AB`.
All five startup and 39 core code gates passed (43 distinct spans), and the
32 hook targets were installed. The source frame/epoch is `20975`, root
parameter 9 / native VS b0. The indexed draw arguments are
`(2520, 1, 2586, 0, 0)`, with zero color attachments and one depth attachment.
All three resource identity/device/description phases agree.

| Evidence | Recorded value |
| --- | --- |
| Resource | 2560×1440 R32_TYPELESS, one mip/slice/sample, ALLOW_DEPTH_STENCIL |
| Actual DSV bytes as six DWORDs | `[40, 3, 0, 0, 0, 0]` |
| Actual DSV meaning | D32_FLOAT, TEXTURE2D, writable, mip 0 / subresource 0 |
| Native recipe, retained separately | `[2, 0, 1, 17, 0]` |
| Constructor journal slot | 36, entry/return orders 71/74 |
| Nested CreateDepthStencilView | entry/return orders 72/73, native return RVA 24E6516 |
| Fresh OM binding | shared-journal orders 5033/5034 |
| Actual draw marker | shared-journal order 5035 |
| Native depth metadata | enabled, write mask 1, GREATER_EQUAL |

The earlier creation at this same CPU handle was not used as the accepted
generation. Replay resolves the later constructor and checks resource and
canonical identity, pinned-heap membership, and no intervening destruction,
reuse or unqualified destination write through binding/draw. Canonical
IUnknown is again 48 bytes below the resource-interface address; exact
cross-interface address equality is not required. The cached PSO creation
descriptor and executing shader bytecode remain unverified.

## Why local state remains unknown

The watcher retained 55 view events: 22 constructors, 22 CreateDSV calls and
11 destructors. The short history retained 2,461 events and 7,536 barrier
records. Its event-entry ticks span 62 ms. It contains 210 actual Reset calls,
218 Close calls, 1,964 ResourceBarrier calls and 69 depth-clear calls. No
ExecuteBundle was recorded. These counts describe CPU recording activity,
not GPU execution or measured frame timing.

The selected command list has 39 earlier history events across 13 observed
Reset calls. Its latest Reset succeeds at orders 4727/4728. No history API
event follows that Reset on the selected list before the chosen draw; the
core separately records the expected binds, viewport/scissor and draw.
In particular, no transition establishes DEPTH_WRITE in that generation.
Replay therefore retains `local_depth_state_declared=false` with reason
`DEPTH_WRITE declaration missing`.

The same pinned depth resource appears in **112 transitions across six other
command lists**:

| Declared transition | Count |
| --- | --- |
| 0x10 → 0xE0 | 14 |
| 0xE0 → 0x10 | 14 |
| 0x10 → 0x800 | 14 |
| 0x800 → 0x10 | 14 |
| 0x10 → 0x840 | 28 |
| 0x840 → 0x10 | 28 |

Fourteen clears target the selected DSV handle on a different list. Their
recorded flags are 3, depth bits 0 (0.0f), stencil 0 and zero rectangles.
This reports the arguments as observed; it does not certify stencil support
or GPU results. Several calls from different lists overlap in CPU time.
Even a transition back to 0x10 just before the draw's CPU marker cannot be
promoted to the selected draw's incoming state without submission evidence.

This is consistent with the earlier [submission-order research](submission-order-and-temporal-inputs.md),
which also found that worker callback order cannot reconstruct the submitted
stream. That older run's queue/fence evidence does not transfer to this run.

## Archive and next diagnostic

Both capture channels passed hash and structural review. Core and provenance
faults/pending counts are zero; no drops or capacity failures. Hooks disabled,
callbacks drained and retained references released. No camera jitter writes
or commands were added to the game's rendering path. Preparation passed
101 native and 164 Python checks.

Private artifacts live under `local/projection-binding-20260923/v0.4.0/`:
`review-1544.json`, `acceptance-1544.json`, `depth-history-1544.json`,
`archive-1544.json` and `restored.json`. The archive verifies 44 source files,
the exact capture/shader hashes and rollback. Primary log SHA-256:
`B7CE39A75507BED8715BE02B580C6160EE62E9D864BAD9B81ADB6F1516534E20`.
Companion SHA-256:
`6856A8FF6302A4A43DF90899B3885D5C6FA7CDA08B27564600B94CD33FAE9E90`.
Restored DLL SHA-256:
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The output-directory sidecar is removed; no launch is pending.

Next, adapt the existing queue-submission observation to this validated view
lifetime and selected list generation. Capture through the selected list's
Close and actual ExecuteCommandLists association, with explicit queue/device,
generation, overlap and capacity checks. Preserve the earlier-list recordings
that may finish after the CPU draw marker. Decide bounds from this capture's
event density rather than simply repeating or lengthening the same window.
Only after this ordering is established should the readback design proceed.
GPU completion, depth pixels, raster displacement and working DLAA/DLSS
remain unproven.
