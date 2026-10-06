# Reflex foundation, 2026-10-02

The user approved moving from the working DLSS Performance prototype to Reflex,
then ordinary frame generation and MFG. Other resolution ratios, settings UI and
broader optimization remain later work. The tested v1.25.44 renderer is preserved.

## SDK result

The optional `mhw_streamline_reflex` library dynamically loads the official
Streamline **2.12.0** production runtime. Only Reflex and PCL are requested;
NGX DLSS/DLSS-G are not initialized by this foundation. All four requested DLLs
must match pinned SHA-256 values and pass Authenticode verification before any
SDK code executes. OTA and downloaded plugins are disabled. Paths come from
local configuration and are not compiled into distributable code.

The standalone `mhw_reflex_sdk_fixture` creates its own DX12 device, queue and
320x240 swap chain. It upgrades its owned interfaces using the public manual
hooking route so Streamline common presentation maintenance runs. It never
opens the game process.

The first successful fixture exited with code 0 after **288 completed GPU frames**,
three modes (Off, Low Latency, Low Latency + Boost), 288 sleep calls and 1,728
markers. The driver returned 64 complete ordered reports per mode. Independent
review verified all 192 report IDs are unique, increasing and within their
respective mode's frame interval. This proves the API/device/presentation setup
on the local adapter; it does not establish game integration or latency gains.

An earlier fixture completed its API work but failed during final COM release:
unloading the interposer after `slShutdown` invalidated proxy vtables. The wrapper
now shuts down SDK services before COM destruction and retains the interposer
module until process exit. Both attempts remain in ignored local evidence.

The third owned-device run uses the new `FrameSession` SDK adapter instead of
issuing markers directly from the fixture. It completed 288 frame tokens, sleep
calls and GPU frames, plus 1,728 markers, with exit code 0. Independent review
again verified 192 ordered driver reports across Off, On and Boost. The fixture
uses synthetic counter stamps on its own device; it does not establish where
those events belong in the game. No game process was accessed by this test.

Owned test 04 corrected a reference leak in the upgrade helper: a successful
`slUpgradeInterface` proxy adds a native reference but does not release the
caller's original owned reference. The helper now releases that reference when
adopting the proxy. Native-interface rendering, two swap-chain resizes and
explicit teardown passed, with zero final device references. This run again
completed 288 frames/tokens/sleeps, 1,728 markers and 192 ordered reports.
Pure ownership tests passed. See [startup/device check](reflex-startup-device.md)
for the ownership contract; earlier API results alone did not prove complete
COM teardown.

## Native frame map and observational probe

Executable version: `0.0.0.0`. Retail SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The offline decoded image was independently verified as
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

The window message pump and renderer worker are separate. The apparent counters
in the window/input object are message bookkeeping, not established simulation
frame IDs. They are not used for Reflex sleep or markers.

The engine update at RVA `0x1af2b60` calls the renderer bank/counter update and
schedules game subsystems. Two bounded traces now establish its observed
relationship to command submission and presentation in the main menu and a
loaded gameplay scene. They do not establish exact simulation/input semantics.
The trace records these
boundaries and their observed renderer epoch, temporal counter and bank:

| Kind | Target | Full code bytes | SHA-256 |
| --- | --- | --- | --- |
| 0: engine update candidate | `0x1af2b60` | 4864 | `9f6f5b3ac1a521ddd59351e41cc66c17dfc453ccd1b969afd4d59787bfef795d` |
| 1: counter/bank update | `0x2293280` | 489 | `576813bea02244e8970c592cf2f2a10e74335cb893c1402f560cf3eb4fa9ad64` |
| 2: record/publish | `0x229a080` | 1428 | `961cf9969c28abc6b5b63ba9fb476978555eb188fdc173940d7ed81fc566a4e0` |
| 3: renderer worker iteration | `0x229b020` | 2420 | `a4a09901e108fe38dabfb6d9df3a52f9d5ccd68a12b8b8e6f7a9ea671cbf5e90` |
| 4: native DX12 presentation wrapper | `0x23a9ce0` | 1447 | `aec3a83be592d010103ac1aa576648a043f30c8691a0b4405e04df8c4bb49f3c` |
| 5: GPU submission | discovered `ID3D12CommandQueue::ExecuteCommandLists`, slot 10 | first 32 bytes retained | measured at installation |
| 6: actual presentation | discovered `IDXGISwapChain::Present`, slot 8 | first 32 bytes retained | measured at installation |

The separate native helper at `0x23ad3a0` issues `DXGI_PRESENT_TEST`. It is not a
presented frame and is deliberately excluded from the proposed marker boundary.

All seven hooks use the Windows x64 ABI and preserve volatile integer/vector
arguments and return registers. The five native routines have no stack arguments
in the verified code; API signatures also fit within four register arguments.
SEH cleanup propagates native exceptions and preserves Win32 last-error state.
The ABI fixture passed 512 integer/vector cases plus exception propagation.

Object addresses are callback-local labels. The worker pins the validated device
and swap chain through hook restoration. An owned, unsubmitted direct queue
discovers the submission method; callbacks do not call COM, submit GPU work,
change resolution, write jitter or initialize a vendor SDK. The trace is limited
to 15 seconds and 262,144 records. Only the first two calls per boundary collect
a bounded 12-address stack. No per-call disk logging occurs on game threads.

At stop, recording closes, callbacks drain, hooks disable, and original method
heads plus all hooked native code spans are checked again. Trampolines and the
DLL remain alive for forwarding calls already in flight. No code is overwritten
on disk. These are hash-gated offsets for this executable, not portable discovery.

The first live main-menu trace passed lifecycle and independent restoration
checks. It recorded 50,537 original calls and 3,596 actual presentations. All
seven boundaries ran on thread 50120. The 3,595 fully contained engine updates
each advanced epoch/temporal by one, toggled the bank and followed this order:
engine entry, bank advance, command recording/publication, game submissions,
native presentation, API presentation, native return, engine return. The capture
started and ended partway through a frame; these partial edges are excluded from
the interval proof. No records were dropped and no exceptions were observed.

Two placement corrections follow from the trace. Engine-update **return is after
Present**, so it cannot represent SimulationEnd. GPU submissions also occur
inside Present from callers outside the game executable; counting every API
submission as game RenderSubmit would mix those operations into the interval.
Each complete observed main-menu frame had five game-origin submissions before
Present and two to four other submissions nested inside Present. This is an
observed frame relationship, not proof of simulation/input semantics in gameplay.

Fresh offline analysis identifies `0x229a080` as command recording and
`0x22980b0` as the subsequent handoff/submission/presentation routine. Within the
latter, `0x23a7f50` submits through the DX12 backend before `0x23aa290` presents.
These are candidates for the rendering interval; an exact SimulationEnd and
consuming-input boundary remain unconfirmed.

An external read verified all five full native code spans and all seven original
32-byte hook heads after stop. The DXGI Present head differed from its disk image,
but matched the probe's retained pre-install bytes exactly. The disk comparison
failure is preserved separately and is **not** a failed restoration. The retained
array was located by its unique five consecutive hash-gated native heads in the
verified probe module, then compared directly to all seven target addresses.
The checks used query/VM_READ only; no remote writes or execution were performed.

The gameplay trace recorded **40,384 original-call pairs**, 2,821 actual presents
and **2,820 complete engine intervals**, all on thread 47856. Each complete
interval followed the same stamp and submission order as the main menu. There
were zero dropped records or exceptions. External reads verified all five full
native functions and seven original heads after automatic stop. The game exited
normally. Native AA/DLSS and graphics settings were unchanged. The post-run MO2
list contains an added enabled effect mod; its timing relative to the capture is
unverified, so this run does not claim an unchanged mod set.

## Frame-token adapter and remaining integration

`src/reflex/frame_policy.hpp` defines an allocation-free, single-thread ordering
policy. Owned monotonically increasing IDs remain distinct even if the game's
temporal counter pauses. The policy requires a stable renderer stamp, one bank
advance, a qualified rendering interval and an actual presentation. TEST
presentations leave the frame unchanged. An invalid order, owner or stamp
disables further SDK actions without fabricating closing markers. Native
originals and hook/resource cleanup remain the integration caller's responsibility.

`FrameSession` borrows an initialized Streamline wrapper and the SDK-owned frame
token. It performs token acquisition, sleep and six PCL markers, including sleep
and markers in Off mode. It counts only successful SDK calls. Pure policy tests
pass; replay accepts all **6,415 complete observed intervals** from the two
captures. Corrupted or truncated replay rows are rejected. This establishes
compatibility with captured counters/order, not semantic marker placement.

An offline vtable map uses 58 bounded, stable subsystem identities captured from
the verified engine object. The calls at `0x485d90` and `0x489800` are input-refresh
candidates; the exact consuming frame and input boundary remain unconfirmed.
Worker jobs also run around the render handoff, so a SimulationEnd marker cannot
be assigned solely from the apparent sequential call order.

In manual mode, Streamline must initialize before the mandatory queue and
swap-chain hooks; the native D3D device may already exist. The native
backend constructor dynamically obtains `CreateDXGIFactory2`; its seven-or-more
argument ABI includes stack arguments and cannot reuse the observational
four-register forwarding stub. The next integration must establish ownership
at documented device/factory creation boundaries, preserve COM lifetimes and
ensure Streamline common presentation maintenance runs on the actual swap chain.
Upgrading a borrowed global pointer or calling a private SDK maintenance export
is not the tested owned-interface route.

See [both native trace results](reflex-frame-boundary-result.md). Early SDK/device
setup and persistent presentation in Off have now passed their game tests.
Next: verify consuming-input and simulation-end placement, then connect the
tested adapter to those boundaries. **Reflex latency reduction is not active
in game yet.** Ordinary FG/MFG wait for that frame contract. The earlier
v0.1.3 [startup/device probe](reflex-startup-device.md) passed its first in-game
launch and independent restoration checks. It initializes in the main-device callback, after
native creation and before return to game queue/swap-chain creation, attaches
the device in Off, and shuts down before rendering starts.
It installs no game proxies, sleep or frame markers. See the
[startup result](reflex-startup-result.md). The v0.1.1 attempt
stopped before hook installation because the graphics modules had not been
loaded. v0.1.2 resolved the system modules but rejected the main-device callback
after missing earlier factory calls; independent reads verified both API heads
and full native creation functions restored. v0.1.3 removes that factory-order
dependency. A new owned-device fixture passed this manual initialization order
with Reflex Off, healthy shutdown and zero final device references.

Installed v0.2.1 subsequently kept the SDK live through main-menu, save loading
and a windowed focus cycle: 49,453 matched routed/native presentations, zero
faults, healthy SDK shutdown and pin release. Independent live checks verified
all 14 original hook heads and both full native creation functions restored.
The user exited normally and configuration hashes stayed unchanged. This is
SDK compatibility evidence, with no token/sleep/marker calls or latency claim.
See [persistent presentation result](reflex-presentation-result.md). Its earlier
v0.2.0 rejection led to the adapter-parent factory lookup: the game's global
renderer is published too late for its device-creation boundary.

Local evidence: `local/reflex-foundation-20261002/`. Proprietary binaries,
decoded assembly and runtime logs remain ignored and are not redistributed.

## Dependency provenance and building

Public headers/source: NVIDIA-RTX/Streamline tag `v2.12.0`, commit
`e8aaa6eaac968711fb62473d4ae8256dde20919b`;
[official release](https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.12.0).
Source license is the repository's MIT-style NVIDIA `license.txt`. Runtime Reflex
components have the release's `reflex.license.txt`; third-party notices are
retained locally. This work does not add runtime redistribution to the repository.

SDK archive SHA-256:
`f5c0a3d870707dddc3570fb4bcd3655cf48a8a68c3a9d342910cfa21b77dcf48`.
Header closure is pinned by `cmake/StreamlineReflex.cmake`; runtime hashes are
pinned in `src/reflex/streamline.cpp`. MinHook remains pinned to v1.3.4 commit
`c3fcafdc10146beb5919319d0683e44e3c30d537`, BSD 2-Clause license.

Set `MHW_STREAMLINE_SDK_ROOT` to an extracted official 2.12.0 SDK, then build
`mhw_reflex_sdk_fixture`. Pass its runtime directory and a **new** output directory.
The observational `mhw_reflex_boundary_probe` builds without Streamline. Both
components use C++20 and `/W4 /WX`; the fixture is a GPU/API test, not a benchmark.
