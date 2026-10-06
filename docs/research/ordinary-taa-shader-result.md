# Ordinary TAA shader capture: run 4376

Accepted 2026-09-25, v1.1.0. The frozen reviewer accepted three CPU requests
at frames/epochs **14994, 14995, 14996**, banks 0/1/0, async 1, technique
23086. All shader metadata and bytes were stable around the original producer.
Each request had 12 owners and 12 binding instructions. The single hook was
disabled, pending callbacks and faults were zero, and all 21 resource pins
released. Normal game exit was confirmed; exact previous DLL/sidecar state
was restored and all four configuration hashes were unchanged.

## Identity and evidence

- Executable version `0.0.0.0`, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Observer SHA-256
  `d0bf80ddba47c4f685e1b446467a7f6f9386bf62f65e1795d48a425c12c5c582`.
- Capture SHA-256
  `40f21809fd1f3d10ea94785b6d93842537699450be5616aef49b8b9b323f779c`.
- Shader: **15,492 bytes**, SM5.0 compute, 16×16×1 thread group, SHA-256
  `2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`.
- Private evidence directory: `local/temporal-shaders-20260925/v1.1.0/`.
  It contains the bytecode, disassembly, log, frozen review, analysis, source
  archive, deployment/rollback receipts and final manifest. Proprietary shader
  bytes and disassembly remain ignored and must not be redistributed.

The shader's embedded resource declarations independently agree with every
named texture association in all three captured binding recipes:

| Register | Input/output | Shader declaration |
| --- | --- | --- |
| t0 | tBaseMap | float4 texture |
| t1 | tBlendMap | float4 texture |
| t2 | tDepthMap | float texture |
| t3 | tGBuffer1 | float4 texture |
| t4 | tGBuffer4 | uint texture |
| t5 | tPrevGBufferID | unorm texture |
| u0 | rwOutput | float3 UAV |

The six SRV and one UAV ordinals are therefore supported by both the captured
engine metadata and this shader's declarations. CPU descriptor addresses were
recorded; complete typed view descriptions were not. Resource descriptions
agree with the preceding run, including the 1×1 named depth fallback.

## Packed motion decoding established from the shader

For the R32_UINT value `p` loaded from tGBuffer4, the shader's shared-memory
repacking and subsequent unpacking give this component order:

```text
dx = binary16_to_float(p >> 16)
dy = binary16_to_float(p & 0x0000fffe)
```

The lowest bit of the low half is explicitly discarded. Its producer-side
meaning is not established by this capture. The high half becomes X and
the masked low half becomes Y; overlooking the intermediate repacking would
reverse the components.

When `bVelocityBase` is true and either decoded component is nonzero, the
shader subtracts this pair from current normalized device coordinates to
obtain the previous position. Before history-coordinate bounds/clamping:

```text
previous_ndc = current_ndc - (dx, dy)
previous_uv - current_uv = (-dx / 2, +dy / 2)
```

For the corresponding full-resolution pixel grid, multiply those UV deltas
by width and height. This establishes the consumer's coordinate convention;
it does not yet validate real vector magnitudes, producer coverage, jitter
inclusion, or a vendor SDK's required motion convention.

If that branch is false, the shader samples tDepthMap and transforms the
current position using `fViewProjIViewProjPF` at CBViewProjection byte offset
736 (cb0 registers 46–49), then divides by W. The live constant-buffer bytes
and per-pixel branch outcomes were not captured. The layer's observed velocity
byte is not treated as proof of the submitted constant value. In particular,
the 1×1 fallback binding alone does not establish its pixel value or whether
the depth branch was useful in this scene.

tGBuffer1 alpha is converted with `uint(alpha * 255) >> 3` and compared with
the previous-ID texture to reject incompatible history in the velocity path.
The exact interpretation of those identifiers remains a producer-side question.

## Verification and remaining work

The unchanged archived v1.1.0 reviewer reaccepted the saved shader and capture.
An offline analysis also checked all seven declared texture bindings against
all three recipes. The finite binary16 repacking model passed 63,488 exact
round trips and seven synthetic component/sign cases. These are arithmetic
checks, not measurements of GPU texture contents.

Next: obtain bounded, frame-associated GBuffer4 pixel readbacks and compare
the decoded field with a stationary control and deliberate camera movement.
Resolve typed views, zero-vector/depth fallback coverage, and jitter inclusion
before using these inputs for reconstruction. The already validated separate
scene-depth path remains available. GPU consumption, motion contents and
DLAA readiness stay false; no modern vendor SDK call or output replacement
was made in this experiment.
