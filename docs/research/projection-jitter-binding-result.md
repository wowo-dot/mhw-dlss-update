# Run 46324: one jitter sample reaches a recorded draw binding

Accepted 2026-09-23. v0.2.1 captured zero-jitter baseline, one nonzero native
sample, immediate camera-field restoration, the exact source's publication,
backend consumption and root-CBV/indexed-draw calls, then zero-jitter recovery.
The recovery deferral fixed the collision from [run 42212](projection-binding-stimulus-result.md)
without losing the selected binding evidence. This establishes a nonzero
CPU-to-command-recording path, not GPU execution or pixel displacement.

## Identity and archive

- PID 46324, creation `134346666460641413`.
- Executable version `0.0.0.0`, SHA-256
  `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
- Recorder SHA-256
  `A468B34138ECA53B51935DB52CA3621BAF13288F835C4B18F2DEE6C0F84D1621`.
- JSONL SHA-256
  `a6085dc1cdb66162e8b8860927a722002887978ca6f26a934239057eb8fe8687`.
- Startup reservation, all 36 live code spans and twenty hook targets passed.
  Twenty-one admitted visits; zero faults, critical drops or pending calls.
  All hooks disabled and drained; no game crash was reported.

Private evidence is in `local/projection-binding-20260923/v0.2.1/`.
`accept_46324.py` replays the exact archived reviewer, source hashes, shader
identity and rollback into `acceptance-46324.json`. `archive-46324.json` records
evidence hashes. The prior rejected run remains intact. The unchanged build
has 38 native and 59 Python preparation checks; these are distinct from this
accepted game evidence.

## CPU stimulus and recovery

| Stage | Frame/epoch | Stored jitter | Captured result |
|---|---:|---|---|
| Baseline | 15574 | `[0, 0]` | Projection and VP equal their unjittered forms |
| Stimulus | 15575 | `[0.265625, 0.40123462677001953]` | Predicted nonzero projection and VP changes |
| Recovery | 15577 | `[0, 0]` | Entire captured constants hash equals baseline |

Exactly one injection and one successful camera-field restoration are recorded.
All three selected calls retain the same camera/type, producer thread, renderer,
view, context and table. Before/after provider snapshots match, with sizing
disabled and no NGX wrapper. The view and unjittered matrices are identical
across all three calls, so camera motion does not explain the matrix change.

Independent review verifies each frame's projection/jitter relation, view times
projection, and the expected homogeneous-coordinate VP change. Its maximum
absolute product residual for the stimulus is `0.00028614088660106063`, within
the existing float32-scaled bound. The maximum VP x/y coefficient difference
is `1.4775390625`; this is a matrix coefficient difference, **not** an observed
pixel displacement. No tolerance was changed to accept the run.

One unreserved recovery admission was deferred because bookkeeping was busy.
The later frame 15577 passed recovery checks. The skipped call's frame is not
recorded, so it is not assigned an exact frame number from the gap alone.
There were no critical capture drops. Recovery source order 33 follows the
selected draw's return at order 32; its completion is order 35.

## Stimulus source through command recording

The 1,072-byte source at CPU address `1259735296` belongs to registry slot 11,
arena 2, offset 264448, aligned extent 1280. Its computed GPU address is
`12381849856`. At consumption the allocator has advanced to arena 0. The source
bytes remain unchanged through the matched draw. The same mode-2 CPU/GPU resource
has three 16 MiB arenas; no copy or GPU command is added by the probe.

Restoration completes at order 6. Publication at order 8 and consumption at
order 9 retain packet `3275687488` and the exact stimulus CPU pointer. Within
the selected native indexed-draw wrapper (`259D930`), nine observed API calls
include:

1. Viewport `(0, 0, 2560, 1440, 0, 1)`, matching the matrix dimensions.
2. PSO `5283586832` and root signature `357102800`.
3. `SetGraphicsRootConstantBufferView(9, 12381849856)` before the draw.
4. `DrawIndexedInstanced(2520, 1, 2586, 0, 0)`.

Stable native metadata selects compact root variant 1. The pinned construction
route maps parameter 9 to VS b0 in space zero; the actual root-signature call
matches the selected pointer. The retained VS/PS containers are byte-identical
to run 6444. Its local disassembly therefore applies: the candidate VS reads
`fViewProj` at CB b0 bytes 0–63 for clip position; the PS has no color output.
No shader disassembly or proprietary binary is added to distributable sources.

The independent verdict includes `stimulus_cpu_sequence_verified`,
`nonzero_matrix_binding_observed`, `nonzero_known_vertex_binding_correlated`
and `matched_viewport_observed`. Actual executing-PSO bytecode, serialized
root-signature blob, GPU constant contents, queue submission/fence completion,
shader execution and raster displacement are still independently unobserved.
A recorded draw call must not be described as proof the GPU executed it.

## Cleanup and next decision

The user exited normally. The previous v0.15 DLL is restored at the MO2 slot,
SHA-256 `689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The temporary output sidecar is removed. All four graphics/profile/mode files
retain their original hashes. Archive replay verifies 32 source files and
restoration. No probe is awaiting an unchanged rerun.

The subsequent [offline attachment/PSO map](projection-attachment-map.md) is
complete and defines a read-only qualification capture before a bounded GPU
observation tied to that draw and queue completion. The retained no-color-output PS suggests depth-oriented work,
but attachments were not captured; do not assume it writes scene color or
identify the pass from that shader alone. Establish resource format/state,
dimensions and frame alignment before comparing baseline/stimulus GPU output.
GPU raster displacement, motion-vector sign/scale/history alignment and modern
DLAA/DLSS integration remain separate qualification steps.
