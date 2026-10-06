# Verified unfiltered output and reconstruction suspension — v1.21.0

Prepared 2026-09-27; offline verification only until a live result is recorded.
This extends the successful v1.20.2 native-postqueue DLAA path. It does **not**
claim whole-renderer exclusive AA: `exclusive_mode` remains false.

## Changes

The owned native TAA dispatch now proves current-color/output ownership,
packet/shader identity, full shader hash, command-list generation, bindings and
resource states separately from the temporal prerequisites. Once that output
route is verified, missing motion/depth/history readiness writes current raw
color into the same native output instead of issuing native TAA. The copy uses
the existing full-resolution R11G11B10_FLOAT resources; barriers restore source
NON_PIXEL_SHADER_RESOURCE and destination UAV. It records no SDK work.

Independent resource pins keep both native textures alive until the command
list is reset **and** its GPU work completes. There are 128 bounded recording
slots, a separate completion fence, one signal per submission batch, and an
explicit wait when the DIRECT queue changes. Execute plus both private fence
signals are serialized across CPU submitters. This avoids treating a larger
out-of-order signal as completion of earlier work. Unrelated batches acquire
no unfiltered resource/device ownership. Duplicate/replayed recordings fault
and freeze retirement; they are never treated as successfully completed work.

An unfiltered temporal suspension disables future injected projection jitter
while continuing to publish zero-jitter matrices. Resumption requires a valid
history reset boundary, then successful DLAA evaluation clears the suspension.
The already-rasterized first failed frame can still contain jitter; copying
color cannot undo it.

A recoverable SDK failure uses the existing raw-output recovery transaction,
latches reason 14, suppresses future injected jitter and SDK evaluations, and
removes TAA2 plus the other native AA stages from subsequent owned postqueues
before native sorting/resource routing. That latched route does not require a
temporal registry entry. Unrelated postprocessing remains. This prototype has
no in-process retry command: a fresh launch/explicit new test is required.
Log review reports the latch; it never accepts a latched failure as a successful
DLAA stability run. No saved AA settings are written.

## Hook and ownership contract

Executable version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
All native code gates, the TAA shader hash and Windows x64 calling conventions
remain those documented in [native-aa-postqueue.md](native-aa-postqueue.md)
and [native-temporal-resolve-implementation.md](native-temporal-resolve-implementation.md).
No new game function is hooked. The existing D3D12 Dispatch, Reset, Close and
ExecuteCommandLists hooks gain the copy/retention behavior above. Retention and
prepared-resolve data are serialized by the runtime mutex; command-list
recordings cannot be reused without their Reset generation being observed.
The DLL, SDK state, command-list registry and replayable recordings remain
pinned at diagnostic stop. There is no SDK shutdown or DLL unload in the game.

## Deliberate diagnostic limits

Candidates that fail the **output identity/state proof** still forward their
native dispatch. Producer publication failures and unmatched consumers can
also remain native. Those require further work before declaring exclusive AA.
The earlier run's 1,172 unclaimed candidates cannot be retrospectively assigned
to causes; its log had no per-reason counters. Do not assume they were all
missing depth leases or assert they have all been eliminated by this build.

Infrastructure faults still stop the diagnostic and restore original hooks.
Live activation with other saved AA choices, resize/device recreation, and
complete user-visible failure handling remain outside this test's proof.

## New receipts

`reconstruction`: schema 1, `suspend-or-latch-unfiltered-v1`, failure reason,
waiting state, suspension/resumption counts. `activation.failure_latched` must
agree, and `queue_unfiltered` closes the queue outcome partition.

`unfiltered_lifetime`: schema 1, independent signal/completed values and the
existing `reset-and-fence-v1` recording schema. Multiple lists may share a
signal, so submitted recordings can exceed signals. Its references are
reported separately from SDK/depth recording references.

`zero_jitter_projections` counts published zero-jitter scopes during waiting.
Existing jitter injection/restoration counters count all owned scopes including
zero-to-zero exchanges. `dispatch_reasons` has twelve fixed entries:

| Index | Meaning |
| --- | --- |
| 0 | Already-latched reconstruction failure; copy verified current color |
| 1 | Dispatch count, caller or list identity |
| 2 | Owner changed (raw/output proof rejection or temporal-only suspension) |
| 3 | Scene-depth identity |
| 4 | Packet, recipe or native binding metadata |
| 5 | Observed bindings or command-list generation |
| 6 | Resource states (raw/output rejection or motion-only suspension) |
| 7 | Missing, mismatched or already-used depth lease |
| 8 | Temporal recording resource pins |
| 9 | Whole shader validation failure; hard fault |
| 10 | Invalid history or suspension without a reset boundary |
| 11 | Copy preflight or lifetime failure; hard fault |

Reason counts cover dispatch skips only; `skips` also includes producer and
depth admission. Outcome counters distinguish unfiltered copies from unclaimed
native forwarding even where the reason index is shared.

## Offline verification and live plan

37 native tests pass with project warnings treated as errors. The new WARP
fixture calls the actual unfiltered replacement and submission/Reset hooks,
checks bit-identical nonuniform pixels, two-list batches, cross-queue fences,
GPU work held pending through Reset, replay rejection, and clean D3D12 debug
validation. The projection fixture checks zero-jitter publication, recovery,
and a latch that cannot auto-clear. Native activation tests cover all four
saved-AA choices, removal of all AA entries without the temporal registry,
preservation of unrelated postprocessing and suppression of native jitter.
108 Python checks pass, including new lifetime/latch corruption tests and
complete versioned parsing over explicitly synthetic controller receipts.

Use the existing bare MO2 profile/forced-DLL entry, native AA Off, native DLSS
Off, DX12, Windowed 2560x1440. Arm after a save is loaded, check baseline and
movement, then sauna and an area transition. Inspect both lifetime trackers,
unfiltered/resume counters and each remaining unclaimed reason. Do not call
this live-verified until those receipts and user observations are saved.

Evidence and frozen tools: `local/native-aa-unfiltered-20260927/v1.21.0/`.
Deployment preserves the exact baseline MO2 DLL and any output-dir sidecar.
After normal game exit, `deploy.ps1 -Action Restore` restores that DLL and the
previous sidecar state. It never overwrites the game executable or assets and
does not change graphics/profile preferences.
