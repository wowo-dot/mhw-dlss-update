# Legacy DLSS RTX 50-series classifier failure

## Confirmed cause

Monster Hunter: World's signed `nvngx_dlss.dll` 1.1.13.0 uses adapter-name
matching for two legacy GPU-type decisions. The implementation recognizes the
GeForce RTX 2060, 2070, and 2080 families, TITAN RTX, and the contemporary RTX
4000/5000/6000/8000 workstation names. An adapter named `NVIDIA GeForce RTX
5090` or `NVIDIA GeForce RTX 5080` reaches the unknown value `0`.

This is confirmed in the original NVIDIA-signed feature DLL with SHA-256
`D4CC11BCEC693DFEFD6EDDBA6DD29BDFC16B4043E7B83150F0097D14B76512D8`.
It is not inferred from the protected game executable.

Two paths must be handled:

| Purpose | Feature-DLL RVA | Confirmed consequence of `0` |
| --- | ---: | --- |
| Capability population classifier | `0x19E00` | `NVSDK_NGX_PopulateParameters` returns `0xBAD00000`, so DLSS is not published as available |
| Shared description parser | `0x19640` | Its caller at `0xA6C0` caches `0`; `DLSSOptimalSettingsCallback` then returns `0xBAD00002` (`PlatformError`) |

The feature's own generic `Graphics Device` fallback maps to GPU type `7`, its
RTX 2080 profile. CreateFeature and EvaluateFeature do not reference either
classifier or the cached GPU type directly. A later feature-creation failure is
still possible if the old CUDA/cubin path is incompatible with Blackwell; that
can only be decided by a controlled runtime test.

## NVIDIA NGX cache discovery

The installed NVIDIA NGX manifest maps MHW application ID `0x0B9D2F5C` to a
second, signed DLSS feature image:

- cache file: `dlss/versions/66158/files/160_B9D2F5C.bin`;
- file version: `1.2.110.0`;
- SHA-256:
  `DB203881EA5028CB584AA6C58D457361DD0E6A2817679878098B08B940549A03`;
- preferred image base / `SizeOfImage`: `0x180000000` / `0xB57000`;
- exported application, architecture, and snippet values: `0x0B9D2F5C`,
  `0x160`, and `0x1026E` (1.2.110).

Its signed version metadata and PDB path identify it as the MHW feature build.
Its classifier at RVA `0x31750` still uses case-sensitive legacy name matching:
RTX 2080 variants map to types 7 through 10, while RTX 40- and 50-series model
strings are absent and an unrecognized description maps to zero. Its
`NVSDK_NGX_PopulateParameters` export can alternatively obtain a scaling bucket
from the driver and map it at RVA `0x316D0`, so observing only the description
parser is not sufficient to identify the active decision path.

Two September 3, 2026 runtime diagnostics established the active selection:

- the cache 1.2.110 image was mapped before Stracker's plugin pass;
- the bridge alone subsequently loaded the game-local 1.1.13 image;
- neither 1.1.13 classifier was called during either 30-second Display-menu
  observation;
- deleting the cache while online caused NGX OTA to restore the exact signed
  1.2.110 file because `nvngx_config.txt` maps `app_B9D2F5C` to `1.2.110`.

Consequently, a `nativePC/plugins` bridge targets the wrong module and starts
too late. Stracker cannot be used for this gate.

## v0.3.3 cache-decision experiment

Version 0.3.3 of `mhw_dlss_unlock` targeted only the signed cache image above.
It was loaded through MO2's per-executable **Force load libraries** path,
preloaded and verified the expected file, and installed two process-lifetime
MinHook detours:

| Decision | RVA | Override boundary |
| --- | ---: | --- |
| Description classifier | `0x31750` | Exact `NVIDIA GeForce RTX 5090`, original result zero, and return RVA `0x0F8E8` or `0x32457` |
| INF scaling-bucket mapper | `0x316D0` | Strict single-hardware-adapter RTX 5090 inventory, original result zero, and return RVA `0x1E009` (capability population) or `0x22F0D` (optimal settings) |

Each detour called the original first and preserved every nonzero result. The
provisional zero mapping was GPU type `7`, which this exact feature already
uses for its RTX 2080 and generic-device fallback. The two hooks were applied
as one transaction. Calls arriving while the slower host-hash and DXGI checks
finished waited for a bounded verification result; failure or timeout returned
the original value.

The bridge refused to override unless all of these checks passed:

- game executable SHA-256
  `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`;
- cache path resolved below the system ProgramData known folder;
- complete cache SHA-256, raw-file PE timestamp, preferred base, image size,
  file version, application ID, architecture, snippet version, and API version;
- mapped PE identity whose relocated image base equals the actual ASLR module
  address, while its timestamp, machine, magic, and image size remain exact;
- both full entry prologues and the 39-byte `PopulateParameters` decision
  sequence;
- all four relative calls resolve to the recorded classifier/mapper entries;
- exactly one non-software hardware adapter exists and it is precisely an
  `NVIDIA GeForce RTX 5090`.

The September 3 acceptance log
`%LOCALAPPDATA%\MhwDlssUnlock\unlock-47052.log` (SHA-256
`BC32D1CC010375EBBB5D69D204A0D7F5E7CA86656442D91D6BC1AAA57BB55721`)
reached `unlock.active` 141 ms after its diagnostic began. Cache 1.2.110 was
observed at +157 ms, `nvngx.dll` at +2438 ms, `_nvngx.dll` at +2453 ms, and the
game-local 1.1.13 feature at +2703 ms. After 30 seconds both classifier counters
were exactly zero, with no timeout, abort, or failure. The DLSS menu row remained
absent.

This is a valid negative result: v0.3.3 made no effective override. It proves
that neither chosen internal cache decision point executed, but it does not
prove that `NVSDK_NGX_PopulateParameters` itself was never entered through a
different or earlier-return path. The bridge is therefore retired rather than
expanded with another guessed patch.

## v0.4 resolver/export boundary

Version 0.4.0 replaces the override with an observation-only resolver tracer.
Static analysis identified the unprotected boundary around the otherwise
protected MHW call site:

- MHW dynamically requests legacy `NVSDK_NGX_D3D12_GetParameters` from
  `nvngx.dll`;
- legacy `nvngx.dll` requests both modern GetParameters variants from
  `_nvngx.dll` at exact caller RVAs `0x9A12` and `0x9A2A`;
- modern `_nvngx.dll` requests `NVSDK_NGX_PopulateParameters` at exact caller
  RVAs `0x43333` and `0x43351`;
- the two permitted feature exports are the game-local 1.1.13 Populate at RVA
  `0x180E0` and cache 1.2.110 Populate at RVA `0x1DFA0`.

The tracer hooks only the exact local build of
`KernelBase!GetProcAddressForCaller`. The KernelBase and kernel32 hashes, PE
metadata, resolver/stub RVAs, resolver prologue, and kernel32 tail-jump target
must all match. Each returned wrapper is selected only after exact mapped PE,
export RVA, prologue/stub, and caller checks. A background worker then verifies
the full module path, on-disk SHA-256, PE metadata, and file version and pins the
module. A rejected identity becomes forwarding-only.

The six wrappers use the real NGX ABIs, call the original exactly once, preserve
`LastError`, never dereference or change an NGX parameter, and return the
original result. They are compiled and linked with Control Flow Guard; the
final binary's Guard CF function table contains all six exported wrapper
addresses. Recording closes after 30 seconds, waits for in-flight resolver and
wrapper calls, drains fixed telemetry, and leaves the retained hook and issued
wrappers pass-through.

The v0.4 runtime question was precise: which legacy/modern/feature exports are
resolved and called, in what order, and what untouched result does the selected
Populate return? The procedure and acceptance gates are in
[`../runtime/rtx50-unlock-test.md`](../runtime/rtx50-unlock-test.md).

## v0.4 runtime result

The accepted v0.4.0 run produced
`%LOCALAPPDATA%\MhwDlssUnlock\unlock-30940.log` (6,854 bytes, SHA-256
`DBEAAAE8EF0421B01C50545F8AED9E5E386DB94530297733F910B74B711526CB`).
It recorded:

- exact identities for legacy `nvngx.dll` 30.0.14.9516 and modern
  `_nvngx.dll` 32.0.16.1047;
- selection and mapping of both the game-local 1.1.13 image and cached 1.2.110
  image;
- one verified `cache-populate` resolution from `_nvngx.dll+0x43351`, but zero
  Populate calls;
- successful legacy and modern GetParameters calls; and
- one resolver target/caller rejection that v0.4.0 counted but could not
  identify.

The zero Populate count is not a menu-layer explanation. In the exact modern
core, the feature loader resolves cached base Init before its generic Populate
fallback. Observing the cached Populate resolution proves discovery returned a
non-fail status. The cached image exports base D3D12 Init but none of the Ext
variants, so the D3D12 loop calls base Init at `_nvngx.dll+0x67694`, stores EAX
at `+0x6769A`, and later suppresses generic Populate when that final status is
fail-class. The remaining unknown is therefore the exact cached Init return.

## v0.4.1 D3D12 status boundary

A direct feature-Init wrapper is not behavior-neutral. Both installed feature
images call `GetModuleHandleExA` with `FROM_ADDRESS | UNCHANGED_REFCOUNT` on
their immediate return address, resolve that module's path with
`GetModuleFileNameW`, and require the wide substring `nvngx.dll`. A wrapper in
`mhw_dlss_unlock.dll` would change the immediate caller and manufacture
`0xBAD00002`. Version 0.4.1 therefore records base Init lookups but always
returns the original pointer unchanged.

The safe observation point is the exact modern-core internal recorder at RVA
`0x264E0`. The pinned build has SHA-256
`B3C82638C60F29AEFF68052FFE7147246A73C80CBEA5883B35A9B0D1ED57DDE7`,
timestamp `0x6A0CB15E`, and image size `0x173000`. Its 28-byte entry prologue is:

```text
40 55 53 56 57 41 54 41 55 41 56 41 57
48 8D AC 24 18 FA FF FF 48 81 EC E8 06 00 00
```

The relevant call is `_nvngx.dll+0x676FF`, returning to `+0x67704`. Its opaque
seven-argument ABI carries feature index in argument 4, API in argument 6, and
final status in argument 7. The detour forwards every call exactly once and
publishes telemetry only for caller `+0x67704`, feature index 1, and API 2.
It preserves `LastError`, never dereferences an argument, and logs the raw
status without changing it.

The resolver worker is woken as soon as a relevant slot binds, then verifies
the complete mapped and on-disk modern-core identity before installing the
recorder hook. This remains timing-bounded rather than a process-start proof.
The v0.4 trace provides a measured margin: modern-core resolver binding occurred
at tick 78196718, its full identity completed at 78196734, and cached Populate
was not resolved until tick 78197156. Version 0.4.1 records its post-apply ready
tick, the first feature-DLL base Init lookup tick, and an atomic readiness
snapshot taken at that lookup. Ordering does not rely on millisecond-tick
equality, and a missed boundary is reported as inconclusive rather than treated
as a zero-call result.

The implementation and acceptance schema are documented in
[`../runtime/rtx50-unlock-test.md`](../runtime/rtx50-unlock-test.md).
