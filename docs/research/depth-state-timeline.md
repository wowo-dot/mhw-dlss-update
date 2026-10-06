# Depth tracker and cross-list timeline

Current result, 2026-09-22: [run 24944](post-evaluate-journal.md#run-24944-current-lists-closed-and-submitted)
accepted the corrected frame's current-list submissions and a continuous depth
declaration chain ending in combined shader read `0xC0`. GPU execution and
contents remain unverified. [Run 46868](three-frame-result.md) now accepts three
bounded consecutive-frame corrections and closed/submitted depth recordings in
all three CPU intervals, followed by the intentional fourth-fault stop.
The v0.7.6/v0.7.7 observations below are historical.

Date: 2026-09-22. **Run 38476 accepted the first-Evaluate CPU timeline and
external depth-tracker snapshots**, using bridge **v0.7.7** and recorder
**v0.4.0**. All 24 tests and the CFG audit pass. The earlier run 4568 retained
only the external tracker. Modern reconstruction remains unimplemented, and
depth's final state/submission in the corrected frame remains unresolved.

## Run 38476: depth calls observed, submission window incomplete

The [capture mailbox](capture-mailbox.md#run-38476-complete-capture-preserved)
saved the completed first Evaluate before the second fault was forwarded.
Its four native resources match corrected frame 20683; the next frame uses a
different output. Evaluate ran on thread 36648, distinct from the preparation
fault thread 47116 and both observed depth-recording threads. The observer
freezes at Evaluate, while other command lists can still be recording.

The bounded window contains 244 API calls / 610 barrier descriptions, with
clean coverage flags and no timeline truncation. The retained depth history is:

| Call | Command-list label | CPU entry / return order | Declared transition | Recording thread |
| --- | --- | --- | --- | --- |
| 227 | `0x15C2E850` | 450 / 451 | GENERIC_READ `0xAC3` to COPY_DEST `0x400` | 45260 |
| 236 | `0x15C2A2C0` | 468 / 471 | PIXEL_SHADER_RESOURCE `0x80` to COPY_DEST `0x400` | 45072 |

Both are ordinary transitions, flags zero, all subresources, from host caller
`+0x259DFCF`. Neither belongs to Evaluate's list `0x15C2B9F0`. These two lists
must not be concatenated into a GPU state sequence using CPU call order.

The timeline retains ten relevant Reset/Close events and thirty queue calls
with eighteen submitted-list labels. Matching pointers initially suggest
submissions at calls 150 and 153, but the later Resets change their meaning:

| List | Earlier Execute call / return order | Latest Reset call / return order | Current Close observed |
| --- | --- | --- | --- |
| Evaluate `0x15C2B9F0` | 153 / 306 | 186 / 372 | None before freeze |
| Depth `0x15C2E850` | 150 / 300 | 188 / 376 | Call 241, return 481 |
| Depth `0x15C2A2C0` | 153 / 306 | 185 / 370 | None before freeze |

The last retained queue call returns at order **356**, before even the earliest
of these current Resets begins at **369**. Consequently, **no submission of
these current recordings was observed in this window**. Calls 150/153 refer
to previous recordings of the same pointer labels. Signal/Wait calls for that
earlier work cannot establish ordering for the current depth transitions.
This is a scope limit, not proof that the current work never submits.

The selected Evaluate recording still has clean color/motion read declarations
and output UAV declarations, but no depth declaration. The depth calls on
other lists establish CPU recording activity; they do **not** establish that
depth is left in COPY_DEST at use. A later read transition, current submission,
GPU completion, initial-state correctness and object lifetime remain unobserved.

The external tracker repeats run 4568's `0x8 | 0x4` merge prediction and next
frame tracker-before `0xC`. It does not resolve this missing API evidence.
Local `review_38476.py` checks all retained timeline events against the bridge
text log and compares submissions against the relevant latest Reset, producing
`review-38476.json`. The next diagnostic should preserve the first-Evaluate
snapshot and separately retain a bounded post-Evaluate continuation through
relevant Close/submission and synchronization events, distinguishing recording
generations. It must report incomplete/overflowed evidence explicitly and
retain the one-correction limit. Do not add a guessed depth transition or repeat
the unchanged capture. That continuation is now implemented and fixture-tested
in the [v0.7.8 journal](post-evaluate-journal.md); run 24944 accepted that
continuation's current-recording submissions and compatible depth read declaration.

## Run 4568: tracker accepted, timeline unavailable

The user enabled DLSS once after ARMED, left Options and reported a crash.
Both provider startups and Create sequence 68 passed. The Create event reports
all observer hooks installed and armed. The external recorder passed all 12
live code gates, then captured 105 work-item reads and **32 tracker reads** at
each of two matching first-chance faults. Every scalar read and recheck passed.

| Observation | Corrected frame 25741 | Uncorrected frame 25742 |
| --- | --- | --- |
| Tracker and context epoch | `0x648D` | `0x648E` |
| Selected active-record state | engine `0x8` | engine `0x8` |
| Requested state | engine `0x4` | engine `0x4` |
| Active record's before state | engine `0x800` | engine `0x800` |
| Tracker's before state | engine `0xFF` | engine `0xC` |
| Current / active record operation | 1418 / 1400 | 1413 / 1397 |

The context descriptor equals the fault's descriptor, and all backing/native/
tracker anchors match the work item. Depth remains a kind-5 owner with a present
primary backing and null secondary backing. Its native resource pointer is
unchanged across the two faults. The active record is from the current epoch
but refers to an earlier operation and a different descriptor.

This **rules out the already-containing-bit-4 hypothesis for this sample**.
The captured helper's same-epoch branch instead selects read-mask merging:
`0x8 | 0x4 = 0xC`, because the requested bit is absent and the existing state is
at most `0xFF`. Static instructions at `+0x229285C` update the active record
and tracker, then search the active record's earlier descriptor for a matching
backing entry and update its state if found. This branch does not append a new
usage record. The next frame's observed tracker-before value `0xC` is consistent
with that merge, but does not prove the earlier barrier entry was found, emitted
or submitted. The specific merge instructions were inspected in the hash-recorded
static export; they were not a separate additional live gate in v0.3.0.

The captured `+0x259BA50` conversion maps engine `0x8` to D3D12 pixel-shader
read `0x80`, and engine `0xC` to combined pixel/non-pixel read `0xC0`.
**Engine `0x8` is not D3D12 UAV `0x8`.** These are metadata and static branch
observations, not acceptance of the native texture's current GPU state.

The external recorder verified exactly one RDX correction and unchanged RIP;
the second fault arrived **16 ms later**, exhausted the budget and was forwarded.
It detached cleanly after 322 debug events and three exceptions (attach plus
the two matching faults), with zero remote memory writes. The game closed.

The bridge file ends after Create and its temporal observation. No finished
Evaluate event, resource inspection or depth/queue timeline is present. The
worker polls every 25 ms, and `publish_bridge_event` does not signal its wake
event; deferred delivery losing the race to the next-frame crash is a plausible
explanation. It is not proof that Evaluate executed or that an unlogged capture
was complete. No queue-state acceptance is claimed for this run.

Both logs and saved-On settings are preserved under
`local/depth-timeline-20260922/`. `review_4568.py` checks the identities,
work-item/tracker bindings, two frames, one-correction limit and absence of
finished bridge evidence, producing `review-4568.json`.

| Evidence | SHA-256 |
| --- | --- |
| `retry-4568.jsonl` | `4762C02232B16ED91C74AAAA9BCB64AB0F8782B8F590FE419018F17D60D99C44` |
| `bridge-4568.crash.log` | `AD3DB02F0E3B8470A3DF13F2E64C68F801BD8142C84B1FD7B5CFA32F26EC13F5` |

After confirming game exit and preserving the exact saved-On configuration,
only DLSS was reset Off. TAA, DX12 On, FullScreen 2560x1440 remain unchanged.
**Do not repeat v0.7.6 unchanged.** Next work is to retain the bounded completed
first-Evaluate snapshot independently of worker text-log scheduling, so the
external recorder can preserve it before forwarding the next fault. That should
also distinguish an unlogged Evaluate from one that never reached capture.
This was implemented in the [v0.7.7 capture mailbox](capture-mailbox.md), and
run 38476 above accepted the completed bounded capture.
The observation design and original staging procedure follow below.

## Question and static evidence

[Run 30272](evaluate-barrier-capture.md) accepted color, motion and output
declarations in the Evaluate command list's current recording. Depth had no
matching barrier there. Its primary backing and tracker were present, but the
tracker's state word and other command lists were not retained by that report.

The hash-recorded export of host `+0x22927F0` explains a possible cause. With
requested engine state `4`, the helper can update read bookkeeping and return
without adding a transition when the selected tracked state already contains
that bit. It uses the active usage record's state when tracker/context epochs
match, and the tracker's state otherwise. This is a branch prediction from
static code, **not an observed state in run 30272**. Engine `4` maps to D3D12
NON_PIXEL_SHADER_RESOURCE (`0x40`) through `+0x259BA50`; these enums cannot be
cast directly.

The original executable's version **0.0.0.0** and SHA-256 were rechecked before
deriving the added offsets:
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The decrypted analysis capture's SHA-256 is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Neither binary is distributed with this repository.

## External tracker snapshot

The existing exception contract remains: null RDX at host `+0x22927F2`, R8=4,
the exact caller and the verified depth work item. The recorder now verifies
**12** live code spans, adding the epoch/active-state branch at `+0x229282C`
and the different-epoch branch at `+0x229290B` to the previous ten gates.

After the original 105-read work-item snapshot, it makes at most **64 additional
scalar reads** through the preparation context in RCX and the verified primary
backing. It rechecks owner+96, backing+32 native resource and backing+112 tracker
against the work-item capture before following anything further.

| Object | Fields captured |
| --- | --- |
| Preparation context | Descriptor, operation and epoch slots at +0/+8/+16, then their scalar values |
| Tracker | First/active record +0/+8, epoch +16, before/state u16 +24/+26, read operation u32 +28, read epoch +32, flag u8 +40 |
| Same-epoch active record only | Descriptor +0, operation u32 +8, next/previous pointer values +16/+24, before/state u16 +32/+34, flag u8 +36 |

There is no linked-list traversal. An older-epoch active pointer is recorded
but never dereferenced. Anchors, epoch and selected state are rechecked before
accepting the snapshot. Unreadable, missing, changed or duplicate-key input
rejects the new retry precondition. The same-epoch path requires a non-null
readable active record. The overall remote-reader limits remain 256 calls and
4096 bytes per matching exception.

`depth-tracker` contains the named scalars; `depth-tracker-decision` reports
completeness, epoch selection and `compatible_read_predicted`, with
`gpu_state_verified=false`. A complete snapshot is required before the existing
one-use RDX correction. Compatibility itself is not required: an incompatible
state may legitimately produce a new transition and is useful evidence too.
The correction policy, context verification/rollback, unchanged RIP and
one-correction budget are unchanged. No remote memory writes, injected calls
or new breakpoints are added. A second matching fault is forwarded and the
recorder detaches; another crash is expected if that path repeats.

## Bridge observation contract

The four command-list hooks and selected-recording summary from v0.7.5 remain.
Three queue methods are added, all Windows COM `STDMETHODCALLTYPE` under the
Windows x64 ABI. Their signatures and caller-owned argument lifetimes are:

| Method | Slot | Signature after implicit `this` | Tested system D3D12Core RVA |
| --- | --- | --- | --- |
| ExecuteCommandLists | 10 | `void(UINT, ID3D12CommandList *const *)` | `0x11280` |
| Signal | 14 | `HRESULT(ID3D12Fence *, UINT64)` | `0x88930` |
| Wait | 15 | `HRESULT(ID3D12Fence *, UINT64)` | `0x88A80` |

The command-list RVAs remain ResourceBarrier `0x23B280`, Reset `0x239FD0`, Close
`0x235DC0`, ExecuteBundle `0x238A80`. Both WARP and hardware fixtures exposed
these seven methods in system D3D12Core **10.0.26100.9278**, SHA-256
`70710DE807F188A85A3F5AD3A729A41BFA7A92B7621CEEAF38059F5640399267`.
RVAs are observations, not fixed hook targets.

At admitted Create, the observer borrows its command list, obtains a temporary
device reference, creates a temporary DIRECT queue, copies its three method
addresses, then releases queue and device before arming. Foreign access faults
or incomplete discovery reject installation. The seven targets must be distinct,
executable committed image memory in system-directory D3D12Core/d3d12. Partial
installation rolls back; trampolines that were enabled remain alive for any
in-flight calls. The existing process-pinned hook policy is unchanged. In the
fixtures, hooks are removed only after calls are quiescent.

During observation, only bounded barrier structures, submitted list addresses,
queue/fence pointer values and scalar metadata are copied. No list/resource/fence
reference is acquired or retained by callbacks. Original arguments, HRESULT and
LastError are preserved. The observer does not add a barrier, submission,
Signal, Wait or any GPU command. The temporary discovery queue is never submitted.

The Create-to-first-Evaluate window remains **two seconds**, with a shared cap
of 2048 API calls, 16384 barrier descriptions and 1024 barriers per call.
Submission storage is capped at 4096 list addresses, 128 per Execute call.
The observer freezes before Evaluate queries; callbacks never wait for each other.
Faults, expiration, overflow, contention or unfinished calls remain visible and
prevent selected-recording declaration acceptance.

The new `bridge.depth-timeline` keeps the latest 16 depth-related barriers
across all observed recordings, up to 64 relevant Reset/Close/Bundle events,
64 queue events and 256 submitted list addresses. Every truncation or malformed
range is reported. `bridge.depth-history`, `bridge.depth-control`,
`bridge.queue-event` and `bridge.queue-list` carry their bounded details. Kinds
are 0 barrier, 1 reset, 2 close, 3 bundle, 4 execute, 5 signal, 6 wait.
Global UAV/aliasing records may match depth and consume history slots too.

Entry/return counters bracket the original API call and identify overlapping
CPU observations. Reservation/call number alone does not establish thread order.
For void API calls, result S_OK is only the observer's completed-call marker.
Queue and list pointer equality is a correlation clue; destruction/reuse and
alternate method implementations are not observed. Bundle contents remain opaque.

An [ExecuteCommandLists call](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists)
is submission evidence, not proof that the GPU executed the work; runtime
validation can reject a submission. Recorded
[queue Signal](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-signal)
and [queue Wait](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-wait)
calls describe requested fence synchronization. Wait returns without waiting
on the CPU. The game observer does not query fence completion or infer GPU
completion from CPU call order. Initial states, pre-window work, implicit
promotion/decay and resource lifetime still require separate evidence.

## Verification, staging and next run

Pure tests cover epoch selection, stale active pointers, anchor/state changes,
failed reads and malformed work items. Timeline tests retain depth from another
list and reject malformed submission ranges while reporting bounded truncation.
Real WARP and hardware fixtures record a depth transition on a producer list,
then Execute, Signal, Wait and a completion Signal. They verify original queue,
fence, values, call results and CPU order. **The fixture itself** submits that
one barrier and waits for fence completion before releasing its resources;
the observer adds none. This does not validate the game's GPU execution.

Release builds pass with project `/W4 /WX`. All 23 CTests and both queue fixtures
pass. The x64 bridge has 113 CFG targets and 15 distinct exported provider/
callback wrappers, all present in the CFG table. Local evidence, source copies,
test logs, binaries and the staging manifest are under
`local/depth-timeline-20260922/` and remain ignored by Git.

| Artifact | SHA-256 |
| --- | --- |
| Bridge v0.7.6, 625152 bytes | `5BE3E39B95EF9B4100BCB0B738D44434E8D4346E76ED02C1C85518830A19689F` |
| External recorder v0.3.0 | `AD0FCBD24899D21E9529C8C52A88E3DA5213BF7C88968FE4512F6F632A7FC9BC` |
| Preserved active v0.7.5 | `045465F3C84E6637DB283B08550218170366C7643E046E4B90680E7B32933035` |

With the game closed, the exact active v0.7.5 DLL was preserved beside the active
MO2 mod DLL as `mhw_dlss_bridge_probe.v0.7.5.dll.disabled` before staging v0.7.6.
The graphics configuration was copied to `graphics_option.before-076.ini`;
it was not modified. DLSS Off, TAA, DX12 On, FullScreen 2560x1440 remain saved.
MO2 PID 7112 retains the verified environment and needs no restart.

Launch the same offline **Monster Hunter: World - DLSS 1.2.110 test** entry and
wait at the main menu with DLSS Off. Verify fresh v0.7.6 startup, exact identities,
both providers, mode and the still-open five-minute worker window, then attach
the archived v0.3.0 recorder with a fresh output file and `--repair-depth-once`.
Only after ARMED should the user enable DLSS once, close its popup and leave
Options. Stay at the main menu; do not load a save or toggle again. Preserve both
logs and the post-run settings before any saved-On recovery with the game closed.

Acceptance requires a complete tracker snapshot tied to the same corrected
work item, verified retry and resource identity, an untruncated error-free
timeline, and explicit interpretation of the relevant recording/submission
events. A compatible tracker bit alone does not prove the native depth texture's
GPU state or contents. If the preceding transition falls outside the bounded
window, report that gap; do not repeat this unchanged capture or invent a state.

To remove the experiment, close MHW and disable the bridge Force Load entry in
MO2. To restore v0.7.5, preserve the new active DLL under a disabled versioned
name, verify the v0.7.5 backup hash above, and copy it to the active name in the
same mod. No original game executable or asset needs restoration.
