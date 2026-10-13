# Occluded-hunter silhouette: native route investigation

2026-10-02. The user selected the undersized hunter silhouette as the next fix
after the broader DLSS run. The focused run captured a native hidden-surface
character-material path after grading. **The coordinate mismatch is observed
in paired CPU recipes. A shader correction passes owned offline GPU tests and
its first live test: the user reports that the silhouette now matches the
hunter. No corrected live pixel readback or matched full-resolution control
has been captured.**
The previously mapped `uOutlineFilter` route was not active in this reproduction.

## Pieces left at render resolution: v0.16.1 and v0.16.2 (2026-10-12)

**Report (game-57, v0.16.0).** With the hunter behind a tree, sword and shield
drawn, most of the see-through hunter matched the output. Some pieces were
still blocky:
- a shield-shaped piece;
- the speckle along the weapon arm.

The counters showed 118,551 of 474,700 late draws left native (25%).

**Diagnostics (v0.16.1, game-58).** The summary now samples every native
draw with its shader hashes and the failed check's values. The techniques
are named from the census. Confirmed:

| Rejection | Draws | Techniques | What failed |
|---|---|---|---|
| 8, buffers | 25,755 + 25,973 + 141 | `Uber_Mt_General`, `PL_Mt_AlphaTest`, `Uber_Mt_AlphaDither`, all **MeshSkin1Weight** (equipment attached to one bone) | check 15: the index buffer's resource facts. Vertex buffer, R16 indices, triangle list and stride 32 all passed. |
| 5, state | 6,542, many recipes | skin, hair, face, `PL_Mt`, `Uber_Mt` (Skin4/8Weight) | state unknown: no barrier for the colour target seen in that command list |
| 2, blend | 52,472 | `PL_Eye_Mt` | colour write mask 0, depth and stencil off: the draw writes nothing |
| 0, no recipe | 2 × 26,236 | `Simple_Mt_AlphaTest … RenderTransparent` (PS `9c5c0c85…`) | not a hidden-surface shader. It is a full lit transparent material (lights, fog, shadows, light probes), drawn once a frame per program in the same pass. |

**Fix (v0.16.2; the attribution is a hypothesis until a run confirms it).**
1. **Buffers.** Each engine buffer must now lie inside its D3D12 resource,
   rather than start at the resource's base address. The exact match was
   stricter than safety needs:
   - the pinned resource still holds every byte the draw reads;
   - the bound views are still checked exactly against the engine range.
   - If the failure is something else, the sample now records the resource's
     facts: valid, dimension, width, address minus base, and bytes.
2. **State.** An unknown state is accepted. The replacement and the native
   draw are predicated against each other on the same render-target view, so
   they need the same state as the native draw. A known state other than
   RENDER_TARGET is still rejected.
3. **Unchanged.**
   - The eye draws stay native; they write nothing.

**game-59 (v0.16.2).** The user named the remaining blocky piece: the shield.
It was drawn at half size and half position near the top left, the original
symptom of a draw left at the 1280 x 720 viewport.
- **Confirmed: the state fix works.** Rejection 5 fell to 0.
- **Confirmed: the shield is the rest of rejection 8.** That is the
  `Uber_Mt_General` and `PL_Mt_AlphaTest` MeshSkin1Weight draws, about 23k.
  Its index range's engine object is not an ID3D12Resource at all (valid 0,
  dimension 0, width 0; 41,916 bytes). The sword passed.
- **v0.16.3** accepts that index buffer without a resource. It requires the
  bound index view to match the engine range exactly, and does not pin it.
  This is safe because the replacement is recorded right after the native
  draw in the same list and reads the same view, and the predicate executes
  exactly one of them. The game's own lifetime guarantee covers it.

**game-60 (v0.16.3): confirmed by the user ("looks fixed").**
- The shield sits on the hunter.
- Rejection 8 is 0.
- Only two kinds of draw stay native, 11,670 each (one a frame):
  - the eye draws, which write nothing;
  - `Simple_Mt_AlphaTest`, with no visible artefact reported.
- DLSS ran without faults, and there was no crash.

**Simple_Mt_AlphaTest: it writes nothing (2026-10-13, v0.24.1).**

Checked offline in the vanilla RenderDoc capture (`local/renderdoc-20261006`,
2560x1440, one gameplay frame) with `tools/rdc/shader_draws.py`. The script
finds every draw whose pixel shader hashes to `9c5c0c85…` and records its
state and the pixels it changes.

- **Two draws:** EIDs 32709 and 32743, `DrawIndexedInstanced` with 1,416 and
  1,758 indices, one instance. The mod sees the same two programs, once a
  frame each.
- **Output:** one target, the 2560x1440 R11G11B10 scene, with **write mask 0**.
  Depth test and write are off, no depth target is bound, and stencil is off.
- **Pixels changed: 0** for both.
- **The program:** it declares SV_TARGET0 (rgba), SV_TARGET1 (x) and
  SV_TARGET2 (xy), applies `bAlphaTestEnable` with `discard_nz`, and scales
  by `fGlobalTransparency`. It declares no UAV and no atomic, so with write
  mask 0 and no depth target it has no effect. The engine draws it anyway.

**Decision.** It stays native permanently: there is nothing to correct.
v0.24.1 classifies it properly instead of as an unknown program.
- `silhouette_silent` reads the engine's own pipeline state (blend and
  depth-stencil descriptions). A late draw that writes no colour (every
  write mask 0), no depth and no stencil is classified before any shader
  lookup.
- It stays native with **rejection 11**, is not cached as a recipe and is not
  reported as an unknown program.
- The eye draws (former rejection 2: write mask 0, depth and stencil off)
  fall in the same class.
- **Confirmed in game (game-99, v0.24.1, Quality, training grounds; the
  user: "silhouette looked correct"):**
  - 228,001 late draws: 190,737 corrected and 37,264 native, all rejection 11
    (about four a frame there).
  - Rejections 0 and 2 at 0, `unknown_programs` 0, `recipe_failed` 0, route
    fault 0, DLSS fault 0.
  - Late draws exist only outside the hubs. Every minute in the hub counts 0,
    as in game-61/62, so hub-only runs since v0.18 (all 0) were expected. A
    DLAA run skips the scan (identity plan), as game-98 did.
- **If a program from this class ever writes:** the engine state check is
  false, so it reappears as an unknown program (rejection 0).

The late-candidates fixture (`scaled_stream_route_fixture … late-candidates`)
checks both cases: a packet whose engine pipeline writes nothing is
classified, and one with a colour write is not.

**Four vertex streams (game-100, v0.24.3).** The perf11 run in the training
grounds, with the user changing the area's objects, left 75,740 silhouette
draws native with rejection 8 (buffers, failed check 2: `vertex_mask` 15, not
1). They were absent in game-99.

- **What they are:** shader census lookups of the rejected pairs give hunter
  materials in `RenderTransparent`: PL_Skin, PL_Hair, PL_EditFace,
  PL_Mt_AlphaTest and Uber_Mt. Their vertex shaders are the **AlphaDither**
  variants (MeshSkin1Weight and MeshSkin4Weight) with four vertex streams.
  That is the hunter's silhouette in the dithered state, near the carts and
  barrels of the second and third object layouts.
- **Where they happened:** 69-99 s and 189-219 s of the recording. No ghost
  is visible at the video's scale. The pieces are dithered and partial, but
  they stayed at the render viewport.
- **The engine's input assembly** (IDA, clean image: `sub_14259E470`, called
  from the indexed execution `0x259d740` with packet +0x18) holds up to four
  streams:
  - owners at +0x00, +0x08, +0x10 and +0x18;
  - strides as bytes at +0x20 to +0x23;
  - offsets at +0x24, +0x28, +0x2c and +0x30;
  - the index owner at +0x38 and its offset at +0x40.

  It calls `IASetVertexBuffers(0, last + 1, views)`, with a zero view for a
  null owner in between.
- **v0.24.3:** `silhouette_buffers` admits one to four streams.
  - The bound mask must be exactly the engine's streams.
  - Each owned stream gets stream 0's checks: the resource contains the
    engine range, and the view matches location, size and stride. Extra
    streams may have strides of 4 to 128.
  - A null stream must have a zero view.
  - Every stream's resource is pinned.
  - Failure codes: stream 0 6-12 as before, streams 1-3 20, 30 and 40 plus
    0-7, the index buffer 14-18.
  - The late-candidates fixture checks one and four streams, a wrong mask,
    stride and location mismatches, and a null stream with a zero or non-zero
    view, on real WARP buffers.

The list of rejections and fixes continues:
   - `Simple_Mt_AlphaTest` stays native: it writes nothing (above, v0.24.1).

## All hidden-surface variants: v0.13.10 (2026-10-08)

**Report (game-33, v0.12.2).** With the hunter partly behind a tree in the
training area, a second see-through hunter appeared at half size and half
position: a late hidden-surface draw still at the 1280 x 720 render viewport
in the 2560 x 1440 output.

**Cause (offline, confirmed for the shader set; the in-game attribution is a
hypothesis until the v0.13.10 run).** The v1.25.44 correction admitted five
pixel shaders by SHA-256 (skin, hair, main, one Uber material, eye). The shader
census (`local/shader-census-20261008`, all 10,425 programs) has **97 pixel
shaders** whose reflection marks
`CBMhMaterialGlobal.fMaterialHiddenSurfaceDrawIntensity` as used. They are
player and Uber material variants:
- Skin, Hair, Eye, EditFace and generic `PL_Mt`;
- `Uber_Mt` and `Uber_Ice_Mt`;
- each in General, AlphaTest, AlphaDither, AlphaDitherAlphaTest, Water and
  Tool forms.

Any draw with one of the 92 others stayed native and produced the ghost. The
admission also fixed the binding registers (material b5, mask t18, depth t0),
but the variants differ:
- mask `tGBuffer1` in t1, t9, t10, t11, t12, t17, t18, t19 or t20;
- material at b5 or b6;
- 62 of the 97 sample `tVolumeBlendDepthMap` at the position times
  `CBScreen.fScreenInverseSize`; all 97 load `tGBuffer1` at `ftou(position)`.

Engine binding IDs are stable across all 97 (census `ps_words` against
reflection):

| Binding | ID |
|---|---|
| CBScreen | 12 |
| CBMhMaterialGlobal | 433 |
| tGBuffer1 | 41 |
| tVolumeBlendDepthMap | 159 |
| tWaterReflectionDepth | 151 |

In the skin shader, every screen-space position use feeds a render-resolution
buffer: the mask load, the depth sample, and a 16 x 16 light-tile index from
`ftou(position) - CBScreen.iViewOffset`. Position z is used for depth
comparisons. Mapping all x/y reads to the render lattice and keeping z is
therefore the complete correction.

**Fix.**
- **Adapter.** `silhouette_shader.cpp` is now the screen-shader generator's
  rule 1 (`make_screen_shader`, no scene register). It replaces the hand
  operand table. Two generator fixes:
  - z/w are copied only as declared (all 97 declare `v0.xyz`);
  - the copy uses the correct `.zwzw` swizzle;
  - a neg/abs modifier on a position read stays attached (`-v0.z`).

  The seven admitted screen-filter hashes declare `v0.xy` with no modifier
  reads, so their generated bytes are unchanged.
- **Allowlist.** `silhouette_allowlist.inc`, generated by
  `tools/gen_silhouette_allowlist.py` from the census candidates that pass
  `silhouette_shader_fixture --candidates`. **All 97 passed:** 15,197,184 pixel
  checks, WARP with GPU validation, 8 parallel runs, about 50 minutes. The
  results are in `local/silhouette-allowlist-20261008`. An allowlist-mode run
  (one new identity, one v1.25.44 identity, corrupted-copy rejection) passed
  as well.
- **Fixture.**
  - Constant fields and textures are bound by reflected name.
  - Every shader gets the full 2x oracle: two occluder patterns at z 0.1 and
    0.9, the native render-size pixel per 2x2 output block, the
    wrong-coordinate control and the selector branch.
  - The five v1.25.44 identities pass through the new path: 783,360 pixel
    checks, WARP with GPU validation.
- **Admission (`silhouette_route.inl`).**
  - Bindings are found by engine ID in any register. Screen, material and
    mask are required; depth is checked when bound.
  - Cull mode and winding are free (the private PSO copies the native
    rasterizer state).
  - Vertex stride may be any multiple of 4 from 12 to 128.
  - CPU and PSO caches hold 64 entries.
  - A negative cache keeps unknown programs from being copied and hashed on
    every draw.
- **Diagnostics.** The alpha `dlss_summary` reports `route.silhouette`:
  - recorded, native and corrected counts;
  - rejections per reason;
  - unknown draws and programs, cache-full count, and the last unknown PS
    SHA-256.

## First live correction result: v1.25.44

The user reproduced the hidden hunter and confirmed matching size/position.
The run completed 1,565 DLSS evaluations with four unfiltered resolves and zero
runtime faults. It recorded 7,075 late indexed nodes: 4,909 conditional corrected
draws with 4,909 state restorations, and 2,166 native draws. Eight private PSOs
were created. The corrected count measures the recorded alternative, not a
per-draw readback of the GPU selector. User confirmation establishes this pose's
visual result; it does not replace a matched native control or broader testing.

Native rejection receipts comprise 1,132 unknown/unadapted recipes, 566 blend/
depth/raster rejections (including zero-write eye draws), 193 unavailable local
color states, and 275 buffer/layout/bytecode stability checks. Those draws stayed
native. The state and buffer rejections remain follow-up questions; this short
test does not establish complete correction coverage across scenes.

The unchanged frozen reviewers accepted the full native log and all 38 external
timeline snapshots. Shutdown completed all 1,569 GPU signals, released all 1,568
tracked recordings, restored all 1,700 jitter changes and full render scale, and
disabled the hooks. A separate live read verified 41 complete native function
spans and 19 hook heads. All 715 prelaunch files and four configuration identities
remained unchanged. The user confirmed a normal scene and normal game exit.

The user also reported severe slowdown ("seconds per frame"). Its cause is not
established: no Present timing or CPU/GPU profile was captured. Preserve the
existing diagnostic checks and defer optimization as requested; do not attribute
this result to logging alone or extrapolate production performance from it.

Private evidence is sealed in `local/hunter-silhouette-output-20261002`: 750 files,
28,392,892 bytes, archive SHA-256
`a115e7ae162d367cf6e5c8fef388a3750a1275022989946e5fe7915c9b7bcc5a`.
It includes `silhouette-correction-review-48764.json`, both full reviews, the
timed collector, and `stop-native-code-48764.json`. The v1.25.44 DLL remains
installed (SHA-256
`5c8e6949841132529b6bea6fe56e74ef4431b38db713ed712bea7dc87d6a7bd5`).
After the game closed, the output sidecar was moved to the fresh
`local/hunter-silhouette-followup-20261002` folder, with its exact previous bytes
preserved there. The DLL was not replaced and no new test was armed. Future
startup logs therefore cannot append into the sealed archive.

## Indexed silhouette correction: v1.25.44 implementation

The late indexed path now records a conditional native draw and a private PSO
with the position-coordinate adapter. Only the reconstructed/spatial frame
choice executes the output-sized silhouette; the native choice executes the
original draw. Vertex/index buffers, topology, root arguments, descriptor heaps,
render targets and texture states stay bound. The PSO, viewport, scissor and
predicate are restored before returning to the native wrapper.

Admission requires a frozen packet and native pipeline recipe, a recognized PS,
no additional geometry/tessellation stages, the hidden-surface material branch,
matching allocation/view constants, the actual single grading RTV, an observed
local RENDER_TARGET state, native alpha blend/depth/raster state, and actual
vertex/index ranges. Eye draws with no color writes stay native. Unknown recipes
stay native. The route reuses the original texture descriptors and CB bindings:
CPU constants and texture descriptions are checked, but descriptor contents,
GPU CB bytes and inherited texture read states are not claimed as measured.

All late indexed candidates enter the frame handoff between grading and final
composition in native stable key/insertion order, including unadapted draws.
The bound is 32 late packets and 64 total handoff nodes. Immutable CPU and PSO
caches each have 32 entries and never evict live variants. Native/private objects
and the frame selector are pinned by recording generation until both successful
Reset and GPU completion. Packet/recording/order/restore failures stop admission
and preserve unproved references. No new native hook or code signature was added
beyond the v1.25.43 indexed wrapper.

Private validation and deployment evidence belongs in the fresh ignored folder
`local/hunter-silhouette-output-20261002`. Both previous silhouette archives are
sealed. The first live result above supplements the preparation evidence; owned
offline pixel tests and user-confirmed live alignment remain distinct evidence.

## Focused run result

The retained v1.25.42 build ran for about 25 minutes, completing 54,325 DLSS
evaluations with three early skips and zero runtime faults. The full native log
and all 298 external timeline snapshots passed their frozen semantic reviewer.
Shutdown completed all 54,328 GPU signals, restored all 54,911 jitter changes and
full render scale, and disabled the hooks. A separate live read verified 40
complete native function spans and 18 hook heads before the user closed the game.
The DLL and all four configuration identities remain unchanged.

Private evidence is in `local/hunter-silhouette-20261002`, including
`full-native-review.json`, `full-timeline-review-r1.json`,
`hidden-surface-comparison.json` and `stop-native-code-37748.json`. The first
timeline orchestration attempt referenced a nonexistent deployment key and did
not run semantic review; its error is preserved, and the corrected review passed.
No new render hooks, process suspension or external process-memory writes were
used for this investigation.

## Captured hidden-surface material route

The queue contains indexed character draws at phase 15 keys `0x7ffffff4` through
`0x7ffffffd`, after the grading endpoint `0x7ffffff1` and before final composition
at `0x80000000`. Their target matches the grading output resource, but the
native attachment rectangle remains `[0, 0, 1280, 720]` during 2560 x 1440 output.
The previous stream route adapted selected quad/filter draws; v1.25.44 adds
these late indexed character draws to the conditional output handoff.

Skin, hair, eye and main character pixel shaders contain an explicit
`CBMhMaterialGlobal` hidden-surface branch. It is selected by
`bMaterialCustomLighting` and uses `iMaterialCustomLightingGBufferIdMask`, a
depth texture, and `fMaterialHiddenSurfaceDrawIntensity` to produce the white
hidden-surface color and its opacity. This gives a concrete route independent
of the inactive `OutlineFinal` hypothesis.

Three paired recipes were accepted with the silhouette absent and three after
the user reproduced it. The selected skin draw had opcode 1, key `0x7ffffff4`,
technique 14618, and pixel-shader SHA-256
`be174b5f4d68ae60081aa5ffb18e3ab244a481aeda176dadf44a2fddf280072d`.
Both controls bind native constant ID 433 at pixel register b5, with
custom-lighting flag 1, G-buffer ID mask 7680 and hidden-surface intensity 0.1.
Its reflected data ends at byte 20; later bytes in the aligned capture are
padding and were not interpreted.

The same draw is enqueued in both controls. Its per-pixel mask/depth tests
determine visibility, so equal draw counts or equal branch constants do not
contradict the user seeing the silhouette disappear.

The pixel shader loads its G-buffer ID mask at t18 using integer pixel-position
coordinates. It samples depth at t0 using pixel position multiplied by the
screen inverse dimensions. The paired CBScreen copies contain allocation extent
2560 x 1440, view extent 1280 x 720, screen inverse `1/2560, 1/1440`, and view
inverse `1/1280, 1/720`. Native metadata reports both sampled texture allocations
as 2560 x 1440. Those are CPU owner descriptions, not verified GPU descriptors
or a proof of their written pixel regions.

This evidence strongly supports a late draw using render-space raster coverage
on an output-space color target. Simply enlarging its viewport would also change
the integer mask loads and normalized depth samples; it would not establish a
correct full-size occlusion calculation.

## Correction contract and remaining verification

Keep the game's occlusion calculation in its native render coordinates, while
mapping the resulting silhouette coverage to the output extent. The implemented
offline candidate enlarges raster coverage and remaps pixel-shader position x/y
back into render coordinates before the existing integer mask reads and depth
samples. Position z and source modifiers remain unchanged. This avoids adding a
separate layer/composition pass. The earlier private-layer proposal is superseded
for these five verified native shader identities.

The captured native color blend is source-alpha / inverse-source-alpha, with
depth testing disabled. The correction must retain the original blending and
order. The direct shader route must qualify actual indexed API arguments,
vertex/index bindings, root and descriptor state, target/sample formats and
states, command-list generations, queue order, resource lifetime and restoration.
Unexpected late draws must retain their original order and rendering.

Attempts to capture a matching full-resolution recipe after stopping yielded
no accepted samples: the active reconstruction view guard was unavailable, and
the separate native-only reader did not observe a prepared matching queue.
Preserve this missing comparison. Do not claim an unscaled control or a verified
GPU silhouette mask from the accepted CPU recipes. The subsequent v1.25.43
binding run established the execution contract, and v1.25.44 connected the
correction. A matched native full-resolution pixel comparison remains missing.

## Offline correction and v1.25.43 binding recorder

The runtime adapter accepts five whole-shader SHA-256 identities (skin, hair,
main character, Uber material and eye). Each has eight verified reads from
the pixel-position input. It changes those source operands to a fresh temporary,
injects an x/y render/output conversion, preserves z and extended modifiers,
and rebuilds the DXBC checksum. Native shader bytes remain private and are
not embedded in distributable code. Unknown or changed shaders are rejected.

An owned D3D12 WARP fixture with debug and GPU validation passed for all five
captured programs: 129,024 pixel checks and 13 rejection/negative checks. At 1x,
native and adapted outputs agree. A 64 x 36 to 128 x 72 test verifies mask/depth
coverage and a separate depth case verifies the preserved z calculation. An
unadapted enlarged viewport fails the pixel oracle, so viewport expansion alone
is insufficient. These are synthetic resources and geometry, not a successful
full-resolution game control or live corrected-pixel proof.

Build v1.25.43 installs a **read-only indexed draw recorder** alongside existing
DLSS. It does not apply the shader correction, alter indexed draw arguments, or
submit extra GPU work. It records at most 16 samples, two per known shader set,
with 4,096 candidate visits. Candidate packets must belong to a completed native
queue plan and execute only once in that frame. Unknown shader identities and
ambiguous/stale candidates are skipped.

The new native execution hook is RVA `0x259d740`, Windows x64: RCX executor,
RDX backend context, R8 indexed packet; no stack arguments. The full 214-byte
function SHA-256 is
`82e2ac66bd7cfa4ce7a61244efaaa6f7e3db0bbb766606962bb23fcb91daafba`.
It forwards the original call and preserves native return registers and nested
diagnostic scope. The actual `DrawIndexedInstanced` call is observed at command
list slot 13, native caller RVA `0x259d806`; its count/start/base and instance
arguments must match the packet. `IASetIndexBuffer` slot 43 extends the optional
API observer; four vertex streams, reset generation, roots, heaps, pipeline,
viewport, scissor and attachment cache are recorded at the draw.

Packets, native binding owners and CPU constants are engine-owned borrowed
objects read only within execution. Resource descriptions use temporary COM
references from verified system methods and require the configured D3D12 device;
those references are released before native forwarding returns. A CPU constant
recipe or resource description still does not establish the contents/lifetime
of its uploaded GPU descriptors. Unknown texture states remain explicitly
unknown. These records can guide qualification, never authorize replacement.

Owned tests passed for multi-stream/index observation, null/reset invalidation,
legacy predicate observation, native register forwarding, fault shutdown and
96 frames across reordered command lists. This added recorder has not yet been
tested inside the game. New hooks must be included in live cleanup verification.
The retail/decoded executable hashes below gate this build; neither image is
modified on disk. Private output is `local/hunter-silhouette-fix-20261002`.

### Live v1.25.43 result

PID 34140 / creation 134353853807668522 passed startup and the focused run.
The recorder saved 16 samples across all five shader variants, with zero
diagnostic errors or rejected native/API calls. All calls returned once, and
their attachment identities remained stable. Both the complete native log and
43 external timeline snapshots passed the frozen semantic reviewers.

Actual device-qualified resource descriptions show a 2560 x 1440
R11G11B10_FLOAT color target, R32_FLOAT depth input and R8G8B8A8_TYPELESS mask
allocation. The mask's allocation format does not establish its SRV format.
The color target's recording-local state is RENDER_TARGET. Native and observed
API viewport/scissor are both 1280 x 720; roots, heaps and pipeline are observed
at the indexed API. These sampled draws use one vertex stream with strides
32, 36 or 40, triangle-list topology and R16_UINT indices. All observed vertex
and index ranges fit their actual buffer allocations.

Two eye draws have a zero color write mask, so a known shader hash alone must
not imply visible silhouette output. All sampled material recipes have custom
lighting 1, mask 7680 and hidden-surface intensity 0.1. Wet roughness differs
between two recipes (0 versus 0.5); an initially overstrict whole-material
comparison was corrected in the separate diagnostic report. No renderer
admission or frozen semantic reviewer was weakened.

The recorder filled during the first two sampled gameplay frames. The user's
visible-silhouette report came afterward; these samples establish execution
bindings, not an isolated pixel comparison of hidden and visible controls.
Uploaded GPU descriptor/constant contents and input texture states remain
unverified. No corrected game pixels or full-resolution game control are claimed.

The run stopped intentionally after 6,498 DLSS frames with zero runtime faults.
All 6,502 GPU signals completed, all 7,020 jitter changes and full render scale
were restored, and all hooks disabled. A separate live read verified 41 complete
native function spans and 19 hook heads. All 711 prelaunch files remained
unchanged. Evidence includes `indexed-contract-review-34140.json`,
`full-native-review-34140.json`, `full-timeline-review-34140.json` and
`stop-native-code-34140.json`. The user confirmed a normal scene and normal game exit. The v1.25.43 DLL remains
installed, with no silhouette correction connected. The 743-file private archive
was sealed and verified with SHA-256
`57121b4290c1a2018bc6c4dc9616a8dcf42354c2a4de62cb844a5c0809e051bd`.

## Visual evidence

The two screenshots retained in `local/dlss-long-run-20261001` show a small white
hunter silhouette toward the upper-left while the HUD fills the view. The run
rendered at 1280 x 720 with 2560 x 1440 output. A render/output coordinate mismatch
is plausible, but the screenshots do not establish the responsible shader or
its position relative to temporal reconstruction. The broad run's radial and
DOF rejections remain separate evidence, not silhouette attribution.

## Earlier offline landmarks: inactive OutlineFinal lead

The pinned image registers the native type `uOutlineFilter`, with a 0x270-byte
unit, type record RVA `0x51c54d0` and unit vtable RVA `0x3512208`. The constructor
assigns phase 14 / order 98. Execute slot +0x60 leads to RVA `0x22ed1f0`.
This route was outside the earlier catalogue of direct callers to the two
post-filter registration helpers; that catalogue was deliberately scoped.

Execute prepares outline-object rendering, an `OutlineFilter` draw reading
object/depth masks, optional downsample/combine draws and an `OutlineFinal` draw.
The allocation routine names `OutlineObjectMap`, `OutlineObjectDepthMap`,
`OutlineTemp`, `OutlineDownSample%d` and `OutlineBlurTemp%d`. Allocation dimensions
derive from renderer dimensions and a separate renderer scale at +0x204.

The final preparation helper at `0x22eb650` selects `OutlineFinal`, binds
`tBaseMap` and `CBScreen`, copies a supplied rectangle into its native raster
setup, and builds a four-vertex strip. Its UV endpoints use renderer +0x1f4.
Execute supplies a target associated with the current view and the rectangle it
previously retained from the render context. This is a concrete independent
coverage/sampling path, but its live target, active rectangle and valid UV region
must be measured before deciding which domain to change.

Relevant complete function identities in the decoded image:

| Role | RVA | Bytes | SHA-256 |
| --- | --- | --- | --- |
| Outline Execute, including chained unwind fragments | `0x22ed1f0` | 2677 | `cdef09b144032c6449e0c6a10d19820d09c99f02616d6f92449fefc0aedf5924` |
| Final preparation | `0x22eb650` | 704 | `3e1fcdb67d517c3ba0ba712598208c3db8d6ff89f3d1f1fe136c823bf3739fd8` |
| Resource allocation | `0x22ecc20` | 888 | `71ac188b4b46a5241ea793de348638f988a05b1bcc9344018e5e184021c1c416` |

These are analysis landmarks, not new hooks. The final helper uses the Windows
x64 convention: unit in RCX, target view in RDX, render context in R8, source
texture owner in R9 and rectangle pointer as the fifth argument. It copies the
rectangle during the call and enqueues the native draw. Any future integration
must independently qualify resource identity, state, recording order and GPU
lifetime; a type or technique match alone cannot grant admission.

Retail executable version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both hashes were verified before deriving these landmarks. Private disassembly
and scripts remain in ignored `local/hunter-silhouette-20261002`.

## Original focused observation setup

Keep the tested v1.25.42 DLL and native AA/DLSS Off. The output sidecar now points
to a fresh evidence directory; the prior 929-file broad-run archive was verified
without altering it. Exact DLL and sidecar backups, SDK path, frozen build source
and four unchanged configuration identities are retained there.

A bounded external reader selects candidate phase-14 quad packets using the
independently mapped native technique IDs, then copies native geometry, viewport
rectangle, shaders, CPU binding metadata and known CBScreen constants twice.
It rejects changed frames/queues/recipes and has read/time/size bounds. It never
writes or suspends the process. These are CPU observations, **not GPU state or
descriptor proof**, and cannot authorize replay.

The first targeted reader accepted no OutlineFinal recipes. All four mapped
technique caches remained uninitialized during the visible-silhouette test.
Bounded indexed-draw readers then captured the hidden-surface path described
above. Exploratory queue inventories were not atomic frame snapshots and do not
authorize rendering. The paired skin recipes provide the stronger CPU evidence.
Do not globally scale HUD geometry, all auxiliary textures or every overlay.

The prior broad run's two raw state-review discrepancies and recovered
dependency/DOF interruptions remain queued in
[its result](dlss-broad-run-result.md); this focused observation does not close
them or weaken their review checks.
