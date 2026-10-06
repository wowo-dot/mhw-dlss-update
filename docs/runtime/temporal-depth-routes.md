# Native depth copies alongside TAA pairs

Closed 2026-09-25: [run 45404](../research/ordinary-taa-depth-routes-result.md).
The recorder completed, but the frozen full reviewer rejects unselected tail
generations. A supplemental audit verifies nine copies in the three selected
frames. Exact previous DLL and sidecar state restored, configurations unchanged.
The preparation/procedure below describes this completed diagnostic.

Prepared 2026-09-25, recorder **1.8.0**, runtime result pending.
This extends the [v1.7.0 candidate observation](../research/ordinary-taa-scene-depth-result.md)
to ask whether the exact selected view+628/container+A0 candidate receives a
native copy from original scene depth in each selected color/motion frame.

## Hooks and executable identity

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decoded-image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Preparation checks both identities and 51 complete code spans. Runtime checks
the host identity and live spans before hook enablement. These RVAs are
version-specific, not portable signatures.

Three hooks join the seven [existing hooks](temporal-scene-depth.md):

| Target | Windows x64 arguments | Validation |
| --- | --- | --- |
| Producer RVA 23CD170 | RCX frontend context, RDX four-u32 rectangle, R8 two-u32 destination offset, R9 destination owner, stack arg 0 kind | 1711 bytes, SHA-256 `8103c87804f325a15bbeb3529f3b575cc14e3bae986156d53e1b796fbbec6d31` |
| Consumer RVA 259DF30 | RCX executor, RDX backend context, R8 packet | 681 bytes, SHA-256 `e84ae8742c4a0ef334692595bb89c04c8be80306d4416c20304566cc1e5dd85a` |
| ID3D12GraphicsCommandList::CopyResource, slot 17 | RCX list, RDX destination resource, R8 source resource | System API discovered on an owned same-device DIRECT list; native return address must be image+259E100 |

The mapped producer and packet layout are documented in
[depth-copy frame mapping](../research/depth-copy-frame-map.md). Both native
entry thunks preserve the established integer/vector register frame and stack
arguments; original calls forward exactly once, with bookkeeping in SEH finally.
CopyResource and ResourceBarrier always forward the original operation.

## Association and lifetime

Only kind-8 copies targeting the selected primary view's candidate are admitted
after the 150 ms warmup. The original source is read through frontend
context+50/attachment+58 at production time. Record frame/epoch/bank/async,
view, source and destination owners/backings, rectangle, queue publication and
immutable opcode-10 packet fields. Publication must be stable across a second
read. Consumer matching uses the newest completed receipt at the packet address;
an in-progress publication at lookup is unqualified, and packet/owner fields
must remain equal through consumption. Frontend frame counters are never read
at the delayed consumer to invent a frame association.

The consumer validates its backend, executor recording, command list and
same-device COM identities. Source, destination and list each acquire one
metadata reference, released after hooks disable and calls drain. Engine owner
and packet storage remain game-owned; bounded rereads enforce the observed
ownership assumptions. This does not grant ownership of the engine structures.

Existing ResourceBarrier interception records at most eight calls / 32 barriers
inside each copy consumer. The shared submission journal adds read-only kind 4
markers around these consumers. Replay resolves Reset -> marker -> Close ->
Execute -> next Reset, rejects overlapping/repeated generations, and reports
the queue and batch-array position relative to the selected TAA dispatch.
No extra command lists, barriers, copies or Signals are submitted for depth.

TAA selection waits for a completed copy request with the same original stamp,
then retains three consecutive paired frames. The reviewer reports every
observed matching copy, including repeated writes to one destination. It does
not pick the latest CPU record as the final GPU write. List-array position in
one Execute call does not prove whole-list serialization or resource readiness.
Missing local transition evidence is unknown; split/subresource/alias conflicts
cannot qualify a copy state. Expected source is full-size R32_TYPELESS/DS,
destination R32_FLOAT/RT, one mip/slice/sample, with source COPY_SOURCE and
destination COPY_DEST observed at CopyResource.

## Bounds, validation and limits

Ten hooks, two-second window, 150 ms warmup, at most 64 copy requests and 64
consumers. Submission bounds remain 4096 events and 8192 submitted pointers.
The existing three color/motion pairs retain six readbacks, 18 observer GPU
commands, three private Signals and a 96 MiB bound. Zero projection writes,
depth readbacks, descriptor writes or SDK calls.

108 native checks pass, including six new copy-route cases: single and repeated
copies, wrong native callsite, producer exception, changed frame and changed
packet. Pure stamp/packet tests cover missing reads, overflow, alias and invalid
rectangles. The Python suite passes 49 tests (nine jitter-only skips), including
packet/marker reuse, barrier/state damage and submission-generation ambiguity.
Historical captures are reviewed with the current reviewer during preparation.

An accepted result establishes observed native copy association and states.
It does not establish exhaustive producer coverage, final writer, depth pixel
contents, color/depth alignment, typed views, or sustained DLAA. Those flags
remain false even when copies precede TAA in the submission journal.

## Local test and rollback

Fresh evidence: `local/temporal-depth-routes-20260925/v1.8.0/`.
Preparation verifies the closed v1.7.0 archive, code gates, tests, exact previous
DLL and four configurations. Installation preserves the exact MO2 slot and
output sidecar, freezes source and reviewer files, and seals a prelaunch manifest.
Installation and prelaunch receipts are authoritative about deployment status.

Use the existing bare-profile MO2 test entry. DLSS Off, TAA On, DX12 On, current
resolution. Load the offline/private save and hold the camera still; posing is
optional. Reply from the phone when loaded. Arm only after checking the current
process's ready log and sealed deployment. Following capture, exit normally
with DLSS Off. With the game closed, `deploy.ps1 -Action Restore` restores the
previous DLL and prior sidecar state. Original game assets and settings are
unchanged.
