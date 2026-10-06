# DOF pipeline fixture and constant capture (v1.25.36)

The v1.25.35 Gallery bundles identify six compute stages followed by one
composition draw. Before admitting that queue, reproduce the entire chain
offline and establish the constants each native stage actually receives.
This candidate changes diagnostic capture only; opcode 6 remains rejected
by the live output planner. Gallery may still suspend DLSS and recover afterward.

## Offline fixture

`tools/dof_pipeline_fixture.cpp` loads the eight private shaders extracted
from v1.25.35, checks their SHA-256 identities, creates the six compute PSOs
and the native composition PSO, and runs them in captured order. It uses a
320x320 synthetic scene, explicit resource transitions, GPU validation and
fenced readbacks of seven intermediate writer images and the final image.
The intermediate Image is captured before and after unpack overwrites it.

Three cases cover zero blur, varying near/far depth, and vignette. Independent
CPU checks cover horizontal/vertical reductions and, for zero blur, the
2x2 color preparation and unchanged final scene. Negative controls poison
the unused portion of an external texture and omit the latest Image writer;
they must visibly change output in the applicable cases. These prove the
fixture's sensitivity, not a correction for the live reduced-resolution path.

Float intermediates deliberately use RGBA32F to expose calculations. Packed
filter output is R32_UINT and alpha is R8_UNORM. Native format equivalence,
live resource states, valid pixel footprints and scaled-pipeline equivalence
remain unverified. The existing composition-adapter fixture separately checks
its native-reference/production output at 1x and 2x.

`tools/dof_fixture_support.hpp` shares the existing fixture's resource,
upload/readback and validated device helpers; no fixture opens the game.
All extracted shaders, constant data and images stay under ignored `local/`.

## Bounded live capture

Same hook, call convention, frame stamp and executable gates as
[v1.25.35](dof-compute-bundle.md). Host SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
No new hook or rendering admission is introduced.

At the prepared DOF queue rejection, copy only type-0 constant bindings:

| Native ID | Register | Reflected buffer | Bounded bytes |
|---|---|---|---|
| 11 | b0 | CBViewProjection | 1072 |
| 12 | b1 | CBScreen | 112 |
| 501 | b3 | CBNewDOFFilter | 96 |

Validate every owner index, ID/register pairing, mode 0, 16-byte alignment,
pointer arithmetic and register uniqueness before reading. Owner is the
raw CPU constant allocation; the projection-binding recorder already uses
this mode's pointer contract. Each compute and draw keeps its own snapshot.
Unknown constants, unreadable memory, or any change between complete paired
captures reject that sample. Two accepted bundles, sixteen attempts maximum,
and at least sixty frames between attempts remain enforced. Reads stop at
capacity. No pointer is read later; there are no COM calls or GPU mutations
in this diagnostic path. Schema 3, `prepared-compute-constants-v1`, distinguishes
the new evidence from historical schema 2. The full reviewer requires it for
v1.25.36; extraction saves raw bounded buffers and labeled field values.

The previous captures show one CBScreen owner shared by all computing stages,
and a different owner at the composition draw. Buffer addresses alone do not
establish which values differ. The new evidence must establish that distinction
before choosing input-coordinate adaptations. Artistic blur/vignette constants
must remain intact. A full allocation is not proof that all its pixels are valid.

## Next runtime check

Use a fresh output directory and preserve the installed DLL before deployment.
Main menu -> loaded save -> affected Gallery cutscene -> recovery. Native AA
and DLSS Off, same window size and solo/offline profile. Require two stable
bundles with the known six shader hashes, exact constant coverage and zero
capture errors. Then extract/compare per-pass CBScreen, DOF parameters and
projection depth conversion. Verify normal cleanup independently before exit.
Keep the new diagnostic DLL installed, as requested. Camp remains deferred.

## Offline validation result

Both WARP and NVIDIA hardware passed the complete pipeline's three synthetic
cases with GPU validation enabled. Shader reflection independently confirms
the three bounded constant-buffer sizes. The existing composition fixture
also passed eight 1x/2x native-reference/production cases on each device, with
zero pixel delta and three sensitive negative controls.

The new fixture initially reported hardware validation state errors while
recycling case allocations. Its final version uses distinct command recordings
and retains all case resources/descriptors until the bounded experiment ends;
both devices then pass. This addresses fixture isolation, not a proven live
renderer defect. Failed logs are preserved in `local/dof-pipeline-20261001`.

Cross-adapter full-pipeline output is not bit-identical for blur: maximum
absolute delta 0.006302, mean below 0.000005 in the synthetic HDR range. Zero-blur
output and the preparation color images match exactly. No full-pipeline
cross-adapter equivalence or live resolution correction is claimed.

The diagnostic passes 59 native tests and 64 Python tests, ten new-version
schema rejection controls, historical v1.25.35 review, WARP/hardware complete
queue refusal tests, and the 96-frame continuous runtime fixture. Builds are
warning-free. Live constant capture remains pending.
