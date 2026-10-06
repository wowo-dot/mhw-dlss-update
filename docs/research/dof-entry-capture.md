# Gallery dispatch entry diagnostics (v1.25.38)

The [v1.25.37 result](dof-resource-capture-result.md) verified two sets of nine
native texture descriptions, but ten of twelve dispatch observations failed
one combined entry check. The exact predicate was not recorded. v1.25.38 splits
that observation into explicit reasons without relaxing rendering admission
or the underlying validation requirements. Gallery may still suspend DLSS.

## Entry evidence

The same native compute executor, Windows x64 calling convention, ownership
rules, executable hash and code gates from [v1.25.37](dof-resource-capture.md)
apply. No additional native/API hook or GPU work is introduced.

Each of the existing twelve bounded pass records adds an `entry` array:
reason, observed backend, recording context, command list, observer lookup
success, known flag, valid flag, Reset generation and renderer frame.
The reason is zero for admitted entries or untouched records, otherwise:

| Reason | Failed check |
|---|---|
| 1 / 2 | Backend read / identity |
| 3 / 4 / 5 | Recording read / command-list read / null list |
| 6 / 7 / 8 / 9 | Observer lookup / unknown / invalid / missing generation |
| 10 / 11 | Complete recipe reread failed / recipe changed |
| 12 / 13 | Renderer frame read / outside the captured frame interval |

`recipe` contains complete-reread success and a difference bitmask: identity
1, packet prefix 2, owners/IDs/modes 4, textures 8, binding range/words 16,
views 32, shader 64, constants 128. A completed reread also retains its twelve
prefix words. Unreadable recipes do not claim any comparison. Every comparison
still covers the full original recipe. No changed field is ignored or accepted.

The API-time recipe recheck records the same comparison if reached. References
and barriers keep the previous bounds and lifetime. Reason metadata cannot
authorize dispatch, barriers, descriptors or copies. Schema 2 and policy
`native-compute-entry-observation-v2` distinguish this channel; v1.25.38 review
requires them, while historical schema-1 evidence remains supported.

## Offline native-format replay

The opt-in `native-formats` mode of `dof_pipeline_fixture` uses the nine captured
resource formats and extents with synthetic pixels and captured v1.25.36
constants. The six original private compute shaders execute in their captured
order. The original preparation/composition shaders form an independent proxy
reference; the coordinate adapter is compared against it. The final comparison
target remains RGBA32F to expose differences. Fixture SRV/UAV descriptors are
explicitly created and are not a capture of the game's descriptor contents.

Packed R11G11B10 and RGBA16 readbacks are decoded before comparison. Alpha bytes
remain byte comparisons. CPU upload conversion uses Windows SDK DirectXMath;
no new downloaded dependency is introduced. All intermediate and final
adapter/reference comparisons remain exact, without a relaxed delta threshold.

The first native-format attempt found that a GPU R11G11B10 store chose the
lower adjacent blue-channel value while the CPU packer chose the upper one.
The independent CPU oracle now accepts only the two representable encodings
bracketing its unquantized result. Exactly representable values have a single
allowed encoding; wrong/far, negative and nonfinite controls are rejected.
This is an explicit experimental oracle criterion, not a claim about every
driver's format-conversion guarantees. Failed attempts are preserved.

The pipeline checks wrong input coordinates, stale Image writer reuse, and
depth outside the active view. These experiments still use synthetic pixels;
they cannot establish the game's valid depth footprint or remaining live
resource states. See `local/dof-entry-capture-20261001/format-replay-review.json`
for completed cases and exact versus non-exact results.

## Runtime sequence

Same solo/offline profile and window size, native AA/DLSS Off. Main menu ->
loaded save -> affected Gallery cutscene -> recovery -> announced stop and
independent cleanup. Inspect specific reasons and any recipe differences in
both samples. Success means a rejection is localized or all twelve entry/API
observations complete; it does not mean Gallery reconstruction is implemented.

Evidence root: `local/dof-entry-capture-20261001`. Preserve the exact previous
DLL and sidecar before installation, use fresh output, and keep the candidate
installed after exit. Explicit rollback with the game closed is
`deploy.ps1 -Action Restore`; the restored sidecar points at a sealed archive
and needs a fresh path before launching. Camp remains deferred.
