# Run 6444: projection buffer reaches a recorded D3D12 draw

On 2026-09-23, v0.1.2 recorded and independently accepted the chain from an
eligible camera projection result through its publication packet, backend
consumption, root constant-buffer binding and an indexed draw call. The game
exited normally, all hooks were disabled and drained, the previous v0.15 DLL
was restored, and all four graphics/profile/mode files remained unchanged.
This is CPU and command-recording evidence, not proof of GPU execution.

## Identity and result

- Process 6444, creation identity `134346624217637489`.
- Executable version `0.0.0.0`, SHA-256
  `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
- Recorder SHA-256
  `EBB3E88B25C5421ED2E82B5EB484E76898E2DF2C386E94661B7127288571F917`.
- JSONL SHA-256
  `b070e1a9d89f230b5536427e39376c361c4f2ec1d5d0bac81726063ee01b574a`.
- Startup reservation succeeded; 29 live code spans passed; twenty hook
  targets installed. The preceding allocation failure did not recur.
- Frame/epoch 16528; 15 admitted visits; zero faults, drops or pending calls.
  One native draw completed, with nine API events and two shader containers.

The ignored archive `local/projection-binding-20260923/v0.1.2/` retains exact
sources, tested DLL, deployment/restoration receipts, raw trace and shader
containers. `review-6444.json` gives `root-cbv-call-chain-observed` at root
parameter 9. `accept_6444.py` replays that review, shader checks and restoration
invariants into `acceptance-6444.json`. Private disassembly stays in the archive.

## Observed chain

The source is a 1,072-byte, 268-word constant buffer in registry slot 11.
Its CPU address, `1243023616`, is preserved in the publication packet and the
backend consumer. The source thread is 45984; the consumer/draw thread is
45592. The consumed allocation belongs to arena 1, offset 264448, with a
1,280-byte aligned extent. At consumption, the allocator has already advanced
to arena 2. This confirms why allocation index cannot identify the consumed
arena.

Mode 2 uses the same CPU/GPU constant resource, size 50,331,648 bytes with three
16 MiB arenas. The independently checked GPU address for the source range is
`12365072640`. Within the matched native draw (`259D930`):

1. `RSSetViewports` receives `(0, 0, 2560, 1440, 0, 1)`.
2. `SetPipelineState` and `SetGraphicsRootSignature` are observed returning.
3. `SetGraphicsRootConstantBufferView(9, 12365072640)` binds the source range.
   Other root CBVs are recorded at parameters 12 and 13.
4. Descriptor tables are bound at parameters 0 and 7.
5. `DrawIndexedInstanced` is observed with 2,520 indices, one instance,
   start index 2586, base vertex zero and start instance zero.

The source bytes remain unchanged through the draw. The matching root CBV is
not invalidated before that draw call. No jitter, provider call, transfer,
readback or GPU command was added by this recorder.

## Captured shader finding

The native graphics state supplied a VS (4,708 bytes, SHA-256
`94c34ad9564bb58ab799ade08682054ac6dff9b6402e7d02b2a2b2f2e8109511`)
and PS (524 bytes, SHA-256
`eabe105695dd464acc4bcba5c3b56a31d7013321eec363dbbf6434beac3c353d`).
No GS, HS or DS was present in that captured state. Both DXBC bounds and hashes
pass independent review. Windows SDK 10.0.26100.0 `fxc /dumpbin` disassembled
the retained containers without invoking the game or modifying the bytecode.

The VS declares `CBViewProjection` at shader register b0. It reads only the
first four float4 rows of that buffer, the `fViewProj` matrix at bytes 0–63,
to transform an instanced world position into clip coordinates. Its x/y/w
outputs come from that transform; the z output includes depth-bias arithmetic.
The reflected `fProj` member at byte 128 and `fNoJitterProj` at byte 800 are
unused by this shader. Thus the earlier CPU `fProj` jitter result alone does
not qualify the matrix this shader consumes. The PS has no color output and
only returns; this is compatible with depth-oriented work, but no render-target
or depth attachment was captured, so the pass is not identified conclusively.

Root parameter 9 is not automatically shader register b0. In addition, the
containers come from native graphics-state metadata; the actual PSO's creation
descriptor was not captured. The API PSO pointer and native shader metadata
association are useful evidence but not an independent PSO-bytecode proof.

## Offline follow-up and next diagnostic

The pinned decoded executable's root-signature builder at `2626030` contains
a variant using root CBVs with fourteen root parameters. Its VS-visible
CBVs begin at parameter 9, mapping to b0–b4 in space zero; parameter 12/b3 and
13/b4 are consistent with this captured VS's other constant buffers. State
setup's compact path chooses backend `+280` when shader-set flags at `+10`
and `+1C` both contain `10000`, and sets the VS binding base to 7. The normal
and VS-only builders use different layouts. These are static candidate
relationships: this capture did not retain those selector fields or the
backend's root-signature pointer array, so the actual variant is not accepted
from binding numbers alone. Exploratory windows are not new hook gates.

Next, extend the bounded read-only capture to retain and revalidate the root
selection metadata, match the actual root-signature API argument, and retain
the source's `fViewProj`, view and unjittered view-projection matrices. Validate
the applicable root construction path against the pinned code/SDK layout and
map the captured shader register to the observed root parameter. Explicitly
check how the earlier jitter stimulus propagates to `fViewProj` before treating
the CPU projection proof as a raster proof. Avoid repeating the same unmodified
capture now that its source-to-draw chain is accepted.

Submission/fence correlation, GPU constant contents, shader execution, raster
displacement, temporal accumulation, and working DLAA/DLSS remain open.

Follow-up: [v0.1.3 run 31912](view-projection-contract.md#run-31912-zero-jitter-contract-accepted)
now validates the live selector fields and zero-jitter matrices. It accepts
the native compact-layout mapping of root parameter 9 to VS b0 against the
same shader containers. Nonzero-jitter propagation and GPU/raster evidence
remain pending; this does not retroactively add missing fields to run 6444.
