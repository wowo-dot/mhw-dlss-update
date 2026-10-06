# Native upscaling handoff: bounded static audit

2026-10-01. The user requested only a quick check of existing DLSS/CAS machinery,
keeping the current implementation as the baseline. No runtime hook, setting,
build, installed DLL or admission policy changed for this audit.

## Decision

Continue the current Gallery integration. This inspection found an explicit
native spatial upscaling output, but did not establish a smaller, compatible
replacement for our temporal integration or a general downstream size switch.
This is a bounded negative finding, not proof that no better native route exists.
Do not spend additional user test runs exploring it without a concrete shortcut.

## Observations

- The previously mapped NGX sizing helper at RVA `0x229a8c0` needs the native
  wrapper, obtains sizing through `0x2383ef0`, combines it with the renderer
  scale and writes renderer `+0x200`. The constructor/update machinery using
  the coupled allocation and active-view scales is already part of our sizing
  research. See [render-resolution control](dlss-render-resolution-control.md).
- The main-view constructor `0x255ba70` conditionally allocates `UpscaleOut`
  into view `+0x6a0` when `+0x6a8` is enabled. It supplies unscaled input display
  dimensions to that allocation. The reconfiguration helper `0x255cfb0`
  similarly replaces/removes this output. This is real native output machinery,
  not just an options-menu string.
- CAS helper `0x23c2620` consumes one caller-supplied color texture and the
  current rectangle. It reads output dimensions from that `+0x6a0` owner,
  constructs input/output ratio and sharpening constants, binds `CSCASFilter`,
  `CBCAS`, `tCASInput`, `tCASOutput` and `SSSystemCopy`, records a compute request
  through `0x23c9980`, then returns the output owner. Without the output owner
  it returns the input. This helper does not supply a temporal depth/motion/
  history contract or reconfigure the preceding postprocessing stages.
- In the large renderer recording function rooted at `0x23c3790`, the CAS call
  at `0x23c7829` follows the phase-15 recording block and phase-16 selection.
  Its result becomes `tBaseMap` for `System_Copy`. This supports interpreting
  the discovered CAS boundary as a late color upscale/copy path. It does not
  establish pixel contents, color space, UI exclusion or live GPU submission
  ordering at that boundary.
- The same enclosing function also has a **separate** branch using
  `CBTemporalUpsample`, `CSTemporalUpsample`, jitter, history, G-buffers and
  density textures. CAS's presence must not be used to claim that the whole
  native renderer has only spatial upscaling. The runtime activation, quality,
  native shader input contract and suitability of that separate branch are
  unverified by this quick audit. It is a parked lead, not a reason to pivot.
- A nearby NGX-wrapper-dependent opcode-23 branch was observed. Its proximity
  to CAS does not identify it as the old DLSS temporal evaluation; that identity
  was not assumed.

## Implications

CAS's output allocation is potentially reusable, but substituting modern DLSS
at a later color-only boundary would still require proving correct temporal
inputs and placement relative to effects. Keeping our early temporal output
while pointing it at `UpscaleOut` would likewise not automatically update the
coordinates of already prepared blur/overlay passes. Neither is a demonstrated
shortcut for the Gallery issue.

Keep the verified TAA-based temporal inputs, SDK implementation and sizing route.
Resume the offline Gallery experiment using captured constants and native blur
shaders. The preparation adapter was an unbuilt draft at the time of this audit;
the subsequent [offline replay](dof-coordinate-replay-result.md) now passes on
both devices. It has not changed live rendering or admitted the six compute passes.

## Evidence and limits

Read-only private artifacts: `local/native-upscale-audit-20261001/`.
The host and decrypted research image were rehashed before deriving references:

- Host version `0.0.0.0`, SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Decrypted research image SHA-256
  `d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

`audit_native.py` saves bounded disassembly and per-span hashes, with direct-call
scan candidates explicitly distinguished from a whole-image control-flow proof.
CAS's chained unwind records resolve to entry `0x23c2620`; the larger caller's
chained records resolve to `0x23c3790`. Individual unwind fragments are not
treated as independently callable functions. Private binaries/instructions
remain ignored. There was no new game launch or live behavioral verification.

Installed v1.25.36 DLL SHA-256 remains
`354128a6bdaa91ac488cb3c5919f7f0a404948010990f038ac8abd05d61d56b7`.
The closed v1.25.36 archive remains sealed. A fresh output root is required before
the next game launch, as recorded in the [last result](dof-pipeline-constants-result.md).
