# Ordinary TAA input requests: accepted run 44924

2026-09-25, standalone observer v1.0.0, bare profile with DLSS Off/TAA On.
The user loaded a save and remained in the scene. Three consecutive requests
at frames **14224, 14225, 14226** pass the unchanged archived reviewer.
This establishes the ordinary TAA CPU input bindings and history rotation;
it does not establish shader consumption, texture contents or working DLAA.

## Identity and closure

Executable version 0.0.0.0, SHA-256
`C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA`.
Observer DLL SHA-256
`827D85B65492F76905CF4FECDC0479D5392B31D0F494DF3BDDBEFCC6DCD5AEF8`.
PID 44924, creation time 134347647777252692. The immutable log is
`inputs-44924-134347647777252692.jsonl`, SHA-256
`73C9CF8F8BDA56F3A471B2FC8EDB8374CB059BF0729E060A7313712751F08652`.
Evidence and frozen source/reviewer reside in
`local/temporal-bindings-20260925/v1.0.0/`.

The one hook captured three requests, then disabled cleanly. Faults and pending
callbacks are zero. All **21 acquired resource references released**. No jitter
was injected and no GPU work was submitted by the observer. The user exited
normally; process absence was checked before restoring the exact previous
DLL at 02:10:15 +03:00. Its SHA-256 is
`689BEF7CF00FF71E4518F452F0467EDF6C3D877F2A6DD090553388D8B5F288CC`.
The temporary output sidecar is removed, all four configuration hashes remain
unchanged, and all 90 archived source files verify against preparation.
No further diagnostic is installed or armed.

## Observed request bindings

The active swapchain is 2560×1440, format 87, adapter LUID low 98689/high 0.
All requests use technique 23086, groups 160×90×1, the same primary view/layer,
and the reviewed ordinary-TAA return address `+0x2392F31`. Epoch equals frame;
bank alternates 0/1/0 and async mode is 1. Every source snapshot is unchanged
across the original producer, and every newly appended opcode-6 packet matches
its state pointer and dimensions. Queue counts are 3799→3800, 3802→3803 and
3799→3800 in the corresponding banks.

| Named binding | Native resource format | Native size | Observation |
| --- | --- | --- | --- |
| `tBaseMap` | R11G11B10_FLOAT | 2560×1440 | Same current-color resource in all three requests |
| `tBlendMap` | R11G11B10_FLOAT | 2560×1440 | Previous output follows the three-slot ring |
| `rwOutput` | R11G11B10_FLOAT | 2560×1440 | Three distinct output owners/resources |
| `tDepthMap` | R8G8B8A8_UNORM | **1×1** | Fallback resource, not the validated scene-depth image |
| `tGBuffer1` | R8G8B8A8_TYPELESS | 2560×1440 | Actual typed SRV and packing remain unverified |
| `tGBuffer4` | R32_UINT | 2560×1440 | Integer resource; contents remain unverified |
| `tPrevGBufferID` | R8_UNORM | 2560×1440 | Stable resource identity across these requests |

All are single-sample, one-mip, single-slice textures. Native descriptions come
from GetDesc on pinned resources, not interpretation of game metadata.
The output from frame 14224 is the history input at 14225; the output from
14225 is history at 14226. The third output returns to the owner initially
observed as history, matching the statically mapped modulo-three allocation.
This proves request-side association, not completion of those GPU writes.

One metadata caveat is retained explicitly: the field serialized as
`engine_height` is the raw high half of owner `+0x88`. For `tGBuffer1` it is
34208 (`0x85A0`), while native GetDesc reports 1440 (`0x05A0`). A packed flag
is a plausible explanation, but its semantics are not established here.
Do not treat that raw field as a physical height or silently rewrite the log.

## Separate live configuration observation

After capture closure, 31 bounded scalar reads observed the same layer type
and primary view, with stable configuration on immediate recheck. This later
observation is **not evidence from the three captured frames**. It found
depth selection 0, reprojection 1, color/clip modes 2/2, debug 0, check-output 1,
and technique cache 23086. The renderer's fallback owner equals the captured
1×1 `tDepthMap` owner, corroborating the reviewed depth-selection branch.

The initial live check stopped because the general context's current view
had changed outside the TAA call. The successful follow-up reports that
transient field without using it as a TAA request anchor, and validates the
primary-view pointer instead. Neither attempt wrote process memory or changed
the recorder/reviewer. The failed script is preserved alongside the successful
script and its named-field receipt.

## Next decision

Keep the separately validated scene-depth path. Do not feed this named 1×1
depth binding to modern reconstruction. Next map the compute shader and typed
views for the observed TAA/G-buffer inputs, then validate motion packing,
direction, scale, jitter treatment and frame alignment against controlled
camera movement. Neither G-buffer is declared a modern motion-vector input
from its name or format alone. The old dense DLSS motion pass remains a
separate, gated producer and is not required for this successful observation.

GPU consumption, resource-state ordering, motion pixels and DLAA readiness
remain false in both the archived review and acceptance receipt.
