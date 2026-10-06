# v1.25.3 menu-transition depth diagnostic

The first continuous DLSS run produced 6,901 successful frames and 232
unfiltered frames. Of the latter, 230 lacked an admitted current-frame private
depth copy and two failed later stream readiness. The user can reproduce a
brief skip while opening menus. The stage causing the missing copy remains
unconfirmed; see `first-continuous-dlss-result.md` for the sealed v1.25.2 run.

This revision adds observation to the existing callbacks. It does not change
resource admission, native target addresses, shader code, SDK parameters,
fallback behavior, render scale, or GPU ownership. No stale depth is reused.
The v1.25.2 byte-width and physical hook-removal corrections remain in place.

## Identity and lifetime

Host version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decrypted analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The preparation receipt revalidates the 35 existing code gates against that
analysis image and freezes the source, build and dependency identities.

Existing native producer/consumer hooks retain the Windows x64 register/stack
capture and return handling documented for continuous scaled output. D3D12
DrawInstanced and CopyResource retain their COM signatures. The relevant
observed native return RVAs are `0x259d72e` and `0x259e100`; the depth consumer
remains `0x259df30`. No additional hook is installed: 48 gameplay hooks total.

`ScaledDepthTrace` retains values only, not pointers it later dereferences or
COM references. Its fixed 128-event ring has no per-frame allocation or file
I/O. The first four missing-copy samples and the latest sample each retain at
most 32 events (first 31 plus latest on overflow), matched by exact frame AND
epoch. `matching` and `ring_overwritten` disclose incomplete evidence. Missing
events are not proof that the game never ran a writer, especially if overwritten.

A separate SRW lock serializes diagnostics. Where both locks are needed,
runtime ownership is locked before the diagnostic lock. No code acquires the
runtime lock while holding the diagnostic lock. Status serialization follows
the same order. An early producer rejection may lack a validated stamp; only
then, diagnostic scalar reads of renderer frame/epoch label the event. Such
labels are observations and never authorize resources or copied depth.

## Status schema

`scaled_depth_capture` schema 1, policy `bounded-scaled-depth-admission-v1`.
All event arrays contain:
`[tick, frame, epoch, depth_sequence, stage, reason, value0..value15]`.
Unused values are zero. Ticks use GetTickCount64, as in the existing recorder.

| Stage | Meaning | Reasons |
| --- | --- | --- |
| 0 | Primary-context kind-8 producer admission | 0 accepted; 1 stamp; 2 destination; 3 attachment/source; 4 source owner; 5 destination owner; 6 queue count |
| 1 | Published depth packet | 0 published |
| 2 | Temporal producer's depth selection | 0 selected sequence; 1 none selected |
| 3 | Matched depth consumer admission | 0 admitted; 1 packet changed; 2 backend/recording; 3 untracked list; 4 concurrent publication |
| 4 | Depth draw admission | 0 admitted; 1 list/caller; 2 repeated copy; 3 snapshot; 4 arguments; 5 resource states |
| 5 | Private depth-copy outcome | 0 copied; 1 changed return snapshot; 2 stopped/busy lease; 3 closed/untracked list; 4 no matching temporal request; 5 pin; 6 source child; 7 resource description; 8 CopyResource admission |
| 6 | Rejected draw's already-observed barriers | 0 state summary |

Values by stage/reason:

- 0: context, destination argument, read destination, attachment, source owner,
  container, flags-readable, context flags, pre-publication queue count.
- 1: outer packet, nested packet, source resource, destination resource, six
  packet rectangle fields.
- 2: temporal sequence, temporal packet, context, view, selected depth resource.
- 3/0 or 3: command list, recording. 3/1: packet, caller. 3/2: native argument
  1, argument 2, backend, recording, list. 3/4: packet, caller, publishers, matches.
- 4: list, caller, copy count, four draw arguments, snapshot rejection, active
  packet, pipeline, four rectangle fields, two packet draw arguments.
  Snapshot rejection is 1 bindings/arguments; 2 active packet; 3 pipeline;
  4 viewport rectangle; 5 copy rectangle; 6 vertex count.
- 5 value0 identifies route: 1 DrawInstanced, 2 CopyResource. Success then has
  list, generation, temporal sequence (and selected depth sequence for route 1).
  Busy has existing lease list/generation/frame. Untracked has attempted list.
  Changed snapshot has snapshot rejection. Pin/child rejection has only route.
  Route-1 selection failure has best same-frame predicate mask, temporal
  sequence, selected depth sequence, list, generation. Bits 0..8 represent
  live, not-forwarding, selected depth sequence, epoch, view, bank, async,
  container, destination owner; the mask is diagnostic only. Route-2 selection
  failure has list/generation. Description rejection has dimension, width,
  height, array size, mip count, sample count/quality, format, source flags,
  destination flags. Copy admission rejection has copy count, caller, list,
  source, destination.
- 6: barrier count; source transition count and last flags/subresource/before/
  after; destination transition count and last flags/subresource/before/after;
  alias-barrier count. It summarizes existing captured barriers only.

Each missing sample has expected `[tick,frame,epoch,temporal_sequence,
selected_depth_sequence,temporal_packet,list,generation]` and actual
`[lease_list,generation,frame,depth_available,evaluated]`.
`readiness_rejected` explicitly counts the later stream-route check, separately
from `missing`. The reviewer requires their sum to equal dispatch reason 7 and
`missing` to equal the existing rejected-lease count. It validates capacities,
frame/epoch identity, counter coverage, immutable samples and monotonicity.

## Verification and planned live check

Both continuous DLL targets build without project warnings. All 44 selected
native tests and 79 Python tests pass, including bounded ring wrap, frame/epoch
isolation, first/latest retention, missing-vs-readiness accounting, corrupted
records, and native shutdown regressions. Full synthetic log reviews pass for
v1.25.0 through v1.25.3; the new version requires the diagnostic channel.
The unchanged v1.25.2 GPU route/shader fixtures are prior evidence, not reruns
or live verification of this diagnostic revision.

Use the existing bare profile, native AA/DLSS Off and unchanged window size.
Load a save, arm, establish a quiet baseline, then open/close the reproducing
menu a few times while staying in the same area. Save a checkpoint immediately
after the report. Inspect stage evidence before changing any admission rule.
Stop intentionally, review GPU closure and native hook restoration, then exit.
Keep the latest DLL installed as requested. Video can supplement timestamps;
the pending clip is not required to begin the focused capture.

Evidence/deployment directory: `local/continuous-dlss-20260929/v1.25.3`.
The exact prior MO2 DLL and output sidecar are backed up there. With the game
closed, `deploy.ps1 -Action Restore` can restore them if requested; it does not
write graphics settings or original game assets.
