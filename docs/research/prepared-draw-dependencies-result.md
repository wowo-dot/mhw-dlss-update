# Prepared draw dependencies: live radial-blur result

2026-10-01, v1.25.28, PID 47832 / creation 134352949446244210.
The first controlled wingdrake trip produced two accepted captures in two
attempts, with no capture errors. The user confirmed that DLSS recovered.
This verifies the diagnostic recorder and the CPU dependency observation;
the wingdrake compatibility fix is still unimplemented.

Host version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Installed DLL SHA-256
`aa24b8e6b7dc63ae1beb0d25f6f7d07252babaa544ad20b79ae9ae5dc059b636`.
DX12, RTX 5090, 2560 x 1440 output and 1280 x 720 render rectangle.
Saved native AA/DLSS Off; existing Stracker-enabled profile unchanged.
No new hook, calling convention, rendering admission or GPU operation was
introduced by this diagnostic. The observation/lifetime contract is described
in [prepared-draw-dependencies.md](prepared-draw-dependencies.md).

## Confirmed command and resource relationships

Frames 43416 and 43476 each contain two draws under key `0x78001004`.
Each capture passed two complete equal CPU reads with a current frame stamp.
These are separate observations sixty frames apart, not consecutive GPU frames.

| Draw | Native arguments | Target | Sampled scene dependency |
| --- | --- | --- | --- |
| First | Four vertices, first 12; stride 16 | Temporary texture; attachment format 24 | PS t0 points to the resource that the second draw targets. |
| Second | Four vertices, first 0; stride 28 | Descriptor's scene output; attachment format 26 | PS t0 points to the first draw's temporary target. |

Both rectangles are 1280 x 720. Engine owner metadata for scene and temporary
textures reports 2560 x 1440; these dimensions are not GPU GetDesc proof.
Both primary UV sets cover 0..0.5, while the second draw's TEXCOORD1 covers
0..1. Its extra COLOR element is packed RGBA8, not another float.

Together with the shader instructions, the prepared bindings establish the
CPU-side path `scene -> temporary copy -> radial-blur scene write`. The first
draw is dependent on scene color; it is not unrelated auxiliary work that can
be ignored while expanding the final draw.

The second shader also reads PS t1 (engine extent 1 x 1, reflected name
`tOcclusionMap`) and PS t2 (128 x 128 with an extent flag, `tFilterTempMap1`).
The latter resource differs between the two captures. Its coordinate domain
must remain distinct from scene color. Owner GPU-format fields are UINT_MAX
in these captures; the attachment format observations above come from the
separate attachment backend. Neither substitutes for actual GPU verification.

## Shader findings and scaling implications

Four distinct VS/PS blobs were exported, structurally validated and
disassembled privately with the Windows compiler runtime.

- First VS: 644 bytes, SHA-256
  `730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd`.
  Passes position and one UV set through.
- First PS: 1080 bytes, SHA-256
  `be6127ba48008790fd0e93558b8248cb8acd7c0577899bf50d28e5618ae37b79`.
  The same shader identity already used by the final-screen copy: samples
  t0 using the supplied UV and CBSystem source mip.
- Second VS: 764 bytes, SHA-256
  `b980e376575d82610809066f45c6ecd6efb090bbee76968d9ee5203d2459a1d2`.
  Passes position and two independent UV sets; its COLOR input is unused.
- Second PS: 6444 bytes, SHA-256
  `90f3f4d5594357e5350acf3d5bda57db4641b50e6aaa5787d3ae341463860741`.
  Reflection identifies CBRadialBlurFilter and CBRadialBlurFunction. The
  instructions use SV_POSITION with CBScreen inverse size, primary scene UVs,
  CBFilter2 UV limits, a dynamic radial sample loop, and the separate auxiliary
  UV set. Reflected default constant values are not captured live constants.

The current planner rejects the first draw at its scene-only attachment
contract. Beyond that immediate rejection, the second VS differs from the
current overlay identity and the PS uses screen-position calculations. Merely
allowing format 24, skipping the copy, or expanding only the final viewport
would not establish correct sampling.

Next: build and verify an offline dependency-aware route for this captured
two-draw structure. Track every draw's reads and writes in native order; give
scene-derived temporaries an explicit extent/UV contract; keep auxiliary
inputs in their native domains. Exercise copy coverage, both UV sets,
screen-position normalization and color clamps in controlled GPU fixtures.
Before live admission, qualify actual GPU resources, states, descriptors,
constants and lifetimes. Use structural resource relationships rather than
wingdrake, monster or scene-name exceptions. Do not assume arbitrary shaders
can be scaled solely because they form a dependency graph.

Gallery intermediate-color connection, other combat-effect interruptions and
the hunter silhouette coordinate issue remain unresolved; this capture does
not prove they share the radial-blur path.

## Recovery and shutdown

- Main-menu and save baselines passed; 3,789 total DLSS evaluations,
  991 unfiltered attempts, zero runtime faults.
- Fifteen suspension periods and fifteen resumptions; not waiting at stop.
- All 4,780 GPU recordings completed and released; 5,232 jitter changes
  restored; full render scale restored.
- Live read-only verification found all 40 full native function spans and
  all 18 hook heads restored before exit.
- User reported normal scene and normal exit. Current DLL retained; closure
  receipt verifies process exit, unchanged DLL/sidecar and four configurations.

Private evidence: `local/overlay-dependencies-20261001`, including frozen
source, checkpoints, `dependencies-extracted/capture.json`, four shader blobs
and disassemblies, `dependency-analysis.json`, full-run review, collector
timeline, closure receipt and final archive manifest. Original prelaunch
files are preserved. Shader blobs/disassembly are not distributable source.
