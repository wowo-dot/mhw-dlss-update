# Reflex startup/device check, 2026-10-02

The corrected automatic v0.1.3 probe passed its first in-game launch. See
[startup/device result](reflex-startup-result.md) for the accepted capture and
independent restoration checks. The same DLL remains installed.
It checks Streamline initialization at the game's graphics creation boundary,
native device support, Off-mode state and shutdown. It does not enable in-game
Reflex, sleep, frame markers, DLAA or DLSS. Persistent in-game Reflex has not
been implemented yet. The v0.1.1 attempt failed before installing any hooks or
initializing the SDK: `GetModuleHandle` could not resolve the creation APIs
before the game's constructor loaded their modules. Its trace and source are
preserved. The user exited before independent memory inspection, so no live
restoration check is claimed for that rejected attempt. v0.1.2 loaded the system
modules successfully, but reached the main-device callback without observing
the earlier factory calls. It rejected SDK setup. Independent live reads
verified both API heads restored and both complete native creation functions
unchanged. Its trace, cleanup evidence and source freeze are preserved.

## Owned interface correction

Streamline 2.12.0's public `slUpgradeInterface` creates a proxy holding an added
native reference; the caller still owns its original native reference. The
former fixture helper detached that original without releasing it.
`src/reflex/owned_interface.hpp` now adopts a changed proxy using
`ComPtr::Attach`, releasing the original. An unchanged interface keeps its
ownership; a failed partial replacement is released while retaining the
original. Never supply a borrowed game global as an owned pointer.

`slGetNativeInterface` returns another owned native reference. The fixture uses
native rendering interfaces and proxies for SDK creation and presentation.
Streamline's swap-chain creation path separately releases the original returned
swap-chain reference when installing its proxy; do not add another release.

Owned test 04 exited with code 0 after 288 completed GPU frames, tokens and sleep
calls, 1,728 markers and 192 verified ordered driver reports across Off, On and
Boost. Two swap-chain resizes passed descriptor checks. Explicit SDK/resource/
proxy/native teardown ended with zero native device references. The interposer
stays loaded while proxy vtables can be referenced. Earlier API results alone
did not prove complete COM teardown. No game process was accessed, and no game
latency benefit is established.

The frame-policy and ownership CTests passed. Three startup-log reviewer test
groups passed, including malformed flags, missing/duplicate lifecycle records,
owner mismatch, failed restoration and unexpected proxy activation.

The new `mhw_reflex_startup_fixture` separately creates the native DX12 device
first, then initializes Streamline in manual mode, attaches it, checks Reflex
Off, shuts down and releases the native objects. That exact order passed with
low latency available, healthy device state and zero final device references.
No queues, swap chains, sleep or markers were created in this fixture.

## Hook contract

Tested executable version `0.0.0.0`; retail SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis-image SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
These are experimental hash-gated offsets, not portable discovery.

| Public target | Typed Windows x64 signature | Qualified native caller |
| --- | --- | --- |
| `CreateDXGIFactory2` | `HRESULT WINAPI(UINT, REFIID, void**)` | Preview return `0x23a15fd`, main return `0x23a49c6` |
| `D3D12CreateDevice` | `HRESULT WINAPI(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**)` | Main device return `0x23a4e2e` |

The preview constructor at `0x23a0950` is checked over 4,158 bytes against SHA
`138af20800e364c70b5a9228f6d294fbcccad1cf106e0013e84d008b2481e4bc`.
The DX12 initializer at `0x23a48e0` is checked over 11,101 bytes against SHA
`8757a054cc1c203128fdfb73ac88975dda7dc73ef5ef1a53a80f22cd0eb8e500`.
The native constructor has stack arguments and is not detoured using the prior
four-register observational stub.

The worker explicitly loads DXGI and D3D12 from the system directory, retains
their loader references and validates each export's executable allocation.
The probe has no static imports from either module; a standalone control
resolved both exports with neither module previously loaded. The worker records
the loaded module identities and original-head metadata before enabling hooks.
It then installs two typed public API detours. SDK calls run in qualified
callbacks outside `DllMain`. Factory callbacks are observations; no earlier
factory callback is required. Initialization runs after the qualified native
main-device creation succeeds, before returning to the initializer's queue and
swap-chain creation. This uses the explicit device-before-`slInit` allowance in
the pinned 2.12.0 manual-hooking guide, while honoring initialization before
mandatory manual hooks. The standard Reflex guide's earlier initialization
recommendation is not claimed by this manual startup test. Original API arguments,
results and Win32 last-error state are preserved. Unqualified calls pass through.
A late load, code mismatch, missing invariant or SDK error ends the experiment
while preserving native API calls.

The main device is temporarily pinned by an owned `QueryInterface` reference.
Adapter LUID, health, node count, Reflex/PCL support and Reflex Off state are
checked. SDK shutdown and pin release precede return to the game's initializer.
No game interface is upgraded. Hooks are disabled, active callbacks drain, and
both retained original 32-byte heads are checked. Trampolines and the module
stay loaded for forwarding calls already in flight. The worker then observes
native swap-chain dimensions, format, buffer count and windowed state.
Main-device discovery is bounded at 180 seconds; swap-chain discovery at a
further 60 seconds. A rejected startup is not a working Reflex result.

## Installation and launch

Installed DLL SHA-256:
`80d0b986326b5c5c4535645598120c0d42fed3f94d34effa1a5c5a0cc3bf443d`.
The destination is the usual local MO2 `dlssTEST/mhw_dlss_bridge_probe.dll` slot.
The output sidecar selects `local/reflex-startup-20261002/game-startup-03/`;
the runtime sidecar selects the pinned local SDK under
`local/reflex-foundation-20261002/sdk/bin/x64/`. No SDK DLL is copied into the
game directory. Paths stay in ignored local configuration, not distributable
binary constants.

Launch normally through the existing MO2 instance/profile in solo/offline mode.
Keep native AA/DLSS Off and the window size unchanged. Reach the main menu and
report ready; no arming or save load is needed. The probe runs automatically
once per process, producing a unique PID/creation-time JSONL log.

Review with `tools/review_reflex_startup.py <log> --output <new-review.json>`.
Acceptance requires complete initialization/device/shutdown/native-swap order,
Off mode, no sleep/markers/rendering changes and both original heads restored.
Independently verify native API heads with read-only process checks before
claiming the runtime test passed.

Exact prior DLL/output/runtime-sidecar bytes are preserved in `game-startup-03/`.
Removal with the game closed:
restore `previous-slot.dll.disabled` to the exact DLL destination; restore
`output-sidecar.before.bin` to the exact output-sidecar destination; restore
`runtime-sidecar.before.bin` to the exact runtime-sidecar destination. Keep this
DLL installed between tests unless a replacement or restoration is requested.

Deployment and prelaunch manifests record installation and unchanged graphics,
profile and mode snapshots. The separate source freeze contains 301 files;
source-manifest SHA-256:
`7e0d39fe3374e1879af0061f407fefc088f98d5beb0b50c32489df60480b770e`.
It was independently checked before installation. The v0.1.1 freeze remains
unchanged with manifest SHA-256
`21c42162320547efb9d3fedccdf366669c86fa8cacfb1ba12b664c55a066818a`.
Creation-map, private assembly, SDK binaries and owned test 04 remain ignored
local evidence. Earlier source snapshots and traces remain intact.

Full presentation maintenance and verified input/simulation markers still need
integration after this check. See [foundation](reflex-foundation.md) and
[native frame map](reflex-frame-boundary-result.md).
