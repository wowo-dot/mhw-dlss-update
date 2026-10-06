# Bounded projection jitter with paired color/motion: v1.6.0

Accepted and closed 2026-09-25: [run 35156](../research/ordinary-taa-jitter-result.md).
All three projection-associated pairs pass capture verification. Scoped pixel
analysis supports color jitter direction and jitter exclusion in selected static
motion; precise color-scale agreement remains unresolved. Exact rollback and
archive are complete. The procedure below describes the tested recorder.

Prepared 2026-09-25. This follows the accepted
[color/motion alignment run 43820](../research/ordinary-taa-paired-result.md).
It adds one scoped projection stimulus to three matched color/motion captures:
baseline, one jittered frame, zero-jitter recovery. It does not evaluate DLAA.

## Native hook and ownership

The seven v1.5.0 hooks and their copy/lifetime rules remain as documented in
[paired readback](temporal-paired-readback.md). One additional hook observes
the previously validated projection helper at RVA `228F3B0` (2,000 bytes),
accepting only caller return RVA `228E834`. Tested executable version `0.0.0.0`,
SHA-256 `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The helper's full code hash remains
`df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373`.
There are 48 distinct pinned code spans; the added direct check repeats the
existing discovery gate. No new native offset is derived for this experiment.

Windows x64 ABI: RCX is the selected camera object +0x50, RDX is renderer
+0x710, and AL is the helper result. The assembly wrapper preserves volatile
integer/vector arguments and result registers using the established forwarding
frame and unwind metadata. Camera, renderer and projection constants are borrowed
from the engine; they are never freed or retained as owned COM objects. The
camera field is accessed only within the original call's scope, with bounded
reads and guarded writes. Trampolines and the DLL remain pinned until exit.

Admission requires the exact global renderer, selected camera/type, active
scene camera, no display-camera flag, zero existing jitter, matching frame/epoch
stamp, stable scene view, bank/async mode and a supported provider state. Legacy
wrapper must be null and the sizing bit clear. The existing 150 ms command-list
warmup precedes selection. The first ineligible call is left untouched; an
ineligible call at the accepted call site during the selected sequence closes
the experiment. Additional or skipped eligible projection frames are rejected.

Each admitted projection sample must be acknowledged by the corresponding TAA
producer before the next projection sample. The exact frame, epoch, bank,
async mode, context and view must match. Both frame and epoch advance by one.
Camera/type, table, view and async identity remain constant. This is CPU
projection-to-TAA request association plus separately verified input copies;
it does not reproduce v0.9.1's complete projection-to-draw/CBV chain.

## Mutation and evidence

The native 20-sample table supplies the pair at `frame % 20`. Both floats must
be finite, at most 0.5 in magnitude and not both zero. Only stage 1 installs
that pair into aligned camera +0x38, using the already tested guarded atomic
compare/exchange from zero. No memory-protection change is allowed. Baseline
and recovery execute unchanged. There is at most **one injection and one
matching restoration**, regardless of later failures.

Immediately after the original helper returns, the observer validates its
return value, projection/unjittered matrices, native sample, viewport and
provider identity. It reads at most 4 KiB of the selected CPU constant block,
checks two copies for stability and captures the five named matrices. Raw
constant-block bytes are not logged. The field restores in `__finally`, even
on exception, stop or failed validation. Restoration compares against the
injected value; an unexpected writer is reported and never overwritten blindly.
Any missing invariant rejects the capture and preserves normal forwarding.

The TAA recorder selects only completed, valid projection samples and acknowledges
them only after its original producer returns and the queued packet is validated
and published to the consumer. All three samples require that association.
The input copies retain the v1.5.0 exact source/format/state and lifecycle gates:
three R32_UINT motion copies and three R11G11B10_FLOAT current-color copies,
18 private command-list operations, three private Signals, no observer Execute.
Each allocation is limited to 16 MiB; all six together to 96 MiB. The capture
window remains two seconds with 4,096 submission events, no rearm. Issued copy
resources stay retained until process exit. Hooks close before qualification/map.

The independent reviewer checks all projection samples and acknowledgments,
zero controls, the single stimulus/restoration, actual candidate bounds and
table bits, exact TAA association, consecutive frame/epoch, stable identities,
time ordering, provider state, viewport, matrix layout, projection jitter
relation and view/projection products. Matrix validation reuses the pure
`projection_contract.py` implementation. Broad pixel and DLAA flags remain false.
Saved CPU matrix values predict raster displacement `(jx/2, -jy/2)` in pixels;
the actual color displacement and motion response require separate pixel analysis,
now recorded with its limits in the result linked above.

## Verification and run

Release `/W4 /WX` build passes **76 native and 38 Python tests**. New native
cases cover scoped restoration during exceptions/cancellation, changed provider,
competing field writer, reentrancy, invalid matrices/return/table, preexisting
jitter, timeout, foreign caller, skipped frames, absent/duplicate acknowledgment
and frame mismatch. The integrated WARP fixture uses the real paired GPU copy
path with an owned CPU projection model and requires exact six-file contents.
The fixture's RGB patterns do not model raster jitter. Python tests damage the
new evidence and budgets; earlier captured versions retain their results.

Use the same bare-profile MO2 entry with DLSS Off, TAA On, DX12 and the existing
resolution, offline/private. Load the scene and leave the character and camera
still; report **Save loaded** from the phone. Arm only for that exact process
generation after verifying its version/hash and ready event. Do not pan or
toggle DLSS. Once captures are saved/reviewed, request normal exit and restore.

Fresh deployment/evidence: `local/temporal-jitter-20260925/v1.6.0/`. Installation
requires the game closed, preserves the exact prior slot and output sidecar,
and records settings hashes. After normal exit, its `deploy.ps1 -Action Restore`
restores the previous DLL and restores/removes the output sidecar. No game
executable, original assets, graphics settings or profile changes are made.

Analysis should select static regions using baseline/recovery agreement, then
compare jittered color displacement with the independent matrix prediction and
measure motion in both the stimulus and recovery frames. Avoid assuming motion
includes jitter: that is the question being tested. Capture acceptance alone
establishes neither color raster displacement nor the SDK motion-vector contract.
