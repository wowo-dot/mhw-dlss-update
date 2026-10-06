# Native temporal preparation independent of saved AA — v1.20.0

Implements scoped renderer activation around the verified native TAA2 stage.
The player's saved AA selection and the native option table remain untouched.
The diagnostic can admit TAA2 preparation with native AA Off, exclude competing
AA stages before queue registration, retain one jitter owner, and bypass native
sharpening together with its next-ring output selection. The qualified DLAA
resolve and validated request-retirement implementation are unchanged.

This is built and checked offline, pending the first live AA-Off run. It is not
yet complete renderer-wide exclusive-AA/failure handling: unqualified GPU
dispatches and publication-blocked callbacks still use the v1.19.1 native path.
The header continues to state `exclusive_mode=false`. CMFM/UI work is deferred.

## Ownership and integration

The native camera-render function encloses projection, unit rendering, queue
registration and final post-processing execution. A wrapper admits only the
known primary DX12 context, camera classes and compatible native backend. It
owns that call until normal or exceptional return, including after an explicit
diagnostic stop. An interlocked context token rejects overlapping ownership;
thread identity prevents another callback from inheriting the scoped gates.
No game object pointer is retained for later mutation.

Within that call, the unit render dispatcher applies its original state, render
flag, mask and display gates. For TAA2 only, AA-enable bit 1 is supplied as a
local decision instead of requiring it in the object's flags. FXAA and the two
other known native TAA types are excluded before they can register post-process
nodes. Other units and contexts forward through the original dispatcher.

TAA2 Execute separately checks the shared table's AA setting. A leaf hook at that
comparison changes only the branch result for the owned context and render
thread; it does not change the table or bypass the subsequent view exclusions,
resource preparation, reset initialization, scheduling or final routing.

The native TAA jitter callbacks are suppressed while activation is requested
(or a previously owned call is finishing), leaving the existing scoped modern
jitter path as owner. Native allocation and reset callbacks remain unchanged.
The sharpening comparison is overridden to choose the existing non-sharpening
branch. That branch routes the current ring entry, avoiding an unwritten next
entry. Neither the layer's jitter-enable nor sharpening byte is rewritten.

Stop refuses new ownership and waits for existing calls to drain before hook
disable. The current diagnostic then resumes native behavior according to the
unchanged setting. This intentional stop is distinct from the still-pending
final feature's latched-unfiltered failure policy.

## Reviewed boundaries

All locations are version-specific image-relative gates. The runtime requires
the pinned executable and complete reviewed function hashes before enabling
them; it does not modify the disk executable. Host version 0.0.0.0, SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded comparison SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

| Hook | RVA | Contract |
| --- | --- | --- |
| Camera render | `0x228e770` | Windows x64 RCX camera, RDX context, R8/R9 original arguments; preserve integer/vector arguments and results, clear scope in SEH finally |
| Unit render dispatcher | `0x2228de0` | RCX unit, RDX context; exact 61-byte leaf gates; forward unrelated calls, retain all gates except the owned AA-enable choice |
| TAA2 table comparison | `0x239217d` | One 7-byte CMP; RAX table, RDX context; owned result ZF=0, resume `0x2392184`; native trampoline otherwise |
| TAA2 sharpen comparison | `0x2392f8e` | One 7-byte CMP; RBX layer, `[RBP+0x98]` context; owned result ZF=1, resume `0x2392f95`; native output branch chooses current ring |
| Native jitter updates | `0x23931e0`, `0x2596090`, `0x25979b0` | RCX exact TAA2/TAA1/TAA3 object; suppress only the reviewed jitter callbacks, forward when inactive or type does not match |

Both comparison hooks preserve GPRs and XMM registers. Only the native CMP's
branch flags are replaced. Thread qualification uses the same Windows x64 TEB
self pointer as `NtCurrentTeb`; the fixture also checks a second thread using
the same context pointer cannot inherit the override. The hook DLL/trampolines
stay pinned; no runtime code or owned GPU state is unloaded in this diagnostic.

Private disassembly, full hashes and the table-reader survey are under
`local/native-aa-ownership-20260927/`. Direct-displacement searches are a bounded
static survey, not a claim to enumerate every indirect option access.

## Offline verification

Release `/W4 /WX` builds pass. All 35 focused native checks pass, including the
actual new ASM wrappers and MinHook comparison detours against owned fixtures.
The new fixture checks all four AA choices; preserved settings/layer flags;
retained type/render/mask/display exclusions; unrelated-call return preservation;
FXAA/TAA1/TAA3 scheduling exclusion; native jitter suppression; thread isolation;
stop during rendering; preservation of a setting changed during rendering; and
SEH cleanup. This uses no proprietary instructions or live game process.

Twelve new Python review checks consume actual fixture receipts, reject corrupt
partitions/types/closure and exercise the new version through the full reviewer.
The full-review fixture reuses private prior GPU/startup fields with synthetic
activation metadata; it is not a v1.20.0 live result.

Versioned `activation` telemetry records choices observed at frame entry, TAA
admission/exclusion, competing-AA and jitter suppression, both branch overrides,
scope state and cleanup. Frame-choice/admission partitions are synchronized;
ASM gate totals are independent monotonic counters. Saved-file identity is checked
outside the process in addition to the runtime's no-write implementation.

Next live check: keep native AA and DLSS Off, load a save, verify that activation
observes saved Off while DLAA replacements advance, then inspect panning, sauna
and an area load. Do not label AA independence proven from these fixtures alone.
The final exclusive resolve/suspension/failure path remains a separate integration
requirement before declaring the renderer ownership milestone complete.
