# The mod's own GPU work around DLSS (0.32.0, 2026-10-27)

## Why

alalalalan01 (RTX 3070 Ti, 1440p Quality) measured ours 1.1 ms a frame
behind another mod. On a 5090 slowed to about a 3070 Ti
(`local/handoff-dlss-inputs-perf-20261027.md`), `gpu_timing` put the mod's
own work at about 0.27 ms a frame at Quality: input copies 0.12, conversion
0.05, output 0.10. On the full 5090 at DLAA it is about 0.17 ms.

## What each frame did (up to dev.19)

At the TAA dispatch, with A the scene allocation, r the render size and O
the output size (A = O; at DLAA r = A too), in bytes a pixel:

| Step | Traffic |
|---|---|
| CopyResource of the game's colour (R11G11B10), depth (R32F), motion (R32_UINT) | 3 x 8 at A |
| Conversion: colour to RGBA32F, depth to R32F (unchanged), motion to RG32F | 12 read + 28 written at r |
| DLSS reads colour, depth, motion | 16 + 4 + 8 at r |
| DLSS writes a private RGBA32F output | 16 at O |
| Packing to R11G11B10 (nearest-even), then CopyResource into the game's TAA output | 20 + 8 at O |
| Frame generation: converted depth and motion, copied twice (staging, then the tagged slot) | 2 x 24 at r |

The diagnostic clear of the private output (65504) was already off in every
build (`no_output_sentinel` is part of the lean toggles, 1919).

## dev.20

- DLSS reads the colour copy itself (R11G11B10 with the render sub-rectangle);
  the converter writes only depth (frame generation stages it at the render
  size) and motion.
- Motion is RG16F: the game stores it as two halves, and scaling by half the
  render size keeps its relative precision.
- DLSS writes the game's TAA output (R11G11B10, in UNORDERED_ACCESS at the
  dispatch, pinned by the caller), as the game's own TAA does: no private
  output, packing or copy back. `record()` checks its layout; after a failed
  evaluation the copies go back to COPY_DEST and the caller's spatial recovery
  writes the output as before.
- Barriers go in batches (one call each way) instead of one call each.

Per pixel that removes 32 (colour) + 8 (motion) at r and 40 at O, 24 more
at r with frame generation; at 1440p DLAA about 300 MB a frame. The images
shrink from 64 to 24 bytes a pixel at DLAA (4K DLAA: about 530 to 200 MB).

## Left (measure dev.20 first)

- The three input copies (24 bytes a pixel at A): DLSS could read the game's
  colour and depth in place, and the converter its motion through a per-frame
  descriptor. The copies also feed the right-edge fill (fill_edge) and the
  spatial recovery, so this needs its own design.
- Frame generation's two copies of depth and motion: the tagged slot could be
  filled once.
