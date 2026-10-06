# v1.25.7: correlate early draw callbacks and changed filter connections

The v1.25.6 trace identified 25 output-order rejections while the user reported
brief panning skips. It separately recorded 615 depth interval rejections and
503 unbuilt output plans during the run containing sauna entry. See
`scaled-output-readiness-result.md`. The boolean executed mask does not say
which command list recorded the draw or when it was submitted. Build detail 4
does not distinguish a failed read from a different grade input.

This diagnostic revision records those missing identities. It preserves the
original build/admission short-circuit order, copies, draws, SDK evaluation,
resource retention, queue waits/signals and stop behavior. It is not a claimed
panning or sauna fix. No new hooks are installed.

## Hook scope and identities

The existing native draw wrapper at RVA 0x259d670 uses the register-preserving
x64 adapter; its DrawInstanced callback identifies the native command-list
generation through the existing binding observer. The parent runtime's existing
ExecuteCommandLists hook uses its original Windows x64 COM signature. Immediately
after forwarding the native call, it reports the already-copied batch and the
generations obtained under the parent lock before the call. The submission lock
still surrounds native Execute and the original private signals. The observation
does not add synchronization to the GPU.

Tested executable remains version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded research image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both identities are verified before deployment. All 36 native code gates and 48 total hooks retain
their existing ABI, targets and lifetime rules. DLAA remains version 1.21.5;
the new runtime submission observation is scaled-only.

## Bounded evidence

`scaled_route.diagnostics` schema 1 uses policy
`bounded-output-correlations-v1`, capacity 4, history capacity 128.

Graph receipts retain the first successful build, first four rejected builds,
and latest rejection. They copy only fields already read by the bounded native
filter scan (at most 16 descriptors). Each descriptor records its pointer, stage,
order, unit, type, input owner and output owner. Links record motion unit, grade
unit, endpoint, motion input/output, grade input/output and captured final source.
The four `link_reads` bits correspond to reads at motion +150/+158 and grade
+248/+250. The native predicate order is preserved. A changed grade input
therefore records bits 0..2 without reading grade output solely for diagnosis.
Reason 18 in this channel covers an exception or a false return without an
existing build-reason code. It does not replace the original route fault.

Order receipts retain the first four and latest executed-mask rejection. They
contain requested frame/epoch, actual plan identity, expected packets, three
draw observations and four recording identities (temporal plus each effect).
Draw observations record tick, list, generation, native recording, executor,
context, packet, native-return flag, validated-API flag, and callback-time native
frame/epoch. The last two are two bounded reads per selected draw wrapper;
zero or an unvalidated observation must not be treated as a proven identity.
The existing validated API observation provides the recording generation.

Later matching callbacks can complete retained order receipts. Matching requires
the frame, epoch and expected stage packet. Submission receipts match exact
list pointer **and generation**, record queue, serialized batch ordinal, position,
batch size and tick, and never acquire a resource reference. A 128-entry history
supports callbacks/rejections observed after submission. Unknown or overwritten
history remains explicitly unknown. First observed submissions cannot be replaced
by later submissions. The reviewer permits callback/submission fields to fill
later while preserving the immutable case identity.

All storage is fixed and protected by the existing route mutex. It never owns
GPU resources or grants admission. The existing five-second writer serializes
the evidence. Receipts establish CPU recording/submission correlation, not GPU
completion, state correctness, or an authorization to remove readiness checks.

## Offline verification

Both DLL targets, the route fixture and recorder tests build with /W4 /WX.
48 selected native tests and 112 Python tests pass; the final recorder revision
was rerun after its identity fields were added. Native/Python tests cover
reordered callbacks, submission before/after observation, generation reuse,
unrelated packet/frame rejection, bounded history wrap and immutable receipts.
Full synthetic reviews cover versions 1.25.0 through 1.25.7 and reject missing
required channels. These synthetic identities are not live evidence.

WARP and the current NVIDIA adapter each pass 96 route frames, 288 effect draws,
786,432 exact copied pixels and full retirement. The actual plan builder still
rejects all 14 malformed graphs; its changed grade-input fixture records reason
6/detail 4 and read mask 7. Actual route submission observations count all 96
fixture batches. Six negative route fixtures also pass. Yesterday's NVIDIA LUID
was stale after the new session; the failed adapter selection is preserved,
and the verified run uses independently enumerated LUID [0,94196], same vendor
4318/device 11141. No rendering failure occurred in that selection attempt.

The sealed v1.25.6 live run is replayed by the updated reviewer. Unchanged depth
GPU evidence remains inherited by source hash; new depth behavior is not claimed.

## Next live sequence and deployment

Use the same bare profile, native AA/DLSS/MotionBlur Off, 2560x1440 window.
Load the save and wait for arming. Capture a still baseline, then pan without
menus or sauna entry. After that checkpoint, enter the sauna and reproduce the
longer interruption; leave it to confirm recovery. Menu checks can be separate
if needed. Announce the intentional stop and verify GPU/jitter/scale/hook cleanup.

Evidence and exact prior slot/sidecar backup:
`local/continuous-dlss-20260929/v1.25.7`. Keep the latest DLL installed after exit.
With the game closed, `deploy.ps1 -Action Restore` restores the preserved DLL
and sidecar only if requested. Original game assets and settings remain untouched.
Performance optimization is still deferred by user preference.
