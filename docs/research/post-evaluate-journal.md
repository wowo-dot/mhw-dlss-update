# Bounded recording after first Evaluate

Date: 2026-09-22. Bridge **v0.7.8**, external recorder **v0.5.0**.
All **25 CTests**, the additional hardware continuation fixture and CFG audit
pass. **Run 24944 accepted the current-recording depth declarations and
submissions after one correction.** GPU completion and texture contents remain
unverified. Modern reconstruction is not implemented. The
[v0.7.9 follow-up](bounded-depth-frames.md) corrects read-state reporting;
[run 46868](three-frame-result.md) accepts three consecutive corrections with
closed/submitted depth recordings and the intended fourth-fault stop.

## Run 24944: current lists closed and submitted

The user enabled DLSS once at the main menu after ARMED, left Options and
reported the expected crash. Startup verified v0.7.8, both providers and both
producer-708 channels. Recorder v0.5.0 passed all twelve live code gates, captured
two complete 105-read work items and 32-read tracker snapshots, applied exactly
one verified RDX correction with unchanged RIP, then forwarded the next matching
fault. It detached after 322 debug events and three exceptions.

The corrected and uncorrected renderer frames are **29981 / 29982**. Their fault
ticks are 47 ms apart; Evaluate occurs 31 ms after the first and 16 ms before
the second. These are diagnostic timings, not a performance or stability result.
The captured four native pointers match the corrected frame, including output,
which differs from the next frame. All 17 getter queries and 26 resource queries
completed, with all ten temporary references released. The four 2560x1440
descriptions remain color/output R11G11B10_FLOAT, motion R32G32_FLOAT and depth
R32_FLOAT; all use the command-list device. PreviousOutput is null.

The first fault's journal contains **206 completed calls**. Evaluate pins **310
calls / 905 descriptions** with a complete prefix, no in-flight callback and
clean coverage flags. The stopped-process journal saved before forwarding the
second fault contains **387 calls / 960 descriptions**, including **77 calls
after the checkpoint**, 53 queue events and 35 submitted-list labels. Every slot
is complete; flags, pending and failed counts are zero. The first 206 records
are unchanged in the later journal. The first-Evaluate mailbox holds 15655 bytes.
The independent review compares its retained events with both the full journal
and bridge text log. The finished live-read journal equals the stopped-process
one, but acceptance uses the stopped-process sample.

The depth resource label is `0x13A09F070`. Both relevant lists have successful
Reset/Close observations, followed by submissions before their next Resets:

| Recording label | Current Reset | Close | Execute call / CPU entry-return order | Next Reset |
| --- | --- | --- | --- | --- |
| Producer `0x159B3CB0` | 191 | 266 | 330 / 660-661 | 372 |
| Evaluate `0x159AF720` | 188 | 316 | 333 / 666-667 | 369 |

Both Execute calls target queue `0xD36E850`. The producer is list index 0 in
call 330; Evaluate is index 2 in call 333. The earlier submissions at calls
153/156 precede the current Resets and are correctly excluded. An intervening
Signal/Wait pair (331/332) has the same fence and value 54130, with the Wait on
the Evaluate queue. No fence completion is queried.

In **submission order**, the complete observed depth declaration chain is:

| List / barrier call | Declared before | Declared after |
| --- | --- | --- |
| Producer / 234 | GENERIC_READ `0xAC3` | COPY_DEST `0x400` |
| Evaluate / 221 | COPY_DEST `0x400` | Combined shader read `0xC0` |
| Evaluate / 280 | Combined shader read `0xC0` | COPY_DEST `0x400` |
| Evaluate / 310 | COPY_DEST `0x400` | Combined shader read `0xC0` |

All four are ordinary transitions with flags zero and all subresources. Call
310 returns at CPU order **620**, immediately before the Evaluate checkpoint
at **621**; the Evaluate list closes later at 316. There are no additional
depth matches through that Close in the retained journal. Call 221 overlaps
producer recording on the CPU, so sorting transitions by CPU recording order
would give the wrong cross-list sequence.

The separate Execute invocations on the same queue establish the relevant
submission ordering under the documented
[ExecuteCommandLists contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists).
Their normal return is **not** a validation-success HRESULT: the API returns
void, and the journal's result zero is its placeholder. Successful Close,
Signal and Wait results are separate recorded facts. No driver acceptance,
initial GPU state, resource lifetime or completed GPU execution is inferred.

Depth's final declaration `0xC0` combines non-pixel (`0x40`) and pixel (`0x80`)
shader reads, as defined by
[D3D12_RESOURCE_STATES](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states).
The v0.7.8 summary reports `declaration_available=true` but
`expected_state_matches=false` because `barrier_history.cpp` compares inputs
against exactly `0x40`. That equality is a diagnostic classification limitation;
the observed `0xC0` includes the required read bit. It is not evidence for an
extra barrier or a missing shader-read transition. Color/motion end at `0x40`
and output at UAV `0x8` in the same selected recording.

The fault's selected tracker state is **engine `0x800`**, active-before `0xC`,
unlike runs 4568/38476 selecting `0x8`. Context/active operations are 1016/954
and 1011/951; tracker-before values are `0xFF` and `0xC`. Static helper analysis
therefore predicts the new-record branch for request `0x4`, not the earlier
read-mask-merge branch. This is a branch inference, not proof of every internal
record mutation or that the correction alone produced all four observed barriers.

`review_24944.py` and `review-24944.json` are saved beside both logs. The review
rejects duplicate JSON keys, checks frame/resource binding, the one-correction
budget, immutable prefixes, all 775 CPU order positions (including the Evaluate
marker), the current recording generations, depth continuity in submission order,
binary/source identities and byte-exact settings recovery.

| Evidence | SHA-256 |
| --- | --- |
| `retry-24944.jsonl` | `9D962F5D998CE22366F49F32EAD867C09C5BE74DCE40E9DB5B6471CE39D2B031` |
| `bridge-24944.crash.log` | `84CC2BE3F3FE1874BF818DD726148901380DB0BBE9DF92AE92503A9539233D7F` |

After confirmed game exit, the exact saved-On configuration was preserved and
only DLSS was reset Off. **Do not repeat this unchanged one-shot run.** The
[v0.7.9 follow-up](bounded-depth-frames.md) implements the reporting fix and a
bounded consecutive-frame policy. Sustained rendering, texture contents, depth
convention, motion/jitter alignment, exposure and later SDK integration remain
separate work. The v0.7.8 run used only one correction.

## Evidence motivating the change

[Run 38476](capture-mailbox.md#run-38476-complete-capture-preserved) saved a
complete first-Evaluate snapshot, including two depth transitions on other
command lists. All retained submissions preceded those lists' current Resets.
Stopping at Evaluate therefore missed the later recording/submission evidence
needed to interpret depth's state at use. Pointer matches to previous recordings
must not be mistaken for submission of the corrected frame.

## Observation and publication

The existing seven system D3D12 hooks continue unchanged in target, ABI and
forwarding behavior. Their signatures, ownership and structural discovery are
documented in [depth-state-timeline.md](depth-state-timeline.md#bridge-observation-contract).
No new host offsets, host patches, breakpoints, remote calls or GPU commands
are introduced. All pointer values remain labels, with no lifetime guarantee.

At first Evaluate entry, the observer pins a **completed prefix** of its original
append-only arrays before querying foreign parameters. It records the reserved
call count, contiguous ready prefix, CPU order marker, tick and command-list
label. Any in-flight callback or hole makes the first snapshot incomplete.
The snapshot is replayed only from that pinned prefix; subsequent Close/Reset
events cannot change its selected recording or resource declarations.
The marker and count are successive atomic observations, not a global thread
barrier; entries/returns must be interpreted using their explicit CPU order.

Admission continues after that checkpoint using the original bounds: **2000 ms
from Create**, 2048 calls, 16384 barrier descriptions, 4096 submitted-list labels,
1024 descriptions per barrier call and 128 lists per submission. The first call
past the deadline stops admission; no timer or extra worker is required. Overflow,
read fault and admission contention also stop admission and are explicit flags.
Already admitted callbacks may finish after a stop. There is no buffer reuse.
The one-use correction and next-fault forwarding policy are unchanged.

After each original method returns, the callback copies its completed record and
owned description/list ranges into a separate shared journal, computes a checksum
and publishes that slot with a release store. The callback has no new allocation,
disk I/O, lock, wait, COM query or resource dereference. It preserves the original
result and LastError. Original call arguments are read only while caller-owned;
later copies use the observer's own bounded arrays. Each callback owns one slot
and disjoint ranges allocated by the existing atomic counters.

The bridge creates the journal during exact-environment preflight, before
installing provider routes. Failure to create either capture channel leaves
the experiment inert. It retains the first-Evaluate JSON mailbox separately;
that mailbox's schema remains 1, with producer bumped to **708** so older
recorders cannot silently accept the new semantics.

## Journal protocol

The page-file mapping is named
`Local\MhwDlssBarrierJournal-v1-<PID>-<creationFILETIME>` and is **753752 bytes**:
an 88-byte header, 2048 96-byte slots, 16384 32-byte D3D12 barrier descriptions
and 4096 64-bit list labels. Compile-time assertions fix the Windows x64 layout.
The header verifies magic, schema 1, producer 708, mapping size, owner PID and
creation time. Existing mappings are rejected without modifying them.

Header states are idle (1), active (2) and stopped (3). Stop flags are overflow
(1), fault (2), contention (4) and expiry (8). Checkpoint state is absent (0),
writing (1), or published (2); its non-atomic fields are read only after acquire
of state 2. Each slot is empty (0), pending (1), published (2) or failed (3).
Pending slots are never read as records. Completed slots are immutable and carry
an FNV-1a checksum covering the stored call and its owned payload ranges. This
detects accidental corruption; it is not authentication against another process
in the same trusted session.

The external reader opens **FILE_MAP_READ** before attaching and holds its own
handle/view until it finishes. It scans the fixed slot array, validates completed
records and ranges before dereference, and retains later completed calls even
when an earlier slot is still pending. Failed/pending counts and global flags
are explicit; a valid channel read does not mean complete observation coverage.
No GPU completion or end-of-frame claim is made by a valid journal.

JSON preserves every completed call and its descriptions/submitted list labels,
including the CPU entry/return orders, results, queue and fence values. All pointer
and integer labels remain exact decimal integers. It does not apply the smaller
first-Evaluate summary's 16-depth/64-queue truncation limits. Serialization and
file I/O occur only in the external recorder, within the fixed record bounds.
Only documented API argument fields are serialized, never arbitrary memory.

Before forwarding each matching fault, recorder v0.5.0 saves and flushes **both**
channels. These journal events have `target_stopped=true`. Armed and finished
reads have `target_stopped=false`, because the process can be running; those are
not globally consistent frame snapshots. The first correction additionally
requires an idle Evaluate mailbox and valid active journal with no stop flags.
All twelve host code gates, 105-read work item, 32-read tracker acceptance and
RDX-only context verification remain intact. No additional correction is allowed.

## Verification

The Release build passes `/W4 /WX`. All 25 CTests pass. The additional WARP test
checkpoints the Evaluate list, then resets a previously submitted producer list,
records depth read-to-copy-to-read transitions, closes and submits the current
producer and Evaluate lists, and records Signal/Wait. Its first snapshot remains
unchanged while the journal gains all eight later calls. The same test passes
on the hardware adapter, exposing the same seven D3D12Core target RVAs.
Only fixture-owned lists are submitted by tests, with a completion fence before
resource destruction; the observer itself submits nothing.

Transport tests cover pending holes with later completed records, invalid ranges,
duplicate publication, process-generation mismatch, old producer rejection,
checksum corruption, all storage pools at capacity and explicit stop flags.
Owned children terminate abruptly after publication; their completed records and
pending slots remain readable through the parent's handle. The debugger's owned
retry fixture publishes a post-checkpoint submission between two faults, and
reads back its flushed JSONL before forwarding the second fault. It verifies
the same one-correction budget and normal second-fault handling.

An independent Python parser checks both maximum-capacity JSON fixtures for unique
keys and exact integers. The full journal retains 2048 calls, 16384 descriptions
and 4096 list labels. Binary inspection confirms x64 CFG instrumentation, **117
CFG targets** and all fifteen distinct provider/callback wrappers. Local evidence,
source archives and binaries are ignored under `local/post-evaluate-journal-20260922/`.

## Identities, staging and completed attended-run procedure

This procedure produced run 24944 above; it is retained for reproducibility,
not a request to repeat the unchanged diagnostic.

| Artifact | SHA-256 |
| --- | --- |
| Bridge v0.7.8, 672768 bytes | `A39CDD8B9D029B93B6474A296DFBEF0C605519F9BFBE4CD7F4A555CC49C7A38E` |
| Recorder v0.5.0 | `E705706FE4709A5F8F2947FBB0E13DA92E8DA428B17B78976FBA47A989219012` |
| Preserved v0.7.7 | `11E3173ACA3999C182A79E10E5F8000B4F5BE15F761551569FC8CFA604B13CC6` |

The tested host remains version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
D3D12Core remains 10.0.26100.9278, SHA-256
`70710DE807F188A85A3F5AD3A729A41BFA7A92B7621CEEAF38059F5640399267`.
The stage script rechecks those hashes, the current DLL and saved settings with
the game closed before replacing only the MO2 mod's active DLL. The exact old
file is preserved beside it as `mhw_dlss_bridge_probe.v0.7.7.dll.disabled`.
DLSS Off, TAA, DX12 On, FullScreen 2560x1440 are preserved byte for byte.

MO2 PID 7112 may stay open. Use the same offline **Monster Hunter: World - DLSS
1.2.110 test** entry and remain at the main menu with DLSS Off. Verify fresh
v0.7.8 startup, both providers, both producer-708 capture channels and the
five-minute bridge worker window. Attach archived recorder v0.5.0 with a fresh
JSONL file and `--repair-depth-once`. After ARMED, enable DLSS once, close the
popup and leave Options. Stay at the main menu, without another toggle or save
load. Another next-frame crash is expected.

Acceptance must bind the first-Evaluate resources to the corrected work item,
then select the journal's stopped-process snapshot before the next fault. Check
flags, failed/pending slots, checkpoint coverage, list Resets, Close results,
matching current-recording submissions and queue synchronization. The trace may
still stop too early or expose another gap; missing evidence must remain missing,
not be converted into a guessed barrier. Preserve logs and saved-On settings,
then recover only DLSS Off after confirmed game exit.

To remove the experiment, close MHW and disable the bridge Force Load entry.
For rollback, preserve v0.7.8 under a versioned disabled name, verify the v0.7.7
backup hash above and copy it back to the active DLL name in the same MO2 mod.
No original executable or game asset requires restoration.
