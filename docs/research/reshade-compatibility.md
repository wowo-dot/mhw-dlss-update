# ReShade and RenoDX with this mod (research, 2026-10-19)

Question (jgb2003-2, mauriwhite-1): can the mod run with ReShade and the
RenoDX HDR add-on? Today it can't: Astera Loader refuses al.d3d12 ("cannot
hook device methods: slot 8: target belongs to reshade.asi") and the mod
never starts.

Sources read (nothing built or run): ReShade main 7bf9de8b (2026-09-24,
BSD-3) and RenoDX main 6d563066 (2026-10-17, MIT), snapshots in
`local/third-party-research/` (SOURCES.md); Astera Loader's
`src/core/graphics/installer.cpp`; this repository at 0.30.1-dev.2. Static
reading only: **[C]** confirmed in the code, **[I]** inferred.

## What ReShade does in a D3D12 game

- **How it gets in [C].**
  - As `reshade.asi` it MinHooks the exports of System32 d3d12.dll and dxgi.dll (`D3D12CreateDevice`, `CreateDXGIFactory*`, …) and returns its own proxy objects.
  - As dxgi.dll/d3d12.dll in the game folder the game calls ReShade's exports directly.
  - Every ReShade module exports `ReShadeVersion`.
- **Wrapped [C]:**
  - the device (one proxy for every interface up to ID3D12Device15);
  - every command queue, and every command list (DIRECT, BUNDLE, COMPUTE, COPY);
  - CBV/SRV/UAV and sampler descriptor heaps (the add-on build, which RenoDX needs, even with no add-on loaded);
  - the DXGI factory and adapter;
  - the swap chain, only when it is created with a ReShade queue proxy.
- **Native [C]:**
  - resources, fences, allocators, heaps, root signatures, PSOs, RTV/DSV heaps;
  - but ReShade patches the native `ID3D12Resource` vtable slot 7 (`GetDevice`), which then returns the ReShade device.
- **Unwrapping [C].**
  - `QueryInterface(IID_UnwrappedObject {7F2C9A11-3B4E-4D6A-812F-5E9CD37A1B42})` returns the native object on every proxy.
  - Each proxy class's own UUID returns the proxy, which makes a cheap "is ReShade" test.
  - `GetDevice` on queue, list, heap and swap-chain proxies, and on any resource, returns the proxy device. On fences, PSOs and root signatures it returns the native device.
- **Proxy and native don't mix [C].**
  - CPU descriptor handles of wrapped heaps are encoded values, not addresses.
  - `SetDescriptorHeaps` and `ExecuteBundle` cast their arguments to ReShade's proxy types unchecked, so a native heap or bundle crashes them.
  - `ExecuteCommandLists` accepts both.
- **Per frame [C].**
  - Before every `ExecuteCommandLists` ReShade submits its own pending list.
  - At Present it renders its effects into the back buffer on its own list, on the native queue the swap chain was created with.
  - `GetDesc` reports the game's original swap-chain description even when an add-on changed it.
- **Streamline [C/I].**
  - ReShade has no DLSS/NGX handling.
  - With `[INSTALL] HookStreamline=1` (and not named d3d*/dxgi) it hooks sl.interposer's exports instead, sitting above Streamline. Otherwise Streamline ends up wrapping ReShade's objects [I].

## What RenoDX's MHW mod does

`src/games/mhworld` [C]:

- **Shaders replaced:** only two pixel shaders, both after TAA:
  - `0x4AA36DCA`, the tone map and LUT grading;
  - `0xF5CB9F69`, the game's own HDR output, its PQ encode with `fHdrOutputWhiteLevel`.

  Its settings arrive as root constants at b13, which it adds to every root signature without changing the existing parameter indices.
- **No resource or swap-chain upgrade.**
  - The rules (R11G11B10 → R16G16B16A16 for every R11G11B10 texture, `ignore_size`) are written, but `swapchain::Use()` is commented out.
  - The Nexus build may differ: check in game.
- Nothing before TAA: scene color, depth, motion vectors and TAA are the game's.
- No DLSS, Streamline or frame-generation code.
  - RenoDX's separate "DLSS Fix" add-on hooks NGX and Streamline to hand them native objects (`NativeFromReShadeProxy`), so NGX and Streamline need unwrapped objects.
- In practice it needs the game's HDR on [I]: the replaced output shader is PQ-only.

## What refuses today

1. **Astera Loader.** `verify_native()` accepts a method only inside System32 dxgi.dll, d3d12.dll or D3D12Core.dll, so ReShade's device refuses at slot 8.
2. **This mod, first stop.** It would refuse even with AL fixed. In worker order:
   - `ngx_startup/startup.cpp` `graphics()`: the swap chain's methods must be dxgi.dll's ("startup-swap-method"; up to 0.40.0-rc.1 reported as `coexistence-NGX-startup`, since 0.40.0 by its own name, which the Status page words as "ReShade or a similar overlay is loaded; remove it");
   - `plugin/device.cpp` `describe_swap`;
   - `projection_binding/discovery.cpp`: `system_target`, with "com-identity" and "resource-device".
3. **This mod, everywhere else.** About 15 more checks assume native objects:
   - methods in system modules: `system_method` on the mod's own lists and queues;
   - device identity by `GetDevice`: `child()`, inline, output, silhouette, stream_route;
   - return addresses inside the game.

   Every site is listed in the appendix.
4. **Presentation.**
   - Today the game holds our native identity chain, made from the native factory, with Streamline under it.
   - Under ReShade the game would hold ReShade's swap-chain proxy over that chain [I]. Whether `identity_same_class` and the live frame-generation switch survive it can only be tested.
5. **HUD-less frame generation.** RenoDX replaces the PQ output that our HUD-less pass reproduces (CBRenderFrame capture), so HUD-less colour would no longer match the final image [I].
6. **A future RenoDX with the upgrade on:**
   - every R11G11B10 texture would become R16G16B16A16, the scene color and TAA output included;
   - our format gates would refuse every frame (producer refusal 13, stream_route 17) while the render scale stays reduced [I]: a blurry image, not a fault;
   - we'd need a guard for that.

## Options

- **A. Above ReShade (recommended to try first).**
  - AL hooks ReShade's proxy methods. It identifies ReShade by `ReShadeVersion` or the proxy UUIDs and accepts that module instead of System32.
  - Our hooks then see the objects the game holds, and game memory and hook arguments agree.
  - This mod would then:
    - compare devices and objects through one helper that unwraps both sides (`IID_UnwrappedObject`);
    - accept ReShade's proxies in the module checks once they unwrap to System32 objects;
    - hand NGX and Streamline native device, lists and queues, as RenoDX's DLSS Fix does;
    - keep using the proxy device for anything it creates, so encoded descriptor handles stay consistent.
  - ReShade's effects then run at Present on the frames DLSS made, before frame generation [I].
- **B. Below ReShade.**
  - AL unwraps and hooks the native code.
  - Our hooks would see native objects that the game's memory doesn't hold, ReShade's own submissions and descriptor heap, and decoded handles.
  - Every identity check would need a native↔proxy map, and ReShade keeps none for queues, lists or heaps. Rejected unless A proves impossible.

## Plan if we do it

1. **ReShade without frame generation first** (`frame_generation_plugin = off`: no Streamline, no identity chain): DLSS Super Resolution and Reflex beside ReShade and RenoDX. This is what the two players asked for (HDR plus DLSS).
2. **Frame generation after it:**
   - the swap-chain layering with Streamline;
   - the live switch;
   - HUD-less under RenoDX: detect the replaced output shader and fall back to frame generation without HUD-less, or capture its output.
3. **Guards:** a clear Status line while ReShade is unsupported, and the format-upgrade case above.

The AL part is in `docs/handoff-al-20261019-reshade.md`.

## Appendix: this mod's sites that assume native objects (0.30.1-dev.2)

Paths under `src/`. The verdicts are for the game's device, queues, lists and
swap chain being ReShade proxies.

**Module checks** (a method must belong to a system module):
- `ngx_startup/startup.cpp` `graphics()`: the swap chain's slots 0 and 12 must belong to dxgi.dll (startup-swap-method). The worker retries 120 s, then stops on startup-swap-method (up to 0.40.0-rc.1: `coexistence-NGX-startup`).
- `plugin/device.cpp` `describe_swap`: slots 0, 2 and 12 must be dxgi.dll's; after 60 s, "presentation".
- `projection_binding/discovery.cpp`:
  - `system_target`, `pin()` (com-identity);
  - its own list's slots, and the device's slots 17 and 10 (system-api-target-N);
  - `refresh_scene` (swap-chain-desc).
- `system_method` on the mod's own lists and queues:
  - `continuous_dlaa/runtime.cpp` (continuous API gate);
  - `temporal_present/observer.cpp` (bindings-system-method);
  - `scaled_temporal/stream_route.cpp` (stream method discovery);
  - `render_resolution/runtime.cpp` viewport telemetry (no refusal).

**Device identity** (`GetDevice` == the game's device):
- `continuous_dlaa/runtime_state.hpp` `child()`, used for list registration at Reset and Execute: fail(19), gate 13;
- `projection_binding/discovery.cpp` (resource-device);
- `scaled_temporal/stream_route.cpp` (plan_rejection 17);
- `silhouette_probe.inl`, `silhouette_draw.cpp` ("silhouette root device": root signatures return the native device);
- `dlss_sdk/inline.cpp` (inline foreign device);
- `temporal_present/output.cpp`, `full_post_inputs.cpp`, `post_census.inl`;
- `temporal_present/unfiltered.cpp`;
- `continuous_dlaa/unfiltered_lifetime.hpp`.

**Device equality between AL and NGX:**
- `ngx_startup` `validate()` via `reflex/temporal_coexistence.cpp` (same-native-device-NGX-owner);
- `reflex/live_frames.cpp` (live-frame-SDK-device, present-owner-or-identity).

**Return addresses inside the game.** These fail if our detours are reached from ReShade's code (option B):
- `continuous_dlaa/runtime_resolve.cpp`, `runtime_lists.cpp`;
- `scaled_temporal/silhouette_route.inl`, `silhouette_probe.inl`, `stream_route.cpp`, `full_post_slot.inl`.

**Presentation:**
- `reflex/presentation.cpp` route (`self` must be the identity or the real chain);
- `identity_same_class` (22 vtable slots);
- `switch_chain`, `publish()`;
- `reflex/identity_chain.hpp`.

**Formats** (an R16G16B16A16 upgrade of R11G11B10 refuses):
- `continuous_dlaa/runtime_requests.cpp` (tBaseMap 26, rwOutput 26);
- `temporal_present/output.cpp`, `unfiltered.cpp`;
- `scaled_temporal/stream_route.cpp`;
- `frame_generation/hudless_pass.cpp`. A back buffer of format 10 skips HUD-less.

**Shader identity.** These pass when an add-on replaces shaders at creation:
- the TAA bytecode SHA-256 from engine memory (`continuous_dlaa/native_taa.hpp`, fail 29 on mismatch);
- the silhouette allowlist (`scaled_temporal/silhouette_shader.cpp`).

  Root and PSO identity against the observer's binds fail if an add-on re-binds cloned pipelines (RenoDX's bind-time fallback, `utils/pipeline.hpp`).
