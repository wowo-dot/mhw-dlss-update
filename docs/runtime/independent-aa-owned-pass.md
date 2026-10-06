# Independent AA: owned in-place pass

2026-09-26, v0.2.0. This bounded test inserts a mod-owned callback at the exact
native TAA position, phase 15/order 2, with native TAA and FXAA Off. It exercises
the unfiltered Off behavior: keep current color untouched and preserve downstream
native bindings. It does not evaluate DLAA, inject jitter or issue GPU commands.

The [eight-frame live run](../research/independent-aa-owned-pass-result.md) passed
callback/graph/resource-description review and exact restoration. Deferred GPU
input validity and independent DLAA remain unverified.

## Why an in-place descriptor

The [prior live control](../research/independent-aa-queue-boundary-result.md)
established the AA-Off post queue. Static TAA Execute at RVA 0x2392150 requests
phase 15/order 2 through 0x24c9240. The reviewed native manager at 0x24f1fc0 treats
an interior descriptor with byte +21=1 as an in-place operation: input +10=0,
output +18=current scratch owner, no ping-pong advance. It still invokes a
non-null unit's vtable+60 if +20=0. The existing order-1024 unit uses this shape.

The new descriptor is inserted between order 0 and order 100. Its callback sees
the current scratch color at the native AA position. Off mode deliberately does
not write this color or generate packets. Every existing descriptor retains its
same input/output assignment. An ordinary no-op filter would advance ping-pong
without producing output and is explicitly rejected by the tests.

This establishes scheduling and a neutral Off route, not the deferred DLAA
consumer. Future reconstruction must preserve current source pixels before an
in-place handoff, prove input states/frame alignment and keep resources alive
through command-list retirement. This test makes no GPU freshness or completion
claim. Existing native-TAA-based DLAA recording is not armed by this DLL.

## Hook, ABI and lifetime

Tested host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded comparison SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
These were verified before the new static mapping. Research RVAs are version
specific and guarded by full code hashes before a hook is enabled.

| Span | RVA | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Native phase/hook | 0x24f1fc0 | 835 | `1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190` |
| Camera caller | 0x228e770 | 528 | `33ae7df95058ac2e1ce8d920ff89c62d664068a975931bb10afb95ea88f00b97` |
| Native queue sort | 0x24f1620 | 808 | `199d3cb4f03566dcc21aaf72c55e562f568595e3e3954f5987890bdb2c33eaa7` |

The one hook retains the [v0.1.0 ABI](independent-aa-queue-boundary.md): Windows
x64 RCX=manager, RDX=context, R8=view, only caller return RVA 0x228e92d admitted.
Native arguments, returns and exceptions are forwarded. The owned callback uses
Windows x64 RCX=owned unit, RDX=context, void return (ignored by the native loop).
Its static vtable has only +60 populated, and it is never registered as a game
layer. `/guard:cf` is enabled. All eight descriptors/units and hook trampolines
remain in the pinned DLL for the entire process lifetime; no slot is reused.

Insertion requires the exact four-node queue, native owner chain, primary view,
2560x1440 rectangle and no native AA seen in v0.1.0. Changed layouts are rejected
before mutation. Two equal queue and resource-owner snapshots precede admission.
The runtime appends one pointer in a previously unused slot and publishes count
4 -> 5 with an interlocked comparison. It never calls the native unbounded
enqueue helper. This depends on the native primary phase being serialized after
layer collection; the busy guard rejects nested hooks. This single-scene test is
not proof of that assumption across every engine lifecycle.

Inside the callback, two matching snapshots verify sorted order 0/2/100/1024/end,
the same originating frame and unchanged native endpoints. It verifies the owner
and queries its D3D12 texture description under temporary COM pins: same device,
2560x1440 R11G11B10_FLOAT, one mip/slice/sample. Pins release after the synchronous
query; no owned GPU work or deferred resource references exist. The callback
adds no context packets and rechecks the owner/queue after querying.

After the native phase, the hook verifies the five-node graph, then restores the
exact original four pointer slots/count and the overwritten unused tail value.
It does this even when the native phase raises an exception. Removal requires
the expected descriptor set/count; a foreign mutation is not blindly overwritten.
Such a collision marks failure and retains all descriptor storage. Normal capture
closure requires every descriptor removed, no pending callbacks and hook disabled.

## Verification

Release `/W4 /WX` build passed. Five targeted CTests passed: AA policy, queue
reader/admission, 1,000 ABI forwards/native unwind, owned in-place graph and
actual queue mutation scope. The new graph test covers all 24 original queue
permutations, changed endpoints, native AA, foreign units, aliases and bad order.
The mutation test checks exact restoration after sorting, the unused tail,
native exception cleanup and refusal to overwrite foreign changes.

The graph test caught an output-indexing defect in the initial implementation;
it was fixed and all five tests rerun successfully before deployment. Twenty-one
Python tests passed, including six new owned-pass arm/review cases. The reviewer
reconstructs the graph from recorded values and rejects stale identities,
incomplete cleanup, changed native endpoints, double callbacks, wrong placement
or resource descriptions. This is stronger than accepting runtime booleans alone.

## Local test and rollback

Evidence root: ignored `local/independent-aa-owned-20260926/v0.2.0/`.
Use the usual bare offline MO2 profile/entry, native **DLSS Off and AA Off**,
Windowed 2560x1440, DX12 On. Load a save and leave it open. No camera movement or
setting toggle is required. The completed run verified this insertion in one
loaded scene; other layouts and lifecycle transitions remain outside its scope.

After verifying deployment/configuration hashes and process generation, run
frozen `source/tools/arm_aa_owned.py` with the log, deployed DLL hash and exclusive
receipt path. The log remains named `aa-boundary-PID-CREATION.jsonl`; arm events
use the distinct `Local\MhwAaOwned-v1-PID-CREATION-` prefix. Do not use the old
queue-only or continuous-DLAA arm tools. Readiness lasts 30 minutes; capture lasts
up to five seconds and at most eight primary phases. Review with frozen
`source/tools/review_aa_owned.py --log LOG --dll-sha256 HASH --output REVIEW`.

The exact DLL destination, prior backup, created output sidecar and all four
configuration hashes are recorded in `deployment.json`. After normal game exit,
`restore.py` verifies slot/backup identities, restores the previous diagnostic DLL,
and removes only this test's output sidecar. It never overwrites changed graphics
or profile settings. Original game files are untouched.
