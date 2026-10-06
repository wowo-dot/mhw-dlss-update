# Native depth copy investigation

Offline audit, 2026-09-24. Input is accepted v0.6.0 run 21900, frame 14548.
Executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Decoded executable SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Reproducible local evidence: `local/depth-readback-20260924/audit.py` and
`audit.json` (22 instruction checks, SDK slot checks, eight depth transitions).
The audit also reproduces the accepted follow-up review of run 21900.

## Confirmed observations

The selected depth resource, 358065648, has these declarations in the retained
DIRECT submissions. Rows follow submitted list order, not CPU recording order.
This is not a claim that entire command lists in one batch execute serially.

| Execute entry | Batch index | Reset / Close | Depth transitions |
| --- | --- | --- | --- |
| 5283 | 0 | 4931 / 5251 | COPY_SOURCE → DEPTH_WRITE |
| 5291 | 0 | 4947 / 5259 | Selected draw; no depth transition |
| 5291 | 1 | 4933 / 5127 | DEPTH_WRITE → COPY_SOURCE + NON_PIXEL_SHADER_RESOURCE |
| 5291 | 2 | 4935 / 5107 | COPY_SOURCE + NON_PIXEL_SHADER_RESOURCE → DEPTH_WRITE |
| 5291 | 3 | 4937 / 5165 | DEPTH_WRITE → DEPTH_READ + shader reads |
| 5297 | 0 | 4949 / 5247 | Read states → DEPTH_WRITE → COPY_SOURCE + NON_PIXEL_SHADER_RESOURCE → DEPTH_WRITE |
| 5297 | 1 | 4945 / 5279 | DEPTH_WRITE → COPY_SOURCE |

Several transitions return to RVA `0x259DFCF`. Its owning function is
`0x259DF30..0x259E1D9`, 681 bytes, SHA-256
`e84ae8742c4a0ef334692595bb89c04c8be80306d4416c20304566cc1e5dd85a`.
It flushes pending barriers through command-list slot 26 and then selects a
copy or fallback draw branch. Windows x64 inputs are executor RCX, context
RDX, packet R8. The packet stores source owner at +0x20 and destination owner
at +0x18; each selects +0xF0 backing when nonnull, otherwise +0x60. Backing
+0x20 is the resource argument.

| API | Slot | Instruction RVA | Return RVA | Arguments on this branch |
| --- | --- | --- | --- | --- |
| CopyTextureRegion | 16 | 0x259E0DC | 0x259E0E2 | Subresource 0 to 0; zero destination coordinates; null source box |
| CopyResource | 17 | 0x259E0FA | 0x259E100 | Destination RDX, source R8 |
| CopyTextureRegion | 16 | 0x259E1A2 | 0x259E1A8 | Subresource 0 to 0; explicit source rectangle and destination XY |

The copy wrapper also falls back to `0x259D670`. Observing its barrier call
does **not** establish that a copy occurred. v0.6.0 did not capture copy APIs,
their destination resources or their argument lifetimes.

## Next observation

[Recorder v0.7.0](../runtime/projection-copy-recorder.md) observes those three
copy call sites and correlates their pinned resource identities with the
selected depth and actual Reset/Close/submission generations. It adds no game
GPU commands or resource transitions. A full depth-source copy with a matching
local COPY_SOURCE declaration would identify a candidate for an additional
owned readback copy without changing the game's source state. It would not
alone establish a final-scene depth image, cross-queue safety, or jitter.

A later readback must validate the precise source interval, full subresource,
device/format/sample count, footprints, command-list generation, GPU completion
and retained lifetimes. Depth/stencil copies require the entire subresource,
zero destination coordinates and a null box; Map itself does not wait for GPU
completion. See Microsoft's [CopyTextureRegion contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-copytextureregion)
and [readback heap procedure](https://learn.microsoft.com/en-us/windows/win32/direct3d12/readback-data-using-heaps).
