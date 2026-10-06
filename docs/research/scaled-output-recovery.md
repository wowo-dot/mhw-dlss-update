# Full-size scaled output transaction

Implemented and verified offline on 2026-09-28. The installed v1.23.9 input
recorder remains unchanged and unarmed. This is a GPU integration component;
the game is not yet displaying this scaled output.

## Implementation

`temporal_present::Output` retains its existing same-size RGBA32-to-packed-HDR
path. An explicit `SpatialSource` constructor adds packed R11G11B10 input,
independent allocation/active/output extents, and pixel-center bilinear spatial
recovery. All four loads clamp to the active top-left rectangle. Every output
pixel is written; inactive allocation pixels never participate. The existing
nearest-even HDR packing is shared. There is no temporal history, sharpening,
tone mapping, exposure adjustment, or native AA in this recovery.

The caller supplies and proves the source state; recording restores it and
leaves native output in UAV state. Layout, identity, geometry, list type, and
bounded reference reservations are checked before commands. Descriptors remain
private and immutable. Native references use the existing bounded retention
contract, and release requires both GPU completion and recording retirement.

`scaled_temporal::record_resolve` derives its geometry and raw-color identity
from the prepared `InlineFrame`; an independently guessed source is not
accepted. It reserves full-output recovery and binding restoration before SDK
work. Successful evaluation writes the full SDK output. Evaluation failure
after binding restoration overwrites the complete output with spatial color.
Unrestored bindings report unrecoverable instead of executing another pass.
The caller still owns feature activation, history resets, downstream routing,
native resource states, queue ordering and recording lifetime. Spatial recovery
cannot remove jitter that was already rasterized. Native TAA/FXAA is not called.

The existing SDK failure policy retains potentially recorded SDK objects until
process exit; this change does not claim to solve feature teardown after SDK
failure. The spatial recovery object itself passes fence/reset retirement.

## Verification

Final native build passed /W4 /WX. Seven focused CTest cases passed: binding
logic, original packed output, 2x spatial output, fractional spatial output,
identity spatial output, native resolve policy, and SDK geometry. WARP uses the D3D12 debug layer and
GPU validation. It checks poisoned allocation padding, source preservation,
full output coverage, invalid geometry/layout/state, incomplete fences, and
subsequent compute/graphics draws using restored bindings.

Identity and 2x output match an independent pixel oracle exactly. The 1.5x
test compares against double-precision interpolation and allows at most one
adjacent packed value per channel: float interpolation at a rounding midpoint
can choose the other neighbor. The initial exact-only fractional test exposed
that distinction; its failed log is retained. This tolerance does not apply to
the initial 2x Performance route or the hardware recovery checks.

Six final hardware cases passed on the established adapter LUID [0,98547]:

- Three Performance evaluations, with history retained after the first reset.
- Three spatial-only frames with no SDK evaluation.
- Three frames each after deliberate failure following conversion, a reported
  Evaluate failure, and failure after a successful SDK output handoff.
- One existing same-size DLAA resolve regression.

All twelve recovery frames, including each first faulting frame, match the
independent packed bilinear reference at every 2560x1440 pixel: 44,236,800 packed
pixels in total. Input padding is poisoned in every scaled transaction case.
All six cases preserve compute/graphics bindings, pass subsequent exact output
checks, bypass native TAA, and produce zero GPU validation warnings/errors.
The hardware fixture uses archived images, not a live game process. Repeated
input images and synthetic metadata prove GPU transport/recovery, not moving
scene reconstruction quality or frame time.

Pinned SDK/header/runtime identities are unchanged from
[the first scaled SDK replay](first-scaled-sdk-output.md). Source captures are
v1.23.3 (`inputs-46960-134350141126140646`, receipt 0) and the accepted v1.17.0
DLAA capture for the regression. Evidence, binaries and frozen sources are
under ignored `local/scaled-recovery-20260928/`.

## Final transfer identified

Further static inspection explains the previously unresolved endpoint in
[v1.23.9](scaled-postfilter-result.md). Tested host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`;
decrypted research image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

The order-134217713 entry is embedded at `AutoExposure unit + 0x238`.
That equality holds in all three accepted v1.23.9 records. The unit vtable
RVA 0x3538400 has Execute at 0x24d0780. This function registers both its main
entry and the extra stage-15 endpoint while the queue collects. During the
later Execute, it builds both the exposure pass and a separate
`ToneMappingApply_ColorGrading` draw. The latter takes its source from
`unit+0x248` (endpoint input), binds the attachment belonging to `unit+0x250`
(endpoint output), and submits a quad at 0x24d1960. Thus the endpoint's null
unit is not evidence of a plain copy or unused color transfer: its work is
constructed by the earlier unit invocation.

Full Execute span 0x24d0780..0x24d199c is 4,636 bytes, SHA-256
`5853f4dd6561e66842edab799f7d87ea3042f7b873dd72ab848e612579ff3e7b`.
Local static exports remain private. No new hook was installed. This explains
CPU construction/resource routing; the color-grading shader and its executed
GPU branch are not yet independently captured.

The same Execute computes both quads' UVs from context rectangle / allocation
size, retaining those values across the function. Enlarging only the final
screen triangle would therefore leave color grading sampling a quarter of the
full-size image. The motion-filter color/depth/motion coordinate distinction
also remains. The next integration must preserve these effects and update
their output viewport/sampling contract, then use this verified GPU transaction.
Do not remove either pass simply because a graphics option is Off, and do not
claim live DLSS presentation from these offscreen results.
