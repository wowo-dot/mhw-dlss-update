# Independent AA-Off DLAA: sauna stop is missing local depth-state evidence

2026-09-27, v0.8.3. **Diagnostic objective met; continuous run remains partial.**
Fault 19 rejected only the depth input's state. Color and motion both supplied
the expected whole-resource RENDER_TARGET -> PIXEL_SHADER_RESOURCE transitions
in the same batch. Depth was untracked in this Reset generation at the marker,
before the batch and after it. There is no recorded conflicting depth state,
split barrier or alias event in that generation.

The user entered the sauna, initially reported continued DLAA, repeated walking,
panning and entry/exit, then noticed deactivation. Five haze entries and four
exits were observed before the fault. This supports the sauna as a useful
reproduction area, not a guarantee that the first entrance triggers failure.
The earlier report that the TAA-dependent implementation worked in the sauna
remains a user observation, not a newly captured A/B test.

## Identity and captured evidence

PID 19364, creation 134349322979379883. Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256
`e4d25cb2290035e036028e29a50bfbfbd41e944565fd2a87936b1354a8e12513`.
Native AA and DLSS Off, Windowed 2560x1440, DX12, bare offline MO2 profile.
[Runtime and hook contract](../runtime/independent-aa-boundary.md).
Session `local/independent-aa-boundary-20260927/v0.8.3/`.

All 413 prelaunch files and startup/process/runtime identities passed before
arming. Arm tick 42925531; first rejected boundary tick 43017953, approximately
92.4 seconds later. Baseline and sauna checkpoints preserve 1,564 and 7,277
successful evaluations respectively.

`capture-19364.jsonl`: 259,092 bytes, SHA-256
`cea37f7e65a3a2756e25b98758e5c3da28821a34c9ab830bc6c6559115bf437f`.
Frozen strict `review-19364.json` rejects full acceptance (`healthy stop`).
Separate `observations-19364.json` verifies only the partial evidence, using
the frozen independent boundary and native-graph reviewers.

## Exact rejected predicate

Source frame/epoch 16848, native queue count 5, command list 358487152,
Reset generation 109615. Marker and batch-begin order 23; batch-end order 29.
The complete six-barrier batch contains exactly two transitions relevant to our
three inputs, both with flags 0 and ALL_SUBRESOURCES:

| Input | Resource | State evidence at boundary |
| --- | --- | --- |
| Color | 14872006208 | 4 -> 128, known and issue-free |
| Depth | 3159495968 | No transition in this recording; untracked/unknown |
| Motion | 3159505760 | 4 -> 128, known and issue-free |

The independent reviewer reconstructs `relevant=2`, `color=true`, `motion=true`,
`invalid_states=2` (depth only). None of the remaining four transitions names
the depth resource. The observer rejected before attempting the next SDK call.
There was no native queue rejection or failed native callback/return.

All three resource identities match the first successful base and haze
receipts, which use the same command-list object but earlier Reset generations.
In the successful base receipt (frame 9440, generation 400), depth has two
transitions, ending COPY_DEST (1024) -> PIXEL_SHADER_RESOURCE (128) at order 77;
the marker is at order 101. The first successful haze receipt (frame 12402,
generation 44105) has the same depth transition at order 44, marker at 68.
The failing generation has no depth transition at all before its marker at 23.

This points to a limitation of recording-local state evidence. A transition in
an earlier native command list or submission is a hypothesis that must be
verified in actual queue execution order. Current data cannot prove the depth
texture's real state was wrong or correct at the rejected point. Never borrow
the most recently recorded state from another CPU thread or a previous frame
without a GPU-order proof. The existing `gpu_state` uncertainty guard is doing
what its local contract requires; simply disabling it is not the fix.

Next: trace this depth resource's transitions and copies across Reset/Close/
Execute, preserve submission array order and queue/fence dependencies, then
establish a valid depth-read contract at the independent insertion point.
The older TAA route's frame-linked depth copy and existing submission journal
are relevant references. No repeated image-readback milestone is required.

## Successful scope and closure

7,408 successful SDK evaluations/native handoffs: 5,282 base and 2,126 haze.
All 7,409 admitted queues returned and all 7,409 markers were consumed. The
first complete haze and first base-after-haze graphs pass independent endpoint
and exact descriptor-order restoration checks. First base/haze input boundaries
also pass their independent checks. Haze appearance and removal are therefore
observed successfully in this run; full continuous acceptance remains rejected.

All 7,408 issued private GPU signals completed; all 7,409 scoped jitter changes
were restored. All hooks disabled, descriptors detached, pending callbacks zero,
no pending private recording. The fault retained two slots, including one
consumed but unevaluated/unsubmitted marker. Awaiting GPU/Reset/submission each
reports one for that marker; this is not an incomplete issued fence or full
record retirement. SDK/list references remained pinned until process exit.

History reset on 506 evaluations. Reasons overlap: one initial, 310 projection
changes and 281 camera-distance reasons. History policy remains unchanged and
separate from the local depth-state issue. No SDK/pass error string was recorded.

The user exited normally. The sealed helper verified process absence, restored
the exact prior DLL (`689bef7c...`) at 2026-09-27T00:42:27.108394+03:00, removed
the matching sidecar and confirmed all four configurations unchanged. No source,
tests or tools changed during capture or closure. No test remains armed.
