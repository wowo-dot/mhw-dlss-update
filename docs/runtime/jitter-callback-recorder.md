# Native callback recorder v1.0.0

This independent diagnostic records the actual TAA2 update/reset callbacks and
camera-jitter setter with DLSS left Off. It provides no NGX substitution,
Create/Evaluate admission, resource correction or vendor SDK integration.
Its target map and unresolved questions are in the
[native callback research note](../research/native-jitter-callback-map.md).

## Contract and limits

Windows x64, C++20, pinned MinHook v1.3.4 (existing BSD-licensed dependency).
Only the host SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`,
version `0.0.0.0`, mapped PE timestamp `0x66D13CB6` and size `0x55D3000`
are admitted. Seven live code spans and the two TAA vtable slots are checked
before any hook is enabled. New version identities fail closed.

| Hook | RVA | Native arguments used |
| --- | --- | --- |
| TAA2 update/jitter | `0x23931E0` | RCX: borrowed TAA2 object |
| TAA2 reset/update | `0x2393260` | RCX: borrowed TAA2 object |
| Camera jitter setter | `0x228F390` | RCX: borrowed camera; RDX: borrowed eight-byte input |

The reviewed functions have no stack arguments. MASM wrappers preserve incoming
volatile integer/vector registers and flags around the entry observer, call the
original trampoline exactly once, and preserve outgoing registers/flags around
the exit observer. C++ observers preserve LastError. Owned fixtures check RAX,
XMM0, LastError, nested attribution, a tail-jump target and restored behavior.
This is added instrumentation with measurable timing cost, not zero perturbation.

Each of three explicitly armed phases is capped at 256 paired calls or 1.5
seconds. The render thread takes no blocking recorder lock, allocates no record
storage and performs no file I/O. Each snapshot allows at most 40 named scalar
reads, each at most eight bytes, with region checks and guarded copies. Lock
contention/capacity loss is counted. There is no arbitrary process-memory dump.
Before/after fields are sequential observations, not an atomic frame snapshot.

Calls carry a process phase/token, thread, entry/exit order markers, ticks and
the enclosing recorded callback token. This identifies nested setter calls
even through a native tail call. A setter observed outside a recorded TAA2
callback has no inferred TAA owner. The source pointers are used only within
that invocation, never retained as owned game objects or followed later by
the worker. Stored addresses are correlation values in private logs.

A bad type/read ends capture but still forwards the original call. Exceptions
from the original game function are not swallowed; an unfinished pair blocks
reuse of the buffer, and the worker reports pending/incomplete evidence.
Only fully published pairs are exported. A two-second drain timeout ends the
recorder. It does not repair a game failure or free memory used by an active call.

After phase 3, a stop request, an invalid capture or 30 minutes without an
accepted phase, the worker disables its three hooks. DLL and trampolines stay
resident until process exit so an already-entered wrapper can finish safely.
It does not uninitialize MinHook or remove another component's hooks. The owned
fixture removes its trampolines only after all fixture callers have joined.

## Output and controls

The DLL reads a UTF-8 file named `<loaded-dll-path>.output-dir`, containing one
absolute existing output directory. Deployment supplies the repository's
private evidence directory; no machine-local path is compiled into the DLL.
An existing log is never overwritten. The filename includes PID and process
creation time. Header records identify host, recorder DLL hash and experiment.

Adapter enumeration records vendor/device/LUID and software status. D3D12
module presence is checked, and saved DX12 configuration must be checked by
preflight. This CPU observer does not identify the active GPU or inspect a
swap chain: format is explicitly `null`, API confirmation and GPU qualification
are false. Do not substitute enumeration for graphics-device evidence.

Events are bound to PID **and creation time**:
`Local\MhwJitterCallbacks-v1-<pid>-<creation>-<phase>`, where 0 stops and
1/2/3 capture menu/still/pan. Events are created before hooks are installed;
the first accepted menu request verifies and installs the hooks. Later windows
are armed only after the preceding phase is published. Repeated/skipped phases
are rejected; diagnostic rejection logging is bounded.

From the repository, the operator uses:

```powershell
python tools/arm_jitter_callbacks.py --log <capture.jsonl> --phase menu --output <menu-request.json>
python tools/review_jitter_callbacks.py <capture.jsonl> --output <menu-review.json>
```

Use `still` and then `pan` for subsequent windows, with separate request/review
receipts. A request receipt alone is not an acknowledgment: require the matching
`armed` and `phase_complete` records. The reader rejects process-generation
mismatches, malformed/partial captures, duplicate tokens/order markers, missing
parents, crossed thread relationships, wrong types and incomplete publication.
Loss and failed samples remain explicit; no result qualifies GPU/raster behavior.

## Local deployment and removal

The existing MO2 test entry force-loads
`D:\MO2\MHW\mods\dlssTEST\mhw_dlss_bridge_probe.dll` even with an empty mod
list. For this experiment that exact slot holds the **standalone callback
recorder**, not the earlier bridge. The old DLL must first be preserved at
`D:\MO2\MHW\mods\dlssTEST\mhw_dlss_bridge_probe.v0.15.0.before-callbacks.dll.disabled`
with SHA-256
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The existing `.mode` sidecar is left in place and is not read by this recorder.
Do not overwrite the earlier v0.14 backup.

Installed 2026-09-23 with SHA-256
`8646227E8F9D58E3E34C8DF3F930B589144D6B0B5D0F88BE9ED843DD8C102AD4`.
The backup hash, new output sidecar, 11 source identities and four unchanged
configuration files were verified in the local receipts. The first completed
[game run 37452](../research/native-jitter-callback-result.md) now accepts all
three bounded CPU windows. Hooks were disabled cleanly and the user exited
normally. The previous v0.15 bridge has been restored; the newly created output
sidecar is removed and saved settings are unchanged. The recorder is no longer
installed in that slot. Consult `restored.json` and `acceptance-37452.json`
alongside the original deployment receipt.

Deployment and rollback receipts, exact new DLL identity, a copy of the old
sidecar/profile/settings and the output directory belong under
`local/jitter-callbacks-20260923/`. The local deployment script requires the
game to be closed and refuses unrecognized destination hashes. If an output
sidecar existed, preserve and restore it; otherwise remove only that newly
created sidecar on rollback. Restore the old DLL by copying the verified
backup to its exact original destination. Keep the backup/evidence.

No original game asset/executable, MO2 profile or saved graphics setting needs
editing. MO2 can remain open when these DLL files are replaced between game
processes. Confirm the actual loaded recorder hash and empty profile on launch.

## User sequence

1. Keep the `barebonesdlss` profile, all mods disabled and Stracker disabled.
   Launch **Monster Hunter: World - DLSS 1.2.110 test** to load the recorder
   through its inherited Force Load rule. Prefer offline play.
2. Leave DLSS Off and TAA selected. Stay at the main menu and notify the operator.
   The operator verifies the process/module/settings and captures phase 1.
3. Load one save, stay still and notify the operator for phase 2.
4. On the operator's cue, continuously pan the camera until told the phase-3
   capture is saved. Respond from the phone to avoid an unintended focus change.
5. After evidence is saved and hook disabling is confirmed, exit normally.
   Archive the run and restore the old bridge slot while the game is closed.

Do not enable DLSS for this experiment. User movement is a controlled input
condition; this recorder does not establish motion-vector contents or correct
jitter application to GPU projection matrices.

## Verification

Build the `mhw_jitter_callback_probe` and `jitter_callbacks_tests` targets in
Release. Project C++ warnings are errors. Run the native `jitter_callbacks_tests`
CTest and `python -m unittest discover -s tests -p test_jitter_callbacks.py`.
The native fixture uses actual MinHook/MASM paths, concurrent callers and known
return values; it also verifies rejection of a foreign executable, invalid
object types, repeated/skipped phases, deadlines and bounded storage. Python
checks target false acceptance of partial or misattributed captures.

Passing these tests alone does not claim a successful game run. Run 37452 adds
live-span verification, callback execution, bounded context/order evidence and
the final hook-disable receipt; its limits are recorded in the result note.
Future changed builds still require those runtime checks independently.
