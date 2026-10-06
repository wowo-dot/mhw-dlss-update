# DLAA output handoff and native bindings — v1.16.0

Completed: [all nine live binding snapshots accepted](../research/native-binding-snapshots-result.md).
Run 36704 also passes the nine-frame offscreen SDK review. Normal exit and exact
rollback are complete. The following describes the frozen experiment.

The first output handoff implementation is in `src/temporal_present/`.
It packs private RGBA32_FLOAT DLAA output into R11G11B10_FLOAT, copies it to
the native output texture, and restores the compute bindings and graphics
descriptor tables affected by that work. WARP and RTX fixtures verify the
result and subsequent compute/draw operations. **It is not yet called in the
game.** The next live DLL observes native bindings while retaining the accepted
nine-frame offscreen experiment.

## Why a binding recorder is needed

The accepted evaluator runs on a private queue after the native input copies
have completed. Presenting those saved images later would not establish a
same-frame integration. The live path must convert the matching inputs,
evaluate DLAA, and replace the output at the selected TAA pass, while preserving
the state expected by the next native operation.

Native helper `+0x259CC20` caches its PSO at recording-object `+0x108` and its
compute root signature at `+0x118`. The latter must equal backend `+0x2A0`.
The helper conditionally rebinds those objects and always sets root descriptor
table zero before Dispatch. Therefore the injected pass cannot rely on the
game to rebuild all bindings afterward. Changing descriptor heaps invalidates
both graphics and compute descriptor tables; see Microsoft's
[descriptor heap rules](https://learn.microsoft.com/en-us/windows/win32/direct3d12/setting-descriptor-heaps)
and [root signature rules](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-a-root-signature).

Tested host: version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The existing executable/code gates still apply. These cache offsets are checked
inside the existing hash-validated native consumer, not used as independent
portable signatures. The private command-list vtable is resolved through the
existing system-module validation; no new absolute driver addresses are used.

## Handoff contract

`Output` is prepared before recording. It accepts distinct, same-device,
full-size, single-sample/mip/array textures: RGBA32_FLOAT source in COPY_SOURCE
and native R11G11B10_FLOAT destination in UAV state. The caller must establish
these states and a matching frame. The source is shader-readable, and the
destination allows UAV use. Recording is one-shot on a DIRECT list.

The compute pass uses private descriptors and a private packed texture. It
clamps negative/overflow values to the unsigned native format's finite range;
there is no tone mapping or exposure adjustment. Explicit nearest-even
quantization precedes the typed store, because narrower floating-point stores
may truncate under the [Direct3D conversion rules, §3.2.2](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm).
The packed texture transitions to COPY_SOURCE, the destination to COPY_DEST,
and CopyResource replaces the native output. Source and destination return
to their entry states. Private resources stay retained until a completed
same-device fence and a caller-certified recording-retirement witness exist;
uncertain issued work retains resources for process lifetime. The caller must
prove that the supplied fence actually covers this recording.

`RestoreBindings` pins the observed list, PSO, roots and heaps. It restores
the compute root arguments, shared heaps/PSO, and graphics descriptor tables.
This covers the state changed by `Output::record`; it does **not** yet certify
all possible NGX SDK state changes. SDK insertion and state coverage remain
separate integration work.

## Read-only binding observation in v1.16.0

`mhw_temporal_state_probe` adds 18 D3D12 method hooks to the existing 11-hook
recorder. Each hook uses the documented Windows x64 COM ABI, with the command
list in RCX and the original arguments forwarded once. The methods are:

| Vtable slots | Methods |
| --- | --- |
| 11, 25, 27, 28 | ClearState, SetPipelineState, ExecuteBundle, SetDescriptorHeaps |
| 29–32 | Compute/graphics root signatures and descriptor tables |
| 33–36 | Compute/graphics single and multiple 32-bit constants |
| 37–42 | Compute/graphics CBV, SRV and UAV root descriptors |

Only successful, validated DIRECT-list resets admitted by the existing
submission journal start an observed generation. That journal pins every
registered list through the interval. Reset notification occurs before its
pending callback is released. The shadow state contains at most 128 lists,
two heaps, 64 root arguments per stage, and 64 constant words per argument.
Successful setters update the shadow after forwarding. Partial constant writes
accumulate; a changed root clears its arguments; changed heaps invalidate
tables in both stages. ExecuteBundle invalidates a generation until Reset or
ClearState because inherited bundle changes are not reconstructed.

At each selected native Dispatch, an immutable snapshot must match the
engine's cached root and PSO. The review also requires the exact frame, command
list and journal Reset generation, valid table zero, bounded arguments and
complete observer shutdown. Snapshots are POD identities and constant values;
the observer does not dereference captured root/PSO identities or retain raw
process-memory dumps. Native pointers must be independently pinned/validated
before any future use for restoration.

Callbacks drain before hook removal; the journal cutoff remains ahead of
unhooking. Trampolines stay retained in the pinned DLL. Missing state stops
acceptance and prevents offscreen SDK evaluation. Native calls still forward.
The recorder does not install game debug layers or change native root bindings.
The existing seven scoped jitter injections and bounded resource copies remain
part of this experiment. `presentation_writes` is zero.

## Verification and live procedure

The Release targets use `/W4 /WX`. The 157 temporal native tests pass, including
the new binding model, a real API-hook output fixture, and nine-frame native
recorder fixtures with packet reuse and a pending shutdown callback. The three
combined recorder reviews and the binding reviewer reject incomplete or stale
snapshots. The unchanged accepted v1.15.3 live capture still passes the updated
full reviewer.

On the RTX adapter, the first/middle/final accepted DLAA images each produce
3,686,400 exact reference-matching packed pixels. Subsequent compute and graphics
passes use the restored bindings successfully. Inputs remain byte-identical;
GPU validation reports zero warnings/errors. This establishes the handoff
primitive in an owned fixture, not native in-game presentation or image quality.

Evidence and deployment/restore script:
`local/dlaa-bindings-live-20260926/v1.16.0/`. The preparation manifest records
source/binary hashes, the prior accepted archive, SDK identity and native code
gates. Installation preserves the exact destination DLL and sidecar and leaves
the four settings/profile files unchanged. `deploy.ps1 -Action Restore` restores
the previous DLL once the game is closed.

Use the existing bare MO2 profile and diagnostic entry, DX12 On, DLSS Off,
TAA On. Load the save, keep the camera still, and report readiness from the
phone. Arm once for the bounded nine-frame capture, preserve and review the
evidence, then exit and restore. Do not run it in public multiplayer. There is
no expected visible DLAA change in this binding-observation run.

After accepting the live binding snapshots, implement matching-frame input
conversion/evaluation and the output replacement with native fallback. Then
extend to continuous history, real frame timing, camera cuts, menus, loading,
alt-tab, resolution changes and device/swap-chain recreation.
