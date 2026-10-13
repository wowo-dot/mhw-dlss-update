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

**Still to do:**

- upscale the render-size inputs (depth, GBuffer4/5, occlusion) for depth of
  field, motion blur and the depth-based effects;
- plan the late silhouette draws in armed frames;
- the 4 frames armed at a transition;
- an effects hunt with `full_post = on`.
