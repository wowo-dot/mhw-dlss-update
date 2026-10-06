# v1.23.2 live result: depth draw admitted, local color state absent

2026-09-27. PID 4296, creation 134350126378930622. Host version 0.0.0.0,
SHA-256 `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256 `716c862950d1e0a76e2bf2066a03652324dbd0ed2b839052793b6a34117b2b35`.
This is a partial capture, not accepted image evidence or a DLSS evaluation.

## Depth correction passed

The recorder observes twelve native depth producers across frames 15005-15008
and nine consumers across 15005-15007. Every observed consumer/draw returns
normally, with stable snapshots; depth-route fault remains zero. Third writers
use the 0x80 source state and firstVertex=3 that v1.23.1 rejected.

For selected frame 15007, the last matching producer is receipt 8. Its native
draw is accepted, and the recorder issues a full-allocation depth readback plus
an owned-depth copy. Native depth transitions RENDER_TARGET -> COPY_SOURCE ->
RENDER_TARGET. The smaller active rectangle remains 1280x720 inside 2560x1440.

The depth copy and TAA dispatch share list 348848352, allocator 348803136,
reset order 1891. Depth marker/begin/end are 1912/1915/1916, before dispatch
1923. `submission::mark` checks and accepts the exact frame, packet, command,
reset and allocator association. This proves a recording association, not GPU
completion: the journal closes before verified close/execute/fence/retirement.

## Next rejection

At TAA, the native packet and shader recipe remain equal and the native
dispatch returns. Its local barriers establish:

| Role | Local evidence |
| --- | --- |
| Color (tBaseMap) | No transition; state unknown, no conflict |
| History (tBlendMap) | Transition to 0x40 |
| Output (rwOutput) | UAV barrier only; state unknown |
| Motion (tGBuffer4) | Transition to 0xc0 |
| Previous GBuffer ID | Transition to 0x40 |

`consumer.cpp::dispatch_before` requires a known local color state of 0x40
before beginning either motion or color copies. It rejects the unknown color
state with fault 16. The original dispatch still forwards. Submission's
copy-before-forward check then reports fault 17, and top-level capture stops
with fault 13. No motion/color readback is attempted. The frozen review rejects
the result with `observer closure`.

There is one valid CPU input record (15007), plus an incomplete record for
15008 admitted during the failure. Both retain matched half-scale metadata,
but the incomplete record is not accepted as a second captured frame. No image
or shader files are saved. No private fence signal is attempted; no NGX work
is enabled.

## Existing implementation to reuse

The pattern resembles the already investigated continuous-DLAA case in
[native-state provenance](native-aa-state-provenance-result.md) and
[native-state admission](native-aa-state-admission-result.md): an earlier
scheduled command establishes a shared color read interval, and output can
remain UAV without a fresh transition in the TAA callback. That implementation
uses `NativeStateAdmission`, `StateTrace` and `NativeStateTrace` with bounded
current-use metadata, packet/ordinal/resource/epoch checks and rereads inside
the owning callback. It never treats a resource tracker's final planned state
as the current state or caches transient metadata across callbacks.

This capture contains no planner receipt for its rejected frame. Therefore
the earlier-command explanation is a supported lead, not a proven 0x40 state
for this run. Adapt the existing proof to the scaled capture path and retain
its evidence in the versioned review. Do not simply mark missing local states
as known, weaken the state requirement, or assume queue ordering. Reuse the
native/adversarial fixture coverage and add the recorder's copy integration
case before another live test. No new shader or render-scale policy is needed
to address the observed rejection.

## Cleanup and archive

The issued depth readback records `phase=quarantined`, `retained=true`,
`mapped=false`, and an empty filename. Retention keeps its source/list/
allocator/readback ownership alive until process exit because GPU completion
is unproven. Owned-input conversion was never attempted. Distinguish those
retained GPU allocations from the observer's released references.

All hook groups are disabled and pending zero. Observer references release
16/16 producer, 27/27 depth, 1/1 consumer and 231/231 submission. Both native
activation patch removal and callback drain pass. After 411 scale overrides,
all nine native scale values restore to 1. User reported a normal scene and
normal exit. Exact process exit, unchanged graphics/profile/mode, the installed
DLL and its output sidecar were verified. Keep this DLL installed.

Root `local/scaled-temporal-20260927/v1.23.2/` contains final log
`inputs-4296-134350126378930622-final.jsonl`, rejected
`review-4296-134350126378930622.json`, `state-analysis.json`, `run-result.json`
and the 396-file final manifest, including intact 388-file prelaunch evidence.

Log SHA-256 `46121af44a9e3451a19467e8439810270924c4748228a2a886389eabead1f316`.
Archive SHA-256 `756cc797eac33c0f0671d7af92ba7b5782a4506b52d93a4503955cb668b47e61`.
