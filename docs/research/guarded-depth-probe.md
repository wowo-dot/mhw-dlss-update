# Guarded depth preparation with native TAA fallback

2026-09-23: bridge **v0.10.0**, explicit mode **`native-depth-probe`**, is
implemented and verified offline. It admits at most one optional producer
item in a process, preserves the game's tracking helper, corrects only the
verified null depth argument, and allows one guarded consumer/Evaluate
capture. Later producer calls use the accepted native-TAA suppression path.
**Run 47820 accepts the guarded CPU path and subsequent main-menu fallback.**
GPU state/order, frame alignment and wider runtime stability remain unverified.
The accepted lifecycle baseline remains
[v0.9.2 run 45100](native-taa-fallback.md#run-45100-recreation-and-menu-toggle-accepted).

## Run 47820: one corrected item, recovery and clean exit

The exact staged v0.10.0 DLL ran in `native-depth-probe` mode on the verified
host with DLSS initially Off. After one enable/apply at the main menu, the user
reported a brief freeze and otherwise normal recovery. No external debugger,
exception repair or intentional terminal crash was used.

Both hook installation statuses reached 4. The terminal work phase was 5
(complete), with one admitted producer, four preparation calls, one corrected
depth argument and one admitted/completed consumer. No preparation or consumer
rejection occurred. The observed depth tracker state was engine mask 0x8.
The final snapshot reached checkpoint 100 in 96 reads. Three stable read-only
samples confirmed these counters and exactly one Evaluate; suppression rose
from 4379 to 4605 without unreadable producer arguments or unrelated callers.

Create sequence 68 issued token `0x10000B12D`. Evaluate sequence 69 completed
the caller/device/handle checks and all 17 parameter queries, then returned
diagnostic NotImplemented (`0xBAD00012`). All four non-null resources were
2560x1440, single-sample Texture2D objects on the command list's device:
color/output format 26, motion format 16 and depth format 41. PreviousOutput
was null. Resource inspection acquired and released ten references without a
fault. Swap-chain creation was 2560x1440, format 87, three buffers. Descriptions
do not establish image contents or frame alignment.

The immutable capture was read through FILE_MAP_READ, its generation/header
and checksum verified, and its JSON preserved before exit. `inputs_verified`
is true, but `qualified` is false: the Evaluate-time barrier snapshot reports
an in-flight call and incomplete coverage. The separately saved journal has
2048 completed checksum-valid records, zero pending/failed slots and the
overflow flag set. Reaching capacity prevents full-coverage acceptance.

The retained prefix is useful for offline ordering analysis. Checkpoint order
672 reserves 336 calls with completed prefix 334. The Evaluate command list
starts its recording at Reset 243, closes at 376 and appears in submission
395 before its next Reset 443. Depth transitions are on another list: calls
327, 337 and 356 declare 0x400→0xC0→0x400→0xC0. That list precedes the Evaluate
list in submission 395. This is CPU submission structure, not proof of GPU
completion or resource lifetime. Neither the overflow flag nor missing
selected-list depth declarations are waived by this CPU-path acceptance.

After capture, the user turned DLSS Off, applied it and exited normally.
The log records one successful Release and both provider Shutdown calls.
Saved settings exactly match the pre-test Off/TAA hash. All 76 staged
native/build/test sources are unchanged; the 33 tests and binary audits apply.

Evidence is under `local/guarded-depth-20260922/`: full run-47820 log,
`capture-47820-0.json`, `journal-47820.bin`, decoded journal, exact-build live
counter samples and `review-47820.json`. Earlier run 12248 lost text coverage
before activation; run 46928 was manually closed during startup. Neither
earlier attempt is counted as an accepted correction or reported crash.

The subsequent [offline submission review](submission-order-and-temporal-inputs.md)
reconstructs the expected declarations and a same-queue signal/completed-value
chain from this saved journal. It retains the original coverage flags and
documents the next frame/jitter capture and completion-bounded recording
design. No unchanged single-item rerun or modern SDK integration is justified
by the current evidence.

## Why there are three guarded stages

Producer `+0x23913F0` queues dense-velocity work and an opcode-23/subtype-3
descriptor with a 64-byte payload. Preparation occurs later; changing only
the depth helper argument would not provide a way to reject an invalid item.
The consumer at `+0x2383CF0` is therefore guarded too. A failed producer
snapshot prevents queuing; a later rejection skips the remaining four-call
preparation sequence and prevents its consumer from touching the payload or
calling NGX. Earlier completed bookkeeping and the original dense-velocity
dispatch are not rolled back. No reconstructed output is written by the
diagnostic provider. Native TAA's preceding output remains the intended image;
the new combined path still needs a visual/runtime check.

The sidecar must contain exactly `native-depth-probe` and LF, with the existing
`capture-one-evaluate` MO2 environment. The ordinary `native-taa-fallback`
mode retains producer suppression and does not install the depth hooks.
ISP stays Create-only. Fresh fallback handles/sizing continue after Release,
but neither recreation nor a full diagnostic ledger replenishes the one-item
producer budget or the one-Evaluate capture budget.

## Contracts and ownership

Host version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The offline code capture is SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both identities were checked before deriving the new contracts. The current
core identity and existing bridge admission checks are unchanged. All new
code spans must match executable host-owned memory before a handle is issued.

| Span | Bytes | SHA-256 |
| --- | --- | --- |
| Tracking helper `+0x22927F0` | 721 | `D27E289F7FDC9F5205E6E8E61615586366A18D0E725235801690A448CDDD8BA2` |
| Preparation function `+0x229CEE0` | 7756 | `11F7CC4D48598967F439659C87142839C4AD3FB900CFAFB7FAB3631655691AF3` |
| Consumer `+0x2383CF0` | 503 | `9A5DBAEF62F4D190685CC92D30531EF7DBD72C7CB8241FD7F20FA94D6AC93EA5` |
| Consumer caller `+0x259DAE0` | 918 | `A0BA3E63DF0026B40BD545D9A7668B899FBCEF7FD83D5E0898CD9A31BD12338D` |

The existing complete producer (737 bytes) and TAA Execute (4221 bytes)
contracts also remain mandatory. Their hashes and producer calling convention
are in the baseline note. These are exact-build diagnostic contracts, not
portable signatures. No executable file or original asset is patched.

The producer hook's Windows x64 argument remains the original argument-block
pointer in RCX. Only return address `host+0x2392F8E` is admitted. A bounded
snapshot resolves the wrapper, layer resources, selected output and main-view
depth. TAA history indices are **0, 1, 2** (`frame % 3` and `(frame+1) % 3`),
and both branches controlled by layer byte +391 are handled. Index 3 is rejected.

Each snapshot checks the known texture class, kind, flags, packed 2560x1440
dimensions, engine format, GPU-format sentinel, nonzero primary/native/tracker
and empty secondary. It compares/rechecks view/container anchors and resource
identities. The selector additionally verifies the kind-5 depth at container
+160, distinct from raw depth +144. Snapshot reads are scalar-sized and bounded
to 160 attempts. These checks establish structural identity, not depth-content
convention or temporal frame alignment.

The new helper detour is a normal function-entry hook, not an instruction
pointer correction. Its assembly shim preserves the original RCX context,
RDX backing and low R8 state, captures the return address plus caller RBX
payload/R15 descriptor, and calls a C++ dispatcher. The 56-byte stack allocation,
call alignment and Windows unwind metadata are independently audited.

| Exact helper return | Resource | Engine request |
| --- | --- | --- |
| `+0x229E86B` | Color primary | 4 |
| `+0x229E883` | Dense motion primary | 4 |
| `+0x229E89E` | Depth: null secondary replaced by validated primary | 4 |
| `+0x229E8B3` | Output primary | 0x400 |

The four calls must arrive in order, on the same preparation thread with the
same context, descriptor and payload, and match the admitted producer's wrapper
and resource identities. The descriptor must still be opcode 23/subtype 3.
Depth also requires the scratch context's current descriptor, a matching
tracker epoch, an active record, and stable engine state 4, 8 or 0x800. These
are engine masks, not D3D12 masks. The original helper performs all bookkeeping;
no owner slot is aliased or written. Unrelated helper callers retain their
original backing and helper behavior.

One mutex covers admission, preparation policy and retirement. The original
tracking helper has no external calls; it runs while that mutex is held so
the consumer cannot observe completed preparation before the helper returns.
The producer itself runs outside this lock, allowing later/asynchronous
preparation. No COM object is AddRef'd or released by these new hooks. The
game retains ownership of queued resources through preparation/consumption;
only identity values are retained, revalidated at use and cleared on rejection
or completion. No raw record is dereferenced after retirement.

The consumer hook has Windows x64 arguments RCX=payload and RDX=command list.
Only caller return `host+0x259DCD7` is guarded; other callers use the original
trampoline. Successful consumption requires all four preparation calls and
a fresh matching resource snapshot. The original consumer runs once, with a
thread-local scope that permits the matching bridge Evaluate capture. That
capture retains existing caller/core/handle/API/parameter/resource guards and
returns NotImplemented; it does not invoke a vendor SDK. The barrier observer
is armed for this diagnostic. The one-item budget then remains spent for the
process lifetime. Rejection stops subsequent reads of that item's metadata.

The DLL and three hooks/trampolines are process-pinned. Consumer guarding is
enabled before changing the helper; producer installation and all bridge
Create checks must complete before any synthetic handle is published. A setup
failure refuses Create. Fixture-only hook removal is exercised after calls
have quiesced. LastError is preserved at guarded boundaries; unrelated calls
retain the original function's side effects.

## Verification and telemetry

All **33 CTests pass** (9.52 seconds). Release `/W4 /WX` builds without project
warnings. The four new owned-process scenarios install the real producer,
assembly helper and consumer hooks: success, invalid producer, invalid
preparation, and a resource changed before consumption. They verify exactly
one original producer/consumer, corrected depth with unchanged other arguments,
no owner writes, lasting suppression, transparent unrelated calls, and removal.
Pure tests reject every failed snapshot read, wrong order/state/identity,
duplicate use, invalid history index and replenished Evaluate budget. Both
history-selection branches and all three indices pass. Existing fallback,
handle-lifetime, journal and resource tests still pass.

The separate hash-verified original-helper execution fixture still passes
states 4, 8 and 0x800 on owned objects. This validates those CPU bookkeeping
branches, not GPU completion or live rendering. CFG audit: **126 targets and
18 distinct wrappers**, including both new entry points. The assembly entry's
unwind record and argument/register capture pass the independent binary audit.

Final DLL: **719360 bytes**, SHA-256
`3A075694B72E1EDED4E58CD961AF3F762959FF5F401D9EB31B1D6793046D49C1`.
Ignored evidence is under `local/guarded-depth-20260922/` (work began September
22 and completed after midnight): final build/CTest logs, `owned-helper.log`,
`binary-audit-0100.json`, `assembly-audit.json`, analysis and staging records.

`bridge.native-taa-fallback` now also reports depth phase, admitted producer,
preparation/correction/rejection counts and consumer admission/completion.
Phases: 0 idle, 1 queued, 2 preparing, 3 prepared, 4 consuming, 5 complete,
6 rejected. The first rejection stage is retained: 1 producer, 10..13 the
four preparation calls, 20 consumer. Capture checkpoint identifies descriptor
(1), index (2), base/view (10/20), texture (30..33), selector (40), rechecks
(50/60/70), or complete (100); read count and observed tracker state accompany
it. Checkpoint 100 with rejection means a sequencing/identity/context guard,
not an incomplete snapshot. No rejection means stage zero.

## Deployment and procedure used for run 47820

Guarded staging preserves the exact v0.9.2 DLL as
`D:/MO2/MHW/mods/dlssTEST/mhw_dlss_bridge_probe.v0.9.2.dll.disabled` and its
sidecar as `mhw_dlss_bridge_probe.mode.before-0100.disabled`. It requires the
game closed and verifies baseline/new DLL, mode, settings, host and test/audit
identities. The final deployment is recorded separately in `staging-0100.json`.
Rollback with the game closed restores **both** preserved files over their
active names. No original game asset, executable or graphics setting changes.

Use the same offline MO2 entry, DLSS Off/TAA, and reach the main menu. MO2
need not restart; no external exception recorder is used. Wait for startup
verification, enable once, leave Options to apply, and stay at the main menu
while replying by phone. Do not load a save or toggle again in this first test.
This procedure is now completed and retained for reproducibility.

Acceptance requires one admitted producer, four successful preparation calls,
one depth correction, one admitted/completed consumer, one complete Evaluate
snapshot and normal responsive imagery followed by continued TAA suppression.
No repeated exception budget or deliberate terminal crash is used. If a guard
rejects, inspect the saved stage/checkpoint and fix/replay that cause offline;
do not repeat an unchanged run hoping for another frame. Full resource-state,
GPU completion, content/frame alignment, display transitions and modern
reconstruction remain separate milestones.
