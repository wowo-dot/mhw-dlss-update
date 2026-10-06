# FG scene observer v0.6.5: runtime result

2026-10-06. Accepted. Four requested captures (main menu, gameplay with HUD,
open in-game menu, gameplay after a confirmed area load) passed the strict
v0.6.5 review. The user reported a normal scene throughout and a normal exit.
Design: [frame-generation-scene-depth-presentation.md](frame-generation-scene-depth-presentation.md).
Code: git tag `v0.6.5`; DLL SHA-256
`5be04bc3cfce30628a7f4dcd2a397488ca9a338ab6b0fafdc6674dded2eec793`.

## Confirmed: the UI is composited onto the final scene target

Every draw previously rejected as reason 9 now qualifies through the separate
depth-bearing metadata path; reason 9 is zero in all four captures. Each such
draw agreed with the native colour, depth, root and PSO caches, returned, and
was submitted before the qualified Present.

| Capture | Depth-bearing draws | After final draw | Alpha / additive | Depth test off | Shaders | Presented index |
|---|---:|---:|---:|---:|---:|---:|
| Main menu | 37 | 37 | 27 / 10 | 37 | 4 | 2 |
| Gameplay, HUD | 152 | 152 | 146 / 6 | 152 | 3 | 2 |
| Menu open | 356 | 356 | 326 / 30 | 356 | 5 | 0 |
| After area load | 45 | 45 | 43 / 2 | 45 | 3 | 2 |

All of them draw onto the final target `T` (the resource our replaced final
draw writes) with RT0 blending `SRC_ALPHA` / `INV_SRC_ALPHA` or `SRC_ALPHA` /
`ONE`, alpha `ONE`/`ZERO`, RGB write mask only, depth test and depth write off,
and a D32 (`DXGI_FORMAT` 40) depth attachment bound but unused. The pipeline
state alone does not prove UI semantics, but together with the order below
this is the UI/HUD composition pass.

Submitted order within the final command list (same queue, every capture):

1. the exact replaced final draw writes `T`;
2. all UI draws composite onto `T`;
3. `T` transitions RENDER_TARGET → COPY_SOURCE | PIXEL_SHADER_RESOURCE and is
   copied whole (2560x1440 box) to an intermediate texture;
4. the presented backbuffer transitions PRESENT → RENDER_TARGET → PRESENT. The
   write between them comes from a call outside the two hooked executor scopes
   (one reason-6 draw per capture has an unreadable backing label).

With the menu open, four 1280x720 copies onto `T` occur in earlier lists of the
same submission and so precede the final draw, matching v0.6.3.

**Consequence for FG:** the HUD-free scene is `T` between steps 1 and 2. That
point is our own replaced draw, so a HUD-free copy can be taken right after it
in the same command list without searching for it.

## Confirmed: presented buffer table

Request-time and Present-time tables agreed in every capture: chain
generation 7 (unchanged through the area load), three known buffer labels, no
conflicts or foreign-chain calls. The game queries `GetCurrentBackBufferIndex`
exactly once per Present, and its result always matched our Present-entry query.
Presented indices were 2, 2, 0 and 2.

## Not established

GPU completion, pixel contents, the `T` and backbuffer formats, the source and
shader of the backbuffer write (straight copy versus gamma/brightness pass, which
decides whether `T` has the backbuffer's colour space), UI alpha, and FG input
lifetime. Reason 0 (outside executor scope) and reason 5 (multi-RTV attachments)
remain counted, largely with the menu open.

## Renderer and cleanup

5,913 DLSS Performance evaluations, 4 skipped resolves, 6 history resets, zero
runtime faults; all 5,917 GPU signals completed, 6,929 jitter changes and full
render scale restored. 7,359 complete Reflex On frames, 44,154 markers, 64
driver reports; Reflex restored Off, SDK shut down and pins released. While the
game was still open, the independent checker verified 37 hook heads and 46 full
native functions restored (778 reads, 75,163 bytes, zero process writes).

Evidence: `local/frame-generation-scene-20261006/game-05`, sealed with
`tools/evidence.py` (47 files, manifest SHA-256
`0cceb33e4b0c4f314f5b74e0587a3411bc60f35e1e2d6f88a701f1191ac57594`).
The v0.6.5 DLL stays installed.
