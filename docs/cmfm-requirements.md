# CMFM requirements (moved)

The DLSS mod's requirements for CMFM (Configuration Menu For Mods) now live in
the CMFM repository: `D:\mh world modding\CMFM\docs
equirements-dlss-mod.md`.
That copy is canonical. Change it there, together with the CMFM agent, when this
mod's needs change.

Summary of what this mod relies on:
- a load point before the game creates its DXGI factory and D3D12 device;
- a C plugin ABI with a consistent plugin folder path (also under MO2);
- this mod as the single presentation owner while frame generation is on;
- CMFM's menu drawn into the game-facing back buffer before Present;
- typed settings, with `mhw_dlss.ini` kept as the source of truth;
- warning popups and the game's AA/DLSS option lockout.

CMFM's entry is its own launcher (decided 2026-10-08, CMFM `docs/decisions.md`
item 6). **CMFM is a requirement of this mod** (user's decision, 2026-10-08).
There is no standalone `winmm.dll` mode any more (removed in v0.13.11). The
mod is a plain `mhw_dlss.dll` in a self-contained plugin folder
(`mhw_dlss.dll`, `mhw_dlss.ini`, NVIDIA runtime, `logs`).

From v0.14.0 the mod is a `cmfm.d3d12` plugin: CMFM owns the DXGI/D3D12 patches
and the mod registers its detours at PRE_MAIN ([research/cmfm-migration.md](research/cmfm-migration.md)).

What CMFM's loader must do for it, in any case:
- Load `mhw_dlss.dll` by full path before the game loads dxgi.dll and creates
  its device. Its DllMain does no work; `cmfm_plugin_init` starts it.
- Load only that DLL from the folder. The NVIDIA DLLs beside it
  (`sl.*.dll`, `nvngx_*.dll`) are not plugins, and the mod loads them itself
  by full path after hash checks.
- Keep the plugin folder separate from the game folder itself. That folder is
  the one place inside the game folder the mod reads its runtime from and
  logs to.
