# MHW DLSS

An open-source mod that brings current NVIDIA technology to Monster Hunter:
World (PC, DirectX 12):

- **DLSS Super Resolution and DLAA** on the game's own TAA pass. Modes: DLAA,
  Quality, Balanced and Performance. They change live, render resolution
  included.
- **DLSS Frame Generation**:
  - 2x on RTX 40 and 50 cards; 3x to 6x and Dynamic multi frame generation on
    RTX 50;
  - generated frames use a HUD-less image, so the HUD stays steady;
  - turns on and off while playing.
- **NVIDIA Reflex**: On, Boost or Off.
- **An in-game settings page** through CMFM, if that's installed, and a
  `mhw_dlss.ini` otherwise.

The mod is an [Astera Loader](https://github.com/wowo-dot/AsteraLoader) plugin.
It checks every piece of game code it hooks against the tested executable by
SHA-256. If anything doesn't match, or the card can't run a feature, that
feature stays off and the game renders as it does unmodded.

**Status:** v0.28.5, the first public release, after a closed beta on RTX 5090,
5080 and 3080 Ti cards (frame generation on RTX 40 cards is untested so far).
Download it from [Releases](https://github.com/wowo-dot/mhw-dlss-update/releases)
or [Nexus Mods](https://www.nexusmods.com/monsterhunterworld/mods/8857).

## Requirements

- Monster Hunter: World on Steam with DirectX 12. Tested executable:
  `MonsterHunterWorld.exe` from the current Steam build (15.23).
- An NVIDIA GeForce RTX card:
  - Frame Generation needs RTX 40 or 50;
  - 3x and above need RTX 50;
  - Dynamic also needs driver 595.41 or newer.
- [Astera Loader](https://github.com/wowo-dot/AsteraLoader/releases) 0.2.2 or
  newer. [CMFM](https://github.com/wowo-dot/CMFM/releases) 0.1.0 or newer
  (Astera Loader's menu plugin) is optional; it adds the settings page.
- The Microsoft Visual C++ 2015–2022 Redistributable (x64).

While DLSS runs, the mod sets the game's Anti-Aliasing and Resolution Scaling
options the way the game's own DLSS did; they show greyed out. Your choices
come back when DLSS is Off.

The game's own DLSS 1 is unavailable while the mod is installed. The mod runs
only where the game has no DLSS 1 context, so it declines it
([details](docs/research/game-dlss1.md)); RTX 50 cards can't run DLSS 1
anyway.

## Install and use

Release packages are zips laid out like the game folder:
1. Install Astera Loader (and CMFM, for the menu) as their READMEs say.
2. Copy this zip's `AsteraLoader` folder next to `MonsterHunterWorld.exe`, or
   install the zip as a Mod Organizer 2 mod.
3. Start the game through Astera Loader: with Mod Organizer 2 through its
   `AsteraLoader.exe` entry; otherwise, with Steam running, start
   `AsteraLoader.exe`. For Steam's Play button to start it too, set the
   game's launch options to `"<game folder>\AsteraLoader.exe" %command%`.
4. DLSS, frame generation and Reflex start **off**. Turn them on under
   **Mods > DLSS > Settings** (the entry below Options), or in the ini.

Where things live:
- **Settings:** `AsteraLoader/plugins/mhw_dlss/mhw_dlss.ini` (each key is
  explained in the file), or with CMFM in game under **Mods > DLSS** (the
  entry below Options).
  The menu applies changes at once and saves them to the ini.
- **Logs:** `AsteraLoader/plugins/mhw_dlss/logs`, one file per launch. Please
  attach yours when reporting a problem.
- **Uninstall:** delete `AsteraLoader/plugins/mhw_dlss`.

The package's own `README.txt` covers every option.

## Planned

No dates yet:
- **AMD FSR 3.1** upscaling, so cards without DLSS (AMD, Intel, NVIDIA GTX)
  get it too;
- **AMD FSR 4** on Radeon RX 9000 cards;
- **Intel XeSS**;
- finding what causes the short DLSS pauses seen once in the closed beta
  (four times in a two-hour DLAA and frame generation 3x session at 21:9).

## Building

You need:
- Visual Studio 2022 or newer with the C++ x64 tools, and CMake 3.24 or newer;
- Python 3.12, for the tool tests and packaging;
- two public NVIDIA SDKs, kept outside the repository. CMake checks each file
  it uses by SHA-256:
  - the [DLSS SDK](https://github.com/NVIDIA/DLSS) at commit `a291cc7d`
    (`MHW_NGX_SDK_ROOT`);
  - the [Streamline SDK](https://github.com/NVIDIA-RTX/Streamline) 2.12.0
    (`MHW_STREAMLINE_SDK_ROOT`).

MinHook 1.3.4 is vendored in `third_party/minhook` (pinned commit `c3fcafdc`) for
the tests' stand-in Astera Loader; the shipped DLL links no hooking library, and
the build needs no download.

```powershell
cmake -S . -B build -A x64 -DMHW_NGX_SDK_ROOT=<dlss sdk> -DMHW_STREAMLINE_SDK_ROOT=<streamline sdk>
cmake --build build --config Release --target mhw_dlss_alpha
```

The mod is `build/bin/Release/mhw_dlss_alpha.dll`, renamed `mhw_dlss.dll` in
the package. Leave out `--target` to build the tests as well.

**Tests:** run `ctest --test-dir build -C Release -j 32` (84 tests, about ten
seconds: the C++ tests and one for the Python tools). They come in two tiers:
- `quick`: everything under about a second. While iterating, run
  `ctest -L quick` plus the changed area's own tests (`ctest -R <name>`).
- `heavy`: WARP pipeline fixtures. Each one reserves four processor slots, so
  a parallel run doesn't push them past their timeouts.

Run the whole suite before handing out a build.

**Packaging** needs a committed tree:

```powershell
py -3 -B tools/package_release.py --dll build/bin/Release/mhw_dlss_alpha.dll --version <x.y.z> --streamline-sdk <streamline sdk> --dlss-sdk <dlss sdk> --out <folder>
```

It copies NVIDIA's runtime DLLs only if they match the tested runtime's
hashes (`tools/fg_runtime.py`), and adds their licenses and the notices for
everything compiled into the DLL.

## Repository layout

- **`src/`**: the mod. The `mhw_dlss_alpha` target in `CMakeLists.txt` lists
  every source file it compiles; [`docs/architecture.md`](docs/architecture.md)
  maps how they fit together. The main parts:
  - `continuous_dlaa/`, `temporal_bindings/`, `scaled_temporal/`,
    `render_resolution/`: DLSS on the game's TAA pass and the render
    resolution;
  - `reflex/` (Streamline, Reflex, presentation and the plugin's entry point)
    and `frame_generation/`;
  - `alpha/`: the ini, the CMFM pages and the status lines.
- **`tests/`**: unit tests, WARP fixtures and the Python tools' tests.
- **`tools/`**: what reads players' logs and builds releases. That's
  `fg_game.py` (run status and summaries), `profile_report.py`, `perf/`
  (PresentMon capture and summary), `package_release.py` and `rdc/`
  (RenderDoc scripts).

  The research probes, fixtures and tools the mod grew out of were retired on
  2026-10-17. The tag `research-final` still builds every one of them; see
  [`docs/research/history/retired-targets.md`](docs/research/history/retired-targets.md).
- **`docs/research/`**: dated notes on what the game does and what each
  version changed. Confirmed results are kept apart from hypotheses.
  - [`history/readme-changelog.md`](docs/research/history/readme-changelog.md)
    is the former README: the research log from the first probe to in-game
    DLAA.
- **`docs/runtime/`**: the plan for each runtime probe (research history).
- **`third_party/`**: small vendored files with their licenses (see
  [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)).
- **`local/`** (ignored): private game databases, dumps, captures and SDKs.
  Never committed.

Good places to start reading:
- [DLSS render resolution control](docs/research/dlss-render-resolution-control.md)
- [frame generation's HUD-less input](docs/research/frame-generation-hudless.md)
- [the game's AA option under DLSS](docs/research/game-aa-option.md)
- [the game's DLSS 1](docs/research/game-dlss1.md)
- [frame generation on and off while playing](docs/research/fg-live-toggle.md)
- [what each card offers](docs/research/card-capabilities.md)
- [the menu](docs/research/menu-rework.md)
- [performance](docs/research/performance.md)

## Working on it

[`AGENTS.md`](AGENTS.md) holds the project's rules:
- the game directory is read-only and no Capcom or NVIDIA binaries are
  committed;
- the game is reverse engineered, NVIDIA's DLLs never;
- hooks are gated by hashes of the tested executable;
- features fail closed;
- experiments run offline.

## Credits and licenses

This mod uses NVIDIA DLSS, NVIDIA DLSS Frame Generation, NVIDIA Streamline and
NVIDIA Reflex. It is not affiliated with, sponsored or endorsed by NVIDIA.
Release packages include NVIDIA's unmodified runtime DLLs under NVIDIA's
licenses, which ship with them.

MinHook (tests only), DirectXShaderCompiler's DxilHash, NVAPI declarations and
the Astera Loader and CMFM headers are used under their licenses
([`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)).

Monster Hunter: World is a trademark of CAPCOM CO., LTD. This mod is not
affiliated with Capcom.

The mod itself is released under the MIT License ([`LICENSE`](LICENSE)).
