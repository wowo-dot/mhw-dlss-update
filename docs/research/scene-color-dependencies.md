# Scene-derived temporary textures: offline implementation

2026-10-01. Implements the next offline step after
[the prepared radial-blur dependency capture](prepared-draw-dependencies-result.md).
No game hooks, installed DLL, saved settings or rendering-admission rules
change in this step. The current v1.25.28 runtime still rejects the wingdrake
path. These components must be connected to the runtime before another
compatibility test can establish a fix.

## Resource plan

`src/scaled_temporal/color_dependencies.hpp` builds a bounded plan in native
draw order. Resources are declared as scene color, auxiliary, or uninitialized
temporary. A temporary written from scene-derived inputs becomes scene-derived
itself. Each input retains its domain and its latest producer index; multiple
writes to the same resource are distinct ordered versions.

The captured two-draw relationship is therefore:

1. Read the scene, write a scene-derived temporary.
2. Read that temporary plus independent auxiliary inputs, write the scene.

The planner rejects unknown reads, undeclared resources, duplicate identities,
SRV/RTV self-aliasing, writes into declared auxiliary inputs, missing final
output, and over-capacity input. A rejected plan publishes no partial result.
It has no scene name, effect name, monster identifier or native descriptor-key
whitelist. Shader semantics are a separate qualification: the graph alone
does not authorize arbitrary shaders or establish GPU resource state.

## Coordinate handling

`src/scaled_temporal/radial_shader.cpp` makes a private variant of the captured
6444-byte radial PS, guarded by its complete SHA-256. Native shader bytes are
provided by the caller; proprietary code is not embedded. Exact instruction
checks precede each token edit, and the rebuilt DXBC receives a valid checksum.

- The temporary-copy draw covers the full output and samples the full scene.
- The radial draw keeps its original primary UVs and independent auxiliary UVs.
- A private scaled inverse size makes the screen-position calculation operate
  in the native coordinate domain. SV_POSITION remains the native MAD operand;
  the bound CBScreen buffer is untouched.
- Only the final t0 sample coordinates are converted to the full scene domain,
  after native radial calculations and UV clamps. All four color sample sites
  are handled, including the chromatic-aberration branch.
- Native constants, resource slots, t1/t2 sampling and auxiliary UVs remain
  unchanged. Reflection/signature chunks are retained; obsolete instruction
  statistics are omitted.

The vertex builder preserves the captured strip order and both UV domains;
its packed RGBA8 color field is distinct from floating-point coordinates.
Geometry currently follows the existing adapter contract: same-aspect 1x or
2x output within its existing size bounds. Other DLSS ratios remain future
work. Identifying a radial shader is not a claim that all reported combat
effects use that shader.

## GPU comparison

`tools/scaled_dependency_fixture.cpp` runs both captured native VS/PS pairs
outside the game, with GPU validation enabled. The candidate uses the new
vertex builder and shader adapter. The independent reference executes the
unmodified shaders, places the temporary copy in a larger allocation, and
adjusts reference screen inverse size instead of rewriting shader instructions.
Both execute real copy and radial draws with explicit resource transitions.

The cases exercise native-size and 2x paths at 96x64 and 128x64 output;
float RGBA targets and the captured
R10G10B10A2 temporary / R11G11B10 output formats; 64 combinations of blur width,
chromatic offsets, sample weighting, alpha/width functions and blending. Four
samples per blur are used. Patterned color and auxiliary textures, nonuniform
radial curves and active UV clamps make coordinate mistakes visible.

Four negative controls must disagree with the reference: copying only the
reduced rectangle, incorrect screen-position conversion, scaling auxiliary
UVs, and leaving scene-color sample coordinates unscaled. These test distinct
failure modes rather than merely checking that a draw executes.

### Numerical qualification

The initial exact comparison passed on WARP, but failed on NVIDIA for scaled,
non-power-of-two, linear-filtered blur. Intermediate copies matched exactly.
Changing screen-position arithmetic order, making the final UV multiplication
precise, and disabling arithmetic refactoring in both programs did not remove
the difference. The production adapter does not keep the ineffective precise
modifier or disable refactoring.

Further controlled GPU captures found:

- All 56 coordinate comparisons matched exactly: both output widths, every
  executed t0 sample site, all four loop iterations, and enabled blur/chromatic
  branches. These are instrumented shader variants, not the final color output.
- Point-filtered versions of all 16 diagnostic cases matched exactly, including
  the intermediate copies. Production samplers remain linear and unchanged.
- The original 96-wide linear-filter diagnostics differed by at most
  0.000729203 in floating-point color. The 128-wide cases matched exactly.

This supports texture filtering as the source of the remaining small delta;
it does not establish a particular driver instruction or a universal hardware
error bound. Direct3D specifies finite subtexel addressing precision and
filtering rules, rather than requiring all differently sized texture layouts
to produce bit-identical filtering results. See Microsoft's
[texture sampling precision specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm#7.18.16%20Texture%20Sampling%20Precision).
The unsuccessful refactoring experiment uses the documented
[precise instruction rules](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/precise).

The full sweep also found a 0.00291669 float difference in the chromatic-only
case, exceeding the first trial budget of 1/1024. That failed receipt is kept.
The completed fixture uses a contrast/weight bound rather than this initial
arbitrary threshold: `.8 * 2/256 * 4 * .5 * .75 = .009375`. These factors are
the fixture's color contrast, one subtexel fraction step on each axis, four
samples, largest channel multiplier, and maximum final gain. Packed inputs
add `2/1023 * 4 * .5 * .75` for filtering precision and one output-format
quantization interval. These are engineering regression bounds for the
controlled pattern and constants, not universal hardware guarantees.

Exact copy and alpha comparisons remain required. WARP, identity geometry
and the power-of-two output retain exact color checks. The bounded comparison
applies only to NVIDIA's 96-wide scaled linear-filter cases. Wrong-coordinate
negative controls must exceed twice the float bound. All per-case differences
are reported; successful bounded comparisons are not relabeled as bit-exact.

### Completed results

- Three native tests passed: dependency versions/rejection, existing effect
  vertices, and existing overlay vertices. Project code built with `/W4 /WX`.
- WARP: 384/384 cases exact, including every copied temporary.
- RTX 5090 (LUID 0:93877): 384/384 cases within the stated rules, 292 exact;
  every copied temporary exact. Largest float-color delta 0.00291669; largest
  decoded R11G11B10 delta 0.015625, at most two packed representable steps.
  The packed bound includes both input-filter and output-format quantization;
  this result is not a claim of bit-exact hardware color.
- Both final sweeps: 1,546 GPU draws, nine invalid-input rejections, four
  wrong-coordinate controls detected, zero GPU validation warnings/errors.
  The smallest NVIDIA negative-control error was 0.0585645, exceeding the
  required twice-bound margin.
- Hardware isolation: 56/56 coordinate cases and 16/16 point-filter cases
  exact. Initial failures and unsuccessful precision experiments are retained.

Private results and source/build identities are in
`local/scene-dependencies-20261001`. Each adapter run has a separate
`result.json`; `validation.json` summarizes completed checks. Private native
and adapted shader blobs/disassemblies remain untracked.

Reproduce the full offline fixture with `scaled_dependency_fixture warp` or
`scaled_dependency_fixture gpu`, followed by the private shader directory and
a new output directory. Optional `diagnostic`, `diagnostic-point`,
`diagnostic-coordinates` and `diagnostic-precise` modes retain the isolation
experiments. These diagnostic modes write measurements, not a pass receipt;
the full fixture writes `result.json` only after all checks pass.

## Runtime integration still required

Connect qualified per-draw read/write resources to the plan, pin actual GPU
resources and descriptors, and prove required states and queue ordering for
the temporary producer and its consumer. Construct the two private draw
replacements, retain their native and private objects through recording
retirement and GPU completion, and restore changed bindings after each draw.
Preparation failure must keep the entire frame on the existing native path.

The captured attachment formats and engine metadata do not replace this
live GPU qualification. No original executable, shader asset or game file is
patched by the offline work, and it does not yet fix Gallery, other combat
effects or hunter silhouettes.
