# Two-provider Create capture, v0.6.0

Date: 2026-09-07. Historical v0.6.0 boundary. Three runtime tests completed both
provider transactions, then crashed in a stateful callback published by DLSS
Populate before Create. The [v0.6.1 correction](optimal-settings-callback-crash.md)
supersedes the DLSS Populate forwarding described here. Accepted v0.6.1 run
34496 verified startup containment, callback inputs and both bridge shutdown
paths; DLSS was visible but unavailable and Create was unobserved in that run.
The later v0.6.2 [run 19800](first-create-capture.md) completes DLSS Create
capture and both provider shutdowns; ISP Create remains unobserved.
This is a no-handle diagnostic, not a modern DLSS renderer. It follows the
[v0.5.3 ISP failure](isp-prerequisite.md) and preserves that build for rollback.

## Admission and ownership

The runtime has one owner for DLSS feature `1` and one for ISP feature `9`.
Each retains its own module identity, route transaction, lifecycle, callback
budget, contract-query budget, contention latch, event ledger and cleanup flags.
Neither owner's Init can succeed until both complete all 14 ordered lookups:
six base lifecycle wrappers, seven absent optional exports, and one bound
original Populate callback. Each must have route mask `0x3F` and lookup mask
`0x3FFF`, verified identities, distinct modules and the same core/resolver thread.
The admission read and resolver transactions share a lock. No lifecycle lock
is held across a foreign callback or the shared admission read.

Core function `+0x65510` resolves all 18 feature tables before returning through
`+0x65F80` into `+0x672D0`, which then invokes provider Init in a separate loop.
Thus the admission barrier does not depend on a future ISP lookup that requires
DLSS Init to have already succeeded. A missed first lookup leaves that provider
untouched and prevents synthetic Init success on its peer. A rejected admitted
route continues to contain its lifecycle; it never falls back to original
stateful exports after synthetic initialization.

The shared pieces are environment verification, parameter getter addresses,
the permanent getter-fault latch, and the four capability observation sources.
Any getter fault stops subsequent getter calls across both providers. The
current mode permits at most 11 scalar Create queries per provider and 28
capability queries across the four sources (50 total). Parameter objects and
command lists are borrowed only for their synchronous call. They are not
retained or used on the log worker. A D3D12 QueryInterface identity check obtains
and immediately releases its temporary COM reference; it submits no commands.

## Exact identities and ABI

The game, system resolver, current core and cached DLSS hashes remain those in
the [baseline boundary](modern-provider-bridge.md#pinned-chain). ISP is 1.2.103.0,
PE timestamp `0x601450EC`, image size `0x2A9000`, SHA-256
`3C47CD2504517120CF48BC943BCCF158190C540C646BAA3F95E547AD9B557EAD`.
Its path is derived from ProgramData and the observed cache layout
`NVIDIA/NGX/models/dlisp/versions/66154/files/160_B9D2F5C.bin`.
All seven entry prologues, export RVAs, mapped ownership and executable pages
are checked in addition to the disk hash/version.

These are Windows x64 calls, expressed with `__cdecl` C++ declarations. Every
result is a 32-bit NGX value. The core resolver uses the same ordered 14-member
sequence and return RVAs listed in the baseline for both provider indices.
The pinned ISP exports none of the seven optional ABI variants.

| Boundary | ISP RVA | Arguments | Invocation return |
| --- | ---: | --- | ---: |
| Init | `0x83B0` | `uint64 appId, wchar_t* path, ID3D12Device*, uint32 api` | core `0x6769A` |
| Scratch | `0x8550` | `int32 feature, Parameter*, size_t*` | host `0x273F212` |
| Create | `0x8580` | `ID3D12GraphicsCommandList*, int32 feature, Parameter*, void** handle` | core `0x67DE6` |
| Evaluate | `0x85C0` | `ID3D12GraphicsCommandList*, handle, Parameter*, callback` | core `0x6704A` |
| Release | `0x85A0` | `handle` | core `0x8AB7` |
| Shutdown | `0x84F0` | none | core `0x43815` |
| Populate | `0x8B40` | `Parameter*` | core `0x67ACE` / `0x67BC2` |

ISP Create's entry moves the command list from RCX to RDX, loads its internal
state into RCX, and tail-dispatches `+0x93F0`; it preserves the parameters in R8
and output handle in R9. Its unused incoming feature enum does not change the
four-argument ABI. Original ISP Init/Create/Evaluate/Release/Shutdown are never
called by the admitted diagnostic lifecycle. No original provider state exists
to clean up. The only retained original is Populate: six scale-factor metadata
setters, with no provider-state reference or GPU operation in the inspected body.
The bridge itself does not set availability or Init-result parameters.

Scratch requires special provenance: legacy `+0xA5C9` dispatches through a
push/return stub; modern `+0x66CF0` tail-dispatches its selected provider. The
return address reaching the wrapper is therefore MHW's stub at `+0x273F212`.
The bridge verifies that exact host caller, the stub's 20-byte prologue and its
10-byte argument/call sequence before returning the original provider's constant
size (`0x300000` for DLSS, `0x4E20` for ISP). These checks read mapped code only;
they do not patch the host. Captured-host source hash is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
The live byte check still needs its first runtime observation.

## Containment and validation

Only exact `capture-create` enables this build. `capture-one-evaluate` parses
as disabled/unrecognized. Create clears the output handle and returns failure
for either feature; repeated Create or invalid ordering cannot issue a token.
An unexpected Evaluate fails without querying resources or calling a callback.
Shutdown services each owner's synthetic Init obligation independently, including
after that owner becomes inert. One owner's completion does not stop logging
while the other still owes cleanup. The existing five-minute trace bound and
pinned-module lifetime remain; wrappers remain available for late cleanup.

All 14 public wrappers have compile-time ABI checks and distinct linker symbols
(`/OPT:NOICF`, CFG retained). Policy tests cover each missing/extra route bit,
identity/commit failures, wrong core/thread, aliasing and wrong provider identity,
unsupported modes, and capture/cleanup in both provider orders. The existing
snapshot fault/budget tests and probe mock suite also pass (10 CTest tests).
These tests do not execute the hash-gated resolver against MHW.

Logs label lifecycle events, checkpoints and summaries with `provider=dlss` or
`provider=isp`. Create phases are `create-dlss` and `create-isp`; event sequences
remain globally unique. Resolver storage grows from 32 to 64 records only in
the bridge build. Capability evidence remains six queries at each DLSS Populate
and eight at each GetParameters return, after both publications finish.

IDA evidence and build audits are local-only under
`local/dual-provider-20260907/`; proprietary images/databases remain untracked.
The [runtime procedure](../runtime/modern-bridge-probe-test.md) describes the
next run and rollback. Actual ISP route completion, capability publication,
Create reachability and orderly shutdown remain unconfirmed until that run.
