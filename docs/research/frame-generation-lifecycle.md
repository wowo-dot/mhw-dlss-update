# Resize and swap-chain lifecycle with FG On (v0.10.x)

Milestone after HUD-less colour (`frame-generation-hudless.md`). Goal: an
in-game resolution change, screen-mode change or swap-chain recreation must
never hang or break the game with DLSS-G active. Later, DLSS and FG should
resume at the new size.

## What the code assumes today (v0.9.3)

- **Presentation layer:** routes the host's `ResizeBuffers` (and
  single-queue `ResizeBuffers1`) through the Streamline proxy, notices new
  swap chains, and keeps its size statistics current. `ResizeTarget` was not
  hooked.
- **Sizes fixed when FG is armed:**
  - the DLSS route: discovery, the NGX feature, the converted inputs, the
    spatial `Output`, `stream_route::prepare`;
  - the render-scale override target;
  - the FG session: `configure` output extent, slot images, `HudlessPass`.
- **Hard-coded 2560x1440:** in two places in `stream_route.cpp` (DOF
  admission, compute-depth diagnostic).
- **Faults:** every fault latches for the life of the process. Read from the
  code (not run), a size change makes `render_resolution` fault. That turns
  the temporal runtime unhealthy, the worker loop ends, and FG is disarmed for
  good while the game falls back to native TAA. Streamline stays loaded.

**NVIDIA's DLSS-G guide** (12.0 and the 17.0 checklist): turn DLSS-G Off
before any window manipulation (resize, maximise/minimise, full-screen
transition). With FG On, DLSS-G presents from its own thread, and the host's
call can deadlock. Its `ResizeBuffers` handling is in the closed `sl.dlss_g`
plugin (before/after hooks registered with the interposer).

## Phase 1 (v0.10.0): observe and guard

- **Lifecycle record.** Every host call to swap-chain creation (desc, hwnd,
  core window), `SetFullscreenState`, `ResizeBuffers`, `ResizeBuffers1` and
  `ResizeTarget` (newly hooked in the FG presentation variant only; it is
  observed and guarded, and the call itself stays native) is recorded with:
  - thread, host-thread and current-chain flags, and whether the call went
    through the SDK proxy;
  - result, duration and arguments;
  - the chain description after the call.

  These go to a bounded ring with its own lock, which the worker drains into
  the run log as `swap_lifecycle`. The SDK's own recursive calls are not
  recorded.
- **Guard.** Before any of these calls is forwarded, the presentation layer
  calls the lifecycle listener on the calling thread, outside the route
  mutex. `game::lifecycle_guard` turns DLSS-G Off (retaining resources) if
  it is On (`lifecycle_offs`). A later Present can turn it On again only
  through the normal path, with valid inputs.
- **Why the worker stops.** `fg_stopped.reason` is one of `stop_request`,
  `duration`, `temporal_unhealthy` (with a `fault_snapshot` of the continuous
  runtime in the input log and status file), `reflex_frames` or
  `presentation`. After FG ends, the worker keeps recording lifecycle calls,
  with a `presentation_state` line on each change, for up to 30 minutes.
- `tools/fg_game.py lifecycle --run <run>` lists the records; the summary
  includes them and the stop reason.

### Phase 1 test

1. Arm at the main menu; play at 2x with FG On and the window focused.
2. Options → Display: change resolution (windowed, e.g. 1920x1080), apply,
   and play briefly. Then return to 2560x1440.
3. Switch the screen mode (borderless, then fullscreen, then back to windowed).
4. Minimise and restore once.
5. Exit normally.

Expected: no hang or crash. The first size change stops DLSS and FG
(`temporal_unhealthy`) and the game continues natively at each size. The
records show what MHW calls, on which thread, and how long each call takes
with FG just turned Off.

## Phase 2 (planned): resume at the new size

A size change becomes a controlled lifecycle stop, distinct from a fault:
1. Turn FG Off and stop the DLSS route, restoring the native path.
2. Wait until the game's renderer and swap chain are stable at the new size.
3. Rebuild the DLSS route: discovery, NGX feature, stream route,
   render-scale target. This also removes the 2560x1440 hard-codes.
4. Recreate the FG session resources and re-arm FG with the new output
   extent.

The details depend on the Phase 1 observations.
