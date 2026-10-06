# Owned DX12 frame-generation foundation

2026-10-02: ordinary FG and fixed-count MFG execute in our own DX12 renderer.
This establishes the SDK input, presentation and shutdown contracts. It is
not an MHW FG deployment, a visual-quality acceptance, or a performance result.
The installed combined DLSS/Reflex game probe remains v0.5.1, SHA256
`63bc422cef32bbf4775ac80b779e557ca1bc756259256e618e055e6e47c68637`.

## Accepted evidence

Private evidence is under `local/frame-generation-foundation-20261002`.
Generated SDK binaries, logs, fixture executables and manifests remain untracked.
Each run has an exclusive output folder, executable SHA256, immutable source
manifest and exit receipt. Final On/MFG source is `source-0.3.2`; Off regression
and default Reflex regression use the recorded `source-0.3.1` contract.

| Run | Mode | Host frames | Enabled frames | SDK-reported presentations | Extra presentations |
| --- | --- | ---: | ---: | ---: | ---: |
| `owned-10` | FG-loaded Off | 144 | 0 | 144 | 0 |
| `owned-on-07` | Ordinary 2x | 144 | 96 | 240 | 96 |
| `owned-mfg-04` | Fixed 3x | 144 | 96 | 336 | 192 |
| `owned-mfg-05` | Fixed 4x | 144 | 96 | 432 | 288 |
| `owned-mfg-06` | Fixed 6x | 144 | 96 | 624 | 480 |

On/MFG runs warm up Off for 32 frames, enable interpolation for frames 33-128,
then return Off for 16 frames. Every enabled observation reports its configured
multiplier and foreground ownership. They each complete 144 Reflex tokens,
144 Sleep calls and 864 ordered markers. Tagged inputs are retained until the
SDK consumption fence completes; the presenting queue is also fenced before
the next upload. All 112 positive input-fence observations complete per run.

All accepted runs report zero frame/presentation faults, zero D3D12/DXGI debug
warnings/errors, effective Reflex On followed by Off, SDK shutdown, restored
native hook bytes, released pins, a successful new native-chain presentation,
and zero final device references. Off additionally tests ResizeBuffers and an
equivalent single-queue ResizeBuffers1 at frames 48 and 96. On/MFG resize and
device recreation have not been tested.

`owned-direct-07` independently executes ordinary FG through SDK interfaces
with no native presentation hooks: 96 extra presentations, zero graphics
validation errors and zero final device references. Its earlier v0.3.1 input
log has a wrong numeric state label; source and actual API arguments use 64.
Current strict review accepts corrected v0.3.2 On/MFG logs, not those older labels.

SDK `numFramesActuallyPresented` is the count since its previous state query.
It is queried after each Present on the same thread. These are SDK execution
observations, not an independent display scanout measurement, GPU output image
readback, latency measurement or guarantee that every future frame is generated.
The bridge's synchronous `native_presents` counter does not count asynchronous
generated presentations; it must not be used as an FG FPS counter.

## Dependencies and capability

Streamline 2.12.0 is pinned to public commit
`e8aaa6eaac968711fb62473d4ae8256dde20919b`.
Release ZIP SHA256:
`f5c0a3d870707dddc3570fb4bcd3655cf48a8a68c3a9d342910cfa21b77dcf48`.
`dependency-receipt.json` records all 37 selected files and their hashes.
All six loaded production DLLs are hash-pinned and signature-verified before
SDK execution: interposer, common, PCL, Reflex, DLSS-G and NGX DLSS-G.
No OTA/downloaded-plugin loading is enabled. Default Reflex-only selection
still requests its original two features and four verified runtime files.

The selected NVIDIA adapter reports vendor 4318, device 11141, LUID 93911/0,
one viewport, four required tags, minimum dimension 100, maximum five generated
frames, dynamic-MFG availability and VSync support. Only fixed modes were tested;
dynamic/automatic MFG was not enabled. The 6x result applies to this adapter's
reported capability, not every FG-capable GPU. Requests above the queried
maximum are rejected before setting options; ordinary-only adapters cannot
accept an MFG request. No driver profile changes were made by these tests.

The SDK is initialized before DXGI/device creation in the owned fixtures.
The existing public project UUID is used through the documented ProjectID path;
no borrowed game AppID or support spoofing is involved. Runtime licensing remains
separate from the public source/header license; see `THIRD_PARTY_NOTICES.md`.

## Inputs and frame alignment

`FrameSession::input_token` borrows the exact current Reflex token only on its
owner thread before Present. It cannot be borrowed during Present, after end,
or after a fault. FG does not request a second token or use a separate frame ID.
Constants and all five resource tags use that token and viewport zero.

| Input | Owned test format | Size | State supplied to SDK |
| --- | --- | --- | --- |
| Depth | R32_FLOAT (41) | 1280x720 | NON_PIXEL_SHADER_RESOURCE (64) |
| Motion | R32G32_FLOAT (16) | 1280x720 | NON_PIXEL_SHADER_RESOURCE (64) |
| HUDless color | B8G8R8A8_UNORM (87) | 1280x720 | NON_PIXEL_SHADER_RESOURCE (64) |
| UI alpha | R8_UNORM (61) | 1280x720 | NON_PIXEL_SHADER_RESOURCE (64) |
| Backbuffer extent | Null resource plus full extent | 1280x720 | SDK interception |

The stationary camera has explicit non-jittered row-major projection/inverse,
identity current/previous clip transforms, position/basis, near/far, FOV and
aspect. A checkerboard and moving square provide matching color/depth/motion;
UI is empty. HUDless color is copied into the final test backbuffer. This tests
resource plumbing and execution; it does not exercise real MHW effects, camera
cuts, transparent HUD composition or DLSS/FG coexistence.

Tags use `eValidUntilPresent`; volatile copy-at-tag-time lifetimes are rejected
by this wrapper until a corresponding command-list contract exists. All four
input tags are cleared when returning Off. The wrapper does not manually call
an FG evaluation API: execution is owned by SDK presentation.

## Native routing and lifetime

The default synchronous Reflex path retains its original native HRESULT and
exactly-one-native-Present checks. The explicit SDK-paced path returns the SDK
transaction result and never inserts an extra native Present to compensate for
a zero/multiple synchronous count. FG-loaded Off already uses a pacer and
offscreen buffers; it is not the same ownership contract as Reflex-only Off.

The FG pacer can call methods on the very same physical swap chain from another
thread. Routing those calls to logical SDK indices/buffers causes invalid native
backbuffer writes. The owned paced contract admits host calls only on the
chain-creation thread and exact host chain. Worker/other-chain calls reach the
original DXGI implementation before acquiring the host route mutex. TLS guards
also protect recursive SDK calls on the host thread. This single-host-thread
foundation is not yet the MHW renderer's validated thread-ownership map.

All hooks are x64 Windows COM methods (`STDMETHODCALLTYPE`) with exact native
interface arguments. Targets are discovered from validated interface vtables,
checked as executable MEM_IMAGE code, and preserved/restored as original bytes;
no MHW executable offsets or patches are introduced here.

| Interface | Slots / methods |
| --- | --- |
| ID3D12Device | 8 CreateCommandQueue |
| IDXGIFactory/2 | 10 CreateSwapChain, 15 CreateSwapChainForHwnd, 16 CreateSwapChainForCoreWindow |
| IDXGISwapChain | 8 Present, 9 GetBuffer, 10 SetFullscreenState, 12 GetDesc, 13 ResizeBuffers |
| IDXGISwapChain1 | 22 Present1 |
| IDXGISwapChain3 | 36 GetCurrentBackBufferIndex, 39 ResizeBuffers1 |

The host retains native interfaces; bridge-owned SDK proxies/pins outlive
callback drain and SDK shutdown. State/trampoline dispatch storage is deliberately
process-lived. Exact tested fixture hashes are in the run receipts; system COM
functions are not signatures derived from a game dump. The retail game hash
remains `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

Owned swap chains use BGRA8, flip-discard, two logical buffers, and flags 2112
(FRAME_LATENCY_WAITABLE_OBJECT plus ALLOW_TEARING). The SDK's windowed pacer
requests ALLOW_TEARING even while Off. Resize flags must preserve the contract.
The 2.12 ResizeBuffers1 route raised E_ABORT. Translation to ResizeBuffers is
admitted only for explicit bounded buffer count, one GPU node, mask one and
every buffer still assigned to the original creation queue. Other assignments
return DXGI_ERROR_INVALID_CALL; they are not silently discarded.

Shutdown stops/drains/restores hooks before SDK shutdown, then releases proxies
and pins. Returning to native rendering requires a **new native swap chain**
on the native host queue. The FG-created physical chain belongs to the SDK
pacer queue; presenting it after shutdown creates work the host fence cannot
cover. Direct SDK tests obtain a new owned native queue reference while the SDK
is alive and use it for the replacement chain. The interposer module remains
loaded until process exit because surviving COM proxy vtables belong to it.

## Rejected controls and corrections

All rejected attempts remain archived. Important distinctions:

- v0.1.0-0.1.3: synchronous Present assumption and missing waitable/tearing flags.
- v0.1.5: unsupported SDK ResizeBuffers1; no successful drain/pin release claim.
  The vendor exception handler also created a dump under ProgramData/NVIDIA/
  Streamline outside our requested log folder. It was not read, copied or
  redistributed. Subsequent fixtures avoid that rejected route.
- v0.1.6-0.1.7: post-shutdown use of the FG physical chain; corrected by new
  native-chain creation and completion before releasing resources (v0.1.8).
- v0.2.0-0.2.6: background On controls returned one presentation despite API/status
  success. Moving inputs and a direct SDK control did not resolve background
  suppression. A zero-valued input fence was already complete and must not be
  passed to SetEventOnCompletion.
- v0.2.7 focused bridge control: first two presentations, then 21 D3D12/DXGI
  errors/device removal due to worker calls routed through logical buffers.
- v0.2.6/0.2.7 direct control: extra-frame execution when foreground, but a stale
  SDK queue proxy was passed to native post-shutdown creation, causing heap
  termination (0xc0000374). v0.3.0 then exposed a temporary owned native-queue
  reference retained until after the final device check. Both defects corrected.
- v0.2.8/v0.3.1 execution passes: inputs actually supplied state 64, but a copied
  diagnostic literal printed 8. v0.3.2 derives the label from the actual enum and
  repeats all four fixed modes; strict review rejects the old label.
- `owned-on-06`: Windows refused foreground ownership within the bounded wait;
  the run stopped before FG activation. `owned-on-07` and the three final MFG
  checks passed after the user focused the owned test windows.

SDK diagnostics are separate from graphics validation: a GetState synchronization
reminder is emitted even though this fixture serializes it with Present. The Off
fixture additionally warns about defaulting an optional backbuffer extent. NGX
logs probes for optional/cache plugins that are not part of this dependency set.
Zero graphics errors must not be paraphrased as zero SDK warnings of all kinds.

## Verification and next work

Warnings-as-errors native builds pass. Five native Reflex/presentation tests and
53 Python review tests pass, including negative evidence for success-without-
interpolation, wrong token, unsupported count, wrong state label, worker routing,
incomplete graphics completion and false cleanup claims. `review_frame_generation_fixture.py`
recomputes presentation counts and requires complete On/Off, foreground, Reflex,
graphics validation and native-return evidence. Review files carry exact log hashes.

Default Reflex regression `reflex-regression-03` passes 144 complete frames/864
markers, two resizes and recreation, 144 matched native presentations, driver
On->Off, SDK shutdown and zero final device references. `reflex-regression-02`
was only a rejected CLI invocation (`1` instead of `on`); it installed nothing.

Next connect MHW's real presentation owner/token, non-jittered camera constants,
depth/motion and **final post-processed HUDless color plus UI separation** with
explicit producer/consumption fences. The existing earlier DLSS reconstruction
output is not automatically that final HUDless image. Establish native-chain
shutdown/recreation and loading/focus suspension before enabling ordinary FG in
the game; then validate MFG using the same inputs and queried capability.
CMFM, alpha packaging, presets and performance optimization remain deferred.
