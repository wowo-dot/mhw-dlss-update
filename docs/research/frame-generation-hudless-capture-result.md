# HUD-free scene and screen write: RenderDoc capture

2026-10-06. One gameplay frame of the unmodified game (launched without MO2, so
no project DLL loaded; native AA and DLSS Off, DX12, 2560x1440, HUD visible),
captured with RenderDoc 1.46 and analysed headlessly with `tools/rdc/`. Local
capture: `local/renderdoc-20261006/mhw_hud_frame14384.rdc` (private).

## Confirmed

**Final scene target `T`** is a 2560x1440 `R11G11B10_FLOAT` colour target holding
**linear** scene colour. In the unmodified game the post-processed scene reaches
`T` by a `CopyTextureRegion` from another target; in our DLSS route it arrives
through the replaced final draw (v0.6.5). Either way, everything after that
point and before the screen write is UI.

**UI pass:** 239 draws onto `T`, nothing else in between: 203 with
`SRC_ALPHA`/`INV_SRC_ALPHA`, 36 additive (`SRC_ALPHA`/`ONE`), RGB write mask,
depth test and depth write off (a D32 target is bound but unused), four pixel
shaders, UI atlas inputs (A8 glyph atlases, BC7 icon sheets), small vertex
counts. Dumped pixels: `T` right before the first UI draw contains the full
scene with no HUD element; after the last UI draw it contains the complete HUD
(health/stamina, minimap, prompts, notifications, item bar).

**After the UI:** `T` is copied whole to an intermediate texture, then a single
full-screen draw writes the swap-chain image (`B8G8R8A8_UNORM`, 2560x1440,
blend off, depth off). Its pixel shader samples `T` and applies, per channel:

```text
c  = saturate(T)
k  = min(0.05 * g + 0.95, 1)          g = fGammaCorrectEx.x (cb3 "CBSystem")
c' = 1 - (1 - k * c) ^ g              brightness curve from the game setting
out = sRGB_encode(c'), alpha = 1      IEC 61966-2-1 piecewise encode
```

So the presented image is `T` with UI, passed through the brightness curve and
sRGB encoding. The UI is composited in linear space before that curve.

## Consequences for frame generation

- The HUD-free image exists exactly once per frame: `T` immediately after the
  scene arrives (our replaced final draw) and before the first UI draw.
- DLSS-G requires HUD-less colour in the **same colour space as the
  backbuffer**. A raw copy of `T` is linear `R11G11B10_FLOAT` and does not
  qualify. The HUD-free input must be `T`'s HUD-free copy passed through the same
  brightness curve and sRGB encode, written to a backbuffer-sized UNORM target.
  The transform is pointwise; its only parameter is `fGammaCorrectEx.x`.
- Because the UI is blended in linear space and then curved, a UI colour+alpha
  buffer cannot satisfy DLSS-G's `UI + (1 - a) * HUDless` recomposition exactly.
  HUD-less colour first; UI alpha (coverage only) is a later refinement.
- Present uses `SyncInterval 0` with `DXGI_PRESENT_ALLOW_TEARING` (Nsight
  capture of the same build), relevant to SDK pacing.

## Not established

How `fGammaCorrectEx` is sourced each frame (read from the game's constant
buffer at the screen write, or from settings memory), the menu/cutscene
variants of this order, and HDR output (not used in this configuration).
