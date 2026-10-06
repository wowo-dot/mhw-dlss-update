# Prepared output return correction: runtime result

2026-09-30, v1.25.18, PID 12848 / creation 134352181330508053.
Evidence: `local/frame-output-return-20260930`.

The main-menu run progressed beyond the previous one-frame fault, loaded a
save, and exercised panning, menus, the sauna and an area transition. The user
initially reported these clean, then reported a sustained DLSS-off appearance
in the smithy, explicitly before the intentional stop. This remains an open
visual issue; do not describe the whole run as accepted or all skips fixed.

Final telemetry recorded 7,718 DLSS evaluations and 156 full-size spatial
frames, with zero runtime or route faults. All 7,874 prepared output chains
completed; 59 early CPU draws and 36 frames across multiple command lists
were observed. No frame was abandoned. All 7,877 private GPU signals completed
and all private recordings released. All 7,910 downstream recording references
released. All 8,656 jitter changes and full render scale were restored.
Independent read-only verification checked 38 full native function spans and
18 hook heads: every original byte range matched after the intentional stop.
The user confirmed normal exit; keep the installed DLL and settings unchanged.

## Remaining input interruption

The final depth admission reasons include 154 rejections at reason 6 and one
at reason 5. Reason 6 covers inspection of the packet/state-node interval
between the depth writer and reconstruction. One retained failing sample has
writer 3149, consumer 3212, first later reader 3192, one relevant node, no
derived state and no applied transitions. This narrows that sample to the
interval-reading part of the check, before its state-replay phase. It does not
identify which packet or condition failed. Do not weaken that check or infer
that the actual GPU state is invalid.

Spatial-fallback bursts were observed around 08:06:23-08:06:34 and 08:07:38
local time; polling bounds do not establish exact onset or map an area name to
a frame. The user's smithy report is a reproduction clue, not a precise
timestamp. The final 156th spatial frame occurred during shutdown. The next
diagnostic should retain the precise rejection point and a bounded packet/node
receipt so the shared depth check can be corrected from evidence.

## Reviewer discrepancy

The frozen full-run reviewer rejects `shader admission accounting`. Since
v1.25.17, `taa_after` validates the shader before preparing the output chain,
and dispatch validates it again. Both calls increment the same cache's
per-variant accepted counters. The reviewer still expects their sum to equal
dispatch checks alone. Every saved timeline sample instead satisfies the sum
of accepted validations = dispatch checks + published prepared candidates;
the final counts are 15,754 = 7,877 + 7,877. No shader rejection was recorded.

Preserve the frozen review and original failure. Future telemetry should count
preparation and dispatch validations separately and test the two-call path;
do not rewrite the original log, normalize it into a success, or silently waive
the review. Individual cleanup and lifetime observations above are independent
of this full-run acceptance failure.

The exact DLL, source, startup identity, arm/stop receipts, collector timeline,
checkpoints, rejected review and native-code audit are in the local evidence
root. The deployment script there preserves removal/restore instructions;
restoration requires a fresh output destination before launching because the
prior sidecar points at sealed evidence. No original game asset was modified.
