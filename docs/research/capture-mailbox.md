# First-Evaluate capture persistence

Date: 2026-09-22. Bridge **v0.7.7**, external recorder **v0.4.0**.
All 24 tests and the CFG audit pass. **Run 38476 accepted capture transport,
the first-Evaluate contract/resource descriptions and the bounded CPU timeline.**
Depth's state at GPU use remains unresolved; modern reconstruction is unimplemented.

[Run 4568](depth-state-timeline.md) captured the engine depth tracker, but no
finished Evaluate/resource/queue event reached the bridge text log. The next
fault arrived 16 ms after the corrected fault, while the worker polls every
25 ms. Deferred logging losing that race is plausible, but the old evidence
does not establish whether Evaluate reached capture. This change preserves
the completed result independently of that worker and records entry separately.

## Run 38476: complete capture preserved

The user enabled DLSS once after ARMED, left Options and reported a longer pause
followed by a crash. Startup verified v0.7.7, both providers, the exact environment
and an idle producer-707 mailbox. Recorder v0.4.0 passed its twelve live code
gates. Each matching fault produced 105 work-item reads and 32 tracker reads;
the sole RDX correction was verified with unchanged RIP. The next matching fault
arrived 16 ms later and was forwarded after the correction budget was exhausted.
The debugger detached cleanly after 325 debug events and three exceptions.

The mailbox was idle at the first fault. Before forwarding the second fault,
the recorder saved and flushed **15193 bytes** of published schema-1 JSON for
Evaluate sequence **69**, including the full contract, descriptions, barrier
summary, cross-list timeline and presentation metadata. The finished event
reports `bridge_capture_saved=true`. This run also preserved the corresponding
text-log event. Independent JSON parsing rejects duplicate keys and cross-checks
the two channels' getter values, descriptions, timeline entries and submitted
list labels. The recorder detaches before process exit; this run establishes
pre-forward persistence, while the owned-child fixtures separately establish
retention after abrupt producer exit.

All four native pointers match the corrected work item at renderer frame
**20683** (`0x50CB`). In particular, the captured output matches that frame and
differs from uncorrected frame **20684**, strengthening the frame association.
The 17 queries completed without fault, and all ten temporary references from
26 inspection calls were released. Color/output are R11G11B10_FLOAT, motion is
R32G32_FLOAT and depth is R32_FLOAT, all 2560x1440 on the command list's device.
PreviousOutput is null. The unique matching swap-chain creation is 2560x1440
B8G8R8A8_UNORM, three buffers. These descriptions do not validate image contents,
temporal conventions, current presentation state or GPU frame alignment.

The observer retained **244 API calls, 610 barrier descriptions, two depth
matches, ten relevant list controls, thirty queue events and eighteen submitted
list labels** without a reported gap, truncation or malformed range. In the
Evaluate list's selected recording, color and motion reach declared non-pixel
shader read and output reaches declared UAV state. Depth still has no matching
barrier on that list. Two other lists record depth transitions to COPY_DEST;
the [recording-aware analysis](depth-state-timeline.md#run-38476-depth-calls-observed-submission-window-incomplete)
shows all retained queue calls precede the relevant lists' latest Resets.
Their matching submission addresses therefore identify earlier recordings,
not submission of the current depth work. No final depth read state or GPU
execution is accepted.

The tracker repeats the preceding run's pattern: selected engine state `0x8`,
request `0x4`, same epoch, active-before `0x800`; tracker-before changes from
`0xFF` to `0xC` on the next frame. Current/active operations are 1379/1361 and
1374/1358. This remains consistent with the statically predicted read-mask
merge, without proving which earlier barrier entry was changed or emitted.

Evidence and `review_38476.py` / `review-38476.json` are under ignored
`local/capture-mailbox-20260922/`. The review also verifies source archives,
deployed/backup binary hashes, host identity and the exact settings recovery.

| Evidence | SHA-256 |
| --- | --- |
| `retry-38476.jsonl` | `1B5DC039AC3080C22FD196B8515A9DCBABDF38EB714C4741E3FA90B77643FC15` |
| `bridge-38476.crash.log` | `955C8C7C62945DECADE34A4DA9F6F562AC971023C5941D389D5AC084D7894951` |

After confirmed game exit, the exact saved-On configuration was preserved and
only DLSS was reset Off; TAA, DX12 On and FullScreen 2560x1440 remain unchanged.
**Do not repeat the unchanged run.** Next work is a bounded continuation beyond
Evaluate, through relevant list closes/submissions and queue synchronization,
with evidence preserved before the next fault. This is now implemented and
fixture-tested in the [v0.7.8 journal](post-evaluate-journal.md). Run 24944
accepted its current-recording submissions and compatible depth read declaration.
The longer visible pause cannot be attributed from these logs; debugger capture
temporarily suspends the process, so it is not evidence of longer stable rendering.

## Publication and lifetime

After exact environment preflight and before resolver installation, the bridge
creates a session-local, page-file-backed mapping. Its name includes protocol
version, process ID and the process creation FILETIME, preventing accidental
reuse across process generations. Existing names are rejected without changing
their contents. Creation failure leaves the experiment inert before provider
routes are installed. No game directory path is embedded in this mechanism.

The [Windows shared-memory lifetime](https://learn.microsoft.com/en-us/windows/win32/memory/creating-named-shared-memory)
allows the recorder's separate handle/view to retain the mapping after the
producer process exits. The recorder opens it using
[FILE_MAP_READ](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile)
before attaching its debugger. It never writes through that view. This is a
bounded record explicitly published by our DLL, not a process-memory dump.

The fixed 131120-byte mapping contains a 48-byte header and at most **131072
bytes of ASCII JSON**. Header fields identify magic, schema 1, producer 707,
mapping size, PID, creation time, state, payload length and a 32-bit FNV-1a
checksum. The checksum detects accidental corruption; it is not authentication.
This local channel assumes the same trust in the user's bridge process and
session as the rest of the diagnostic. It is not an inter-user security boundary.

| State | Value | Meaning |
| --- | --- | --- |
| Unavailable | 0 | Reader could not open the expected mapping |
| Idle | 1 | Initialized; first Evaluate has not entered |
| Entered | 2 | First DLSS Evaluate wrapper entered; no completed publication |
| Writing | 3 | Producer is serializing; payload must not be consumed |
| Published | 4 | Complete immutable payload, subject to header/length/checksum validation |
| Failed | 5 | Producer rejected the payload or serialization exceeded its bound |
| Invalid | 6 | Reader rejected state, identity, length, checksum or payload framing |

MSVC x64 lock-free 32-bit `atomic_ref` operations publish transitions with
release/acquire ordering. The read-only side uses loads only. Schema offsets,
header size and lock freedom have compile-time assertions. Publication is
one-shot; the result is never reused for another frame. No wait, heap allocation,
file write, COM call or GPU command is added by the serializer on the Evaluate
thread. It copies already-captured scalar data into the preallocated mapping.
The external recorder performs allocation and disk I/O in its own process.

The Evaluate entry marker is set before acquiring the bridge boundary lease.
After the existing bounded inspection, the result is serialized before reserving
a text-log event slot. A full text-log queue therefore cannot discard the shared
result. Provider event publication also wakes the existing worker; correctness
of the shared capture does not depend on that wakeup. This adds no game/system
method hooks, and the seven v0.7.6 D3D12 observer targets are unchanged.

The process-pinned bridge owns its mapping handle/view for its lifetime. The
external reader owns independent RAII handle/view objects until it finishes.
An abrupt exit during serialization leaves a non-published state; the reader
reports it and never treats a partial buffer as a complete capture. Preservation
after game exit requires the recorder to have opened its handle beforehand.

## Payload and recorder contract

The payload contains the first Evaluate's sequence/tick/thread/caller, logical
handle value, result, capture/command/boundary flags, handle-binding evidence,
17 getter observations, resource descriptions/reference accounting, complete
bounded barrier summary, retained cross-list depth/control/queue events and
submitted list labels, plus presentation-creation metadata. Missing snapshots
have explicit `has_*` flags. `published` means transport completed; it does not
mean the enclosed inspection, contract, barrier coverage or GPU state passed.

Arrays retain the original capacities and report truncation/coverage separately.
Malformed retained counts and submission ranges reject serialization before
array access. Integer and pointer labels are lossless decimal JSON integers;
offline consumers must preserve 64-bit integer precision. Float observations
are IEEE-754 `uint32` bits, allowing NaNs to be recorded without invalid JSON.
Getter `key` indices follow the enums in `contract_snapshot.hpp`; resource roles
are 0 color, 1 motion, 2 previous output, 3 depth, 4 output. Queue/control kinds
retain the v0.7.6 enum. Pointer labels do not establish object lifetime.

Recorder v0.4.0 requires an idle producer-707 mailbox before debug attach and
again before announcing ARMED. At each matching fault it reads the mapping and
**writes and flushes** a `bridge-mailbox` JSONL event before allowing the debug
event to continue. Only the first valid published event includes `capture`;
later status events do not duplicate the payload. It checks the mapping again
on finish, including after producer exit, and reports `bridge_capture_saved`.
If the first fault sees a non-idle mailbox, the one-use correction is rejected.

The twelve host code gates, 105-read work item, bounded depth tracker, RDX-only
context edit, rollback and one-correction budget are unchanged. Published data
does not relax those checks. A subsequent matching fault is forwarded, not
corrected. There are no new remote calls, memory writes or breakpoints. Readable
mailbox data remains evidence to review, not a basis to invoke GPU work.

## Verification

The Release build passes project `/W4 /WX` for all targets. All **24 CTests**
pass, including the existing WARP queue/barrier fixtures. New tests cover:

- Every retained array at capacity, exact 64-bit values, signed values and NaN
  bits; the representative maximal-count payload is 65009 bytes. An independent
  Python JSON parser confirmed valid syntax, unique keys and all 17 getters.
- Small/empty output spans, malformed counts/ranges, no overwrite of an existing
  mapping, process-generation mismatch and single immutable publication.
- Corrupt checksum, bad owner/producer, oversized length and non-published states.
- A parent opening read-only before its owned child publishes and terminates
  without destructors or a logging worker. The full result remains readable
  after child exit. Failed publication survives as failed, with no payload.
- The external debugger's actual save/flush helper during the second fault of
  its owned retry fixture. The output file is read back **before** forwarding;
  only then is the fixture's handler allowed to proceed. The first RDX retry,
  second AV forwarding, normal handler behavior and post-exit read all pass.

The bridge remains x64/CFG-instrumented with **115 CFG targets** and all 15
distinct provider/callback wrappers listed in the table. Tests establish capture
transport and existing observer behavior, not game acceptance or rendered output.
Evidence and source archives are ignored under `local/capture-mailbox-20260922/`.

## Staging and completed test procedure

The following procedure produced run 38476 above. It is retained for
reproducibility, not a request for another unchanged test.

| Artifact | SHA-256 |
| --- | --- |
| Bridge v0.7.7, 651264 bytes | `11E3173ACA3999C182A79E10E5F8000B4F5BE15F761551569FC8CFA604B13CC6` |
| Recorder v0.4.0 | `34A5AC3B14D65D29C04FE59813BFFAD08C2D436E38D3FF16EB457F37931A21EA` |
| Preserved v0.7.6 | `5BE3E39B95EF9B4100BCB0B738D44434E8D4346E76ED02C1C85518830A19689F` |

The exact original host SHA-256 was rechecked:
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`,
version 0.0.0.0. D3D12Core remains 10.0.26100.9278, SHA-256
`70710DE807F188A85A3F5AD3A729A41BFA7A92B7621CEEAF38059F5640399267`.
No new game offsets or host patches are introduced.

Stage only while the game is closed, preserving the exact active DLL beside it
as `mhw_dlss_bridge_probe.v0.7.6.dll.disabled`. Preserve the settings without
changing them: DLSS Off, TAA, DX12 On, FullScreen 2560x1440. The existing MO2
PID 7112 environment and **Monster Hunter: World - DLSS 1.2.110 test** entry apply.
No MO2 restart is required.

For the attended offline run, reach the main menu with DLSS Off. Verify fresh
v0.7.7 startup, exact environment identities, both providers, mailbox readiness
and the five-minute worker window. Attach the archived recorder v0.4.0 with a
new JSONL file and `--repair-depth-once`. After ARMED, enable DLSS once, close the
popup and leave Options; remain at the main menu without another toggle or save
load. Another next-frame crash is expected. Preserve both logs and saved-On
settings before recovering saved DLSS Off with the game closed.

Acceptance requires a published, validated payload tied to the corrected frame's
resource identities and clean existing capture checks, not merely
`bridge_capture_saved=true`. Inspect the earlier depth-record merge and queue
timeline without inferring GPU completion. Idle/entered/writing/failed results
identify the remaining gap and must not be promoted to completed observations.

To remove the experiment, close MHW and disable the bridge Force Load entry.
For rollback, preserve the new active DLL under a versioned disabled name,
verify the v0.7.6 backup hash above, then copy it to the active DLL name in the
same MO2 mod. No original executable or asset requires restoration.
