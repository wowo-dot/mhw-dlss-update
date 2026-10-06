> Update, 2026-09-30: v1.25.8 live capture exposed a binding metadata parsing
> error. v1.25.9 reads one range and eight opaque metadata bytes per stage.
> See [binding capture result](scaled-haze-binding-capture-result.md).

# v1.25.8: tested haze shader adapter and bounded enqueue capture

The sauna inserts uHazeFilter between the motion and grading passes. The
existing scaled route rejects that changed graph. This revision adds an
offline-tested shader adapter and a capture at the native haze enqueue call.
It does not yet enable a fourth scaled effect or change rendering admission.
Evidence: `local/scaled-haze-adapter-20260930`.

## Adapter and tests

`make_haze_shader` requires the entire 14,596-byte PS SHA-256
`4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69`.
It accepts identity or exact 2x geometry, adds one temporary, and multiplies
the final color sample coordinate by output/render at five hash-verified
instruction sites. The native depth, noise, world reconstruction, clamps and
branch logic remain intact. Reflection and signatures are retained; stale
statistics are omitted and the DXBC checksum is recomputed. No captured shader
is included in source or a distributable binary.

The GPU fixture receives the local shader file explicitly and creates its own
test textures/constants and fullscreen vertex shader. It checks all five
techniques, each with zero and nonzero distortion. Identity results exactly
match the native shader. For 2x, the independent reference runs the **unmodified
native shader** with an equivalent larger color allocation, leaving all other
inputs identical. A separate CPU bilinear oracle checks zero-distortion
color, including the retained native edge clamp. These tests prove the
coordinate transformation under fixture inputs; they do not prove the game's
current native geometry, descriptors, mip settings or sampler selection.

The first fixture failed its CPU oracle at the outer edge. Investigation found
that native t3 sampling uses unclamped bilinear UVs and reads one border texel
beyond the active rectangle. The corrected fixture supplies that valid border,
with poison beyond the legitimate footprint. Depth padding remains poisoned.
The adapter was unchanged by this fixture correction. Both failed attempts
are retained. The same issue must be considered against the actual t3 binding.

WARP and NVIDIA RTX 5090 (LUID [0,94196]) each passed 40 draws, 532,480 float
comparisons and 13 malformed shader/geometry rejections, with zero GPU warnings
or errors. All five techniques retained exact native-reference output.

## Capture hook contract

The existing fullscreen helper hook is reused at native RVA 0x2297e20, narrowed
to the haze caller return RVA 0x24c7ff9 and the exact renderer/context. The
existing Win64 assembly wrapper preserves arguments and return registers.
Before forwarding, the observer records the primary frame/view/epoch/bank,
unique stage-15/order-20000 uHazeFilter descriptor, named texture context slots,
helper rectangle/dimensions/offset, flags, and current queue count. After native
return it requires exactly one appended draw, key 0x78004e20, in the same frame
and queue. It does not write native data or change the call's result.

The callback reads the CPU pipeline recipe, vertices, input layout, bounded
VS/PS bytes, shader owner tables and packed binding ranges. It resolves the
pixel shader's t0..t3 owners through those ranges and cross-checks the named
context inputs captured at entry. It rechecks packet, pipeline, shader bytes,
vertices, owner metadata and frame identity before retaining a sample. Engine
texture metadata and source CPU descriptor handles are labeled as such; no
COM query, GPU descriptor contents or resource states are proven here.

Three successful samples are retained, at least 60 renderer frames apart, with
at most 16 attempts. Arrays cap owners at 64, binding words at 128 per range,
input elements at two, vertices at four and shaders at 16 KiB each. CPU pointers
are read only inside the callback and are never dereferenced later. Failed
observations are counted without changing rendering health or admission.
Samples are serialized by the existing worker under the route mutex. The hook
uses the existing pending-callback drain and removal sequence.

Tested host SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded comparison SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Two additional read-only function gates cover the haze callback (0x24c7760,
2256 bytes) and graphics binding loop (0x259f330,1664 bytes), using the hashes
documented in `scaled-haze-live-reader.md`. The number of hooks is unchanged.

The native synthetic fixture accepts three complete enqueue recipes and rejects
ten malformed cases, plus a foreign caller and capture-capacity overflow.
It checks wrong view, duplicate descriptor, wrong named source, changed frame,
wrong append count, vertex stride, shader identity, packed owner index,
mismatched named binding and failed native return. Capture rejection leaves
rendering health unchanged. The existing scaled route passed 96 continuous
frames / 288 effects / 786,432 exact pixels with GPU validation and retirement.
108 selected native tests passed. The new log reviewer passed four tests
(including 14 corruption subcases); 13 existing reviewer tests and the prior
2,271-frame live log replay passed. A separately labeled synthetic v1.25.8
replay verifies integration and rejects a missing haze channel.

## Deployment and next runtime test

Deploy only with the game closed through the evidence directory's `deploy.ps1`.
It verifies and preserves the exact installed v1.25.7 DLL and output-sidecar
bytes before replacement. It records the v1.25.8 hash and checks unchanged
game/profile/mode settings. `deploy.ps1 -Action Restore` restores those two
files after checking current and backup hashes; it does not change settings.
Keep the latest DLL installed after the test, as requested by the user.

Use the existing MO2 test entry, native AA/DLSS Off, unchanged window size.
Load outside the sauna and wait for arming and a healthy baseline. Then enter
the sauna and remain there with menus closed long enough to collect three
recipes. DLSS may still suspend there: this build observes the missing stage.
Confirm recovery outside the sauna, stop intentionally, verify GPU retirement,
jitter/full-scale/native-code restoration, then close normally and archive.

Next connect the observed geometry and bindings to the tested adapter, validate
the optional fourth effect's resource states and lifetime, and only then admit
that graph. Early CPU recording and panning-history decisions remain separate
issues. Performance work remains deferred.
