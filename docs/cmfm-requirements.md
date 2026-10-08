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
item 6). Steam's Play button without launch options does not start it, so the
mod keeps its standalone `winmm.dll` + `mhw_dlss\` mode permanently, not only
until CMFM exists ([research/mod-loaders.md](research/mod-loaders.md)).
