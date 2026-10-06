# FG scene observer v0.6.5: depth-bearing draws and presented buffer

Prepared 2026-10-06. Diagnostic design; runtime validation is pending. This
extends the accepted v0.6.4 CPU scene map
([result](frame-generation-scene-raster-result.md)). It does not load the
Streamline FG plugin, tag FG inputs, add GPU commands, read pixels or change
which draws the renderer replaces.

## Questions for the next run

1. Do the draws that v0.6.4 rejected as reason 9 (one RTV plus a readable
   depth descriptor) return and get submitted before the qualified Present?
   Where are they in submitted order relative to the exact final draw?
2. Which swap-chain buffer is presented by that Present, and do returned native
   copies or draws reach that buffer after the final draw?

Both answers are CPU submission metadata. They do not establish GPU completion,
pixel contents, HUD semantics, resource formats or FG input lifetime.

## Depth-bearing draw qualification (diagnostic only)

`RasterBindings::output_known`, `raster_current()`, reconstruction admission
and the default observer library are unchanged. A separate qualification in the
scene probe accepts a draw for **metadata** when the existing v0.6.4 observation
shows: an observer entry, healthy accepting hooks, known/valid bindings, raster
and root generations equal to the current binding generation, vertex/viewport/
scissor/topology known, output unknown, one RTV argument read successfully and
one depth argument read successfully.

It then requires agreement with the native executor cache under the existing
executor/packet/attachment scope (reasons 1-7 unchanged):

| Check | Native source (tested offsets already used by `snapshot_attachments`) | Rejection |
|---|---|---|
| Color | `state+0x98` cached RTV and backend `+0x10` handle equal the API RTV | 16 |
| Depth | attachment `+0x40` view, view `+0x60` backend, backend `+0x10` handle and `+0x18` resource, view `+0x74` format, cached handle `state+0xd8`, cached resource `state+0x140`; handles equal the API DSV, cached resource equals the view resource | 17 |
| Root | `state+0x110` equals API graphics root | 18 |
| PSO | `state+0x108` equals API PSO | 19 |
| Scope | a diagnostic draw carrying the exact final packet | 20 |

No rotating handle table is traversed. Reason 9 now means neither the preserved
single-RTV/no-depth output nor this depth-bearing output was available.
Reasons 15 and 21-23 are reserved and must stay zero.

Each qualified event records the API DSV handle, the eight native output values
with read bits, and pipeline facts: RT0 blend description (10 values), depth
enable/write mask/function, and DSV format. The pipeline snapshot passes the
depth view format exactly as the existing attachment snapshot does.

## Diagnostic events never gate closure

v0.6.4 closes a capture at the first qualified Present after every recorded
event has returned and been submitted. v0.6.5 keeps that gate for the preserved
(followed-path) events only. Depth-bearing and presentation-only events are
**diagnostic**: they are stored in up to 512 of the 1,024 event slots, never
consume the preserved capacity, and cannot block or authorize closure. At
closure the trace counts diagnostic events that had not returned or had not
been submitted. Diagnostic duplicate submissions and premature Resets have
separate counters. Unsubmitted diagnostic events are reported, not upgraded.

## Presented buffer table (scene-only presentation library)

`mhw_fg_reflex_presentation` compiles the same `presentation.cpp` with
`MHW_FG_SCENE_PROBE` and is linked only by the scene target. Default targets keep
`mhw_reflex_presentation`. Inside existing routed host calls it records:

- a chain generation, advanced on chain publication/retirement and every
  successful resize;
- labels the host received from routed `GetBuffer(ID3D12Resource)` per index
  (up to 16), with conflict, foreign-chain and out-of-range counters;
- the last host `GetCurrentBackBufferIndex` result and the Present count when
  it was observed;
- one bridge `GetCurrentBackBufferIndex` query at routed Present entry, before
  the SDK/native Present. This is the only added native call. It runs under the
  existing bypass guard, so it reaches the original function and is excluded
  from the ownership map. It is skipped for `DXGI_PRESENT_TEST` and for the
  SDK-paced FG contract.

A separate SRW lock protects the table; writers already hold the route mutex,
and readers (capture request worker, Present observer) take only that lock. No
buffer is retained, released, described or queried for resource state.

Each capture request stores the request-time table. Draws, barriers and copies
whose resource matches only those labels are recorded as diagnostic
presentation events; they never join the followed final-target path. A returned
native copy from a followed resource keeps its v0.6.4 behavior. At closure the
trace stores the Present-time table. The reviewer accepts a presented-buffer
mapping only when the request and Present tables have the same chain, generation
and labels, and the Present-entry index names a known label. It separately
reports whether the host's own last index query in that frame agrees.

## Review and logging

Trace schema 5, policy `submitted-scene-depth-presentation-map-v5`; probe log
schema 7, version 0.6.5. `tools/frame_generation_scene.py` is the strict v0.6.5
reviewer; the accepted v0.6.4 reviewer remains frozen with its source. The
reviewer reports per capture: completed depth-bearing draws and their order
after the final draw, their shader/blend/depth facts, the presented index and
label, events on the presented buffer after the final draw, and whether an
ordered copy path reaches it. All UI, pixel, completion and lifetime claims
stay false.

The status writer now copies the bounded trace under the route lock and
serializes it after release, so render callbacks do not wait on the larger
text. In the default target the status output is unchanged.

## Offline checks

Native trace tests cover the buffer table (generations, conflicts, foreign
chains, index/Present association), diagnostic capacity isolation, non-gating
closure, separate duplicate/premature-Reset counters, presentation-only copies
that never extend the followed path, and v5 serialization. Reviewer tests reject
inconsistent native depth agreement, misclassified events, wrong counters,
reserved reasons, reason-9 samples that should have qualified, and stale or
foreign presentation tables. Both the scene and default targets build with
C++20 `/W4 /WX`.
