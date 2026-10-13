# Long session (game-110, v0.24.8), and v0.24.9 (2026-10-13)

## The session

v0.24.8, frame generation 2x, Quality, Reflex Boost, DoF on, SDR (the game's HDR
was off this time), 2560x1440 windowed. About 40 minutes: hub, field, training
grounds, cutscenes.

- DLSS: 472,731 frames, fault 0. 11,999 skips:
  - 9,615 from the equipment box's depth of field;
  - 2,298 from a cutscene blink (Namielle);
  - 10 single-frame `depth_missing` skips, as in earlier runs.
- Silhouettes: 5.38 M corrected, 1.05 M silent (rejection 11), 0 unknown.
- Frame generation:
  - 472,728 frames tagged, 0 exceptions;
  - the HUD-less image recorded on every tagged frame, with 472,736 valid SDR
    brightness probes;
  - 120,932 presents had no ready frame (menus, loading and the skips above).
- sl.log has no errors.
- What the user noticed: only the equipment box ("window skips") and the
  Namielle cutscene. Each skip is the fail-closed fallback: the game's own image
  for that frame, and frame generation paused.

## The equipment box: depth of field with other far/near settings

The pre-motion depth of field (v0.24.5) qualified, except for its last check,
which failed (`failed_at` 95).

That check required CBNewDOFFilter words 11-14 (`far_ignore`, `near_ignore`,
`far_coef`, `near_coef`) to be gameplay's 1, 1, 1.0, 1.0. The offline fixture
had only been validated with those values. The equipment box blurs the
background behind the hunter with other values.

Only passes 0 (TileDepthH) and 3 (DownSample) read these fields, in their
per-texel CoC math. The disassembly in
`local/shader-census-20261008/dof-cb3/` shows:

- `far_ignore` nonzero keeps a positive (far) CoC, and `near_ignore` nonzero
  keeps a negative (near) CoC. Otherwise that CoC becomes zero and the depths
  become the focus depth; with both zero, there is no blur.
- `far_coef` and `near_coef` scale the positive and negative CoC.
- DownSample averages a sample when its own CoC (zero within the focus
  threshold) times the quad's CoC is not negative.

The route runs that math unchanged; pass 3's adaptation changes colour loads
only. So any value the game writes gives the game's own result.

**v0.24.9:**

- `dof_blur_supported` accepts either flag 0 or 1, and coefficients that are
  finite and non-negative. Other values still refuse the frame (95).
- The route summary now records:
  - qualified frames per configuration (`configs`);
  - the configuration of the first and the last refusal.
- `dof_prepare_scale_fixture` adds 6 far/near configurations on patterned depth
  (near, focus, mixed and far quads). Each runs at Performance, Quality and the
  3413-wide pillarbox.
  - A CPU oracle of the sample choice above matches both the adapted pass and
    the native proxy (within 1.2e-7).
  - The CoC writer is bit-identical between the two.
  - Every non-gameplay configuration changes the result, so the flags are
    really exercised.
  - It passes on WARP and on the RTX 5090.

## The cutscene blink: a source-free effect before the motion filter

The plan refused with reason 6, detail 9 (filters: tonemap at order 0,
`uCircularDOFFilter` 0x353e048 at order 1, TAA at order 2, `uBlinkFilter`
0x35441a0 at order 17).

The pre-motion slot (orders 3-99) expected a filter with an input (the hub's
depth of field). The blink has none.

- The blink is a constant full-screen draw: copy VS `730b7d23`, PS `27c041a9`,
  CBBlink only.
- It draws in place on the TAA output (its output equals TAA's).
- The circular depth of field at order 1 runs before TAA, at render size, and
  needs nothing.

**v0.24.9:** a source-free descriptor in the pre-motion slot becomes an
in-place overlay. It is the same overlay as the ones after the motion filter:
the native draw, with only its viewport and scissor widened to the output.

- It must draw on the image the motion filter reads: the TAA output (plan
  resource 1, which DLSS fills at full size), or the pre-motion filter's output
  when its key follows that filter.
- Its flags must be the in-place overlay's 0, 1, 0.
- It goes through the usual draw qualification: geometry, the pinned VS, the
  overlay pixel-shader contract, inputs, and the target resource.
- Ordering lets such overlays precede the motion filter; dependency ordering
  skips them.
- Anything else still refuses the frame (6/9).
- `scaled_stream_route_fixture` (overlay modes) has a pre-motion case:
  - accepted, ordered first, targeting resource 1;
  - 4 malformed variants rejected: another target, other flags, an input, and
    an RTV on another resource.
  - It passes on WARP and the RTX 5090 with the census's real Blink shaders, and
    on WARP with the captured overlay shaders.
- 673 tests pass.

## Next

- A v0.24.9 check:
  - open the equipment box (`configs` should show its far/near values);
  - replay the Namielle cutscene from the Gallery (plan rejection 6 should stay
    at 0).
- The user asked whether the whole post-processing chain can be fixed at once
  instead of effect by effect. The research has started
  (`full-size-post-chain.md`).
