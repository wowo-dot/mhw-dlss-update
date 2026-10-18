# Architecture of the shipped mod (v0.28.6, 2026-10-17)

The map behind the 0.29.0 cleanup: what `mhw_dlss.dll` (CMake target
`mhw_dlss`, `mhw_dlss_alpha` up to 0.28.6) is made of, how it starts, what runs per frame, every hook,
how it fails, and which log output players send us. Line numbers refer to
`ddb7ab9` (v0.28.6). Since then the research targets were retired (tag
`research-final`, `docs/research/history/retired-targets.md`) and the
research switches were resolved to their shipped values, so the files have
lost the `#if` branches those line numbers count. This page describes only
the shipped configuration.

## 1. What ships

- **The DLL's own files** (listed in `CMakeLists.txt`). Up to v0.28.6 they
  were compiled with five research switches (`MHW_ALPHA`,
  `MHW_CONTINUOUS_SCALED`, `MHW_FG_GAME`, `MHW_REFLEX_FRAME_PROBE`,
  `MHW_REFLEX_TEMPORAL`). Their shipped branches are now the only code; the
  one `MHW_` switch left is the test seam `MHW_TEMPORAL_FIXTURE`.
- **13 static libraries:**
  - none of them has an `MHW_` define (`mhw_fg_reflex_presentation` had
    `MHW_FG_SCENE_PROBE`; its presentation buffers, which the crash log
    writes, are now unconditional);
  - the NVIDIA DLSS SDK's `nvsdk_ngx_d.lib` and DxilHash are linked in too
    (and MinHook, up to 0.28.6).
- **Size at v0.28.6** (section 9 has the cleanup's progress):
  - about 27,000 lines of our own code (21,900 in the DLL's files and their
    headers, 5,200 in the libraries), out of 56,000 in `src/`;
  - the DLL file is 4.9 MB, and its `.data` is 22 MB in memory;
  - about 12 MB of that `.data` is research-only buffers: `provenance.cpp`
    9.9 MB, the recorder's capture 1.9 MB. The observer's 4.7 MB entry table
    is shipped.

| Area | Files | Job |
|---|---|---|
| Plugin entry, worker | `plugin/` (0.29.0; `reflex/presentation_probe.cpp`, 1,909 lines, up to 0.28.6) | `plugin.hpp` (the shared state), `plugin.cpp` (AL exports, start, phases), `device.cpp` (device attach, shutdown), `worker.cpp` (the worker's stages and the session loop), `reflex_control.cpp`, `generation.cpp` (the FG switch), `dlss_control.cpp` (mode and aspect changes), `log_events.cpp` (most of the main log) |
| Presentation | `reflex/presentation.cpp` (1,250), `identity_chain.hpp`, `present_result.hpp` | DXGI/D3D12 routing through Streamline's proxies, the identity chain the game holds, the live FG chain switch |
| Reflex | `reflex/live_frames.cpp` (419), `frame_boundaries_entry.asm`, `streamline.cpp`, `frame_session.cpp`, `driver_status.cpp` | Frame boundary hooks (simulation, render submit, present), Streamline loading (pinned SHA-256 and signature), PCL markers |
| AL glue | `astera/astera_hooks.cpp` | al.d3d12 chains, al.hooks wrappers, internal-call scopes |
| DLSS runtime | `continuous_dlaa/runtime*.cpp` (0.29.0; one `runtime.cpp`, 1,654 lines, up to 0.28.6), `activation.cpp` (316), `temporal_bindings/*`, `dlss_sdk/inline.cpp` | `runtime_state.hpp` (the shared state), `runtime.cpp` (qualification, install, stop, finish), `runtime_projection.cpp` (the jittered projection), `runtime_requests.cpp` (TAA's request and depth packets), `runtime_resolve.cpp` (the Dispatch replacement that evaluates DLSS), `runtime_lists.cpp` (the D3D12 hooks, list and fence tracking), `runtime_native.cpp` (the game stubs' dispatch, AL's chains), `runtime_modes.cpp` (live preset, aspect and Off), `runtime_log.cpp` |
| Game options | `continuous_dlaa/game_options.cpp` | The game's own DLSS rules: AA Off, resolution scaling High, locked menu rows |
| Render resolution | `render_resolution/runtime.cpp` (432) | Sets the game's render scale on its render thread; detects aspect switches |
| Post chain at output size | `scaled_temporal/stream_route.cpp` (990) + five `.inl` (one translation unit: see its head), `full_post_inputs.cpp`, `silhouette_draw.cpp`, `screen_shader.cpp`, `silhouette_shader.cpp`, `resolve.cpp` | full_post: the game's post chain at output size after DLSS, upscaled depth and GBuffers, silhouette replay |
| Observer | `temporal_present/observer.cpp` (387), `bindings.cpp`, `output.cpp`, `unfiltered.cpp` | A CPU mirror of each command list's bindings, raster state and predicate; binding restore after injected work |
| Frame generation | `frame_generation/game_session*.cpp` (0.29.0; one `game_session.cpp`, 1,145 lines, up to 0.28.6), `hudless_pass.cpp`, `crash_log.cpp` | `game_session_state.hpp` (the shared state), `game_session.cpp` (the worker's controls, Status), `game_session_inputs.cpp` (staged depth and motion, the HUD-less image), `game_session_present.cpp` (per-Present tags and options, wind-down, window changes), `game_session_log.cpp`; the process crash log |
| Settings and menu | `settings/*` (0.29.0; `alpha/` up to 0.28.6: config, menu, menu_model, status, capabilities, version) | ini, the CMFM pages, Status lines, the card's capabilities |
| NGX startup | `ngx_startup/startup.cpp`, `worker.cpp` | Initializes the mod's NGX inside the game's own NGX init; declines the game's DLSS 1 |
| Helpers | `projection_binding/{identity,discovery,contract,stimulus,policy}`, `game_memory/{bounded_scalar_read,projection_snapshot}` (`bridge_probe/` up to 0.28.6), `common/` (0.29.0: `require`, `quoted`, the stubs' frame macros) | Code gates, structure discovery, guarded reads, matrices and jitter |

**Compiled in but unused by the shipped mod:**
- the capture machinery of `projection_binding/recorder.cpp` and
  `provenance.cpp`, plus `depth_readback.cpp`, `output.cpp`,
  `attachment_output.cpp` and most of `attachments.cpp`;
- 41 of the entry stubs in `projection_binding/entry.asm` (only
  `mhw_binding_forward` is used);
- `dlss_sdk/live.cpp`, which is linked only as the carrier of the NGX
  library.

## 2. Start-up

1. **`al_plugin_info` / `al_plugin_init`** (`presentation_probe.cpp:1843-1897`,
   AL's PRE_MAIN thread):
   - checks the exe hash and refuses DirectX 11;
   - claims the presentation role and registers every al.d3d12 detour at order
     0;
   - requires al.hooks and hands it over (0.29.0: refuses without it, as
     without al.d3d12);
   - starts the worker and waits up to 30 s for it to accept;
   - adds the CMFM pages.
2. **Worker, before the device exists:**
   - reads the ini, opens the log, installs the crash log;
   - (up to 0.28.6: reserved a MinHook trampoline pool);
   - accepts.
3. **Game render-setup thread, at DEVICE_CREATED** (`attach_device`, 562-757):
   - gathers the card's capabilities;
   - runs Streamline `slInit` and attaches it;
   - unloads DLSS-G at start;
   - installs the presentation layer.
4. **NGX startup** (worker → `temporal::prepare` → `ngx_startup::run`):
   - waits until the game's NGX init can be hooked;
   - when the game calls its DLSS 1 builder `0x23842a0`, the hooked
     `NVSDK_NGX_D3D12_Init` (`0x273f230`) first initializes the mod's NGX
     with the game's device;
   - the builder's wrapper is then freed, so the game gets no DLSS 1 context.
5. **Arming** (once the swap chain has been steady for 3 s and the game
   qualifies), in this order:
   - DLSS (`temporal::arm` → `continuous_dlaa::install`, which also installs
     render_resolution, the stream route and activation);
   - Reflex options;
   - the frame boundary hooks;
   - the observer;
   - FG configure/arm.
6. **Main loop** (100 ms):
   - menu requests (DLSS mode/Off, Reflex, FG), aspect changes;
   - health checks;
   - heartbeats every 20 s.

## 3. Per frame

- **Engine thread:** simulation boundary (`Main:Update`), then render publish
  (`Render:Publish`), then render submit (`Render:Submit`). These are the
  Reflex markers and FG's frame token.
- **Render thread:**
  - inside the camera frame (activation kind 0), the projection hook injects
    and captures jitter;
  - the post-queue hook removes competing AA units and registers TAA2;
  - inside TAA2 (kind 1), the TAA request is published and the depth producer
    records;
  - full_post sets the viewport to output size after TAA's callback, runs the
    tone map's callback at full size, and restores the viewport at the
    scheduler's exit;
  - the render scale is written through the game's setter.
- **Recording threads:**
  - the Dispatch executor's TAA dispatch is replaced by DLSS
    (`scaled_temporal::record_resolve`) and FG's input staging;
  - the copy executor replaces the input slot and TAA's copy;
  - late indexed draws are replayed at full size (silhouettes);
  - the observer mirrors the bindings.
- **Submission:** ExecuteCommandLists signals the mod's fence for each leased
  list; Reset retires the recordings.
- **Present:**
  - the game presents the identity chain;
  - the presentation layer presents the real chain through Streamline;
  - FG tags the inputs, sets its options and reads DLSSGState.

## 4. Threads

| Thread | Owner | Runs |
|---|---|---|
| AL PRE_MAIN | AL | `al_plugin_init` |
| Worker | the mod (`presentation_probe.cpp:1024`) | Start-up, NGX startup, arming, main loop, logs |
| NGX startup worker | the mod (`ngx_startup/worker.cpp`) | Readiness wait, hook install and removal |
| Region refresher | the mod (`bounded_scalar_read.cpp:234`, every 20 ms, never exits) | Read-cache upkeep |
| CMFM menu | CMFM | Menu callbacks: settings only, applied by the worker |
| Game: render setup, engine, render, recording workers, present | the game | Every hook in section 5 |

## 5. Hooks

All targets are in `MonsterHunterWorld.exe`, checked by SHA-256 code gates
against the tested build (205 gates; `sigscan.py gates`) before anything is
patched.

**About `r11`.** AL's entry stub is `mov r11, imm64; jmp [r11]`, so it
overwrites r11. At a function start that's harmless: r11 is volatile and is
not an argument register. The only mid-function sites are TAA2's two compare
instructions, and on 2026-10-17 the clean database showed r11 dead after both:
every path reaches a call or a return before r11 is read. So every hook below
can go to al.hooks.

**Game functions** (al.hooks chains since 0.29.0; MinHook up to 0.28.6, except
the frame boundaries):

| Target | What | Module | Stub | Site |
|---|---|---|---|---|
| 0x1af2b60 Main:Update, 0x229a080 Render:Publish, 0x23a7f50 Render:Submit | Reflex frame boundaries | live_frames | frame_boundaries_entry | start |
| 0x273f230 NVSDK_NGX_D3D12_Init, 0x23842a0 the game's DLSS 1 builder | NGX startup, DLSS 1 decline | ngx_startup | none | start |
| 0x23c9980 TAA packet producer, 0x259cc20 Dispatch executor, 0x23cd170 depth copy producer, 0x259df30 copy executor, 0x228f3b0 viewport projection | DLSS requests, depth, jitter | continuous_dlaa runtime | temporal/depth_route/jitter_entry | start |
| 0x228e770 sCamera::draw, 0x2392150 TAA2 Execute, 0x23931e0 / 0x2596090 / 0x25979b0 AA jitter units, 0x24f1fc0 post queue | Frame ownership, TAA2 forced, jitter units skipped | activation | activation_entry | start |
| 0x239217d, 0x2392f8e | TAA2 table and sharpen compares (forced branch) | activation | leaf stubs | **mid-function** (r11 dead) |
| 0x229ed30 renderer update | Render scale | render_resolution | resolution_entry | start |
| 0x2297e20 final-quad copy, 0x259c650 copy executor, 0x259d740 indexed executor, 0x24d0780 tone-map callback, 0x23cb520 projection helper | full_post, silhouettes | stream_route | stream_entry | start |
| 0x43dcb0, 0x43eeb0, 0x53f580, 0x53f440, 0x53f190, 0x540720 | The game's AA/RS apply and graphics-menu rows | game_options | none | start; enabled at install, **never removed** |

**DXGI/D3D12 methods (al.d3d12, registered at PRE_MAIN):**
- presentation: 26 methods (CreateCommandQueue, factory creation, the
  swap-chain methods);
- DLSS runtime: Reset, Close, ResourceBarrier, Dispatch, CopyResource,
  DrawInstanced, DrawIndexedInstanced, ExecuteCommandLists;
- stream route: CopyTextureRegion;
- observer: 25 command-list setters.

**Hook lifetime in the shipped build:**
- hooks are never removed once armed;
- `continuous_dlaa::finish()` runs only on early worker failures;
- the NGX startup hooks are disabled after start-up, which is where tifa-2/4's
  removal failed before 0.28.6.

## 6. Failing closed

| Module | On a fault | Still running |
|---|---|---|
| DLSS runtime (`fail(n)`, first code kept) | `healthy()` false → `fault_snapshot` → `cd::stop()`: the game's own AA, scale and options come back | Hooks forward; a dispatch already claimed finishes |
| Stream route (codes 1-15) | Part of `healthy()`: DLSS off for the session | The same |
| Activation / render_resolution | Its own fault codes (activation 1-7, resolution 1-7) → `healthy()` | The same |
| Presentation (`fault(site, hr, why)`) | FG off for the session; routing ends only while the game does not hold the identity chain (0.28.6) | DLSS, Reflex, menu |
| Reflex frames | The first reason is kept; frame session stops; FG off | Originals keep running |
| FG session | Per Present: Off with the reason counted in `off.*` | DLSS, Reflex |
| Game options | Never faults; counts `call_faults` | Everything |
| Worker | A `require` failure in the main loop ends the worker (see section 8) | DLSS and hooks; menu changes stop applying |

Not a fault: `reconstruction.latch(14)` after a recovered SDK failure leaves
DLSS degraded (no AA, history reset) with `healthy()` still true.

## 7. What players send us (keep)

Log files (in `AsteraLoader/plugins/mhw_dlss/logs`, or MO2's overwrite):
- `reflex-frames-<pid>-<creation>.jsonl`, the main log:
  - start-up: `start`, `alpha_config`, `card_capabilities`, `sdk_initialized`,
    `fg_support`, `presentation_installed`;
  - arming: `frames_armed`, `fg_armed`, `fg_switch_ready`;
  - loop: `frame_heartbeat` (late queues, presentation faults), `fg_status`
    (the `session` object), `dlss_summary` (`runtime`: frames, skips,
    `skip_reasons`, `producer_refusals`, `route`, `game_options`,
    `consumer_depth`, …), `frame_stats`, `latency`, `swap_lifecycle`,
    `fg_switch`, `dlss_mode_change`, `dlss_scene_change`,
    `presentation_fault`;
  - end: `game_exit`, `fg_stopped`, `worker_failed`.
- `<main log>.crash.jsonl`: `host_exception` / `unhandled_exception` with
  `module+0xRVA` sites and a stack. A build's `.map` file resolves those
  (`local/release/builds/<version>-<sha8>/`).
- `inputs-<pid>-<creation>.jsonl`: the DLSS runtime's `continuous` reports and
  `fault_snapshot`.
- `startup-<pid>-<creation>.jsonl`: NGX startup (`code_readiness`,
  `game_dlss`, `capabilities`, `stopped`, `failed`).
- `startup-error.jsonl` next to the ini, if the worker fails before its log
  opens.
- Astera Loader's core log: the plugin's status, its `menu:` lines, game
  hooks.

The tools that read them are `tools/fg_game.py`, `tools/profile_report.py`
and `tools/perf/`. Removing a field breaks them and our comparisons with
older reports, so fields keep their names. A changed meaning bumps the
event's `schema`.

## 8. Debt found by the map (2026-10-17)

**Bugs:**
- The game-exit summary's `fg_status` prints booleans as 0/1
  (`presentation_probe.cpp:1448`: no `std::boolalpha`).
- `provider_check` in the startup log writes the full runtime path, which may
  include the player's user name (AGENTS.md: no personal paths).
- A `require` that throws in the worker's main loop (`driver_mode`, the
  temporal status writes) ends the worker. DLSS keeps running, but menu
  changes and logging stop.
- `temporal::sample()` at `game_exit` can throw when the runtime went
  unhealthy in the same tick, so `worker_failed` replaces the exit summary.
- Fault code 44 means two things: a consumer depth copy and
  `render_resolution::finish`.
- An install that fails after queueing MinHook enables leaves them in
  MinHook's global queue; game_options and render_resolution stay enabled.
  (Gone with MinHook in 0.29.0; under AL each hook is live as registered.)
- Under AL, `presentation_hook_restoration` reports `restored:false` and the
  hash of an all-zero head for every presentation method: AL owns those
  patches, so nothing restores or records them.

**Fluff:**
- About 12 MB of research buffers, and the capture machinery of
  recorder/provenance/depth_readback.
- The MinHook and "without AL" branches. AL is always present for the
  shipped DLL.
- The pre-AL creation hooks, `boot_monitor`, `path_setting` and
  `sdk-runtime-dir.txt`.
- Code after `return 0` (`presentation_probe.cpp:1690-1748`).
- `poll_perf` marker files, still polled in the shipped build.
- The non-keyed FG staging path.
- The ownership diagnostics (`presentation_ownership.hpp`, a ~60 KB sample
  array).
- Unused functions and fields listed in the area maps:
  - `View::display_hz`/`target`
  - `HudlessPass::width/height`
  - `observed_zero`
  - `yielded_now`
  - the viewport hook path
  - `inline.cpp`'s `Readback` and `collect`/`finish`
  - `observer.cpp`'s `capture`/`sample`/`write`
  - unused stream hook slots
  - `late_recorded`

**Duplicates:**
- the module+offset formatter (`crash_log.cpp` `describe` and
  `presentation_probe.cpp` `code_site`);
- `quoted`/`quote`, four `require` helpers;
- three `Chained<>` wrappers, the asm SAVE/RESTORE macros (four copies);
- the 256-block command-queue scan (six copies);
- `same_device` (three), hex/SHA-256 helpers (four);
- the off-state reset in the FG session (three);
- the Reflex and FG mode name mappings (three).

**Structure:**
- `presentation_probe.cpp`, `continuous_dlaa/runtime.cpp`, `game_session.cpp`
  and `stream_route.cpp` each hold several jobs.
- The `.inl` files share anonymous-namespace state, so splitting them needs an
  internal state header first.

**Names:**
- "alpha" means the shipped plugin.
- "probe" names the plugin entry (`presentation_probe.cpp`).
- `silhouette_probe.inl` holds production code.
- "identity" means four things.
- The `"build":"alpha"` marker is checked by `package_release.py`.

## 9. The 0.29.0 plan

Behaviour stays the same; the log fields stay.
1. This map.
2. MinHook to al.hooks:
   - the NGX startup hooks with a low order, because `_ReturnAddress()` must
     see the game's caller;
   - game options, render resolution, activation (including the two compare
     sites), the DLSS runtime, the stream route;
   - then MinHook leaves the DLL's link, and the trampoline pool goes with
     it.
3. Retire the research targets behind the tag `research-final`, then remove
   the switches, fallbacks and research buffers from the shipped files.
4. Restructure:
   - split the four big files along the seams in the area maps;
   - name things for what they are;
   - one helper per duplicate.
5. Fix the bugs in section 8.
6. Prove it:
   - the tests that still apply and the code gates;
   - a perf13-style comparison;
   - in-game checkpoints after NGX startup, after the DLSS runtime, and after
     MinHook's removal;
   - a long session.

**Progress:**
- The defaults: `Config`'s defaults and CMFM's "Reset to defaults" had
  DLSS Performance, FG 2x and Reflex On, unlike the shipped ini (all off).
  Both follow the ini now (`671be63`).
- Steps 1 and 2 are done; checkpoints 1 and 2 passed in game (0.29.0-dev.1
  and dev.2).
- Step 3:
  - the research targets are retired (`9a8a056`);
  - the projection recorder's capture machinery and its buffers left the DLL;
  - the research switches are resolved to their shipped values. The DLL
    built from the result has the same functions, sizes and bytes as before;
    only its time stamps and one RTTI name (the anonymous namespace's hash,
    which follows the source path) differ;
  - `continuous_runtime_tests` used to compile `runtime.cpp` without the
    switches, so it tested the unscaled path, which no longer ships. It now
    runs the shipped path, with test doubles for the modules the runtime
    calls (`tests/runtime_neighbours.cpp`);
  - dead functions out, found with a link without inlining and
    `/VERBOSE:REF` (unreferenced external functions) and clang-tidy's
    unused-code diagnostics (internal ones): the code after `return 0`,
    `boot_monitor`, `path_setting`, `observed_zero`, `yielded_now`, the
    filter-shader builder, the scene probe, and the recorder's API and
    capture records (the silhouette scope's per-thread `pb::Draw` became a
    four-field `DrawSite`);
  - MinHook out of the DLL. Every game-function hook is in an al.hooks
    chain, every DXGI/D3D12 detour in an al.d3d12 chain; start-up refuses
    an Astera Loader without al.hooks. Gone with it: the trampoline pool
    (one RWX page near the game's code, reserved even under AL), the pre-AL
    creation hooks, the viewport hook path, the observer's partial hook
    sets and its research `capture`/`sample`/`write`, and the thread
    snapshot adapter (AL carries that fix). Log changes: `frame_pool`,
    `reservation`, `api_module` and `hook_restoration` are gone, `prepared`
    keeps `loader` and `sdk_initialized`, the NGX removal drops
    `thread_lists`; `hook_service` is always `"al"`. No log tool reads the
    dropped events for shipped logs (`reflex_frames.py` and
    `review_reflex_presentation.py` check them only in old research logs).
    Tests hook through a stand-in loader on MinHook
    (`tests/fake_astera.cpp`), so they run the shipped path.
- Deferred to step 4, where their files are split anyway: the presentation
  layer's `OwnershipCall` record (threaded through `route()` and every
  swap-chain hook; the GetBuffer hook also uses it for the crash log's
  buffer map), and the DLSS frame's one-shot and capture modes (the DLL
  always builds it continuous, caller-retained, without captures).
- Step 4:
  - the plugin entry (`reflex/presentation_probe.cpp`, eight jobs on one
    set of file-local globals) is `src/plugin/`: the shared state in
    `plugin.hpp`, one file per job, and the 430-line worker a sequence of
    named stages. Checked: every code line of the old file is in the new
    ones apart from the declarations, the stage glue and `gate()` (dead
    since the creation hooks went: 109 gates), and the 48 log events and
    their fields are written as before.
  - the presentation layer's ownership diagnostic is gone: every routed
    swap-chain call built an `OwnershipCall` record for a recorder only
    the research probes switched on; `route()` no longer takes one.
  - the DLSS runtime (`continuous_dlaa/runtime.cpp`, 1,442 lines after
    step 3) is eight files by job on `runtime_state.hpp`, its state as
    `inline` globals in `mhw::continuous_dlaa::detail`; the runtime test
    builds the same files instead of including the source. Checked as for
    the plugin: every code and comment line moved apart from the `inline`
    prefixes, declarations and namespace lines (`DEPTH_NOTE` became the
    function it called); the log fields are unchanged.
  - the DLSS frame (`dlss_sdk::InlineFrame`) keeps the one mode the
    runtime builds: continuous, caller-retained, no captures. The one-shot
    `record()`, the readback captures (`Readback`, `collect()`), `finish()`
    and the GPU fixture's fault points (`RecordFault`) are gone, and with
    them `continuous_dlaa::record_resolve` (`resolve.cpp`, the unscaled
    resolve; nothing called it). The frame records the same commands. Its
    state in the log drops four constant fields (`continuous`,
    `caller_retained`, `captures`, `collected`).
  - The linker oracle (step 3) cannot see a static-library object nothing
    references, since the linker never loads it. Comparing each library's
    objects with the DLL's map file finds two: `continuous_dlaa/resolve.cpp`
    and `dlss_sdk/live.cpp` (the research's bounded three- or nine-frame
    evaluation). Both are gone; the DLSS frame links the NGX library
    itself, `live.hpp` is `initialization.hpp` (the borrowed NGX
    initialization) and `history.hpp` keeps `HistoryFrame`.
  - Headers have the same blind spot: a declaration nothing calls needs no
    definition, and an unused inline function is never compiled. A sweep
    for names no other file mentions and for declarations without a
    definition removed the research's renderer snapshot, depth selection
    and depth work interfaces (`bridge_probe`; `renderer_snapshot.hpp` is
    `bounded_scalar_read.hpp`), the old depth-route and jitter modules'
    declarations, a stale `frame_boundaries::reserve`, and three unused
    helpers.
  - The C++ sources have one layout (`.clang-format`, clang-format 22 from
    Visual Studio): one commit (0b27046, in `.git-blame-ignore-revs`)
    whose DLL differs from the one before only in its link timestamp.
    Line numbers quoted above are from before it.
  - the frame generation session (`game_session.cpp`) is four files by job
    on `game_session_state.hpp`, checked as the other splits: every code
    and comment line moved, the 150 log field writes are unchanged.
  - `stream_route.cpp` stays one translation unit with its five `.inl`
    parts: they share the route's state and lock, the per-draw hooks call
    into every part, and without link-time code generation separate files
    would stop those calls inlining. Its head now maps the parts; two
    stale part headers (full_post "prototype", the silhouette probe
    "diagnostic only") are corrected.
  - Duplicates, first pass: `src/common/` holds one `require()`/`hr()`
    (fourteen file-local copies) and one JSON `quoted()` (two);
    `crash_log::code_site()` is the one module+offset formatter (the crash
    log quotes it, the plugin's `code_site()` wraps it). The three
    `same_device` checks stay: the two throwing ones name the step that
    failed, and those strings reach the players' logs.
  - `astera::Gate` is the one gate in front of the D3D12 chain entries
    (four `Chained<>` copies; each owner keeps a one-line alias), and
    `src/common/native_frame.inc` holds the stubs' frame macros (SAVE and
    RESTORE in six `.asm`, the shared stub in four as `NATIVE_ENTRY`);
    every stub assembles to the same code and unwind data as before.
  - The FG session's Off (DLSS-G off, applied and shown state reset) is
    one `switch_off()` for `turn_off()` and `lifecycle_guard()`; the unload
    path stays separate on purpose (no SDK call, no mode change counted).
    The plugin's Reflex mode name is the ini mode's name (`sl::ReflexMode`
    is numbered the same, checked at compile time).
  - Left as they are: the command-queue scans in the stream route's plan
    builder, census and post chain each keep their own failure counts
    (`plan_rejection` 9/10, the census's `unreadable`, the commands'
    `error`), which the logs report; a shared visitor would have to carry
    each, in code only the game exercises. The hex/SHA-256 helpers differ
    (memoized code hashes, file digests, Streamline's pinned DLLs).
  - Names: `src/alpha/` is `src/settings/` (`mhw::settings`), the
    plugin's `alpha_config`/`alpha_home`/`alpha_base` are `config`/`home`/
    `config_folder`, `MHW_ALPHA_VERSION` is `MHW_VERSION`, and comments
    that called the shipped mod "the alpha" say what they mean. Strings
    the logs, the ini and the package carry keep their text: the
    `alpha_config`, `alpha_waiting` and `alpha_arm` events, `"build":
    "alpha"`, the ini's "(alpha)" header (88 event and 1,833 field
    writes compared before and after).
  - `src/bridge_probe/` (the research's bridge probe) is
    `src/game_memory/` (`mhw::game_memory`): the guarded reads of game
    memory and the camera projection snapshot.
  - The mod is no longer labelled alpha (the user's call after checkpoint
    4): the package's ini header and README drop "(alpha)", the start
    event drops its constant `"build":"alpha"` (no tool read it), and
    `package_release.py` checks the DLL for the start event's
    `{"event":"start","schema":1,"version":"<version>",` instead. The
    `alpha_config`, `alpha_waiting` and `alpha_arm` event names stay, so
    old and new logs read the same.
