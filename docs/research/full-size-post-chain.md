# A full-size post-processing chain (research, 2026-10-13)

The user asked whether the post-processing could be fixed wholesale instead of
effect by effect.

**Today's route:** DLSS writes a full-size image at the TAA copy. Every later
effect is then qualified one by one, and adapted to the new size. An unknown
effect, or an unknown configuration of a known one, refuses the frame: the
game's own image is shown and frame generation pauses (game-110: the equipment
box's depth of field and a cutscene blink).

**The alternative studied here:** after DLSS, make the engine itself run the
rest of the frame at full size, as it does at render scale 1.0 (DLAA). There,
every effect renders natively at full size with no adapters.

Static analysis only, on the clean database `local\ida-clean-20261011\mhw.i64`
(executable `c2ebbbd2…90ea`). Nothing here has been tested in game.

## Confirmed in the code

### How the post chain gets its size

- **The scene sets the rectangle.** `sub_1423C3790` (the view's scene draw,
  called from `sCamera::draw` 0x228e928) reads the context's view rectangle
  (cDraw +0x5c..+0x68). It multiplies that rectangle by the view's scale
  product (view +0x6b8) and sets the result with `setViewport`
  (`sub_1423D0330`). That is the active render rectangle.
  - The same function draws the final `System_Copy` upscale with the full
    rectangle. It then restores the scaled one.
- **`setViewport` (0x23d0330)** stores the rectangle (cDraw +0x5c..+0x68). It
  writes a new CBScreen with `fScreenOffset/Scale`, `iViewOffset/Size`,
  `fViewOffset/Size/InverseSize`, all derived from that rectangle and the
  render target size (cDraw +0x8c, set by `setRenderTarget`).
  - `setRenderTarget` writes `fScreenSize`, the content scales and
    `bCheckerboard`.
- **The post scheduler.** `sCamera::draw` then calls the scheduler
  (`sub_1424F1FC0`) with the same context. It sorts the registered
  descriptors by (stage, order) and runs two loops:
  - first, it assigns the ping-pong buffers and records the descriptors' copies
    (`sub_1424F1B20`: an opcode-13 copy of the context's current rectangle,
    e.g. the TAA → motion copy, key 0x78000002);
  - then it calls each unit's vtable +0x60 callback in sorted order, and
    records the last descriptor's copy to the view output.
- **Inheritance.** No callback resets the rectangle: each one inherits the
  context's current rectangle and CBScreen. Examples:
  - `uBlinkFilter`'s draw uses cDraw +0x5c..+0x68 directly as its viewport
    (`sub_1424C8C20`);
  - `uToneMapFilter` reads the same fields;
  - the copy helper copies exactly that rectangle.
- **Effects that read the scale directly.** Radial blur and god rays project a
  world point with `sub_1423CB520`. It uses the context rectangle, then
  divides by the view scale (view +0x6b8) to get display coordinates. With a
  full rectangle, that division must not happen. Nothing else in the 15 known
  filter callbacks (to depth 2) reads the view scale.
- **Packet order is key order.** Packets carry a sort key, stage << 27 | order,
  taken from cDraw +0x154 at record time. They execute in key order, not in
  CPU order. `sub_142585410(ctx, i)` sets the pass and order from a table at
  0x354cb70 (triples of pass, order, hash).

### Callback order differs from key order: tone mapping

The tone-map unit's descriptor sorts first (order 0), so its callback runs
before TAA's. It records in two parts:

- index 315 = (15, 0): exposure and luminance work, key 0x78000000, before TAA
  on the GPU. It must stay at render size.
- index 324 = (15, 0x7fffff1): the grade at the endpoint, key 0x7ffffff1, after
  every effect. It must be full size.

So the switch cannot be "every callback after TAA's": it has to follow the key.
The other stage-15 users of that table:

- FXAA (0x7fffff3) and `uTemporalAAFilter3` (0x7fffffe), neither active with
  the game's AA off;
- the scene function's own (15, 0x7ffffff).

### Low-resolution inputs that post shaders read

From the shader census (737 post-family PS/CS), the textures written during the
scene at render size:

| Texture | Readers after TAA |
| --- | --- |
| `tDepthMap` | DoF (all variants), DepthColor, DistanceFog, ColorCorrectCubeSophia/Table, CombinedFilter, auto-exposure VR blur |
| `tGBuffer4` (motion) | MotionBlur (all variants), CombinedFilter |
| `tGBuffer5` (fur/extra motion) | MotionBlur |
| `tOcclusionMap` | ColorCorrect, GodRays, RadialBlur |
| `tPrimDepthMap` | RadialBlur |

Everything else they read is produced inside the chain, so it follows the
rectangle: tile/neighbour max, DoF intermediates, bloom, the temp maps.
Exceptions are tables and LUTs, which are size-independent.

## Design: "full-size post"

**On the CPU, on the render thread, per view:**

1. At the scheduler's entry (hook `sub_1424F1FC0`), record the render
   rectangle and arm the switch for this context.
2. When TAA's callback returns (hook its Execute, 0x2392150), call
   `setViewport(full rectangle)`. Every later callback inherits the full
   rectangle and a full-size CBScreen, as at DLAA.
3. For tone mapping, which runs before TAA, the change has to follow the key:
   - when `sub_142585410` is called with the endpoint index (324) inside the
     tone-map callback, switch to the full rectangle;
   - when that callback returns, restore the render rectangle, so the
     pre-TAA circular depth of field at order 1 stays at render size.
4. In the projection helper `sub_1423CB520`, skip the division by the view
   scale while the switch is on. This covers radial blur and god rays.
5. At the scheduler's exit, restore the render rectangle and disarm. The UI and
   later views are unaffected.

**On the GPU:** in the same recording, after DLSS (at the TAA copy, where the
route already records it), upscale the render-size inputs to the full
allocation, in place or into copies the bindings already point to:

- depth;
- `tGBuffer4` and `tGBuffer5` (motion, rescaled when it is in pixels);
- possibly `tOcclusionMap` and `tPrimDepthMap`.

**Route:** in this mode the plan needs only:

- the TAA copy (DLSS) and the final copy (the HUD-less anchor);
- the input upscales.

Every effect between them stays native. Unknown effects stop mattering, and
most adapters (filter, haze, radial, DoF, overlays, screen filters, possibly
silhouettes) become unnecessary.

## Open questions, before a prototype

1. **Exceptions to the rectangle model.** Does any post packet take its
   rectangle from somewhere other than the context? Compare, per key, every
   stage-15/16 packet's rectangle and target size at Quality and at DLAA. A
   diagnostic needs no rendering change.
2. **Copies in the first scheduler loop.** Which descriptors record copies
   there? They use the render rectangle, recorded before any callback. TAA's
   copy is DLSS's; any other after TAA would need the full rectangle.
3. **Formats and states of the auxiliary inputs.**
   - Is the depth buffer depth-stencil with a stencil plane that later passes
     read?
   - How is motion packed? The route's DLSS conversion already decodes it.
   - Where do the inputs live (output-size allocations, as the DoF textures
     do)?
4. **Late draws.** Silhouettes and other scene geometry drawn after TAA would
   need full-size depth and, possibly, stencil.
5. **Threading.** The switch is CPU state on the render thread's context, and
   the view scale is never written. Check that no other thread records into
   that context during the scheduler.
6. **Frame generation and Reflex.** Unaffected in principle: the final copy and
   the HUD-less capture stay as they are. Check depth and motion tags.

## Census result (game-113, v0.24.11)

The post census ran in the hub, the sauna (haze) and the field, at Quality,
DLAA and Performance: 19 samples, 14 written.

Hub, Quality (frame 25259) against DLAA (19259):

- Same packets: same keys, opcodes and targets (all output-size R11G11B10).
  The only differences are rectangles and the dispatches derived from them.
- Render rectangle at Quality, full at DLAA:
  - the grade (15, 0x7fffff1), motion (15, 100), haze (15, 20000) and the
    depth-of-field composition (15, 26005);
  - the tone map's early draw (15, 0);
  - the TAA dispatch (15, 2): 106x60 groups, against 160x90;
  - the TAA copy (15, 2).
- Already output-sized at Quality, and identical:
  - the six depth-of-field dispatches (8x1440, 128x5, 16x9, 80x45 x3);
  - the tone map's three exposure dispatches (80x45).
- The (15, 0x7ffffff) dispatch is `CSTiledDeferredCopyGBufferID`, recorded by
  the scene function, not the scheduler. It copies the GBuffer ID for next
  frame's TAA, belongs to the scene, and correctly stays at render size.
- Stage 16 (the final copy at order 0, the UI at 4000-11000) is full-size in
  both.
- The sauna pair (31259 against 40259) adds the haze descriptor (order 20000)
  with the same pattern.

**This confirms the model.** Every scheduler-recorded effect after TAA uses the
context's render rectangle at Quality. Nothing else differs. The switch needs no
per-effect knowledge beyond the tone map's two keys, the projection helper, and
the render-size inputs.

The census cap (224 packets) was hit on UI-heavy frames: the queue holds stage
16 before stage 15. A later census should keep only stage 15 and the final copy.

## Next step (done: census above)

A diagnostic build, with no rendering change, that logs the post census:

- every packet with a stage-15/16 key: key, opcode, rectangle, target size;
- the copies of the first scheduler loop;
- the formats and flags of the auxiliary inputs.

A Quality run and a DLAA run answer questions 1-3. Then the prototype goes
behind an ini key (`full_post = on`), with the census as its check.

## Prototype results

**game-114 (v0.25.0), hub, depth of field and motion blur off.**

- The mechanics held: 25,052 frames armed, switched and restored; identity
  plans built and resolved with DLSS; fault 0.
- The scene showed the top-left render-size corner of the full-size chain,
  stretched over the screen (a 1/scale zoom; the user's screenshots).
- **Cause:** the tone map's final grade (`ToneMappingApply_ColorGrading`)
  reuses the texture coordinates its exposure step computed from the
  rectangle at the start of the callback. The endpoint-key switch therefore
  gave a full viewport with render-size coordinates.
- 4,714 armed frames never ran TAA's callback (title and loading screens).

**v0.25.1:**

- The tone map's callback runs entirely at the full rectangle. Its exposure
  step (`AutoExposureApply*`, into TAA's input, before TAA) uses no CBScreen,
  and its coordinates are linear in the pixel position, so the render region
  is unchanged. The key-setter hook is gone.
- A frame arms only when the previous one ran TAA's callback.

**game-115 (v0.25.1), same test.** The user: "looks good to my eyes".

| Measure | Count |
| --- | --- |
| Frames armed (final copies) | 15,068 |
| Switched and restored | 15,064 |
| Not switched (transitions) | 4 |
| Not armed (no TAA in the previous frame) | 3,778 |
| DLSS frames | 15,066 (fault 0, 2 single-frame skips) |
| Frame generation tagged | 15,064 (0 exceptions) |

- Frame rate is as before (224-274).
- The HUD-less image skipped 150 frames (no_restore: no pipeline or graphics
  root bound when the final copy opens a command list). This is the identity
  path's existing behaviour: game-98 at DLAA had 112.

## The render-size inputs (v0.25.2, v0.25.3)

The post chain reads three textures the scene writes at the render size,
into the top-left render region of output-size allocations:

| Input | Format | Lookup | Post readers |
| --- | --- | --- | --- |
| Scene depth | R32_FLOAT | view +0x628 -> +0xA0 (`sub_14255CE10`) | DoF, fog, depth colour, haze, god rays, motion blur, colour cubes; also `tPrimDepthMap` (radial occlusion) |
| GBuffer4 | R32_UINT | view +0x620 -> +0x78 | motion blur (two f16 halves in screen units, low bit a flag) |
| GBuffer5 | R8G8_SNORM (engine 40) | view +0x620 -> +0x80 | motion blur, where GBuffer4's flag is set (screen units) |

`tOcclusionMap` is derived from the depth inside the chain (a 1x1
`OcclusionFactor` pass) or is the depth itself. The tone map's grade reads no
depth. The scene clears the GBuffer and rewrites the depth every frame, and the
depth texture is not double-buffered, so overwriting all three after DLSS has
no effect on the next frame.

**The upscale** (`full_post_inputs.hpp`): each render region is copied into a
buffer, expanded by a compute pass (nearest texel under each output texel's
centre, bit-exact), and copied over the texture. None of the values is in
pixel units, so none is rescaled. Only root constants and root buffer views
are bound; the compute root signature, its arguments and the pipeline are
restored.

**Where, and in which states.** The game plans every barrier of a frame
before executing it (`sub_14229CEE0`): each texture has spans of uses with a
native state, converted to D3D12 by `0x259ba50` (bits: 4 NPSR, 8 PSR,
0x20 COPY_SOURCE; 0x100 RT, 0x400 UAV, 2048 COPY_DEST). Read states are bit
sets, and consecutive reads merge into one span. The copy executor
(`0x259c650`) flushes the packet's barriers just before its
`CopyTextureRegion`.

- The slot: after TAA's callback, in TAA's key, the scheduler's context
  records one engine copy at key (15, 3) (the order written as the scheduler
  writes it, `cDraw +0x154`, then restored), 1x1, into GBuffer5. The planner
  then has GBuffer5 in COPY_DEST at that packet. The copy hook replaces its
  copy with the upscale, or with nothing; it never runs.
- Its source is TAA's output, which the TAA copy already reads as COPY_SOURCE
  at (15, 2): the slot's read joins that span.
- **game-116 (v0.25.2):** with GBuffer4 as the source, its COPY_SOURCE read
  merged into TAA's read span; TAA's barrier then made it
  PSR|NPSR|COPY_SOURCE, and DLSS refused every armed frame (reason 6). The
  slot recorded nothing.
- The depth and GBuffer4 stay out of the packet. Their states are those TAA's
  consumer proved (the depth admission; GBuffer4's transition in TAA's
  barriers), published with the DLSS copies, provided no planner node of
  theirs lies after TAA's dispatch up to the slot. A new span on the depth
  after TAA could be hoisted before the consumer, whose admission accepts only
  reads.

**game-117 (v0.25.3), hub and field, depth of field and motion blur on.** The
user: blur looked correct, no skips, "everything looks much clear".

| Measure | Count |
| --- | --- |
| DLSS frames | 31,318 (skips 4, fault 0) |
| Frames switched to the full chain | 31,318 |
| Slots recorded and planned | 31,318 |
| Upscales | 30,880 (98.6%) |
| Without the upscale: states not yet published | 184 |
| Without the upscale: interval not proven quiet | 253 |
| Frame generation, HUD-less tagged | 31,080 (no_restore 235) |

The 437 frames without the upscale (1.4%) run the full-size chain on
render-size inputs for that frame. "Not yet published" is a worker reaching
the slot before the TAA consumer's worker; "not quiet" needs finer reasons.

## Plan states and the silhouettes (v0.25.4)

**The inputs' states come from the plan.** At the slot, each input's state is
the after-state of its last planner node up to the slot (its own included; a
child draw's nodes for an opcode-10 copy), or its tracker's initial state. A
transition the planner moved between TAA and the slot is applied, and the
slot no longer waits for TAA's worker. When TAA's consumer has already
published, the plan's states at TAA must equal what it proved.

**Silhouettes.** The hunter's late draws (keys 0x7ffffff4..fd, scene code)
read the scene depth as `tVolumeBlendDepthMap` (`sub_14255CE10`, bound by the
scene function) and GBuffer1 at the render size, and were drawn at the render
size into the full-size chain. In full-post plans:
- the existing silhouette route takes the late draws (target: the grade
  output, plan resource 2); the hand-off is TAA, its copy, the slot, the late
  draws, the final copy;
- the slot keeps the depth's render region in its own buffer (copies only
  when no binding snapshot is known), and before the first late draw in key
  order the region is restored over the depth in the state the plan gives
  there. The post chain before it reads the upscaled depth.

**game-118 (v0.25.4): training grounds (silhouette), hub, field and three
cutscenes, depth of field and motion blur on.** The user: the silhouette
looked correct, the cutscenes "all looked perfect".

| Measure | Count |
| --- | --- |
| DLSS frames | 41,672 (skips 4, fault 0) |
| Frames switched to the full chain | 41,665 (11 armed at a transition, not switched) |
| Upscales | 41,664 of 41,665 slots, no skips |
| Plan states checked against TAA's consumer | 41,501 equal, 0 mismatches |
| Plan states without the consumer yet (worker order) | 163 |
| Depth restores before the late draws | 6,314 (0 skipped, 0 stale) |
| Silhouettes corrected | 124,467 (22,521 silent draws stay native; 3 buffer refusals) |
| Frame generation, HUD-less tagged | 41,440 (0 exceptions; no_restore 230) |

The 41,501 agreements validate the plan reading against the depth admission
and TAA's own barriers.

**Still to do:**

- the 11 frames armed at a transition (the chain stays at the render size
  while the final copy is the full DLAA copy);
- a broader effects hunt (special fights, other zones);
- then make `full_post` the default and remove the per-effect route (the
  user's call).

## When to arm (v0.25.5 to v0.25.8)

**v0.25.5: armed at the scheduler's entry.** The final quad stays native and
is only remembered. At the scheduler's entry (after the registration pass) the
frame was armed when TAA's descriptor was in the filter list (unit vtable
0x351bda8, stage 15, order 2, skip byte +0x20 clear).

- **game-119 (v0.25.5):** nothing armed. The final-quad helper (0x2297e20) has
  about 40 callers, filters and other views among them; each call reset the
  candidate. **v0.25.6:** only a valid final quad starts a candidate.
- **game-120 (v0.25.6):** 5,019 schedulers, 5,019 "TAA not registered". At
  the scheduler's entry the list is in registration order, not yet sorted. The
  colour grade's endpoint (15, 0x7fffff1) has no unit and comes before TAA's
  descriptor, and the search stopped at the first entry without a unit (the
  fixture had TAA first). **v0.25.7:** entries without a unit, or skipped, are
  passed over.

**game-121 (v0.25.7): Fatalis, Safi'jiiva, Alatreon.** Every DLSS frame armed:
157,555 of 157,551 frames, every input upscaled, 0 state mismatches, 106,684
depth restores, fault 0. Two defects:

- **The tent menu was mangled:** the render-size image sat in the top-left
  corner, with stale content around it. The runtime refuses every tent frame at
  TAA's producer (`taa_before`; a skip with no reason, no publication). The
  scheduler's entry had already recorded the full-size DLAA copy, so the copy
  ran with no plan and no DLSS output to copy. 6,743 such frames.
- **The quest reward screen's background was zoomed.** That background is a
  one-frame scene capture (`sub_142268890`, called by the camera's draw right
  after the final quad). `sScene` (image+0x51c4368) holds the request:
  +0xE998 equal to the renderer's epoch, set by `sub_141B602B0` for the
  result's `ui\result\tex\capture_back`. The capture draws the target of pass
  +0xE990 (default 15; view +0x60 + 0x40·pass → +0x58, the final quad's
  tBaseMap) into +0xE988, at key (pass, priority +0xE994; default 0xffffffff,
  the end of the chain), with `System_CopyScaleOffset`. Its texture
  coordinates are the context's rectangle over the view's output size: the
  render region. In an armed frame that target holds the full-size chain
  output, so the capture showed its top-left corner stretched.

**v0.25.8:**

- **The entry requires an admitted queue.** The runtime's own check, as for
  `post_queue_complete`: its TAA, the owned context, the frame's view.
- **Armed after TAA's callback,** and only when the runtime has published the
  frame there (`sr::publish` runs from the TAA hook inside the callback). Only
  then is the DLAA copy recorded, in the final quad's key. The identity plan
  finds its final copy by key and owners, not position. The tone map's
  callback still runs at the full rectangle once the frame has entered. On a
  frame that then stays native, this leaves the render region as it was: its
  steps are per-pixel or linear in the pixel position, as before TAA in armed
  frames.
- **Unplanned copies are dropped.** The DLAA copy is registered with its
  packet. At execution, without a prepared identity plan that takes it as the
  final copy, its copy is dropped, and the final quad's native upscale (which
  it would overwrite) stays.
- **The scene capture is widened.** Its quad (stage 15, after TAA and the
  slot, between the final quad and the scheduler's entry, with exactly the
  render region's vertices) is found when the frame arms. Once the plan is
  prepared, before the commands execute, its texture coordinates are widened
  to the whole texture.

**game-122 (v0.25.8): hub, tent, a quest and its reward screen.** The user:
the tent "look is correct but its skipping", and "endscreen looks correct now".

| Measure | Count |
| --- | --- |
| Armed frames (DLAA copies), all planned | 33,808 |
| Upscales | 33,807 (1 in flight), 0 state mismatches |
| Unpublished at TAA (the tent; native) | 16,136 |
| Unadmitted (title, loading) | 4,048 |
| Unplanned copies dropped | 0 |
| Scene captures widened | 1 (the reward screen), 0 refusals |
| Depth restores / silhouettes corrected | 21,301 / 426,039 |
| DLSS skips | 16,400: the tent's 16,136 and 264 transition frames without depth (planned: spatial resolve) |

The tent is native because the runtime refuses its frames at TAA's producer,
which full_post does not change (game-121 shows the same refusals). v0.25.9
counts those refusals by check (`producer_refusals`, `producer_last`) to find
out why.

## The tent's projection (v0.25.9, v0.25.10)

**game-123 (v0.25.9):** every tent frame was refused at TAA's producer with
reason 7: the projection in the ring was from frame 14,083, while the tent
frames reached 16,931. No projection had been captured since the tent opened.

**Cause.** `sCamera::draw` (0x228e770) loops over eight `sCamera::Viewport`
slots (0x1a0 bytes each, from camera +0x50: +0x8 `mpCamera`, +0x18
`mpSceneTexture`, +0x20 `mVisible`, +0x23 `mPriority`, +0x25 `mDisplay`). For
each visible slot of the display being drawn, it computes the projection
(`sub_14228F3B0`) and draws the scene and post chain. sMhCamera's camera
switch (`sub_141AC3840`) hides viewports 0, 1 and 2 and shows the one the new
camera names. `uTentCamera` (and the map camera) switch to viewport 1. The
runtime's projection anchor accepted only viewport 0.

**v0.25.10:** the anchor accepts the viewport being computed when it is
visible, of display 0 and without a scene texture (drawn into the view the
frame owns). Projections are counted per viewport.

**game-124 (v0.25.10):** the user: "tent looks corrected". 3,882 projections
from viewport 1 (the tent), 0 producer refusals, 0 unpublished frames, 12,400
armed and planned frames for 12,398 DLSS frames, 1 skip, fault 0.
