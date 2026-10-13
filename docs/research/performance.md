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

## Session 20261013-perf3 (v0.23.4, game-89): the region cache in game

Same spot; FG off; Reflex Boost; the profiler off.

| Run | fps | 1% low | p50 / p99 ms | CPU busy | GPU busy |
|---|---|---|---|---|---|
| Quality, v0.23.2 (perf1) | 225 | 136 | 4.38 / 7.35 | 4.31 ms | 2.89 ms |
| **Quality, v0.23.4** (05:52:37-05:53:52) | **234** | **162** | 4.20 / 6.15 | 4.14 | 2.86 |
| DLAA, v0.23.4 (menu segment, 94 s) | 228 | 152 | 4.29 / 6.60 | 4.24 | 3.97 |
| Unmodded (perf1) | 249 | 178 | 3.95 / 5.63 | 3.89 | 3.24 |

- At Quality: +4% fps, p99 down 1.2 ms, 1% low up 19%. CPU busy is 0.25 ms
  above the unmodded game (it was 0.42 ms).
- DLAA is unchanged: at about 4 ms of GPU work per frame the GPU is now near
  the limit.
- **The counters:** in a steady 20 s window, about 57k first-level misses,
  36k region hits and 40k queries, so about 8.5 VirtualQuery calls a frame
  (2,000 a second). About a hundred hot regions each expired every 100 ms
  and were queried twice, and 64 entries were too few.

**v0.23.5:**
- An expired region is refreshed with one query from its base (`refreshes`
  in the counters).
- The second level keeps 256 regions.

## Session 20261013-perf4 (v0.23.5, game-90): profile after the region cache

Quality; FG off; Reflex Boost; `profile = on`.
- **Frame rate under the profiler:** 225-230 fps (perf2, v0.23.3, same
  setup: 201-203).
- **Reads:** about 4 VirtualQuery calls a frame, almost all one-query
  refreshes (about 900 a second); 18k per 20 s window, against 40k in perf3
  and far more before.
- **Present thread, 75k samples:**

  | | v0.23.3 (perf2) | v0.23.5 (perf4) |
  |---|---|---|
  | NtQueryVirtualMemory (leaf) | 10.1% | 1.8% |
  | Native queue hook (outer) | 9.0% | 1.8% |
  | Jitter hook (outer) | 2.1% | 0.8% |
  | The mod's own code (leaf in the mod) | - | 0.8% |

  - The leaf-in-the-mod share splits into: bounded reads 0.18%, stream route
    0.23%, continuous_dlaa hooks 0.20%, other 0.2%.
  - The SHA-256 shader checks are about 0.6% (inner).
  - The mod's own CPU time on the Present thread is therefore about 3-4%,
    or 0.15 ms of a 4.4 ms frame.
  - Everything else under the mod's hooks is the game's own work (main
    update, frame build) or the driver's (Execute, Present, Reset) passing
    through.
- **Remaining gap:** at Quality, 234 fps (perf3) against an unmodded window
  of 249, but the unmodded slices ranged 237-262 standing still. The
  remaining difference is close to the launch-to-launch spread, so a
  back-to-back A/B in one PresentMon session decides it.

## Session 20261013-perf5: back-to-back A/B (v0.23.5)

One PresentMon session: unmodded, then the mod (Quality, FG off, Boost,
profiler off). Both launches show two standing plateaus:

| Plateau | Unmodded | v0.23.5 | Difference |
|---|---|---|---|
| First, after loading | 241 fps (CPU busy 4.02 ms) | 208 (4.67) | -14% |
| Settled | 257-266 (3.64-3.80) | 229-232 (4.18-4.23) | -11% |
| 1% low, settled | 183-196 | 150-155 | |
| GPU busy | 3.2 ms | 2.86 ms | |

The A/B shows a cost of 0.4-0.6 ms of CPU a frame, more than the profile's
own-code share (about 0.25 ms). The rest is probably work the mod causes
inside the driver and the game (extra submissions, DLSS's CPU side, Reflex
markers), which the sampler attributes to the game or the driver.

## v0.23.6

1. **The region refresher.**
   - A thread (every 20 ms) renews, from its base, each region a reader took
     within the last second once its answer is 40 ms old.
   - Hot regions therefore stay within the 100 ms window, and the render
     thread rarely queries. A region that changed or is no longer readable
     is dropped, and its next reader queries it.
   - `reads.background_queries` counts the refresher's queries.
2. **SHA-256.**
   - One BCrypt provider for the process; one was opened for every hash
     before.
   - A memo of 32 inputs (256 B-256 KB) with their hashes, answered only for
     byte-identical input. The stream route hashed the same overlay and haze
     shaders on every plan build, that is, every frame.
3. **`perf_ab = on`** (diagnostic; needs FG off):
   - DLSS alternates every 30 s between running and paused (the live
     rebuild's pause: every hook stays installed and the game renders
     natively);
   - each switch goes to the plugin log as `perf: dlss = paused (applied
     now)`, which `presentmon_summary.py --log` cuts on.

   One launch then separates the per-frame DLSS work from the fixed cost of
   the installed hooks, in the same scene.

Tests:
- `perf_tests`: the refresher (renews a used region, its queries are its own,
  answers past the first window, drops an inaccessible region), and the
  memo's hashes;
- `alpha_config_tests`: `perf_ab` and its FG refusal;
- `test_presentmon_summary`: the `perf:` lines.

## Session 20261013-perf6 (v0.23.6, game-91): perf_ab and unmodded

One PresentMon session. The mod launch had `perf_ab = on` (Quality, FG off,
Boost; nine switches 30 s apart); then an unmodded launch.

| Steady phases | fps | CPU busy | p99 | 1% low | GPU busy |
|---|---|---|---|---|---|
| DLSS running (4 phases) | 236-237 | 4.07-4.09 ms | 5.71-5.84 ms | 171-175 | 2.88 |
| DLSS paused, hooks installed (3 phases after the first) | 244 | 3.96-3.97 | 5.56-5.64 | 177-180 | 2.26 |
| Unmodded, settled (06:49:26-06:50:56) | 260-263 | 3.68-3.73 | 5.18-5.38 | 186-193 | 3.19 |

- **DLSS's own work:** 0.11 ms of CPU a frame (237 against 244 fps).
- **The installed hooks while DLSS is paused:** 0.27 ms a frame (244
  against 261 fps). This is now the larger part.
- **Read-path queries:** about 700 in the whole session. The refresher made
  326k, off the render thread.
- **The mod's code on the other threads** (the game's command-list
  recorders), from game-90's rotation samples:
  - the binding observer's hooks (`after`, `address_call<13>`, `find`,
    `table_call<7>`, `Bindings::descriptor`) lead, with SRW-lock activity;
  - each observer call takes two atomic adds on one process-wide `pending`
    counter, a per-list lock and a state update;
  - the rotation spreads samples over about a hundred threads, so these
    shares understate the cost on the few recording threads.

## v0.23.7: hook timing

`hook_timing = on` (diagnostic) times every 64th hooked call of a thread with
the time-stamp counter: the hook's own time (entry to exit, less the
forwarded native call) and the forwarded call itself.
- **Categories:** observer, reset/close, execute, barrier, draw, indexed,
  dispatch, copy.
- **Output:** a `hook_timing` event every 20 s, with estimated calls per
  frame and own and forwarded microseconds per frame
  (`perf/hook_timing.hpp`).

## Session 20261013-perf7 (v0.23.7, game-92): hook timing

`perf_ab` and `hook_timing` on (Quality, FG off, Boost). Steady 20 s windows,
per frame (time summed over all threads):

| Hook | Calls | Own time | Forwarded (native) |
|---|---|---|---|
| **Binding observer** | **~22,000** | **~4.4 ms** (~200 ns a call) | ~1.05 ms (~48 ns a call) |
| Indexed draw | ~2,600 | ~0.2 ms | ~0.14 ms |
| Reset/Close | ~32 | 0.04-0.32 ms (more while DLSS runs) | ~0.15 ms |
| Dispatch (DLSS recording when running) | ~70 | 0.06-0.26 ms | - |
| Barrier | ~150-190 | ~0.1 ms | ~0.23 ms |
| Draw | ~140 | 0.01-0.05 ms | ~0.04 ms |
| Execute | ~8 | ~0.01 ms | ~0.18 ms |

The observer's own time is four times the D3D12 calls it wraps. Spread over
the recording threads, it is how 0.27 ms reaches the critical path.

**Its per-call costs:**
1. Two atomic adds on one process-wide `pending` counter, from every
   recording thread.
2. Large clears in `Bindings`, which mirrors two root signatures of 64
   arguments, each with 64 constants (about 36 KB):
   - `root()` assigned `{}` to the 18 KB side on every signature change;
   - `descriptor()` cleared a 280-byte argument on every call;
   - `reset()` assigned `{}` to all 36 KB;
   - a heap change cleared every table argument.

## v0.23.8: a lean observer

- **`perf::ShardedCount`:** an in-flight count with one cache-line shard per
  thread. It keeps the drain contract: observing every shard at zero once
  after clearing the flag. The observer's and continuous_dlaa's `pending`
  use it.
- **`Bindings`:**
  - `RootBindings::set` marks the set arguments, and a signature change, a
    Reset or a heap change clears only those;
  - `descriptor()` no longer clears the constants (`mask` says which are
    valid);
  - equality compares kind, address, mask and the masked constants.
- **Tests:** sparse clears and `set` (`temporal_present_bindings_tests`); the
  sharded count under eight threads (`perf_tests`).

## Session 20261013-perf8 (v0.23.8, game-93): the lean observer in game

| Steady phases | fps | CPU busy | p99 | 1% low |
|---|---|---|---|---|
| DLSS running | 240-242 | 4.00-4.02 ms | 5.50-5.57 ms | 179-182 |
| DLSS paused | 247 | 3.92 | 5.36-5.39 | 185-186 |
| Unmodded (perf6) | 261 | 3.70 | 5.2-5.4 | 186-193 |

- **The observer:** 4.4 ms of CPU a frame became 2.4 ms (about 110 ns a
  call at about 22,000 calls).
- **1% lows** now nearly match the unmodded game.
- **The gap:** -7.7% running (it was -11 to -14%), -5.5% paused.

## The observer bench (`observer_bench`, not a ctest)

WARP command lists, native and with the observer installed (MinHook):

| Threads (one list each, draw-like mix) | Overhead a call |
|---|---|
| 1 | 21 ns |
| 2 | 23-24 ns |
| 4 | 24-25 ns |
| 8 | 26-29 ns |

- Hook timing inside the bench reports 22-24 ns of own time a call, so the
  in-game timing is accurate. The game's ~110 ns is real and specific to the
  game.
- **Candidates:**
  - the call mix (root constants, vertex buffers, root-signature changes);
  - the per-thread list cache missing when MHW's job threads move between
    lists, so each lookup searches the 128-entry table.

## v0.23.9: the observer's timing split

- `hook_timing` splits the observer into root, table, constants, view,
  raster, predicate and the rest (PSO, heaps, ClearState, bundles).
- It also counts the list-cache misses and the entries searched
  (`counters_per_frame`).

## game-94 (v0.23.9): the observer's timing split

Steady windows, per frame:

| Calls | Count | Own ns a call | Forwarded ns a call |
|---|---|---|---|
| Root CBV/SRV/UAV | ~9,900 | ~120 | ~20 |
| Descriptor tables | ~7,400 | ~90 | ~27 |
| Raster (topology, viewports, scissors, buffers, targets) | ~3,300 | ~110 | ~54 |
| ClearState, PSO, heaps, bundles | ~1,030 | ~78 | ~270 |
| Root signatures | ~310 | ~155 | ~60 |
| Root constants | 1-3 | ~150 | ~165 |

- **The list cache:** about 63 table searches a frame, about 1,670 entries
  compared. It is not the cause.
- **The cost:** a flat 70-100 ns a call above the bench (about 21 ns), in
  every call type. It lies in what every call shares: four locked
  instructions (the in-flight add and subtract, and the entry lock's acquire
  and release). In the bench's loop they are cheap. In game each waits for
  the store buffer, which the driver has just filled recording the forwarded
  call.

## v0.23.10: no locked instructions on the observer's path

- **`perf::InFlightCount`** replaces the sharded count.
  - Each thread has a slot written with plain stores, with a compiler fence
    before the admission flag is read.
  - The drainer calls FlushProcessWriteBuffers once, after clearing the
    flag, and then polls (the asymmetric Dekker pattern).
  - `perf_tests` races eight threads against 200 drains. No admitted
    callback is still inside after one.
- **`perf::owner_lists`** (bit 1024, in `all_toggles`): the observer takes
  no per-list lock.
  - D3D12 lets one thread at a time record a list, and every reader of an
    entry is a hook on that thread (checked: continuous_dlaa, the stream
    route, DOF, silhouette, FG `closing`).
  - Only `forget()` touches a released list's entry, under both locks.
  - `fg_game.py` knows the bit (`LEAN_MASK` 1919).
- **The bench:** 6.5-11 ns of overhead a call for 1-8 threads (21-29
  before).

## Session 20261013-perf9 (v0.23.10, game-95)

`perf_ab` and `hook_timing` on, Quality, FG off, Boost. DLSS fault 0; clean
exit.

**Hook timing, steady windows, per frame:**
- the observer: about 22,000 calls at about 39 ns of own time each, 0.85 ms
  in all (2.4 ms in v0.23.8, 4.4 ms in v0.23.7);
- indexed draws: 80-100 us (160 before);
- all hooks: 1.1-1.5 ms (2.7-3.2 before).

**Frame rate:**

| Steady phases | fps | CPU busy | 1% low | p99 |
|---|---|---|---|---|
| DLSS running | 242-245 | 3.94-3.99 ms | 175-179 | 5.57-5.70 ms |
| DLSS paused | 246-248 | 3.90-3.93 | 181-182 | 5.49-5.53 |

- The observer's remaining time on the recording threads is mostly off the
  critical path: cutting it by two thirds gained about 2 fps.
- **The day's progress at Quality** (hub, RTX 5090, CPU-bound):

  | | Morning (v0.23.2) | Now (v0.23.10) | Unmodded (perf6) |
  |---|---|---|---|
  | fps | 225 | 242-245 | 261 |
  | 1% low | 136 | 175-179 | 186-193 |
  | p99 | 7.35 ms | 5.6-5.7 ms | 5.2-5.4 ms |

- **The remaining gap** (about 6%, with DLSS paused about 5%) mixes:
  - what the mod changes in the game (TAA forced into the queue, the
    render-scale override);
  - Reflex's per-frame calls;
  - the hooks on game functions;
  - launch-to-launch variance (an unmodded launch's own slices ranged
    241-266 fps).
