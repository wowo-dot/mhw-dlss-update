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

## Next step

A diagnostic build, with no rendering change, that logs the post census:

- every packet with a stage-15/16 key: key, opcode, rectangle, target size;
- the copies of the first scheduler loop;
- the formats and flags of the auxiliary inputs.

A Quality run and a DLAA run answer questions 1-3. Then the prototype goes
behind an ini key (`full_post = on`), with the census as its check.
