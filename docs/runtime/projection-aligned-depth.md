# Same-request-frame depth readback v0.8.2

The [v0.8.1 result](../research/depth-copy-frame-result.md) identified the original
copy-request stamp. Its next offline audit found the matching frame-14825 copy
at native order 5396: it was recorded before selected draw marker 5563 but
submitted afterward (Execute 5605, batch index 1, versus selected Execute 5599,
index 0). No observed depth clear, overwrite or alias lies between them in that
DIRECT queue's submitted recordings. v0.8.1's CPU-order admission skipped this
copy and captured frame 14826 instead. The matching frame-14825 pixels were not
saved. Local `v0.8.2/offline-alignment.json` reproduces this finding.

## Selection and bounded action

v0.8.2 keeps jitter Off and moves selection to the successful return of the
already-hooked projection helper. It atomically publishes one immutable record
of that source's core order, constants pointer, buffer ID, thread, renderer,
context, frame/epoch/bank/async stamp and a provenance order. The live stamp must
still match the source's entry stamp. Missing, duplicate or changed selection
cannot authorize readback. A future stimulus must publish only after restoration;
that branch is wired to the same helper but is not enabled by this diagnostic.

The next eligible native copy must have an original packet receipt built after
this selection, with exactly matching frame, epoch, bank, async mode, primary
context and producer thread. There is no fixed +1 offset, fallback to a later
frame, or worker-side global-frame substitution. The existing typed packet,
resource/canonical identity, full-depth extent, same-device, immediately adjacent
pure COPY_SOURCE/COPY_DEST declarations and recording-generation checks remain.

This copy may be recorded before the selected draw. At insertion, its source is
the pinned, described native CopyResource source from the matching request.
The selected draw's DSV check is deferred until submission because the draw may
not yet be recorded. Before adding the private Signal, the recorder requires:

- The selected draw returned and its submission is observed on the same queue.
- The copied resource and canonical identity match that draw's depth attachment.
- The original writable D32 view/lifetime check still succeeds.
- The selected submission precedes the copied submission, or occupies an earlier
  command-list index in the same batch. The same-list case is conservatively refused.

Failure after recording cannot undo the readback copy. It stops observation and
retains private resources until process exit without mapping uncertain data.
The copy itself only reads a native COPY_SOURCE resource into an owned buffer;
it does not transition or write a game resource. Success retains the same
private-fence completion plus later successful Reset requirement before Map.
Budgets remain one full-depth readback copy, one private Signal and two seconds.
There are no new hooks, queue waits, provider calls or jitter writes.

## Evidence and review

Header `0.8.2` adds `depth_frame_policy="selected-source-request"`; startup ready
requires `same_frame_readback=true`. Schema 4's frame-packet object gains a
return-published `selection` record. The readback site records its selection
order at insertion and fills its final draw marker only after the later
submission qualification. Consumers cannot mutate the published selection.

The new full-review contract requires the selection to match the independently
recorded projection source, the packet request's full stamp and thread, and
the actual copy. It checks queue/list submission order rather than requiring
CPU copy recording to follow the draw. Every intervening observed recording is
checked for a matching depth clear, a copy into the selected depth, an applicable
alias barrier or an unobserved bundle. Such evidence rejects the association.
Older versions retain their original CPU-order review contract.

Passing reports `selected_projection_copy_frame_matched` and
`selected_draw_precedes_depth_copy_in_queue_order`. It does not establish all
contributing pixel draws, cross-queue resource safety, final-scene coverage or
raster jitter. Those broader fields remain false. This is the zero-jitter
alignment control before designing a paired pixel-displacement experiment.

## Identity, verification and operation

Pinned executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
All [v0.8.1 ABI and ownership contracts](projection-depth-frame.md) remain:
41 hooks, seven startup gates, 39 core gates, 45 distinct byte-gated spans.
Startup still verifies the matching executable and live code before admission.

Preparation passes Release `/W4 /WX`, 159 native and 504 Python checks. Six
new owned GPU cases cover copy-first recording with same-batch and separate
Execute submission, wrong frame, missing/changed source and later DSV mismatch.
The two positive cases recover the exact GPU clear value through the production
readback path. The mismatch case retains its issued allocation unmapped with
zero private Signal attempts. Full-review synthetic composition remains labelled
as independent owned traces, not an actual native end-to-end game capture.

Local evidence is `local/projection-binding-20260923/v0.8.2/`. Verify preparation,
deployment and prelaunch receipts agree before testing. Use the existing MO2
`barebonesdlss` profile and `Monster Hunter: World - DLSS 1.2.110 test` entry.
Keep DX12 On, DLSS Off, TAA On; load a save, keep the camera still and reply
from the phone. Arm only the matching ready process generation.

Preserve the raw depth and complete core/provenance/packet journals, review with
the unchanged deployed source, then exit normally. With the game closed,
`deploy.ps1 -Action Restore` restores the exact previous DLL and sidecar state,
checking all four configuration hashes. Original game assets are untouched.
The [v0.8.2 run 42232 result](../research/depth-alignment-result.md) now passes
the unchanged deployed review with matching projection/request frame 51739,
correct DIRECT submission order and saved depth pixels. Consult the result
and local restoration receipt for the current deployment state. No unchanged
rerun is needed.
