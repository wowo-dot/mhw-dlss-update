# Run 49944: fence observation works; global checkpoint stops the sequence

2026-09-22, bridge v0.8.0 / recorder v0.7.0. This is a **partial diagnostic
result**, not acceptance of three Evaluate captures or working DLSS.

The exact staged bridge loaded in `capture-three-evaluates` mode; both provider
preflights and presentation observation passed. The recorder opened all three
idle mailboxes and the idle journal before reporting ARMED. Process creation
FILETIME was `134345738531093028`. The user enabled DLSS once, left Options and
reported a crash. The game subsequently exited.

## What happened

The first fault at renderer counter **19848** (`0x4D88`, tick 27291578) passed
the 14 code gates and tracker checks. One RDX-only correction was verified.
The next counter was **19849** (`0x4D89`, tick 27291609), on the same preparation
thread 34392. Both tracker snapshots reported complete, same-epoch state `0x4`.
The second correction was rejected as `capture-channel-precondition`, with two
corrections still available. Normal exception handling was preserved and the
recorder detached. This was not a spent-budget stop or a new fault location.

Slot zero contains Evaluate sequence 69 at tick 27291593, thread 46244, token
`0x10000B12D`, command list `0x1593BEA0`. Caller, copied handle, binding and
boundary checks passed. All 17 getters and 26 inspection calls completed;
ten acquired references had ten completed releases. Color/motion/depth/output
were same-device 2560x1440 resources with formats 26/16/41/26. PreviousOutput
was null. The return remained the intentional `0xBAD00012`. Slots one and two
stayed idle because the recorder stopped before their Evaluate calls.

The immutable snapshot correctly reports **in_flight=true**, therefore
**coverage_complete=false** and **qualified=false**. Its checkpoint has order
546, 273 reserved calls and only 263 consecutive completed calls. In the final
journal, call **264** is a ResourceBarrier on another command list
(`0x15941B60`, thread 47168), with entry 527 and return 555: it crosses the
Evaluate marker. Its eight transition resources match none of the captured
inputs. Calls 265–273 had already returned before the marker, but the first
pending slot prevented the global prefix from including them.

The full stopped journal later proves this particular overlap was unrelated
to the captured inputs. That fact was unavailable to the current prefix reader
at its checkpoint. Do not relabel the original snapshot as qualified or remove
the overlap check. The next implementation should distinguish incomplete
evidence for the captured recording/resources from unrelated parallel work,
using published records and preserving rejection of relevant or unknown gaps.

The captured list's generation is Reset 215, Close 319, Execute 342, next Reset
396. A shader-read declaration for its depth input is in call 240. Other lists
record depth transitions on overlapping CPU timelines; recording order across
threads must not be substituted for queue execution order.

## Fence evidence and its limit

Stopped journal prefixes contain 231 and 412 completed calls, with the first
prefix unchanged, zero pending/failed records and zero coverage flags at both
stops. The final running read matches the final stopped read. The 412 records
contain 839 barrier descriptions, 20 Execute calls, 17 queue Signals, 16 queue
Waits, **68 GetCompletedValue queries** and **two CPU fence Signals**. All
entry/return orders plus the one Evaluate marker cover orders 1–825 exactly.
No query returned the removed-device sentinel.

There are observed increases from lower baselines to earlier queue-signal
values, including Signal 141 / query 345 on fence `0x15949AA0` (19871), and
Signals 172/184 / queries 182/185 on fence `0x4D72BBD0` (1/2). These remain
conditional on object identity/lifetime and absence of unobserved external
writers; the diagnostic does not monitor fence creation/destruction or shared
writers. The two observed CPU Signals use another fence, `0xD20F5C0`.

For the captured Evaluate list's Execute **342**, the observed later signals
and queries do **not** establish completion:

| Queue Signal call | Signalled value | Later observed value |
| --- | --- | --- |
| 344, fence `0x15949AA0` | 19872 | 19871 |
| 363, fence `0x4D72BBD0` | 3 | 2 |
| 385, fence `0x4D7291A0` | 1 | No query |
| 388, fence `0x159BB800` | 19848 | 19847 |

These observations show that the added hooks capture natural fence progress.
They do not prove that this Evaluate submission completed, that it stalled, or
that its resource contents are valid. The process stopped early in the planned
three-correction experiment.

## Preserved evidence and recovery

`local/multi-evaluate-20260922/review_49944.py` independently checks identities,
immutable prefixes, CPU ordering, stop reason, exact overlap, inspected inputs,
recording generation and conservative fence evidence. It writes
`review-49944.json`; its assertions pass.

| Artifact | SHA-256 |
| --- | --- |
| `retry-49944.jsonl` | `6FD8A020EA45DA91C894BD9A00D764F42A5C1D3785BE143D041BA6D1894A337E` |
| `bridge-49944.crash.log` | `4A1FB964BDE32DF87CAE0636AD00D8752C472250F8287CA80F92B2470D0D26E2` |
| Saved-On graphics settings | `0465CA1EBA56493E4946AB825CABD1E46653647E09CC3DC420D86372AC8768C9` |

After confirming game exit, the log/settings were archived and only DLSS was
restored Off. The original pre-test settings hash
`02BAB1311AD739420182D2FF5DB8A6ED9536235E80B51D2B736A6232633CB235` was restored
exactly. The staged binaries and sidecar remain unchanged from the
[v0.8.0 preparation](multi-evaluate-fence-capture.md). No new DLL is staged.
Keep the game closed for the checkpoint revision; repeating the unchanged test
would leave the same concurrency limitation. No correction-budget increase is
needed.

Follow-up: [v0.8.1 stopped-process checkpoint review](scoped-checkpoint-review.md) passes the recorded failure replay and local tests. Its game acceptance is pending.
