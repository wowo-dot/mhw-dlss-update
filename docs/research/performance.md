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

## Session 20261013-perf1 (v0.23.2, PresentMon 2.6.0)

**Setup:**
- RTX 5090 (driver 610.88), Ryzen 9 9950X3D, 240 Hz display;
- windowed 2560x1440, 16:9, standing in the hub;
- three launches: unmodded, then the mod with FG off (presets switched live),
  then the mod with FG 2x at Quality (frame_generation_auto off);
- the mod used Reflex Boost.

**Steady windows** (frame rates as shown on screen; display intervals):

| Run | Window | fps | p50 / p99 ms | 1% low | GPU busy | CPU busy | Display latency |
|---|---|---|---|---|---|---|---|
| Unmodded, the game's AA | 05:09:30-05:10:28 | 249 | 3.95 / 5.63 | 178 | 3.24 ms | 3.89 ms | 6.95 ms |
| Mod, DLAA | 05:11:46-05:12:28 | 227 | 4.31 / 6.74 | 148 | 4.01 | 4.27 | 8.10 |
| Mod, Quality | menu segment, 63 s | 225 | 4.38 / 7.35 | 136 | 2.89 | 4.31 | 7.01 |
| Mod, Balanced | menu segment, 63 s | 224 | 4.39 / 7.46 | 134 | 2.69 | 4.32 | 6.83 |
| Mod, Performance | menu segment, 62 s | 224 | 4.37 / 7.14 | 140 | 2.54 | 4.32 | 6.68 |
| Mod, Quality, FG 2x | 05:16:44-05:17:50 | 432 (about 220 rendered) | 2.20 / 5.11 | 196 | 2.18 per present | 2.17 per present | 6.91 |

- The mod's own `frame_stats` agree: 224-227 fps with FG off.
- The unmodded 10 s slices varied from 237 to 262 fps while the player stood
  still.
- Captures and summaries (`summary-*.json`) are under
  `local/perf/20261013-perf1/`.

**Findings:**
1. **The game is CPU-bound on this machine.** GPU busy (2.5-4.0 ms) stays
   below the frame time (about 4.4 ms). The presets cut the GPU load by 28%
   (Quality) to 37% (Performance) against DLAA, but the frame rate stays at
   about 224. Slower GPUs gain from the presets; this one does not.
2. **The mod costs CPU time.**
   - CPU busy per frame rises by about 0.4 ms (3.89 → 4.27-4.32 ms).
   - The frame rate drops 9-10% against the unmodded window (249 → 224-227;
     6-14% across the unmodded slices).
   - p99 rises by 1.1-1.8 ms (1% low 178 → 134-148).

   Reflex's report puts the most CPU time in the render submit phase
   (2.1-4.9 ms per frame; simulation 1.7-2.9 ms), where the mod's
   command-list hooks run. This is the first thing to measure with the
   mod's own timing.
3. **DLAA's GPU cost** over the game's own AA: about 0.8 ms of GPU busy
   (3.24 → 4.01 ms).
4. **FG 2x** shows 432 fps at the same display latency as FG off at Quality
   (6.9 against 7.0 ms; Reflex Boost). About 0.8% of the presents were never
   shown. FG adds about 1.5 ms of GPU time per rendered frame (Reflex GPU
   time 2.3-3.5 → 3.8-5.0 ms).

**Next:**
- The mod's own CPU timing: hook time per category per frame, and lock
  waits, to find the extra 0.4 ms and the wider p99.
- GPU timestamps for the passes the mod adds.
- An A/B with Reflex off, to separate Reflex's share.

## Session 20261013-perf2: CPU profile (v0.23.3, game-88)

**Setup:**
- one launch: FG off, Quality;
- `profile = on`: the v0.12 stack sampler, at about 625 samples a second on
  the game's Present thread;
- Reflex Boost for about 2 minutes, then Reflex off for 1.5 minutes.

**Reflex:** Boost and off ran at the same rate (201-203 fps; display latency
7.45 and 7.50 ms). Reflex is not the cost.

**The sampler costs about 10%:** 201 fps here against 225 at Quality in
perf1. Its shares, not its absolute times, are what count.

**Profile** (10 windows, 125k Present-thread samples, `tools/profile_report.py`):
- `ntdll!NtQueryVirtualMemory` is 10.1% of the Present thread. All of it
  comes from `bridge_probe::readable_end`, the region check before each
  bounded game-memory read:
  - 78% under the native queue hook (`mhw_aa_queue_entry`). The hook spends
    88% of its time there: each frame it walks the game's render-unit list
    (`registry+0x15d0`, up to 1,024 nodes, two reads each) to find the TAA
    unit;
  - 20% under the jitter hook (`temporal_jitter_entry`).
- At about 5 ms a frame that is about 0.5 ms, the gap to the unmodded game.
- **Why:** the region cache had one entry per 64 KB block (direct-mapped over
  1 GB, so blocks 1 GB apart collide), each kept for 100 ms. The walk's
  thousand-odd blocks were each queried again every 100 ms, in waves, which
  also fits the wider p99.
- The hooks around game functions (`reflex_boundary_update`,
  `mhw_aa_frame_entry`) carry the game's own work, mostly
  `MonsterHunterWorld.exe+0x225e9f0`, not overhead.

## v0.23.4: a region-level read cache

`readable_end` has a second level: 64 whole regions, each with the time of
its own query.
- A block that misses the first level (expired, never seen, or colliding) is
  answered from its region when that region was queried within the window.
- A miss there queries once; when the page run reaches back to the
  allocation base (a second query from there ends at the same place with the
  same state and protection), the region starts at the allocation base.
- A region therefore costs one or two VirtualQuery calls per 100 ms, not one
  per block.
- The safety contract is unchanged:
  - no answer is used past 100 ms (the first level copies the region's query
    time);
  - every read stays SEH-guarded.
- `dlss_summary` logs `reads {level1_misses, region_hits, queries}`.
- Tests (`perf_tests`): a 4 MB region's 64 blocks cost two queries, also
  after the window, and a region made inaccessible is seen once the window
  has passed.
