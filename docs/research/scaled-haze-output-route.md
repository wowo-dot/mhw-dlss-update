# Optional haze output route (v1.25.10)

Prepared 2026-09-30 after the three accepted v1.25.9 enqueue recipes.
This is the first live-enabled haze adapter; in-game success remains unverified.

## Rendering contract

The completed same-frame post-filter queue must contain the existing temporal
copy, motion filter, color grade and final screen draw. If present, exactly one
haze descriptor must have type RVA 0x35372b0, stage 15, order 20000, descriptor
unit+0x140, motion output as input and color-grade input as output. Its queue
draw must have key 0x78004e20. Duplicate, missing or broken links reject the plan.

Semantic stage IDs remain motion=0, grade=1, final=2; haze is 3. Execution order
is [0,1,2] without haze and [0,3,1,2] with it. Admission, recording, completion,
barrier tracking and retirement all use that frame's admitted sequence.
Changing sequences does not evict live resources.

The private haze PSO uses the captured 712-byte VS and the exact hash-qualified
14596-byte PS through make_haze_shader. The native three-vertex strip's reduced
UV footprint is preserved, including its vertex order. The viewport becomes
full output size. Only the final color-sampling coordinates are multiplied by
two; depth/noise/filter sampling, constants, techniques and clipping stay native.

Shader identities:
- VS c7c73b6ac661514c34c53681e217dc3fe7014e21a99a41adb0fcb7befc1f5de6
- PS 4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69

## Bindings, states and lifetime

At enqueue, read the actual single count/start pair for each shader stage.
Require the captured nine-entry PS binding recipe, owner-table indices 2..5
for t0..t3, ordinary owner modes, non-null descriptor sources, and the exact
motion-output color owner. Retain the haze output and all three auxiliary
resources with the existing graph resources. GetDevice/GetDesc validate device,
dimensions, sample count, texture dimension, compatible float/unorm formats,
and shader-readable resource flags. Depth must cover the output allocation,
the volume must be 32 cubed, and the filter must be a bounded 2D texture.

Recheck the complete owner/view/source-descriptor table at temporal claim and
at the haze API draw. Require the API observer's command generation, native
PSO/root/raster/RTV identity, output RT state, and color pixel-SRV state.
The native code-gated executor still binds all descriptors and constant buffers;
the adapter preserves them and verifies tracked state restoration afterward.

Auxiliary states and GPU descriptor contents are not independently read back.
Their native binding/transition path is reused unchanged, with identical
auxiliary sampling footprints. GetDesc and CPU descriptor-source identities
must not be described as independent GPU descriptor-content proof. The offline
GPU fixture exercises actual descriptors, states and shader reads under D3D12
GPU validation. Live descriptor allocation and root-layout behavior remain part
of the next runtime check.

The existing x64 Windows ABI post-filter, draw-executor, fullscreen-helper and
copy hooks are reused. No new hooks or target offsets. Existing full code gates
include haze preparation RVA 0x24c7760, fullscreen helper 0x2297e20, shader binder
0x259f330 and draw executor 0x259d670; see the preparation manifest for the full
38 spans and their hashes. Tested host SHA-256:
c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea.
Decoded executable:
d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c.

All native resources, draw PSOs, roots, vertex buffers and bound descriptor heaps
are retained until both submission-fence completion and successful recording
Reset. Per-recording capacity increases from 24 to 32 pins to cover four draws'
heaps. Private effect objects follow the existing stop/drain retirement path.
Capacity failure rejects admission; no live entry is evicted.

## Offline results

Evidence root: local/scaled-haze-route-20260930 (not sealed before runtime).

- Ordinary production route: 96 frames, 288 draws, 786432 exact copied pixels,
  changing native vertex resources and complete retirement.
- Mixed route: 96 frames alternating ordinary/haze, 336 draws, 48 haze frames,
  zero GPU validation warnings/errors, all recording references released.
  Wrong order, changed bindings and invalid color state reject before recording.
- CPU graph: 14 existing malformed graphs; 10 new malformed haze cases,
  actual auxiliary-resource pins, post-admission binding mutation rejection and
  successful native-VS/PSO construction. Three bounded capture samples and their
  ten existing malformed-input cases still pass.
- Captured native VS/production triangle plus shader adapter: 50 real GPU draws,
  860160 float comparisons, all five techniques with/without distortion,
  13 rejected shader/geometry inputs, zero GPU validation warnings/errors.
  Independent unmodified native-PS texture-layout oracle and CPU color oracle.
- Four native CTest targets passed, including 22000 changing-output lifetime
  iterations; 28 Python scale/readiness/correlation tests and 13 captured-log
  regression tests passed. Prior v1.25.9 live trace still reviews successfully;
  synthetic v1.25.10 telemetry is explicitly marked synthetic.
- C++ Release build uses /W4 /WX; no new warnings.

GPU tests used RTX 5090, LUID [0,94196]. These tests establish local contracts,
not in-game stability or visual quality.

## Runtime test and rollback

Deploy only while the game is closed to the existing MO2 dlssTEST DLL slot.
Preserve the exact v1.25.9 DLL and output sidecar in the new evidence directory;
deployment.json records hashes and paths. Change the output sidecar to the new
directory so the sealed previous run remains untouched. Keep native AA/DLSS
Off, DX12, 2560x1440 windowed, barebonesdlss profile and the existing test entry.

Launch, load the save outside the sauna, and report ready. After verified
arming/baseline, enter the sauna, stay there, then leave/re-enter and check
recovery/continuity. Stop deliberately after collecting the evidence. Keep this
DLL installed afterward as requested. Panning history resets and consumer-depth
admission skips remain separate; this build targets the haze interruption.

For explicit rollback while closed, the local deploy.ps1 -Action Restore checks
the installed and backup hashes before restoring the exact prior DLL/sidecar.
Do not perform rollback automatically after a successful test. Neither original
game assets nor MonsterHunterWorld.exe are changed.

