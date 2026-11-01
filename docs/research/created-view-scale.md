# A created view's scale (0.40.1, xvinnie-1 runs, 2026-11-01)

Clean image (`a78ea69b…1d80`), database `local\ida-clean-20261011\mhw.i64`.

## The bug

With the game's 16:9 aspect option in a 3440x1440 window, the scene is a
pillarboxed 2560x1440, and the render scale is quantized against the window
(3440). DLSS stopped for the rest of the session, with no fault and no Status
change, whenever the game remade its buffers. Every frame was refused at the
TAA producer:

- after frame generation's chain switch (runs 4 and 5): refusal 10, the
  input stamp; dev.2's `stamp_refusal` showed why 8 (the view's combined
  scale 1702/2560 = 0.664844 against the plan's 2288/3440 = 0.665116), and
  the view's scale held the same ratio;
- after the ResizeBuffers when the CMFM menu opened (run 6), and from the
  start with DLSS on, where the start-up rebuild resizes twice (run 7):
  refusal 12, the TAA rectangle 1285x722 against the planned 1285x723.

It is older than 0.40.1; 0.40.0 has it. It needs a scene the window basis
doesn't divide evenly: 16:9 in 3440, 21:9 (3413) in 5120 at most modes, 16:9
in 3840x1600. A full scene (the basis is its own width), 16:9 in 5120 (exactly
2:1) and letterboxing are not affected, nor is xVinnie's full 3440 scene.

## The game's two view states (decompiler, confirmed)

- **Created view**, `sub_14255BA70` (the view constructor, renderer view
  creation at `+0x2293f10`): clamps the scale s to [0.5, 1], then stores
  `(float)(int)(W * s) / W` at view `+0x6b0` (W the scene width, renderer
  `+0x198`) and its product with the content scale, clamped to [0.5, 1], at
  `+0x6b8`.
- **Updated view**, `sub_14255D700`: stores the scale itself (clamped) at
  `+0x6b0`, its product at `+0x6b8`, and marks `+0x6bc` dirty. Its only caller
  is the renderer's scale block right after the update the mod hooks
  (`0x229833a`, after `0x22982e7`), with the main view `+0x1F450` and the
  desired scale `+0x1f0`, and only when the desired differs from the applied
  `+0x1f4`.
- The view rectangles scale each side by the view's scale in float and
  truncate (`sub_1426352B0` reads `+0x6b0`). The TAA rectangle follows the
  combined scale: game-22's title view held 1706/3413 (below 0.5) at `+0x6b0`
  and rendered 1706x720, the clamped 0.5.

So one plan renders two rectangles: trunc(dims × q) in an updated view, and
trunc(dims × trunc(W·q)/W) in a created one. They differ by at most a row or
a column, and only when W·q is not an integer.

## The fix

When the scale is already applied (`+0x1f4` equals the plan) and the main
view holds another scale, the update hook gives the view the plan's scale
through `sub_14255D700`, at the point in the frame where the game itself calls
it. The view then renders the plan's rectangle whatever made it. The function
is in `sizing_code()`'s gates (95 bytes). `created_views` in `dlss_summary`'s
`stamp_refusal` and in the `resolution` event counts these calls.

Not taken: choosing a 16-pixel step on which both states agree. Most plans
already agree, but over realistic windows the search moved up to hundreds of
steps for some shapes, changing the player's scale.

dev.3 also takes the created view's ratio as a planned combined scale, for a
frame that is rendered before the hook's next visit.
