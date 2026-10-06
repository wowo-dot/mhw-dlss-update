# One-Evaluate diagnostic contract

**2026-09-22 update:** [Run 12196](depth-argument-retry.md) captured the first
Evaluate after one externally corrected depth-preparation argument. All 17
getters completed without fault, and all four non-null resource pointers match
the captured work item. The following frame crashed at the uncorrected depth
fault; Release/Shutdown and resource descriptions/states remain unverified.
This accepts the parameter capture, not full lifecycle or rendering. The
original implementation contract and earlier history follow.

Status: v0.7.0 implements the next opt-in diagnostic after the accepted
[v0.6.2 Create capture](first-create-capture.md).
[Run 32832](first-handle-lifetime.md) accepts Create success, the copied handle
at Release and clean shutdown. Evaluate was not called; full acceptance is pending.
The subsequent [loaded-scene attempts](loaded-scene-evaluate-gate.md) also have
zero Evaluate calls. The temporal-pass selector and queue producer are now
mapped statically; their live activation conditions are the next observation.
The purpose is one bounded capture of MHW's Evaluate parameters and a verified
Release/Shutdown sequence. No modern reconstruction runtime is loaded.

## Identities and evidence

The game and current core were rehashed before deriving these offsets on
2026-09-07. All RVAs below require these exact identities and the existing
two-provider startup/host accessor admission checks.

| Image | Version / SHA-256 |
| --- | --- |
| Game executable | Steam build 15539686, PE timestamp `66D13CB6`, version `0.0.0.0`; `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA` |
| Captured host used by IDA | `D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C` |
| Current `_nvngx.dll` | `32.0.16.1047`, PE timestamp `6A0CB15E`; `B3C82638C60F29AEFF68052FFE7147246A73C80CBEA5883B35A9B0D1ED57DDE7` |

Local, untracked evidence is under `local/evaluate-20260907/`: identity records,
bounded `host-handle` and `core-handle` IDA exports and `contract-bytes.json`.
The captured host remains a static reference; the runtime compares its relevant
accessor bytes against the live, hash-pinned game after unpacking.

## Core ownership and ABI

The provider entry points use the Windows x64 ABI. Create receives a D3D12
graphics command list, uint32 feature index, parameter pointer and handle-output
pointer. Evaluate receives a command list, handle, parameters and progress
callback. Release receives the handle; base Shutdown has no arguments.
The progress callback is never invoked by this diagnostic.

Core Create `+0x67C60` calls the provider and returns from it at `+0x67DE6`.
On success, the core allocates a 32-byte record:

| Offset | Value |
| --- | --- |
| `+0` | uint32 operation `1` (Create) |
| `+4` | zero reserved DWORD |
| `+8` | cloned parameter object pointer |
| `+16` | original creation command-list pointer |
| `+24` | **eight-byte copy** of the provider's returned handle contents |

The handle returned to MHW is **record + 24**, not the provider's original
pointer. Core Evaluate `+0x66F90` selects the provider using the signed DWORD
at handle + 4 and forwards the copied handle unchanged; its provider return
address is `+0x6704A`. Core Release `+0x8A30` does the same, returning from the
provider at `+0x8AB7`. A successful Release adds a separate operation-2 record.
The core frees the recorded allocations during Shutdown `+0x43550`, after the
provider Shutdown call returning at `+0x43815`.

The DLSS diagnostic's process-lifetime uint64 payload is
`0x000000010000B12D`: feature index 1 in the high DWORD. The old dormant pointer
comparison would reject the core's copy and has been replaced. No ISP handle
is issued: global `capture-one-evaluate` maps ISP to `capture-create`.

Create success additionally requires the complete scalar capture and the nine
present keys seen in run 19800: nonzero bounded equal input/output dimensions,
both node masks 1, quality 0..2, RTX 0 and Hint.HDR 1. Absent ResourceWidth and
ResourceHeight remain permitted. Live byte checks cover the core's copy,
returned-pointer construction, Evaluate dispatch, Release call and the host's
D3D12 resource getter at `+0x273E430` (parameter vtable slot `+0x48`).

Only an exact provider caller with outstanding token and Init ownership can
read the copied record. Validation checks alignment, a readable committed
32-byte range and an SEH-protected copy; it checks the fields above against the
issued value and original creation command-list identity. The first validated
copied address becomes the only accepted address for that lifetime. The cloned
parameter and command-list pointers are never dereferenced or retained here.
A read fault permanently disables handle reads. A separate SRW lock serializes
the bounded read with retiring Init ownership in Shutdown, preventing reads
from starting after the core can free its records.

Release may occur before the first Evaluate. A verified copied handle consumes
the token obligation exactly once. Release must precede Shutdown for successful
acceptance; Shutdown does not erase an outstanding token obligation. Independent
cleanup remains available after a capture-policy failure, subject to the same
caller, handle and lifetime checks. The bridge never frees core-owned memory.

## One capture and rendering limitation

At most the first Evaluate invocation attempts admission and foreign queries.
It must have the exact core caller, the verified copied handle, a real D3D12
command list, a parameter pointer, complete routes and the expected lifecycle.
The capture budget is 17 getters: five opaque resource pointers, six float
values and six signed values. Missing optional keys are recorded as absent.
Any protected getter fault stops subsequent queries. Repeated Evaluate calls
return `0xBAD00012` without more handle/parameter reads or per-frame events;
the summary counts total and suppressed calls.

Sizing remains available between Create and the first Evaluate, then rejects
further admission. A rejected sizing callback performs no parameter query or
write and is logged as `admission-rejected`, without a synthetic getter fault.
The 16-callback process budget remains unchanged.

**The game ignores the Evaluate return value.** In captured host function
`+0x2383CF0`, the D3D12 Evaluate call at `+0x2383EB6` is followed by leaving the
critical section without checking the result. Thus the diagnostic's failure
return cannot guarantee a rendered fallback. An incomplete or stale frame is
possible while On. This is a brief offline capture experiment, not a usable
renderer; turn DLSS Off and exit promptly after the attempt. The diagnostic
does not allocate, retain, describe, transition, copy or write GPU resources.

The host cleanup at `+0x23848D0` calls Release for a retained handle and clears
its handle field. Run 32832 now validates successful Release before both
Shutdowns, with DLSS Off afterward, on a path with no Evaluate. Cleanup after
Evaluate remains a separate live acceptance requirement.
Resource formats, dimensions, states, HDR interpretation, jitter and frame
alignment remain unverified even if the opaque-pointer capture succeeds.

## Verification and runtime acceptance

The Release build uses `/W4 /WX`; the new portable tests cover packed feature
IDs, per-provider mode selection, copied-record/address validation, Create
admission and sizing lifetime. Existing tests cover one-shot lifecycle and
snapshot budgets/faults. These do not establish live handle compatibility.
The local PE audit checks x64, CFG instrumentation and all 15 distinct provider
wrappers/callbacks in the CFG target table.

Follow the [v0.7.0 offline procedure](../runtime/modern-bridge-probe-test.md).
Accept only a version-identified `capture-one-evaluate` run with complete
two-provider startup, DLSS Create success/token, one committed Evaluate and
complete 17-query snapshot. The `bridge.handle` event must show all four checks
true. Require successful copied-handle Release before both provider Shutdowns,
no outstanding cleanup, no faults/contention/overflow, and a quiesced trace.
ISP must remain in effective `capture-create` mode. Repeated-call suppression
is expected if MHW continues invoking Evaluate. An absent Evaluate or failed
handle check is an inconclusive/failed attempt, not modern DLSS success.
