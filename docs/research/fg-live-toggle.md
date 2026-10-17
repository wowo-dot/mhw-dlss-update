# Frame generation on and off while playing (v0.27, 2026-10-14)

The menu turns frame generation on and off live (the user, 2026-10-13). This
note records why that needs a new swap chain at every switch, what the game
offers for it, and the design.

## Why a loaded but Off DLSS-G is not enough

v0.27.0 loaded DLSS-G at every start and set it Off when the player turned
frame generation off. In game it cost about 40%:

| Run | Frame generation | Rendered fps | p95 |
|---|---|---|---|
| game-129 (v0.27.1) | 2x | 254 | 6.2 ms |
| game-129 | Off (DLSS-G loaded) | 140-157 | 14-20 ms |
| game-131 (v0.27.3) | On | 263 | 4.5 ms |
| game-131 | Off (DLSS-G loaded) | 109-111 | 21 ms |
| v0.26, `frame_generation = off` (DLSS-G not loaded) | Off | ~248 | |

- The CPU sampler (game-130, game-131): with DLSS-G Off the game thread spends
  about 55% of its time in `sub_1423ADE30`, the game's per-frame wait on its
  swap chain's latency waitable (8% while On).
- Not the latency itself: v0.27.3 held the chain at the game's own
  `SetMaximumFrameLatency(3)` for the whole Off stretch (`seen` 3) with no
  change. Streamline's Off path limits how fast presents retire.
- NVIDIA's DLSS-G guide, 18.0: a loaded DLSS-G keeps its extra queue and makes
  the host render off-screen, so Off still costs a copy and a queue sync; "the
  swap-chain must be torn down and re-created every time DLSS-G is switched on
  or off", with `slSetFeatureLoaded` between. The integration checklist (17.0)
  repeats it.

## What the game does (clean image, IDA)

- `cD3DRender::initForD3D12` creates the one DX12 swap chain
  (`mDXGIFactory5->CreateSwapChainForHwnd(mCmdQueue, window, &desc, nullptr,
  nullptr, ...)`), calls `SetMaximumFrameLatency(3)`, and keeps
  `GetFrameLatencyWaitableObject()` at `cD3DRender+0x10F0`. The game never
  creates another main swap chain.
- `sub_1423ADE30(cD3DRender)`, from the game thread's frame start
  (`sub_14229B020` at 0x229b073): if `+0x10D9` (a present happened) and not
  `+0x10DA`, `WaitForSingleObjectEx(+0x10F0, 1000, TRUE)`; a timeout sets
  `+0x10DA`.
- `sRender+0x1F548` holds pending display changes. Bit `0x4000` means the
  swap chain's buffers must be made again: `sub_14229C320` (the HDR switch)
  sets it when the effective HDR state changes, as does `sRender::init`.
  `sub_14229B020` consumes it at the next frame: it waits for the render
  thread (`sub_14229CBF0`), then `sub_1423AC560` -> `cD3DRender::resizeBuffers12`
  flushes the GPU (`sub_1423ADD40`), releases the back buffers, calls
  `ResizeBuffers` at the current size (via `sub_14239F810`/`sub_1423A01D0`,
  with `GetFullscreenState`, `ResizeTarget` and the HDR colour space), and
  `sub_14229B9A0` -> `sub_1423AC9E0` gets the three back buffers again.

So the game offers its own "make the buffers again" request, and inside its
`ResizeBuffers` call no back buffer of the chain is referenced (DXGI requires
that for `ResizeBuffers`; v0.25 logs show its resizes succeed).

## Why the game cannot hold the swap chain that is replaced

Two flip-model swap chains cannot share a window, so the old chain must be
destroyed before the new one exists, and nothing the game holds may be it.
A wrapper object of our own does not work under Astera Loader: AL's factory
layer runs first, sees the object the game receives
(`Hub::swapchain_created`), and installs every plugin's swap-chain detours on
that object's vtable after checking each method is native code
(`NativeInstaller::install`, `verify_native`).

## Design

- **The identity chain.** At the game's `CreateSwapChainForHwnd` the mod makes
  the real chain as today (Streamline's factory proxy, the game's window) and
  also a composition swap chain (`CreateSwapChainForComposition`, no window,
  never presented) on the game's queue with the same size and format. The game
  receives the identity chain. AL hooks it like any DXGI swap chain (same
  class), so plugins and overlays keep working.
- **Routing.** Every method the game or an overlay can call on the identity
  chain goes to the real chain: the routed set of today (Present, Present1,
  GetBuffer, GetCurrentBackBufferIndex, GetDesc, ResizeBuffers, ResizeBuffers1,
  SetFullscreenState) plus GetFullscreenState, ResizeTarget,
  GetContainingOutput, GetFrameStatistics, GetLastPresentCount, GetDesc1,
  GetFullscreenDesc, GetHwnd, the background colour, rotation, source size,
  frame latency and its waitable, matrix transform, colour space and HDR
  metadata. Calls on the real chain itself (Streamline's own) stay native.
- **The switch.** The worker asks for a chain with or without DLSS-G and sets
  the game's `0x4000` request. In the game's `ResizeBuffers` on the identity
  chain the mod releases the real chain (Streamline's proxy goes with it),
  calls `slSetFeatureLoaded`, makes the real chain again with the game's
  parameters and the new size, sets the frame latency, colour space and HDR
  metadata the game set before, and resizes the identity chain to match. The
  presentation contract follows: Streamline-paced with DLSS-G, synchronous
  without.
- **The waitable.** The game read the waitable once, at start. After a switch
  the mod writes the new chain's waitable into `cD3DRender+0x10F0` (checked
  first to hold the handle the mod handed out) and closes the old one. The
  write happens inside the game's own resize on the game thread, the only
  thread that waits on it.
- **When.** DLSS-G is loaded exactly while frame generation is on and DLSS runs.
  DLSS Off and frame generation Off both run without it at full speed. A
  start with frame generation off unloads DLSS-G before the game makes its
  chain.
- **Fail closed.** No switch in exclusive fullscreen, without the identity
  chain, or with the game code or waitable not as expected; a failed creation
  is retried with the previous configuration.

## Implementation (v0.27.4)

- `Streamline::frame_generation_load`: `slSetFeatureLoaded`, and after attach
  the DLSS-G functions are resolved again.
- `identity_chain.hpp`: the 22 routed slots, the composition description and
  the class check. `tests/identity_chain_tests.cpp` (WARP, this Windows): a
  composition chain shares all 22 routed functions with a window chain, each
  its own function; R10G10B10A2 composition chains work.
- `Presentation`: `adopt_identity` at the game's `CreateSwapChainForHwnd`,
  routing of every identity-chain call (faults were meant to keep routing; until
  v0.28.6 one path did not, see "tifa-6" below),
  `switch_chain` in the routed `ResizeBuffers`, `request_switch` and
  `waitable_slot`. A busy window retries creation (E_ACCESSDENIED, 25 x 20 ms).
- The worker (`sync_generation`): wants DLSS-G loaded while frame generation is
  on and DLSS runs; before an unload frame generation is disarmed and its
  inputs settled; it then asks for the switch and sets the game's `0x4000`
  request (again every 1.5 s, up to 20 times). Gates: `sub_14229C320`,
  `sub_1423ADE30`, `sub_14229B020`, `cD3DRender::initForD3D12`.
- At start with frame generation or DLSS off, DLSS-G is unloaded before the
  game makes its chain.

## Hardware fixture (2026-10-14, RTX 5090)

`mhw_frame_generation_switch_fixture` plays the game's part (identity chain,
latency 3 and its waitable each frame, resize for a switch):

| Phase | fps | Generated |
|---|---|---|
| DLSS-G loaded, On (8 ms pace) | 75 | 300 of 300 |
| Loaded, Off | 285 | |
| Switched to unloaded, Off | 301-304 | |
| Switched to loaded, On | 73 | 299 of 300 |

Three switches of 13-76 ms; buffers 6 with DLSS-G and 2 without; the
waitable replaced each time; latency 3 kept; zero device references at the
end. The light test scene keeps the Off gap small; the game showed about 40%.
The existing on fixture (96 of 96 full-multiplier frames, GPU validation) and
`identity_chain_tests` pass.

## tifa-6 (2026-10-17): the identity chain presented, and the game aborted

A player (RTX 5080, 6x dynamic, VSync off, frame generation on at start)
turned frame generation off and on live. Both switches worked (`fg_switch`
reasons 2 and 1, 6 buffers), then the worker stopped with
`fg_stopped "presentation"` and `presentation_state` showed `faults: 1,
routing: false, fault_site: 0, queues: 16, identity: true`. The game then
aborted on purpose (`MtDebug::abort`, a write of 0xDEADBEEF to address 0 at
exe+0x21fd0c9) from `sRender::checkResult`, called with the result of its
`Present(0, DXGI_PRESENT_ALLOW_TEARING)`: `DXGI_ERROR_INVALID_CALL`.

**Cause** (confirmed in code, crash registers and the clean image; review of
2026-10-17, `presentation-review-20261017.md`):

1. The queue hook proxied every queue made on the game's device and kept the
   proxy; ten more than the usual six (made after the game's chain, from other
   threads: Streamline's, an overlay's; which is not known) filled the 16 slots.
2. The 17th threw `presentation-queue-capacity`; the plain `fault()` cleared
   `accepting` without condition (only the routed-call fault spared the
   identity chain), and recorded no site.
3. With `accepting` off, every identity-chain call went to the identity
   chain's own method. It is a composition chain made with no flags and never
   presented: its own Present with the game's tearing flag returns
   `DXGI_ERROR_INVALID_CALL` (`identity_chain_tests`, WARP: 0x887a0001).

**v0.28.6:**

- `identity_route` (`present_result.hpp`, unit-tested): a game call on the
  identity chain goes to the real chain (Streamline's proxy, or natively after
  the last resort below), or without one is answered (Present: S_OK, the frame
  dropped; resize: the last resort, else the identity chain without the game's
  flags; window, fullscreen, latency, colour space and HDR from the game's own
  state, kept for a later real chain), and the frame observer still sees the
  dropped Present,
  never to the identity chain's own method.
- One `fault(site, hr, why)`: the site, HRESULT, `what()` and thread are
  logged (`presentation_fault`, `presentation_state`); routing ends only where
  the game holds the real chain under the synchronous contract
  (`fault_keeps_routing`); a creation that throws still ends it (no chain is
  left). `stop()` ends it for the fixtures (MinHook, after their last frame);
  under AL it refuses while the identity chain exists, as AL's chains stay.
  The alpha never stops while the game runs.
- Queues (`queue_admission`): proxied only before the game's chain exists; a
  full table forwards natively; later ones are counted (`late_queues`, the
  first four callers by module in `frame_heartbeat`) without the route mutex.
- A switch whose two attempts fail (before: `ResizeBuffers` failed, the game
  aborted) makes the real chain natively on the game's window, past
  Streamline, with DLSS-G unloaded (reason 8, `last_resort`); switches are
  then refused. If that fails too the identity chain is resized, the game's
  latency waitable replaced by a set event, its calls answered, and the worker
  asks the game for a resize every 2 s, each trying the last resort again.
- Under SDK pacing a failed Present is a fault and the game gets S_OK, except
  a lost device (`presentation_result`); it aborted on the SDK's HRESULT. If it
  keeps failing (no unload possible), after 300 in a row the game gets the
  HRESULT again (`paced_present_result`): its own abort and crash report
  rather than a frozen picture.
- The worker keeps going after a fault that kept routing: frame generation off
  for the session (`generation_fault`, status "presentation fault", DLSS-G
  unloaded by a switch), DLSS, Reflex and the menu live; `presentation_fault`
  is logged at counts 1, 2, 4, 8... (bounded). The status page names the
  refusal reasons (exclusive fullscreen, latency waitable, switch failed). A switch refused or
  failed is not asked again until the wanted state changes (it was asked
  every 200 ms, each a full buffer remake); a game fullscreen change allows
  one more try.
- `fg_runtime_retained` and `worker_failed` log the real routing state.
- Also: an aspect change with frame generation off no longer leaves it
  unavailable for the session (v0.27.4 bug); the game's latency waitable is
  the one it keeps in its slot (read at arming), not a later caller's.
- Reviewed twice before commit (two adversarial agents: lock order, bypass,
  present accounting, and the runtime scenarios FG off/on, fault under
  pacing, double switch failure, exclusive fullscreen, startup variants,
  exit); their findings are in the commit. Open: `late_queue_callers` may
  name `mhw_dlss.dll` or another plugin instead of the creator under AL; the
  in-game check on the 5090 (tifa's settings, FG off/on, Steam overlay).
