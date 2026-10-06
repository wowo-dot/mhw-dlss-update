# Gallery DOF composition: v1.25.34 candidate

Live follow-up: [v1.25.34 result](dof-output-route-result.md). Gallery still
suspends because the planner rejects six compute commands preceding composition.
The adapter itself did not execute. Recovery and intentional cleanup passed.

2026-10-01. This connects the intermediate color pass identified by
[the v1.25.33 Gallery capture](gallery-intermediate-result.md). Offline shader,
resource-routing and regression tests pass. Gallery success remains unverified
until the new DLL is tested in the game.

## Connection and coordinate contract

The completed native post-filter descriptors now supply a bounded chain of
source/destination owners between motion output (or admitted haze output) and
grading input. The chain must be unique, acyclic and in increasing native key
order. Each intermediate draw still needs its own shader, binding, geometry,
attachment and resource qualification. No cutscene or area name enables a draw.
Unknown intermediates continue to use the native rendering path.

The first admitted intermediate is the captured DOF composition. Its VS/PS
hashes and nine binding words are exact matches to the v1.25.33 evidence.
PS t2 must be the descriptor source; the actual RTV must be its destination.
The native four-vertex, 28-byte layout is checked, including both UV sets and
color. All five texture owners and native SRV handles are checked at preparation,
native draw entry and API draw. Actual resources must belong to the same device,
have matching extents, single-sample 2D storage and supported sampled formats.
The four auxiliaries preserve their native formats, extents and SRVs. No GPU
format is inferred from the captured engine field UINT_MAX.

The adapter takes caller-supplied, hash-verified private DXBC and modifies four
accounted instruction sites. It does not embed a game shader. With display pixel
position P, render/output scale S and native inverse size I:

- Native DOF/vignette position is P * S.
- Auxiliary texture coordinates remain (P * S) * I.
- Reconstructed t2 coordinates alone expand by 1/S.

This retains live artistic constants, samplers, root arguments and descriptor
bindings. It does not replace the blur with sharp color or disable the vignette.
Changing vertex UVs would not fix this shader: its pixel stage uses SV_Position.
Reflection/signatures remain intact; stale shader statistics are omitted.
The supported geometry is still top-left, equal-aspect 1x or 2x. This does not
establish arbitrary quality ratios or dynamic-resolution support.

## Ownership, ordering and cleanup

This revision adds no native hook target, RVA, calling convention or code patch.
It uses the existing post-filter preparation and draw/API callbacks, native
unsigned stable key order, frame/epoch stamps, atomic native/reconstructed choice,
resource-state observations, command submission checks and fence-plus-Reset
retention. Tested host executable SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.

All five inputs and the output are pinned. Self-aliases and auxiliary writes
inside the selected color chain reject. Known contradictory SRV states reject;
unchanged native SRVs may retain their existing inherited read state. A private
RTV draw still requires its observed render-target state. Constant-buffer bytes
are neither rewritten nor independently owned by this adapter; native bindings
and command ownership remain authoritative. DOF binding changes reject; the
existing radial-only auxiliary refresh policy is unchanged.

The binding diagnostic schema is now 2, supporting a fifth texture and dependency
kind 3. The dependency route adds a bounded `dof_draws` counter. Old schema-1
archives remain reviewable; v1.25.34 requires the new channels.

## Offline evidence

Evidence is under ignored `local/dof-output-route-20261001`:

- `scaled_dof_fixture`: WARP and NVIDIA hardware each pass eight 1x/2x cases,
  including blur, vignette, nonzero screen offset and patterned inputs. An
  independent unmodified-native-shader reference uses expanded source storage
  and transformed constants. Maximum float difference is zero. Production
  R11G11B10 pixels match exactly. Three negative controls detect incorrect
  full-resolution auxiliary/vignette coordinates. D3D12 GPU validation is enabled.
- `dof_plan`: real COM resources with synthetic native descriptors verify the
  five-input route and ordering on WARP and hardware. Eight malformed plans
  reject; a changed fifth descriptor is diagnosed.
- Existing wingdrake copy/radial production recording and auxiliary replacement
  fixtures pass on WARP and hardware, including negative cases, state restoration,
  exact GPU pixels and fence-plus-Reset resource release.
- Continuous runtime regression passes 96 frames, 288 effects and 786,432 exact
  pixels. The focused 59-test native suite, geometry fixture and Python binding,
  overlay, refresh, session, scaled and whole-run suites pass. The sealed v33
  run passes the updated reviewer. Synthetic v34 logs reject missing channels,
  old schemas, over-counted draws and runtime faults.

Initial build/fixture failures are retained locally: duplicate CMake insertion,
an insensitive no-blur/no-vignette negative control, and incomplete synthetic
review accounting. These were test/setup corrections, not runtime results.

## Live test and remaining limits

Use the existing solo/offline Stracker-enabled MO2 profile, DX12 windowed
2560x1440, native AA/DLSS/MotionBlur Off, without resizing. Verify startup/module
identity and arm at the main menu. Check a loaded scene, then replay the affected
Gallery cutscene. Inspect DOF draw counts, suspension reasons, binding/state
checks and visual blur/vignette coverage; afterward verify clean normal gameplay.

Offline inputs are synthetic. They prove the adapter's coordinate calculation,
not the live auxiliary producer contents, all native constant values or every
DOF variant. The live run must check those integration assumptions; a matching
shader alone is not a universal quality claim. Monster-attack effects and the
hunter silhouette remain separate pending work. Camp-entry skips remain deferred.

Deployment preserves the exact prior DLL and output sidecar. Keep this candidate
installed after normal exit. If restoration is explicitly requested, close the
game and use `local/dof-output-route-20261001/deploy.ps1 -Action Restore`; the
restored sidecar points at a sealed archive and needs a fresh output directory
before another launch. Original game files and settings are not modified.
