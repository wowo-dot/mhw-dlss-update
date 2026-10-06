# v1.25.6: distinguish the remaining output-readiness skips

The v1.25.5 run admitted all 4,181 consumer-depth copies, but four non-startup
frames rejected downstream output readiness. The user reported brief flashes
while opening the menu, possibly at a different camera angle. See
`scaled-consumer-depth-result.md`. The exact rejecting readiness predicate was
not recorded; early downstream CPU recording is only a hypothesis.

This revision adds observation at `stream_route::ready` without relaxing its
conditions. The original short-circuit order, resource-retention call, route
claim, failure latch and GPU operations are preserved. The accepted decision is
recorded before the existing claim mutation; retention failure is recorded
before the existing fault is latched. No new native memory reads, GPU commands,
resource references or hooks are introduced by these receipts.

## Identity and scope

Executable version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
All 36 native code gates and 48 hooks retain the targets, x64/COM calling
conventions and ownership rules documented in `continuous-scaled-output.md`
and `scaled-consumer-depth-copy.md`. The consumer-owned depth copy is unchanged.
The DLAA target retains version 1.21.5 and does not compile the stream route.

Performance optimization remains deferred by user preference while implementation
and general bug fixing continue. Existing research tools remain available.
The relative costs of diagnostics and rendering architecture are not measured.

## Receipt schema

`scaled_route.readiness` has schema 1, policy `bounded-output-readiness-v1`,
`diagnostic_only: true`, capacity 8. Every decision increments attempts and its
reason counter. Accepted equals route claims; rejected equals the parent
`scaled_depth_capture.readiness_rejected` count in a healthy coherent status.
The recorder retains one immutable first success, the first eight rejections,
and the latest rejection thereafter. It does not take a success snapshot each
frame. Fixed storage lives alongside the existing route state and uses its
mutex. Serialization occurs in the existing five-second status writer.

Reasons, in the original decision order:

| Value | Decision |
| --- | --- |
| 0 | Accepted |
| 1 | Recorder no longer accepting |
| 2 | Route fault already latched |
| 3 | Prior output route still active |
| 4 | Missing recording or generation |
| 5 | Plan not built |
| 6 | Plan already claimed |
| 7 | One or more downstream draws already executed on the CPU |
| 8 | Plan stamp differs from the current request |
| 9 | Effect pipeline missing |
| 10 | Effect signature differs |
| 11 | Current temporal packet unreadable or changed |
| 12 | Resource retention rejected |

Each receipt has a global attempt ordinal, rejection ordinal (0 for baseline),
reason, selected pipeline stage (0-2, otherwise sentinel 3), and these fields:

- `at`: tick, requested frame, epoch, command list, generation, native recording.
- `request`: context, view, bank, async.
- `plan`: slot, frame, epoch, published, built, claimed, executed mask, temporal
  packet, context, view, bank, async.
- `active`: slot, list, generation, recording, resolved, copied, draw count,
  associated plan frame and epoch (last two zero when no route is active).
- `accepting`, `fault`, `stamp_equal`, missing-pipeline mask and mismatching
  signature mask. These are observations under the route lock; atomic stop/fault
  flags can change between the original branch and its receipt.

`executed` is a CPU callback completion mask, not a GPU completion proof. These
receipts classify the existing rejection; they cannot authorize removing it or
independently establish image quality, native submission order or resource state.

## Verification and next live test

Both DLL targets and the actual output-route fixture build with `/W4 /WX`.
All 47 selected native tests and 104 Python tests pass. Native recorder tests
cover every reason, first-success-only capture, bounded first/latest failures
and repeated successful decisions. Python tests consume their actual serialized
output and reject changed receipts, corrupted counters, invalid reason evidence
and disagreement with the output/depth channels. Complete synthetic reviews
cover versions 1.25.0 through 1.25.6 and reject a missing readiness channel.

WARP and NVIDIA each exercise the modified route's 96-frame GPU fixture, with
288 downstream draws, 786,432 exact copied pixels and full recording retirement.
Each also rejects 96 deliberate foreign-frame readiness attempts; the new trace
correctly distinguishes 31 unbuilt slots from 65 already claimed slots after wrap.
Six malformed-route cases also run. These fixtures are isolated from the game;
they do not identify the live menu cause. The updated reviewer replays the sealed
v1.25.5 live log. Unchanged depth GPU evidence is inherited by source hash.

Use the same bare MO2 entry with native AA/DLSS/MotionBlur Off and the unchanged
2560x1440 window. Load a save and wait for arming. Capture a quiet baseline, then
repeat short menu openings at the original and changed camera angles. Inspect
the reason receipts before choosing a correction. Announce the intentional stop,
verify complete GPU/jitter/scale/hook cleanup, and exit normally.

Evidence/deployment: `local/continuous-dlss-20260929/v1.25.6`. Deployment preserves
the exact previous v1.25.5 DLL and sidecar. With the game closed,
`deploy.ps1 -Action Restore` restores those two files only if later requested.
Keep the latest installed by default. Settings and original game assets are
not changed. This is a diagnostic build, not a claimed menu fix.
