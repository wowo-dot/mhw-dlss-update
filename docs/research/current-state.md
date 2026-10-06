# Current state

Updated 2026-10-06. **DLSS Frame Generation and multi-frame generation work in
MHW** on the DLSS Performance path with Reflex On. The multiplier changes live
from 2x to 6x on the RTX 5090; the user saw smooth 60/90/120/180 fps. The
current build is v0.8.0 (git tag `v0.8.0`). DLAA, DLSS Performance and Reflex
keep working. Next: the HUD inputs, then resize, then the configuration-based
alpha with performance cleanup.

## Latest result

`frame-generation-game-slice-result.md`. v0.7.3 (2x): about 9.5 minutes of
gameplay, menus, loading and alt-tab, with 13697 frames tagged (depth, motion
vectors, camera constants) and zero SDK errors, camera failures or exceptions.
v0.8.0 (MFG): the multiplier was switched live through 2x, 3x, 4x and 6x, with
a menu and an area change at 6x. Streamline presented exactly 2/3/4/6 frames per
host frame, with zero errors, and the user found every step smooth. Both runs
ended with FG Off on stop and a normal exit.

Things to know:

- DLSS-G only interpolates while the game window is focused.
- When interpolation resumes, the driver's flip handshake causes one or two
  dropouts of about 150 ms. These come from the driver and Streamline, not from
  our inputs.
- The base rate in gameplay is about 30 fps in both the FG build and the
  DLSS-only v0.6.5 build. That is diagnostic-build overhead and must be fixed
  before the alpha.
- With DLSS-G loaded, NGX keeps Streamline's first search path, so the game's
  DLSS provider lives in the same combined runtime folder
  (`tools/fg_runtime.py`).
- A boot crash at host RVA `0x2244930` with any build means Steam was started
  through MHW's restart path. Restart Steam normally.

## Plan

1. HUD inputs. The HUD-free `T` (linear `R11G11B10_FLOAT`, right after our
   final draw) goes through the game's screen-write transform (brightness
   `fGammaCorrectEx` + sRGB) to form the HUD-less input; then UI alpha.
   Background: `frame-generation-hudless-capture-result.md`.
2. Lifecycle: resolution change and swap-chain recreation with FG On.
3. Configuration-based alpha: remove the diagnostic readers and overhead,
   restore the base frame rate, keep the research build separate.

## Installed now: v0.8.0

`mhw_fg_game_probe` is in the MO2 slot with its run folder
`local/fg-game-20261006/game-06` (sealed). Install or restore with
`tools/mo2_slot.py` (the receipt in the run folder has the restore command).
`<dll>.fg-mode` can contain `off` (run without DLSS-G) or `2x`..`8x` (the
starting multiplier; default 2x). `fg_game.py multiplier --run <run> --x N`
changes the multiplier while armed.

Procedure: launch through MO2 to the main menu, then
`python -B tools/fg_game.py arm --run <run>` (DLSS, Reflex and FG On, 600 s),
play with the game window focused, `stop` (FG Off; the SDK stays alive until
the process exits), exit normally, `summary`, then `tools/evidence.py seal`.

## Working method

Git is the source of truth: one commit and annotated tag per tested version.
Runtime evidence stays private under `local/` and is sealed after normal exit
with `tools/evidence.py`. FG builds keep Streamline alive until process exit,
so they have no hook-restoration check. The DLSS-only builds keep theirs.

Older dated entries: `history/current-state-log-to-20261005.md`.
