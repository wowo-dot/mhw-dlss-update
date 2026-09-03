# RTX 5090 NGX D3D12-status trace

Version 0.4.1 is a bounded, observation-only diagnostic for Monster Hunter:
World's legacy NGX startup chain. It does not expose the DLSS menu row, change
an NGX result, proxy or replay feature Init, replace a signed NVIDIA binary, or
install DLSS 2/3/4.

The v0.4.0 run proved that the signed cached MHW feature 1.2.110 is selected,
that its generic `PopulateParameters` export is resolved but never invoked, and
that both legacy and modern GetParameters calls return success. Version 0.4.1
keeps those resolver observations and adds the exact downstream boundary that
records the final per-feature D3D12 status.

## Required state

- `MonsterHunterWorld.exe` has SHA-256
  `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
- The game-local `nvngx_dlss.dll` is the original signed 1.1.13.0 file with
  SHA-256
  `D4CC11BCEC693DFEFD6EDDBA6DD29BDFC16B4043E7B83150F0097D14B76512D8`.
- The NVIDIA NGX cache contains the signed 1.2.110.0 MHW feature at
  `dlss/versions/66158/files/160_B9D2F5C.bin`, SHA-256
  `DB203881EA5028CB584AA6C58D457361DD0E6A2817679878098B08B940549A03`.
- The modern NVIDIA core is version 32.0.16.1047 with SHA-256
  `B3C82638C60F29AEFF68052FFE7147246A73C80CBEA5883B35A9B0D1ED57DDE7`,
  PE timestamp `0x6A0CB15E`, and `SizeOfImage` `0x173000`.
- The physical `dlssTEST` mod root contains `mhw_dlss_unlock.dll`; no active
  copy remains under `nativePC/plugins`. The staged v0.4.1 diagnostic is
  648,192 bytes with SHA-256
  `9255432A0A5176ED07C6F9669AC695EA5DA59EAA3108B2B92B22997EB15D006E`.
- MO2 Force Load is enabled for the MHW executable with only that root DLL's
  entry enabled. Keep stale LuaEngine and unrelated library entries disabled.
- DirectX 12 is enabled, screen mode is fullscreen, and FidelityFX CAS +
  Upscaling is off. These settings preserve the known menu-eligibility state;
  the observer itself does not change them.

Stracker's plugin pass is not used. Earlier timing evidence showed it starts
after the NGX selection path, while this diagnostic must see the first export
lookup.

## What v0.4.1 observes

The resolver tracer retains the six exact v0.4.0 slots:

| Slot | Requested by | What it distinguishes |
| --- | --- | --- |
| `legacy-core-capability` | MHW executable | Legacy capability-object acquisition |
| `legacy-core-parameters` | MHW executable | Legacy parameter-object acquisition |
| `modern-core-capability` | Signed legacy `nvngx.dll` | Handoff to the current driver core |
| `modern-core-parameters` | Signed legacy `nvngx.dll` | Handoff to the current driver core |
| `local-populate` | Signed modern `_nvngx.dll` | Use of game-local DLSS 1.1.13 |
| `cache-populate` | Signed modern `_nvngx.dll` | Use of cached MHW DLSS 1.2.110 |

It also records exact-name `NVSDK_NGX_D3D12_Init` resolver events. Every such
event is passthrough: the pointer returned by NVIDIA is returned to its caller
unchanged. This matters because both installed feature builds authenticate the
module containing their immediate return address and return `0xBAD00002` if it
is not an `nvngx.dll`; an ordinary C++ Init proxy would manufacture that
failure.

After the modern core's full mapped and on-disk identity is verified and the
module is pinned, the worker hooks only its internal recorder at RVA `0x264E0`.
Telemetry requires all three exact gates:

- caller/return RVA `_nvngx.dll+0x67704` from the D3D12 loop call at
  `_nvngx.dll+0x676FF`;
- feature index `1`, the DLSS descriptor;
- API `2`, D3D12.

The recorder's seven arguments are forwarded exactly once. Pointer arguments
are never dereferenced or logged; only context nullness, the raw status, tick,
thread, and caller are captured. Other recorder callers remain transparent and
are counted only as gate rejects.

## Test sequence

1. Use the title/options flow or a private offline/solo session. Do not take an
   experimental hook into public multiplayer.
2. Launch MHW through the dedicated MO2 executable entry with the single Force
   Load rule enabled.
3. Open Display settings and leave the game running for at least 30 seconds.
   The missing DLSS row is expected: v0.4.1 records behavior only.
4. Exit normally.
5. Inspect `%LOCALAPPDATA%\MhwDlssUnlock\unlock-<pid>.log`.

A valid startup begins with lines equivalent to:

```text
trace.start version=0.4.1 mode=ngx-d3d12-status-boundary-observer expected-loader=mo2-force-load slot-wake-event=true mutations=0
trace.host-boundary mapped=true
trace.system-boundary verified=true
trace.resolver-ready target=KernelBase!GetProcAddressForCaller pre-enable-ngx-present=false lifetime=process mutations=0
trace.host-identity verified=true
trace.active host-verified=true gpu-allowlist=RTX_5090 hook-lifetime=process mutations=0
```

The modern core and status slot must then verify, followed by an armed boundary:

```text
trace.identity slot=modern-core-capability state=verified ...
trace.identity slot=modern-d3d12-feature-status state=verified ...
trace.boundary-ready slot=modern-d3d12-feature-status target=_nvngx.dll+0x264E0 expected-caller=_nvngx.dll+0x67704 feature-index=1 api=2 ... init-proxied=false init-replayed=false mutations=0
```

`trace.resolver-event` reports `passthrough`, `rejected`, or `null-result` plus
the requested name, module filename, mapped PE metadata, target RVA, caller, and
tick. A feature-DLL base Init lookup from `_nvngx.dll+0x4320C` increments
`feature-d3d12-init-lookups`; its returned pointer is never substituted.

The decisive record has this shape:

```text
trace.call ... slot=modern-d3d12-feature-status ... feature-index=1 api=2 context-nonnull=true status=0x... status-name=... failed=... mutated=false
```

Key status labels are:

- `0x1`: `success`;
- `0xBAD00000`: `fail`;
- `0xBAD00001`: `feature-not-supported`;
- `0xBAD00002`: `platform-error`.

The pinned NGX result range through `0xBAD00012` is also named in the log; any
unknown value is retained verbatim as `other`. A fail-class status is detected
with the NGX `0xBADxxxxx` mask.

The final summary should contain `feature-status-hook=enabled`,
`resolver-calls-before-ready=0`, `quiesced=true`, and one of these coverage
values:

- `target-status-observed`: the exact DLSS/D3D12 status was captured;
- `armed-before-feature-init-lookup`: the hook was ready before the first
  observed feature-DLL base Init lookup, but no target status was recorded;
- `armed-no-feature-init-lookup`: the boundary was ready, but no feature-DLL
  base Init lookup occurred;
- `armed-after-first-init-lookup-call-coverage-inconclusive`: the boundary may
  still have caught the later call, but timing alone cannot prove it;
- `inconclusive`: identity, hook installation, or timing was insufficient.

Treat any `trace.abort`, `trace.failure`, `trace.boundary-unavailable`, rejected
identity, nonzero conflict, dropped resolver event, or `quiesced=false` as
failed or inconclusive. The recorder hook and resolver hook remain installed as
pass-through boundaries after the 30-second window; telemetry and new binding
stop. `startup-worker-gap-observable=false` still records the unavoidable gap
between forced-DLL attachment and resolver-hook installation.

## Rollback

1. Exit MHW completely.
2. Disable the observer's MO2 Force Load rule.
3. Disable or remove the distinct `dlssTEST` mod if desired.

Disabling the mod alone is insufficient while an absolute Force Load rule is
still enabled. The observer does not modify the game directory, executable,
NVIDIA cache, or signed feature DLLs.
