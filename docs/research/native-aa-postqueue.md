# Native post-processing queue activation — v1.20.2

Prepared 2026-09-27. Offline fixtures pass; no live success is claimed yet.
The qualified DLAA GPU replacement, input validation, history, and request
retirement remain byte-identical to v1.20.1. Final exclusive AA/failure policy
is still incomplete (`exclusive_mode=false`). CMFM/UI work remains deferred.

## Why this boundary

The v1.20.1 result proved camera-scope admission with AA Off, but no downstream
TAA preparation. Its worker-path explanation was incomplete: workers normally
register post-processing stages; the actual TAA preparation runs later on the
main render context. This agrees with the unchanged input validator and the
successful v1.19.1 TAA-selected runs. Adopting cloned worker contexts into that
validator would change a proven contract unnecessarily.

Pinned offline code shows camera render `0x228e770` calling the unit manager,
then postqueue `0x24f1fc0` at `0x228e928`. The manager joins its submitted jobs
and merges worker output before returning. TAA2 Execute `0x2392150` registers
itself via `0x24c9240(layer, 15, 2)` while queue collection is active. The queue
then clears collection, sorts entries, assigns their input/output resources,
and calls each layer's Execute directly at `0x24f2288`, passing the main context.
That direct call never passes through the old `0x2228de0` dispatcher hook.
These are static observations; no claim is made about which manager branch ran
in the previous live test.

## Implementation and lifetime

The camera scope still admits only the DX12 primary display/view, with no resize
or wrapper presentation active. The new postqueue hook requires this same thread,
main context, incoming view, bound primary view and exact global queue identity.
It executes after native worker completion and before native sorting/routing.

A bounded registry scan (maximum 1024 nodes, unique exact TAA2 vtable) obtains the
registered native layer each time. It retains the native state, render-enable,
context-mask, display-mask and category-mask gates; only AA-enable is supplied
by local registration. A malformed queue/registry or absent layer is rejected.
No pointers are retained across frames. This relies on the same native layer and
registry lifetime as camera rendering and postqueue execution.

The native queue has sixteen pointer slots at +0x40, followed by scratch targets
at +0xc0. All entries and the TAA descriptor ownership are read and checked before
mutation. Existing TAA registration is reused exactly once; otherwise the native
registration helper inserts it with its normal stage/order. Duplicate or full
queues are rejected. FXAA/TAA1/TAA3 entries are removed before native sorting and
resource assignment. Unrelated stages remain. Native preferences, layer enable
flags, worker contexts, cached job lists and original game files are untouched.
The registration helper does update transient layer descriptor metadata; telemetry
schema 2 therefore names the zero-write counter `layer_flag_writes`, not the older
ambiguous `layer_writes`.

Only an admitted TAA Execute inside this queue receives the saved-AA comparison
and unsharpened-output overrides. Worker registration calls remain native. Nested
queues suspend comparison authority, including when entered from an active
Execute. Stop retains an already-owned frame/queue to return; SEH finally blocks
restore TLS and comparison ownership and drain callback counts. Native exceptions
are propagated and latched as faults. Hook removal waits for scope completion.

## Hook contracts and fingerprints

All entry hooks use Windows x64 RCX/RDX/R8/R9, preserve volatile registers, flags,
XMM0–5 and original return values through the existing assembly forwarding frame.
The queue has three meaningful arguments: queue, context, view. The native caller's
remaining register value is preserved. CMP hooks replace one seven-byte instruction
and preserve registers; they resume before the native conditional branch.

| Hook RVA | Contract |
| --- | --- |
| 0x228e770 | Camera render(camera, context, incoming view, display); outer scope |
| 0x24f1fc0 | Postqueue(queue, context, view); register/filter before sort and routing |
| 0x2392150 | TAA2 Execute(layer, context); scope only inside admitted postqueue |
| 0x23931e0 / 0x2596090 / 0x25979b0 | Native TAA jitter callbacks; existing suppression |
| 0x239217d | Saved-AA CMP; owned TAA Execute only |
| 0x2392f8e | Sharpen/current-output CMP; owned TAA Execute only |

New whole-span SHA-256 gates:
- Postqueue 835 bytes: `1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190`.
- Queue access/register/reset span 176 bytes: `2965519c5b4feea0ed564a4bfce9c87a49def3e96433a7ce7e796791d270162e`.
- Native registration helper 43 bytes: `3907659e9c8215d943edde455a4b54fe655c5a52f6b1bfd9571ec43d71c7d58f`.
- Existing camera, TAA2 and jitter whole-function gates are retained in activation.cpp.

Host version 0.0.0.0 SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded-image SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Private disassembly: `local/native-aa-postqueue-20260927/`; never distribute it.

## Verification and next run

Release /W4 /WX clean; 35 native and 47 Python checks pass. The native-owned
fixture exercises actual ASM entry/gate hooks, worker prefilter and join, all four
saved choices, inserted/existing queue entries, retained native exclusions,
competing and unrelated stages, capacity/duplicates/registry cycle, other views
and threads, stop, nested queues/Execute and frame/queue/Execute exceptions.
A final nested-queue authority correction was rebuilt and its affected checks rerun.
Fixtures verify our contract, not the proprietary renderer or image quality.

Evidence/control root: `local/native-aa-ownership-20260927/v1.20.2/`.
DLL SHA-256: `f6a56c8ea444f1bd7bb4bef621b11fd02591048b6d154cef6fa14b76513795c9`.
Deploy only with the game closed; deploy.ps1 saves the exact previous slot and
output-dir sidecar, and `-Action Restore` restores them after exit. No settings
change is needed: native AA/DLSS Off, Windowed 2560x1440, DX12, bare offline profile.

On save-loaded: verify process generation, loaded modules, startup and source/config
seal, then arm within the existing five-minute startup window. Establish advancing
queue insertion/admission, both branch overrides and DLAA evaluations before travel.
If zero, queue reason counters identify: 0 queue identity/phase/count, 1 registry,
2 native layer gates, 3 descriptor/entry identity, 4 capacity, 5 reentrant queue.
Only a live run can verify resource availability and alignment with AA Off.
