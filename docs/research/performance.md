# Performance check (roadmap step 3)

Goal: measure what the mod costs and gives, against the unmodded game and the
game's built-in DLSS. Other mods are out of scope.

## Tools

- **`tools/perf/presentmon_capture.ps1`** runs the Intel PresentMon console
  application on `MonsterHunterWorld.exe`.
  - It asks for administrator rights once (PresentMon needs them for ETW) and
    continues in an elevated window.
  - Each press of the hotkey (default Alt+Shift+F11) records 30 s into a new
    CSV under `local/perf/<session>/`. `session.json` records the PresentMon
    version, the hotkey and the start time.
  - It writes 2.x metrics, wall-clock start times (`--date_time`) and Reflex
    PC latency (`--track_pc_latency`).
  - It prefers a copy of the console exe under `local/tools/presentmon-*/`
    (from the GameTechDev/PresentMon GitHub release) to the installed one,
    newest version first.
- **`tools/perf/presentmon_summary.py <folder>`** prints one row per capture:
  - duration and presents;
  - presented and displayed frame rates;
  - 1% and 0.1% lows (1000 / the 99th and 99.9th percentile display
    interval);
  - p50, p99 and maximum display interval;
  - GPU and CPU busy;
  - PC latency;
  - dropped frames.

  `--json` saves everything, including frame types and the first and last
  start times.
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
- **Recordings:** 30 s each, after the frame rate settles. Two recordings per
  scenario, so variation shows.
- **Runs:**
  1. **Unmodded:** the mod disabled in MO2; the game's own AA at its usual
     setting.
  2. **The game's built-in DLSS:** the mod disabled; the game's DLSS on at
     each quality it offers.
  3. **The mod:** DLAA, Quality, Balanced and Performance with FG off, then
     FG 2x at Quality. The presets change live, so this is one launch.
- **Next:** the mod's own GPU timestamps for the passes it adds, and the CPU
  time in its hooks (a separate build).
