# Persistent Reflex Off presentation, 2026-10-02

The next v0.2.1 diagnostic keeps Streamline 2.12.0 Reflex/PCL initialized during
native DX12 presentation. It does not enable sleep, frame markers, DLAA, DLSS,
frame generation or latency reduction. The accepted v0.1.3 startup result and
working DLSS Performance v1.25.44 remain preserved.

## Interface and hook contract

The game retains native device, factory, command queue and swap-chain pointers.
The bridge owns additional references and private SDK proxies. Native creation
calls pass through the proxy to inform the SDK, then a new owned native reference
is returned to the game. Other device/queue methods stay native. All mandatory
presentation methods use the owned swap-chain proxy; its calls into native
methods reach forwarding trampolines under a TLS recursion guard. No renderer
global, native vtable, command list, backbuffer or asset is replaced.

The two typed public creation hooks use the already-tested startup contract.
The qualified main native device return is RVA `0x23a4e2e`. The factory is
acquired as an owned reference through the native adapter's public `GetParent`.
It must match the factory of the renderer being constructed. The hash-qualified
call supplies that renderer's device output at owner + `0x18`; bounded reads
confirm its adapter at + `0x10`, device at + `0x18`, and factory at + `0` before
installation. The original public API result and Win32 last-error state are
preserved. Setup executes on the creator thread, after device creation and
before queue/swap-chain creation.

The v0.2.0 game run rejected `startup-native-factory-slot` before installing any
presentation hook. Its global lookup was too early: the hash-verified initializer
publishes the renderer at RVA `0x23a72d3`, after creating its queues and swap
chain. v0.2.1 records the adapter-parent factory and checks the still-local owner
without depending on that global. v0.2.0 successfully checked Reflex availability
and shut down the SDK; independent live reads verified both public API heads and
the two full native creation functions restored. User normal exit and unchanged
configuration checks passed. The failed run and its source are preserved under
`local/reflex-presentation-20261002/game-presentation-01-archive/` and
`source-presentation-0.2.0/`. This is a clean rejection, not persistent game
acceptance.

| Native interface | Mandatory method | Vtable slot |
| --- | --- | --- |
| ID3D12Device | CreateCommandQueue | 8 |
| IDXGIFactory | CreateSwapChain | 10 |
| IDXGIFactory2 | CreateSwapChainForHwnd / ForCoreWindow | 15 / 16 |
| IDXGISwapChain | Present / GetBuffer / SetFullscreenState / GetDesc / ResizeBuffers | 8 / 9 / 10 / 12 / 13 |
| IDXGISwapChain1 | Present1 | 22 |
| IDXGISwapChain3 | GetCurrentBackBufferIndex / ResizeBuffers1 | 36 / 39 |

Each detour uses the exact public `STDMETHODCALLTYPE` signature, including stack
arguments. Targets are discovered from the qualified native interfaces and
must be distinct executable image addresses. Owned device/factory identities
are checked through public `slGetNativeInterface`. Queue ownership is bounded
at 16 retained proxies; an exhausted bound rejects feature routing. Calls on
unrelated interfaces pass through unchanged. One tracked swap chain is supported,
consistent with this SDK integration. Bridge pins for the previous chain are
released before recreation so they cannot block another flip-model chain for
the same HWND. The game must still release its own obsolete references.

Native Present is captured under the recursion guard: there must be exactly
one native call per routed frame. Its actual HRESULT is returned even if the
SDK's after-hook replaces a status result. An SDK rejection before native
presentation falls back to one native call; a rejection afterward never repeats
presentation. `DXGI_PRESENT_TEST` is counted separately and does not count as a
frame. Reflex Off means no generated frames or expected Present skipping.

Stop first closes routing, synchronizes with dynamic hook installation, disables
all hooks and drains callbacks. Every captured 32-byte head must match its
original. SDK shutdown precedes release of owned proxies/native pins. Failed
shutdown retains pins. Trampolines, dispatch storage and modules remain resident
for stale forwarding entries. The game continues through native interfaces
after stop. There are no SDK calls from `DllMain` or CRT process-detach cleanup;
an early process exit is not recorded as verified shutdown.

Tested retail executable SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The complete native constructor and DX12 initializer retain their startup gates
(4,158 and 11,101 bytes). These remain experimental hash-qualified offsets;
portable signature discovery is later work.

## Isolated validation

The exact bridge passed an owned DX12 fixture with 144 GPU-completed routed
frames and 144 native Present calls. It alternated Present/Present1, exercised
a TEST call, GetBuffer/GetDesc/current index/windowed state, ResizeBuffers and
ResizeBuffers1, and recreated the swap chain for the same hidden HWND. Two
resizes, two chains and one queue passed with zero faults. All 12 COM heads
restored, early pin release was rejected, SDK shutdown completed, and the host
performed native presentation afterward. Final native device references were
zero. The SDK log ended with zero delayed resources and no warning/error records.

Both targets compile with `/W4 /WX`. Ownership and frame-policy CTests passed;
three strict presentation-review test groups cover lifecycle omissions/order,
activation flags, mismatched native identities, duplicate heads, pending calls
and missing/duplicate native presentation. This is fixture evidence, not an
in-game result. Ignored evidence is under
`local/reflex-presentation-20261002/owned-03/`; this fixture additionally acquired
the original factory through the same adapter-parent helper before queue/swap
creation. Earlier fixtures are preserved under `owned-01/` and `owned-02/` with
their source freezes. The first v0.2.1 game run subsequently passed 49,453 matched
presentations with zero faults through main-menu, save-load and focus checks;
all 14 heads were independently verified restored after intentional stop. See
[game result](reflex-presentation-result.md). This establishes compatibility in
the tested run, not active Reflex latency reduction.

## Game test

Install the probe in the usual MO2 dlssTEST slot only with the game closed,
preserving exact prior DLL/output/runtime-sidecar bytes. Runtime dependencies
remain under the repository. The runtime sidecar and mode file stay unchanged;
the output sidecar selects a fresh ignored run directory.

Launch normally in solo/offline mode, keep native AA/DLSS Off and window size
unchanged, and reach the main menu. The SDK activates automatically in Off.
There is no manual DLSS toggle or arming. After its native swap-chain snapshot,
the recorder runs until its unique `<log>.stop` file exists, a rejection occurs,
or a ten-minute ceiling expires. A changed native main device stops this
diagnostic; rebinding the SDK to a replacement device is not implemented yet.
Periodic counters are bounded to one record per five seconds. SDK frame
maintenance runs through the public proxy; no private common export is called.

Verify the main-menu baseline, then one save load/focus cycle. Stop intentionally
before exiting, review with `tools/review_reflex_presentation.py`, and independently
read retained original heads and full native gates while that process is still
open. Require matched routed/native Present counts, zero faults, native host
interfaces, SDK shutdown, pin release and all 14 heads restored. A visual result
alone does not establish persistent compatibility.

Exact removal with the game closed: restore the run's `previous-slot.dll.disabled`
to its recorded DLL destination, `output-sidecar.before.bin` to its recorded
output sidecar and `runtime-sidecar.before.bin` to its recorded runtime sidecar.
Keep the current diagnostic installed between runs as requested.

After persistent presentation is accepted, verify input/simulation semantics
and connect the frame-token adapter and markers before testing Reflex On/Boost.
No in-game latency benefit is claimed here. Ordinary FG/MFG follow that complete
contract. See [startup result](reflex-startup-result.md) and
[native frame map](reflex-frame-boundary-result.md).
