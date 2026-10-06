# Independent-AA GPU trace: completion confirmed, local motion state unknown

2026-09-26, v0.4.0, PID 47780 / creation 134349200024568753. The eight
independent requests completed their CPU and GPU submission lifecycles. Full
qualification was correctly rejected because motion had no transition before
the marker in that particular command-list recording. The offline queue replay
explains this gap without altering the captured log or the sealed reviewer.

The user closed normally. Exact prior diagnostic DLL restored at
21:15:28.919944 +03; test sidecar removed; all four configurations unchanged.
No next build installed or armed. Keep native AA Off, DLSS Off, Windowed
2560x1440, DX12 On and the bare offline profile.

## Verified observations

Executable version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
DLL SHA-256 `285a138fe2501baf76fd3195c076695564497e6ff8d63ece38a57d164d8f1438`.
D3D12 device 353608912, adapter LUID 98931:0, swap-chain format 87,
2560x1440. [Runtime hook/ownership contract](../runtime/independent-aa-gpu-trace.md).

- Eight unique packets, source frames 11189-11196, consumed exactly once on
  thread 44624. Renderer counter at consumption was always source +1.
- Native parameters and owner identities unchanged; exact post-queue restoration;
  native AA option bytes remained Off. All eight descriptors detached.
- Each request has an observed Reset -> marker -> Close -> Execute chain,
  its own successful Signal, completed fence value 1 and a later recording Reset.
- All eight GPU completions were observed at tick 30636437. The journal started
  at 30636078 and closed at 30636593: 515 ms total observation, 3436 API events,
  one queue, 726 selected barrier calls containing 924 full-image transitions.
- Journal/GPU-trace faults zero, pending callbacks zero, all six hooks disabled.
  All 536 journal pins, three watch pins and per-request pins were released.
  Deferred fault 9 and outer fault 6 report failed state qualification, not a
  failed GPU Signal or callback drain.

The capture contains no private GPU image commands, SDK evaluation or jitter.
Existing DLAA is not running in this test.

## State reconstruction and the missing motion transition

| Input | Pinned resource | Format | Same-recording state at all eight markers |
| --- | --- | --- | --- |
| Color | 4896491808 | R11G11B10_FLOAT, 26 | RENDER_TARGET, 4 |
| Depth | 4896406128 | R32_FLOAT, 41 | PIXEL_SHADER_RESOURCE, 128 |
| Packed motion | 4896420816 | R32_UINT, 42 | Unknown: zero prior transitions in this recording |

Color has one prior full-image transition and depth has two, consistently in
all eight recordings. The observed motion texture is active: the complete
capture contains 396 of its explicit transitions across three command lists.
It is not absent merely because its local per-recording state was unknown.

Each sampled ExecuteCommandLists submits `[355474544, 355462672]` in that order.
Our marker is on the second list. The first list's final motion transition is
NON_PIXEL_SHADER_RESOURCE (64) -> RENDER_TARGET (4). The second list has no
motion transition before our marker; its later barrier transitions motion
4 -> 128, alongside color 4 -> 128.

The first sample illustrates the exact links:

| Observation | Journal order / identity |
| --- | --- |
| Preceding list Reset | 3373, list 355474544 |
| Its final motion transition | 3407, 64 -> 4 |
| Marker list Reset | 3369, list 355462672 |
| Owned marker | 3411 |
| Later color/motion 4 -> 128 barrier | 3413 |
| Shared Execute batch | 3423, preceding list index 0 / marker list index 1 |

The offline replay resolves each submitted list to its observed Reset/Close
generation, then processes barriers in queue submission and list-array order.
It does not use CPU recording order across lists as GPU order. All 924 captured
explicit transitions replay consistently; all eight marker positions inherit
the explicit-chain states `[4,128,4]`, with motion provenance in the immediately
preceding list of the same submitted batch.

This is an inference from the observed explicit transition chain, distinct from
the runtime verifier's local unknown state. It does not establish pixel
freshness, absence of unobserved operations or cross-queue writers. It is not
authorization to read motion from an arbitrary future recording. The frozen
full-pass reviewer still rejects this capture; the partial reviewer records
`accepted:false`, `gpu_completion_verified:true`, `gpu_states_verified:false`.

## Next implementation

Resource state can cross a command-list boundary even though recording identity
cannot. The current local tracker deliberately resets to unknown and therefore
cannot qualify this motion input by itself. Preserve recording generations,
exact submission-list order and the producer's final state when adding a
resource-state chain. Do not seed from a previous frame or the most recently
recorded list on another CPU thread. A record-time decision must be supported
by the native scheduled-state contract or equivalent provenance available at
that point; a later offline reconstruction alone cannot authorize it.

Use this captured ordering to build and test that qualification, then connect
input conversion/readback and existing DLAA work to the independent entry with
scoped transitions restoring native states. The same neutral eight-packet run
need not be repeated unchanged. Independent jitter/controller, suppression of
other native AA selections and AA-Off failure recovery remain integration work.

## Evidence and rollback

Ignored archive: `local/independent-aa-gpu-20260926/v0.4.0/`.
376 prelaunch files and 341 frozen sources; prelaunch SHA-256
`6b294fa4afd7baa91b8474bb3548f70474d82b8f897452cac5a82df6329e2fd0`.
Saved capture: 855,513 bytes, SHA-256
`a524b181b510236c8358da1c5e5e7ae6268abe2795ef8ce5794d22d80103d783`.
`review_partial.py` and `review-partial-47780.json` retain the independent
lifecycle checks, exact rejection and ordered explicit-barrier replay.

Arm, prearm, save and restoration receipts are retained. No native source,
tests or tools changed during this capture/closure. Previous slot restored to
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`.
No game executable or original assets were modified.
