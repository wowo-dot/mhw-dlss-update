# Same-frame DLAA handoff — v1.17.0

Status: [first live one-frame handoff accepted](../research/first-inline-dlaa-result.md).
Normal exit and exact rollback are complete; continuous DLAA remains unverified.
This follows the accepted [native binding capture](../research/native-binding-snapshots-result.md).

`mhw_temporal_inline_probe` retains the nine-frame input/jitter recorder and
replaces exactly receipt 4 (the fifth frame). DLSS stays **Off**, TAA stays **On**,
and the bare offline profile stays selected. The original native TAA Dispatch
forwards once before the injected work. Other frames execute native TAA; the
modified texture may also become native temporal history. This is a one-frame
handoff diagnostic, not continuous DLAA or a quality/stability certification.

## Recording and ownership

The existing Windows x64 D3D12 Dispatch hook (list in RCX, group counts in
EDX/R8D/R9D) still matches the native helper at RVA 0x259CC20 and return site
0x259CFCA. No additional hook targets are introduced: 29 total, including the
18 binding-observer methods. The hook's thread-local injection flag forwards
our Dispatch/ResourceBarrier calls without counting them as native calls. The
binding observer continues observing those calls and must match the complete
pre-injection root/PSO/heap state after restoration.

Tested executable version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The existing 51 native and four startup code spans remain mandatory. Original
game binaries/assets are never patched on disk.

Preparation runs on the worker before capture admission. It borrows the
verified startup NGX initialization on the game device, creates one full-size
DLAA feature on a private DIRECT queue, and waits for its completion and list
retirement. It does not call NGX Init or Shutdown again. The runtime/SDK pin is
unchanged (310.7.0, preset K, full-resolution mode, flags 75).

At receipt 4 the journal requires that all three private raw copies belong to
the same frame, packet, command list, allocator and Reset generation. Depth
must precede the TAA consumer; color/motion must precede its original Dispatch.
The output must be a matching R11G11B10 UAV. The projection record must already
be published, acknowledged and restored. SDK jitter is the matching native
projection jitter mapped to input pixels `(x/2, -y/2)`.

After the native Dispatch, the same list records:

1. Raw R11G11B10/R32_FLOAT/R32_UINT conversion to RGBA32/R32/RG32_FLOAT.
2. One reset DLAA Evaluate, with motion scale 1, exposure 1 and diagnostic
   16.666666 ms. There is no retained SDK history across frames in this test.
3. Explicit nearest-even packing to R11G11B10, then a copy to native `rwOutput`.
4. Readback copies of the converted inputs, SDK output and packed native result.
5. Restoration of both root signatures and their recorded arguments, descriptor
   heaps and PSO. Raw copies return to COPY_DEST; native output returns to UAV.

Private image allocations (including the output packer and readbacks, excluding
SDK-internal allocations and previously budgeted raw capture sets) are bounded
to 400 MiB. At 2560×1440 they total 365,690,880 bytes. Submitted resources stay
retained through a fence for that exact native recording and its subsequent
Reset retirement. The journal transfers an extra fence reference before its
cleanup. Collection verifies that witness before mapping or releasing the NGX
feature/parameters. Uncertain issued work retains its resources for process
lifetime. Native game state and COM lifetimes otherwise follow the existing
capture ownership rules.

Missing preconditions skip insertion and reject the capture. An SDK API failure
before handoff restores bindings and leaves the original native output in place.
GPU errors or bad image contents can only be diagnosed after submission; this
bounded experiment does not claim to recover from arbitrary SDK/device faults.

## Verification and limits

The hardware fixture uses archived native pixels and the observed root layout:
compute tables 0/1 and CBVs 2/3/5/6; graphics tables 0/1/16 and CBV 5. Both
descriptor heap types are bound. Subsequent compute and graphics passes reuse
those bindings, viewport/scissor, render target and primitive topology. GPU
validation checks the executed command list; every output pixel is compared
with independent arithmetic packing and every converted input with a separate
GPU conversion. Missing-state, wrong-output and wrong-jitter cases verify
unchanged native pixels. The fixture's frame metadata is synthetic and makes
no claim about temporal image quality.

The live reviewer additionally binds the startup ownership, actual projection
jitter, selected native texture, exact recording/fence, all nine native binding
snapshots and all five hashed output files. It compares conversion to the
independently verified raw capture and packing to the SDK result. Continuous
history ordering, real frame timing, cuts, menus, alt-tab, resize/recreation and
visual quality still require later work.

## Local deployment and removal

The versioned local evidence folder contains `deploy.ps1`, a source freeze,
prelaunch manifest and `previous-slot.dll.disabled`. Installation replaces only
the explicitly authorized MO2 diagnostic DLL slot and its output-directory
sidecar, preserving the exact previous file. With the game closed, run that
script with `-Action Restore` to restore the previous DLL and sidecar state.
No graphics, save, profile or original game files need changes.
