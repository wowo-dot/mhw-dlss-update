# In-game ordinary frame generation: first result

Date: 2026-10-06. Build `mhw_fg_game_probe` v0.7.3 (git tag `v0.7.3`, commit
`5572ba4`), DLL SHA-256 `88a05d16…de23`, host SHA-256 `c2ebbbd2…90ea`,
Streamline 2.12.0, DLSS-G snippet 310.7.0 (FG v2+ model) from the combined
runtime folder. Design: `frame-generation-game-slice.md`. Evidence (private,
sealed): `local/fg-game-20261006/game-05`, manifest SHA-256 `53a6f0ea…8051`.

## Result

DLSS Frame Generation (2x) runs in Monster Hunter: World on the DLSS
Performance path with Reflex On. The user saw about 30 fps doubled to 60 fps,
described motion as smooth, and saw only the minor defects expected from frame
generation.

Confirmed from the logs:

- Startup: DLSS-G supported (`eOk`, 4 required tags), SDK-paced presentation
  with a 6-buffer 2560x1440 chain. Boot timeline normal, no crash records.
- Session (armed at main menu, about 9.5 minutes): 14996 FG-session presents,
  13697 tagged with depth, motion vectors and camera constants. Zero camera,
  constants, tag or option failures; zero SDK errors; zero exceptions. Every
  tagged present used the input recorded one frame earlier. The input ring
  never starved, duplicated or went stale.
- Streamline state: status `eOk`, 2 frames presented per host frame, input
  processing fence advancing (last value 25051).
- 1297 presents had no recorded input (loading screens and other frames where
  the DLSS pass does not run). FG was switched Off for those, as designed.
- Stop turned FG Off (acknowledged); the SDK stayed alive until a normal exit.

## Observations

- **Focus.** DLSS-G interpolates only while the game window is focused. While
  unfocused, Streamline reports 1 frame per present and the fence stays at 0;
  our tagging is unaffected.
- **Re-engagement dropouts.** Each time interpolation resumes, the driver goes
  through a presentation handshake: RSYNC init, independent-flip promotion
  toggling, flip-metering warm-up, maximum frame latency 1↔2. This produces one
  or two 140–190 ms stretches without interpolation within the first second.
  These dropouts come from the driver and Streamline, not from our inputs. The
  NVIDIA profile reports VSync "Force ON".
- **Base frame rate.** Gameplay ran at 27–34 host fps. The DLSS-only v0.6.5
  build measured 24–33 fps in the same scenes, so the low base is overhead of
  the diagnostic builds, not frame generation. Late in the session the rate
  dropped to 2–3 fps (frames over 100 ms); the user attributes that to the
  diagnostic readers. Both must be fixed before the alpha.

## Not verified yet

HUD-less colour and UI alpha (HUD is interpolated as part of the scene),
multi-frame generation, latency, frame pacing measurements, resolution changes,
and visual quality beyond the user's impression.

## Related findings from the same day

- When DLSS-G is loaded, Streamline initializes NGX first and NGX keeps the
  first feature search path. The game's DLSS provider must therefore live in
  the same combined runtime folder (fixed in v0.7.1).
- A Streamline-paced swap chain cannot be torn down under MHW; error paths now
  turn FG Off and keep the SDK alive (v0.7.1).
- Boot crashes at host RVA `0x2244930` with any build were caused by Steam
  having been relaunched through MHW's own restart path. A normal Steam restart
  fixed them; no code change was involved.
