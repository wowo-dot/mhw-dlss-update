# Read-only renderer gate observation

Date: 2026-09-07; updated 2026-09-08. Version **0.7.1** passes early renderer
observation and cleanup in run **35100**. Run **43380**, confirmed by the user
to reach a loaded save, adds 12 valid samples with all selector gates ready,
no sizing exhaustion and clean exit cleanup. Evaluate remains absent; precise
scene timing is not instrumented. The [v0.7.0 loaded-scene attempts](loaded-scene-evaluate-gate.md)
completed Create and cleanup without Evaluate. This diagnostic samples the
renderer fields used by the DLSS selector to narrow the next investigation.
It does **not** instrument temporal-pass execution or command enqueueing.

## Identity and structural contract

The existing bridge identity checks remain mandatory. The disk identities were
rechecked before deriving the new byte gates:

| Artifact | SHA-256 |
| --- | --- |
| MHW, Steam build 15539686, version fields `0.0.0.0`, PE timestamp `0x66D13CB6`, image size `0x55D3000` | `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA` |
| Captured host used for IDA analysis | `D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C` |
| Modern NGX core, version 32.0.16.1047 | `B3C82638C60F29AEFF68052FFE7147246A73C80CBEA5883B35A9B0D1ED57DDE7` |

Before each sampling attempt, the observer checks five exact instruction
sequences in the live host: selector `+0x2392A18` (47 bytes), main view
`+0x2391F3C` (26), dimensions/scale `+0x2391F5C` (16), frame index
`+0x23922C2` (24), and optimal-settings storage `+0x229A987` (12).
These include the RIP-relative renderer singleton references and field offsets.
A mismatch stops this observer. Offsets are module-relative and apply only to
this contract, not to other game versions. Exact extracted bytes and local
identity records remain ignored under `local/renderer-gates-20260907/`.

## Observed fields

The renderer singleton is at host `+0x51C4480`. The following are borrowed
scalars, not owned objects. No pointer is retained in a published snapshot.

| Object / offset | Interpretation and use |
| --- | --- |
| Renderer +120 | Backend pointer; follow only for the two fields below |
| Renderer +165 | `mUseDLSS` byte |
| Renderer +176 | Optimal scale returned by the sizing callback |
| Renderer +180 | Sizing flags, bit 0 permits DLSS |
| Renderer +408 / +412 | Present width / height |
| Renderer +516 | Content scale used in allocation |
| Renderer +127504 | Renderer frame index used by the TAA2 ring |
| Renderer +128080 | Main-view pointer |
| Main view +88 | Context flags; report mask `0x840000` separately |
| Backend +272152 | NGX-ready byte assigned by the game after Create returns |
| Backend +272128 | NGX-wrapper pointer; record presence, never dereference |

The selector's blocking mask is `1` for DLSS Off, `2` for sizing disallowing
DLSS, `4` for no backend, `8` for backend not ready, and `16` for no NGX wrapper.
A missing backend also leaves readiness and wrapper presence false.

The main view may differ from the context passed to an actual TAA2 execution.
Its exclusion flags are therefore **not** included in the selector mask.
An advancing renderer frame index does not prove TAA2 execution, enqueueing,
Evaluate, or GPU completion. All gates ready with zero Evaluate would leave
upstream activation and dispatch as the next investigation.

## Timing, ownership and failure behavior

Only `capture-one-evaluate` mode samples, during a live DLSS Init/token lifetime
and after the complete provider route has been verified. The worker starts no
earlier than 250 ms after successful Create records its timestamp, then samples
at most once every 2 seconds, for at most 64 snapshots. It skips while a wrapper
is active. This delay reduces premature readiness readings; it is not a formal
synchronization with the game's post-Create assignment or rendering thread.
A single early blocked sample can be transient.

Each snapshot permits at most 24 scalar reads, each 1–8 bytes. The complete
current graph takes 16 reads. This caps scalar reads at 1,536 per process,
separately from the five bounded instruction comparisons per attempt.
Those comparisons use the same protected reader in at most 17 chunks, after
checking that each sequence starts in executable memory owned by the host image.
`VirtualQuery` must confirm committed readable memory, without guard/no-access
protection, and the complete scalar must fit within that region. A protected
copy catches a fault if the mapping changes after the check.

The observer rechecks the singleton, backend and view pointers. A changed
singleton ends that snapshot before further reads through the old root. These
checks detect some recreation races; they cannot make the graph or its values
atomic. The bridge's shared handle-lifetime lock prevents concurrent Init
retirement by Shutdown during the reads, but does not own game objects. A
Release detected during capture discards the snapshot. No COM calls, resource
inspection, GPU commands, host data writes or host hooks are added.

Unreadable memory, a read exception, or invalid scalar values stops future
sampling for this process. An absent renderer or changed pointer graph can be
observed again at the next interval. The observer does not change provider
behavior or attempt to repair game state on failure. Accepted scalar ranges
are boolean bytes 0/1, dimensions 1–16384 and finite scales greater than zero
and at most 4. A stopped or unsampled observer is inconclusive.

Only the first sample and changes to gate values produce snapshot log events;
frame-index advances alone do not. The final renderer summary includes sample,
read, event and discarded counts, first/last valid frame indices, contract and
stop status. Events explicitly say `async=true frame-coherent=false` and
`temporal-execution=not-instrumented`. Final checkpoints for both providers now
record sizing-call and exhaustion totals, addressing the incomplete final
budget telemetry in v0.7.0 without increasing its callback budget.

## Validation and next live run

Release builds with `/W4 /WX`; all **13 CTest tests pass**. New tests cover gate
classification, a fault at each of the 16 reads, pointer replacement, invalid
values, address overflow, event deduplication, inaccessible pages and guard
pages without consuming the guard. They do not establish live object lifetime
or renderer behavior. The x64 binary retains CFG instrumentation: all 15
distinct exported provider wrappers/callbacks occur among 104 CFG targets.

The Release DLL is 356,352 bytes, SHA-256
`BAA156066CB3D555023EEE53378ACF29DB08D9FC5F318C71035A6136AEF654C2`.
Local build audit and deployment records are under
`local/renderer-gates-20260907/`.

Use the [current offline procedure](../runtime/modern-bridge-probe-test.md).
The procedure for run 43380 began with DLSS **Off** after run 35100. Preserve that initial state and
the existing 1:1 sizing and anti-aliasing configuration. Enable DLSS once from
the main menu after startup, leave Options, then load a save once, allow
several samples in the scene, return to the main menu, turn it Off and exit.
Do not repeat On/Off/On within the one-shot provider lifetime.

Run 43380 completed the gate-sampling test and left settings **On**. The above
is the historical procedure, not a request to repeat it. Review the latest
result below before preparing another test.

Require v0.7.1, valid identities, successful Create, verified renderer contract,
multiple valid samples, copied-handle Release, both Shutdowns and a quiesced
complete trace. Inspect final sizing counts even if only 16 events were stored.
If Evaluate is reached, use the existing
[one-Evaluate acceptance contract](one-evaluate-capture.md). The diagnostic
still performs no reconstruction; MHW ignores its deliberate Evaluate failure,
so an incomplete or stale picture remains possible. Preserve a failed run
rather than retrying within the same process.

## First live renderer observation: run 35100

The user reports DLSS greyed out Off and wonders whether the previously saved
On state caused it. The log records successful Create at sequence 70, 10,172 ms
after trace start, then successful copied-handle Release at sequence 71 only
2,593 ms later. All four handle checks pass. The precise reason for that early
Release is not instrumented; startup renderer reconfiguration is a hypothesis,
not an established caller-level explanation. Scene loading is not confirmed
for this run.

The observer verified all five live code sequences and captured two valid
16-read snapshots without faults, discarded lifetimes, pointer changes or
invalid values. The first event is 265 ms after Create:

| Observation | Value |
| --- | --- |
| `mUseDLSS` / sizing flags / backend ready / NGX wrapper | `1` / `0x1` / `1` / present |
| Selector block mask | `0x0`, gates ready in both samples |
| Present dimensions / optimal scale / content scale | 2560x1440 / 1.0 / 1.0 |
| Main-view flags / exclusion candidate | `0x2D00D1FD` / false |
| Renderer frame index | 34 in the first sample, 473 in the last |
| Snapshot events / valid samples / scalar reads | 1 / 2 / 32 |

Frame-only changes are deliberately deduplicated. The summary's second sample
therefore does not have its own event. These asynchronous observations establish
that all selector gates can be ready during this lifetime. They do not establish
that TAA2 executed, a work item was queued, or a loaded scene was sampled.
Both provider summaries still show zero Evaluate and zero suppressed Evaluate.

Twelve sizing calls succeed before Create. After Release, four logged sizing
requests reject admission with no parameter reads/writes and no getter fault.
The policy in `evaluate_gate.cpp` admits sizing only while populated or while
the one-Evaluate token is outstanding; released is intentionally terminal for
this experiment. This is a concrete diagnostic limitation consistent with the
greyed-out menu. Final checkpoints now establish 32 total sizing calls and a
16-call budget exhaustion. Exhaustion follows the first post-Release admission
failure, so increasing the budget alone would not fix this run.

DLSS and ISP Shutdown at sequences 76/77 both succeed, with no outstanding
Init/token, no contention, handle-read fault or lifecycle/event overflow. The
trace completes quiesced at 95,719 ms. MHW was closed at review and no new
matching crash dump was found. The saved graphics configuration now has DLSS
Off; its other difference from the pre-run snapshot is `PosX` 0 to 1280.
The review did not modify the settings or replace the deployed v0.7.1 DLL.

Archived evidence: `artifacts/logs/bridge-35100.run001.v0.7.1.log`, 66,992 bytes,
SHA-256 `A86915FA63E8AD23020744944F6105C70F619F594255AD665FB2A067294A903A`.
Assertions, the structured assessment and a read-only settings snapshot are
under `local/renderer-gates-20260908/`. Settings SHA-256 is
`178AD0827945C68DAABBDEED077F339FAE92EF73F3044668197688383DF30A05`.

The next controlled run uses the same binary but starts with saved Off and
enables once from the main menu, so the early saved-On lifetime is avoided.
This is a different initial condition from run 35100. Require live scene
observations before treating the ready gates as evidence about gameplay.
The missing Evaluate remains unresolved; do not infer modern rendering success
or blame 1:1 sizing or Anti-Aliasing Off from this run.

## Run 43380: ready gates through a longer lifetime

The user confirms reaching the loaded save, then exiting without returning to
the main menu. That omission did not prevent cleanup verification. Release
and both Shutdowns occur at the same recorded millisecond, in that order.
The run needs no repeat merely to perform a menu Off transition.

| Observation | Result |
| --- | --- |
| Create / Release / DLSS and ISP Shutdown sequences | 68 / 70 / 71, 72; all `0x1` |
| Create after trace start / token lifetime | 35,359 ms / 23,187 ms |
| Renderer samples / valid samples / scalar reads | 12 / 12 / 192 |
| Selector state across samples | Ready; `use-dlss=1`, sizing bit 0 set, backend ready, NGX wrapper present |
| Present dimensions / optimal scale / content scale | 2560x1440 / 1.0 / 1.0 |
| First / last renderer frame index | 5,245 / 9,882 |
| Sizing calls / successes / exhausted | 11 / 11 / false |
| Evaluate / suppressed Evaluate | 0 / 0 for both providers |
| Total trace duration / quiesced | 58,422 ms / true |

The renderer contract verifies, all graph checks pass, and the observer has
no stops, read faults or discarded samples. Only the first snapshot produces
an event: gate values remain unchanged while the frame index advances. The
main-view exclusion candidate remains false. Asynchronous polling does not
establish values between samples, and there are no load-completion markers
to assign individual samples to title, loading or gameplay. The loaded-save
claim comes from the user, not from the frame counter.

All 11 sizing responses have four completed writes and verified readbacks.
There are no admission rejections or budget overrun in this run. It therefore
shows absent Evaluate without the prior run's early Release and subsequent
sizing failures. Create has the same verified D3D12 command list and complete
11-query scalar contract; all copied-handle Release checks pass. Both provider
summaries have complete routes, no outstanding cleanup, no contention or event
overflow. MHW was closed at review and no new matching crash dump was found.

This establishes the live gate diagnostic's result, not temporal-pass execution
or modern DLSS rendering. The next investigation is the temporal layer's
activation, actual execution context, and work-item enqueue/dispatch path.
The inspected execute routine `+0x2392150` has context and mode exits before
the four-gate selector, and `+0x23913F0` produces the type-3 work consumed by
`+0x259DAE0`. Those execution points remain uninstrumented. Do not keep repeating
the same gate test or change sizing/anti-aliasing based only on this result.

Archived log: `artifacts/logs/bridge-43380.run002.v0.7.1.log`, 60,369 bytes,
SHA-256 `6F34C92CB1B465B5DFECE00AB26584D3A408A5CC504667383F15FB5EDCACB47B`.
Assertions and the structured assessment are in
`local/renderer-gates-20260908/review_43380.py` and `reviewed-run-43380.json`.
The unchanged deployed DLL remains v0.7.1. The saved graphics file is now
**DLSS On**, SHA-256
`02A6031BF7CE848A454D606FD3E6E68D056E0739DCE6D24272D930BF492D6BAE`.
The review only copied it into the repository. Establish a known Off baseline
before a future one-shot test; no new runtime attempt is requested yet.
