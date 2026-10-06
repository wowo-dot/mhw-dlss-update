# Projection submission recorder v0.5.0

Prepared 2026-09-24 after [run 1544](../research/depth-view-state-result.md).
That run established the selected DSV creation/lifetime, but the depth
transitions were on other command lists. This extension records the actual
submission containing the selected draw's recording generation. No game
result is claimed until the versioned runtime evidence is reviewed.
[Run 47376](../research/projection-submission-result.md) now accepts the
selected recording's actual submission; incoming state and GPU evidence remain
partial. That runtime cycle is closed and the previous DLL restored.

## Target, ABI and lifetime

The executable remains version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The five startup and 39 core code gates are unchanged (43 distinct spans).
The [v0.4.0 watcher](depth-view-state-recorder.md) remains in place.

The new hook is `ID3D12CommandQueue::ExecuteCommandLists`, SDK vtable slot 10:
Windows x64 RCX is the queue, EDX is the 32-bit list count, R8 is the list
pointer array, with no stack parameters and a void return. Its target is
discovered from an empty owned DIRECT queue on the actual device, checked
against the permitted system module, and checked again on admitted calls.
Discovery creates no submissions. The installed SDK declaration independently
establishes queue slots 7/18 (`GetDevice`/`GetDesc`), list slot 8 (`GetType`),
and device slot 8 (`CreateCommandQueue`). Its hash and slot map are archived.

The wrapper preserves argument/return registers, LastError and exceptions,
and forwards the original exactly once. Actual queue entry and return order,
thread, canonical identity and the copied list array are recorded. Reset and
ResourceBarrier retain the existing shared core hooks; they are not hooked
twice. There are 33 hook targets: 12 native and 21 API.

At first observed use, each list and queue is queried for its exact base
interface, canonical IUnknown, owning device and type/description. A retained
COM reference prevents address reuse until capture drain. A bounded atomic
registry publishes each complete identity independently. Concurrent first-use
initialization fails closed; it does not wait while an original or COM call
is running. Partial query outputs are released. Cleanup waits for all admitted
calls before releasing resources, heap, device, command lists or queues.

## Capture and limits

Startup watching still has a 30-minute limit. The short history starts 50 ms
before source selection. The core projection/draw capture freezes at its
normal completion point; the history continues until the selected list occurs
in a returned Execute call. A draw-return marker and atomic selected-list
publication connect those stages. Worker polling also enforces the original
two-second history deadline if the game stops making observed calls. A reset
between draw and submission is preserved and rejected by replay.

Limits remain 2,048 view records, 4,096 history calls, 65,536 barriers and 256
resource pins. New limits are 128 pinned lists, eight queues, 128 lists per
submission and 4,096 submitted list entries. Fixed journal arrays occupy
6,791,680 bytes; referenced SDK objects and the existing core capture are
additional. The previous 62-ms capture used 2,461 history calls, leaving
1,635 slots for the short submission tail (and new Execute observations).
This is a measured starting bound, not a guarantee: overflow ends qualification;
the recorder neither evicts old evidence nor silently extends the window.

Readiness and companion evidence use schema 2; the main log is version 0.5.0,
experiment `projection-submission-readonly`. Schema-1/v0.4.0 replay remains
supported without changing previous verdicts. The companion file is bounded,
hash-checked and bound to the exact main-log stem.

## Review contract

`selected_recording_submitted` requires a complete successful Reset-to-Close
generation containing the draw/binding markers, a returned actual submission
on a DIRECT queue, pinned matching identities, and no overlapping recording,
ambiguous submission or intervening reset. It is API association, not GPU
completion. The selected batch position is reported separately.

`submitted_depth_state_declared` replays the earlier lists in that same batch,
then only the selected list's prefix before the draw. Earlier lists may have
been recorded after the selected CPU draw marker. Each needs an observed,
complete generation and the resource lifetime must precede its Reset. Bundles,
relevant aliasing, broken transition continuity, unresolved splits, descriptor
destruction/overwrite through submission return, or other observed queues
leave state unqualified. An earlier Execute batch does not supply missing
state in this version; partial results preserve that limitation explicitly.

These are declarations in submitted command lists. Batch position is not a
whole-list GPU-completion boundary. GPU execution state, fence completion,
resource contents, raster jitter and working DLAA/DLSS remain unverified.
Production adds no barriers, copies, clears, signals, waits or vendor calls.

## Verification and runtime procedure

Release builds use `/W4 /WX`. Owned SDK fixtures cover a real two-list
submission with a CPU-later predecessor barrier, reversed batch order, reset
after the selected draw and deadline expiry. Admission-only fixtures cover
invalid counts/pointers, duplicate lists, pending cleanup and exceptions
without sending invalid work to the driver. Resource/list/queue reference
balances are checked. Only the test fixture submits its own commands and waits
for its own fence. Reviewer mutation tests and a synthetic composition of
independent owned captures cover schema/controller integration; that composition
is not a native end-to-end capture. See `preparation.json` for final counts.

The archive is `local/projection-binding-20260923/v0.5.0/`. Preparation seals
source, test, SDK and binary hashes. Install/restore use that directory's
`deploy.ps1`; `deployment.json` and `prelaunch.json` establish the installed
state. The exact previous MO2 DLL and any output sidecar are preserved. Restore
verifies both the deployed hash and backup before replacing the slot; it removes
only the newly added sidecar or restores its previous bytes. No game asset or
profile change is needed.

Use the existing bare MO2 DLSS test entry, DX12 On, DLSS Off and TAA On. Load
one save and report that it is ready. Validate the new PID/creation-bound
startup log, then arm once. Keep the scene open until the result is saved;
exit normally when asked. A new launch is required for another capture.
