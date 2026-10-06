# DLSS capability publication and the MHW return boundary

Date: 2026-09-07. Version 0.5.2 confirmed successful DLSS capability delivery to
MHW while the user reported the menu row still missing. Version 0.5.3 confirms
ISP availability is present zero and its Init status is `0xBAD00002` at both
GetParameters returns. Modern DLSS rendering is unimplemented.

These are historical capability baselines. The later
[accepted v0.6.1 run](optimal-settings-callback-crash.md#accepted-v061-run)
publishes success for both providers, captures the local sizing callback, shows
the DLSS row as unavailable, and completes both bridge shutdown paths.

## Identity and evidence

The executable, cached feature, legacy core and current core were hashed again
before this trace. They still match the complete identities in
[the provider boundary specification](modern-provider-bridge.md): host
`C2EBBBD2...79D90EA`, cached 1.2.110 `DB203881...549A03`, legacy core
`3F598582...AC8D12`, current core `B3C82638...57DDE7`. The host version resource
is unusable (`0.0.0.0`); the recorded Steam build is 15539686.

IDA 9.3 exported targeted functions from a working copy of the modern-core
database and a new legacy-core database. The exporter verified database input
MD5 and on-disk input SHA-256. Local-only evidence is under
`local/capability-trace-20260907/`: `identities.json`, `export_targets.py`,
per-pass configurations/reports, assembly and decompiler output. Original
databases and signed binaries were not edited. Call instructions and the
parameter implementation were used to check decompiler types.

The prior log `artifacts/logs/bridge-45284.run002.v0.5.1.log`, SHA-256
`CDA7DB8D9F89ABA196AC21C6F937FDC3D97248E72A4F160C868C0F762CD0BA7A`,
proves diagnostic Init passed all seven gates, the driver recorded success,
and both cached Populate calls returned success. It has no Scratch, Create,
Evaluate, Shutdown, or final trace summary. It does not prove resource capture
or complete lifecycle cleanup.

## Confirmed publication sequence

1. Cached `NVSDK_NGX_PopulateParameters` at RVA `0x1DFA0` writes
   `DLSSOptimalSettingsCallback` and 40 pairs of resolution scale factors.
   Its explicit parameter writes do not set `SuperSampling.Available`.
2. The current core's function at RVA `0x677A0` sets the capability fields.
   Availability requires a non-failure feature Init result AND a signed
   comparison of the recorded device architecture against the feature's
   architecture threshold (`+0x679CC` through `+0x679D2`). Populate is gated
   only on Init's failure class and callback presence. It can therefore run
   successfully while availability is zero.
3. The first object is the capability object at core context `+0x2470`.
   Its fields are written before the callback returning at `+0x67ACE`.
   The second is a distinct per-device object, populated before the callback
   returning at `+0x67BC2`.
4. On first construction, the capability object is cloned at `+0x67C02` and
   stored at context `+0x2468`. The clone entry is D3D12 parameter vtable slot
   17 (`+0x88`), implemented at `+0x654C0`. Copy/deep-copy helpers preserve
   values and rebuild named/encoded aliases (`+0x34C0`, `+0x5D4C0`,
   `+0x5D340`). This clone is not the second Populate argument. Later
   population of an existing capability object does not refresh `+0x2468`
   through this first-construction branch.
5. Modern GetParameters at `+0x66C50` directly returns `+0x2468`.
   GetCapabilityParameters at `+0x669F0` instead clones `+0x2470` each call.
   These are different APIs and ownership paths.
6. Legacy GetParameters at `nvngx.dll+0xA582` transfers control through slot
   `+0x6E3D0`, preserving the argument and return boundary. There is no
   parameter translation in that stub. The v0.5.1 run recorded the outer
   return to MHW at `+0x23842FD`.

Device architecture is fed from NVAPI discovery (`+0x68000` via `+0x65F80`
and `+0x672D0`); the pinned core explicitly recognizes architecture `0x1B0`.
Neither the actual device field nor threshold was captured in the game.
The v0.5.2 result below shows this additional gate did not suppress DLSS
availability in that diagnostic run. It does not establish compatibility of
the original provider, whose Init was replaced by the diagnostic bridge.

## Verified scalar contract

| Key | Written type | Current-core key helper RVA |
| --- | --- | ---: |
| `SuperSampling.Available` | signed 32-bit | `0x3C7F0` |
| `SuperSampling.FeatureInitResult` | signed 32-bit; retain status bits | `0x3F0B0` |
| `SuperSampling.NeedsUpdatedDriver` | signed 32-bit | `0x3D200` |
| `SuperSampling.MinDriverVersionMajor` | unsigned 32-bit | `0x3DC10` |
| `SuperSampling.MinDriverVersionMinor` | unsigned 32-bit | `0x3E660` |

The availability short key is bytes `23 01`, followed by NUL. Both it and the
named key are queried. Logs label the short form
`SuperSampling.Available[encoded-0x01]`, without embedding a control byte.

The pinned D3D12 parameter vtable is at RVA `0xB4970`; signed/unsigned setters
are at offsets `+0x18`/`+0x20`, and corresponding getters at `+0x58`/`+0x60`.
Runtime capture uses the existing preflighted host C getter exports, not an
assumed C++ vtable layout. The modern getter implementation shares a lookup
helper that can allocate a missing entry. "Observation" therefore means no
capability setters or intentional value changes; it does not mean a getter
can never allocate or change internal bookkeeping.

## Accepted v0.5.2 capability evidence

Use the second run, `artifacts/logs/bridge-42060.run002.v0.5.2.log`, 26,469 bytes,
SHA-256 `A6B74F044D0F3DB4A68D835BA2DEF5200DC1D92F7C930AE378855501B24330E1`.
The first run, `bridge-9944.run001.v0.5.2.fullscreen-setup.log`, is preserved
only as setup context because the user switched to fullscreen during it.
The user explicitly reports the DLSS row was still missing in the second run.

The second run verifies the pinned host, cores, feature and getter targets,
the complete provider route, diagnostic Init validation `0x7F`, and both
successful Populate calls. All four sources contain these identical values:

| Field | Present | Value |
| --- | --- | --- |
| `SuperSampling.Available` | yes | `1` |
| Availability encoded alias | yes | `1` |
| `SuperSampling.FeatureInitResult` | yes | `0x1` (success) |
| `SuperSampling.NeedsUpdatedDriver` | yes | `0` |
| Minimum driver major/minor | yes | `418` / `81` |

Capability Populate uses pointer `0x93E31C00`; device Populate uses
`0x93E42100`. Modern and legacy GetParameters both return `0x93E42700`.
These are opaque, run-specific identities. The matching values and aliases
rule out a lost availability value across these captured boundaries.

The 20-second checkpoint reports source/complete masks `0xF`, 24 queries,
zero busy skips, no getter fault, and lifecycle state `populated`. It also
reports `init-cleanup-pending=true`. There are no Scratch, Create, Evaluate,
Release, Shutdown, final summary or stop records. This accepts capability
delivery evidence only; it does not certify full cleanup or resource capture.

## Captured MHW consumer: an additional ISP requirement

A working copy of the existing captured-game IDA database was inspected after
the second run. Input MD5 is `A137D3636F22A730F71522C193B0472E`; captured-image
SHA-256 is `D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
This differs from the packed on-disk executable hash by design. The current
on-disk executable was rehashed and still matches `C2EBBBD2...79D90EA`.
Local-only exports are in `local/capability-trace-20260907/host-consumer/`.
The database/input originals were not changed. The runtime return RVA anchors
this trace, but the full captured function's bytes were not revalidated live;
no new hook or patch is derived from these addresses.

The captured D3D12 setup function begins at MHW RVA `0x23842A0`. Its
GetParameters call returns at `0x23842FD`, matching the second runtime log.
Assembly confirms this sequence after a successful, nonnull GetParameters:

1. Query signed `SuperSampling.Available` directly through vtable `+0x58` at
   call RVA `0x238432E` (return `0x2384331`). Getter failure or value zero
   writes diagnostic value `2` at RVA `0x51C6C28` and returns null.
2. Query signed `ImageSignalProcessing.Available` through the same slot at
   call RVA `0x238435F` (return `0x2384362`). Getter failure or value zero
   writes diagnostic value `3` at the same location and returns null.
3. Only after both pass does it allocate and construct the NGX wrapper object.
   Earlier Init/GetParameters failure instead writes diagnostic value `1`.

The game calls the vtable directly here, so observing only calls to its
exported `NVSDK_NGX_Parameter_GetI` would miss these two reads. The export
itself forwards through the same `+0x58` slot used by the existing probe.
This establishes the ISP prerequisite in the capture, not the actual branch
taken during the second run or the complete menu-visibility decision.

The second run maps a separate cached `dlisp` 1.2.103.0 module, SHA-256
`3C47CD2504517120CF48BC943BCCF158190C540C646BAA3F95E547AD9B557EAD`.
The DLSS-only bridge leaves that provider untouched. Its presence is not
proof of successful ISP Init or availability. The current-core helpers
`+0x3C7F0` and `+0x3F0B0` map feature index `9` to
`ImageSignalProcessing.Available` and `ImageSignalProcessing.FeatureInitResult`;
the publisher writes both as signed 32-bit values. ISP is visited after DLSS
(feature index `1`) in the publication loop at `+0x677A0`.

## Capability diagnostic, v0.5.3

The existing explicit capture modes admit one snapshot per source. Populate
retains its original six queries. Each GetParameters return adds the two signed
ISP values, for eight queries. ISP is intentionally not queried during DLSS
Populate because its later publication may not have happened yet. Those two
early records have `attempted=false`; they do not mean ISP is missing or zero.

| Source / mask bit | Position |
| --- | --- |
| `capability-populate` / `0x1` | Successful exact Populate, return `+0x67ACE` |
| `device-populate` / `0x2` | Successful exact Populate, return `+0x67BC2` |
| `modern-get-parameters` / `0x4` | Successful modern return nested inside the verified legacy wrapper |
| `legacy-get-parameters` / `0x8` | Successful legacy return to exact MHW caller `+0x23842FD` |

GetParameters capture also requires verified slot identities, the complete
provider route, outstanding successful diagnostic Init, the provider-lookup
thread, and populated lifecycle state. It borrows the existing non-waiting
boundary lease; a busy boundary skips observation without waiting or poisoning
that busy transaction. GetParameters result and LastError are preserved.
No new detour or returned export wrapper is added. The existing x64 ABIs are
`uint32_t __cdecl(NgxParameter*)` for Populate and
`uint32_t __cdecl(NgxParameter**)` for GetParameters.

Snapshots are synchronous while the parameter object is valid. No ownership
is acquired, released or transferred. Only scalar copies and opaque pointer
integers reach the writer. At most 28 capability getter attempts are added to
the existing 28-attempt Create/Evaluate budget. All typed getter adapters share
an exception latch: after the first fault, none invokes another host getter.
An absent field retains `present=false` and its raw getter result; absence must
not be interpreted as a present zero.

A single checkpoint after 20 seconds reports `final=false`, source/complete
masks, query count, faults, skipped busy observations, lifecycle state and
outstanding Init/Release cleanup. It neither stops recording nor claims
Shutdown succeeded. No work is added to DllMain shutdown.

## Accepted v0.5.3 ISP evidence

The completed run is `artifacts/logs/bridge-5352.run001.v0.5.3.log`, 28,244 bytes,
SHA-256 `62E538E4C287775804737AA3E4C3A3F3C1F419CF0BC976D084EC37F681963E21`.
It identifies version 0.5.3 in exact `capture-create` mode and the same pinned
host, cores, DLSS provider and RTX 5090 inventory as the preceding run.

All six DLSS values repeat the accepted v0.5.2 results at all four sources.
At both modern and legacy GetParameters returns:

| Key | Getter result | Present | Value |
| --- | --- | --- | --- |
| `SuperSampling.Available` | `0x1` | yes | `1` |
| `SuperSampling.FeatureInitResult` | `0x1` | yes | `0x1` |
| `ImageSignalProcessing.Available` | `0x1` | yes | `0` |
| `ImageSignalProcessing.FeatureInitResult` | `0x1` | yes | `0xBAD00002` |

The ISP query itself succeeded; the returned Init status is the failure.
The local NGX header defines that status as `FAIL_PlatformError`, which does
not by itself identify an architecture, driver, caller-validation or other
internal cause. Both returns use pointer `0x7EF6FFF0`; the capability/device
Populate pointers are separately `0x7EF5E370` and `0x7EF6EBF0`.

Both Populate summaries report six attempted queries; both returns report
eight. The four early ISP records are intentionally unattempted. The checkpoint
reports source/complete masks `0xF`, 28 queries, no busy skips or getter fault,
complete routing, and lifecycle `populated`. Diagnostic Init again passes
`0x7F`. No Scratch, Create, Evaluate, Release, Shutdown, final summary or stop
record appears; Init cleanup remains pending at the checkpoint.

This confirms an unsatisfied prerequisite presented to MHW and supports the
captured ISP failure branch as the explanation for setup stopping. It does
not directly observe MHW's two subsequent getter calls, its diagnostic state
write, or every menu condition. The earlier report of a missing row belongs to
the v0.5.2 second run; this run was reported completed without a separate visual
description. Full cleanup and resource capture remain unproven.

See [the ISP lifecycle research](isp-prerequisite.md) for the downstream
feature use and requirements for the next bridge milestone.

## Next research gate

- Map the separate ISP provider lifecycle before changing its advertised state.
- Preserve the distinction between recorded ISP Init failure and its unknown
  internal cause. Directly wrapping its original Init changes caller provenance.
- The next implementation must account for both feature providers and retain
  the current failure behavior until its complete boundary is verified.
- Repeating the same v0.5.3 menu test adds no missing contract evidence.

Use the [controlled test procedure](../runtime/modern-bridge-probe-test.md).
Create/Evaluate remain separate capture gates. This diagnostic introduces no
availability override or modern SDK evaluation.
