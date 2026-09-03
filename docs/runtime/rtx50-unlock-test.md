# RTX 5090 NGX resolver-boundary trace

Version 0.4.0 is a bounded, observation-only diagnostic for Monster Hunter:
World's legacy NGX startup chain. It does not expose the DLSS menu row, change
an NGX result, replace a signed NVIDIA binary, or install DLSS 2/3/4.

## Required state

- `MonsterHunterWorld.exe` has SHA-256
  `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
- The game-local `nvngx_dlss.dll` is the original signed 1.1.13.0 file with
  SHA-256
  `D4CC11BCEC693DFEFD6EDDBA6DD29BDFC16B4043E7B83150F0097D14B76512D8`.
- The NVIDIA NGX cache contains the signed 1.2.110.0 MHW feature at
  `dlss/versions/66158/files/160_B9D2F5C.bin`, SHA-256
  `DB203881EA5028CB584AA6C58D457361DD0E6A2817679878098B08B940549A03`.
- The physical `dlssTEST` mod root contains `mhw_dlss_unlock.dll`; no active
  copy remains under `nativePC/plugins`. The staged v0.4.0 diagnostic is
  608,768 bytes with SHA-256
  `CAC50470A968ABA8129E8A01A7E4563C9F8E6521A298925506B75F89C7E5DF89`.
- MO2 Force Load is enabled for the MHW executable with only that root DLL's
  entry enabled. Keep stale LuaEngine and unrelated library entries disabled.
- DirectX 12 is enabled, screen mode is fullscreen, and FidelityFX CAS +
  Upscaling is off. These settings preserve the known menu eligibility state;
  the observer itself does not change them.

Stracker's plugin pass is not used. Earlier timing evidence showed it starts
after the NGX selection path, while this diagnostic must see the first export
lookup.

## Test sequence

1. Use the title/options flow or a private offline/solo session. Do not take an
   experimental hook into public multiplayer.
2. Launch MHW through the dedicated MO2 executable entry with the single Force
   Load rule enabled.
3. Open Display settings and leave the game running for at least 30 seconds.
   The missing DLSS row is expected: v0.4.0 records behavior only.
4. Exit normally.
5. Inspect `%LOCALAPPDATA%\MhwDlssUnlock\unlock-<pid>.log`.

A valid startup begins with lines equivalent to:

```text
trace.start version=0.4.0 mode=ngx-resolver-boundary-observer expected-loader=mo2-force-load mutations=0
trace.host-boundary mapped=true
trace.system-boundary verified=true
trace.resolver-ready target=KernelBase!GetProcAddressForCaller pre-enable-ngx-present=false lifetime=process mutations=0
trace.host-identity verified=true
trace.active host-verified=true gpu-allowlist=RTX_5090 hook-lifetime=process mutations=0
```

The tracer recognizes six exact slots:

| Slot | Requested by | What it distinguishes |
| --- | --- | --- |
| `legacy-core-capability` | MHW executable | Legacy capability-object acquisition |
| `legacy-core-parameters` | MHW executable | Legacy parameter-object acquisition |
| `modern-core-capability` | Signed legacy `nvngx.dll` | Handoff to the current driver core |
| `modern-core-parameters` | Signed legacy `nvngx.dll` | Handoff to the current driver core |
| `local-populate` | Signed modern `_nvngx.dll` | Use of game-local DLSS 1.1.13 |
| `cache-populate` | Signed modern `_nvngx.dll` | Use of cached MHW DLSS 1.2.110 |

For each matched lookup, `trace.identity` must be `verified`. `trace.call`
records the unmodified result and calling module; `trace.summary` separates
resolution and invocation counts. Particularly useful outcomes are:

- `local-populate ... result=0xBAD00000`: the original 1.1.13 capability
  population rejects this runtime path;
- `cache-populate ... result=0xBAD00000`: the cached 1.2.110 feature rejects it;
- a nonzero `resolves` count with zero `calls`: the function pointer was
  acquired but not invoked during the window;
- zero `resolves`: the runtime used a different API path or the startup
  boundary was missed.

The final records should include:

```text
trace.window closed=true bindings-enabled=false quiesced=true hook-retained=true
trace.complete ... target-identity-or-caller-rejects=0 resolver-calls-before-ready=0 quiesced=true ... mutations=0
```

Treat any `trace.abort`, `trace.failure`, rejected identity, nonzero conflict,
or `quiesced=false` as failed or inconclusive. Dropped events do not alter
behavior, but they mean only the fixed first-event sample is available; totals
remain in the summary. `startup-worker-gap-observable=false` records the small
unavoidable interval between forced-DLL attachment and the worker installing
the hook, so a zero-call conclusion remains bounded evidence rather than proof
about every instruction executed since process creation.

## Rollback

1. Exit MHW completely.
2. Disable the observer's MO2 Force Load rule.
3. Disable or remove the distinct `dlssTEST` mod if desired.

Disabling the mod alone is insufficient while an absolute Force Load rule is
still enabled. The observer does not modify the game directory, executable,
NVIDIA cache, or signed feature DLLs.
