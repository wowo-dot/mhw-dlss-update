# Submitted native depth copies — run 6868

Accepted bounded observation, v0.7.1, 2026-09-24. The recorder observes **52
native full-resource depth copies in 13 repeated recording cycles**. Every copy
has a unique successful Reset/Close/Execute association on the DIRECT queue and
a local COPY_SOURCE declaration. The per-call resource-retention correction
completes without the previous warmup failure.

This is copy argument, resource identity, recording and submission evidence.
No game depth pixels were read, no jitter was injected, and no GPU commands
were added. `depth_copy_readback_ready`, GPU contents, complete cross-queue
resource safety and raster-jitter verification remain false.

Executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256 `D78DFC5A1917C298CCD318771553EF2494234E536C4045C7685AEC1D654475F6`.
Archive: `local/projection-binding-20260923/v0.7.1/`; `acceptance-6868.json`,
`copy-history-6868.json` and `archive-6868.json` seal the review, stage map and
restoration. `accept_6868.py` reproduces the review and asserts the repeated
pattern against the raw provenance channel. Preparation receipts remain an
unchanged record of the state before this game run.

## Resource and draw association

The selected frame/epoch is 14375. The depth resource is 356307664, canonical
identity 356307616, device 354141072: 2560×1440 R32_TYPELESS, one mip, array
slice and sample, ALLOW_DEPTH_STENCIL. The D32_FLOAT view and all three core
resource-query phases agree with the copy observer's independently retained
resource metadata.

Both destinations are 2560×1440 R32_FLOAT, one mip/slice/sample,
ALLOW_RENDER_TARGET. They are game textures, not readback buffers.

| Label | Resource | Canonical identity | Observed copies into it |
| --- | --- | --- | --- |
| A | 5346197312 | 5346197264 | 39 |
| B | 5346207104 | 5346207056 | 13 |

All 52 calls are `CopyResource`, returning at native RVA `0x259E100` within
the byte-gated wrapper. This capture covers the known native call sites,
not every possible copy in the renderer. Object addresses are run-local
evidence and must not become discovery constants.

The selected draw marker 5519 is in list 356101616, Reset 5189 / Close 5523,
submitted at Execute 5555 as index 0 of a seven-list DIRECT batch. The
immediately preceding DIRECT Execute 5547 contains the depth transition
`0x800 → 0x10` and matching clear. The existing broader review accepts the
same-queue depth-write declaration; the narrower local-list and selected-batch
prefix checks remain false with their original reasons.

## Four-copy stage map

The following table is ordered by submitted batch and list-array position,
then by calls within each list. It is **not a whole-list GPU serialization
proof**. CPU entry order differs because recording occurs on multiple threads.

| Execute / list index | Reset / Close | Copy entry | Destination | Local depth state |
| --- | --- | --- | --- | --- |
| 5555 / 2 | 5177 / 5426 | 5230 | A | `0x840` COPY_SOURCE + NON_PIXEL_SHADER_RESOURCE |
| 5555 / 2 | 5177 / 5426 | 5265 | B | `0x840` |
| 5561 / 0 | 5191 / 5511 | 5479 | A | `0x840` |
| 5561 / 1 | 5187 / 5543 | 5353 | A | `0x800` COPY_SOURCE |

In all 13 cycles the early batch contains seven lists and the later batch
contains two. The depth transition declarations, in the same submission-array
order and including the preceding clear batch, are:

`0x800 → 0x10 → 0x840 → 0x10 → 0xE0 → 0x10 → 0x840 → 0x10 → 0x800`.

For the selected cycle, barrier 5351 declares depth `0x10 → 0x800` and A
`0xC0 → 0x400`. Copy 5353 follows on that same recording; barrier 5444 later
declares A `0x400 → 0x80`. No later depth transition occurs within that list.
This unique pure-COPY_SOURCE stage is a **structural candidate** for readback
from the original depth resource. It is not yet identified as final-scene
depth. A's identity alone cannot select the stage: A is overwritten three
times per observed cycle. Selecting the last CPU copy event would also choose
the wrong list position in this frame.

The following DIRECT Signal 5567 sets fence 354403680 to 14393; native
completion read 6057 reaches 14393. The existing bounded reviewer accepts
`selected_queue_fence_completion_observed=true`, including the contained Wait
inside later Execute 5561. This preserves its recorded-window scope and does
not establish all resource accesses on other queues or actual pixel contents.

## Cleanup and next experiment

The capture contains 55 view events, 2,972 history calls, 7,087 barriers, 113
submissions, 211 submitted-list entries and 143 copies involving ten distinct
copy resources. All 286 per-call copy references were released after drain,
along with retained list/queue/fence references. There are zero errors, faults,
drops or pending calls; all hooks disabled cleanly. The user exited normally.

The exact previous v0.15 DLL was restored, the output sidecar removed, and all
four configuration hashes remain unchanged. All 59 archived build sources
verify. Preparation passed Release `/W4 /WX`, 131 native checks, 390 Python
checks and 30 owned journals; those are preparation results, not new test runs
after this capture. The earlier v0.7.0 failure remains rejected and archived.

Next: design one bounded readback at the original depth's pure-COPY_SOURCE
window. Admission must qualify the live resource/view, dimensions, format,
device, recording generation and preceding transition pattern; this run's
addresses and event numbers are not a selector. Copy a full subresource into
an owned readback allocation, without changing the game's source state. Tie
it to the actual containing submission, use an owned fence, and retain all
resources until completion before mapping. Missing invariants must prevent
insertion; post-insertion failure must not release in-flight resources.

Pixel validity, depth range/coverage and scene association come before any
jitter-displacement conclusion. No unchanged v0.7.1 rerun is needed, and the
game readback experiment is not yet implemented or deployed.
