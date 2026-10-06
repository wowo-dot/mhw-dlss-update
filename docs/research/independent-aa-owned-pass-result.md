# Independent AA: owned in-place pass result

2026-09-26: v0.2.0 passed eight live frames of owned callback insertion and exact
queue restoration with native TAA/FXAA Off. The callback ran at the native AA
position, phase 15/order 2, and its neutral Off behavior preserved every existing
color endpoint. This is the first live mod-owned pass in the independent-AA route.
It did not evaluate DLAA, request jitter or record GPU work.

## Run identity and checks

Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
PID 56352, creation FILETIME 134349167940833460. Deployed DLL SHA-256
`109f3902198d30e39785a2a2112bb80dca276add7c885563d635417a4cceed27`.
[ABI, ownership, code gates and offline tests](../runtime/independent-aa-owned-pass.md).

All 352 sealed prelaunch files, the installed DLL/sidecar, prior DLL backup and
four configuration hashes were verified before arming. All 326 sources were
frozen. Process generation and executable hash matched the startup record.
The user reported a loaded save. D3D12 discovery identified adapter LUID
98931:0, swap-chain format 87, Windowed 2560x1440. Native DLSS and AA stayed Off.

The independent frozen reviewer accepted all eight samples: frames 23695-23702,
thread 54768, first-to-last capture tick span 31 ms. Each frame had one distinct
owned descriptor and exactly one callback. Native queue count changed 4 -> 5
during the phase, then returned to 4 with its original descriptor ordering.

Inside the callback, all five nodes were sorted at orders 0/2/100/1024/134217713.
Every original descriptor's phase, flags, unit identity, input and output matched
the pre-insertion graph. The owned descriptor had input null and output equal to
scratch +c0, as required for the interior in-place shape. Its callback occurred
after 12 native packets had been appended in that phase on every sampled frame.
It added zero packets itself; the entire native phase still appended 31 packets.
These are CPU packet receipts, not GPU execution timing.

The same color owner/resource was observed in all eight callbacks. Actual
D3D12 resource queries, under temporary synchronous COM pins, verified:

| Property | Observed value |
| --- | --- |
| Device | Same device as renderer discovery |
| Dimension / extent | Texture2D, 2560x1440 |
| Format | R11G11B10_FLOAT (26) |
| Mips / array size / samples | 1 / 1 / 1 |
| Resource flags | Render target and unordered access (5) |

Owner scalar snapshots and the queue matched before and after the query. The
callback left the color untouched. Resource descriptions do not establish pixel
freshness, state at consumption, frame alignment of depth/motion, or lifetime
through deferred GPU execution. No objective image-quality claim follows, and
the user supplied no separate visual assessment for this run.

## Closure and evidence

Final status: eight visits/samples, fault 0, pending 0, callbacks drained, all
owned descriptors detached and the single hook disabled. The user confirmed
normal exit. Process absence was verified before exact rollback at 20:22:53 +03:
previous DLL SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`, test output
sidecar removed, all four configurations unchanged. No next build is installed
or armed. Original game executable/assets were untouched.

Ignored evidence root: `local/independent-aa-owned-20260926/v0.2.0/`.
`review-56352.json` records independent acceptance. Saved capture
`capture-56352.jsonl`: 43,350 bytes, SHA-256
`293ca7b7961ffe1743e2309aa275d261322dbfcd26cd567eb8db89abaa9c9af9`.
Prelaunch manifest SHA-256
`d3b4c6f0823dc7b438f628bb50ed5617eb9021d9518f327f9c504e9610933900`.
Prearm, arm, capture-save and exact restoration receipts are preserved alongside
the frozen source/build/test evidence.

The sealed final archive contains 365 verified files, with all 352 prelaunch
files and 326 frozen sources reverified. Native source/tests/tools still match
the launch snapshot. Final manifest SHA-256:
`9380e113e1f83b6a7c737c5245be473bde84887ed57ce30e6b2c414c3f100823`.

## Next work

The owned CPU callback and neutral Off route are now demonstrated in this loaded
scene. Do not repeat this same scheduling-only test. Connect the callback to a
versioned deferred render request and identify its GPU consumption point before
the downstream native effects. Prove color/depth/motion identity, states and
ordering there, and retain referenced resources through GPU completion and
command-list retirement. The existing conversions/SDK recording can then move
to this route, with the pre-projection controller owning jitter independently
of native TAA selection. Native AA suppression for other user selections and
actual-failure AA-Off recovery are still integration work, not completed by this
AA-Off-only capture. This run does not qualify transitions or resizing.
