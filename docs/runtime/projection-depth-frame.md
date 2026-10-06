# Depth-copy frame receipts v0.8.1

This diagnostic adds original packet receipts to the accepted
[v0.8.0 readback](projection-depth-readback.md). Its purpose is to resolve the
[copy-request timing gap](../research/depth-copy-frame-map.md). It retains the
one-copy, one-private-Signal budget, two-second capture bound, resource/state
checks, private-fence completion and successful recording Reset before Map.
No jitter is injected. There is no provider integration or change to game settings.

## Hooks, ownership and admission

The pinned executable version and hashes, producer/consumer ABI and packet
layout are documented in the [static map](../research/depth-copy-frame-map.md).
Two native hooks at RVA `0x23CD170` and `0x259DF30` bring the total to 41:
14 native and 27 API hooks. There are 45 distinct byte-gated spans: 39 core
checks and seven startup checks, with one overlapping span. The producer's
complete body is a new startup gate; the consumer's body was already gated.
Startup also verifies trampoline reservation reach for both new targets.

Only depth requests from primary context renderer+`0x710` are admitted. Empty
rectangle and self-copy no-ops forward unchanged. Before and after the producer,
the recorder requires stable renderer frame/epoch/bank/async values at offsets
`0x1F210`, `0x1F208`, `0x1F200`, `0x93`. It checks the queue's bounded append,
last packet pointer, owners and typed packet contents twice at return. Native
return values, stack arguments, LastError and exception propagation are preserved.

The consumer resolves packet owners to actual resource pointers and associates
the latest completed receipt for that packet address. A thread-local scope
matches its nested native CopyResource to the exact command list and resources.
No matching receipt means no added readback. A stamp, object or invariant failure
stops the recorder; original calls continue. Packet fields are checked again at
consumer return. The reviewer rejects overlapping reuse or repeated consumption
of the selected generation.

Both channels are bounded to 256 entries. Return-published records are immutable;
pending calls prevent teardown. These typed observations add no COM references,
resource writes, GPU commands or renderer waits. Existing readback ownership
retains its private resources until process exit, including on successful Map.
The original journal's references still release after hook drain.

## Evidence contract

Header version `0.8.1` retains experiment `projection-depth-readback`. Native
companion schema 4 gains an explicit `frame_packets` object (schema 1); ready
requires `frame_tagging=true`, capacity 256 and 17 provenance targets. The core
and provenance channels together install 41 hooks. Older capture versions retain
their original review contracts.

Full review requires the old resource, barrier, submission, fence, recording
retirement and pixel checks, plus an unambiguous producer/consumer/copy chain.
It reports original request frame/epoch/bank, packet orders and deltas from the
selected projection draw. It explicitly leaves
`depth_pixel_producer_frame_verified=false`. This is a request-frame receipt,
not proof that all texture pixels belong to that frame or that raster jitter works.

Owned GPU fixtures cover receipt matching, recycled packet addresses, changing
stamps/data, missing receipts, capacity, consumer exceptions, and native no-ops.
Mutation tests exercise the complete reviewer using a clearly labelled synthetic
composition of independent owned journals. That composition is not game evidence.
Build/test counts and exact identities are sealed in the local preparation receipt.
No v0.8.1 game result is claimed at preparation time. The subsequent
[run 43440](../research/depth-copy-frame-result.md) passed the unchanged review,
established the original copy-request stamp and completed normal rollback.

## Runtime and rollback

Use MO2 profile `barebonesdlss` and the existing
`Monster Hunter: World - DLSS 1.2.110 test` entry. Keep DX12 On, DLSS Off and
TAA On. Load one save, keep the camera still, and reply from the phone. Arm only
the matching PID/creation ready log. Review the journal, readback record, frame
receipts and raw pixel companion together, then exit normally with DLSS Off.

Local evidence directory: `local/projection-binding-20260923/v0.8.1/`.
Deployment preserves the exact prior MO2 slot DLL and output sidecar, archives
the tested sources, and checks four configuration identities. When the game is
closed, `deploy.ps1 -Action Restore` restores the archived DLL and restores or
removes the sidecar. The deployment, prelaunch and eventual restoration receipts
are authoritative for the slot's current state. Original game assets are untouched.
