# Scaled depth draw capture: v1.23.1

Prepared 2026-09-27 to address the [v1.23.0 partial result](scaled-temporal-first-result.md).
The goal remains three native temporal color/depth/motion input sets at 0.5
active scale and full allocation scale. No DLSS/DLAA SDK evaluation occurs.
The known continuous DLAA runtime and saved graphics settings are unchanged.

## Native observation and ownership

The exact tested host is SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`,
version 0.0.0.0; captured executable SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The existing 26 native code checks remain, plus the 206-byte draw helper at
RVA 0x259d670, SHA-256
`d6b0b80a37a8fadba69d215cfcebd960c74f8d3554e9375b03ac7785d4f6ee39`.

One new hook observes ID3D12GraphicsCommandList::DrawInstanced (vtable slot 12),
for a total of 21. Its Windows x64 signature is
`void(list*, UINT vertexCount, UINT instanceCount, UINT startVertex, UINT startInstance)`;
the fifth argument remains on the stack and all original arguments are forwarded.
It only records within the existing thread-local, pinned native depth consumer.
Other draws forward immediately. The admitted caller must be image+0x259d72e.

At the draw, require:

- The same pinned list, source/destination owners and resources as the depth packet.
- The nested packet matches recording+0x158, its draw arguments match the API,
  and its rectangle equals the original smaller depth request.
- Source R32_TYPELESS depth and destination R32_FLOAT render-target allocations
  have matching dimensions and unchanged resource identity.
- Complete observed barriers establish source state 192 and destination state 4.
- The nested binding pointers, rectangle and cached pipeline identity remain
  identical across the original draw. These are binding identities, not an
  independent descriptor-content or pixel-shader disassembly claim.

The readback is recorded only after that draw returns. It transitions depth
from RENDER_TARGET to COPY_SOURCE and restores RENDER_TARGET even if an injected
copy fails after a returned transition. The old CopyResource branch still uses
COPY_DEST restoration and retains its original qualification.

The failed trace had three depth producers before TAA; the first writer used
a different command list from TAA. Therefore the draw path selects the last
matching published producer before the selected TAA request, by frame/view/
container/epoch/bank/async and destination identity. It does not infer GPU
completion from CPU publication: the existing exact list, allocator/reset,
dispatch, submission, fence and retirement requirements still apply afterward.
If those do not match, the capture is rejected.

The journal's historical native-copy marker and consumer copy/copy_exit fields
now bracket either operation; the new explicit operation field distinguishes
draw from copy. Python checks reproduce the producer selection and draw/state
checks. Raw images retain full allocations; v1.23.1 pixel-conversion comparisons
use only the validated active rectangle. Unused allocation pixels are not
asserted to be valid temporal inputs. Whole-file sizes/hashes remain checked.
Motion conversion still uses allocation dimensions for this diagnostic and is
not certified as the scaled DLSS motion convention.

## Shutdown and validation

Activation shutdown now retries a drained interval before disabling patches,
records patch removal separately, then waits for callbacks already routed to
the retained trampoline to return. Both patch removal and callback drain must
pass. Native scale restoration remains a separate checked operation.

The build uses /W4 /WX. Twenty-two native checks pass, including full-size copy
regression, draw success, three draws before each TAA dispatch, wrong caller,
wrong draw arguments, changed pipeline/nested packet, absent retirement and a
forced private-signal failure. The signal-failure fixture now explicitly injects
that failure for depth modes. Pure state tests reject split barriers, partial
subresources, aliasing and copy/draw state confusion.

Retained WARP fixtures execute actual depth-texture sampling into an 8x6 region
of a 17x13 allocation. The captured region contains the expected per-frame
depth and the unused region retains a distinct clear value. Raw images and
typed conversions are checked against those values, with GPU validation enabled.
Rehashed damaged active pixels fail review; unused pixels are outside the
semantic comparison. Synthetic fixture identity is not live game evidence.

Use the same bare MO2 launch entry, native AA/DLSS Off, fixed windowed 2560x1440.
Load a save and stay still. Agent arms the exact process via the frozen
arm_scaled_temporal.py. Three-frame capture and full-scale restoration are
automatic. Review the frozen log/images before making an upscaling claim.

Evidence root: `local/scaled-temporal-20260927/v1.23.1/`. The exact installed
v1.23.0 DLL/output sidecar are backed up before replacement. Keep the new DLL
installed after the run; deploy.ps1 provides explicit rollback with the game
closed. Live verification of the new route remains pending.
