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

### Borderless and 21:9 (game-15 to game-17)

- **Borderless (game-15)**
  - Same window chain as windowed: 2560x1440, `windowed` true (the borderless
    window), the same `create_for_hwnd` and same-size `ResizeBuffers`.
  - DLSS (1280x720) and FG work, and **DOF at 1440p in the FG build works**:
    289 complete sequences, zero rejections.
- **The aspect-ratio option applies live.** The user switched to 21:9 during
  the game-15 session:
  - MHW called `ResizeBuffers` at the same size with FG On. The guard turned
    FG Off first (`lifecycle_offs` 1).
  - The next render-scale snapshots no longer qualified (`unqualified` 2), so
    `render_resolution` latched fault 4.
  - The temporal runtime turned unhealthy and FG stopped
    (`temporal_unhealthy`). The game continued natively; no hang or crash.
- **21:9 at startup (game-16, game-17, v0.10.4 diagnostics)**
  - The window chain stays 2560x1440, but the renderer draws the scene at
    **2560x1080** (64:27), letterboxed.
  - Startup snapshot: display 2560x1080, `native_width_override` 1, scale
    basis 2560x1440, all scales 1, sizing 0, no wrapper.
  - Admission refuses with check 13 (basis differs from display). DLSS and
    FG stay off and the game stays native.

**What supporting 21:9 (and pillarboxing on 32:9) needs**

- **A scene extent separate from the swap-chain extent.** Discovery,
  `render_resolution`, the stream route and the DLSS output would use the
  renderer's scene size: 2560x1080, rendered at 1280x540.
- **The final composition must place the scene at its offset in the back
  buffer** (here 180 rows of letterbox).
- **FG tags with sub-rectangle extents.** DLSS-G takes an extent per tag:
  the back buffer and HUD-less at the scene rectangle; depth and motion
  vectors at the render size.
- **The live aspect change** is a real in-session size change. That needs
  Phase 2's controlled stop and restart at the new scene size, which is no
  longer moot for this one setting.

### v0.11.0 result (game-18, 2560x1440 window, 21:9): DLSS works

v0.11.0 reports the renderer's scene size and keeps the window size
separately. Both are accepted only when the game's aspect basis explains the
difference.
- **DLSS:** 4,368 frames at 1280x540 → 2560x1080 with no faults. The
  render-scale override ran 7,740 times with 0 unqualified snapshots, and the
  depth extent followed the scene (2560x1080).
- **DOF at 21:9:** 841 qualified and 840 completed with zero rejections, using
  the derived 64:27 sizes.
- **User check:** "played a bit with dof, looked fine".
- **FG** stayed off as designed (`fg_unsupported`: scene 2560x1080, window
  2560x1440).
- **The exit stop** was again `reflex_frames` at shutdown.

**Next (v0.11.1): FG with the scene rectangle.**
- Log where the screen write places the scene: the screen-write viewport at
  list Close, and CBSystem `iRegion` through the capture shader.
- Configure DLSS-G with the window as output, and tag the back buffer and
  HUD-less with the scene rectangle as their extent.
- Have the HUD-less pass write the scene at that offset, with black outside.

### v0.11.1 result (game-19, 2560x1440 window, 21:9): FG works

v0.11.1 gives DLSS-G the window as its output and the 21:9 scene as a
rectangle inside it.
- **Armed with scene `[0,180,2560,1080]`**, i.e. centred with 180 rows of
  letterbox above and below.
- **FG 2x:** 4,523 host frames presented as 2 frames and 4,633 tagged. Zero SDK
  errors, exceptions or camera failures.
- **HUD-less:** tagged on 4,633 of 4,634 recorded frames, with no skips.
- **DOF under FG at 2560x1080:** 764 sequences completed and none refused.
- **Lifecycle:** two mid-session same-size `ResizeBuffers` passed with
  `lifecycle_offs` 0, and the session stopped at exit (`reflex_frames`).
- **User check:** a DOF cutscene with FG "looked fine". No vertical shake, HUD
  ghosting or bleed into the bars was reported.
- **Where the scene lands.** The captured screen write renders into a
  scene-sized intermediate (viewport and scissor `0,0,2560,1080`), and a later
  pass places it in the window. CBSystem `iRegion` carries no position in this
  shader variant: its words hold stale constants, last
  `{2/2560, -2/1080, -1, 1}`, a pixel-to-clip transform for a 2560x1080
  target. The centred rectangle therefore rests on the visual check, which
  passed. The HUD-less image stays scene-sized and is tagged with extent
  `{0,0,2560,1080}`; it is not padded to the window.
- The `no_ready` presents (2,724) follow the earlier runs' pattern of menu and
  loading presents with no tagged game frame (game-14 1,876, game-15 3,655).

**Start at any resolution: status**
- 16:9 at 1080p and 1440p: done.
- Windowed, borderless and exclusive fullscreen at startup: done.
- 21:9 letterbox, DLSS and FG: done.
- **Open:**
  - The 5120x1440 window, at 32:9 (full window) and at 21:9 (pillarbox).
  - 4K, which needs DSR.
  - The live aspect change. It still fails closed; recovery needs a
    controlled stop and restart at the new scene size.

### 21:9 in a 5120x1440 window: an odd scene (game-20 to game-26)

MHW's aspect option offers only 16:9 and 21:9, so a 5120x1440 window always
pillarboxes. At 21:9 the scene is **3413x1440** (1440 x 64/27, truncated),
placed at x = 853.

**How the game sizes it.** The native setter (RVA `0x229bd70`, read from the
decrypted image) quantizes the scale against the config basis, i.e. the
window width, whenever the aspect override flag is set:
`q = (trunc(5120 x 0.5) & ~15) / 5120 = 0.5`. The scene then renders at
**1706x720**, because 3413 x 0.5 = 1706.5 is truncated. No whole-pixel render
size has 3413:1440 proportions, so every exact-2:1 rule had to give way to
"one scale, truncated per axis".

| Build | Run | Result |
| --- | --- | --- |
| v0.11.1 | game-20 | Refused: DLSS geometry required exact 2:1 (`inline extent/device`). |
| v0.11.2 | game-21 | The render plan takes the window basis. DLSS geometry, the effect route and the DOF check accept the truncated half. NGX recommends 1707x720 (it rounds); a one-pixel tolerance accepts the game's 1706. Then refused: the spatial fallback still required an exact proportion. |
| v0.11.3 | game-22 | DLSS and FG ran (3,020 DLSS frames, no extent mismatches). FG stopped at the title screen: the new title view held the scale 1706/3413, and the snapshot range check required at least 0.5. |
| v0.11.4 | game-23 | That scale is admitted, but the title view kept it on every frame. The per-frame stamp still required 0.5, so DLSS skipped and FG had nothing to generate. The view still drew the planned 1706x720 viewport. |
| v0.11.5 | game-24 | The truncated ratio counts as the planned scale. 10,580 DLSS frames with 1 skip; FG stayed on through the title and save screens. |
| v0.11.6 | game-25, game-26 | Edge-column fill (below). game-26: DOF at 3413 wide, 841 sequences completed and none refused; the guessed truncated DOF half (1706) was right. FG 2x presented 1,124 frames as 2, with HUD-less tagged on all of them. |

User checks: "pretty good" (game-22), and DOF cutscenes "looked fine"
(game-25, game-26).

**Known artifact: a right-edge tint after arming mid-session.**
- **What it looks like:** a smooth tint over about the last 40 px of the
  scene's right edge, present at every height. Its colour is whatever was on
  screen when DLSS armed: blue from one main-menu background (game-22),
  orange and teal from others (game-24, game-25). It was invisible when armed
  in the hub (game-23), because the leftover pixels matched the scene.
- **Not FG:** it stayed with FG off and DLSS on (game-25).
- **It clears after a title-screen visit,** where the game recreates its
  render targets, and it did not return in the hub.
- **Cause (inferred):** the game's effects sample to 1706.5 of 3413 columns,
  half a column past what it renders. The texels outside keep the last
  full-size frame drawn before the reduced scale. v0.11.6 copies the last
  rendered colour and motion column into that column each frame; the tint
  stayed. So the leftover pixels sit in another texture, most likely the
  glow/bloom chain, whose smaller levels each keep their own edge and spread
  it over tens of pixels.
- **Next:** the configuration-based alpha arms at startup, before the menu
  renders, so those textures never hold menu colours. Re-check this there
  before chasing individual textures. The fill stays as a defensive measure
  for direct colour and motion reads.

Tooling note: `fg_game.py stop` also ends the periodic status and inputs
logging; DLSS keeps running unlogged. Don't stop FG before a check whose
counters matter, such as DOF.

**Start at any resolution: status**
- 16:9 at 1080p and 1440p: done.
- Windowed, borderless and exclusive fullscreen at startup: done.
- 21:9 letterboxed in a 16:9 window: DLSS and FG done.
- 21:9 pillarboxed in the 5120x1440 window (odd 3413 width): DLSS, DOF and
  FG done; the arming-time edge tint is open.
- **Open:**
  - 16:9 pillarboxed in the 5120x1440 window (2560x1440 at x = 1280).
  - 4K, which needs DSR.
  - The live aspect change. It still fails closed.

### FG sub-rectangle fixed: v0.11.7 (game-27, game-28)

**Every letterbox and pillarbox FG run before v0.11.7 ran DLSS-G on the
whole window, not on the scene.**
- **The bug:** Streamline's `sl::Extent` is `{top, left, width, height}`, but
  the back-buffer tag passed the scene rectangle as `{x, y, ...}`. Every
  letterboxed or pillarboxed rectangle then fell outside the back buffer.
- **What Streamline did with it:** reset the subrect to the full back buffer
  ("Invalid backbuffer resource extent ... Resetting extent to full backbuffer
  resource size"). It then ignored the scene-sized HUD-less image on every
  frame ("HUD-less buffer extent does not match color buffer size"):
  - game-19: 4,523 times;
  - game-22: 1,893;
  - game-26: 1,190;
  - game-27: 7,797.
- **Consequence:** the scene's motion was mapped across the whole window:
  - 1.33x vertically in the 21:9 letterbox;
  - 1.5x horizontally for 21:9 on the 5120x1440 window;
  - 2x with a 1280-pixel shift for 16:9 on the 5120x1440 window.
- **Why it went unnoticed:** only the last case was obvious. The user recorded
  scene content warped into the side bars (game-27). The earlier 21:9 "looked
  fine" checks therefore judged FG without HUD-less and with misplaced motion.
- **Fix (v0.11.7):** the correct field order, with a `static_assert` on the
  `sl::Extent` layout and unit tests for both pillarboxes and the letterbox.
- **Tooling:** `fg_game.py status` and `summary` now count Streamline's own
  errors and warnings from `sl.log`. Our tag counters show only that a tag was
  set, not that DLSS-G used it.

**game-28 (16:9 in the 5120x1440 window, scene `[1280,0,2560,1440]`):**
- **FG 2x:** 1,961 frames presented as 2.
- **HUD-less:** tagged on all 2,250 frames, with **0 Streamline errors**.
- **DOF:** 296 sequences completed and none refused.
- **User check:** "much cleaner, nothing spilled to the black bars".
- **The one remaining sanitize warning** appears once per run, also in
  full-window 16:9 runs (game-15), so it comes from before the first tagged
  frame.

**Both 21:9 layouts re-checked with v0.11.7.** FG now gets the real
rectangle and HUD-less for the first time.

| Run | Layout | Scene | Presented as 2 | HUD-less tagged | Streamline errors | User |
| --- | --- | --- | ---: | ---: | ---: | --- |
| game-29 | 21:9 pillarbox, 5120x1440 window | `[853,0,3413,1440]` | 1,460 | 2,040 | 0 | "hud looked stable" |
| game-30 | 21:9 letterbox, 2560x1440 window | `[0,180,2560,1080]` | 1,363 | 2,182 | 0 | "hud looked stable" |

The windowed 2560x1440 runs log a few flip-metering notes ("FC feedback: 1";
game-19 had 11). They are not errors.

**Start at any resolution: status (v0.11.7)**
- Done, with DLSS, DOF and FG:
  - 16:9 at 1080p and 1440p;
  - windowed, borderless and exclusive fullscreen;
  - 21:9 letterboxed (2560x1440 window);
  - 21:9 pillarboxed (5120x1440 window, odd 3413 width);
  - 16:9 pillarboxed (5120x1440 window).
- **Open:**
  - 4K, which needs DSR;
  - the live aspect change, which fails closed;
  - the right-edge tint at odd widths after arming mid-session. Re-check it
    when the alpha arms at startup.
