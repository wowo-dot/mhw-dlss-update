# Live aspect change (part B, research)

Roadmap step 2, part B: recover from the game's live 16:9 / 21:9 switch
instead of stopping DLSS. Part A, live presets (live-presets.md), built the
rebuild this needs. The scene size also changes here, and so do the targets
the game draws into.

## What the game does (clean image, IDA, 2026-10-13)

Names are Fexty's where they exist. Offsets are in `sRender` (the renderer,
`image+0x51c4480`) unless stated.

- **The aspect mode:** `+0x7b440` is requested and `+0x7b43c` applied, with
  0 the window's own, 1 16:9 and 2 64:27. Applying it uses ratios of
  1.7777778 and 2.3703704 (64:27: 2560x1080 in a 2560x1440 window).
- **Applying it: `0x229ef20` (named `sRender::finishScreenShot`).**
  - It runs on the render thread, from the frame-begin function `0x22980b0`
    (call at `0x22983f5`), which also calls the scale update we hook
    (`0x22982e7`).
  - When requested ≠ applied, under the renderer's lock, it:
    - copies requested to applied;
    - fits the window size (`+0x1f448`/`+0x1f44c`) to the ratio and writes the
      scene size `+0x198`/`+0x19c`;
    - writes the ratio to `+0x7b444`;
    - writes the scene rectangle in the window (`+0x188..+0x194`, via
      `0x229f090`; also used by the GUI);
    - sets flag 4 in the pending-change word `+0x1f548`.
- **Consuming the flags: `0x229b020`, on the game thread** (call at
  `0x22e4be4` in the game loop `0x22e4ae0`). With flags pending it waits for
  the render thread (`0x229cbf0`, `WaitForSingleObject`), clears the flags,
  and then:
  - **Without flag 0x40:** if the backend has a pending resize, or in
    exclusive full-screen with flag 4, it recreates the views. `0x229c880`
    presents, waits for the GPU and releases the views and target objects
    (`+0x1f4d0`, `+0x1f4e0`, `+0x1f4d8`, `+0x41b90`); `0x23ac560` resizes
    the backend (the same-size `ResizeBuffers` seen in game-15); `0x229b9a0`
    recreates them; `0x2293f10` creates the target set at the scene size.
  - **With flag 0x40:** the target set is resized in place or recreated
    (`0x2298db0`, then the renderer's virtual `+0x28`, then `0x2293f10`).
  - Then it publishes the scene size to the camera
    (`qword_1451C4400 + 0xd58/0xd5c`) and the scene rectangle.
- **The `+0x2cda` flag in `sAppExt` is not the aspect option.**
  `sAppExt::ctor` always sets it to 1 on PC. It makes the window take its size
  from the display config, and the render-scale basis (`+0x2cd0`/`+0x2cd4`) is
  then the window's client size (`0x22e5310`, window creation).

**Unknown until seen in game:**
- the order of these steps across the render and game threads, frame by frame;
- whether the window-mode path recreates the targets (no flag 0x40, no
  backend resize pending);
- whether the content scale (`+0x200`, our check 16) moves.

## The observation build (v0.22.1)

- **`render_resolution` captures the switch.** Each visit's snapshot now
  also reads the window size, the requested and applied aspect, the
  pending-change flags and the scene rectangle.
- **A visit is notable when:**
  - requested ≠ applied;
  - the scene or window size differs from the previous visit;
  - the sizing bit is set;
  - or it is the first refused snapshot.
- **Each capture** holds the 16 visits before its first notable visit and the
  240 from it, with QPC times. There are two per session: the switch and the
  way back.
- **The worker writes them** as `aspect_capture` events: in the loop, after a
  DLSS fault (waiting up to 3 s), and in the post-fault loop.
- **The switch still stops DLSS** (fault 4), but since v0.22.0 the game then
  renders natively at its own scale.

## game-84 (v0.22.1, 2026-10-13): the switch, visit by visit

The run used DLAA, a 2560x1440 window and 16:9 → 21:9 from the game's
options. Capture 0 holds 256 visits:
- **Visits 0-15:** 16:9 running: scene 2560x1440, aspect [1,1], flags 0,
  scales 1.
- **Visit 16 (frame 6862):** aspect [2,1], the request pending; nothing else
  has changed yet.
- **Visit 17 (frame 6863), 119 ms later:**
  - aspect [2,2], scene 2560x1080, window still 2560x1440;
  - refused with check 15: the scene differs from our plan;
  - flags 0 (set and consumed between our visits);
  - sizing 0; content scales 1; override 1, basis 2560x1440.

  The 119 ms is the game recreating its targets (the "stop" frame-stats
  window's maximum interval is 119,199 µs).
- **From visit 18 on:** the same, every visit.

**Other results:**
- **No second capture:** the post-fault windows show no second stall, so
  there was probably no switch back.
- **Fault 4 stopped DLSS; FG stopped (`temporal_unhealthy`).** The game ran on
  at its own scale (the player saw a clear image; at DLAA the scale was 1
  anyway).
- **The rectangle read 0:** `pb::read` reads scalars only, so v0.23.0 reads
  it as four ints. The depth size reads 0 because the snapshot stops before
  the depth read at check 15.

## The recovery (v0.23.0)

1. **`render_resolution` yields.** A refused snapshot with check 15 is the
   game's own aspect switch when four things hold:
   - the requested aspect equals the applied one;
   - not mid-resize;
   - the window is the discovered one;
   - the scene is exactly the window fitted to the applied ratio, the way
     `0x229ef20` computes it (16.F/9.F or 64.F/27.F).

   Then the override stops (`requested` false) without a fault, and
   `scene_changed()` reports it. That refused visit wrote nothing, so the
   game renders at its own scale from there. Input stamps refuse, so DLSS
   skips frames without a fault. Any other check-15 refusal still faults.
2. **The worker runs `change_scene()`** when it sees the change, retried
   every 500 ms while not ready, up to 40 times. Frame generation is paused
   and settled as for a preset change.
3. **`continuous_dlaa::change_scene()`** is the preset rebuild (`rebuild()`)
   with the scene:
   - it rediscovers the scene (`pb::discover`; the device, swap chain, window
     and format must be the same; discovery refuses mid-resize, which counts
     as "not ready");
   - it pauses, drains and raises the queue barrier as for a preset change;
   - `render_resolution::rebase()` plans the configured preset on the new
     scene and basis, then resumes the override;
   - it retires the DLSS feature and the spatial output;
   - it reallocates the three scene-size copies;
   - it creates the feature and output at the new scene;
   - `sr::resize()` takes the new render and output extents (the diagnostic
     depth readbacks are disabled when the output changes);
   - `runtime` becomes the rediscovered scene.
4. **Frame generation:** `release_inputs(true)` also drops the HUD-less pass
   and images; `configure()` takes the new scene rectangle, centred in the
   window; it is then armed again.
5. **Failures:**
   - before the rebase: retried;
   - from it on: fault 48 and `stop()`, so the game keeps its own rendering;
   - 40 tries without success: DLSS stays off (`stop()`).

`dlss_scene_change` events log each attempt whose reason differs from the
last, and the final one. They give the scene, render size, phase times
(`phase_us`), the barrier and the FG steps.

**Tests (`render_resolution_contract`):**
- a 64:27 switch in the 2560x1440 window yields without a fault and closes
  the stamps;
- a yielded override refuses a retarget;
- `rebase()` resumes on 2560x1080 with stamps at 1280x540;
- the way back yields again;
- rebase needs a pending change;
- a refused scene that is not the game's fit still faults.

## game-85 (v0.23.0, 2026-10-13)

Four launches:
- one ended during startup (no crash in the Windows log);
- one started at 21:9 (the setting kept from game-84): DLAA at 2560x1080
  ran for about 2 minutes with fault 0, and FG used the scene
  `[0,180,2560,1080]`;
- two switched live: 21:9 → 16:9 at DLAA (FG off), and 16:9 → 21:9 at
  Quality.

**What worked: the yield.** Both captures show the request visit, then about
100 ms later the new scene with check 15. In both directions there was no
fault. From the next visit the game ran at its own scale (Quality's 0.6625
became 1).

**What failed: the rebuild.** Every `change_scene()` attempt failed in
`pb::discover` with `live-code-39442032`. That is the code gate on draw
execution (`0x259d670`), which this mod's own stream route hooks at install,
so a second full discovery can never pass. After 40 tries (20 s) DLSS
stopped and the game kept its own rendering.

**Side effects:**
- The retry path armed FG again between tries, at the old scene rectangle.
- `cd::stop()` left `live_modes()` true, so the later menu preset changes
  were refused with "the screen layout is changing" and retried, instead of
  being reported as "DLSS is not running".
- Early failures logged the mode as `performance` (the enum's zero).

## v0.23.1: the fixes

- **`pb::refresh_scene()`** is the scene part of `discover()` for an
  installed runtime. It reads the renderer again and requires the installed
  backend, device, swap chain and constant arena. It also requires the
  sizing bit clear and reads the swap-chain description. It does not check
  the code gates again: they held at install, and the mod's own hooks now
  patch some of those functions. `discover()` and `refresh_scene()` share
  `scene_extent()`.
- FG stays paused across the tries and is armed again only after a
  successful rebuild at the new rectangle. If the change gives up, FG's
  status becomes stopped ("scene-change").
- `stop()` clears `live_installed`, so after a stop the menu changes the
  preset "from the next start".
- `ModeChange::mode` starts at the requested mode.

## game-86 (v0.23.1, 2026-10-13)

One session of about 3 minutes. It started at 21:9 and Performance, and the
user then tried to break it.

**The aspect change works:**

| Change | Preset | Rebuild | FG |
|---|---|---|---|
| 21:9 → 16:9 | Performance (1280x720) | 234 ms | configured again |
| 16:9 → 21:9 | Quality (1696x715) | 188 ms | configured again |
| 21:9 → 16:9 | Quality (1696x954) | 203 ms | DLSS faulted right after |

- Six live preset changes in between all succeeded (141-188 ms).
- A Reflex change, FG 2x to 6x, Dynamic and 16 target changes all applied
  live.
- DLSS fault 0 until the third change.

**Then fault 15: the command-list registry was full.**
- `continuous_dlaa` registers every DIRECT command list on the device at its
  first Reset and keeps a reference to it, in 128 slots. Typical sessions use
  31-52; game-86 reached 128. The observer keeps its own 128-slot table of
  the same lists.
- The game does create command lists after startup. In the clean database,
  `ID3D12GraphicsCommandList` is created in `cD3DRender::initForD3D12` and in
  `0x24e7420`. The latter is a proxy context with its own queue and list, made
  by `0x2623760` for each `nDraw::ExternalRegion` (`0x25a00b0`, a vtable
  method).
- The registry's reference kept every released list alive, so the table only
  grew. The many option-menu visits in this test are the likely cause. Which
  screens create ExternalRegions is not reversed: the fix does not depend on
  it.

## v0.23.2: released lists are pruned

- When the registry is full, `prune_released()` frees every list whose last
  reference is the registry's own (AddRef/Release returns 1). Its creator
  released it, so nothing can Reset, record or submit it again.
- Its generation is retired in every per-list table as its next Reset would
  retire it: the stream route, the recording pins, the unfiltered copies and
  frame generation. `observer::forget()` frees its observer slot; then the
  reference is dropped.
- These lists are never pruned:
  - one with our recording not yet submitted (the lease, or a pending pin);
  - one pinned elsewhere (the stream route's lifetime, the spatial output),
    because it has further references.
- The observer reuses freed slots. A per-thread cached lookup is checked
  against an epoch that every `forget()` advances, so it cannot point at a
  reused slot.
- `dlss_summary` and the status log `lists {live, peak, pruned}`; the
  observer logs `lists_forgotten`.
- A table of live lists still fails closed (fault 15).

**Tests:**
- `temporal_continuous_released-lists`: 300 transient lists through the real
  Reset hook; the full table prunes, 256 are pruned, and 44 stay live.
- `temporal_continuous_held-lists`: 129 held lists give fault 15, with
  nothing pruned.
- `observer_list_slots`:
  - a forgotten list misses, through this thread's cache and on another
    thread;
  - a new list takes the slot;
  - a full table of live lists fails.

## game-87 (v0.23.2, 2026-10-13): passed

The user tried to break it and could not.
- **Six live aspect changes**, all succeeded on the first attempt in
  156-219 ms, with FG configured again each time:
  - four at Quality (1696x715 / 1696x954);
  - two at Performance (1280x540 / 1280x720).
- **Two live preset changes**: Quality → Balanced → Performance, 172 ms each.
- **The registry:** 51 lists at the start, 108, then 127, then full at 128.
  The prune freed 114 released lists; 32 were live at the last summary.
- **Health:** DLSS fault 0 for the whole session (20,104 frames); clean exit.

Live 16:9 / 21:9 recovery (roadmap step 2, part B) is done; v0.23.2 is tagged.

## game-124 (v0.25.10): a crash after repeated switches at 6x frame generation

Two runs crashed the same way. In the second, eight switches at Performance
with 6x MFG passed, about one every 1.3 s. Then:

1. The Reflex frame tracker faulted at an engine frame's end: stage 5, the
   policy disabled (0x100), `eErrorInvalidIntegration` (19). Frame generation
   stopped (`fg_stopped`, reason `reflex_frames`).
2. `presentation_state` showed `faults: 1, routing: false`.
3. A render worker crashed in `D3D12Core.dll`: access violation reading
   0x40000001b0.

**The full dump** (WER LocalDumps, DumpType 2; stays in `local/dumps`):

- The faulting thread is in the game's barrier executor for packet opcode 22
  (`sMhRender::execute2` case 22, state vtable slot 24, `sub_14259C100`). It
  transitions a target COMMON→RENDER_TARGET (packet +0x18 = 0): the back
  buffer. The call went through our `barrier_hook`, which forwards the game's
  array unchanged.
- The barrier's resource was 0x4000000000. It is read from the owner's backend:
  `[backend + 0x48 + 8·index]`, index at backend +0x2c. The index was **4**:
  past the game's back-buffer table, so it read garbage.

**Cause.** Under SDK pacing (frame generation) the game renders into
Streamline's buffers. Our `index_hook` returns Streamline's
`GetCurrentBackBufferIndex` (0..2) only while routing is on. A presentation
fault (`fault()`: a failed paced Present, or a resize that cannot be
described) turned routing off. The game then got the physical chain's index:
0..5, with six buffers for 6x.

**v0.25.11:** under SDK pacing, a routed fault keeps routing
(`fault_keeps_routing`). The worker turns frame generation off, as the stop
path already intended ("routing stays live until exit"). The synchronous
Reflex-only contract still falls back. The fault's site and HRESULT are logged
in `presentation_state` (`fault_site`, `fault_hr`), to find out why the paced
Present failed during the switch.
