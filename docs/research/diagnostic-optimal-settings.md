# Controlled optimal-settings output, v0.6.2

Date: 2026-09-07. Accepted run 20164 verifies sizing output and menu selection.
[Run 19800](first-create-capture.md) adds the first verified DLSS Create capture,
its deliberate no-handle failure and both bridge shutdown paths.
This follows [accepted v0.6.1 run 34496](optimal-settings-callback-crash.md#accepted-v061-run),
which captured Width 2560, Height 1440, PerfQualityValue 0 and RTXValue 0, then
deliberately returned unsupported. The user saw the DLSS row but could not
select it. Both bridge shutdown paths completed in that run.

## Confirmed host contract

The disk executable was rehashed before this analysis:
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`,
version resource `0.0.0.0`, recorded Steam build 15539686. The captured host
used by IDA has SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
All pinned core and provider identities remain mandatory; see the
[boundary specification](modern-provider-bridge.md#pinned-chain).

MHW `+0x2383EF0` invokes the callback at `+0x2383FE0`, returning to `+0x2383FE4`.
Only a result outside the NGX failure class allows the host to read OutWidth,
OutHeight, DLSSMode and Sharpness. For DLSSMode 1 and a width/height ratio of 1,
it returns scale 1 and feature flags 1. The downstream `+0x2384420` tests flag
bit 0 to call DLSS Create (feature 1), and bit 1 to call ISP Create (feature 9).
With flags 1, its ISP creation branch is skipped. A failed DLSS Create makes
that function return false before retaining a handle.

This establishes a static route to DLSS Create. Run 20164 confirms the menu is
selectable. Leaving Options with On applied subsequently reached Create in
[run 19800](first-create-capture.md).

## Output publication

The callback retains the Windows x64 ABI `uint32(Parameter*)`, exact caller and
call bytes, both complete provider transactions, DLSS populated state, and two
verified callback publications. It never calls the original DLSS callback or
Populate. The borrowed parameter object is used only synchronously; events
retain scalar values rather than its pointer.

Before writing, require all four input queries to complete, report presence and
return exactly 1 with the expected key/type. Width and Height must each be in
1..16384, PerfQualityValue in 0..2, and RTXValue exactly 0, as the inspected host
sets it. These are diagnostic bounds, not advertised rendering capabilities.

Write and immediately read back each value in this order:

| Key | Type | Diagnostic value |
| --- | --- | --- |
| OutWidth | uint32 | Input Width |
| OutHeight | uint32 | Input Height |
| Sharpness | float | 0.0 |
| DLSSMode | int32 | 1 |

DLSSMode is written last. Each readback must complete, report presence, return
1 and equal the requested value. Return success only after all four readbacks
and a clean boundary release. No render dimension is reduced; mode 1 is used
as routing metadata and does not claim working DLAA or DLSS.

The existing host getter exports are reused. The new setter exports and the
six scalar byte gates were checked in bounded IDA output, including their
parameter-vtable slot selection:

| Export suffix | Host RVA | Parameter vtable offset | Windows x64 arguments after Parameter* |
| --- | --- | --- | --- |
| GetUI | `0x273E550` | `0x60` | const char*, uint32* |
| GetI | `0x273E4F0` | `0x58` | const char*, int32* |
| GetF | `0x273E490` | `0x70` | const char*, float* |
| SetUI | `0x273E830` | `0x20` | const char*, uint32 |
| SetI | `0x273E7D0` | `0x18` | const char*, int32 |
| SetF | `0x273E780` | `0x30` | const char*, float (XMM2) |

All names have prefix `NVSDK_NGX_Parameter_`. Getters return uint32; setters
return void. Early preflight checks export RVAs and executable image ownership.
Routing, Populate and callback use check exact captured bytes, including the
selected vtable slot, after the packed host code is available. All six scalar
byte gates passed in run 20164. No code is patched by these checks.

## Failure and lifetime limits

There are at most 16 callback event slots and admitted attempts per process;
each can perform four input getters, four setters and four readbacks. Repeated
invocations recapture their inputs rather than reuse another request's sizing.
Budget exhaustion returns unsupported without parameter access and is logged.
The original 28 capability scalar queries remain a separate budget.

Missing/invalid input causes zero output writes. Missing accessors are rejected
before publication. A setter fault, readback fault or mismatch stops the
publication immediately; there is no attempt to repair partially written
metadata. Such a rejected publication poisons the DLSS boundary to prevent
later feature work. Host failure handling ignores the partial outputs. Platform
exceptions also latch off later parameter access across both providers.
Concurrent boundary ownership causes failure; independent Shutdown remains
available for each synthetic Init. Callback returns preserve LastError.

`capture-create` remains the only admitted active mode. Create continues to
capture its existing bounded contract, clear the output handle and return
unsupported. No original provider Init/Create/Evaluate, feature handle, resource
query in Evaluate, or GPU submission is enabled by this version.

## Verification and deployment

All 11 CTest tests pass, including successful 1:1 requests at multiple dimensions
and modes, invalid inputs, missing accessors, and faults/mismatches at every
setter and readback position. The Release build passes `/W4 /WX`; the existing
third-party CMake compatibility notice is unchanged. Binary inspection confirms
x64, CFG, and all 15 distinct provider wrappers/callbacks in its 103-entry CFG
table. The pure tests do not validate live game setter dispatch.

Release v0.6.2 is 331,264 bytes, SHA-256
`C2245D6D56A23DCE5AD3F449AE2370531D5C36AC9C530EE1AC87D57633CBFF48`.
It was staged with MHW closed; source and deployed hashes match. The accepted
v0.6.1 DLL is preserved as `mhw_dlss_bridge_probe.v0.6.1.dll.disabled` beside
the active MO2 DLL. The pre-test graphics settings were copied into the ignored
research directory. Game files and MO2 rules were not changed.

Local-only evidence, source baseline, binary audit, settings copy and deployment
manifest are in `local/sizing-20260907/`. See the
[offline procedure](../runtime/modern-bridge-probe-test.md#current-v062-sizing-and-create-test).

## Accepted v0.6.2 run 20164

The user enabled DLSS, saw the normal information popup and the setting remain
On, then returned it to Off and exited. They clarified that they stayed within
Options while it was On, without first returning to the title screen. The
persisted settings afterward are DLSS Off, DX12 On, FullScreen, 2560x1440.
No MHW process remained when the completed log was inspected.

The active DLL still matches the staged v0.6.2 hash above. The complete log is
`artifacts/logs/bridge-20164.run001.v0.6.2.log`, 45,753 bytes, SHA-256
`871D79AC2A6AE2A64E1FB8BB9DED35D8B95462543C42ECB138C93AD9B7CFBF8C`.
The screenshot and structured evidence are preserved under
`local/sizing-20260907/runs/20164/`.

The trace verifies the pinned host/core/provider identities, RTX 5090, recognized
`capture-create` mode, both complete provider transactions and synthetic D3D12
Init calls. All 28 capability scalar queries complete without a getter fault.
Both providers publish availability and Init status 1 through both core layers.

Callback sequences 58, 59, 60 and 61 each verify the exact caller and accessor
bytes, admission and clean boundary release, then return success. Each captures
Width 2560, Height 1440, PerfQualityValue 0 and RTXValue 0. Each writes and
verifies OutWidth 2560, OutHeight 1440, Sharpness 0 and DLSSMode 1. Thus this run
contains 16 successful input getters, 16 completed setters and 16 matching
readbacks. The 20-second checkpoint precedes these calls and its zero callback
count is not a final total.

Sequences 62 and 63 record successful DLSS and ISP Shutdown. Both final
lifecycle states are inert with no outstanding Init/token cleanup, no boundary
contention, no dropped bridge events and no lifecycle overflow.
`trace.complete` reports 52,922 ms, quiesced true, complete two-provider startup
coverage and zero dropped resolver events. No provider Scratch/Create/Evaluate/
Release event was observed and no feature handle was issued.

This run accepts sizing publication and the menu setting transition. The
subsequent [run 19800](first-create-capture.md) kept this DLL and left Options
while DLSS was On, reaching the first provider Create. Rendering remains absent.

Bounded follow-up IDA exports identify Create callers at `+0x23AA143` in
`+0x23A9CE0` and `+0x23AC670` in `+0x23AC560`. These paths reset the DLSS command
allocator/list, call the creation routine and derive readiness from retained
handles. This supports separating menu selection from renderer creation, but
does not directly identify the UI action that invokes those paths. The local
exports are in `local/sizing-20260907/create-trigger/`.
