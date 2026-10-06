# Current-use admission live result, v1.21.3

Closed and archived 2026-09-27. The bounded current-use state supplement passed
its first live activation and area-load test. The user reported DLAA stayed
active and clean through the loading screen and subsequent walking/panning.
This verifies the scoped state-admission change, not whole-renderer AA coverage
or every transition type. See the [implementation contract](native-aa-state-admission.md).

## Run identity and evidence

- PID 2524, creation FILETIME 134349976894368484. Windowed 2560x1440, D3D12,
  swap-chain format 87, adapter LUID [0, 98547]; native AA/DLSS Off.
- Host version 0.0.0.0, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- DLL SHA-256
  `0c1aaba52e641dffd7bdc51b64cbdf32537e49273e053dc9ad3a9c73899ab763`.
- Root `local/native-aa-state-admission-20260927/v1.21.3/`.
- 324 frozen sources; 375-file prelaunch seal SHA-256
  `955cfe4da3bf6a8bb8887a054414cef9c3a3641c11b23b70b23f60175b6d0fc1`.
- 394-file final archive SHA-256
  `1afe05d49e2b464fb369fa244937e8263e6907dc4a49782d79bea13e00b14a7a`.
- Final log SHA-256
  `56c41283a6049e64f130049d90a34ca83a3221f6925c246ca5e5fb6d15793048`.

`run-result.json`, `final-review-2524.json`, the three immutable checkpoint
pairs, startup/module/prearm receipts and `normal-exit.json` preserve the result.
`finalize_run.py` reran the frozen full reviewer and verified the seal, sources,
dependencies, deployed DLL, prior-slot backup, sidecar and four configurations.
No configuration change or restoration was performed. The current DLL remains
installed at the user's request; the game closed normally and process absence
was checked. There is no armed run.

## Verified behavior

Arm-to-stop duration was 245.890 seconds. The assistant announced and requested
the stop. Baseline had 14,258 DLAA evaluations, the area checkpoint 17,311
(+3,053), and the final drained status 18,485. All nine channels passed at each
checkpoint: history/SDK, requests, retention, resolve, forwarded requests,
activation, reconstruction, state trace and native state admission. The complete
frozen log review also accepted the run.

All four supplement attempts accepted with reported stable fresh identity
checks; independent replay accepted their captured shapes:

| Phase | Frame | Consumer index | Pattern |
| --- | ---: | ---: | --- |
| Activation | 19,919 | 5,864 | Earlier color transition and output initialization |
| Activation | 19,920 | 5,862 | Local color transition; output initially UAV |
| Area load | 36,411 | 370 | Earlier color transition and output initialization |
| Area load | 36,412 | 1,373 | Local color transition; output initially UAV |

Each pair contains consecutive frames. Current output use is `0x400 -> 0x400`
while its final planned state is `0x20`; admission uses the current-use contract.
Renderer epoch is request epoch +1. Native words are not API state flags.
The final tracker state remains excluded as authority. Acceptance precedes
other eligibility gates, so an admission receipt alone does not establish that
its particular frame reached Evaluate.

There were zero runtime faults, SDK failures, request collisions, actual state
rejections or native forwards among tracked replacement candidates. Native AA
remained Off. `exclusive_mode=false`: this does not count every renderer pass
as covered by the replacement.

Final drain verified:

- 18,897 SDK/depth recordings created, submitted, completed, retired and released.
- 443 unfiltered-copy recordings created, submitted, completed, retired and released.
- Both lifetime trackers have zero live records, resource references or pending
  submission/reset/GPU work.
- 19,381 jitter injections and restorations, all hooks disabled, no pending callbacks.

The single-fence telemetry correction produced no failed coverage checkpoint.
The prelaunch WARP regression remains the deterministic evidence for completion
advancing during serialization; this live run does not prove that race occurred.

## Remaining depth eligibility observations

443 frames used the unfiltered output path: 412 had dispatch reason 3 and 31
reason 7. Reason 3 combines failure to reread `SceneDepth` with inequality
against its producer snapshot. Reason 7 combines command-list, generation,
frame, available-depth and already-evaluated lease checks. Existing aggregate
counters cannot distinguish those predicates.

Both live checkpoints reported active DLAA and no pending suspension. Final
status at intentional stop had 399 suspension episodes, 398 resumptions and
`waiting=true`, with no failure latch. Preserve that final unmatched suspension;
do not report every suspension as recovered or assume the final unfiltered
frame was caused by the stop. This run did not reproduce the earlier extended
depth stall, but it also does not establish its cause or repair it.

Offline source inspection identifies one concrete question: `SceneDepth` uses
default equality across texture identity **and** SRV/CPU-descriptor metadata,
attachment pointers/resources and readability flags. `scene_depth` describes
the attachment as a diagnostic observation whose lifetime may differ from the
depth copy's destination. A change there could produce reason 3 while the
required texture remains the same, but no per-field evidence was recorded here.

Next work should distinguish failed reads, actual texture/owner changes and
diagnostic attachment/descriptor changes, alongside exact lease-rejection
predicates. Start offline; add bounded observations if the saved evidence is
insufficient. Keep the frame/resource/list-generation checks intact until the
cause is demonstrated. No new diagnostic build is prepared by this result.

There was no pixel readback, controlled focus cycle, sauna, resize test or soak
in this run. The planner-contract and two-read checks remain observational
within the native consumer's lifetime; they are not an independent GPU-state
query. CMFM/UI, resolution-changing DLSS and frame generation remain deferred.
