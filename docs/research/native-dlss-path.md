# The game's own DLSS path (static spike, 2026-10-08)

Question: could the mod ride MHW's built-in DLSS integration (swap its DLSS 1
call for modern DLSS and let the engine handle resolution), instead of
watching every Direct3D call and re-recording the post-filter chain at full
output? That would remove most of the mod's CPU cost.

Method: headless IDA 9.3 on a private copy of the decoded image's database
(`local/ida-native-dlss-20261008/`, ignored). Executable SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`. Static
reading only; nothing ran in game.

## Confirmed (static)

- **DLSS is a TAA2 mode.** `uTemporalAAFilter2`'s frontend (`0x2392150`)
  takes the DLSS branch when renderer+165 (`mUseDLSS`) is set, renderer+180
  bit 0 is set and the NGX context at `[renderer+0x78]+0x42700` exists (with
  its ready byte at +0x42718). It fills a `CB_DL` constant block (time,
  screen size, view, projection, view-projection, previous view-projection).
- **The command.** `0x23913F0` dispatches `TemporalAA2_ResolveDenseVelocity`
  into the TAA unit's velocity texture (unit+424), then queues render command
  type 3 (packet opcode 23) with a 64-byte payload: NGX context, color (unit
  +336), motion vectors (unit+424), output (the TAA unit's own output slot,
  unit+360+8k), depth, reset/blend value and the render rectangle.
- **Execution.** The D3D12 backend handler `0x259DAE0` runs type 3 through
  `0x2383CF0` (sets Color, MotionVectors, Output, Depth, Reset, MV scale and
  offset, Rect, then `NVSDK_NGX_D3D12_EvaluateFeature_C` on the engine's own
  list) and type 4 through `0x2383990` (DLISP sharpening). A D3D11 twin
  exists (`0x25996D0`). This is the same filter stage the mod already uses
  (its TAA dispatch hook is in `0x259CC20`).
- **Resolution.** `0x2383EF0` asks DLSS 1's optimal-settings callback for
  OutWidth/OutHeight and returns the scale and mode. `0x229A8C0` stores that
  scale (renderer+176/+184) and sets renderer+512 to it, clamped to 0.5-1.0.
  The graphics menu (`0x43DCB0`, `0x4431F0`, `0x440870`) uses the same query to
  show DLSS and pick the anti-aliasing mode.
- **Scale fields (corrected after a live read, see below).** The renderer
  reflects `mContentScaleActivePF` ("ContentScalePF", renderer+504) and the
  screen constants (`0x23CE3C0`) publish `fContentScale` (+500),
  `fContentScalePF` (+516 x +504), `fContentScaleBase` (+516),
  `fContentScaleActual` (+520) and `fContentScalePassScreen` (+524). "PF" is
  the engine's previous-frame suffix (as in `fViewProjPF`), not "post
  filter": the native scale update preserves the old active scale (+0x1f4)
  in +0x1f8 (`dlss-render-resolution-control.md`). It exists for temporal
  reprojection across a scale change.

## Live read (game-47 and game-48)

| Field | Alpha armed (game-47) | Research build, unarmed (game-48) |
|---|---|---|
| +496/+500 active scale | 0.5 | 1.0 |
| +504 previous-frame active scale | 0.5 | 1.0 |
| +512/+516 allocation scale | 1.0 | 1.0 |
| +520 product | 0.5 | 1.0 |
| +165 mUseDLSS, NGX context | 1, none | 1, none |

The alpha's resolution control lowers the active scale (the scene draws into
the upper-left of full-size textures). The post filters follow the active
scale, which is why the stream route re-records them at full output. The
game's own DLSS 1 sizing (`0x229A8C0`) writes the allocation channel (+0x200)
instead; what the post chain does in that mode is unknown, and it cannot run
on the user's RTX 5090 (no NGX context).

## Assessment

There is no separate post-filter resolution switch to flip: the earlier
"ContentScalePF" reading was wrong, and the v0.12.11 experiment built on it
was reverted unused. Riding the game's own DLSS mode would need the native
DLSS 1 sizing path (allocation scale) mapped end to end and an NGX context
supplied by the mod. That is a large reverse-engineering project with an
uncertain result, so it is parked. Reducing the mod's CPU cost continues
within the current route (cached shader identities, plan reuse, cheaper
submit/reset/present hooks).

## If this is resumed

1. Map the allocation-scale path: what `0x229A8C0`'s +0x200 write does to
   texture sizes, the TAA output and the post chain.
2. Only then consider a live experiment, behind a switch, in the research build.
