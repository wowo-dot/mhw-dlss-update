# One jitter sample through a matched draw, v0.2.1

Prepared 2026-09-23 after accepted [zero-jitter run 31912](../research/view-projection-contract.md#run-31912-zero-jitter-contract-accepted)
and the [scoped CPU jitter experiment](scoped-camera-jitter.md). The v0.2.0
[run 42212](../research/projection-binding-stimulus-result.md) captured the
CPU response and backend consumption but stopped on recovery-entry contention.
The corrected [v0.2.1 run 46324](../research/projection-jitter-binding-result.md)
is accepted and the previous DLL is restored. This diagnostic adds one scoped camera-field mutation to the
existing source/publication/consumption/root-CBV/draw recorder. It does not
implement DLAA/DLSS, call a provider, add GPU commands, or claim GPU execution.

## Identity and hook contract

Target executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The same 36 live code spans, twenty hook targets, startup trampoline reservation,
COM discovery, and ABI forwarding from the [binding protocol](projection-binding-recorder.md)
remain required. Those include the complete camera helper, its caller, matrix
producer/multiply/copy routines, native sample selection, and root constructors.
The game executable and original assets are never modified.

The sole mutation surrounds native helper RVA `228F3B0`: Windows x64 RCX is
the embedded projection object at camera `+50`, RDX is renderer `+710`, and
AL is the result. The accepted caller returns to `228E834`. The ASM wrapper
preserves argument/return registers, including the bool result and LastError,
and forwards every original call once. Camera/context/resource pointers are
borrowed during their native calls. Guarded reads and comparisons detect some
changes; they do not establish ownership against every concurrent native writer.

## Bounded experiment

One arm permits two seconds, 100,000 admitted callbacks, three selected CPU
calls and one completed native draw. Baseline and recovery are CPU observations;
only the stimulus is followed through the publication/binding chain. This avoids
holding three asynchronous draw captures or inventing cross-frame packet identity.

Each CPU call requires the typed active camera/display-zero entry, zero stored
jitter, a current camera epoch, ordinary view flags, valid bank/async state,
disabled resolution-changing sizing, and no NGX wrapper. The renderer's raw
`mUseDLSS` boolean may be 1 with menu DLSS Off, as observed in accepted controls.
Before/after provider snapshots must match, including backend/device identity.
Selected calls must have the same camera/type, renderer, context, view, table
and thread, with strictly increasing frame/epoch. Same-frame repeats are skipped.
Viewport dimensions must remain unchanged. A competing eligible producer ends
the experiment; it does not acquire another mutation budget.

1. Baseline: capture the helper's zero-jitter projection, view and view-projection
   matrices without modifying camera data.
2. Stimulus: read the native sample at table `+E6A0+8*(frame%20)`. Require a finite,
   nonzero pair within ±0.5. Compare/exchange exactly eight aligned bytes at
   camera `+38` from zero to that pair, call the original once, and capture the
   resulting matrices. Immediately compare/exchange the injected pair back to
   zero before making the source eligible for publication capture.
3. Recovery: capture a later unmodified zero-jitter call. It may arrive before
   or after the matched draw; completion requires both recovery and that draw.

Mutation accepts only committed PAGE_READWRITE storage; it never changes camera
page protection. Restoration runs in `__finally`, including SEH unwind, and
does not depend on acquiring the capture lock or the capture still being active.
A competing value is preserved and rejects the run. Hard process termination
does not guarantee unwind. No retry, sustained forcing, or TAA-enable change is
allowed. Restoring the camera bytes does not undo generated constants or any
image/history effect from the single jittered frame.

The stimulus source becomes immutable after successful restoration. Existing
packet/slot/arena/content checks follow that source through the selected draw;
baseline/recovery copies cannot replace it. The selected arena is protected
against reuse until draw recording completes. Bookkeeping locks never span an
original native call. In v0.2.1, an unreserved recovery entry may skip observation
when the lock is busy and select a later eligible frame. This is allowed only
after the completed stimulus/restoration with no selected producer active,
and is logged separately as `recovery_deferrals`. At most 64 deferrals are
permitted; the next attempt stops as capacity-exhausted. Stimulus admission,
admitted CPU exits and all binding evidence retain fatal contention handling.
Field restoration still runs on every admitted stimulus. Stop/drain retains the existing two-second bound and
pinned DLL/trampolines until process exit.

## Independent review

Schema 1/version 0.2.1 names `projection-binding-stimulus`, an injection budget
of one and three CPU samples. A `stimulus` record retains all three sources,
candidate bits, before/after provider state, restoration result and call order.
The header declares `max_recovery_deferrals: 64`; the stimulus record retains
the actual count. Missing or excessive deferral counts reject v0.2.1 evidence.
The reviewer still accepts correctly bounded historical v0.2.0 captures.
The selected top-level source must equal the stimulus source. It can be published
only after that call's restoration order. Logs remain bounded; no raw process
dump is made. Private shader containers stay under the ignored repository archive.

The reviewer checks both projection and view-projection jitter relations against
each frame's own unjittered matrices, verifies VP against view times projection,
and requires a measured nonzero x/y matrix change for the stimulus. Baseline and
recovery must have zero jitter and numerically identical unjittered counterparts.
Camera motion between frames therefore cannot substitute for a jitter response.
It also requires an observed viewport matching the source dimensions before the
draw. Root/register correlation retains the exact known-VS-hash rule from v0.1.3;
an unknown shader does not inherit that contract. Missing actual bindings remain
partial evidence. Executing-PSO bytecode, GPU contents, submission and raster
displacement are still unverified even if all command-recording checks pass.

## Verification and operation

The Release build uses `/W4 /WX`. There are 38 native cases, including the prior
24 binding cases and eleven combined-stimulus cases: full chain, recovery-before-
draw, exception, competing write, provider change, exit-lock contention, stop
while pending, changed anchor, zero sample, read-only camera and changed recovery
viewport, plus deferred recovery, deferral exhaustion and fatal consumer
contention. The 34 existing Python cases and 25 stimulus cases review owned fixture
logs and reject altered evidence. Preparation receipts identify the exact
tested source and binary; fixture results are never accepted as game captures.
The preparation receipt records the final counts, source hashes and DLL hash.
The unchanged zero-jitter result from run 31912 remains a regression check.
All 97 v0.2.1 preparation checks pass. Tested DLL SHA-256:
`A468B34138ECA53B51935DB52CA3621BAF13288F835C4B18F2DEE6C0F84D1621`.

Deployment evidence and helpers live under
`local/projection-binding-20260923/v0.2.1/`. Installation requires game absence,
preserves the exact previous MO2 test-slot DLL, and records graphics/profile/mode
hashes. The `.dll.output-dir` sidecar points to that archive. `deploy.ps1 -Action
Restore` requires a closed game, unchanged deployed DLL/sidecar and verified
backup; it restores the previous slot and removes only the new sidecar.

Use the existing **barebonesdlss** profile and **Monster Hunter: World - DLSS
1.2.110 test** MO2 entry, preferably offline. Leave DLSS Off and TAA selected.
After loading a save, remain in an ordinary scene and report ready. No menu toggle
or pan is required. Validate the loaded DLL/process generation and startup
reservation before `tools/arm_projection_binding.py --action arm`. The same
generation-bound control events apply; one arm only, thirty-minute idle readiness.
Require actual capture/stopped receipts, review the log, then exit normally and
restore/archive before preparing a different experiment.

That sequence completed in run 46324. One recovery entry deferred, then the
later recovery and selected jittered draw chain passed with zero critical
drops/faults. The user exited normally, the previous slot DLL and sidecar state
were restored, and all configuration hashes are unchanged. No unchanged rerun
is requested; retain these instructions as the tested historical protocol.
