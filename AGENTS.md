# Monster Hunter: World Temporal Upscaling Research

## Objective

This repository is for an open-source investigation and implementation of modern temporal anti-aliasing and upscaling in the Windows PC version of Monster Hunter: World. The initial renderer target is DirectX 12. Work should proceed through small, verifiable diagnostics before integrating any vendor SDK.

## Canonical Local Paths

- Repository: `D:\mh world modding\dlssupdate`
- Game directory: `D:\SteamLibrary\steamapps\common\Monster Hunter World`
- Game executable: `D:\SteamLibrary\steamapps\common\Monster Hunter World\MonsterHunterWorld.exe`
- IDA directory: `C:\Program Files\IDA Professional 9.3`
- IDA executable: `C:\Program Files\IDA Professional 9.3\ida.exe`

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
