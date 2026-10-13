# Performance check (roadmap step 3)

Goal: measure what the mod costs and gives, against the unmodded game. The
game's built-in DLSS cannot be used for the comparison (the user, 2026-10-13).
Other mods are out of scope.

## Tools

- **`tools/perf/presentmon_capture.ps1`** runs the Intel PresentMon console
  application on `MonsterHunterWorld.exe`.
  - It asks for administrator rights once (PresentMon needs them for ETW) and
    continues in an elevated window.
  - It records continuously until Ctrl+C, one CSV per game launch
    (`--multi_csv`), under `local/perf/<session>/`.
  - It writes 2.x metrics, local wall-clock start times (`--date_time`) and
    Reflex PC latency (`--track_pc_latency`).
  - It prefers a copy of the console exe under `local/tools/presentmon-*/`
    (from the GameTechDev/PresentMon GitHub release) to the installed one,
    newest version first.
  - **No hotkey.** On 2026-10-13 PresentMon saw the game but Alt+Shift+F11
    did nothing while MHW had focus, so the hotkey was dropped.
- **`tools/perf/presentmon_summary.py <folder>`** prints one row per capture:
  - duration and presents;
  - presented and displayed frame rates;
  - 1% and 0.1% lows (1000 / the 99th and 99.9th percentile display
    interval);
  - p50, p99 and maximum display interval;
  - GPU and CPU busy;
  - PC latency;
  - dropped frames.

  It cuts the captures with:
  - `--slices N`: consecutive N-second slices;
  - `--log mhw_dlss.log`: the stretches between the mod's live menu changes,
    skipping `--settle` seconds after each;
  - `--window HH:MM:SS HH:MM:SS`: one interval.

  `--json` saves everything.
- **The mod's own logs:**
  - `frame_stats` gives the rendered frame rate and frame-time percentiles,
    and `latency` gives Reflex's latency report;
  - `mhw_dlss.log` timestamps every menu change, so captures line up with the
    settings active at the time.

With frame generation, PresentMon counts every present, generated or
rendered; the rendered rate comes from `frame_stats`.

## Method

- **Settings:**
  - same place and camera, for example the Astera hub start position, not
    moving;
  - windowed 2560x1440, 16:9, the game window focused (DLSS-G only generates
    with focus);
  - V-Sync off in the game and in the NVIDIA profile.
- **Recordings:** about 60 s per scenario, standing still after the frame
  rate settles. The summary cuts the scenarios by time (menu changes, or
  slices).
- **Runs** (the same graphics settings in all of them):
  1. **Unmodded:** the mod disabled in MO2 (Astera Loader stays).
  2. **The mod, FG off, Reflex Boost:** DLAA, Quality, Balanced and
     Performance, switched live in one launch.
  3. **The mod, FG 2x, at Quality.** Turning FG on or off applies only from
     the next start, so this is its own launch.
- **Next:** the mod's own GPU timestamps for the passes it adds, and the CPU
  time in its hooks (a separate build).
