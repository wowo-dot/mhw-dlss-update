# Live owned GPU inputs — accepted run 51256

Completed and closed 2026-09-25, recorder **1.10.0**. The first in-game private
GPU snapshot and conversion run passed its frozen independent reviewer.
Frames **25383–25385**, 2560x1440, DX12, adapter LUID 0:97816, swap-chain format
87. User reported a loaded save and later confirmed normal exit.

## Verified outcome

All three selected depth/color/motion sets were copied directly from the native
GPU resources into private GPU textures at the existing qualified intervals.
Both raw and private copies share each source's bounded recording interval.
The original resource state was restored before forwarding continued.
Exact frame, packet, command-list, allocator and Reset-generation receipts,
producer completion and retirement all qualified before private consumption.

The worker converted those private textures using its own explicit typed
SRVs/UAVs, lists, queues, heaps and output resources. All three private fences
completed at value 1; completed recording allocators/lists retired before CPU
mapping. No native descriptor bindings or device debug settings changed.

| Output | Per-frame dimensions/format | Independent result |
| --- | --- | --- |
| Color | 2560x1440 RGBA32_FLOAT, alpha 1 | Exact R11G11B10_FLOAT decoding |
| Depth | 2560x1440 R32_FLOAT | Byte-identical to matched raw depth |
| History motion | 2560x1440 RG32_FLOAT | Exact signed half decoding, low-bit mask and pixel scaling |

**77,414,400 float values** match the independent NumPy reference exactly.
Every output is finite; depth is within [0,1]. The nine converted files total
309,657,600 bytes; nine raw reference files total 132,710,400 bytes.
The three owned sets account for 775,618,560 bytes of GPU/default/readback image
allocations, plus 132,710,400 bytes of original reference readbacks.
This bounded diagnostic retains issued image references until process exit;
these allocations are not a proposed continuous runtime memory budget.

## Recorder closure and scope

All faults and pending callbacks are zero, hooks disabled, recorder accepted
and settled. Producer completion was observed by +219 ms from observation start.
The journal has 1,074 events: 433 Resets, 433 Closes, 193 Executes, 12 native
copy markers and three selected TAA markers.
Metadata reference releases: producer 24/24, consumer 3/3, native depth routes
36/36, submission lifecycle 249/249.

There are 15 depth requests, 12 matched native copies and nine qualified native
copies. Three frame-25386 tail copies lack observed Execute/retirement after the
selected frames and remain explicitly unqualified. Three requests were
unconsumed; 109 depth consumers were outside matched requests. The original
capture-wide writer coverage is still incomplete.

This verifies the live owned color/depth/motion handoff and typed conversion.
It does not establish the engine's opaque descriptor contents, SDK evaluation,
continuous temporal reconstruction, exposure choice or presentation. Broad
legacy flags remain false; the narrower live-owned-copy, typed-load and
motion-conversion flags are true. Depth/color timing and motion convention
retain their earlier independently scoped calibration.

## Evidence and rollback

Private directory: `local/temporal-owned-live-20260925/v1.10.0/`.
Host SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Recorder DLL SHA-256:
`24055de78d1154030b2595875327bbb6982a13c9963afea89b8bb90aa7f81c82`.
Log: `inputs-51256-134348263058124381.jsonl`, 263,609 bytes, SHA-256:
`ba8ef4566fb136ce62dc2189ad6a5cf9286d0f7a9d550faf48c71fb531f9ec32`.
Frozen review: `review-51256.json`, SHA-256:
`47004419fbf1b0ad7bcc49a5387af74a95fdfe840e291741fcca27312ab608a8`.

All 205 prelaunch files and 142 frozen sources verify. The closed archive has
232 files; `archive-51256.json` SHA-256:
`017f8520effcb35986ff794b811155d0fb1d0930e5a34978ec7e0933b3b824b5`.
Prior DLL restored at **19:17:03 +03**, its exact original hash verified,
temporary output sidecar removed, all four configuration hashes unchanged.
No next diagnostic is installed or armed.

**Next:** prepare the first bounded native-resolution SDK evaluation using
these private GPU inputs and explicit provider parameter contracts. Keep the
first result private and inspect its completion and output before continuous
jitter/history and replacement of the game's TAA output.
