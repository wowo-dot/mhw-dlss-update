# First game depth pixels — run 51816

Accepted bounded observation, v0.8.0, 2026-09-24. One added GPU copy reads the
qualified original depth resource into an owned READBACK buffer. The private
fence reaches one, the containing command list successfully Resets without an
observed intervening resubmission, and the worker saves **3,686,400 finite depth
values at 2560×1440**. The unchanged deployed reviewer accepts the complete
capture. No jitter was injected and no vendor SDK was used.

Executable version `0.0.0.0`, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
DLL SHA-256 `779947C7385909A6F57CAA00E1353A2A5BA83F3E50F100B52DC6E2A162780F45`.
PID 51816, creation FILETIME 134347517954817112. Local evidence remains ignored
under `local/projection-binding-20260923/v0.8.0/`; the raw pixels, extracted
shaders and rendered preview are not distributable repository assets.

## Copy and completion evidence

The selected projection draw has frame/epoch 58792 and marker 5085. Its original
depth object is 356609328, canonical identity 356609280: R32_TYPELESS,
2560×1440, one mip/slice/sample, ALLOW_DEPTH_STENCIL, with the qualified writable
D32_FLOAT view. All three core resource-query phases and the copy observer agree.
These object addresses are run-local evidence, not discovery constants.

| Event | Observed order / identity |
| --- | --- |
| Selected draw's DIRECT Execute | 5121, queue 354334928 |
| Readback list's successful Reset | 5243, list 356368192, allocator 356229264 |
| Adjacent native barrier | 5421: depth `0x10 → 0x800`, native destination `0xC0 → 0x400` |
| Native full CopyResource | 5425–5449, destination 1843213776 |
| Added full-subresource copy | 5452–5454, private buffer 16070485760 |
| Readback list's successful Close | 5597 |
| DIRECT Execute containing copy | 5615–5618, list index 1 |
| Private fence Signal | 5619–5620, fence 1295814048, value 1, success |
| Later successful list Reset | 5733–5734, captured recording retired |
| Worker completion and Map | completion value 1, tick 26337125, successful Map |

The copy uses source subresource zero, zero destination XYZ and no source box.
Its R32_TYPELESS footprint has offset zero, row pitch 10,240 bytes and 1,440
rows. The buffer contains 14,745,600 tightly packed little-endian float bytes.
Exactly one private Signal was attempted. The added copy uses the existing
COPY_SOURCE interval and adds no game-resource transition or modification.

The earlier selected draw also retains qualified view provenance, submitted
recording, same-queue depth-write declaration and bounded native fence-completion
observation. These are distinct from completion of the new private copy.

## Pixel result and interpretation

Pixel SHA-256:
`86c0ddbcb963c13ca072c8aed780d17202a710a82c00ef5c11cde4b2d1820cf2`.

| Statistic | Value |
| --- | --- |
| Finite values | 3,686,400 |
| Nonfinite / outside [0, 1] | 0 / 0 |
| Exactly zero / exactly one | 13,708 / 0 |
| Minimum / maximum | 0 / 0.06426510959863663 |
| Minimum positive | 0.00011401114170439541 |
| Median positive | 0.012358712032437325 |

`plot_depth_51816.py` renders the original resource row order with logarithmic
color scaling and zeros shown black. Inspection of `depth-preview-51816.png`
shows coherent scene geometry: a central character silhouette, floor, stairs,
railings and a hanging fixture. This is evidence of scene-shaped depth content,
not merely an allocation or a uniform clear. The color scale is raw nonlinear
depth, not reconstructed world distance.

The readback is a later CPU copy of the selected resource. The selected draw's
frame number does **not** establish the exact frame producing all copied pixels.
The final scene stage, complete resource access coverage across queues and
raster response to jitter remain unverified. The narrow fields
`depth_readback_completed` and `depth_pixels_observed` are true; the older broad
GPU-content/state/safety fields and `final_scene_depth_verified` remain false.

The companion contains 57 native copies involving selected depth. Fifty-six
have complete submitted generations (42 combined-state and 14 pure-COPY_SOURCE
copies). One at the window boundary lacks a complete submitted generation and
remains rejected individually; it is not the admitted readback copy 5425.

## Cleanup and reproducibility

The capture contains 55 view-watch events, 3,299 history calls, 7,658 barriers,
126 Execute calls, 231 submitted-list entries and 155 copies involving ten
resources. The history entry window spans 157 ms. All 310 native copy observer
references and its list/queue/fence references release after drain. Zero
faults, errors, drops or pending calls remain; hooks are disabled.

Private readback references have a separate lifetime: the buffer, fence,
source, list, allocator, queue and device remain retained until process exit
even after successful Map. Observer zero-pin counters do not report their
release. The user exited normally; the process is gone.

The exact previous v0.15 DLL was restored, the output sidecar removed and all
four configuration hashes match. `accept_51816.py` reproduces the saved review
using the archived deployed Python sources, verifies all 67 build source files
and rollback identities, and writes `acceptance-51816.json` and
`archive-51816.json`. The archive seals 25 evidence files, including raw depth
and its preview. Preparation passed Release `/W4 /WX`, 144 native checks and
433 Python checks; those are preparation results, not new runtime test counts.

Next: map the admitted copy stage to its exact producing frame, then design
bounded baseline/jitter/restoration depth comparisons. A paired experiment
must account for scene motion and prove that the injected projection belongs
to the copied pixels before measuring displacement. No unchanged v0.8.0 rerun
is needed, and this result is not working DLAA yet.
