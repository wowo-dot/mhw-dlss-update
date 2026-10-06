# Ordinary TAA native recording result — v1.2.0 run 53044

Accepted and closed on 2026-09-25. Three consecutive CPU requests were matched
to their native D3D12 Dispatch recordings, including the local transitions
before each Dispatch. This closes the recording-site/state question for the
motion buffer in this run. No observer GPU commands, pixel copies, jitter
writes or vendor SDK calls were made.

## Evidence and closure

- Host version `0.0.0.0`, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Probe SHA-256
  `39819d803a0cd40dd3e618bb3c56aa7bcfe0f756cb91e3f9681890cf775e4ea6`.
- PID 53044, creation identity `134347681612000289`.
- Private evidence: `local/temporal-consumer-20260925/v1.2.0/`.
- Log `inputs-53044-134347681612000289.jsonl`, SHA-256
  `99281f70a38d0d9a2bbe522cc3e444d502eb0b0343c7ef7398369966f48c42d4`.
- The unchanged archived reviewer accepts the capture; separate analysis
  verifies all 123 prelaunch files and names each transition independently
  from the captured resource identities.

All four hooks disabled, both observer fault/pending counters were zero,
21 resource references and three command-list references were released.
The user confirmed normal exit. The exact prior DLL was restored (SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`),
the temporary output sidecar was removed, and all four graphics/profile/mode
configuration hashes stayed unchanged. All 101 archived sources verify.
The previous diagnostic occupies the slot; no new capture is armed.

## Confirmed recording observations

Frames/epochs are 13801, 13802 and 13803, banks 1/0/1, async flag 1.
Each CPU request has groups 160×90×1 for the 2560×1440 rectangle.
Producer thread 27108 publishes the three distinct packet receipts; consumer
thread 54104 matches their packet, shader recipe, backend and native list.
Each records exactly one Dispatch, from return RVA `259CFCA`, on a DIRECT
list belonging to the same device. All three observations use the same
canonical command-list identity. This does **not** establish that they use
the same recording generation: Reset and submission were outside this probe.

Each consumer has one stable ResourceBarrier call with five full-resource,
non-split transitions, returning from RVA `259CF9E` before Dispatch:

| Named input | Before | After |
| --- | --- | --- |
| tBaseMap | RENDER_TARGET (`0x4`) | NON_PIXEL_SHADER_RESOURCE (`0x40`) |
| tBlendMap | COPY_SOURCE (`0x800`) | NON_PIXEL_SHADER_RESOURCE (`0x40`) |
| tGBuffer4 | RENDER_TARGET (`0x4`) | PIXEL_SHADER_RESOURCE + NON_PIXEL_SHADER_RESOURCE (`0xC0`) |
| tPrevGBufferID | UNORDERED_ACCESS (`0x8`) | NON_PIXEL_SHADER_RESOURCE (`0x40`) |
| rwOutput | NON_PIXEL_SHADER_RESOURCE (`0x40`) | UNORDERED_ACCESS (`0x8`) |

No local transition establishes tDepthMap or tGBuffer1 state; their states
remain unknown. There are no state conflicts in these observations.
These are recorded API arguments and scope-local state deductions, not proof
of execution or a complete cross-list state history.

GBuffer4 remains a 2560×1440 R32_UINT texture with one mip, slice and sample.
The shader artifact is 15,492 bytes and byte-identical to run 4376, SHA-256
`2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`.
The [established static decode](ordinary-taa-shader-result.md) therefore
applies to the captured consumer recipe; no new shader interpretation is
needed. Actual motion pixels and per-pixel branch outcomes remain unknown.

## Next experiment

Use this matched recording site to obtain a bounded GBuffer4 readback while
preserving the game's expected state. First associate it with an observed
successful Reset/allocator generation, Close and ExecuteCommandLists on the
actual queue. Carry the original producer frame through that association.
Same list pointer across frames cannot substitute for generation tracking.

A copy would require an explicit transition to COPY_SOURCE and restoration
to the observed `0xC0` state, with all invariants checked before recording any
observer work. Reuse the validated depth probe's queue/fence and retirement
approach where applicable, while validating the R32_UINT format and footprint
separately. Preserve issued GPU resources through completion and uncertain
resubmission; map only after private completion and validated retirement.
Do not reuse the depth-only R32_TYPELESS/DEPTH_STENCIL admission unchanged.

Then compare decoded pixels under stationary and camera-motion controls,
and resolve jitter inclusion, typed views and producer coverage. This run
does not establish submission, completion, readback eligibility, motion
contents, or DLAA readiness.
