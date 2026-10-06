# Gallery dispatch states captured — v1.25.39

The recorder correction passed its 2026-10-01 live test. All twelve native
compute dispatch observations completed across two Gallery samples, with
known states for all 36 captured texture uses and no capture errors. This
completes the missing dispatch-state observations; Gallery DLSS rendering is
still not implemented.

## Run identity and scope

Process 51896, creation 134353293718652206, loaded DLL SHA-256
`13202cd81a73df4f406b650648776e1ad9b6ce822916becee73cbf3db474d1f7`.
Host SHA-256 remained
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Prearm checks verified 671 prelaunch files, seven module identities and four
configurations. The runtime stayed DX12, windowed 2560x1440, native AA/DLSS Off.
Main-menu and loaded-save baselines passed with only two startup skips.

Two complete prepared bundles at renderer frames 14666/14726 retain six compute
commands and their final composition draw. Their actual native dispatches were
observed at frames 14667/14727, respectively, within the existing allowed
interval. Each sample used one command list and Reset generation (56595/57495).
Every entry and Dispatch check passed, followed by one clean native return.

## Result of the comparison correction

All twelve complete recipe rereads succeeded and had `stable_difference=0`.
Sample 0/pass 1 had raw difference zero; the other eleven retained raw mask 2.
Their only changed prefix word was word 2, the low half of packet `+0x8`.
No raw evidence was discarded and no stable recipe field was ignored. This
matches the [v1.25.38 diagnosis](dof-entry-capture-result.md) and verifies the
[v1.25.39 correction](dof-stable-recipe.md) in the game.

Nine native resource descriptions per sample match the preceding capture.
All eight shader files also match; the extracted CPU dependency graph again
contains 22 edges and eight external reads. The recorder retained 48 barrier
observations without losing any. No new GPU work was added by the diagnostic.

## Dispatch-state observations

Both samples give the same resource states below. SRV states are actual
observed D3D12 state masks, not inferred from native scheduling pointers.
`0x40` is non-pixel shader resource; `0xC0` includes pixel and non-pixel shader
resource; every listed UAV was in `0x8` (unordered access).

| Pass | Dispatch groups | SRVs and observed states | UAVs (all 0x8) |
| --- | --- | --- | --- |
| 1 | 8, 1440, 1 | depth: 0xC0 | TileMaxH |
| 2 | 128, 5, 1 | TileMaxH: 0x40 | TileMax |
| 3 | 16, 9, 1 | TileMax: 0x40 | Neighbor |
| 4 | 80, 45, 1 | depth, scene, Neighbor: 0xC0 | CoC, Image |
| 5 | 80, 45, 1 | CoC, Neighbor: 0xC0; Image: 0x40 | packedDOF, DOFAImage |
| 6 | 80, 45, 1 | packedDOF: 0x40 | Image |

The observations cover this captured execution, not arbitrary future frames.
The final composition's full descriptor contract and the live valid depth
pixel footprint remain unverified. Resource dimensions alone do not establish
which depth region contains current-frame data. Prior native-format replay
used synthetic pixels and explicitly constructed descriptors; it does not
close those remaining live-input questions.

## Recovery, stop and archive

The user confirmed clean recovery after leaving the Gallery, and the counters
confirmed resumed DLSS evaluation. The announced intentional stop recorded:

- 3,613 DLSS frames, zero runtime faults and 1,582 skipped dispatches: two at
  startup and 1,580 during Gallery suspension. Skips are not displayed-frame counts.
- All 5,195 main and 1,582 unfiltered GPU recordings completed and released;
  all 3,642 output recordings and 18 diagnostic references released.
- All 7,758 jitter changes restored, full scale restored and no pending work.
- Frozen full review and twelve component reviews passed. Independent live
  reads verified 40 full native code spans and 18 hook heads restored.
- Collector ended on the healthy terminal record: 28 events/snapshots,
  5,740,440 bytes, zero read retries and no observed runtime fault.

The user confirmed a normal scene and normal exit. Closure verifies the game
and collector are absent, all four configurations are unchanged, the installed
DLL/backup identities match, and the prelaunch files remain intact before
sealing the archive. The current DLL is kept installed as requested.

Evidence root: `local/dof-stable-recipe-20261001`, especially
`gallery-dispatch-observations-51896.json`, `gallery-state-review-51896.json`,
`closed-result-51896.json`, and `archive-final.json`. Use a fresh output path
before another launch. No next candidate is installed by this closure.

Next: close the live view/descriptor and depth-footprint questions, then connect
the six-pass pipeline and composition with validated coordinates and resource
lifetime. Gallery remains ahead of monster effects and hunter silhouette;
camp-entry skips are deferred.
