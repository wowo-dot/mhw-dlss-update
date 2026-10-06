# Independent AA-Off DLAA: first queue rejection identified

2026-09-27, v0.8.1. **Diagnostic objective met; continuous run remains partial.**
The native queue grew from four nodes to five during ordinary walking/panning.
The exact four-node owned-pass plan rejected the next frame, latching fault 8.
The user confirmed DLAA stopped before any area change. No loading-transition
pass is claimed.

## Evidence

PID 58436, creation 134349300239740607. Host version 0.0.0.0 and SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Test DLL SHA-256
`3ea0e489466c9bb0428efa1d09a25cf31793c2459377f730545f7b7e9f83b2a0`.
Native AA Off, native DLSS Off, Windowed 2560x1440, DX12, bare offline profile.
Session `local/independent-aa-admission-20260926/v0.8.1/`.

All 403 prelaunch files and 374 frozen sources were verified before arming.
Startup capabilities and exact process/build ownership passed. Arm tick
40656265; first rejection tick 40783265, 127 seconds later. The user reported
clean still/moving output before noticing the stop. Baseline and walking
checkpoints preserve 3,029 and 9,570-frame progress respectively.

`capture-58436.jsonl`: 90,640 bytes, SHA-256
`408aa9e35a56f8b82de74dd1e0ca718bfad7ab55ae6a64e7db740d6dc15b7c5b`.
Frozen strict `review-58436.json` correctly rejects full acceptance
(`healthy stop`). Separate `observations-58436.json` checks the narrower
failure and completion evidence without relabeling the full run as accepted.

## Exact failure

The last admitted queue was frame/epoch 20501, bank 1. The rejected queue was
frame/epoch 20502, bank 0. Descriptor recycling succeeded: the prior insertion,
removal and callback completion all passed, with zero attached owned nodes.
The recorded step is `plan`, before owner rereads or any new queue mutation.

All four existing nodes are exactly unchanged, including descriptor identities,
units, vtables, flags and input/output owners. Manager, context, view, renderer,
rectangle, scratch/attachment owners and packet count are unchanged. Native
TAA and FXAA flags remain zero. The appended node is:

| Field | Value |
| --- | --- |
| Vtable RVA | 0x35372b0 |
| Phase | 15 |
| Order | 20000 |
| Flags | 0, 0, 0 |
| Input / output at collection | null / null |

This exceeds `plan_owned_pass`'s exact four-node contract. The extra node's role
and its eventual assigned endpoints need offline inspection. Its null endpoints
are collection-time values, not evidence that it consumes no color. Merely
allowing count five would also leave attachment/removal and callback endpoint
checks incorrect. A valid fix must preserve the native chain as the scheduler
sorts and assigns owners, then restore its original descriptor ordering.

The v0.8.0 failure had no queue snapshot, so this is a confirmed cause for
v0.8.1, not retrospective proof that v0.8.0 hit the identical extra pass.

## Completed work and closure

There were 9,674 successful SDK evaluations, native handoffs, published/consumed
markers and completed GPU signals. Source frames/epochs were 10828-20501.
No SDK or pass failure string was recorded. All 9,675 scoped jitter injections
were restored. All hooks disabled, descriptors detached and pending callbacks,
private recordings, GPU/Reset/submission waits were zero. The failure latch
retained two completed records and one projected record until process exit;
SDK/list references also remained pinned. Do not claim full resource release.

History reset on 612 evaluations. Reasons overlap: one initial reset, 180
projection-change reasons and 492 camera-distance reasons. These are separate
from queue admission failure and remain a quality-review item.

The user exited normally. The sealed helper checked process absence, restored
the exact previous DLL (`689bef7c...`) at 2026-09-27T00:05:07.652813+03:00 and
removed the matching output sidecar. All four configurations were unchanged.
No source/tests/tools changed during capture or closure. Next: identify the
extra pass and extend the queue model with explicit endpoint-preservation tests.
