# MO2 and Stracker's Loader deployment

Status: staging into the dedicated `dlssTEST` MO2 mod is authorized. Keep the
mod disabled after staging; no in-game run has been performed.

## Layout

Stracker's Loader discovers native plugins under `nativePC/plugins`. Configure
and build the probe, then use CMake's install step with the MO2 mod directory as
the prefix:

```powershell
cmake -S . -B build -A x64
cmake --build build --config RelWithDebInfo
ctest --test-dir build -C RelWithDebInfo --output-on-failure
cmake --install build --config RelWithDebInfo `
  --prefix '<MO2-instance>/mods/dlssTEST' `
  --component probe
```

The only installed payload is:

```text
<MO2-instance>/mods/dlssTEST/
└── nativePC/
    └── plugins/
        └── mhw_dlss_probe.dll
```

Before installing, preserve any pre-existing file at that exact destination.
The probe must remain a distinct MO2 mod. Do not copy it over
`nvngx_dlss.dll`, `dinput8.dll`, `loader.dll`, the game executable, or another
plugin. The install must not write to the physical game directory.

Staging ends after verifying that the expected DLL is present in the disabled
`dlssTEST` mod. Do not enable the mod or launch the game as part of staging.

The probe is process-lifetime-only. This deployment assumes Stracker's Loader
retains each loaded plugin module until process exit; do not hot-unload or
hot-reload it. Disabling the MO2 mod takes effect on the next launch, after MHW
has exited completely.

## Staged test protocol

Use offline play for every diagnostic run.

1. Confirm the staged probe mod is still disabled.
2. Confirm Stracker's Loader is enabled and its `enablePluginLoader` setting is
   `true`.
3. Disable unrelated MO2 forced-library injections for a clean run, or record
   every remaining injection and its enabled state as an explicit confounder.
   A forced-library rule can remain active even when its source mod is disabled.
4. Record the selected experiment as `legacy-lifecycle`.
5. Enable the probe mod and start MHW in DirectX 12 mode with the game's shipped
   DLSS enabled.
6. The default first run observes only lifecycle calls. It does not invoke the
   protected host parameter getters.
7. Exit the game and inspect `%LOCALAPPDATA%\MhwDlssProbe\probe-<pid>.log`.
   Treat the run as incomplete unless the log identifies the executable
   SHA-256, graphics API, adapter, swap-chain format and resolution, and enabled
   experiment. It must also report the expected feature-DLL hash and metadata,
   untouched hook prologues, and `api=d3d12` while the game behaves normally.
   A `complete=false` identity is a failed/inconclusive run, including when
   multiple same-process swap chains make the presentation target ambiguous.
8. A later, separate run may set `MHW_DLSS_PROBE_QUERY_PARAMETERS=1`. Record the
   selected experiment as `legacy-parameter-capture`. It captures
   at most 16 evaluations by default and permanently disables further queries
   after the first getter or resource-description exception.

Never set `MHW_DLSS_PROBE_TEST_OVERRIDE` for a game run. It exists solely for
the mock executable in the automated test suite.

## Removal and recovery

1. Exit MHW completely.
2. Disable the probe mod in MO2.
3. Remove the `dlssTEST` mod through MO2 if it is no longer needed.

Because deployment is virtual, disabling the distinct mod removes the probe
from the game's view. No original game or Stracker's Loader file is overwritten.
If the lifecycle-only run does not produce a log, leave parameter querying off
and diagnose loader timing before trying again.
