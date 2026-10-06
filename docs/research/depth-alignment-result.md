# Matching projection and depth-copy request: v0.8.2 run 42232

Captured across the 2026-09-24/25 local midnight boundary with DX12 On, DLSS Off
and TAA On in a loaded save. The unchanged archived v0.8.2 reviewer accepts the
full capture: the selected projection and original depth-copy request both have
frame/epoch **51739**, bank 1 and async mode 1. The selected draw's submitted
recording precedes the depth copy on the same DIRECT queue. No observed depth
clear, overwrite, applicable alias or bundle invalidates that interval.

This establishes the zero-jitter alignment control described in the
[runtime design](../runtime/projection-aligned-depth.md). It does not establish
every draw contributing to the saved pixels, final-scene coverage, cross-queue
resource safety or raster jitter. Those review fields remain false.

Executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256
`382092D20A11C6ABEAD23B1DF141ED07374414B5AE6BE97071CBDA5720F23611`.
Process generation: PID 42232, creation FILETIME 134347568568738369.

## Confirmed association

| Observation | Recorded evidence |
| --- | --- |
| Immutable source selection | Provenance order 4325, core order 1; frame/epoch 51739, bank 1, async mode 1 |
| Original copy request | Producer orders 4332–4333, same full stamp and frontend thread 17840; queue count 50→52 |
| Packet | Pointer 3274458848, opcode 10, kind 8, full 2560×1440 rectangle |
| Worker consumer | Orders 4647–4721, thread 44440, receipt 40, unchanged packet contents |
| Native copy | Orders 4673–4708, source 359274544 / canonical identity 359274496, destination 2931283280 |
| Added readback recording | Orders 4715–4717, Reset 4484, adjacent barrier 4666, Close 4858 |
| Selected draw recording | Marker 4834; command list 359007088, Reset 4486, Close 4838 |
| Selected submission | Execute 4870, batch index 0, DIRECT queue 209657504 |
| Copy submission | Execute 4876–4879, batch index 1, same queue |
| Private completion | Signal 4880–4881 to value 1; observed complete and later successful Reset 4994–4995 before Map |

Both the native and added depth copies were recorded before the selected draw.
Submission order places them afterward. This is the previously predicted case
that the old CPU-order rule skipped. The new recorder does not apply a fixed
frame offset or substitute a copy-worker global frame sample. It ties the
readback to the immutable selected source and original request, then qualifies
the selected draw/DSV association before its private Signal.

The reviewer checks nine recordings between the selected draw and copy,
including the endpoint recordings. Fifty-two producer receipts cover four
requests per frame across frames 51730–51742. Forty-nine consumers link to
receipts; none are orphaned. The last three producer receipts have no captured
consumer and remain unmatched. Only the selected association authorizes pixels.

## Pixels and cleanup

One readback copy and one private Signal save 14,745,600 bytes: 3,686,400 finite
depth values at 2560×1440, 8,267 zeros, no nonfinite or out-of-range values.
Minimum is 0 and maximum is 0.06954862922430038. Pixel SHA-256:
`98f18253ba07fb9665ce240de45cd9dda377884e851c87ef2feaf483bab13376`.
The preview shows coherent character/weapon, floor, columns, spiral stairs,
railings and hanging fixture. Logarithmic coloring does not represent linear
world distance. The picture alone does not prove its frame origin.

The journal contains 55 watch events, 2,888 history calls, 6,693 barriers,
111 submission calls, 202 submitted list entries, 133 copies and 49 depth
copies. All 266 observer copy references release after drain. Private readback
references remain retained until process exit. Hooks disable with zero faults,
drops or pending calls and no fatal stop. No jitter writes or provider calls
were made.

The archived build and all 77 source identities verify. Preparation passed
Release `/W4 /WX`, 159 native and 504 Python checks. Evidence is under
`local/projection-binding-20260923/v0.8.2/`, including the unchanged review,
`packet-analysis-42232.json`, raw pixels and `depth-preview-42232.png`.
Process exit was observed before rollback. The exact prior v0.15 DLL was
restored, the output sidecar removed, and all four configuration hashes are
unchanged. `acceptance-42232.json`, `archive-42232.json`, `exit-42232.json` and
`restored.json` seal that evidence. The archive does not infer how the game
exited: the user's normal-exit confirmation was still pending when sealed.
Capture, review, restoration and archive are complete.

## Next decision

No unchanged v0.8.2 rerun is needed. Combine the accepted bounded projection
stimulus with this request/queue association to capture corresponding baseline,
stimulus and restoration pixels. Each sample needs its own source/request stamp,
resource identity, recording/submission order and completed readback; a later
frame cannot substitute for a missing sample. Validate that extension on owned
GPU fixtures before another game launch.

Compare static scene regions for the predicted displacement and recovery while
accounting for animated geometry. This is the next test of whether the scoped
matrix change reaches rasterized depth. It is not yet working DLAA, and the
single zero-jitter capture does not establish temporal reconstruction quality.
