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
- **A separate post-filter scale exists.** The renderer reflects
  `mContentScaleActivePF` ("ContentScalePF", float at renderer+504,
  initialised to 1.0). The screen constants (`0x23CE3C0`) publish
  `fContentScale` (+500), `fContentScalePF` (+516 x +504), `fContentScaleBase`
  (+516), `fContentScaleActual` (+520) and `fContentScalePassScreen` (+524)
  to the shaders separately.

## Not established

- Whether post filters actually run at a different resolution when
  ContentScalePF differs from the scene scale, and what the engine sets it to
  in DLSS mode. The writers of renderer+504 were not isolated (a field-offset
  scan near the renderer global returned mostly unrelated structures).
- Whether the native path can run on the user's RTX 5090 at all: DLSS 1.x
  hides its menu row there (`docs/runtime/rtx50-unlock-test.md`), so the NGX
  context and its ready byte would have to be supplied by the mod.

## Assessment

Capcom put DLSS where the mod puts it, with the same inputs, and the engine
has a dedicated post-filter content scale. If the engine renders the post
chain at full output once that scale is set, the mod could drop the
stream route's per-draw interception and re-recording for that work and let
the game do it. That is a large simplification and the main CPU saving.
It is a hypothesis until a runtime check shows the post chain's actual size.

## Next steps

1. **Read-only live check** (next run, no code change): read renderer
   +496..+524, +165, +176..+196 and +180 from the running game in the current
   setup (game resolution scaling, DLSS mod armed) to see which fields carry
   0.5 and what ContentScalePF holds.
2. **Diagnostic experiment** (separate build, behind a switch): set
   ContentScalePF for one session and record the post filters' render-target
   sizes and viewports with the existing observers. If the chain renders at
   full output by itself, design a v2 route around it.
3. Keep the current route as the shipping path until that is proven.
