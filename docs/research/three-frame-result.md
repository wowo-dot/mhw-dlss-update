# Three consecutive depth corrections: run 46868

Date: 2026-09-22. Bridge **v0.7.9**, recorder **v0.6.1**.
**Accepted: three consecutive RDX corrections, first-Evaluate capture, and
closed/submitted depth recordings in all three intervening CPU intervals.**
GPU completion, texture contents and modern reconstruction remain unverified.

## Runtime correction and terminal fault

The user enabled DLSS once from the main-menu Options after ARMED, left Options
and reported the expected crash. Startup verified both providers and both
producer-709 capture channels. Four matching first-chance null-depth faults
passed all fourteen live code gates on preparation thread **8476**:

| Renderer counter | Tick (ms) | Action | Remaining corrections |
| --- | --- | --- | --- |
| 18057 (`0x4689`) | 25261640 | RDX corrected; original instruction retried | 2 |
| 18058 (`0x468A`) | 25261671 | RDX corrected; original instruction retried | 1 |
| 18059 (`0x468B`) | 25261687 | RDX corrected; original instruction retried | 0 |
| 18060 (`0x468C`) | 25261703 | `correction-budget`; original fault forwarded | 0 |

Each work-item snapshot completed 105 reads; each tracker snapshot completed
32 reads. No read failed, budget exhausted or anchor changed. All twelve
cross-frame identities remained stable. The depth native pointer was
`0x14AAB2B00`; its primary backing was `0x1D5830080`, secondary null. All four
observations selected same-epoch engine state **`0x4`**, with active previous
state `0x800`. This tests the already-requested-read branch. Repeated corrections
on the `0x800` branch were not exercised by this run.

Every applied context was verified RDX-only with RIP unchanged. The three
corrections occurred across 47 ms; the fourth fault was 63 ms after the first.
The recorder saved and flushed both channels before every decision, forwarded
the terminal fault and detached normally: **330 debug events, five exceptions,
three context attempts and three instruction retries**. The terminal rejection
was the intentional correction limit, not a new failed identity or tracker
check. This does not establish that continuing indefinitely would be safe.

Output native labels rotated through `0x13A9E6FE0`, `0x13A9E5330`,
`0x13A9E6650`, then the first label again. Equality is not proof of allocation
lifetime. No resource was retained by the external reader.

## First Evaluate and diagnostic classification

Evaluate sequence **69**, tick **25261656**, ran on thread **48520** with list
`0x159EC0E0`. The mailbox contains **16259 bytes**, with clean handle binding,
command validation and boundary flags. All seventeen getters and twenty-six
resource-inspection calls completed; all ten acquired references were released.
Color, motion, depth and output exactly match the first corrected work item:

| Input | Format | Dimensions |
| --- | --- | --- |
| Color | R11G11B10_FLOAT (26) | 2560x1440 |
| MotionVectors | R32G32_FLOAT (16) | 2560x1440 |
| Depth | R32_FLOAT (41) | 2560x1440 |
| Output | R11G11B10_FLOAT (26) | 2560x1440 |

PreviousOutput was null. All non-null resources shared the command list's
device. The unique captured swap-chain creation was B8G8R8A8_UNORM (87),
2560x1440, three buffers, adapter LUID `0:184EE`.

The pinned prefix has **342 calls / 1005 descriptions**, 67 matching calls and
current Reset 204. All coverage flags pass. Color/motion end in declared `0x40`,
depth in `0xC0`, output in UAV `0x8`. All four now report
`expected_state_matches=true`, accepting the combined-read reporting fix in an
actual capture. Depth has five selected-list transitions; the four-entry recent
history is intentionally bounded, while the six-entry cross-list timeline
preserves the entire first depth chain including the producer transition.

The provider returned intentional `0xBAD00012` (NotImplemented). **Only this
first Evaluate was captured.** Later Evaluate contracts were suppressed by the
unchanged one-shot policy. Subsequent journal evidence must not be described as
three complete Evaluate captures or three rendered DLSS frames.

## Closed and submitted recordings across the three intervals

Stopped-process journal prefixes contain **228, 424, 614 and 807** calls. Each
is an immutable prefix of the next, with zero pending/failed slots and zero
flags. The final stopped snapshot contains **2079 descriptions, 67 submitted
list labels and 101 queue events**. All CPU entry/return order values, plus the
Evaluate marker at **685**, cover 1..1615 without a gap. There are 465 completed
calls after that marker. The final running-process read is identical; acceptance
uses the preceding stopped snapshot.

Each interval between consecutive depth faults contains one producer recording
and one later depth-consuming recording. Their current Reset, successful Close,
matching ExecuteCommandLists call and next Reset were selected independently:

| Interval after corrected counter | Recording | Reset | Close | Execute | Next Reset |
| --- | --- | --- | --- | --- | --- |
| 18057 | Producer `0x159E4CF0` | 199 | 297 | 362 | 395 |
| 18057 | Evaluate list `0x159EC0E0` | 204 | 348 | 365 | 400 |
| 18058 | Producer `0x159E4CF0` | 395 | 464 | 557 | 586 |
| 18058 | Consumer `0x159EC0E0` | 400 | 543 | 560 | 591 |
| 18059 | Producer `0x159E4CF0` | 586 | 665 | 745 | 779 |
| 18059 | Consumer `0x159ED810` | 592 | 739 | 748 | 785 |

The third consumer is a different command-list address, so it was identified
through its resource barriers and recording generation, not assumed to be the
first Evaluate list. Only interval 18057 is independently bound to a captured
Evaluate contract. The other rows identify CPU recordings between observed
renderer counters, not completed GPU frames.

All six submissions used queue `0xD39ACD0`. Each producer Execute returned
before the corresponding consumer Execute entered, with an intervening
recorded cross-queue Signal/Wait pair using the same fence/value and successful
HRESULTs. No fence completed-value query was made. ExecuteCommandLists is void;
its stored result zero is a placeholder, not evidence of driver acceptance.

In submitted-list order each interval has six ordinary, whole-resource depth
transitions. The first begins at GENERIC_READ `0xAC3`; subsequent intervals begin
at the preceding declared `0xC0`:

`initial → COPY_DEST 0x400 → PIXEL_READ 0x80 → COPY_DEST 0x400 → combined read 0xC0 → COPY_DEST 0x400 → combined read 0xC0`

The initial producer calls are 250/435/631; the final read declarations are
331/527/712. All adjacent declarations agree in submission order. No relevant
split/alias/global barrier invalidates that chain in the retained evidence.
This is declaration continuity and CPU submission evidence, not an observed
execution state or proof of texture contents/lifetime.

## Preserved evidence and next step

`local/read-ready-depth-20260922/review_46868.py` independently checks unique JSON
keys, every fault/correction, immutable prefixes, complete CPU order numbering,
resource binding, recording generations, submissions, declaration continuity,
first-mailbox/text agreement and the settings recovery. Its report is
`review-46868.json`. The existing build and all 25 tests remain unchanged.

| Artifact | SHA-256 |
| --- | --- |
| `retry-46868.jsonl` | `49170ED468147812B7DC375AADBCF6C990100A1BC4DAF695E7AC42A76124FB58` |
| `bridge-46868.crash.log` | `31A719EA46CC8021207B96BD5F1100907D71516AFB9720BE52BB5FBA5587F70F` |
| Exact saved-On settings | `0465CA1EBA56493E4946AB825CABD1E46653647E09CC3DC420D86372AC8768C9` |

After confirmed game exit, the saved-On settings were archived and only DLSS was
reset Off. TAA, DX12 On and FullScreen 2560x1440 remain saved. The installed
bridge and archived recorder/backup hashes are unchanged from
[the v0.6.1 preparation](read-ready-depth.md). No new runtime binary is staged.

**Do not repeat the unchanged three-frame test.** The next diagnostic should
capture a bounded sequence of actual Evaluate inputs and tie the relevant
submissions to observed fence completion. It must preserve independent frame
bindings even when command lists/output buffers rotate. Motion/jitter alignment,
depth convention, exposure and resource contents still need validation before
modern DLSS/DLAA integration. Increasing the correction budget alone does not
resolve those missing observations.

The follow-up implementation is documented in [three Evaluate inputs and natural fence observations](multi-evaluate-fence-capture.md); game acceptance is pending.
