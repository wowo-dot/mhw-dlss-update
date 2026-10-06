# Independent AA: live post-processing queue boundary

2026-09-26: observer v0.1.0 passed its eight-frame CPU callback capture with
native TAA, FXAA and DLSS Off. This establishes a live scheduling boundary that
does not require TAA to be selected. No owned pass was inserted and independent
DLAA is not yet connected to this route.

## Identity and capture

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
PID 61960, creation FILETIME 134349152740821787. Observer DLL SHA-256
`27e1dac69c9308bd13c7940af928a1cf0e66c6f6fcc20592ec9e1eecf91106f7`.
The [hook contract](../runtime/independent-aa-queue-boundary.md) specifies the
two complete code gates, Windows x64 forwarding ABI and ownership limits.

Startup discovery identified D3D12, adapter LUID low 98931/high 0, swap-chain
format 87, 2560x1440. The exact installed DLL, process generation, four saved
configurations and all 338 prelaunch files were verified before arming. All 317
frozen sources are retained. The user reported the save loaded before capture.

The observer recorded eight calls and eight complete before/after pairs, frames
17259 through 17266. Each side had two equal scalar reads, and each pair retained
the same frame, epoch, bank, manager, renderer, primary view, context and full-size
rectangle. Banks alternated 1/0. All samples came from thread 61632; first-to-last
sample tick span was 47 ms.

## Confirmed observations

The manager entered enabled and collecting, with four descriptors. It returned
with collecting cleared and the same four descriptors sorted. Both AA option
bytes remained zero. None of the four known native TAA/FXAA unit vtables appeared
in this captured queue. Each native phase appended 31 context packets; this is
CPU packet production, not a measurement of GPU execution or dispatch count.

All descriptors had phase 15. The observed sorted owner chain was:

| Unit vtable RVA | Order | Input owner | Output owner | Flags +20/+21/+22 |
| --- | ---: | --- | --- | --- |
| 0x3538400 | 0 | View attachment +3e0 | Manager scratch +c0 | 0/0/0 |
| 0x3537a70 | 100 | Manager scratch +c0 | Manager scratch +c8 | 0/0/0 |
| 0x3536c98 | 1024 | Null | Manager scratch +c8 | 0/1/0 |
| No unit | 134217713 | Manager scratch +c8 | View attachment +420 | 1/0/0 |

Descriptor identities, phase/order/flags and owner pointers were stable across
all captured calls. Input/output pointers already matched their returned values
on entry: this capture does **not** independently prove that the resources were
newly written, or that their pixels were current. Static inspection establishes
where the native phase assigns owners; the observations establish those values
at its boundary.

Every pair met the preparation helper's owned-slot preconditions: collecting,
enabled, fewer than the conservative 16-descriptor bound and no known native AA
unit. This is not a capacity guarantee for arbitrary queues or proof that a new
descriptor can safely participate in deferred GPU work.

The recorder ended with fault 0, pending 0, callbacks drained and its one hook
disabled. It did not write game data, request jitter, insert passes, issue private
GPU commands or use the vendor SDK. DLL/trampoline memory remains pinned until
process exit, as intended.

## Evidence and next implementation

Ignored root: `local/independent-aa-scheduler-20260926/v0.1.0/`.
`review-61960.json` accepted the CPU queue observation. The independent saved copy
`capture-61960.jsonl` is 15,590 bytes, SHA-256
`7bda19e369a3e87081970acebcdfaf0eca5ca843561154608da8d5db3275e980`.
`prearm-61960.json`, `arm-61960.json` and `capture-saved-61960.json` preserve the
process, arm and save receipts. Prelaunch manifest SHA-256:
`2f46624f439f48162ccf61a3202a246e64eacc7c5d245c13d40fcf31305224f7`.

Next, implement an owned descriptor/callback at the correct native phase/order,
with bounded admission, per-frame ownership and removal. Before inserting a
rendering pass, establish its unfiltered output path and deferred command/resource
lifetimes: a no-op filter can redirect ping-pong output without writing it. Reuse
the existing validated input conversions and DLAA recording only after the new
path's color/depth/motion states, freshness and frame alignment are established.
The pre-projection controller must own jitter independently of native AA options.
Actual failures remain latched to AA Off with an explicit reason, as agreed;
native TAA is not the replacement-failure path.

The user confirmed normal exit, and process absence was verified before rollback.
At 20:01:13 +03, the exact prior diagnostic DLL was restored (SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`), and only this
test's `.dll.output-dir` sidecar was removed. All four configuration hashes stayed
unchanged. `restored.json` records closure. Native AA and DLSS remain Off, Windowed
2560x1440/DX12 On. No next diagnostic is installed or armed.

The final archive verifies 351 files, including all 338 sealed prelaunch files
and 317 frozen sources. `final-manifest.json` SHA-256:
`8b9de11f111eb8a1978380223a708b6d9af819de25b44543e9439d131035a70d`.
Native source/test/tool files still match their frozen launch versions.
