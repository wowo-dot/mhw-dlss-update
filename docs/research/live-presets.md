# Live preset changes (v0.22.0)

Roadmap step 2, part A: change the DLSS preset (DLAA, Quality, Balanced,
Performance) while the game runs, instead of from the next start. Part B, the
live 16:9 / 21:9 change, builds on the same rebuild and comes next.

## What the game does

Nothing is reallocated when the render scale changes:
- The game renders into the top-left corner of full-size targets (its active
  scale).
- `render_resolution` writes that scale (renderer `+0x1f0`) on every visit of
  the game's scale update. The content and allocation scales stay 1
  (dlss-render-resolution-control.md).

A preset change is therefore a new target scale for the game, plus a rebuild
of everything on our side that was sized for the old one.

## What was sized at start

| Piece | Holds | Rebuild |
|---|---|---|
| `render_resolution` target | the planned extent and scale | `retarget()`; the render thread takes one copy per visit |
| `InlineFrame` (DLSS) | NGX feature (render size, output, quality value, preset hint), converted inputs, output | `retire()` then a new one |
| spatial `Output` | its source region | retire, then a new one |
| `scaled_render`, edge flags, `scaled_geometry` | the render extent | set under the runtime lock |
| stream route | `render_w/h`; effect, dependency and silhouette pipelines (geometry baked in) | `sr::resize()` |
| published requests, depths, projections | frames at the old extent | cleared |
| DLSS history | the previous frame | cleared: the next frame resets |
| FG slot and staging depth/motion | render-size copies | `release_inputs()`; recreated at the next record |

The scene-size pieces stay: the raw copies, the HUD-less pass and images, and
FG's scene rectangle.

## The change (`continuous_dlaa::change_mode`)

The worker runs it when the menu asks (`request_mode`; the loop polls
`requested_mode`). Frame generation pauses first: it is disarmed, and the
worker waits until `inputs_settled()`, meaning no slot holds a copy on a list
not yet submitted or one the SDK may still read.

1. **Pause.** `accepting` false and `sr::stop()`. No new frame is admitted.
   - The game's own TAA renders the frames (activation keeps inserting it),
     at the old scale.
   - A frame already claimed by a prepared route plan still writes its output
     through the spatial path, as on any stop.
2. **Drain.** Polled for up to 3 s, until:
   - no TAA publisher is inside its callbacks, and the jitter slot is free;
   - no private recording is leased;
   - our fence has reached `signal_value`;
   - the stream route is idle (`sr::idle()`: no plan prepared, none
     running DOF, no route list active).

   Then each module's `pending` count must be seen at zero once. Every hook
   counts itself before it reads `accepting`, so one zero proves that no
   callback that could still have seen the runtime accepting is running.
3. **Queue barrier.** The worker signals our fence on the game's DIRECT
   queue, under the submission lock that orders `execute_hook`'s signals, and
   waits for it. Queue order proves every earlier submission complete,
   including the game lists that read our images and FG's copies.
4. **Retarget.** `render_resolution::retarget(scale)`. The next visit writes
   the new scale, and the change waits for 4 visits showing it applied.
   - `active_ready()` (32 visits) gates the jitter again, so DLSS resumes about
     32 frames later.
   - Until then, input stamps for old-size frames are refused (skipped, not
     faulted).
5. **Rebuild.**
   - Under the lock: retire the spatial output and the DLSS feature
     (`InlineFrame::retire`: NGX `ReleaseFeature` and `DestroyParameters`,
     proven by the barrier).
   - Create the new feature and output outside the lock, then
     `sr::resize()`.
   - Swap them in under the lock, set the extents, clear the requests and the
     history.
6. **Resume.** `sr::begin()`, `accepting` true. Back in the worker,
   `release_inputs()` drops FG's render-size copies, and FG is armed again.

**Failures:**
- Before step 4 nothing has changed: the runtime resumes as it was
  (`resumed`). A drain timeout, with the game paused or minimized, is retried
  every 2 s, up to 15 times.
- From step 4 on, the runtime stops (fault 47). The game's own rendering
  returns, at its own scale and anti-aliasing.

As install() does, DLAA on a scene whose basis width is not a multiple of 16
runs its just-below-100% extent as Quality.

## A related fix: a DLSS fault returns the game's own rendering

Before v0.22.0, a DLSS fault in the alpha only stopped admitting frames. The
render-scale override kept asking the game for the reduced scale, so the game
rendered at, say, 50% with its own TAA until restart. The worker now calls
`continuous_dlaa::stop()` after a `temporal_unhealthy` stop. That turns off the
render-scale override and the TAA activation, so the game renders at its own
scale with its own anti-aliasing.

## Menu and logs

- **Menu:** "DLSS mode" applies at once while DLSS runs (no "Restart
  needed"); otherwise it applies from the next start. Its help line says the
  picture pauses for a moment.
- **`dlss_mode_change` (worker log):**
  - from, requested and resulting mode;
  - `ok`, `unchanged`, `resumed`, the last phase reached and the reason;
  - the render extent, the barrier's fence value, and the time at the end of
    each phase;
  - for frame generation: paused, `settle_ms`, settled, released;
  - the total time and the retry count.
- **FG status:** `input_releases`, plus the ring's `released` count.

## Tests

- **`render_resolution_contract`:**
  - an invalid retarget keeps the plan;
  - a retarget closes the old input stamps;
  - the next visit writes the new scale, the one after observes it, and the
    stamps follow it;
  - and back;
  - a stopped override refuses a retarget.
- **`frame_generation_game`:** the ring is settled only without recording,
  unsubmitted, tagged or consuming slots, and `release_all()` frees ready ones.

## In game: game-83 (v0.22.0, 2026-10-13)

DLAA, 2560x1440, FG 2x with Auto pause, Reflex Boost. The player went DLAA →
Quality → Balanced → Performance → DLAA from the menu, about 10-15 s apart.
The run ended with `game_exit`, no fault and an empty crash log.

| Change | Render | Total | Phase ends, ms (pause, drain, barrier, retarget, rebuild, resume) |
|---|---|---|---|
| DLAA → Quality | 1696x954 | 188 ms | 0, 16, 16, 47, 172, 172 |
| Quality → Balanced | 1472x828 | 172 ms | 0, 16, 16, 47, 172, 172 |
| Balanced → Performance | 1280x720 | 156 ms | 0, 15, 15, 31, 156, 156 |
| Performance → DLAA | 2560x1440 | 187 ms | 0, 15, 15, 47, 187, 187 |

- **Timing:**
  - FG settled within one tick (`settle_ms` 0);
  - the drain took one tick;
  - the rebuild (retiring the old feature, then NGX creating the new one)
    took about 125 ms.

  The times are `GetTickCount64` ticks, so 15.6 ms steps.
- **DLSS:**
  - kept running with fault 0: frames 1,504 → 4,564 → 8,846 → 12,638 across
    the four 20 s windows;
  - one `first` history reset per change;
  - about 19 skipped frames per change (the jitter gate's 32 visits, less the
    pause).
- **FG:**
  - on again after every change, showing 2 frames per Present;
  - `input_releases` 4 and `released` 3;
  - `size_mismatch` 0, `create_failed` false;
  - its render size followed: 1696x954, 1280x720, 2560x1440;
  - `not_armed` grew by about 20 Presents per change.
- **Back at DLAA:** the whole-copy depth admission ran again on every frame
  (4,201 of 4,202).
- **Streamline:** its two warnings in this run also appear in game-77 and
  game-81, before any change. One is "slDLSSGGetState must be synchronized
  with the present thread": the worker's single state query at `fg_armed`.
