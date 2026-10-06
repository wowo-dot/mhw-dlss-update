# Gallery replay with captured depth — offline result

Both v1.25.40 depth images and their paired constants now run through the whole
six-pass blur pipeline and composition offline. On NVIDIA and WARP, the adapted
color path matches the independent reference exactly at all seven retained
intermediate images and every final output pixel. Both paths include one
explicit horizontal-reduction synchronization correction discovered by this
experiment. This is not a claim that the unmodified native shader is equivalent
or that live Gallery integration is complete.

## Inputs and scope

The source archive `local/dof-depth-pixels-20261001` was reverified: 754 files,
manifest SHA-256
`8f16b3e95e4fd6b6cb8896d458e3f1a6dcfd9bf809d22e9ce774fed618f481c3`.
Each full 2560x1440 R32F input is paired with its own 1792-byte constant pack
from prepared frames 14139/14199 and native execution frames 14140/14200.
Extraction verifies the captured buffers agree across all six compute commands
and the composition draw. Hashes and offsets are in `inputs.json`.

Depth pixels, constants and resource formats are captured. HDR color is still
a synthetic high-frequency pattern; native descriptor contents are not captured.
The reference feeds the original preparation shader an independently generated
CPU color proxy and uses the original composition shader with transformed
screen constants. The candidate uses the existing preparation and composition
coordinate adapters. Both use the complete depth image, original artistic
constants and chronological Image overwrite. Fixture SRVs/UAVs are explicit.

All 3,686,400 depth values are read back bit-exactly after each complete pipeline
execution. Wrong-size and nonfinite inputs are rejected before device creation.
The corrected adapter rejects altered or truncated shader bytes. Builds use
`/W4 /WX`; successful validated runs reject GPU warning/error messages.

## Horizontal-reduction synchronization finding

The original captured horizontal shader merges eight group-shared partial
results using 128 threads, then lets the first 16 threads load/store the result
without a group barrier between those operations. Its vertical counterpart
does contain a barrier at the corresponding boundary. Earlier constant-tile
synthetic inputs did not expose incomplete partial reductions.

With varying live depth, the native horizontal shader fails an independent CPU
max/min reduction on both devices. Repeated NVIDIA runs fail at different pixels.
It also fails with GPU-based validation disabled, so validation instrumentation
alone does not explain the result. Under the final arithmetic-aware oracle:

| Device/control | First reported disagreement |
| --- | --- |
| NVIDIA, original shader, GPU validation off | tile (58,2), minimum radius: 0.000457764 versus approximately 0.000447922 |
| WARP, original shader, GPU validation on | tile (5,0), maximum radius: 0.000526428 versus approximately 0.000534064 |

The offline correction inserts exactly one `sync_g_t` instruction, outside both
conditional branches, before original SHEX word 450. The entire 6,524-byte
shader is SHA-256 gated. Arithmetic, coordinates, constants, bindings and
thread-group size remain unchanged. Reflection is checked and stale STAT data
is removed when rebuilding the DXBC container. The resulting shader passes the
horizontal and vertical reduction oracles for both real inputs on both devices.
The NVIDIA validation-disabled corrected control passes as well.

This demonstrates a synchronization defect in the captured shader's offline
execution. It does not attribute the live Gallery DLSS suspension to that defect:
the current live planner still deliberately rejects the unsupported compute
sequence. No game asset or live shader was patched by this experiment.

## Separate arithmetic-oracle correction

Sample 1 contains zero-depth pixels whose CPU quotient is exactly 16. NVIDIA
can produce a value immediately below that boundary before packed-format
storage, yielding 15.875 in the R11 channel. The first oracle incorrectly treated
the CPU quotient as exact shader arithmetic. Microsoft's [floating-point
rules](https://learn.microsoft.com/en-us/windows/win32/direct3d11/floating-point-rules)
permit MAD expansion and reciprocal/multiply division with bounded rounding.

The replacement oracle propagates conservative float-neighbor intervals through
the permitted operation sequence, then brackets the packed-format conversion.
It still rejects the original missing-partial-reduction cases. Boundary controls
accept 16 and 15.875 for this quotient but reject 15.75. Vertical reduction remains
an exact comparison against the already rounded horizontal pixels. Candidate
versus reference comparisons remain exact; no final-image tolerance was added.
The initial failures and reports are retained.

## Results

Each final case executes five whole pipelines: reference, adapted, wrong color
coordinates, wrong Image writer, and cleared outside-view depth. Deltas below
are against the reference on the same device; they are not cross-device equality.

| Sample | Device | Final adapted delta | Wrong color delta | Wrong writer delta | Cleared outer depth delta |
| --- | --- | ---: | ---: | ---: | ---: |
| 0 | NVIDIA | 0 | 0 | 0 | 0 |
| 0 | WARP | 0 | 0 | 0 | 0 |
| 1 | NVIDIA | 0 | 0.0622469 | 0.153389 | 0.00310171 |
| 1 | WARP | 0 | 0.0617239 | 0.153389 | 0.00310171 |

All seven intermediate comparisons are also exact, including byte comparison
of the packed R8 alpha readback. Sample 0's final image is insensitive to these
controls, but its wrong color coordinates change preparation by 0.1875 and
clearing outer depth changes the horizontal reduction. Sample 1 exposes all
three errors in the final image. Therefore a zero final delta for sample 0
alone would have been insufficient verification.

The native-format synthetic regression still passes on both devices. The
original shader remains rejected under the corrected oracle; the synchronized
shader passes. Native resource formats and full depth storage are preserved.
The known live outer-depth pixels must not be silently cleared or cropped.

## Remaining live work

Code review confirms that `compute_resources.inl` and the compute bundle are
diagnostic, while `dependency_route.inl` can handle the final DOF draw with
already-produced native auxiliaries. There is no owned live implementation of
the six compute stages yet. The offline fixture is not linked into the DLL.

The next implementation needs to:

1. Preserve or validate the native input view contract, including the complete
   depth input. Actual SRV contents remain unverified; resource dimensions alone
   do not authorize inventing a view or discarding outside-view storage.
2. Move the validated compute sequence into owned, format-correct resources and
   descriptor tables, preserving the chronological Image overwrite and the
   horizontal synchronization correction.
3. Connect its final composition to reconstructed color within the existing
   frame decision, binding restoration, command-list generation and fence/Reset
   lifetime rules. Unsupported frames must retain native rendering.

No launch is requested by this offline result. v1.25.40 remains installed with
the four configurations unchanged. Its previous output root is sealed; prepare
a fresh destination before the next runtime test. Gallery remains the priority,
followed by monster effects and the hunter silhouette; camp-entry work is deferred.

Private evidence root: `local/dof-live-depth-replay-20261001`. See `review.json`,
`inputs.json`, final device reports, retained failure outputs, patched shader
disassembly and source/binary snapshots. No captured shader, depth image or
vendor binary is added to distributable source.
