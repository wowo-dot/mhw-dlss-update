# Overhead cleanup and FG staging: v0.12.0 to v0.12.6

Runs game-31 to game-37, 2026-10-08. Windowed 2560x1440 at 16:9, DLSS
Performance (1280x720), FG 2x, Reflex On. RTX 5090; display 240 Hz.

## Result

| Place | Unmodded | v0.11.7 / v0.12.0 all switches off | v0.12.6 |
|---|---|---|---|
| Hub, starting camera | about 210 fps (user, Steam overlay) | 32 fps base | 187-208 fps base |
| Training grounds | not measured | 49 fps base (v0.12.2, FG off) | 190-270 fps base, FG on every frame |

Base fps is host frames (FG doubles it on screen). With every switch on, the
armed base rate matches the unmodded game at the hub's starting camera.

The user saw no stalls or visual change from the switches, and FG stayed on
in the training grounds with v0.12.6 ("looked stable throughout").

## How it was measured

- **Host frame intervals:** a 0.25 ms histogram of game Present intervals per
  5 s window (fps, p50/p95/p99, max, frames over 50 ms), before arming, while
  armed and after FG stops (`frame_stats`).
- **Driver report:** while armed, Reflex's per-frame report (simulation,
  render submission, present, driver, queue, GPU active) every 5 s
  (`latency`).
- **Live switches:** each fix is a perf switch (`src/perf/toggles.hpp`) with
  the original fail-closed contract, toggled with `fg_game.py perf --set` (and
  `<dll>.perf` at startup), so one session compares them in place.
- **Stack sampler** (v0.12.2): about 1 kHz on the host Present thread, plus
  rotating other threads, with unwinding against pre-read exception tables
  while the thread is suspended (`src/perf/sampler.cpp`,
  `tools/profile_report.py`).

PresentMon needs administrator rights on this machine, so it was not used.

## What cost the frame rate

All of it was CPU work on the game's render thread: no GPU readbacks or fence
waits on the frame path. With all switches off, the hub frame was 31 ms, with
28.5 ms of render submission and 3.6 ms of GPU work.

| Run | Switch or fix | Hub base fps | Render submission |
|---|---|---|---|
| game-31 | none (as v0.11.7) | 32 | 28.5 ms |
| game-31 | quiet_status | 32 | 28.2 ms |
| game-31 | + cached_reads, cached_methods (VirtualQuery per field; 128 KB stack fill + module lookup per method check) | 110 | 6.9 ms |
| game-31 | + list_locks (one observer lock per command list) | 112 | 5.3 ms |
| game-31 | + packet_filter, no_viewport_trace, no_output_sentinel | 112 | 5.3 ms |
| game-34 | 64 KB-block region cache, pooled_selectors | 208 | 2.6 ms |
| game-35 | shared lock-free region cache | 187 | 3.2 ms |

The hub numbers differ a little between launches with the scene.

**Training grounds** (silhouette path, heavier native render queue):

| Run | Base fps | Render submission | NtQueryVirtualMemory share of the Present thread |
|---|---|---|---|
| game-33 (v0.12.2) | 49 | 15.6 ms | 42% |
| game-34 (v0.12.3) | 152 | 4.8 ms | 42% |
| game-35 (v0.12.4) | ~190 | 1.0 ms | 5.7% |

The 8-entry and then 256-entry per-thread caches thrashed on the native
activation queue walk (up to 1,024 nodes per frame). The shared cache has
16,384 direct-mapped entries, one per 64 KB block, as seqlocks; perf_tests
checks it under contention.

## Unarmed with the DLSS-G plugin

With the DLSS-G plugin loaded, the hub ran at 105-120 fps before arming, in
every build. Without the plugin (`<dll>.fg-mode` off, game-32) it ran at
205 fps, the unmodded rate.

Streamline replaces the swap chain when the plugin loads: 6 buffers instead
of 3, and maximum frame latency 1. Armed, with Reflex pacing, the base rate is
back at the unmodded level, so this only affects the time before arming. The
alpha arms at startup and should not load the plugin when FG is off.

Separately, the NVIDIA driver profile had V-Sync forced on. That held the
unarmed hub at 240/3 = 80 fps in the first game-31 launch. The user turned it
off for later runs.

## FG input staging (v0.12.5, v0.12.6)

Once the render thread got fast (game-35), FG switched off on most frames in
heavy views of the training grounds. The user's overlay alternated between FG
and DLSS, depending on where the camera looked.

- **The original design:** the FG session staged each frame's DLSS inputs in a
  single slot and took them at that frame's final-scene draw.
- **What happened in heavy views:** a worker recorded frame f's final draw
  before the render thread recorded frame f's DLSS.
  - The GPU order is unchanged: the final draw samples the DLSS output.
  - In the single-slot design, the final draw found frame f-1 and gave up
    (`final_mismatch`: up to every frame).
  - game-36's `mismatch_delta` was +1 on every mismatch, and DLSS
    reconstructed 38,916 of 38,918 claimed frames.
- **The fix**, perf switch `keyed_staging`:
  - DLSS's converted depth and motion are rewritten every frame, so stage()
    now copies them on the DLSS list into one of three per-frame staging
    copies.
  - A frame-keyed queue matches each final draw to its own frame (v0.12.5).
  - A final draw that comes first copies from its frame's staging copy into
    an FG slot acquired unconfirmed. The frame's stage() then confirms the
    slot and supplies the camera; the input ring presents confirmed slots only
    (v0.12.6).
- **game-37:** 0 mismatches in 22,029 training-grounds frames. 381 early final
  draws occurred; 380 were confirmed, and one missed its Present and was
  released.

## Open

- About 18 fps of the remaining hub cost is DLSS and FG work itself; the
  remaining CPU share of the mod on the Present thread is about 10%. The
  biggest remaining items are SHA-256 shader checks, plan rebuilding and the
  Execute, Reset and Present hooks.
- The partly blocked hunter silhouette is drawn at render size (beta backlog).
- One training-grounds window in game-37 had 244 host frames without a
  final-scene record at all (no FG slot acquired); the cause is unknown, and
  the user saw nothing.
- The alpha: a config file, arming at startup, diagnostics compiled out, and
  the DLSS-G plugin only with FG enabled.
