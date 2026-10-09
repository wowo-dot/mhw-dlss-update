# Monster Hunter: World Temporal Upscaling Research

## Objective

This repository is for an open-source investigation and implementation of modern temporal anti-aliasing and upscaling in the Windows PC version of Monster Hunter: World. The initial renderer target is DirectX 12. Work should proceed through small, verifiable diagnostics before integrating any vendor SDK.

## Canonical Local Paths

- Repository: `D:\mh world modding\dlssupdate`
- Game directory: `D:\SteamLibrary\steamapps\common\Monster Hunter World`
- Game executable: `D:\SteamLibrary\steamapps\common\Monster Hunter World\MonsterHunterWorld.exe`
- IDA directory: `C:\Program Files\IDA Professional 9.4` (the Hex-Rays IDA MCP needs 9.4; 9.3 stays installed)
- IDA executable: `C:\Program Files\IDA Professional 9.4\ida.exe`
- Private game databases: `local\ida-native-dlss-20261008\decrypted.i64` (input: the decoded image below; use it for code). `local\ida_static\MonsterHunterWorld_C2EBBBD2.i64` was built from the exe on disk, so its `.text` is SteamStub ciphertext (no functions at the live-frame RVAs, checked 2026-10-10); use it only for data sections and strings.
- Decoded game image (read in place): `D:\mh world modding\ai\glavenus\MonsterHunterWorld_decrypted.exe`, SHA-256 `d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`, decoded from the tested exe `c2ebbbd2…90ea`. The exe on disk has SteamStub-encrypted code, so offline gate and signature checks run on this image.
- Claude Code reversing tools: `C:\Users\akifs\.claude\re-tools\` (Python 3.12: `py -3.12 -B`); x64dbg: `D:\reverse engine jank\x64dbg\release\x96dbg.exe`

These paths are machine-local configuration. Do not encode them into distributable binaries or assume other contributors use the same paths.

## Safety and Scope

- Treat the game directory as read-only unless the user explicitly requests an installation or runtime test.
- Never overwrite or patch `MonsterHunterWorld.exe` or original game assets.
- Put builds, logs, captures, extracted shaders, signatures, and analysis artifacts under this repository, not in the game directory.
- If a runtime test requires deploying a DLL, preserve any existing file at the exact destination and document how to remove the test files.
- Do not run experimental hooks in public multiplayer. Prefer offline mode for runtime research.
- Do not disable integrity checks, anti-cheat, licensing, or platform protections.
- Do not commit or redistribute Capcom binaries, game assets, private SDK contents, crash dumps containing user data, or proprietary IDA databases.
- Never commit NVIDIA binaries to git. Release packages may include the unmodified NVIDIA runtime DLLs (DLSS, Streamline, Reflex, DLSS Frame Generation) the build was tested with, under the NVIDIA RTX SDKs license and the Streamline/Reflex licenses: ship NVIDIA's license files and an attribution notice ("uses NVIDIA DLSS, Streamline and Reflex") with them, keep them outside the project's open-source license, never imply NVIDIA endorsement or use NVIDIA logos, and notify NVIDIA before any commercial release.
- Do not reverse engineer, decompile or disassemble NVIDIA binaries; analyse the game, not NVIDIA's DLLs.

## Reverse-Engineering Workflow

1. Record the executable version and SHA-256 before deriving signatures or offsets.
2. Prefer signatures and structural discovery over hard-coded absolute addresses.
3. Keep confirmed observations separate from hypotheses in research notes.
4. For every hook, document the target function, calling convention, ownership/lifetime assumptions, and tested executable hash.
5. Validate resource formats, dimensions, states, and frame alignment before treating a render target as color, depth, motion vectors, exposure, or UI.
6. Add modern temporal reconstruction incrementally: diagnostic loading, frame/resource observation, jitter and motion validation, DLAA, then resolution-changing upscaling. Frame generation is a later milestone.

## Reverse-Engineering Tools

Claude Code has an IDA MCP (Hex-Rays), an x64dbg MCP, a Frida MCP and `sigscan.py`; the user-level `reversing` skill describes them. This file's rules come first.

- **IDA:** open only the private game databases listed above (or a new copy under `local\`), never the NVIDIA module databases under `local\` (`nvngx_*`, `_nvngx`) and never anything in `ai\glavenus`. Ask through the MCP's `execute_python` instead of writing one-off headless scripts. Write-back and exports stay under `local\`.
- **Code gates and heads** (`py -3.12 -B C:\Users\akifs\.claude\re-tools\sigscan.py`, on the decoded image):
  - `gates <image> src` hashes every `{rva, size, "sha256"}` gate in the source against the image; on 2026-10-10 all 182 matched. Run it before staging a build that adds or changes a gate.
  - `audit <image> src` checks the spaced-hex heads in `reflex/live_frames.cpp`. They are verified at pinned RVAs, so a head may match elsewhere too (`Render:Publish` matches 20+ times; fine).
  - `make <image> <VA>` and `scan <image> "<pattern>"` find and check patterns when exploring new sites.
- **x64dbg and Frida on the running game:**
  - the user launches the game through MO2 and says OK in the session before each attach; never attach during startup (SteamStub);
  - offline only, as for any experimental hook;
  - prefer x64dbg hardware breakpoints and non-stopping logging breakpoints: they change no code bytes, and the game re-checks its code with CRC;
  - Frida hooks only functions this mod or Astera Loader already hooks. Software breakpoints, memory writes, other hooks and calls into game code each need their own OK;
  - detach when done (x64dbg `detach`, never `terminate_session`) and note what was attached, when and why in the run folder.
- Point the tools at the game's code only. NVIDIA binaries stay off limits (Safety and Scope).

## Implementation Conventions

- Use C++20 and CMake for native Windows components unless a task explicitly requires another toolchain.
- Target 64-bit Windows and keep DirectX 11 and DirectX 12 code paths isolated.
- Prefer RAII, smart COM pointers, bounded logging, and clean hook removal.
- Keep third-party dependencies explicit and pinned. Record their licenses.
- Never log access tokens, Steam identifiers, personally identifying paths beyond the documented local configuration, or raw process memory indiscriminately.
- Feature code must fail closed: if a required resource or invariant is missing, disable the feature and leave the original rendering path intact.

## Verification

- Build with warnings enabled and treat new warnings in project code as defects.
- Unit-test pure logic such as signature parsing, configuration, format classification, and jitter generation outside the game process.
- Runtime tests must produce enough versioned logging to identify the executable hash, graphics API, adapter, swap-chain format, and enabled experiment.
- A visual result is not sufficient by itself; check stability during camera cuts, menus, cutscenes, resolution changes, alt-tab, and device/swap-chain recreation.

## Git Hygiene

- Keep generated build trees, logs, captures, dumps, game files, SDK binaries, and IDA database files untracked.
- Make focused commits that distinguish scaffolding, research notes, signatures, hooks, and feature integrations.
- Do not rewrite or discard user changes without explicit approval.
