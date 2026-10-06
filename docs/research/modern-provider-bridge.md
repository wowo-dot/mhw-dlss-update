# Modern DLSS provider bridge boundary

Status: v0.5.2 confirms successful DLSS capability delivery to MHW, while the
user still reported the menu row missing; Create and full cleanup were
unobserved in that version. Version 0.5.3 confirms that the
[ISP prerequisite](isp-prerequisite.md) is unsatisfied: availability `0`, Init
status `0xBAD00002`, with successful getters through both core layers.
Version 0.6.0 adds the [two-provider capture boundary](two-provider-create-capture.md).
Three tests completed those transactions but exposed a stateful callback outside
the exported lifecycle. The [v0.6.1 correction](optimal-settings-callback-crash.md)
contains that callback. Accepted run 34496 captured its four sizing inputs,
reached the Display menu with DLSS visible but unavailable, and completed both
bridge shutdown paths, with no Create in that run.
The [v0.6.2 sizing diagnostic](diagnostic-optimal-settings.md) passed its sizing
and menu-selection test in run 20164, with both bridge shutdown paths complete.
[Run 19800](first-create-capture.md) left Options while On and captured the first
verified DLSS Create: 11 scalar queries, intentional failure with no handle, and
both bridge shutdown paths complete. Version 0.7.0 adds the separately reviewed
[one-Evaluate diagnostic](one-evaluate-capture.md).
[Run 32832](first-handle-lifetime.md) passed live Create success, copied-handle
validation at Release and both Shutdowns, with no Evaluate call. Evaluate and
cleanup after it remain untested. The contract specifies the core-owned handle copy,
DLSS-only mode, stricter admission and the game's ignored Evaluate result.
[Loaded-scene runs 37452 and 28340](loaded-scene-evaluate-gate.md) repeat successful
handle cleanup without reaching Evaluate. Static analysis now locates the
temporal-pass selector and type-3 work-item producer; live gates remain unknown.
Version 0.7.1 adds [read-only renderer gate observation](renderer-gate-observation.md)
to measure those fields after Create. It passes all 13 tests; run 35100 has two
valid early samples with all selector gates ready and clean handle cleanup,
but still no Evaluate. Early Release ends the one-shot lifetime, after which
sizing rejects admission. The next attempt starts with saved Off and enables
once from the main menu. Run 43380 did so, reached a loaded save and recorded
12 valid ready-gate samples, all 11 sizing calls successful and clean cleanup
on exit, but zero Evaluate. Temporal-layer activation and enqueueing are the
next investigation; their execution is not instrumented.
Version 0.7.2 adds [historical shader-cache and deferral observations](temporal-activation-observation.md)
before and after Create. All 14 tests pass; live acceptance is pending. The
observations are read-only and do not count pass or enqueue executions.
The single-provider table below remains a baseline reference.
This milestone does not initialize
or evaluate a modern DLSS runtime and does not render an upscaled frame.

## Why the bridge replaces the DLSS 1 patch experiment

The NVIDIA cache selects the signed MHW feature build 1.2.110. Its D3D12 Init
returns `0xBAD00002` on the RTX 5090, and its embedded kernels contain SASS for
SM75 and SM86 but no PTX fallback. Changing its legacy adapter classification
would therefore not provide a viable Blackwell renderer. The old feature image
is retained as an ABI reference only; it is not patched, overwritten, or
redistributed.

The next useful boundary is one level higher. The current NVIDIA core resolves
the selected feature provider's lifecycle exports by name, stores the returned
pointers, and invokes them later. `mhw_dlss_bridge_probe` can capture the
contract at that boundary without modifying `MonsterHunterWorld.exe` or either
signed NVIDIA image.

## Pinned chain

All values below describe the one local build under investigation. A mismatch
is a hard rejection, not a reason to reuse the offsets on another build.

| Artifact | Pinned identity |
| --- | --- |
| `MonsterHunterWorld.exe` | PE timestamp `0x66D13CB6`, `SizeOfImage` `0x55D3000`, SHA-256 `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA` |
| Legacy `nvngx.dll` | 30.0.14.9516, PE timestamp `0x60E65AC5`, `SizeOfImage` `0x78000`, SHA-256 `3F598582D4C23530EF2F8B312ACA2561753E1C7B540A7822DEFDC6DC23AC8D12` |
| Current `_nvngx.dll` | 32.0.16.1047, PE timestamp `0x6A0CB15E`, `SizeOfImage` `0x173000`, SHA-256 `B3C82638C60F29AEFF68052FFE7147246A73C80CBEA5883B35A9B0D1ED57DDE7` |
| Cached feature | 1.2.110.0, PE timestamp `0x64405395`, `SizeOfImage` `0xB57000`, SHA-256 `DB203881EA5028CB584AA6C58D457361DD0E6A2817679878098B08B940549A03` |

The cached feature must resolve to
`%ProgramData%\NVIDIA\NGX\models\dlss\versions\66158\files\160_B9D2F5C.bin`.
The exact local system-resolver boundary is additionally pinned to
`KernelBase.dll` SHA-256
`9D26852459745322B704E2A49E448922839C584D06E208CF9ABCD0D857C4B7CD`
and `kernel32.dll` SHA-256
`0AB61F2E0D412A585233F1B308C120CBA74F800C1030BFFE0AABD20DF8C6D907`.
Those two identities are Windows-build-specific.

The observed call chain is:

```text
MonsterHunterWorld.exe
  -> legacy nvngx.dll resolver/stub slots
  -> current _nvngx.dll D3D12 feature table
  -> cached 1.2.110 feature exports
```

In the legacy core, Create resolves through call/return RVAs
`0x99C4/0x99CA` into slot `0x6E3B0`, Evaluate through
`0x99F4/0x99FA` into `0x6E3C0`, and Release through
`0x9A9C/0x9AA2` into `0x6E3F8`. The packed host prevents a similarly honest
static derivation of its internal call sites, so the bridge does not invent
host offsets.

The current core's D3D12 resolver performs one exact 14-lookup transaction.
The order, expected provider result, and resolver return are independently
checked:

| Order | Export | Expected cached target | Resolver return in `_nvngx.dll` | Class / bit |
| ---: | --- | ---: | ---: | --- |
| 1 | `NVSDK_NGX_D3D12_Init` | `+0x8410` | `+0x4320C` | base lifecycle `0x0001` |
| 2 | `NVSDK_NGX_D3D12_Shutdown` | `+0x8310` | `+0x43222` | base lifecycle `0x0002` |
| 3 | `NVSDK_NGX_D3D12_Shutdown1` | null | `+0x43241` | null guard `0x0040` |
| 4 | `NVSDK_NGX_D3D12_GetScratchBufferSize` | `+0x8370` | `+0x43257` | base lifecycle `0x0004` |
| 5 | `NVSDK_NGX_D3D12_CreateFeature` | `+0x83A0` | `+0x4326D` | base lifecycle `0x0008` |
| 6 | `NVSDK_NGX_D3D12_CreateFeature1` | null | `+0x43283` | null guard `0x0080` |
| 7 | `NVSDK_NGX_D3D12_EvaluateFeature` | `+0x83E0` | `+0x43299` | base lifecycle `0x0010` |
| 8 | `NVSDK_NGX_D3D12_ReleaseFeature` | `+0x83C0` | `+0x432AF` | base lifecycle `0x0020` |
| 9 | `NVSDK_NGX_D3D12_Init_Ext` | null | `+0x432C5` | null guard `0x0100` |
| 10 | `NVSDK_NGX_D3D12_Init_Ext1` | null | `+0x432DB` | null guard `0x0200` |
| 11 | `NVSDK_NGX_D3D12_Init_Ext2` | null | `+0x432F1` | null guard `0x0400` |
| 12 | `NVSDK_NGX_D3D12_PopulateParameters_Impl` | null | `+0x43333` | null guard `0x0800` |
| 13 | `NVSDK_NGX_PopulateParameters` | `+0x1DFA0` | fallback `+0x43351` | observed callback `0x1000` |
| 14 | `NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl` | null | `+0x43367` | null guard `0x2000` |

The six base lifecycle lookups form route mask `0x003F`; the seven optional,
guard-only lookups form mask `0x2FC0`; the generic Populate callback forms
mask `0x1000`. A complete lookup transaction is therefore `0x3FFF`.

Verified base-export invocation returns are Init `+0x6769A`, Create
`+0x67DE6`, Evaluate `+0x6704A`, Release `+0x8AB7`, and base Shutdown
`+0x43815`; base Shutdown takes no arguments. Scratch is validated through its
stored slot and arguments and reports the pinned `0x300000` bytes. The generic
Populate callback is invoked from `+0x67ACE` or `+0x67BC2` after being resolved
through the exact fallback at `+0x43351`.

The device/null call returns at `+0x43776` and `+0x43827` belong to Shutdown1,
not base Shutdown. Shutdown1, CreateFeature1, the three extended Init exports,
and the two D3D12-specific Populate implementations have ABI or control-flow
semantics that differ from their base counterparts. They are never coerced
through a base wrapper. Export names, target RVAs or expected nulls, entry
bytes, mapped PE ownership, caller image, caller RVA, and executable-page
properties must all agree.

## Provider substitution rule

The cached feature keeps private initialization state. Calling its Init and
then mixing its Create, Evaluate, Release, Shutdown, optional variants, or
Populate callback with bridge-owned state would be unsafe. The entire exact
14-lookup transaction is therefore all-or-nothing. The bridge preflights the
exact host, system resolver, modern-core file, cache file, and host getter
exports before enabling its resolver hook. The first provider lookup then
verifies only bounded mapped-memory/path/address evidence; it performs no disk
I/O under the resolver.

The transaction must begin with Init and follow all 14 rows above in that
order. The six base entries route only to their matching local lifecycle
wrappers. Each of the seven ABI-sensitive optional symbols must resolve null;
the bridge returns guarded null without passing it through any base wrapper.
An unexpected non-null optional target is suppressed to null, poisons the
transaction, and increments the guard rejection and suppression telemetry.
The exact non-null generic `NVSDK_NGX_PopulateParameters` target at cached RVA
`+0x1DFA0`, resolved by the current-core caller at `+0x43351`, must be bound and
routed to `cache_populate_wrapper`. It is not allowed to remain a raw callback
inside an otherwise substituted table.

Init cannot return synthetic success unless the base route mask is `0x003F`,
the seven guard lookups contribute `0x2FC0`, the callback lookup contributes
`0x1000`, the total lookup mask is `0x3FFF`, and the route state remains clean.
Completion is release-published only after the final null guard has been
validated, so a concurrent invocation cannot observe a merely recorded but
not-yet-validated final lookup.
If the worker first observes any later member, it does not join halfway: the
whole run remains pass-through and is reported as
`inconclusive-init-lookup-missed`. Once Init has been substituted, any missing,
reordered, wrong-module, wrong-target, unexpected non-null guard, wrong-caller,
or callback-binding failure poisons the transaction. Admitted base members
then return local fail-closed wrappers, guard variants remain suppressed to
null, and an unusable generic callback is suppressed rather than leaking a
mixed raw/bridge provider table. Unrelated NGX lookups after the complete
14-member transaction are not claimed as part of this provider set.

The resolver hook is installed by a worker created from `DllMain`; MO2 Force
Load does not by itself prove that the worker won the race against the game's
first NGX lookup. A zero-route or missed-Init run is therefore timing evidence,
not a claim that the provider is unsupported. A later renderer bridge will
need loader-safe startup synchronization rather than relying on retries.
The legacy core's pinned identity remains call-chain and runtime telemetry
evidence rather than an unsafe disk check inside the provider resolver
callback.

The generic `PopulateParameters` callback is the one non-lifecycle member of
the transaction. `cache_populate_wrapper` invokes the exact bound callback
under the same non-waiting boundary transaction, observes both permitted
callers, and accepts up to two population transitions for the capability and
device parameter objects. Create may also follow Init directly because the
current core can skip its later population pass.

No mode patches an instruction, spoofs a GPU name, copies a GPU resource, runs
an old kernel, or submits a command list. Capture storage is fixed and bounded
to 28 Create/Evaluate getter attempts plus at most 28 capability getter attempts
in v0.5.3: one complete Create, one complete Evaluate, and four scalar capability
snapshots at distinct boundaries.
Within Create/Evaluate, the first callback fault, wrong token, wrong order,
unverified caller, identity change, or capacity exhaustion latches the lifecycle
inert. A capability-only getter fault disables all later host getter calls
without replacing the upstream GetParameters result; outstanding cleanup
obligations remain intact.

Provider wrappers use a single non-waiting boundary transaction around
Populate mutation and Create/Evaluate getter capture. Unexpected concurrent or
re-entrant entry poisons the active transaction, so it cannot later publish a
successful synthetic Init or token. Exact base Shutdown and exact-token
Release retain independent cleanup obligations and may consume those
obligations even after poisoning; Shutdown never silently consumes an issued
token's separate Release obligation.

## Explicit modes

`MHW_DLSS_BRIDGE_MODE` is parsed exactly and is disabled for an empty,
misspelled, padded, or differently cased value.

| Value | Deliberate behavior |
| --- | --- |
| `disabled` | Default. Preserve the v0.4.1 resolver/status observation path and do not substitute a feature provider. |
| `capture-create` | Satisfy only enough of the provider handshake to capture one Create parameter contract, return no feature handle, and fail Create closed. Evaluate is never accepted. This is the first runtime test mode. |
| `capture-one-evaluate` | v0.7.0 separate opt-in test, runtime acceptance pending. DLSS may issue one packed token after gated Create, validate the core's copied handle and capture one Evaluate. ISP stays Create-only. Evaluate returns failure with resources untouched; MHW ignores that return, so an incomplete frame is possible. |

The one-evaluate token has feature index 1 in its high DWORD because the current
core copies eight bytes into its own record and reads that field before dispatch.
Release must accept the verified copied address, not require the original pointer. It is a
control-flow token, not a modern DLSS feature handle, and success at that step
does not mean rendering occurred.

Create capture is limited to dimensions, node masks, quality/RTX values, and
the legacy HDR hint. The optional Evaluate capture records only bounded legacy
keys for color, motion vectors, previous output, depth, output, motion-vector
scale/offset, sharpness, reset/depth/DLAA hints, and pass hints. Resource values
are logged only as opaque process-local pointers; the probe neither describes
nor retains their resources. Any protected getter fault stops all later
queries.

The pinned provider handshake additionally requires application ID
`0x0B9D2F5C` and NGX API version `0x12`. The API value is Init's fourth
argument; it is not feature snippet metadata. The shipped 1.1.13 feature
reports snippet `0x1010D`, while cached 1.2.110 reports snippet `0x1026E`.
Once synthetic Init or a one-Evaluate token has been returned, the exact
base-Shutdown caller and exact-token Release remain cleanup-success paths even
if a later capture fault has made the policy inert. This prevents bridge-owned
state from being stranded in the current core; it does not resume rendering.

The first controlled `0.5.0` run on 2026-09-04 proved the full Init-first
14-lookup route, but failed closed before device verification because the
bridge incorrectly compared Init's observed `0x12` against shipped-feature
snippet `0x1010D`. Static disassembly confirms the selected base-Init branch at
`_nvngx.dll+0x67694` loads the fourth argument from its API-version field into
`r9d` and returns at the pinned `+0x6769A` boundary. The separate Init-Ext
branches have a fifth argument; this base export does not. Version `0.5.1`
corrects the API invariant and logs a seven-bit Init validation mask; `0x7F`
means caller, lifecycle, application ID, application path, API version, route
table, and D3D12 device all passed.
The ignored local run artifact is `bridge-45488.run001.v0.5.0.log`, SHA-256
`C237A932008879C7C0AFC61CF756290EA7BA48383AB4F92FCDC5432429EFE02D`.

The first controlled procedure is in
[`../runtime/modern-bridge-probe-test.md`](../runtime/modern-bridge-probe-test.md).
