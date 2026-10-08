# MHW mod loaders and the alpha's winmm.dll (2026-10-08)

Source read (not built or run): Strackeror/MHW-QuestLoader at 9770302 (2024-06-06)
and Fexty12573/SharpPluginLoader at 7be2393 (2026-10-08), both MIT. Executable:
sha c2ebbbd2…, PE timestamp 0x66D13CB6.

## Confirmed

Game imports (packed exe, read from the PE directories):
- Static: KERNEL32 only.
- Delay: USER32, SHELL32, ole32, OLEAUT32, IMM32, DINPUT8, XINPUT9_1_0, d3d11,
  d3d12, WS2_32, IPHLPAPI, steam_api64, WINMM, amd_ags_x64, MFPlat,
  MFReadWrite, WININET, SETUPAPI, GDI32, ADVAPI32.
- Not KnownDLLs (so the game folder copy wins): DINPUT8, XINPUT9_1_0, d3d11,
  d3d12, IPHLPAPI, WINMM, MFPlat, MFReadWrite, WININET.

So winmm.dll is mapped at the game's first winmm call, not at process start as
the v0.13.8 commit message says. The exception is when an earlier module
imports it statically. Steam's GameOverlayRenderer64.dll does; steam_api64,
amd_ags_x64, d3d12, dxgi, dinput8 and XInput9_1_0 do not.

Stracker's Loader (open source; the installed copy in MO2 matches it):
- loader.dll is built 2024-06-06 12:48 UTC, 19 minutes before the last commit.
  Its strings match loaderdll.cpp.
- dinput8.dll is a DirectInput8Create pass-through. Its DllMain calls
  LoadLibrary("./loader.dll") and then Initialize.
- Initialize reads loader-config.json, finds the game version by signature,
  then LoadLibrary's every nativePC\plugins\*.dll.
- Plugins therefore load when the game first delay-loads DINPUT8.

SharpPluginLoader (SPL) 0.0.8:
- Ships its native DLL as winmm.dll on Windows (ucrtbase.dll on Linux).
  It forwards the exports to System32. It only activates inside
  MonsterHunterWorld.exe.
- Preloader, from DllMain:
  - resets the exe's load-config security cookie to the MSVC default;
  - inline-hooks GetSystemTimeAsFileTime;
  - on the post-unpack `__security_init_cookie` call, hooks
    `__scrt_common_main_seh`, WinMain and sMhMain's constructor.
- Needs winmm mapped before CRT init. Its install doc says that without Steam's
  overlay injection the game closes at startup.
- `loader-config.json` `SPL.PreloadDLLs`: LoadLibraryA on each listed path at
  that post-unpack, pre-CRT point. This is before WinMain, so before the
  device and swap chain.
- SPL shares loader-config.json with Stracker's Loader (adds an `SPL` object)
  and rewrites it every launch.
- Hosts .NET 8 (hostfxr). C# plugins live in nativePC\plugins\CSharp.
- D3D12 ImGui/primitive overlay, set up from a mid-hook on the sMhRender
  assignment:
  - takes the swap chain from renderer+0x1470 and the queue from renderer+0x20;
  - inline-hooks that swap chain's Present (vtable 8) and ResizeBuffers (13),
    and the queue's Signal (14);
  - draws on the back buffer in Present, via ExecuteCommandLists on the game
    queue.

## Confirmed in game (2026-10-08)

Load order is from the live PEB module list, read with
PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ.

- game-50 (MO2 force-load): Stracker's dinput8.dll loaded at 116, after
  sl.interposer (91) and the NGX/DLSS-G runtime (96–109). Its plugins load
  after frame generation is already set up, so that path is too late for us.
- game-51 (v0.13.8 as winmm.dll in an MO2 mod): the proxy was mapped at 43,
  System32 WINMM at 31 and d3d12.dll at 45. It then failed closed before
  logging: under usvfs, GetModuleFileNameW gives the virtual game path but
  weakly_canonical() gives the real mod folder. Fixed in v0.13.9 (lexical
  comparison, startup-error.jsonl).
- game-52 (v0.13.9, same layout): the hooks went in before the game created
  its device and factory:
  - the D3D12CreateDevice hook logged `device_checked` for the game's own call
    (return RVA 0x23a4e2e);
  - dxgi.dll was not yet loaded when CreateDXGIFactory2 was hooked
    (api_module kind 0, was_loaded false). d3d12.dll was already loaded (kind 1).
  Startup:
  - NGX accepted the shared provider by path and hash under the VFS;
  - the alpha armed 6.2 s after launch.
  Run totals:
  - 15,285 DLSS evaluations, 3 skips, 0 route faults;
  - 15,284 HUD-less frames tagged, 0 no_restore;
  - 0 exceptions, 0 Streamline errors.

## Implications (hypotheses until tested)

- SPL and the alpha both install winmm.dll in the game folder. One overwrites
  the other.
- With SPL, the alpha can load through `SPL.PreloadDLLs`, earlier than its own
  winmm.dll would. This needs the alpha to accept running as
  mhw_dlss\<name>.dll, with the ini next to it, as its home folder. Today only
  `<dll dir>\mhw_dlss\mhw_dlss.ini` sets alpha_home.
- With frame generation, the swap chain at renderer+0x1470 is presumably
  Streamline's proxy. SPL's inline Present hook would then patch Streamline
  code, and its overlay would reach DLSS-G as UI (present in the final colour,
  absent from the HUD-less colour). Not tested.
- A winmm load that arrives late would race the alpha's asynchronous worker
  against the game's D3D12 device creation. The MO2 force-load avoided this.
  game-51 shows the real order.
