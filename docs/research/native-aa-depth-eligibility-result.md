# Depth eligibility live result, v1.21.4

Closed and archived 2026-09-27. The diagnostic identifies the scene-depth
rejections as attachment-only changes, while the destination texture and
descriptor identities remain unchanged. A separate group reaches the lease
check with no available depth copy. The user reported DLAA stayed active and
clean after an area load followed by walking/panning.

## Run and verification

- PID 33872, creation FILETIME 134349990745610612; D3D12, Windowed 2560x1440,
  swap-chain format 87, LUID [0, 98547], native AA/DLSS Off.
- Host version 0.0.0.0, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- DLL SHA-256
  `1b3e32588459ce8473bb2506d23ff8076a5a9787cf296edde9c33944c12d0d5c`.
- Root `local/native-aa-depth-eligibility-20260927/v1.21.4/`.
- 382-file prelaunch seal SHA-256
  `eb3c208f61c827fec042c21e64328c6c177bb24bb12b0cc7a8ce011a27ce9244`.
- 401-file final archive SHA-256
  `533f43e492b53c96b56a2bd39788fa1673e7ae88035825d27de0e0448999307b`.
- Final log SHA-256
  `5f293fdf0a1921faa172592d99f0325b2734dae0a1026fc11d6fe2de52bc4ad6`.

The assistant requested intentional stop after 99.844 seconds. All ten channels
passed at baseline, area-load and final checkpoints, and the frozen full-log
review accepted the run. Baseline: 1,174 DLAA evaluations, 87 scene rejections,
no lease rejections. Area-load checkpoint: 4,761 evaluations (+3,587), 300 scene
rejections, 126 lease rejections. Final: 7,841 evaluations, 438 unfiltered frames.

Zero runtime faults, SDK failures, request collisions, state rejections or native
forwards among tracked replacement candidates. All four native current-use
supplement attempts accepted. All 323 suspension episodes had resumed at final
stop; no failure latch or waiting state remained.

All 8,153 SDK/depth recordings and 438 unfiltered-copy recordings were submitted,
completed, retired and released. Both trackers had zero live references/pending
submission/reset/GPU work. All 8,680 jitter scopes restored, hooks disabled,
and no pending callbacks. User confirmed normal exit; process absence checked.

The archive includes startup/modules/prearm receipts, raw checkpoints, their
independent reviews, arm/stop receipts, final review, normal exit and run result.
`finalize_run.py` revalidated the complete frozen run, all sealed files, sources,
dependencies, installed DLL, prior backup/sidecar and four configurations. No
settings change or restoration occurred. v1.21.4 stays installed as requested;
there is no armed run.

## Exact rejection evidence

All 312 reason-3 rejections returned a readable scene; no core read failed.
Each differs in exactly these four fields, with every other field's difference
counter zero:

- `attachment`
- `attachment_owner`
- `attachment_resource`
- `attachment_readable`

The scene container, thirteen destination texture Owner members, SRV pointer,
CPU descriptor and SRV readability remained equal at this comparison across
all 312 cases. The first eight and latest retained samples show the source
attachment absent in the producer snapshot and present at the consumer; each
reports 24 reads with zero failed reads. The aggregate counters establish which
fields changed in all cases; only the retained samples establish direction and
particular values. There is no separate pixel/depth readback in this run.

All 126 reason-7 rejections fail list, generation, frame and depth-available
checks, with no already-evaluated rejection. Every retained sample (first eight
and latest) has the exact empty lease `[0,0,0,0,0]`. This identifies absence at
consumption, not a stale populated lease in those samples. It does not explain
why the earlier depth copy was absent, or tie every rejection to a precise
loading/focus moment. These rejections appeared between baseline and area-load
checkpoints, then remained at 126 while another 3,080 DLAA evaluations completed.

The runtime count of depth recordings equals 7,841 successful frames +312
scene rejections in this run. This aggregate equality is not per-frame evidence
that every scene rejection would pass the later lease check. The current early
return prevents observing that later check on reason-3 frames.

## Next correction

Use an explicit destination identity comparison at dispatch time instead of
whole-`SceneDepth` equality. Exclude only the four transient source-attachment
diagnostic fields demonstrated above. Preserve scene readability, container,
full destination Owner, SRV/descriptor identity and SRV readability. Keep full
producer-before/after equality unchanged.

This follows the existing separation: the captured native copy packet supplies
the source attachment; its exact source/destination COM identities and copy
states are validated in `copy_hook`. A selected TAA request matches the copy's
frame/epoch/view/bank/async/container/destination identity before retaining its
resource pins and copying the private depth. Resolve then requires that private
copy's exact list/generation/frame and unused availability. A later mutable
context attachment is not the same thing as that retained copy's provenance.

Keep all of those actual copy, state, identity, ownership and frame checks.
Missing-depth-copy frames must continue to suspend reconstruction and use the
existing unfiltered path. Test the explicit comparison against diagnostic-only
changes and actual identity/readability changes before a new live deployment.
Retain bounded evidence to measure accepted attachment-only changes and any
remaining lease failures. No corrected build is prepared by this result.

Scope remains a short diagnostic run with one user-reported area load. There
was no controlled focus/resize cycle, soak or pixel-quality readback. Whole
renderer AA exclusivity remains unverified (`exclusive_mode=false`). CMFM/UI,
resolution-changing DLSS and frame generation remain deferred.
