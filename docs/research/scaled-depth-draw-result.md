# v1.23.1 live result: pixel-only depth source state

2026-09-27; PID 13188, creation 134350115518128752. Tested host SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`,
version 0.0.0.0. DLL SHA-256
`94ad8eaa7adacbf7e0d92e87f4c3e3691da97c38dd42ff2e6fe3dfce0b759624`.
The [prepared draw contract](scaled-depth-draw-capture.md) was too restrictive
about the source state. This run is a failed capture, not verified DLSS input.

## Confirmed observations

Frame 60616 has a stable half-scale stamp, temporal rectangle [0,0,1280,720]
and dispatch [80,45,1]. Allocations remain 2560x1440. All three preceding depth
producers use the same reduced rectangle and destination resource.

The scoped DrawInstanced hook sees all three writers. Their native consumer
and draw calls return normally, their nested packet/rectangle/argument/pipeline
snapshots remain stable, and their caller is the pinned image+0x259d72e.

| Producer receipt | Source state after barrier | Destination state | Draw arguments (vertices, instances, first vertex, first instance) | Same list pointer as TAA |
| --- | --- | --- | --- | --- |
| 0 | 192 (0xc0) | 4 (RENDER_TARGET) | 3, 1, 0, 0 | No |
| 1 | 192 (0xc0) | 4 (RENDER_TARGET) | 3, 1, 0, 0 | No |
| 2 | 128 (0x80) | 4 (RENDER_TARGET) | 3, 1, 3, 0 | Yes |

All recorded transitions cover the entire resource, with no split flags;
barrier calls return with stable snapshots. The last producer precedes the
selected temporal request and matches its depth identity. Command-pointer
equality does not yet prove allocator/reset generation or GPU completion.

`draw_states` currently requires source==0xc0 and destination==4. The final
writer therefore fails draw admission with depth fault 17. Downstream faults
are submission 18, native consumer 15 and top-level 13. These are recorder
rejections; the trace records normal returns, not a game crash. The frozen
review correctly rejects the capture with `observer closure`.

No diagnostic image copy or private signal was attempted. No images or shader
were saved. The temporary native temporal stage ran, with no injected jitter
and no NGX/DLAA/DLSS evaluation. Full scale restored after 437 overrides.

## Interpretation and next change

Microsoft defines 0x80 as PIXEL_SHADER_RESOURCE and permits combining it with
the non-pixel shader read bit (0x40). Pixel-only sampling does not require that
additional bit. [D3D12 resource states](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states).
The source's 0x80 transition is consistent with this native draw route. Requiring
0xc0 copied an observation from the first writer into a universal requirement.

The bounded correction is to accept exactly 0x80 or 0xc0 for this draw source,
while still requiring destination==RENDER_TARGET and preserving alias/split/
partial/conflicting-barrier rejection. Do not accept arbitrary states merely
because a read bit is present. The original CopyResource path stays unchanged.
Readback transitions only the destination, so it must still restore state 4.

Update the runtime predicate and versioned offline review together. Exercise
both states on real WARP draws, including the observed startVertex=3 packet
and multiple writers before TAA. Retain negative cases for non-pixel-only,
write, unknown, conflicting, split, partial and aliased states. Another live
capture must still prove the exact GPU generation, retirement and image content.
No new binary or source correction is included in this result.

## Closure and evidence

All hook groups report disabled, pending zero. Producer pins release 8/8,
depth 9/9, consumer 1/1 and submission 256/256, with no retained submission
references. Activation patch removal and callback drain both pass; the
v1.23.0 contradictory cleanup flag does not recur in this run. Nine native
scale values restore to 1. User confirmed a normal scene and normal exit;
process exit, unchanged settings/profile/mode and installed DLL were checked.
Keep this DLL installed as requested.

Root: `local/scaled-temporal-20260927/v1.23.1/`. Final log:
`inputs-13188-134350115518128752-final.jsonl`; rejected review:
`review-13188-134350115518128752.json`. `draw-state-analysis.json` records the
per-writer state/argument/list comparison; `run-result.json` records closure.
The final 362-file manifest includes the intact 354-file prelaunch evidence.

Final log SHA-256:
`8719b1f9d718a7e35b3022da7955e54efc99e02f0c14acb71830f1d37f6b5dc5`.
Final archive SHA-256:
`1281dd4458f618c18098a96564433c80717f45ab94a70044d7475e603f4177d5`.
