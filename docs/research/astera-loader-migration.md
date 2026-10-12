# The DLSS mod as an Astera Loader plugin (v0.14.0 and v0.15.0, 2026-10-09)

The loader is required (user's decision, 2026-10-08). From v0.14.0 the alpha
is a loader plugin on its D3D12 service: the loader owns one patch per
DXGI/D3D12 method, and the mod is one detour in each chain.

The loader was called CMFM until 2026-10-09 and is now **Astera Loader**
("AL"); CMFM is its menu plugin. v0.14.0 was built against CMFM's names
(API 1.3, `cmfm.d3d12`), v0.15.0 against AL's (API 1.4).

Contract:
- AL `include/al/al_plugin.h`, API 1.4, vendored as
  `third_party/al/al_plugin.h` (commit recorded in its README);
- AL's checklists: `D:\mh world modding\AsteraLoader\docs\handoff-dlss-migration-20261009.md`
  (v0.14.0) and `handoff-dlss-astera-rename-20261009.md` (v0.15.0).

The first checklist's "keep the standalone winmm.dll mode" no longer applies;
that mode was removed in v0.13.11.

## v0.15.0: Astera Loader's names and al.hooks

Names only, ABI unchanged:

| Before (v0.14.0) | v0.15.0 |
|---|---|
| `third_party/cmfm/cmfm_plugin.h` | `third_party/al/al_plugin.h` |
| exports `cmfm_plugin_info`, `cmfm_plugin_init` | `al_plugin_info`, `al_plugin_init` |
| `cmfm.d3d12`, `cmfm.game` | `al.d3d12`, `al.game` |
| `src/cmfm/cmfm_hooks.*`, `mhw::cmfm`, `mhw_cmfm_hooks` | `src/astera/astera_hooks.*`, `mhw::astera`, `mhw_astera_hooks` |
| `cmfm\plugins\mhw_dlss\` | `AsteraLoader\plugins\mhw_dlss\` |

The plugin entry's own helpers are `loader_*`, so they never collide with
`al_` names in the header. The log fields that said `cmfm` now say `loader`
(`device_checked`, `prepared`, `factory_context` source
`loader-adapter-parent`).

### Why frame generation was off in AL's hooks-mo2 runs

AL's diagnostic plugin `site_probe` hooked Main:Update (`exe+0x1af2b60`)
through `al.hooks`. Its entry patch changed the bytes the live frame gate
hashes, so `live-frame-full-gate` refused and the worker turned frame
generation off. AL's own frame tick will hook Main:Update from API 1.5, so
this would have happened for everyone.

### The live frame hooks on al.hooks

`src/reflex/live_frames.cpp`. Under AL (`mhw::astera::active()`), the three
Reflex frame functions join AL's chains instead of MinHook patches:

| Kind | RVA | Name | Order | Original first 16 bytes |
|---:|---|---|---:|---|
| 0 | `1af2b60` | `mhw_dlss Main:Update` | -1000 | `40 53 48 81 EC 80 00 00 00 48 8B D9 E8 5F C6 76` |
| 2 | `229a080` | `mhw_dlss Render:Publish` | 0 | `48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 83` |
| 11 | `23a7f50` | `mhw_dlss Render:Submit` | 0 | `40 53 48 83 EC 20 48 8B D9 E8 D2 16 14 00 84 C0` |

- **Head bytes:** the game exe's code is encrypted on disk (SteamStub), and
  the saved crash dump does not hold these pages. They were read from a scratch
  copy of the project's own decoded database
  (`local/ida-native-dlss-20261008`, input SHA-256 `d7861173...884c`). For
  each function the full decoded bytes match the gate's SHA-256, and
  Main:Update's head matches AL's handoff (SharpPluginLoader's signature).
- **Registration:** each pattern goes to `al.hooks` with the target address.
  AL checks it against the original bytes, also after another plugin's patch,
  and writes `next` into `originals[kind]` before the detour can run, so
  `mhw_reflex_boundary_forward` is unchanged.
- **Order:** Main:Update at -1000 puts the Reflex simulation markers outside
  the other per-frame detours, AL's frame tick included.
- **Code gate:** the full-function SHA-256 is still checked, over the 16
  pattern bytes plus the live bytes from offset 16 on
  (`verify_code_with_head`). AL's chain owns the entry, and AL has checked
  those bytes. It is checked at install and again at stop.
- **Install:** after device creation, when `al.hooks` is armed, so each
  hook is active as soon as AL accepts it. They join in reverse order, with
  Main:Update last. Entries wait on the frame lock until all three are in, so
  a frame starts only with all three in place. A refused or inactive hook
  fails the frame stage, as a failed gate did.
- **Stop:** `set_enabled(handle, 0)` for each hook, then the call drain. AL
  keeps the entry patched for its chain, so the old head-byte restoration
  check no longer applies. `frame_hook_restoration` reports `chain`,
  `state` and `restored`. Under AL, `restored` means the hook is out of the
  chain (disabled or failed) and the function's code still passes the gate.
- **Logs:** `frame_hooks_installed.native_gates[]` adds `chain`
  (`al.hooks` or `minhook`), `order`, `handle` and `state`.
  `original_head_sha256` stays the function's original first 32 bytes.
- **Without `al.hooks`** (a loader older than API 1.4 refuses the plugin
  anyway), init logs a warning and the frame stage fails with
  `live-frame-al-hooks-missing`. Reflex and frame generation stay off.
- **Not needed under AL:** the trampoline pool. It is still reserved,
  because the remaining game-code hooks use the mod's MinHook.

Research builds without AL keep the MinHook path unchanged.

Still on the mod's own MinHook:
- NGX init;
- activation;
- render resolution;
- the continuous runtime's 5 game hooks;
- the stream route's 5.

These are render internals no other known plugin hooks. Moving them to
`al.hooks` would let another plugin join them later.

### Checking it (AL's plan)

Keep `site_probe` installed: it hooks Main:Update at order 0, so both share
one chain.

AL's `AsteraLoader\logs\core.<pid>.log` should show:
- `plugin name=mhw_dlss version=0.15.0 status=active`;
- `game_hook target=exe+0x1af2b60 ... state=active layers=mhw_dlss,site_probe`
  (in the SHUTDOWN summary);
- `d3d12_state=running presentation_owner=mhw_dlss frames=<n>`.

The mod's log should show `frame_hooks_installed` with `chain` `al.hooks`
and `fg_armed`, with frame generation on screen.

## v0.14.0: entry

`presentation_probe.cpp`, alpha only:

- **`al_plugin_info`**:
  - name `mhw_dlss`;
  - phases DEVICE_CREATED and SWAPCHAIN_CREATED;
  - supported exe: the pinned SHA-256 only, so AL refuses any other exe.
- **`al_plugin_init`** runs on AL's PRE_MAIN thread, outside the loader
  lock:
  1. Gets `al.d3d12` 1.0 and `al.game`. Refuses DirectX 11.
  2. Claims the presentation role.
  3. Activates `mhw::astera` (`src/astera`).
  4. Registers every detour in one `hook()` call (below).
  5. Hands `al.hooks` to `mhw::astera` (v0.15.0).
  6. Starts the worker and waits until it accepts the device (or failed).
     It does not wait for the NGX stage, which follows device creation; that
     would deadlock the game's start-up.
  7. Sets status ACTIVE, or DISABLED with the reason.
- **`DllMain`** does no work in the alpha.
- **DEVICE_CREATED** runs the former `device_hook` body (`attach_device`) on
  the game's thread: Streamline, Reflex, `Presentation::install`. The factory
  is the adapter's parent. The renderer-output check needs the game's
  `ppDevice` and only runs in the MinHook path. AL qualifies the call
  instead (the game's own import of D3D12CreateDevice).
- **SWAPCHAIN_CREATED** checks that every swap-chain continuation is filled.
  AL installs that family before firing the phase. If any is missing, status
  DEGRADED and the worker never arms.
- **Exe identity:** AL's `al.game` exe hash replaces re-hashing the exe
  file on the worker's critical path (`host_identity(..., hash_file=false)`).
  The mapped-PE checks stay.

## v0.14.0: D3D12 hooks

47 detours; the registered table is logged to AL's plugin log.

| Owner | Family: slots |
|---|---|
| presentation | device 8; factory 10, 15, 16; swap chain 8, 9, 10, 12, 13, 14, 22, 36, 39 |
| observer | command list 11, 20, 21, 22, 25, 27–44, 46, 55 (25 methods) |
| continuous runtime | command list 9, 10, 12, 13, 14, 17, 26; queue 10 |
| stream route | command list 16 |

How a hook works under AL:
- `next` is each owner's existing continuation storage: `originals[i]`, and
  for the presentation layer a static `chain_next` read by `original<>()`. AL
  fills it before the first call.
- Each registered detour sits behind a gate. Until its owner would have
  enabled its MinHook patch, the gate continues straight to `next`, as an
  absent hook did. "Live" means:
  - presentation: once it exists (`dispatch`);
  - observer: from `install` to `finish`;
  - continuous runtime and stream route: from the runtime's apply to its
    finish.
- At install, owners still resolve and check their vtable targets. They skip
  `MH_CreateHook` and fail closed when a continuation is missing ("AL
  chain").

## Internal calls

`Bypass(true)` also marks AL internal calls. This covers proxy creation and
upgrade: install, the proxy queue, proxy swap-chain creation and release. AL
then sees the game's swap-chain creation once.

Present routing keeps the plain bypass. Streamline's native Present inside the
proxy's Present must still reach `present_hook`, which counts it (`Capture`)
to decide whether a native present is still owed. Only the presentation owner
may detour swap-chain methods, so no other plugin sees those inner calls, and
AL's events skip nested Presents.

## Dropped under AL

- The creation-export hooks and their head-byte restoration check.
- Presentation `stop()` byte comparisons.
- MinHook enable/disable for the 47 methods and, from v0.15.0, the three live
  frame hooks.

Kept:
- vtable target checks (System32 dxgi / d3d12 / D3D12Core);
- the Streamline pins and Authenticode;
- the single-instance mutex;
- all game-code gates.

## Results

- **game-54 (v0.14.0, CMFM names):** frame generation, HUD-less frames and the
  silhouette ran as before. AL's side of that run (chain layers, owner,
  hello's counts) was not checked from this repository.
- **game-55 (v0.15.0, tagged):** 94 launches, most of them from the Astera
  Loader and CMFM sessions' own runs. In 89, all three live frame hooks were
  active through `al.hooks` and frame generation armed. The four refusals
  were SharpPluginLoader runs from before Astera Loader 7cf2e6c.

## Open

- **For AL overlays (later):** with frame generation the game renders into
  Streamline's proxy buffers, which GetBuffer routes to. A PRE_PRESENT overlay
  must draw into the buffer the game presents. If AL's layer queries the
  back-buffer index or buffers through an internal call, it reaches the
  native chain's, not the proxy's.
  - CMFM no longer needs this: since 2026-10-11 its menu is the game's own
    (CMFM draws nothing; `cmfm-menu.md`).
  - Any future overlay plugin still does.
