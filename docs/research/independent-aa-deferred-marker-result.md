# Independent-AA deferred marker result

2026-09-26: **v0.3.0 passed eight live deferred requests with native AA Off**.
The owned post callback now reaches the native DX12 command-recording consumer
at phase 15/order 2. Each request retained its original source identity and held
the same color, depth and motion-candidate resources through CPU consumption.
The native parameter table stayed unchanged. No DLAA evaluation, jitter or
private GPU command was recorded in this diagnostic.

[ABI, code gates, ownership and offline verification](../runtime/independent-aa-deferred-marker.md).
Host version 0.0.0.0, executable SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
PID 55784, creation FILETIME 134349183990087877. DLL SHA-256
`b7fd6e3ff858c57dab7ae928107493fb3b87bdc735eb07fcc510f86fc321c905`.

## Observations

The user reported a loaded save. All 363 prelaunch files, 333 frozen sources,
installed DLL/sidecar, previous DLL backup and four configuration hashes were
verified before arming. The startup process generation matched the running game.
D3D12 discovery: adapter LUID 98931:0, device 354308960, swap-chain format 87,
Windowed 2560x1440. Native AA and DLSS stayed Off.

The frozen reviewer accepted all eight consecutive source frames 14132-14139.
The owned callback ran on thread 58224; native consumption ran on thread 58816.
Each callback appeared after 12 native packets in the phase and added one empty
packet. The entire phase appended 32 entries, including our marker. The review
field `native_packet_deltas` is a legacy name for this total, not 32 native-only
entries. Post descriptors followed 4 -> 5 -> 4 and original ordering/endpoints
were restored exactly in every frame.

All eight packet addresses and generations were distinct. Each was consumed
exactly once with sort key `0x78000002` (phase 15/order 2). Frontend key fields
were restored and the coalescing cache stayed null. Inherited +154 was
`0xfffffe2f`, confirming that the owned callback needs its own scoped packet key.
Published entry addresses were reused across later frames; they are unsuitable
as deferred identities. The unique packet addresses avoided that ambiguity.

At consumption, the global renderer frame was **source frame +1** for all eight
requests. The request continued to identify its original frame/epoch/bank;
resource identity checks did not relabel it from the advancing global counter.
The consumer used one DIRECT list (356294880), verified on the discovered device,
through executor 2516748960 / recording 2516749072. Parameter-table comparison,
current-packet identity, unchanged list and drained native barrier vector passed.

Actual resource descriptions were stable across all requests:

| Candidate | Resource | Format | Extent | Flags |
| --- | --- | --- | --- | --- |
| In-place color | 5229852704 | R11G11B10_FLOAT (26) | 2560x1440 | RT + UAV (5) |
| Scene depth | 2803918720 | R32_FLOAT (41) | 2560x1440 | RT (1) |
| Packed motion | 4646638000 | R32_UINT (42) | 2560x1440 | RT (1) |

All are distinct Texture2D resources, one mip/slice/sample, on the same device.
Color owner was 2802131264, depth owner 4879309792, motion owner 4879321792.
Double-read owner checks matched before publication and around consumption.
Diagnostic COM pins remained held until callbacks drained, then all were released.
These descriptions and pointer checks do not establish pixel freshness, correct
resource states, motion decoding, depth alignment or GPU submission completion.

## Closure and evidence

Final status: eight samples and visits, eight complete requests, one consumption
per request, no fault or pending callback, all descriptors detached and both
hooks disabled. Capture was saved and independently reviewed before requesting
exit. The user confirmed normal closure; process absence was verified. Exact
rollback completed at **20:49:07.610803 +03**: previous DLL restored, output-dir
sidecar removed and all four configurations unchanged. No next build is installed
or armed. Native source/tests/tools remained identical to the launch snapshot.

Ignored evidence root: `local/independent-aa-deferred-20260926/v0.3.0/`.
`capture-55784.jsonl`: 52,881 bytes, SHA-256
`459bf5b19d4934a05dbb0e6bdf1870389997c133b195e2d6813c221068854e84`.
`review-55784.json` records acceptance. Prelaunch manifest SHA-256
`d828b4cd8ddc68b7b6d77eb21bfa1bef60d72e0286e39907b1520adef57da648`.
Previous/restored DLL SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`.
Prearm, arm, capture-save and restoration receipts are preserved alongside the
frozen source, builds, six native tests, 27 Python checks and private-memory
enqueue/consumer replays. Original game executable/assets were untouched.

Final archive: 375 files verified, all 363 prelaunch files and 333 frozen sources
reverified. Manifest SHA-256
`344c824248145503546d35b60da9406ac81fae7e95a49d6c6dd51ae233d47690`.

## Next integration

Do not repeat a neutral marker-only launch. The independent backend entry is
demonstrated in this scene. Next, establish the actual color/depth/motion states
and frame ordering at this entry, and track command-list recording generations,
queue submission completion and retirement for private GPU work. Use that to
move the existing conversions/readback and DLAA evaluation onto this route.
The independent pre-projection jitter/controller, native-AA suppression for
other native selections, AA-Off failure behavior and transition validation remain
integration work. Historical TAA-backed continuous DLAA is separate evidence;
this run does not claim independent live DLAA is complete.
