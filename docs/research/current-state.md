# Current state

Updated 2026-10-08.

> This page stops at v0.13.4. The release candidate (v0.28.4) is described in
> the [README](../../README.md); later work is in the dated notes here.

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

**Performance (v0.12.0 to v0.12.6, `performance-v012-result.md`).**
- **Hub:** the armed base rate went from 32 fps to 187-208 fps with FG 2x
  (unmodded: about 210).
- **Training grounds:** from 49 fps to 190-270 fps.
- **The cost was CPU work on the render thread.** The worst items were one
  VirtualQuery per guarded read and a process-wide lock on every command-list
  call.
- **Each fix is a live perf switch.**
- **FG input staging** now survives the game recording a frame's final draw
  before that frame's DLSS (v0.12.6).

**Config alpha (v0.13.0 to v0.13.4, `alpha-v013-result.md`).**
- **`mhw_dlss.ini`** next to the DLL; the session arms itself about 5 s after
  launch; lean paths on; a one-minute summary log.
- **Fixed in the alpha series:**
  - a crash on a Velkhana attack (readiness trace read an unbuilt plan);
  - FG drops on FOV animation, such as the windrake ride (projection resets);
  - DLSS skips in the Fatalis fight smoke: `uDepthColorFilter` now runs at
    full output through a hash-gated shader adapter, bit-exact against the
    native shader in a GPU fixture.

The current build is the alpha v0.13.4 (git tag `v0.13.4`); research builds
are at v0.12.8.

Both 21:9 layouts were re-checked with v0.11.7 (game-29, game-30): HUD-less was
accepted on every frame, with zero Streamline errors and a stable HUD.

**Next:**
1. **Shader census:** dump every post-filter shader and sort them by how they
   address the screen.
2. **Generated adapters:** hash-gated, one fixture per family.
3. **Flexible effect chain:** approved filters at any chain position.
4. **Suspects:** colour-correct LUT, blink, image plane, circular DOF, the
   second TAA, and the Safi and Alatreon radial blurs.
5. **Then:** 4K, and the silhouette drawn at render size.

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
- With every perf switch on (`<dll>.perf` = 895), the armed base rate matches
  the unmodded game. With the DLSS-G plugin loaded and the mod not armed, the
  frame rate is about half the unmodded rate: Streamline's swap chain uses
  frame latency 1. Arming with Reflex removes this.
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
   - Overhead: done in v0.12.x (perf switches; all on in the alpha).
   - The pixel ceiling is 16,777,216 since v0.11.0, and the DLSS readbacks are
     off in the game.

UI colour+alpha (DLSS-G recomposition) is deferred. HUD-less colour alone gave
a stable HUD, and the game blends its UI in linear space before the brightness
curve, so exact recomposition is not possible
(`frame-generation-hudless-capture-result.md`).

## Installed now: v0.12.6

`mhw_fg_game_probe` is in the MO2 slot, with the slot sidecar `<dll>.perf` =
895: every lean path, including keyed FG staging. It includes everything from
v0.11.7 plus:
- the perf switches;
- frame-time windows and the driver report every 5 s;
- the stack sampler (switch `sample_profile`; `tools/profile_report.py`);
- per-frame FG staging.

The latest sealed run is `local/fg-game-20261006/game-36`; game-37 is the
v0.12.6 check (FG stable in the training grounds). `fg_game.py frames` prints
the frame windows, and `fg_game.py perf --set all[,sample_profile]` switches
live. `fg_game.py stop` also ends the status logging. A run folder can hold
several launches when MHW is restarted for a setting; `fg_game.py` uses the
newest, or `--pid`. Install or restore with `tools/mo2_slot.py` (the receipt
in the run folder has the restore command; the linker map is copied in for
the sampler). `<dll>.fg-mode` can contain `off` (run without DLSS-G) or
`2x`..`8x` (the starting multiplier; default 2x). `fg_game.py multiplier
--run <run> --x N` changes the multiplier while armed.

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
