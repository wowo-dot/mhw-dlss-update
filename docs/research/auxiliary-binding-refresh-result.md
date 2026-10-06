# Auxiliary texture refresh: wingdrake runtime result

## Targeted result

v1.25.32 passed the main-menu, save-load and wingdrake checks on 2026-10-01.
The user reported DLSS stayed active and clean during travel. The recorder
observed the same class of replacement that stopped v1.25.31: radial PS t2
changed from 128x128 to 256x256 at frame 11215, stage 5. Entry admission refreshed
it once; all subsequent API comparisons matched the accepted entry bindings.
There were 347 completed scene-copy draws and 347 completed radial draws, with
zero refresh rejections, API binding rejections, runtime faults or route faults.

This verifies the [auxiliary refresh implementation](auxiliary-binding-refresh.md)
for the reproduced replacement. It does not establish universal scene coverage.
Gallery playback and the remaining monster-effect and hunter-overlay issues
were not retested in this run.

## Run identity

- Evidence: `local/auxiliary-binding-refresh-20261001`.
- Process: PID 6072, creation 134353070882316766.
- DLL SHA-256: `3d809257854d14defd454fcd0b40783a85a74f7ab3e8f07a1c3b5a06d7c54588`.
- Executable SHA-256: `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- DX12, RTX 5090, LUID [0,93877], windowed 2560x1440; reconstruction input
  1280x720, native AA/DLSS/MotionBlur Off, Stracker's enabled in the same profile.
- Prelaunch manifest: 766 files, including 598 frozen source files; SHA-256
  `a36252608711f32674aacdcd856c0a13dd157a13bf098e1ae938f61a9fe43030`.

## Terminal evidence and cleanup

The test stopped intentionally, with a healthy terminal event. Final counters:

- 9,246 DLSS evaluations; 9,274 main GPU signals completed and recordings released.
- 26 unfiltered GPU recordings, all completed and released; six suspension
  periods and six resumptions, with no remaining wait or latched failure.
- 9,861 output-route recordings retained and released; zero live or pending.
- All 9,740 jitter changes restored; full render scale restored.
- All runtime, activation, resolution and output hooks disabled. A separate
  read-only live check verified 40 complete native code spans and 18 hook heads.
- The collector ended with `native-terminal`; the user confirmed a normal
  scene and normal exit. Game and collector absence were independently checked.

The top-level `skips=2627` includes rejected dispatch attempts and is not a
count of displayed unfiltered frames. Use the separate lifetime receipts above.
The DLL remains installed at the user's request. Final close verification and
the archive manifest record installation, backup and configuration hashes.

## Deferred camp observation and review limitation

After reporting clean travel, the user noted that any skips a few seconds later
coincided with trying to enter camp, and explicitly requested deferring that
investigation. The retained command samples contain two color-chain mismatch
observations and 21 dependency-discovery rejections. The latter start at frame
11577, after the successful refresh at 11215. Their exact dependency subgate
was not captured by the periodic reader; camp attribution is user context,
not proof of the renderer cause. Reconstruction recovered without a runtime
fault. Keep this item in the [beta backlog](beta-backlog.md).

The frozen aggregate checkpoint review reports `command qualification` failure.
This is a known diagnostic schema mismatch: `continuous_output_commands.py`
accepts low qualification values only through 11, while the frozen renderer
emits 12 for dependency discovery and 13 for dependency order rejection.
The saved sample has signature `[17,0,12,10,0,32]`, matching the producer's
discovery branch. The outer diagnostics reviewer already permits 12/13.
The original failed reviews are retained. No reviewer bounds or renderer gates
were relaxed for this archive; do not describe the aggregate review as passing.
All eight other checkpoint component reviews passed, including refresh,
bindings, lifetime, reconstruction, activation and request validation.

Before relying on the aggregate reviewer for another run, correct its producer
schema coverage with positive and negative tests. This is separate from
investigating or admitting the deferred camp render path.

Follow-up on 2026-10-01: the correction and re-review passed, with the original
sealed archive unchanged. The new receipts are under
`local/gallery-followup-20261001`; see [the follow-up](gallery-intermediate-capture.md).
