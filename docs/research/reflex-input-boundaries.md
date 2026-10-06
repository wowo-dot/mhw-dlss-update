# Reflex input and submission boundary capture

The persistent SDK Off presentation route has passed. This separate v0.2.0
observer prepares semantic placement of sleep and frame markers; it does not
enable Reflex, initialize a vendor SDK, or change rendering. The accepted
presentation binary and its source/evidence remain archived separately.

## Confirmed offline observations

Retail executable SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Private decoded research image SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Assembly and decoded bytes stay ignored/local; this document contains only
structural observations and qualification metadata.

Inside the complete engine update, bank advancement precedes the keyboard,
mouse and input-device subsystem updates. The same order exists in both its
sequential and worker branches. The keyboard path calls a platform routine
which rotates current/prior bit sets, invokes GetAsyncKeyState and a device
polling helper. The mouse path polls cursor/device state and derives input
edges. The input-device subsystem invokes its base refresh before consuming
that state. These establish input refresh candidates; they do not by themselves
prove which values affect a particular presented frame.

The engine worker-count field selects parallel scheduling. The batch routine
`11da7a0` builds a job list and calls `225e840`; that routine executes jobs and
waits for the scheduled worker completion count before returning. By contrast,
the late `221bea0` / `221a890` tasks call `221bf40`, which sorts linked job lists
and redistributes work. An outstanding late task is therefore not automatically
evidence of outstanding game simulation. The observer captures two bounded
worker plans to identify which functions actually overlap recording.

`229a080` begins command recording. `22980b0` includes waits, backend submission
and presentation. The narrower `23a7f50` wraps backend submission and returns
before presentation. Engine-update return is after Present and cannot be used
as SimulationEnd. Proposed sleep at engine entry, SimulationEnd/RenderSubmitStart
at qualified recording entry, and RenderSubmitEnd at qualified backend-submit
return remain hypotheses pending the new gameplay trace.

## Additional hook qualification

The observer retains the seven already tested frame/submission boundaries and
adds six full-function hash gates:

| Kind | RVA | Bytes | SHA-256 | ABI/role |
| --- | --- | --- | --- | --- |
| Keyboard refresh | 485d90 | 606 | `22a594985eff78bbbe182a98c59ee6ef8d6514b8d9c07b34183902dc736570cd` | Win64, subsystem `this` in RCX |
| Mouse refresh | 489800 | 1626 | `5fee2e4f12148a329c4cad0b327c91bf6f7ef4090d485c33447c54f5a2f48398` | Win64, subsystem `this` in RCX |
| Input-device refresh | 1b168c0 | 700 | `919becfc62d4fac31843de4bf06eaffa2888fe89de7870cd4eeb893cb00ced34` | Win64, subsystem `this` in RCX |
| Synchronous job batch | 11da7a0 | 177 | `77f1dcb493093ec6923959c706275f8665f8af55cb14a20a3b4f42a272fae527` | Win64, job-list owner in RCX |
| Backend submit | 23a7f50 | 69 | `1183e1807b99ec2ada753fa147af8c59b1f99f29bbbf6176ed7479fd48e09a46` | Win64, backend `this` in RCX |
| Base device poll | 22a5040 | 1296 | `2c2964093037a1ede0f3fddcbea44ff0c9029df1e66b12717c6468bdcdb04424` | Win64, same input-device `this` in RCX |

These routines receive no stack arguments. The shared assembly wrapper preserves
volatile integer/SSE arguments, results and flags; SEH forwarding preserves
exceptions and LastError. The expanded ABI fixture covers 2,048 integer/vector
cases and exceptional cleanup. Do not reuse it for a target with stack arguments.

Before arming, full native hashes must match. The engine global must link to
the observed renderer; its three input-subsystem vtable slot-six functions
must match the gated targets. Calls log whether their RCX matches the qualified
owner. Native device/swap-chain COM identities are validated and pinned for the
capture. Input/engine pointers are borrowed, never retained or mutated; pointer
identity changes invalidate qualification. Test one stable loaded scene.

## Capture and cleanup

Target: `mhw_reflex_input_probe`, schema 2, version 0.2.0. Unique controls use
`Local\MhwReflexInput-v2-{pid}-{creation}-{phase}`. Launch only creates a log,
reserves trampolines and waits for arming. The capture lasts 15 seconds, retains
at most 262,144 paired records, and writes them after hooks stop. It records
frame stamps, callers, timing, worker counts/pending masks, and at most two
64-slot worker routing plans. Input argument/results are redacted; no key,
button, mouse-coordinate or job payload is read or logged.

All 13 original head hashes are retained in the log for independent read-only
restoration checks, alongside the eleven full native gates. Stop closes recording,
drains, disables hooks, verifies native bytes and releases COM pins. DLL and
trampolines remain resident to forward stale calls safely. A lifecycle receipt
does not establish marker semantics or a latency benefit.

Local preparation backs up the exact installed DLL and sidecars before replacing
the MO2 test slot. No original game binary/assets or settings are changed. Use
the fresh output folder recorded in deployment.json. To remove this test with
the game closed, restore its previous-slot.dll.disabled and output-sidecar.before.bin
to their recorded destinations; the runtime sidecar is retained unchanged.

The loaded-save capture passed on 2026-10-02: 53,520 paired observations,
2,630 complete engine intervals, zero drops/exceptions, and independently
restored thirteen heads and eleven full native functions. The user reported
normal rendering and normal exit; the diagnostic DLL remains installed.
See [the accepted timing map](reflex-input-result.md) for the complete ordering,
late-worker inference and limitations. The two sampled routing tables were
empty, so they do not independently identify the overlapping workers.

The recorded kind-three name `renderer` is retained for trace compatibility.
Its qualified call occurs after engine update and Present, before the next
engine update; it is not RenderSubmitEnd. Connect the tested FrameSession adapter
at engine entry, recording entry, backend-submit return and actual Present.
That integration still needs live SDK/frame-token and marker verification.
