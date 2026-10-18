# To Astera Loader: al.d3d12 with ReShade (2026-10-19)

For the Astera Loader session (`D:\mh world modding\AsteraLoader`). The
Astera Loader repository is read-only from here; nothing in it has been
changed. Background and the DLSS mod's side: the DLSS repository's
`docs/research/reshade-compatibility.md`. It is a proposal for the user and
the AL session to decide, not a request to start.

## What happens today (confirmed)

- jgb2003-2's log (2026-10-18), with `reshade.asi` for RenoDX HDR:
  `al.d3d12 is unavailable: cannot hook device methods: slot 8: target
  belongs to reshade.asi`. AL marks the mod degraded and the DLSS mod never
  starts.
- Cause: `src/core/graphics/installer.cpp` `NativeInstaller::install` reads
  the vtable slot of the object it is given (the game's device, which is
  ReShade's proxy) and `verify_native()` accepts only System32 dxgi.dll,
  d3d12.dll or D3D12Core.dll.

## ReShade's D3D12 objects (ReShade main 7bf9de8b, static reading)

- **Proxies the game holds:**
  - the device (one proxy for every ID3D12Device version);
  - every command queue and command list;
  - CBV/SRV/UAV and sampler descriptor heaps (in the add-on build, which RenoDX needs);
  - the factory and adapter;
  - the swap chain, when it is created on a ReShade queue proxy.
- **Native:** resources, fences, allocators, root signatures and PSOs. But ReShade MinHooks the native `ID3D12Resource` vtable slot 7 (`GetDevice`), so that slot points into reshade.asi too.
- **Detection:**
  - every ReShade module exports `ReShadeVersion`;
  - `QueryInterface` with a proxy class's UUID returns the proxy (device `2523AFF4-978B-4939-BA16-8EE876A4CB2A`, queue `2C576D2A-0C1C-4D1D-AD7C-BC4FAEC15ABC`, command list `479B29E3-9A2C-11D0-B696-00A0C903487A`, swap chain `1F445F9F-9887-4C4C-9055-4E3BADAFCCA8`, factory `019778D4-A03A-7AF4-B889-E92362D20238`).
- **Unwrapping:** `QueryInterface(IID_UnwrappedObject {7F2C9A11-3B4E-4D6A-812F-5E9CD37A1B42})` returns the native object, AddRef'd. Older ReShade releases may lack it; the fallback is `GetDevice` on a fence made through the proxy, which returns the native device.
- **Proxy and native don't mix:**
  - wrapped heaps hand out encoded CPU descriptor handles;
  - the proxy's `SetDescriptorHeaps` and `ExecuteBundle` cast their arguments to proxy types unchecked.
- **Locks:** `D3D12CreateDevice` holds ReShade's non-recursive adapter mutex for the whole call. The proxy's AddRef and Release and the resource `GetDevice` hook take it too, so calling them from a hook inside device creation would deadlock [I].

## The proposal: hook above ReShade

The DLSS mod's analysis prefers this: plugins see the objects the game
holds, so game memory and hook arguments agree.

- When a vtable target's module exports `ReShadeVersion` (or the object answers the proxy UUID), accept that module in place of System32 for that object family. Log it (`d3d12_reshade version=… families=…`). The MinHook target is ReShade's proxy method, shared by all proxies of that class.
- Keep refusing every other non-system module, as for Smooth Motion and Stracker's factory hook.
- Plugins then receive proxies in `DEVICE_CREATED` and `SWAPCHAIN_CREATED` and as `this` in chains. Document for plugin authors that they must unwrap objects before handing them to NGX or Streamline, and keep descriptor heaps and lists consistently proxy or native.
- **Open questions for AL:**
  - which object AL describes in `swapchain_created` and subclasses for input (the swap-chain proxy forwards `GetHwnd`);
  - whether the factory hooks (slots 10, 15, 16) also land on ReShade's factory proxy;
  - whether AL's own internal objects (`begin_internal`) are created through ReShade's device.

Hooking below ReShade (unwrap, then patch the native code) was considered and
rejected for now: the game's memory holds proxies, ReShade keeps no
native→proxy map for queues, lists or heaps, and plugins would also see
ReShade's own submissions and its descriptor heap.

## What the DLSS mod changes on its side (for context)

- One unwrap-aware identity helper for its device and `GetDevice` comparisons, and module checks that accept a ReShade proxy over System32 objects.
- Native objects handed to NGX and Streamline.
- First milestone without frame generation (no Streamline): DLSS Super Resolution and Reflex beside ReShade and RenoDX. Frame generation's swap-chain layering after that.
