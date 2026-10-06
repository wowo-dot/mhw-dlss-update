# Current state

Updated 2026-10-06. The v0.6.5 scene diagnostic passed in game and stays
installed (git tag `v0.6.5`). DLAA, DLSS Performance and Reflex work in MHW;
MHW frame generation is not enabled yet. Priority: ordinary FG, then MFG; the
configuration-based alpha and performance cleanup follow.

## Latest result

`frame-generation-scene-depth-presentation-result.md`: the UI is composited
directly onto the final scene target `T` after our replaced final draw, with
alpha/additive blending and depth off, in the same command list. `T` is then
copied to an intermediate texture and the presented backbuffer is written by a
call outside the hooked scope. The HUD-free image is therefore `T` right after
our final draw. The swap chain (3 buffers, generation stable through an area
load) and its presented index are mapped; the game queries the index once per
Present. Zero faults, full cleanup verified.

## Plan (agreed 2026-10-06)

DLSS-G requires only depth and motion vectors (plus camera constants and the
Reflex frame token); HUD-free colour and UI alpha are quality inputs. Next:

1. Minimal ordinary-FG slice in MHW: reuse the DLSS-path depth/motion vectors,
   non-jittered camera constants and the existing Reflex token, plus a HUD-free
   copy of `T` taken right after our final draw. Accept HUD artifacts at first.
2. FG swap-chain lifecycle in game: SDK-paced presentation, worker routing,
   resize/alt-tab, and chain retirement/recreation at shutdown.
3. Apply the screen-write transform (brightness `fGammaCorrectEx` + sRGB) to
   the HUD-free copy so it matches the backbuffer; then UI alpha.
4. MFG on the same inputs.

GPU capture (`frame-generation-hudless-capture-result.md`, RenderDoc driven by
`tools/rdc/`) confirmed the HUD-free `T` with pixels. `T` is linear
`R11G11B10_FLOAT`; the screen write applies the game's brightness curve and sRGB
encode into the BGRA8 backbuffer. DLSS-G's HUD-less input must therefore be the
HUD-free `T` passed through that same pointwise transform, not a raw copy.

## Working method

Git is the source of truth: one commit and annotated tag per tested version.
Runtime evidence stays private under `local/` and is sealed after normal exit
with `tools/evidence.py`. Accepted reviews and the independent hook-restoration
check remain mandatory for every game run.

Older dated entries: `history/current-state-log-to-20261005.md`.
