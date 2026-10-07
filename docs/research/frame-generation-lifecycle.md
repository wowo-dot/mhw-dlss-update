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

### Phase 1 result (game-11, 2026-10-07)

- **MHW applies resolution and screen-mode changes only on restart.** It
  never resizes or recreates its swap chain to change size in a running game.
  The run folder holds three launches: 2560x1440, 1920x1080, then 2560x1440
  again (windowed), with no hang or crash.
- **At startup** MHW creates the chain once (`create_for_hwnd`: 3 buffers,
  flags 2114, windowed; the chain then reports 6 buffers) and calls
  `ResizeBuffers` 4 times at the same size, the last about 13 s after creation.
  All of these come before arming, so the guard never had to act.
- **While armed** (third launch, about 7.5 minutes at 2x, then a normal exit)
  there were no lifecycle calls at all. FG went Off and On only through the
  usual missing-frame path (`no_ready` 464, `mode_changes` 11, both during
  loading).
- The guard stays in place: it is the documented contract, and it costs
  nothing when no call arrives.

## Phase 2 (replaced): start at any resolution

There is no live size change to recover from. The mod instead has to work at
whatever size the game starts at; until now it had only run at 2560x1440. An
audit of the FG DLL's sources found:

1. **DOF admission accepted only 1440p** (`stream_route.cpp`,
   `dof_native_sequence.hpp`). At other sizes, frames with DOF fell back to
   native, and FG blinked off on them. **Fixed in v0.10.1:** sizes and
   dispatch counts are derived from the screen size (20-pixel tiles from the
   pass 0 shader, half-size blur, the documented group sizes). The CBScreen
   check is derived too, with one ULP allowed on the reciprocals. The first
   mismatch is logged as `dof_native_sequence.miss`.
2. **TAA dispatch rounding** is assumed to be ceil(render/16)
   (`temporal_bindings/inputs.cpp`). It divides exactly at 1440p, so the
   1080p run is its first real check.
3. **4K and other sizes over 4,194,304 pixels** fail at startup: the
   `valid_extent` ceiling, the 80 MiB readback cap and the 400 MiB budget in
   `dlss_sdk/inline.cpp`, plus the ceilings in `temporal_present` and
   `filter_shader.cpp`. The large readbacks only serve diagnostic captures, so
   this goes together with the alpha cleanup.
4. **Display widths that are not a multiple of 32** (3440, 1366, 1680) fail
   at startup. The game rounds its render width down to a multiple of 16, so
   the render size is no longer exactly half. Supporting them needs the game's
   actual render size throughout instead of width/2.
5. **Even sizes are required** (odd widths or heights fail at startup).

### v0.10.1 test (game-12)

Launch at 1920x1080 windowed. Arm at the main menu, play at 2x in an area
with DOF (cutscenes, Gallery, focus-heavy scenes), then exit. Expect:
- DLSS frames at 960x540, with no `temporal_unhealthy` stop;
- `dof_native_sequence.qualified` rising, with `mismatches` at 0;
- HUD-less tagging and the brightness capture as at 1440p.

If any mismatch appears, `miss` gives the observed value. A later launch
should cover borderless and fullscreen at startup:
`render_resolution` requires the game's display size to equal the chain's.

### v0.10.1 result (game-12, 1920x1080)

- **What worked:**
  - DLSS ran at 960x540 → 1920x1080 with no faults, which confirms the
    ceil(render/16) TAA dispatch.
  - FG at 2x tagged HUD-less on every frame, and the brightness was captured.
- **What failed:**
  - In a DOF cutscene, every frame fell back to native: 1,629 plan
    rejections. The user saw DLSS and FG off.
  - The rejection came from dependency discovery for the DOF composition
    (`input_bindings`, qualification 12), before the DOF sequence check.
  - The cause: the first DOF texture, the CoC (engine format 2, 960x540), is
    backed by `R16G16B16A16_TYPELESS` (DXGI 9) instead of the 1440p
    `R16G16B16A16_FLOAT`.
- **Not a 1080p regression as such:** none of the earlier FG runs met a DOF
  scene (zero DOF draws in game-05 to game-11). The only DOF game test was the
  DLSS-only Gallery run at 1440p (v1.25.41, 884 complete sequences).

**v0.10.2** accepts the TYPELESS member of the expected format's family, in
dependency discovery and in the DOF sequence check. This is safe because the
route only samples these textures through the game's own typed views.
- **Diagnostics:** `dependency_route.texture_check` and `first_failure` (the
  first discovery failure, with the texture's role and formats), plus format
  and size misses in `dof_native_sequence.miss`.
- **Fixture:** the DOF route fixture has a typeless-CoC mode
  (`MHW_DOF_TYPELESS_COC=1`). It replays exactly on WARP and on the hardware
  adapter, and fails under the old exact-format rule.

Test (game-13): 1080p, the same DOF cutscene, at 2x.

### v0.10.2 result (game-13, 1920x1080)

- **Dependency discovery now passes** with the typeless CoC.
- **The DOF sequence check refused all 864 cutscene frames** on one value:
  - The TileMaxH engine extent was `0x84380060`, against an expected
    `0x04380060`. The derived 96x1080 size is correct.
  - Owner+0x88 packs a 16-bit width, a 14-bit height and two flag bits. Bit 31
    was set; it accompanies a secondary backing, which the route already
    follows to find the resource.
  - Every other check after that point went unevaluated, because the check
    stopped at the first mismatch.

**v0.10.3:**
- The size check masks the two flag bits, as `temporal_bindings` already does.
- `qualify()` now records the first structural failure (`failed_at`) and
  collects up to eight size, dispatch and CBScreen mismatches instead of
  stopping at the first. The plan's resource format and size checks do the
  same across all nine textures.
- `flagged` lists the roles whose extent carries flag bits.
- The fixture's `MHW_DOF_POOLED=1` mode reproduces both 1080p differences:
  the typeless CoC and bit 31 on roles 2-8.

Test (game-14): the same cutscene at 1080p.

### v0.10.3 result (game-14, 1920x1080): DLSS, FG and DOF work

The user saw DLSS and FG on through the DOF cutscene.
- **DOF:** 795 sequences qualified and 794 completed, all six passes on each,
  with 1,588 adapted dispatches restored. The composition draw counted 794
  times. No rejections, failures or mismatches.
- **DLSS:** 1,106 frames at 960x540 → 1920x1080, with 3 skips and no faults.
- **FG:** 1,230 frames tagged with HUD-less at 2x. It was On through the
  cutscene; the Off periods (`no_ready`) were loading and transitions.
- **A same-size `ResizeBuffers` came during play.** It is the first lifecycle
  call seen while armed, from the host thread. FG was Off at that moment
  (loading), so the guard had nothing to do. The buffer generation advanced
  (7 → 8), and FG came back On afterwards.
- **`fg_stopped` with reason `reflex_frames`** (`reflex_faults` 1) came at
  game exit, after the last present (7,808). Reflex hit one error during
  shutdown; this does not happen during play.
