# Gallery native texture descriptions captured; dispatch states partial

The v1.25.37 run on 2026-10-01 captured two complete resource-description
sets at renderer frames 21416 and 21476. Both have nine resources, identical
descriptions and the known six compute shaders plus composition shaders.
Evidence: `local/dof-resource-capture-20261001`; PID 49960, creation
134353241475473450. DLL SHA-256:
`3be3541dbc36c96c1393e39e404dd7cc071673080467bf2bc93abb166b9d9d0b`.
See the [capture contract](dof-resource-capture.md) for hook and ownership details.

## Confirmed resource descriptions

These descriptions come from the held ID3D12Resource objects and verified
device identity. Roles follow binding words and the previously reflected,
hash-matched shaders. Resource descriptions do not independently prove SRV/UAV
descriptor formats or valid pixel contents.

| Role | Allocation | Resource format | Flags |
|---|---|---|---|
| External depth | 2560x1440 | R32_FLOAT (41) | render target |
| TileMaxH | 128x1440 | R11G11B10_FLOAT (26) | UAV |
| TileMax | 128x72 | R11G11B10_FLOAT (26) | UAV |
| Neighbor | 128x72 | R11G11B10_FLOAT (26) | UAV |
| External HDR scene | 2560x1440 | R11G11B10_FLOAT (26) | render target + UAV |
| CoC | 1280x720 | R16G16B16A16_FLOAT (10) | UAV |
| Image, reused by unpack | 1280x720 | R11G11B10_FLOAT (26) | UAV |
| Packed DOF | 1280x720 | R32_UINT (42) | UAV |
| DOFAImage | 1280x720 | R8_UNORM (61) | UAV |

All are Texture2D, 64 KiB alignment, one mip, one array slice, one sample,
sample quality zero, unknown/default resource layout and one format plane.
The offline fixture's RGBA32F float intermediates are therefore intentionally
different from native resources. Native-format replay must account for that
precision difference before equivalence is claimed.

## Partial dispatch-state evidence

The diagnostic retained 44 transition observations with no lost events. Only
two of twelve captured native dispatch observations passed the entry checks:

- Frame 21416, pass 1: depth state 0xC0 (pixel + non-pixel shader resource),
  TileMaxH state 0x8 (UAV).
- Frame 21476, pass 5: CoC/Neighbor state 0xC0, Image state 0x40 (non-pixel
  shader resource), packed DOF/DOFAImage state 0x8.

Both had exactly one Dispatch, matching native packet, dimensions, command-list
generation, compute root/PSO and complete CPU recipe, and returned normally.
All ten other observations have entry error 1 and no API snapshot. Their states
remain unknown. The current combined check does not distinguish backend,
recording/list, observer validity or recipe mismatch, so this run cannot name
the failing predicate. It is not evidence that native GPU execution failed.

Two complete paired CPU bundles were extracted privately: eight unique shaders,
22 producer/consumer edges and eight external reads across both samples.
No pixel readback, independent descriptor-content validation, complete live
compute-state contract or Gallery rendering fix is claimed.

## Recovery and cleanup

The user confirmed clean recovery after Gallery and a normal scene/exit.
Intentional stop completed after 1,990 DLSS frames and 1,789 skipped dispatches
(not a displayed-frame count), with zero runtime faults. All 3,779 main and
1,789 unfiltered GPU signals/recordings completed and released; all 2,010 output
recordings and eighteen diagnostic references released. All 9,451 jitter edits
and full render scale restored. Frozen full review and all twelve component
reviews pass. Independent live reads verified forty native code spans and
eighteen hook heads restored.

Collector: 26 timed snapshots/events, 4,905,603 bytes, no read retries or faults.
Timeline SHA-256:
`cc0b257b55b9c453e9db35030daddb027bcd93741383bbce49f1ab290f8e065f`.
The installed DLL and four configuration identities are preserved. Consult
`closed-result-49960.json` and `archive-final.json` for final checks and sealing.
Choose a fresh output root before another launch.

Next distinguish the entry-check predicates without weakening validation, and
use these actual formats in the offline replay. Establish the remaining states,
view descriptors and external valid depth footprint before live compute
admission. Camp remains deferred; Gallery precedes monster effects/silhouette.
