# Selected draw submission and preceding depth work — run 47376

2026-09-24, [recorder v0.5.0](../runtime/projection-submission-recorder.md).
The selected projection-consuming draw now has an observed successful
Reset-to-Close generation and an actual returned DIRECT-queue submission.
Its depth view retains the previously established creation/lifetime checks.
Depth-state qualification and GPU execution/completion remain incomplete.
The game exited normally; the previous diagnostic DLL is restored, its
temporary output sidecar is removed, and four configuration hashes are unchanged.

## Accepted observations

PID `47376`, creation time `134347322751535317`; executable version `0.0.0.0`,
SHA-256 `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256: `116A02A7BBA8111DE2013B26F4D1B528521277267C6AB8C8433EE2C05E62E8F9`.
All five startup and 39 core gates passed (43 distinct spans); 33 hook targets
were installed. The source frame/epoch is `10691`, root parameter 9 / native
VS b0, recorded indexed draw `(2520, 1, 2586, 0, 0)`.

The depth-only draw uses the actual writable D32_FLOAT/TEXTURE2D/mip-0 view
`[40,3,0,0,0,0]` of a 2560×1440 R32_TYPELESS resource, one mip/slice/sample,
ALLOW_DEPTH_STENCIL. Native recipe `[2,0,1,17,0]` is separate evidence.
Resource interface `356482576`, canonical IUnknown `356482528`, device
`354203040`; all three resource-query phases agree. The accepted creation
is journal slot 37, nested in constructor slot 36, orders 72/73 inside 71/74.
The earlier same-handle creation is not mistaken for this generation.

| Selected-list evidence | Shared-journal entry/return order |
| --- | --- |
| Successful actual Reset | 4307 / 4308 |
| Fresh depth binding | 4613 / 4614 |
| Draw marker / native draw return | 4615 / 4616 |
| Post-draw ResourceBarrier call | 4617 / 4618 |
| Successful actual Close | 4619 / 4620 |
| Returned ExecuteCommandLists | 4633 / 4634 |

List `356222576` is first in the seven-list batch. Queue `208108896` has
canonical identity `208108848`, the same device, and actual description
`[DIRECT, priority 0, flags 0, node mask 1]`. Its generation contains the
selected draw; no intervening Reset is substituted. The post-draw barrier
does not establish depth state before the draw. Replay accepts
`selected_recording_submitted=true`, while the broader GPU-related fields
remain false.

## Where the earlier depth work appears

The immediately preceding returned Execute on this same DIRECT queue is
orders `4629/4630`, containing list `356175088`. Its successful Reset is
`4291/4292`, Close `4611/4612`. That complete generation includes:

- Orders `4460/4465`: the selected resource transitions from `0x800` to
  `0x10` (DEPTH_WRITE), all subresources.
- Orders `4609/4610`: the selected DSV is cleared with flags 3, depth bits
  zero (0.0f), stencil zero and zero rectangles. These are observed arguments,
  not certification of stencil support on D32 or of GPU results.

A COPY submission occurs at `4625/4626`; a COMPUTE submission at `4631/4632`
falls between the two DIRECT submissions in CPU callback order. The recorder
does not capture queue Signal/Wait or fence completion. No matching depth
transition appears in the reconstructed COPY/COMPUTE generations, but absent
barriers do not prove those queues cannot access the resource.

The v0.5.0 state reviewer intentionally covers only the selected Execute
batch's prefix. Here the selected list is first, so its incoming declaration
would need the preceding batch. The additional observed queues also trigger
the conservative `other queue submission requires synchronization evidence`
reason. These are scope limitations, not evidence of an invalid game renderer.
Keep `local_depth_state_declared=false` and
`submitted_depth_state_declared=false`; do not relax them based on CPU timing.

## Capture bounds and archive

The clean capture contains 55 view events, 2,260 history calls, 6,535 barrier
records and 193 list entries in 102 submissions. It pins 23 lists and three
queues, then releases all references after drain. Submission counts are
76 DIRECT, 13 COPY and 13 COMPUTE. History has 195 Reset, 196 Close, 1,707
ResourceBarrier and 60 depth-clear calls; no ExecuteBundle. Event-entry ticks
span 125 ms, which is not a GPU timing measurement.

The selected depth resource has 96 transitions on six lists and 12 matching
clears. Reconstructed complete submitted generations contain 92 of those
transitions, all on DIRECT. Thirteen submitted recordings lack an observed
Reset at the start of the capture; their prefixes stay unknown. This limits
any attempted replay of the entire window, independently of the complete
adjacent generations identified above.

Core and history have zero faults/drops/pending, hooks disabled, and clean
reference release. The process remained responsive after capture and the
user reported normal exit. No jitter writes or rendering commands were added.
Preparation passed 110 native and 205 Python checks.

Private archive: `local/projection-binding-20260923/v0.5.0/`.
`review-47376.json`, `submission-history-47376.json`, `acceptance-47376.json`
and `archive-47376.json` seal the observations, 47 archived sources and rollback.
The recorded build/source snapshot remains unchanged by these result notes.

Primary log SHA-256:
`EAA24974F9C75E0BB7685D1B5951098ACBBABCF210538BEBEF2E8E501C1A1154`.
Companion SHA-256:
`2505998F2FC2FDC9C3542C729DA55BBF78CAD305EDEC01A1D14ACBBCC2462A6B`.
Restored DLL SHA-256:
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.

Next: audit these retained adjacent DIRECT batches offline and identify the
queue Signal/Wait and completion observations needed to extend qualification.
An unchanged v0.5.0 rerun cannot recover unobserved synchronization. Preserve
this runtime verdict while designing that extension. GPU contents, raster
displacement, executing PSO bytecode and working DLAA/DLSS remain unverified.
