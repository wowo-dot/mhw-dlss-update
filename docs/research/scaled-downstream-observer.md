# Native downstream scaling observer

2026-09-28, v1.23.8. Offline mapping and a prepared diagnostic; live
qualification is pending. This extends the accepted v1.23.7 final-copy capture.

## Finding and proposed handoff

The primary view records a `System_Copy` draw at call site RVA 0x23c7b03
(return 0x23c7b08), targeting the attachment at view+0x460. The source is bound
as `tBaseMap`; the ordinary branch starts with the owner behind view+0x420.
Other branches can replace that source, so the recorder reads the actual
binding instead of assuming it. The helper at RVA 0x2297e20 generates UVs from
the supplied source rectangle, allocation dimensions and two-float offset.
The destination viewport is context+0x5c.

This call is inside view preparation, before CPU execution of the native
post-filter queue. That CPU order does **not** establish GPU order: the draw
is tagged stage 16; native TAA is stage 15/order 2. The native queue append
routine at RVA 0x23cabf0 packs the stage into bits 27–31 of the command key.
The filter queue assigns input/output owners before executing its units and
can contain filters after TAA. Thus expanding only TAA's final copy remains
insufficient evidence of a correct full-resolution handoff.

The proposed integration must carry the reconstructed image through those
filter owners and make the final copy sample the full result. The exact live
chain and draw geometry are the next capture's question. No output rectangle,
UVs, native texture data or native filter entries are changed by this observer.

## Hook and bounded association

Windows x64 ABI for RVA 0x2297e20:

- RCX: renderer; RDX: context.
- R8: four signed/native nonnegative source-rectangle integers.
- R9: two allocation-dimension integers.
- Stack argument 5: two-float UV-offset pointer; argument 6: triangle boolean.

The ASM entry preserves registers, return values and the two stack arguments;
the existing forwarding bridge propagates exceptions. Selection is limited to
the exact return site, primary renderer context/view, frame, epoch, bank,
asynchronous flag and thread. One thread-local candidate is overwritten for
each qualifying draw. Only the three already selected TAA frames enter the
retained records. Both owner metadata and the appended command records are
revalidated at selection. No engine pointer is used to invoke a virtual method.

Each candidate reads at most eight newly appended command pointers, opcodes and
sort keys. The selected frame also records at most sixteen native filter entries
with stage/order, unit/vtable, input/output owners and three flags. The TAA entry
must be layer+0x140, stage 15/order 2, with the already qualified input and
final-copy destination. All scalar reads remain at most eight bytes; arrays
are read one scalar at a time. No indiscriminate memory dump is taken.

These are engine metadata snapshots, **not new COM lifetime pins or a GPU
execution trace**. They do not prove GPU resource state, draw consumption,
presentation or successful DLSS. Existing final-copy pins and the three-frame
input readbacks retain their previous independent checks. The observer closes
admission, drains callbacks and disables its hook with the parent recorder.
The pinned diagnostic DLL and MinHook trampoline remain allocated until exit.

## Version gates and verification

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Captured decrypted executable SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Both identities are checked before offline instruction exploration.

New exact live byte gates:

| RVA | Bytes | SHA-256 |
| --- | ---: | --- |
| 0x2297e20 | 656 | 58fe604c883de6d505ce2211797c2efaddc360046d10bb14c2e50fafbae4ab6d |
| 0x23c7831 | 740 | 0da63ff39cc114c78483728e19221e1dd22806398522ec0095ec6c6349801247 |
| 0x24f1fc0 | 835 | 1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190 |

Offline instruction exports remain ignored under
`local/scaled-handoff-20260928/`. Partial unwind regions are not treated as
complete functions. No captured executable or instruction export is committed.

The build is warning-clean. 117 native checks passed, including nineteen new
cases using the real ASM entry and an owned native-shaped function. They cover
register/stack forwarding, SEH, nesting, cancellation, foreign callers/threads,
bad addresses, changed frame/viewport/owners/packets, and filter bounds.
Two retained combined WARP captures passed GPU validation and the previous
live v1.23.7 capture passed the updated reviewer. Four downstream reviewer
checks include tampered identity, geometry, ordering, bounds and cleanup.

The standalone fixture normalizes its actual ASM return address only when
feeding the production code-location reviewer; raw fixture evidence is retained.
The two WARP fixtures retain their existing input/final-copy scope. They do not
simulate the game's downstream draw or claim end-to-end GPU scaling coverage.

## Runtime procedure and removal

Use the same bare, offline MO2 entry with native AA/DLSS Off, DX12 On and the
unchanged window size. Load a save and keep the camera still. Verify the fresh
PID/creation, DLL hash and v1.23.8 header before arming. This remains a short
three-frame capture with half active render scale, one restored jitter sample
and no NGX evaluations. All 24 hooks must be disabled and full scale restored
before accepting shutdown. Check the scene and exit normally afterward.

Deployment preserves the exact previous DLL and output-directory sidecar in
`local/scaled-temporal-20260928/v1.23.8/`. Its `deploy.ps1 -Action Restore`
checks installed and backup hashes before restoring only that DLL/sidecar.
Do not run it while the game is open. The latest DLL stays installed after
testing unless the user changes that preference. Original game files and
graphics/profile/modlist/mode settings are not modified by deployment.
