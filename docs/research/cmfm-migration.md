# The DLSS mod as a CMFM plugin (v0.14.0, 2026-10-09)

CMFM is required (user's decision, 2026-10-08). From v0.14.0 the alpha is a
CMFM plugin on `cmfm.d3d12`: CMFM owns one patch per DXGI/D3D12 method, and
the mod is one detour in each chain.

Contract:
- CMFM `include/cmfm/cmfm_plugin.h`, API 1.3, vendored as
  `third_party/cmfm/cmfm_plugin.h` (commit recorded in its README);
- CMFM's checklist: `D:\mh world modding\CMFM\docs\handoff-dlss-migration-20261009.md`.

The checklist's "keep the standalone winmm.dll mode" no longer applies; that
mode was removed in v0.13.11.

## Entry

`presentation_probe.cpp`, alpha only:

- **`cmfm_plugin_info`**:
  - name `mhw_dlss`, version 0.14.0;
  - phases DEVICE_CREATED and SWAPCHAIN_CREATED;
  - supported exe: the pinned SHA-256 only, so CMFM refuses any other exe.
- **`cmfm_plugin_init`** runs on CMFM's PRE_MAIN thread, outside the loader
  lock:
  1. Gets `cmfm.d3d12` 1.0 and `cmfm.game`. Refuses DirectX 11.
  2. Claims the presentation role.
  3. Activates `mhw::cmfm` (`src/cmfm`).
  4. Registers every detour in one `hook()` call (below).
  5. Starts the worker and waits until it accepts the device (or failed).
     It does not wait for the NGX stage, which follows device creation; that
     would deadlock the game's start-up.
  6. Sets status ACTIVE, or DISABLED with the reason.
- **`DllMain`** does no work in the alpha.
- **DEVICE_CREATED** runs the former `device_hook` body (`attach_device`) on
  the game's thread: Streamline, Reflex, `Presentation::install`. The factory
  is the adapter's parent. The renderer-output check needs the game's
  `ppDevice` and only runs in the MinHook path. CMFM qualifies the call
  instead (the game's own import of D3D12CreateDevice).
- **SWAPCHAIN_CREATED** checks that every swap-chain continuation is filled.
  CMFM installs that family before firing the phase (`hub.cpp`
  `swapchain_created`). If any is missing, status DEGRADED and the worker
  never arms.
- **Exe identity:** CMFM's `cmfm.game` exe hash replaces re-hashing the exe
  file on the worker's critical path (`host_identity(..., hash_file=false)`).
  The mapped-PE checks stay.

## Hooks

47 detours; the registered table is logged to CMFM's plugin log.

| Owner | Family: slots |
|---|---|
| presentation | device 8; factory 10, 15, 16; swap chain 8, 9, 10, 12, 13, 14, 22, 36, 39 |
| observer | command list 11, 20, 21, 22, 25, 27–44, 46, 55 (25 methods) |
| continuous runtime | command list 9, 10, 12, 13, 14, 17, 26; queue 10 |
| stream route | command list 16 |

How a hook works under CMFM:
- `next` is each owner's existing continuation storage: `originals[i]`, and
  for the presentation layer a static `chain_next` read by `original<>()`. CMFM
  fills it before the first call.
- Each registered detour sits behind a gate. Until its owner would have
  enabled its MinHook patch, the gate continues straight to `next`, as an
  absent hook did. "Live" means:
  - presentation: once it exists (`dispatch`);
  - observer: from `install` to `finish`;
  - continuous runtime and stream route: from the runtime's apply to its
    finish.
- At install, owners still resolve and check their vtable targets. They skip
  `MH_CreateHook` and fail closed when a continuation is missing ("CMFM
  chain").

Game-code hooks stay on the mod's own MinHook and its trampoline pool:
- NGX init;
- Reflex frame functions;
- the continuous runtime's 5;
- activation;
- render resolution;
- the stream route's 5, including the indexed executor.

## Internal calls

`Bypass(true)` also marks CMFM internal calls. This covers proxy creation and
upgrade: install, the proxy queue, proxy swap-chain creation and release. CMFM
then sees the game's swap-chain creation once.

Present routing keeps the plain bypass. Streamline's native Present inside the
proxy's Present must still reach `present_hook`, which counts it (`Capture`)
to decide whether a native present is still owed. Only the presentation owner
may detour swap-chain methods, so no other plugin sees those inner calls, and
CMFM's events skip nested Presents.

## Dropped under CMFM

- The creation-export hooks and their head-byte restoration check.
- Presentation `stop()` byte comparisons.
- MinHook enable/disable for the 47 methods.

Kept:
- vtable target checks (System32 dxgi / d3d12 / D3D12Core);
- the Streamline pins and Authenticode;
- the single-instance mutex;
- all game-code gates.

## Open

- **In-game run** with the CMFM profile and hello enabled, per CMFM's test
  plan:
  - `d3d12_chain` layers `cmfm,mhw_dlss`;
  - presentation owner `mhw_dlss`;
  - hello's PRE_PRESENT count equal to the game's presents;
  - Steam overlay; alt-tab, resize and window mode; exit code 0;
  - frame generation and the silhouette as before.
- **For CMFM overlays (later):** with frame generation the game renders into
  Streamline's proxy buffers, which GetBuffer routes to. A PRE_PRESENT overlay
  must draw into the buffer the game presents. If CMFM's layer queries the
  back-buffer index or buffers through an internal call, it reaches the
  native chain's, not the proxy's. To settle with CMFM before the menu draws.
