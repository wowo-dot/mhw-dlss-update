# Current state

Updated 2026-10-08.

**What works**
- **DLSS Frame Generation and multi-frame generation**, on the DLSS
  Performance path with Reflex On. The multiplier changes live from 2x to 6x on
  the RTX 5090; the user saw smooth 60/90/120/180 fps.
- **HUD-less colour input** (since v0.9.1). The HUD stays stable in generated
  frames, and v0.9.2 takes the brightness from the game's own screen write.
- **DLAA, DLSS Performance and Reflex** keep working.

**Start at any resolution.** MHW applies resolution and screen-mode changes
only on restart, so the resize milestone became "start at any resolution".
Working so far (`frame-generation-lifecycle.md`):
- 1080p and 1440p.
- Windowed, borderless and exclusive fullscreen.
- The **21:9 aspect option**, which letterboxes a 2560x1080 scene in the
  window: DLSS and DOF since v0.11.0, FG since v0.11.1. DLSS-G gets the scene
  as a rectangle inside the window.
- **21:9 pillarboxed in a 5120x1440 window**: an odd 3413x1440 scene rendered
  at the game's truncated 1706x720. DLSS, DOF and FG work since v0.11.5 and
  v0.11.6. A right-edge tint after arming mid-session is still open.
- **16:9 pillarboxed in a 5120x1440 window**: DLSS, DOF and FG (game-28).
- **FG's scene rectangle:** before v0.11.7, every letterbox and pillarbox ran
  DLSS-G on the whole window, because `sl::Extent` is `{top, left, ...}`. That
  also made Streamline ignore the HUD-less image. `fg_game.py status` now
  reports Streamline's own errors.

The current build is v0.11.7 (git tag `v0.11.7`).

Both 21:9 layouts were re-checked with v0.11.7 (game-29, game-30): HUD-less was
accepted on every frame, with zero Streamline errors and a stable HUD.

**Next:** the configuration-based alpha with performance cleanup (including
4K).

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

1. **Start at any resolution.**
   - Done: 1080p, 1440p, borderless, exclusive fullscreen, 21:9 letterboxed,
     21:9 and 16:9 pillarboxed in 5120x1440 (DLSS, DOF and FG).
   - Open: 4K (needs DSR).
   - The arming-time right-edge tint at odd widths: re-check once the alpha
     arms at startup.
   - The live aspect change fails closed. Recovering from it needs a
     controlled stop and restart at the new scene size.
2. **Configuration-based alpha.**
   - Remove the diagnostic readers and overhead, and restore the base frame
     rate.
   - Keep the research build separate.
   - The pixel ceiling is 16,777,216 since v0.11.0, and the DLSS readbacks are
     off in the game.

UI colour+alpha (DLSS-G recomposition) is deferred. HUD-less colour alone gave
a stable HUD, and the game blends its UI in linear space before the brightness
curve, so exact recomposition is not possible
(`frame-generation-hudless-capture-result.md`).

## Installed now: v0.11.7

`mhw_fg_game_probe` is in the MO2 slot. It includes:
- the scene extent for the aspect option;
- the FG scene rectangle (Streamline's field order since v0.11.7);
- the game's own render extent for odd scenes, with the edge-column fill;
- the DOF sequence from the screen size;
- the v0.10.0 lifecycle guard.

The latest sealed run is `local/fg-game-20261006/game-30` (21:9 letterbox,
FG with HUD-less). `fg_game.py stop` also ends the status logging. A run
folder can hold several launches when MHW is restarted for a setting;
`fg_game.py` uses the newest, or `--pid`. Install or restore with
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
