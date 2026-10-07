# Current state

Updated 2026-10-06. **DLSS Frame Generation and multi-frame generation work in
MHW** on the DLSS Performance path with Reflex On. The multiplier changes live
from 2x to 6x on the RTX 5090; the user saw smooth 60/90/120/180 fps. Since
v0.9.1 DLSS-G also gets a **HUD-less colour** input, and the HUD stays stable
in generated frames. v0.9.2 takes the brightness from the game's own screen
write. MHW applies resolution and screen-mode changes only on restart, so the
resize milestone became "start at any resolution". The current build is
v0.10.1 (git tag `v0.10.1`), which derives the DOF sequence from the screen
size (`frame-generation-lifecycle.md`). DLAA, DLSS Performance and Reflex keep
working. Next: confirm 1080p, then the configuration-based alpha with
performance cleanup (including 4K).

## Latest result

`frame-generation-game-slice-result.md`. v0.7.3 (2x): about 9.5 minutes of
gameplay, menus, loading and alt-tab, with 13697 frames tagged (depth, motion
vectors, camera constants) and zero SDK errors, camera failures or exceptions.
v0.8.0 (MFG): the multiplier was switched live through 2x, 3x, 4x and 6x, with
a menu and an area change at 6x. Streamline presented exactly 2/3/4/6 frames per
host frame, with zero errors, and the user found every step smooth. Both runs
ended with FG Off on stop and a normal exit.

`frame-generation-hudless.md`. v0.9.1 and v0.9.2 (2x and 4x): HUD-less colour
was tagged on every FG frame, with zero skips or errors. The user saw the
expected FG artifacts in the scene but none on the HUD. The captured
brightness followed the in-game slider from 0.1 (minimum) to 3.0 (maximum);
v0.9.3 widens the accepted range to cover the minimum.

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

1. Start at any resolution: 1080p (v0.10.1), then borderless and fullscreen
   at startup.
2. Configuration-based alpha: remove the diagnostic readers and overhead,
   restore the base frame rate, keep the research build separate. Lift the
   4K limits there (the large readbacks only serve diagnostics). Widths that
   are not a multiple of 32 (3440 ultrawide) need the game's actual render
   size.

UI colour+alpha (DLSS-G recomposition) is deferred. HUD-less colour alone gave
a stable HUD, and the game blends its UI in linear space before the brightness
curve, so exact recomposition is not possible
(`frame-generation-hudless-capture-result.md`).

## Installed now: v0.10.1

`mhw_fg_game_probe` (DOF sequence from the screen size, plus the v0.10.0
lifecycle guard) is in the MO2 slot with its run folder
`local/fg-game-20261006/game-12` (latest sealed run: `game-11`, v0.10.0 with
three launches; `game-10` holds an unlaunched v0.9.3 install). A run folder
can hold several launches when MHW is restarted for a setting; `fg_game.py`
uses the newest, or `--pid`. Install or restore with
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
