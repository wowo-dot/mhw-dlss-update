# v0.6.0 startup crash and callback containment

Date: 2026-09-07. The user reports a crash after the Capcom logo/loading screen,
before the main menu. Three v0.6.0 runs reproduce the same fault. The diagnostic
was rolled back to v0.5.3 while investigating; v0.6.0 is preserved with a
`.disabled` suffix. This was a gap in the bridge's callback ownership, not a
failure to restart MO2 or inherit `capture-create`.

## Confirmed evidence

All three runs completed both provider resolver transactions, both synthetic
Init calls and both Populate calls per provider. The 28 scalar queries show
DLSS and ISP Available `1` and FeatureInitResult `1` through both core layers.
The 20-second checkpoints have both lifecycle states `populated`, complete
route tables and outstanding Init cleanup. No Create or Evaluate is recorded.

| PID | Preserved log SHA-256 |
| ---: | --- |
| 46268 | `0E1E58F5C3728F706E18A50F7A7C443F7E34364C2340EA87DA86A3E0297F7931` |
| 6356 | `A96A1FF66DBA58FE117D1909F2A13916E7DB3A9EFF6F1EDC8BC023BB8E7ACDF4` |
| 10092 | `6FAEB6A5E2C0306C9EAABBFEC360A4E21D3E34CBD0D459C998BCC80CC5720F76` |

Logs are under `artifacts/logs/bridge-<pid>.v0.6.0.crash.log`. Windows reports
`0xC0000005` in cached DLSS 1.2.110, RVA `0xF8B4`. Bounded inspection of all
three local minidumps confirms a read from address `0x58` with RAX zero at that
instruction. The input identities remain the exact game/core/provider hashes
in [the bridge specification](modern-provider-bridge.md#pinned-chain).

In the inspected provider, `+0xF898` loads its `Block` runtime pointer and
`+0xF8B4` reads `[RAX+0x58]`. The caller is the optimal-settings callback at
`+0x22E10`, which calls this stateful GPU-type helper at `+0x22EA7`. The original
Populate at `+0x1DFA0` writes that callback pointer into
`DLSSOptimalSettingsCallback` before writing its scale-factor metadata.

The top return slots in PID 10092 agree with the inspected stack allocations:
`RSP+0x58` returns to provider `+0x22EAC`, and `RSP+0x188` returns to MHW
`+0x2383FE4`. MHW's `+0x2383EF0` function gets the callback from the returned
parameter object, writes Width/Height/PerfQualityValue/RTXValue, and calls it
at `+0x2383FE0`. Other scanned stack values are only candidates, not a complete
unwound stack trace.

The bridge deliberately does not initialize the original provider's state.
Forwarding its Populate therefore allowed a stateful function pointer to escape
the six-export lifecycle boundary. v0.5.3's failed ISP prerequisite prevented
the game from reaching this downstream path. The successful ISP publication in
v0.6.0 exposed it.

The older PID 5352 dump has a different fault, `FextyMod.dll+0x179B`; it is not
evidence of this callback crash and was not investigated further here. The
v0.5.3 rollback is a previous diagnostic baseline, not a claim of crash-free exit.

## v0.6.1 correction

With synthetic initialization active, the DLSS Populate wrapper never calls the
old Populate. It publishes only `bridge_optimal_settings_wrapper` and verifies
that exact pointer with a readback. It does not first publish the unsafe callback
and then try to overwrite it. The original DLSS scale-factor table is omitted
from this narrow containment build. ISP's separately inspected scalar-only
Populate remains forwarded. Disabled/unclaimed observation keeps original behavior.

The pointer setter/getter are MHW exports at `+0x273E8F0` / `+0x273E610`, Windows
x64 ABI `void(Parameter*, const char*, void*)` and
`uint32(Parameter*, const char*, void**)`. Worker preflight checks their export
RVAs and mapped ownership. Because the disk image is packed and the forced DLL
starts early, their captured entry bytes are checked at provider routing before
synthetic Init, and again immediately before publication. These checks only read
code. A mismatch rejects the admitted route; no integrity or unpacking mechanism
is modified. The crash minidumps do not include these host code pages. The new
runtime byte gates passed in the accepted v0.6.1 run below.

The new callback uses `uint32(Parameter*)`, preserves LastError and always returns
`0xBAD00001`. It never calls the original callback, writes output settings,
creates a handle, or submits GPU work. MHW checks the NGX failure class before
reading OutWidth, OutHeight, DLSSMode or Sharpness, and keeps its default scale
and disabled feature flags on failure. Successful sizing metadata remains a
later step; this build tests callback containment and captures input scalars.

Only the first callback invocation can attempt four scalar input queries. Its
caller must be MHW `+0x2383FE4` with the captured seven-byte call sequence, both
routes must remain verified, DLSS must be populated, both callback publications
must have passed readback, and the shared getter-fault latch must be clear.
The parameter pointer is borrowed synchronously; logging retains scalar copies.
Later invocations return the same failure without further queries. Faults stop
all later getters. There are two pointer-publication readbacks in addition to
the unchanged 28 capability scalars and the new four-scalar callback capture.

Regression coverage requires synthetic DLSS Populate not to forward the old
function and checks the four callback input names/types, missing getters and
faults at every query position. The full 10-test suite passes. Compile-time ABI
and binary CFG checks include the new callback. The runtime evidence below
validates the exercised path separately from these offline checks.

See the [test procedure](../runtime/modern-bridge-probe-test.md). Local dump
metadata, bounded IDA exports, source snapshots and rollback/build manifests are
under `local/dual-provider-20260907/crash/`; no dumps or proprietary code are tracked.

## Accepted v0.6.1 run

PID **34496**, 2026-09-07, used the recognized `capture-create` mode and the
320,000-byte Release DLL with SHA-256
`0E41068F677E82989A0E0AC8000F54021C2F1011C0E2FE6F93D1E5CCE5BAF91B`.
The complete log is preserved as
`artifacts/logs/bridge-34496.run001.v0.6.1.log` (40,982 bytes), SHA-256
`444036B3B8B6A0D5A8B77839260EA8510BEC012B7BC414DF9ADB825F77C788D3`.
The exact host, core and provider identity gates passed. Both provider
transactions completed with lookup mask `0x3FFF`, route mask `0x3F`, and no
provider route or guard rejects. Both synthetic Init calls verified D3D12.

All 28 capability scalar queries completed without a getter fault. Both core
layers returned availability `1` and Init status `1` for DLSS and ISP. The
20-second checkpoint recorded two successful local callback publications and
zero publication failures. Its zero callback-call count was an intermediate
observation: the callback ran later.

`bridge.optimal-settings`, sequence 58, verified MHW caller `+0x2383FE4` and
completed all four input queries:

| Input | Value |
| --- | ---: |
| Width | 2560 |
| Height | 1440 |
| PerfQualityValue | 0 |
| RTXValue | 0 |

The callback reported `original-called=false`, `outputs-written=false` and
the deliberate unsupported result `0xBAD00001`. The user's screenshot shows
the Display menu with DirectX 12 On, Full Screen, 2560x1440, CAS Off, and a
visible but unavailable NVIDIA DLSS row. This agrees with the captured host's
failure path retaining disabled feature flags. The screenshot and structured
run record are preserved under `local/dual-provider-20260907/runs/34496/`.

Sequences 59 and 60 record successful DLSS and ISP Shutdown from core caller
`+0x43815`. Both final lifecycle states are `inert`, with no outstanding Init
or token cleanup, no boundary contention and no dropped lifecycle events.
`trace.complete` reports 96,016 ms, `quiesced=true`, complete two-provider
startup coverage and zero dropped resolver events. No Scratch, Create,
Evaluate or Release was observed; no feature handle or GPU work was issued.

This run verifies startup containment, the local callback input contract and
both bridge shutdown paths. It does not establish rendering, Create
reachability, or stability across broader gameplay and display transitions.
The next diagnostic milestone is [controlled optimal-settings output metadata
in v0.6.2](diagnostic-optimal-settings.md) to reach the first Create call while
preserving Create's no-handle failure. Run 20164 accepts its sizing output,
selectable DLSS setting and both bridge shutdown paths, but still records no
provider Create. Subsequent [run 19800](first-create-capture.md) captures DLSS
Create and completes both shutdown paths after deliberate failure. v0.6.1
remains preserved as the prior containment baseline.
