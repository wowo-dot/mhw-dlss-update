# Bounded consecutive-frame depth correction

Date: 2026-09-22. Bridge **v0.7.9**, recorder **v0.6.0**. Local tests pass.
**Run 7148 rejected a different tracker branch before applying any correction.**
The guard/capture worked; three-frame behavior remains untested. The
[v0.6.1 recorder follow-up](read-ready-depth.md) admits the newly verified
already-requested-read branch. [Run 46868](three-frame-result.md) now accepts
three consecutive corrections and the intended fourth-fault stop, with closed/
submitted depth recordings in each CPU interval; GPU completion remains unverified. The original
v0.6.0 design below follows accepted declarations/submissions in [run 24944](post-evaluate-journal.md).

## Purpose and scope

The previous one-use correction allowed one frame's depth preparation to pass
the null-backing load. Its command lists were closed and submitted, with depth
declared as combined shader read `0xC0`. The next frame reached the same fault.
This experiment permits at most three corrections on consecutive renderer
counters, then preserves normal handling of the next fault. Another crash is
expected; this is not a playable rendering implementation.

The first-Evaluate capture remains one-shot and returns intentional
`NotImplemented` (`0xBAD00012`). Subsequent Evaluate calls remain suppressed by
the existing policy. The append-only journal can record later CPU D3D12 calls,
but this does not capture three full Evaluate contracts or prove three completed
GPU frames. No modern SDK is invoked, no GPU commands are added, and no resource
ownership or GPU lifetime is inferred from pointer equality.

## Correction contract

The default remains `--repair-depth-once`; the new experiment requires the
explicit `--repair-depth-three-frames` option. Both modes use the exact host
SHA-256 and twelve live code gates. Each matching stopped debug event rereads
the code, caller, integer/control context, work item and depth tracker.

The target is host RVA `0x22927F2`, the `mov r10,[rdx+112]` in the helper at
`0x22927F0`. The x64 caller must return to `0x229E89E`, with RDX null and R8=4.
RCX is the preparation context; the captured caller uses RBX for the payload
and R15 for its descriptor. Only first-chance read AVs at address `0x70` qualify.
The replacement RDX is the freshly validated depth primary backing. RIP and
every other captured integer/control register must remain unchanged. No host
code/data is patched and no remote function is called. A tentative context edit
is rolled back on verification failure; the process resumes its original
instruction only after acceptance.

The additional three-frame policy requires all of these conditions:

- The existing complete, unique-field work-item contract passes on every frame:
  descriptor 23/3, known texture/view classes, exact formats/flags, 2560x1440,
  stable owners/backings and depth matching the view's member 160.
- Each fresh tracker capture is complete and stable, matches the work item,
  uses the same-epoch active record, and has engine state `0x800` before and
  after its reads. This restricts the test to the new-record helper branch
  observed in run 24944, rather than broadening to untested branches.
- The renderer counter advances by exactly one; duplicates, gaps, regression
  and wraparound are rejected. The preparation thread stays the same.
- Twelve identities stay fixed: renderer, view, container, wrapper, depth owner,
  depth primary/native/tracker, and raw-depth owner/secondary/native/tracker.
  Output may rotate but must pass its full per-frame structural checks.
- At most three admissions, within 500 ms of the first admission. Time must not
  regress; time and the journal deadline are rechecked before committing each
  context edit. This is an admission/commit bound, not a GPU execution deadline.
- Both read-only capture channels are saved and flushed before each decision.
  The first mailbox must be idle with no checkpoint; subsequent mailboxes must
  be published with a complete, nonempty first-Evaluate checkpoint. Journal
  integrity must pass, state must be active, and flags/pending/failed must be
  zero. A pending callback stops this conservative test; it is not diagnosed as
  an application failure.
- The journal remains inside its original two-second Create window and retains
  at least 512 call slots, 2048 description slots and 512 submitted-list slots
  before another correction is admitted. These margins are not a prediction of
  the next frame's size; the existing hard bounds still stop observation.

Any rejected matching event is recorded, forwarded normally and followed by
detach. The terminal event still captures the full work item and tracker when
the live code/context contract passes. Timeout, cancellation and event limits
detach without killing the target. The three-frame policy is owned by one
recorder session and cannot replenish its budget. No resource is AddRef'd by
the external reader; addresses are observational labels only.

Channel publication/checksums prove transport integrity, not semantic acceptance
of Evaluate or GPU work. The unchanged first snapshot must still be reviewed
for complete contract/resource/coverage flags after the run.

## Read-state reporting

Input `expected_state_matches` now requires the non-pixel shader-read bit and
allows combinations with pixel shader read, depth read and copy source. Thus
`0x40` and the observed `0xC0` pass. Write bits, missing non-pixel access and
unsupported bits fail this conservative texture diagnostic. Output remains
exactly UAV `0x8`. Availability, continuity, split/subresource, aliasing and
coverage gates remain required.

This is a diagnostic predicate, not a universal D3D12 state validator. For
example, it intentionally does not accept the buffer-oriented bits present in
`GENERIC_READ`. Microsoft documents that compatible read states may be combined
and write states are exclusive: [D3D12 resource states](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states).
The v0.7.8 archived snapshot is unchanged; its `false` exact-equality result is
explained in its research note.

## Local verification

Release builds pass with `/W4 /WX`; all 25 CTests pass. Added policy cases cover
frame/time/thread rejection, changed resource identities with otherwise valid
individual snapshots, tracker branch/anchor failures, output rotation, channel
phases, checkpoint gaps, pending/failed slots, deadline and capacity headroom.
Barrier replay covers combined reads, rejected write combinations and exclusive
output UAV. Existing WARP observer, journal and transport tests also pass.

The owned debugger fixtures execute the original load after one or three
RDX-only corrections. Every admitted context is verified; tentative rollback
is independently checked. The next real AV reaches the child's own handler,
both channels are read back from flushed storage before forwarding, and the
reader retains them after exit. The three-correction fixture records four
faults/four journal calls and exits with the expected handler result 57.

The two existing run-24944 snapshots were also checked for all twelve stable
identities, consecutive counters 29981/29982, preparation thread 37436,
47 ms fault interval and same-epoch engine `0x800`. This supports the chosen
policy shape; it does not establish that a second correction works in MHW.

The final DLL has 117 CFG targets, including all fifteen distinct provider and
callback wrappers. Observer hook targets, calling conventions and buffer
lifetimes are unchanged from the [v0.7.8 journal](post-evaluate-journal.md).
Both transport layouts remain schema 1 and now require producer 709, preventing
accidental pairing with older binaries.

## Local artifacts and attended run

Evidence is under ignored `local/three-frame-depth-20260922/`:
build/test logs, owned-fixture captures/review, binary audit, source archive,
prior-run shape review and guarded staging manifest. Binaries and game captures
must remain untracked.

| Artifact | SHA-256 |
| --- | --- |
| Bridge v0.7.9, 672768 bytes | `FE31C1A27871D07AC33143E84269072BA1807C36300343CA3B82920FBA0AC237` |
| Recorder v0.6.0 | `5CE544B83A76BFCB51EF0F1AA1ED028D095ABC2DE1F454B0968F794398459EDD` |
| Preserved bridge v0.7.8 | `A39CDD8B9D029B93B6474A296DFBEF0C605519F9BFBE4CD7F4A555CC49C7A38E` |

Host version 0.0.0.0 remains SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
D3D12Core 10.0.26100.9278 remains
`70710DE807F188A85A3F5AD3A729A41BFA7A92B7621CEEAF38059F5640399267`.
Staging rechecks these files with MHW closed and preserves the exact v0.7.8 DLL
as `mhw_dlss_bridge_probe.v0.7.8.dll.disabled` beside the active DLL in MO2's
`dlssTEST` mod. Saved DLSS Off/TAA/DX12 On/FullScreen/2560x1440 settings are kept
byte for byte. No original game executable or asset is changed.

Keep MO2 open and use the existing **Monster Hunter: World - DLSS 1.2.110 test**
entry, offline. Reach the main menu with DLSS Off and wait for the recorder.
Verify v0.7.9 startup, both providers, producer-709 channels and the five-minute
worker window. Open archived recorder v0.6.0 with a new JSONL path, a 180-second
watch limit and `--repair-depth-three-frames`. Only after ARMED, enable DLSS
once, close its popup and leave Options. Stay at the main menu; do not load a
save or toggle again. Reply by phone to avoid a display switch.

Review every correction and terminal event, immutable journal prefixes, the
first-Evaluate work-item binding, per-list recording generations, closes,
submissions and depth declarations. Require explicit evidence for frame
association; queue entry/return times alone cannot establish it. Do not infer
GPU completion, resource contents or temporal alignment. Archive the terminal
stopped snapshot, log and settings; reset only DLSS Off after confirmed exit.

To remove the test, close MHW and disable this bridge's MO2 Force Load entry.
To roll back, preserve v0.7.9 under a versioned disabled name, verify the v0.7.8
backup hash above, and copy that backup to the same active DLL destination.
No MO2 restart or original game-file restoration is required.
