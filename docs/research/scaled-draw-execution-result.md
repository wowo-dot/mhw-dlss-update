# Accepted native output-draw capture, 2026-09-28

v1.23.12 captured all nine selected native draws across frames13642-13644,
PID11952 / creation134350922451217513. The frozen reviewer accepted the capture.
The user reported a normal scene afterward and normal exit; process exit was
verified. The current diagnostic DLL remains installed as requested.

This repairs the v1.23.11 attachment/texture-owner handoff mistake described in
[the observer contract](scaled-draw-execution.md). All three stages were captured
together at native draw-wrapper execution without changing their commands,
pipelines, vertices or output resources.

## Confirmed observations

| Stage | Active viewport | Output allocation | VS bytes | PS bytes |
| --- | --- | --- | --- | --- |
| Motion filter | 1280x720 | 2560x1440 | 752 | 10008 |
| Color grading | 1280x720 | 2560x1440 | 752 | 5824 |
| Final screen | 2560x1440 | 2560x1440 | 644 | 1080 |

All outputs are DXGI_FORMAT_R11G11B10_FLOAT (26), single-sampled. Each stage
has one color attachment and no depth attachment. Layout is POSITION float2
at byte0 and TEXCOORD float2 at byte8, both slot0/per-vertex. Shader bytes,
input elements and native depth/blend/raster metadata remain identical across
the three observations of each stage. Eighteen shader files contain five
unique programs; all were bounded, hashed and disassembled locally.

The first two stages share the 752-byte vertex shader, SHA-256
`abddc634c053c157f295842964b023610ff64a7505d4b253edbe1612680ea937`.
Their pixel shader identities match the earlier motion-filter adapter and
color-grading analysis:

- Motion filter: `3eed9b57bbbdf38d14d9a94596fc93a107aa99eab275af7118b95febbbc1ba18`.
- Color grading: `174a09e5b23c7e01fb6c47ce86b61250d695113109abb4ffae7219117c31a8cb`.
- Final-screen VS: `730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd`.
- Final-screen PS: `be6127ba48008790fd0e93558b8248cb8acd7c0577899bf50d28e5618ae37b79`.

The final-screen vertex shader passes position.xy and UV.xy through, supplying
position.z=0 and w=1. Its pixel shader performs one explicit-LOD sample of t0
using sampler s0, UV input and CBSystem cb3[1].x (fSourceMipLevel). It does not
use SV_POSITION, depth or motion, and does not apply the gamma fields declared
in its constant-buffer metadata. The capture does not include the live mip
constant value or descriptor-table contents.

Inference for integration: retain the native final-screen shader and sampling
constants, provide full-output source coordinates with the native half-texel
convention, and avoid rescaling the reconstructed image from a half-size active
rectangle. Color grading can likewise retain its native pixel shader with
full-output viewport/UVs. The already-tested motion-filter adapter handles the
different color versus low-resolution auxiliary coordinate bases.

## Verification and limits

The capture reviewer verified three input triplets, including19,353,600 exact
converted float comparisons and GPU completion/retirement for the existing
input-copy path. All nine added draw records passed their CPU wrapper,
attachment, packet, shader and native cached-state checks. Those new records
do not certify GPU submission/completion, an API-captured PSO creation recipe,
or descriptor-table contents. No DLSS evaluation or full-output handoff occurred.

Cleanup completed: request pins24/24, native-compute pins3/3, submission
pins309/309, depth-route pins36/36, no retained submission references, no pending
callbacks/faults, one jitter injection restored, full render scale restored,
activation removed and all hooks disabled. The settings, profile, mode,
installed DLL, previous-DLL backup and output sidecar passed identity checks.

SEALED evidence: `local/scaled-temporal-20260928/v1.23.12/` (605files).
Archive SHA-256:
`f66aea915701e54fc90062bd5a4147f8230247cd359ff5e3c55240aa743981bc`.
Final/captured log SHA-256:
`0d95398561970f573beb61aea806c9f7d8e92cc36d5f23b52a5531cdfb599ab3`.
Installed DLL SHA-256:
`83fbad2cc7c41464be83f2997ab57e69b2a5473aaac76d142a0c402d4efbf9ae`.

Do not modify that archive. The subsequent [full-resolution native-effects
implementation](scaled-full-effects.md) now passes offline GPU tests, including
the scaled SDK resolve and motion-filter adapter. Live output/copy routing is
the next integration step. No new test DLL is prepared yet.
