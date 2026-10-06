# Scaled post-filter construction capture

Prepared 2026-09-28, v1.23.9. Runtime evidence is pending.

The accepted v1.23.8 capture establishes native TAA's reduced rectangle and
the final stage-16 draw's reduced source UVs. It does not establish execution
of the intervening filter, or that differing engine owners imply differing
GPU resources. The graphics configuration has MotionBlur=Off, but that alone
does not establish a pass-through filter. Do not discard the filter or expand
its color rectangle based on that setting alone.

The planned full-size DLSS handoff remains unimplemented. Before changing the
route, this build captures the actual stage-15/order-100 execution on the same
three qualified input frames: input/output engine ownership, the final draw's
source ownership, both draws' referenced vertices, queued commands, and the
filter pixel shader. These shader bytes stay in ignored local artifacts.

## Pinned native contracts

Host: version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Offline decrypted source SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Original executable/assets are never modified.

- New hook: RVA `0x24ca1f0`, the Execute slot at vtable `0x3537a70` + `0x60`.
  Windows x64 ABI: RCX filter instance, RDX native render context. Full volatile
  register/flags preservation uses the existing assembly forwarding frame.
  Only the postqueue caller returning to RVA `0x24f228b` participates; earlier
  registration calls and other views forward unchanged. All native calls,
  returns and structured exceptions are forwarded.
- Execute's full 792-byte span is gated by SHA-256
  `ace3852211fb63c13cfca395d4992f922addc69db0841fa44e540819779e60aa`.
- Native draw producer RVA `0x23ca4a0`, full 445-byte function through its ret,
  is gated by SHA-256
  `97d8e82e322796881438a9318a4f5170401bf031b28d21afd19c77256a82816e`.
  An earlier offline export included padding and the next function's prefix;
  that larger span is not used as this gate.
- Existing downstream gates still cover the final quad builder, its caller,
  and the complete postqueue assignment/execution function.

The known draw producer records an opcode-0 packet with vertex-state pointer
at +0x18, graphics-state pointer at +0x20, count at +0x40, and first vertex at
+0x44. The vertex state has stride at +0x20 and arena offset at +0x24. CPU
vertices are addressed as context+0x2b50's pointer plus offset plus first*stride.
Only stride-16 triangles/quads are read (48/64 bytes); no surrounding arena is
dumped or changed. Final geometry is re-read after the filter returns.

Graphics state+8 leads through the shader handle to the shader set. Set+0x58
is the pixel-stage entry; entry+0x10 points to size/data metadata. The bounded
DXBC is structurally checked, required to contain a pixel SM5.0 program, read
twice, and checked against unchanged metadata. This layout is shared with the
existing projection-binding graphics shader recorder.

## Ownership, limits and cleanup

Selection occurs in the same native CPU frame/thread at the previously
qualified TAA final-copy boundary. The downstream filter must be the observed
stage-15/order-100 unit. Before and after its Execute, frame/epoch/bank/view,
engine owners, referenced final vertices and context rectangle must match.
Each selected Execute may append at most 32 commands; exactly one opcode-0
draw at that stage/order is expected. A mismatch preserves partial evidence
and stops the diagnostic. It never suppresses native rendering.

Only three records, three pixel shaders (262,144 bytes each maximum), and the
four scalar property words at unit+0x168 are retained. Property words are raw
bits, not asserted meanings. Engine resource addresses do not independently
prove COM identity, format, resource state or GPU execution. No COM resource
is used or held by this observer. Native owner/packet pointers are inspected
only during their same-frame construction lifetime and never dereferenced
later for saving.

Completion includes return from all three selected filter calls. Admission
closes before drain and hook removal; the DLL and trampolines remain pinned.
The new observer has zero mutations and adds one hook (25 total). Existing
half-scale and scoped jitter diagnostics are restored as before. No SDK
evaluation or full-resolution presentation replacement is enabled.

The native fixtures exercise the actual assembly entry, selection/completion
ordering, return forwarding, SEH, stop during callback, nested callbacks,
unreadable/changed vertices, wrong stride, changed frame/owner/viewport,
foreign caller/thread, missing draw and oversized shader. Python checks
independently verify saved bytes, frame/owner/packet links, final UV arithmetic,
cleanup, budgets and tampered evidence. These fixtures do not simulate native
post-filter GPU execution or establish that MotionBlur Off is a color copy.

Next decision: inspect the actual captured shader and route. Full output needs
both correct copy/consumer extents and final source coordinates, with a valid
full-output failure path. A partial active-region copy cannot supply a full
output texture. Preserve the native temporal inputs and post-processing effects.
