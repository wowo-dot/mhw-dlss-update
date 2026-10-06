# Continuous DLAA prototype (v1.18.0)

The user requested continuous rendering directly after the accepted v1.17.0
one-frame native handoff. This build replaces the fixed nine-receipt recorder
with streaming admission and reusable private textures. It is a prototype for
an offline, bare-profile gameplay test. Its first continuous live run passed
telemetry review on 2026-09-26; see the [result](../research/first-continuous-dlaa-result.md).
The user reported a clean image. The run ended on request after 20,802 evaluations
with completed GPU work, restored jitter and disabled hooks; exact rollback is
complete. Temporal quality and full transition stability remain to be validated.

## Rendering and lifetime

The original native TAA Dispatch always executes once. A qualified frame then
copies current color and packed motion, converts them with same-frame scene
depth, evaluates full-resolution SDK DLAA, packs its output into native
R11G11B10 storage, and restores all mirrored compute/graphics bindings. Depth,
color, motion, native Dispatch, and replacement must share one DIRECT command
list Reset generation. The original renderer remains the fallback.

One feature, fixed descriptors and reusable textures serve every qualified
frame. There is no frame or time cutoff. Only one private recording may await
submission. Subsequent recordings are admitted after ExecuteCommandLists and
its private Signal return. Switching DIRECT queues inserts a GPU Wait on the
last private signal; no game-thread CPU fence wait is introduced. Unexpected
re-execution, early Reset, missing states, or exhausted tracking storage stops
admission. Full resource re-creation is deliberately not implemented yet.

SDK history survives adjacent frames. Frame gaps, camera/view/depth identity,
projection or viewport changes, long frame intervals, and large view-matrix
jumps reset it. Matrix jumps are a cut heuristic, not proven coverage of every
in-game camera transition. Frame time comes from projection QPC timestamps.
Jitter uses the native table and is restored in scoped cleanup, including a
native exception or stop request. The matched frame's SDK offsets retain the
verified half-scale/sign conversion. Native DLSS remains Off and TAA On.

Storage is bounded: 32 projection/TAA slots, 64 depth requests, 128 command lists,
128 resource pins, and 16 output destinations. Private image/readback allocation
is 365,690,880 bytes at 1440p plus three raw input textures and SDK allocations.
The prototype reserves readback buffers but does not copy/read them per frame.
Private resources, SDK feature and native pins stay alive until process exit;
stop disables admission/hooks, and never guesses GPU/list retirement or calls
SDK Shutdown. No hot unload is supported. Latest status is overwritten every
five seconds; the durable log samples sparsely with logarithmic growth.

## Hook contract

Tested on host SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Install retains the existing structural discovery and exact code gates.
All native hooks use the preserved x64 register/stack ABI wrappers, forwarding
original functions once. Renderer producer RVAs are 0x23c9980 (TAA) and
0x23cd170 (scene-depth copy); consumer RVAs are 0x259cc20 and 0x259df30.
Projection helper 0x228f3b0 is qualified by caller 0x228e834 and scoped camera
jitter ownership. D3D interception covers Reset, Close, ExecuteCommandLists,
ResourceBarrier, Dispatch and CopyResource, plus the existing 18-method binding
mirror. Native resource COM references and list references are pinned and
same-device/format/extent/state/frame checked. Native assets are never patched.

## Offline verification

- 240 serial DLAA evaluations at 2560x1440, two explicit history resets.
- 600 evaluations with three recordings in flight and alternating DIRECT queues.
- Exact final SDK-to-native packing, downstream compute/graphics output and
  full binding equality; zero GPU validation warnings/errors in both runs.
- 1000 actual streaming projection callbacks against owned renderer memory;
  scoped restoration, stop, invalid provider/matrix/return and exception cases.
- 1000 actual streaming submission callbacks on WARP with two queues;
  duplicate generation and early Reset rejection.
- History reset policy tests and the 159-check existing temporal regression run.
- Original one-frame success plus missing-state/output/jitter fallback GPU tests.

Synthetic GPU sequences replay archived inputs and synthetic metadata. They
prove reuse, synchronization and binding behavior, not temporal image quality
in moving gameplay. The first live run confirmed growing evaluations, matching
terminal jitter/restoration counts, completed signals, fault=0, sustained
operation and a user-reported clean scene. Its 4,566 history resets and 73 skipped
attempts need investigation; sparse telemetry does not establish their causes.
Menus, cuts, alt-tab, resolution/device changes remain live acceptance work.
A crash is possible and is not an acceptance result.

## Local deployment and rollback

Evidence: `local/dlaa-continuous-20260926/v1.18.0` (ignored).
`deploy.ps1 -Action Install` checks the closed game and exact existing slot,
backs it up, copies only the MO2 diagnostic DLL and writes its output sidecar.
`deploy.ps1 -Action Restore` restores the verified prior DLL/sidecar while the
game is closed. Configurations are archived and checked unchanged.

Launch the usual MO2 DLSS test entry with the bare profile, load the save with
native DLSS Off/TAA On, and arm using `tools/arm_continuous_dlaa.py` with the exact
log and DLL SHA. Its `--stop` uses the same process-generation event. This build
has no fixed five-minute capture window. Do not launch public multiplayer.

`last_skip`: 1 dispatch identity/count; 2 changed input owner; 3 changed depth;
4 packet/shader/native cache; 5 binding mirror; 6 source/output states;
7 absent depth/list-generation lease. Fault codes are defined in runtime.cpp;
any fault ends admission and preserves the original path on later frames.
