# Main-menu, loading and rainy-area DLSS test, v1.25.14

2026-09-30, PID35100/134352038274769723. DLL SHA
48567ef9214b302e94f5936507a3a7bd79faeec306e80ce6958eea8d93368e69.
Host version0.0.0.0, SHA
c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea.
Same bare profile, nativeAA/DLSS/MotionBlurOff, DX12windowed2560x1440,
Performance1280x720. No settings writes. Current DLL kept installed.

## Confirmed observations

Startup and arming passed. This main-menu scene selected gameplay shader23086,
not the newly admitted29842 variant. All21,293 shader checks passed; no shader
rejection. New variant admission remains unexercised, so this does not verify
the prior shader fix in-game. Main menu had ZERO DLSS evaluations: depth
admission reason5 rejected the following native state12 instead of expected8.
Three stable saved planner samples show writer12->256 followed by256->12.
Pinned converter259ba50 maps12 to D3D12PS|NON_PS(192),8 toPS(128),256 toRT(4).
The full interval still needs validation before admitting that combined state.

User reported visible skips while loading. Saved output-order receipts show
final-stage draw recorded before temporal interception on another CPU worker.
In retained samples, both lists submit on the same queue in the same batch,
temporal list index1 and final list index2. The existing p.executed guard rejects
because the native final interception opportunity has passed. CPU recording
order is not GPU execution order, but these receipts alone do not authorize
dropping the guard or modifying already-recorded commands.

Loaded gameplay recovered; user reported clean image and counters confirmed.
User then reported rain and suspected skips. Two captured checkpoints show
1,867 additional depth rejections with ZERO new evaluations. In both latest
stable planner samples, depth writer precedes a native depth reader which
precedes temporal reconstruction; reconstruction remains within its read span.
Example indices3475(writer),3518(first reader),3538(temporal),3591(last use).
Transition256->8 is explicitly observed on first reader. Current required
writer<temporal<first-reader ordering rejects this before interval scanning.
Rain was user-observed; rain itself is not proven causal. After moving away,
user reported normal scene and evaluation count advanced from8,270 to8,735.

The wider pass was intentionally stopped after these three actionable defects.
Formal cutscene/monster/attack coverage was not established. Preserve the Steam
recording; collector timestamps bracket file reads, not exact GPU frame times.
Armed01:05:02UTC approximately, ended01:14:39.720UTC (04:14:39.720 local).
Collector577.250seconds,115events,12,130,177bytes,zero read retries or faults.

## Final verification and limits

9,200successful DLSS frames;12,093unfiltered replacements =10,888depth-order/
state rejections +1,205output-order rejections. Zero runtime/SDK faults.
All10,405GPU recordings completed, retired and released; all22,500jitter
changes restored. All9,200output routes complete/retained/released,live0.
Two unqualified native draw observations; no latched route fault.
Full resolution restored, callbacks drained, hooks disabled. Independent live
reads verify38complete native code spans and17hook heads restored. Frozen
reviewer ACCEPTS telemetry/cleanup; this is NOT a visual compatibility pass.
User asked about DLSS stopping; assistant confirmed intentional stop. User
then reported normal scene and normal exit. Process absent/settings unchanged.

Private evidence local/main-menu-variant-20260930, including separate menu,
loading, rain/recovery checkpoints, rain-analysis.json, menu-loading-analysis.json,
timed collector and native-code restoration. Source-1.25.14 is frozen.

Next: extend depth admission for the demonstrated native read states and
already-started read span, preserving full state-node/identity/epoch/lifetime
proof and adding meaningful CPU/GPU fixtures. Separately address the final
pass recorded early on another worker. No guard relaxation was applied live.
No reconstruction is currently running. Keep1.25.14installed per preference;
use fresh evidence before next launch into this sealed archive. History
distance-only comparison remains diagnostic and is not a final cut policy.
