# NVSharpen after the game's grade (0.32.0, 2026-10-27)

The publicly promised sharpening slider (2026-10-23). DLSS's own sharpening is
deprecated in the SDK, so the mod runs NVIDIA Image Scaling's NVSharpen, as
NVIDIA recommends for DLSS titles.

Sources: NVIDIA Image Scaling SDK 1.0.3 (NVIDIAGameWorks/NVIDIAImageScaling,
tag `v1.0.3`, commit `35e13ba316c98eeecf16f37eae70ce88019911f6`, MIT; every
file of GitHub's tag archive matched the tag's git tree, 2026-10-27), and the
clean database `local/ida-clean-20261011/mhw.i64` (image `a78ea69b…`).

## What NIS asks for (its README and headers, confirmed)

- **Placement:** after tone mapping, after low-pass effects (motion blur,
  bloom), before film grain and the UI.
- **Colour:** display-referred. Three modes:
  - LDR: [0, 1] after tone mapping and the OETF (gamma);
  - PQ: [0, 1], Rec.2020 PQ; its luma uses BT.2020 weights (`getY`);
  - Linear: [0, 12.5] with 1.0 = 80 nits. Its sharpening ramp
    (`kSharpStartY` 0.3 to 0.5 of `sqrt(Y) * 0.2828`) starts above 1.0, so
    SDR content in it would only get the minimum strength.
- **Not in place:** it reads `in_texture` and writes `out_texture`; blocks read
  their neighbours' pixels.
- **Sampling:** NVSharpen loads its tile with `NVTEX_SAMPLE` at texel centres
  only (`kShift` = 0.5 − 2), so a value encoded after the sample equals the
  texel encoded. The store is `NVTEX_STORE`; `NIS_CLAMP_OUTPUT` saturates it
  (LDR and PQ).
- **Sizes:** NVSharpen for NVIDIA GPUs: 32 x 32 blocks, 128 threads
  (`NISOptimizer(false, NVIDIA_Generic)`). Constants from
  `NVSharpenUpdateConfig`; sharpness 0 maps to (almost) no sharpening.

## Where the game's image is display-referred (confirmed, earlier notes)

- The frame's final scene target T is linear `R11G11B10_FLOAT`, after the
  grade. The UI is drawn into T, then the screen write encodes it
  (`screen_transform.hpp`): the brightness curve and sRGB for SDR, or the
  BT.709 → BT.2020 rotation and PQ for HDR10
  (`System_CopyToBackBufferPQSingle`, `depth-of-field-and-hdr.md`).
- Since v0.26.0 every plan is an identity plan: the frame ends with the
  game's copy from the grade output to T (`stream_route.cpp`, Plan). The
  final-scene observer runs right after that copy, before any UI draw, with T
  in COPY_DEST. Frame generation's HUD-less copy is taken there.

So T at the observer is the finished, HUD-free image: the place NIS asks for.

## The HDR parameters (confirmed, static)

`0x142288bd0` fills CBRenderFrame from the renderer singleton
(`qword_1451C4480`, `layout::image::renderer`):

- `bHdrOutput` = renderer +0x21A (byte; `0x142289179`);
- `fHdrOutputWhiteLevel` = renderer +0x21C (float, nits; `0x1422891ce`);
- `fHdrOutputGamutMappingRatio` = renderer +0x220.

The renderer's init stores 300.0 at +0x21C (`0x14229958a`). The property
`HdrOutputWhiteLevel` (`0x142290880`) reads and writes +0x21C
(`0x142298ed0`, `0x14229c370`). `HdrOutput`'s setter (`0x14229c320`) stores
the player's choice at +0x21B and the effective state (choice and display
support) at +0x219, and requests a new chain (+0x1F548 |= 0x4000); +0x21A is
latched from +0x219 (`0x142293435`). game-109's capture read a white level of
720.69, the player's setting.

## The pass (0.32.0)

In the final-scene observer, before frame generation's record:

1. T: COPY_DEST → NON_PIXEL_SHADER_RESOURCE; the mod's scratch image C (T's
   size and format, typed UAV store) in UNORDERED_ACCESS.
2. NVSharpen reads T and writes C, with an encode on load and the matching
   decode on store (two `#ifndef` guards added to the vendored
   `NIS_Scaler.h`):
   - SDR: sRGB encode of saturate(T), LDR mode, sRGB decode;
   - HDR10 (renderer +0x21A set): BT.709 → BT.2020, PQ of
     `c x white / 10000`, PQ mode, then the inverses.
   The store writes T + (decode(sharpened) − decode(encode(T))): where
   NVSharpen changes nothing, T is written back bit for bit (a GPU decode of
   the encode is not exact: the first WARP run moved a flat SDR column by one
   R11G11B10 step), and SDR values above 1 are kept. The white level only
   places NIS's luma ramp.
3. C → COPY_SOURCE, T → COPY_DEST, CopyResource(T, C), C back to
   UNORDERED_ACCESS. T ends as it came; the game's bindings are restored
   (RestoreBindings), as the HUD-less pass does.

Frame generation then copies the sharpened T, so its HUD-less image matches
the frame. Sharpness 0 records nothing. With DLSS off the mod steps aside and
the game's own CAS is available again.

## Not checked yet (hypotheses)

- That no post effect runs on T between the observer and the UI (grain,
  vignette). The plan's last native step before the UI is the final copy.
- The cost in game (one dispatch and one copy at the output size).
- How strong the default steps look; the slider maps linearly to NIS's
  sharpness.
