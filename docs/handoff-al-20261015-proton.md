# To Astera Loader: the launcher can't find its DLL under Proton (2026-10-15)

For the Astera Loader session (`D:\mh world modding\AsteraLoader`). The
Astera Loader repository is read-only from here; nothing in it has been
changed. A Linux user started Astera Loader 0.2.2 (commit 1e0ab01,
`AsteraLoader.dll` `f8a6ebff…7ef3`) through Steam under Proton, with the
tested exe (`c2ebbbd2…90ea`). The game ran, but without Astera Loader, and
the launcher showed "Astera Loader did not confirm startup".

Evidence: the launcher log, saved as
`D:\mh world modding\dlssupdate\local\beta-reports\proton-1\launcher.log`.
There is no `core.*.log`: the core opens its log in `al_bootstrap`, which
never ran.

## Launch setup

Steam on Linux runs launch options as a Linux command, so
`"<game folder>\AsteraLoader.exe" %command%` cannot work there. The
suggested line swaps the exe inside `%command%`, so that Proton runs the
launcher in the game's prefix:

```
PROTON_ENABLE_NVAPI=1 bash -c 'exec "${@/MonsterHunterWorld.exe/AsteraLoader.exe}"' -- %command%
```

The swap leaves no game path argument, which fits the log's
`game_path_source=launcher folder`. We don't know for certain which line
the user used.

## What the log shows (confirmed)

- The launcher found the game, inherited `SteamAppId=582010`, created the
  game suspended and ran both remote threads (`RtlGetVersion`, then
  `LoadLibraryW`).
- `LoadLibraryW_thread_exit_low32=4237819904` (`0xFC980000`) is non-zero:
  **the DLL loaded** into the game.
- No `bootstrap_status=` line follows, so `remote_image(child, core_native)`
  returned 0 (`src/launcher/main.cpp` ~406). The launcher never called
  `al_bootstrap` and resumed the game without Astera Loader.

## Cause (from Wine's source; the actual names are not in the log)

The lookup compares names from two different APIs:

- **Game side:** `GetMappedFileNameW`. Wine's server answers it from the
  NT name the file was opened with (`get_mapping_filename` →
  `get_view_nt_name` → `get_nt_name(fd)`, stored as the caller passed it).
  Here that would be `\??\S:\steamapps\...\AsteraLoader.dll`.
- **Launcher side:** `GetFinalPathNameByHandleW(VOLUME_NAME_NT)`. Wine
  builds it from `QueryDosDeviceW` of the drive plus the rest of the path,
  e.g. `\Device\HarddiskVolumeN\steamapps\...\AsteraLoader.dll`.

On Windows both return the `\Device\HarddiskVolumeN\…` form, so they
match. On Wine they cannot. The `ntdll` and `kernel32` lookups just before
it worked because `remote_system_function` uses `GetMappedFileNameW` on
both sides. This is the rule in AL's AGENTS.md (never compare paths from
two sources), in a case that only shows up on Wine.

## Suggested fix

- Take the expected name from the same API: `CoreImage` already maps
  `core_file` as an image (`SEC_IMAGE_NO_EXECUTE`), so
  `mapped_name(GetCurrentProcess(), view)` gives the name in exactly the
  form the game side reports, on Windows and on Wine.
  - On Wine the names should agree: `core_real` comes from that same
    handle, so the child's `LoadLibraryW(core_real)` opens the file under
    the same drive letter.
  - On Windows nothing changes: the launcher's view of the real file reports
    the same `\Device\…` name as today, under MO2 too.
- Log the expected name (`core_expected=`), and, when no match is found,
  the mapped names of the `MEM_IMAGE` allocations whose base's low 32 bits
  equal the `LoadLibraryW` result. The next mismatch can then be read from
  the launcher log alone.
- Optional fallback, if the name still fails somewhere: accept the single
  allocation whose low 32 bits match and whose PE headers (`TimeDateStamp`,
  `SizeOfImage`, `CheckSum`) equal the local image.

## Testing

- Windows: AL's launcher tests, then the four launch setups in AL's
  AGENTS.md (MO2, MO2 with SPL, direct start, Steam launch options). The
  log should show `bootstrap_status=0` and `core_confirmed_before_resume` as
  before.
- Linux: only the user can test. Give them a test build and the launch line
  above.

## Untested on Proton beyond this point

- Astera Loader's hardware breakpoint observer (`SetThreadContext` with
  debug registers, `src/core/phases/runtime.cpp` ~264). Wine supports debug
  registers on Linux, but AL has never run there; per AL's rules a failure
  must leave the game running.
- `al.d3d12` hooks on vkd3d-proton (D3D12) and DXVK's DXGI.
- On the DLSS mod's side (ours, for later): `PROTON_ENABLE_NVAPI=1` is
  required for DLSS. The Streamline signature check with whole-chain
  revocation may fail under Wine; Reflex and frame generation would then
  switch off. DXVK doesn't implement `CreateSwapChainForComposition`, so the
  live frame-generation toggle would fall back to needing a restart.
