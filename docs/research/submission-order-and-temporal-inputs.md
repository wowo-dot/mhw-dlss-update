# Submitted recordings and temporal-input capture

2026-09-23, offline follow-up to [v0.10.0 run 47820](guarded-depth-probe.md).
The saved journal contains the expected resource-state declarations before
Evaluate when reconstructed in submitted recording order. It also contains a
same-queue signal and a subsequent completed-value observation reaching that
signal. Static analysis locates two frame-indexed sources for `fJitterOffset`.
These narrow the next diagnostic; they do not qualify a modern rendering path.

The game stayed closed throughout this analysis. No native source, installed
DLL, mode sidecar or game setting changed. Modern DLSS remains unimplemented.

## Evidence and identity

Original executable version **0.0.0.0**, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
The decrypted analysis image has SHA-256
`D78611731E4A5F214E306D1D577D4782B59ADAF1C75DE54D2983CAA45074884C`.
Both were rehashed before deriving the new static locations. RVAs below apply
only to that image; they are research locations, not portable hook signatures.

The immutable receipt is `local/guarded-depth-20260922/channels-47820.json`.
Its Evaluate JSON is 14,211 bytes, SHA-256
`1E7C72241BB8BD11126536C142C2F780BA66DAA272147E36FFFAACC382E6D15F`.
The journal is 753,832 bytes, SHA-256
`EF4A973175C0F07E2A009AD42D492920920724A31EC0BC25968538E2FE0D67C0`.
The independent replay verifies those payload hashes, process generation,
producer/schema, record checksums and capture/resource binding.

Results, static verification and local disassembly are retained under ignored
`local/submission-review-20260923/`. The final replay is
`submission-47820.final.json`. Proprietary images and instruction exports stay
local and untracked.

## Recorded declarations at Evaluate

CPU callbacks from different worker threads interleave. The selected Evaluate
marker has order 672, but some barriers needed by its work were recorded later
on another list that precedes it in the same submitted batch. Replaying only
callbacks that completed before the CPU marker loses those declarations.

Submission **395** contains four lists. The relevant prefix is:

| Batch index | Reset call | Close call | Relationship to Evaluate |
| --- | --- | --- | --- |
| 0 | 246 | 336 | Earlier list; no transition naming the four captured resources |
| 1 | 242 | 369 | Earlier list containing color, depth, motion and output transitions |
| 2 | 243 | 376 | Evaluate list; replay only its commands before the marker |

The selected list's next Reset is 443. The same pointer appeared in an earlier
submission, so matching pointer identity without its recording generation
would associate the wrong work. No selected-list callback crosses Evaluate.
The one callback crossing the global marker, call 335, belongs to batch index
1 and names none of the four captured resources.

| Captured role | Relevant transition calls in submitted order | Last declaration before Evaluate |
| --- | --- | --- |
| Color | 366: `0xAC3 → 0x4`; 367: `0x4 → 0x40` | `0x40`, non-pixel shader resource |
| Motion | 368: `0 → 0x8`; 274 on the Evaluate list: `0x8 → 0x40` | `0x40`, non-pixel shader resource |
| Depth | 327: `0x400 → 0xC0`; 337: `0xC0 → 0x400`; 356: `0x400 → 0xC0` | `0xC0`, pixel and non-pixel shader resource |
| Output | 367: `0 → 0x8`; UAV barriers at 368 and 274 | `0x8`, unordered access |

Six transitions in that table were recorded after the CPU marker on batch
index 1. They still precede the selected list in the submitted stream. Output
transition 370 (`0x8 → 0x800`) occurs after Evaluate on the selected list and
must not be used as its input state. PreviousOutput is null.

These are **declared states**, conditional on the observed recording and
resource identities. Initial states, resource lifetimes, unobserved uses and
cross-queue dependencies remain unverified. Multiple lists in one
`ExecuteCommandLists` call can overlap on the GPU; batch position is not a
whole-list completion guarantee. See Microsoft's
[submission semantics](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists).
The replay does not insert decay between lists in that batch and does not
treat a UAV barrier as a state transition. Those distinctions follow the
[resource-barrier rules](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12).

## Retained fence observation

After submission 395, **queue Signal 397** successfully records value **11522**.
GetCompletedValue 398 returns 11521; **query 596** reaches 11522 on the same
fence. Every recorded writer of that fence before query 596 is a successful,
monotonically increasing signal from this queue. There is no recorded CPU or
foreign-queue writer for that fence in this interval. The API intervals used
to establish submission-before-signal do not overlap.

This is an observed submission/signal/completed-value chain, with unobserved
writers and lifetime reuse still excluded from the claim. It does not prove
correct resource contents or frame alignment. The query's 15 ms tick delta
from submission is coarse CPU observation time, not measured GPU duration.
The replay rejects `UINT64_MAX`, which
[GetCompletedValue documents as device removal](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12fence-getcompletedvalue).

The journal retains 2,048 valid completed records, zero pending/failed records,
and **overflow flag 1**. Its retained interval is only 250 ms despite the
configured two-second window. Query 596 precedes overflow, but this does not
clear the original coverage flags or change `qualified=false`. The existing
runtime checkpoint policy is unchanged. The offline report always emits
`gpu_safe=false`.

## Frame and jitter sources found statically

The original TAA producer caller `+0x2392150` reads the renderer's qword frame
counter at `+0x1F210`. It derives history indices modulo 3 and passes them,
the view, rectangle and reset input to optional producer `+0x23913F0`.
The producer emits a 64-byte work payload. Its selected output history owner,
rectangle and reset-related value reach consumer `+0x2383CF0`; the current
probe does not retain their complete producer-frame provenance.

The `mTUJitter` property is a **mode selector**, not the jitter vector. Getter
`+0x2299390` reads the renderer dword at `+0x7B424`; setter `+0x229C7B0` writes
it and marks dependent state dirty. In the dispatch region at `+0x23C6C5D`,
the selector chooses the value source bound to `fJitterOffset`:

| Mode | Accessor RVA | Returned address |
| --- | --- | --- |
| 1 | `+0x227F7B0` | `table_object + 0xE740 + 8 * (frame % 18)` |
| 2 | `+0x227DB60` | `table_object + 0xE6A0 + 8 * (frame % 20)` |
| Other | Static source | Image `+0x503C3E8`; contents not qualified here |

The renderer pointer is obtained from image `+0x51C4480`. The table object is
obtained from a different global, image `+0x51C4368`. Both accessors use the
same renderer-frame field as the producer caller. The eight-byte table stride
is consistent with two floats; the actual binding's encoded component count,
active mode, live values, units, sign and projection relationship are not yet
captured. The general constant-copy helper `+0x898EE0` derives component count
from the encoded parameter, so a name or stride alone is insufficient.

Static accessor spans are pinned by SHA-256:

| Span | Bytes | SHA-256 |
| --- | --- | --- |
| Mode getter `+0x2299390` | 7 | `A64D4FC3D032CD443D7083FB7A71BA2E8A99ABB9A9847C4C9B3C1732BE02F638` |
| Mode setter `+0x229C7B0` | 28 | `1ECAF88A5F3040CFC25DD4F353507F8C4C31CDDE2E5B9E95EA16223F88DC56E8` |
| Mode-1 accessor `+0x227F7B0` | 53 | `9CF3D0002A218375575109ACF8BD205121D1A93A2144108DD30B6D0BEF7F5F1C` |
| Mode-2 accessor `+0x227DB60` | 52 | `D9A8D5C90857455446E900CEAE4CE7A176B875EA02D2BDB8C4FF34B03536E5E6` |

The local verifier rechecks image identities, decodes these spans and records
RIP-relative targets. It verifies the unsigned reciprocal-division bounds and
104,000 frame values per mode, including uint64 wraparound arithmetic. This
validates the address formulas, not the runtime contents of either table.
No new hook was installed at these locations; runtime byte/structure checks
would be required before using them in a future diagnostic.

## Next diagnostic design

Keep the accepted **one optional item / one Evaluate** budget and native TAA
fallback. The next implementation should collect an immutable packet and stop
the journal at an explicit, bounded completion condition:

1. At producer admission, retain frame-counter reads bracketing the snapshot,
   both history indices and the selected branch, view identity, rectangle,
   reset inputs, resource-owner identities and context epoch. Include the
   selector and bounded raw jitter-table sample, with stable object/frame
   checks. An asynchronous renderer sample is not a substitute.
2. Bind that packet to the exact queued payload/descriptor, preparation
   context, consumer invocation and Evaluate resource set. Compare the
   captured frame/epoch at each applicable stage; reject mismatches. Re-reading
   a mutable global during consumption must not relabel an older work item.
3. Bind the selected command-list recording through Reset, Evaluate, Close,
   submission and same-queue signal. When a natural completed-value query
   reaches the bound signal, close journal admission, drain already admitted
   callbacks, and publish an immutable termination reason/frontier. Retain
   deadlines, capacity limits and every failure/overflow flag. Ambiguity,
   missing completion, device changes or competing writers must remain failed
   or incomplete captures. Record 596 in this run is evidence that a focused
   window can fit; it is not a universal capacity guarantee.
4. Compare the captured table sample with the value actually bound to
   `fJitterOffset` for that frame before asserting shader jitter alignment.
   Establish units, axis/sign and projection behavior, followed by motion and
   depth contents. Keep GPU readback and modern SDK invocation as later,
   separately validated changes.

This design was implemented after this analysis in the
[v0.11.0 frame-packet probe](frame-packet-probe.md); game acceptance remains
pending. No unchanged v0.10.0 rerun is needed for this offline review.

## Reproduction and validation

The analyzer uses Python's standard library and never opens the game process:

```text
python tools/review_submission.py --receipt local/guarded-depth-20260922/channels-47820.json --output local/submission-review-20260923/reproduced.json
python -m unittest discover -s tests -p test_submission_review.py -v
```

Output creation is exclusive to preserve previous evidence. **34 synthetic
tests pass**, covering corruption, identity/binding, missing or reused
recordings, overlapping callbacks, split/subresource/aliasing barriers, state
discontinuity, CPU/foreign/rewound fence signals and device removal. The real
capture replays to the table and fence chain above while retaining all flags.
The existing 33 native tests and binary audits apply to the unchanged v0.10.0
build; this offline-only change did not require rebuilding or redeploying it.
