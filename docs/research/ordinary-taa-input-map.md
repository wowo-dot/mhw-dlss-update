# Ordinary TAA input requests

2026-09-25. Following the accepted [three-direction depth sequence](depth-sequence-result.md),
the next observer targets ordinary TAA while DLSS remains Off. The optional
DLSS producer returns before generating dense motion when its wrapper is absent.
Its old full-resolution `R32G32_FLOAT` motion texture is therefore not an
established input for the current Off-state experiment.

The original executable is version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The decoded analysis image is
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both were rehashed before this mapping. These are version-specific research
RVAs, not portable signatures. Private instruction exports remain under
ignored `local/temporal-bindings-20260925/`.

## Static observations

TAA2 Execute at `+0x2392150` selects a `CSTemporalAA2` variant, binds named
textures through `+0x23D0130`, and calls the compute request producer at
`+0x23C9980`; this call returns to `+0x2392F31`. It precedes the optional
DLSS producer at `+0x23913F0` and the optional sharpening dispatch. The
observer admits only the ordinary TAA call site.

| Binding | Source in this function | Cached binding ID RVA |
| --- | --- | --- |
| `tBaseMap` | TAA2 layer `+0x150`, current scene-color owner | `+0x4BE60E0` |
| `tBlendMap` | Layer `+0x168 + 8*((frame-1)%3)` | `+0x4BE60E4` |
| `tDepthMap` | Selected depth helper or fallback texture | `+0x4BE60E8` |
| `tGBuffer1` | View `+0x620` container, slot `+0x60` | `+0x4BE60EC` |
| `rwOutput` | Layer `+0x168 + 8*(frame%3)` | `+0x4BE60F0` |
| `tGBuffer4` | Same container, slot `+0x78` | `+0x4BE60F4` |
| `tPrevGBufferID` | Same container `+0xC0` | `+0x4BE60F8` |

The binding setter writes the owner to context `+0x710 + 8*(id&0xFFF)`.
The G-buffer getter is eight bytes and returns container `+0x58 + 8*index`.
The shader names identify binding roles, not texture contents: neither G-buffer
is yet qualified as modern motion vectors. Velocity packing, direction, scale,
jitter inclusion, exposure and valid object/background coverage remain open.

The compute producer accepts Windows x64 arguments RCX=context, EDX/R8D/R9D=
group counts and two stack arguments (indirect pointer and byte flag).
At the selected, fully reviewed call site, RBX retains the TAA2 layer.
The observer wrapper records RBX without changing it and forwards all native
argument/return registers through the existing unwind-described wrapper.
Only the low byte of the sixth argument is meaningful; upper stack bytes can
contain unrelated data and must be preserved, not interpreted as a boolean.

The producer allocates a 48-byte packet: low-five-bit opcode 6, state pointer
at `+0x10`, XYZ group counts at `+0x18/+0x1C/+0x20`, byte mode at `+0x24`,
and indirect pointer at `+0x28`. It appends one 16-byte queue entry through
context `+0x2B78`/`+0x2B80`; entry `+8` points to the packet. The ordinary
TAA call uses direct groups `ceil(width/16), ceil(height/16), 1`.

## Reviewed live gates

| Span | Bytes | SHA-256 |
| --- | ---: | --- |
| TAA2 Execute `2392150` | 4221 | `d2c6c0f6a5ed39c998bcf62afed19132e6dd0d7cfbb19e18b80c15531af2bf91` |
| Dispatch producer `23C9980` | 321 | `d073b70ae437d281596861720893c3b3cc9f82155fa0c83f2c3ae6918fd2f3a2` |
| Texture setter `23D0130` | 126 | `467232d83007c7713430d1ae05c38ed6013f86afe8185f5577cd4a9936394053` |
| G-buffer getter `257CA70` | 8 | `5f55ddcc3069359da7b5df1443cb44504c3c71a47126a3362a58dc9f0a469771` |

The initial exports used individual unwind ranges; those files contain only
prefixes and are preserved as such. Complete TAA spans were re-exported against
the prior reviewed hashes. New helper boundaries were reconstructed through
chained unwind records. No conclusions use the prefixes as complete functions.

## Next capture

The [v1.0.0 observer](../runtime/temporal-input-observer.md) records three
distinct originating frames. It validates the six statically mapped owner
sources, records the selected depth binding, pins the seven D3D12 resources,
and checks source stability and the newly queued request on return.

This establishes CPU request association and resource descriptions only.
GPU consumption, current resource states, motion pixels and DLAA remain
unqualified. Follow-up work should use the observed actual resources to trace
the matching GPU bindings and extract the relevant shader before selecting
a motion conversion or adding modern reconstruction.
