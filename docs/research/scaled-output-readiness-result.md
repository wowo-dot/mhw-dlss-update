# v1.25.6: output ordering and sauna rejection paths captured

The diagnostic run on 2026-09-29 identified separate rejection paths. Brief
skips cannot be labeled menu-only: the user subsequently identified panning as
the trigger for the actual brief frame skip. Entering the sauna or a similar
spot also interrupted DLSS; leaving the affected spot restored it. No admission
or rendering change was made in this revision.

## Identity and evidence

- PID 21952; creation FILETIME 134351286024946286.
- DLL SHA-256 `9adac48d8127569975cb3818db473d6640b86daec6e2f0ffa5962a6d61f4c7c0`.
- Executable version 0.0.0.0; SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- D3D12, adapter LUID [0,93935], 2560x1440 output, 1280x720 render,
  swap-chain format 87, Performance mode/preset 11.
- Evidence: `local/continuous-dlss-20260929/v1.25.6`; frozen source
  `source-1.25.6`; `run-analysis-21952.json` retains checkpoint changes and
  distinct sampled readiness/depth receipts.

Startup capabilities, 521 frozen/current source hashes, installed DLL and
configuration hashes passed before arming. The collector saved 41 snapshots
and events over 208,343 ms, 2,592,359 bytes. Seven status reads needed retries;
no log read retry or runtime fault was recorded. Timeline SHA-256:
`83c45e4dde20d9551c8df9a79cf475d346f100dc272ab47edac18f2445b225e0`.

## Confirmed decisions

There were 6,266 successful DLSS evaluations and 1,145 skipped requests:

| Count | Recorded rejection |
| ---: | --- |
| 1 | Initial pipelines not yet available |
| 25 | One or more downstream draw callbacks already completed on the CPU |
| 503 | Output plan not built |
| 615 | Consumer-depth interval proof rejected |
| 1 | Recorder no longer accepting during the intentional stop |

The first later readiness failures appeared in the observation ending
07:15:53.816 local time. Their sampled plans were built, unclaimed, and matched
the current request. Pipelines were present and signatures matched. Executed
masks 4, 6, and 7 show downstream stages had reached their CPU callbacks before
the temporal readiness check. This confirms which gate rejected the frame;
it does not prove those commands had executed on the GPU.

The longer interruption has two additional observations:

- Failed depth receipts retained at 07:16:14.218 and 07:16:19.325 local time
  have [writer, consumer, next-use] indices [3278,3341,3321] and
  [3286,3349,3329]. The next native depth use is **before** the temporal
  consumer. The current proof only accepts a consumer between writer and next
  use. No state-node scan or private copy was admitted for these failures.
- During the unbuilt-plan period, sampled build diagnostics were reason 6,
  detail 4. This covers reading `grade+0x248`, matching that grade input to the
  motion stage output, and reading `grade+0x250`. It does not distinguish a
  failed read from a changed connection. All 1,118 unqualified plans equal the
  615 depth rejections plus 503 later readiness rejections, but this equality
  is not a substitute for missing per-frame field snapshots.

The depth failures appeared first; later requests could pass depth admission
while still lacking a qualified output plan. The trace therefore must not be
reduced to one missing-depth explanation. An intervening sauna post-processing
effect is a plausible hypothesis, not yet a mapped or validated resource path.

## Recovery and cleanup

The user reported normal appearance after leaving the affected spot. At the
recovery checkpoint, 6,131 evaluations had completed, the last skip was clear,
and all 15 pre-stop suspension periods had resumed. Fault remained zero.

The test was then stopped intentionally. The full reviewer passed accounting:
all 6,796 GPU signals completed, private recordings were retired/released,
all 7,417 jitter changes were restored, full render scale returned, and hook
disable receipts passed. Read-only process checks separately verified all 36
native code gates and 17 native hook heads against expected bytes. COM hook
removal is covered by runtime receipts, not those native byte checks.

The user confirmed a normal scene and normal exit. Keep the latest DLL
installed. Capture/accounting and cleanup passed; visual continuity did not.
The five-second checkpoints and reported actions do not establish exact
action-to-frame correlation. There was no live pixel readback in this run.

## Next work

Investigate how to qualify and record the full-resolution downstream chain
without assuming its CPU callbacks occur after the temporal callback. Validate
actual command-list submission dependencies before changing that ordering
check. Separately map the intervening depth uses and post-processing connection
in the sauna path; do not simply remove interval or resource-identity checks.

Retain diagnostics. Performance optimization remains deferred by user request;
this run does not measure its share of the frame-time cost.
