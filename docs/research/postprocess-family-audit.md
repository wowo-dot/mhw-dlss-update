# Shared post-processing families — 2026-10-02

The remaining work should be organized around native pass families and their
resource contracts, with representative live tests. It does not require a patch
for each monster. The current implementation already does this for the known
copy/radial, haze, overlay, and six-compute DOF paths. This audit changes no
rendering code and does not claim universal effect coverage.

## What the offline audit established

Both executable hashes were reverified before decoding. The audit follows direct
calls to the two native filter-registration helpers, resolves chained x64 unwind
entries, and links candidate Execute slots to the engine's type-record getters
and initialization code. Names below therefore have code-to-type provenance;
they are not inferred merely from nearby strings or standard Microsoft RTTI.

There are 15 direct-caller function groups for these two helpers in the pinned
image. Fourteen groups link to 15 native type names; two radial types share one
callback. The other group references GodRays constants, but its type was not
established by this search. This is a scoped registration catalogue, not an
enumeration of every render pass: indirect registration, other schedulers,
shader branches, and effects outside this queue remain outside that claim.

| Native type | Vtable RVA | Execute RVA |
| --- | --- | --- |
| `uDepthColorFilter` | `0x3423850` | `0x1c27a40` |
| `uTemporalAAFilter2` | `0x351bda8` | `0x2392150` |
| `uFXAAFilter` | `0x351c1a8` | `0x23939d0` |
| `uNewDOFFilter` | `0x3526650` | `0x24233b0` |
| `uImagePlaneFilter` | `0x352a520` | `0x244c3e0` |
| `uRadialBlurFilter` | `0x352f160` | `0x2481900` |
| `nEffect::uEffectRadialBlurFilter` | `0x3544910` | `0x2481900` |
| `uHazeFilter` | `0x35372b0` | `0x24c7760` |
| `uMotionBlurReconstructionFilter` | `0x3537a70` | `0x24ca1f0` |
| `uToneMapFilter` | `0x3538400` | `0x24d0780` |
| `uMotionBlurFilter` | `0x353b858` | `0x24fbfb0` |
| `uCircularDOFFilter` | `0x353e048` | `0x25244a0` |
| `uColorCorrectCube` | `0x35403a8` | `0x2528a30` |
| `uBlinkFilter` | `0x35441a0` | `0x2547f20` |
| Type not paired; references `CBGodRaysFilter` | — | `0x24c37e0` |
| `uTemporalAAFilter3` | `0x354df48` | `0x2597870` |

The effect-system radial type's initializer passes the base radial type record
as its parent to the same native registration routine. Both Execute slots lead
to `0x2481900`, which invokes the shared rendering routine at `0x24819d0`.
The derived type can still select different parameters, bindings, or shader
techniques; callback equality alone is not proof of identical GPU recipes.

The two registration helpers also expose the common contract:

- `0x24c9270` copies the unit's phase/order at `+0x138/+0x13c` into its descriptor
  at `+0x140/+0x144`; `0x24c9240` takes explicit phase/order arguments.
- Both enqueue through `0x24f2350`. The existing scheduler at `0x24f1fc0` sorts
  descriptors and assigns source/destination owners before callback execution.
- The full-screen geometry helper at `0x2297e20` derives sampling coordinates
  from a rectangle and texture dimensions. A larger viewport alone does not
  correct all scene texture reads, auxiliary reads, or compute dispatches.

The Gallery callback additionally selects between its blur and another draw
branch. A native class name does not uniquely describe its current packet chain.
The six-pass integration remains qualified by its actual shaders, views,
dimensions, constants, ordering, and resource lifetimes.

## Coverage and outstanding proof

| Contract family | Current implementation | Remaining evidence needed |
| --- | --- | --- |
| Scene color plus depth/motion/noise | Motion and haze adapters preserve each input's coordinate domain. | Representative untested techniques; a class match is insufficient. |
| Scene copy, temporary texture, radial composition | Dependency graph plus shader adapters and refreshed auxiliary bindings. Wingdrake passed in v1.25.32. | Recheck old combat reports on the current v1.25.41 build. |
| Overlay that does not read reconstructed scene color | Native shader/blend/constants, qualified geometry and corrected viewport/scissor; 16- and 28-byte layouts supported. | Hunter silhouette draw identity and actual coordinates remain unmapped. |
| Intermediate compute chain and composition | Native six-pass Gallery resources/views/barriers, with two qualified dispatch adaptations. Gallery passed in v1.25.41. | Other compute recipes and techniques are not automatically admitted. |
| Unknown resource dependency or shader semantics | Bounded rejection receipts and native rendering. | Identify the missing contract, reproduce offline, then admit that family. |

The old Safi repeat report and Alatreon nova report in
`local/dlss-long-run-20261001` retain an order-4100 descriptor with type
`0x3544910` and overlay rejection reason 17. The nova graph also contains the
known haze unit. Those reports came from v1.25.25, **before** the successful
wingdrake dependency and auxiliary-refresh fixes. They establish the queued
native family, but did not save the exact attack-frame shader/binding recipe.
Thus some old attack interruptions may already be covered; that is a testable
hypothesis, not a completed fix. Do not invent separate Safi/Alatreon patches.

The small hunter silhouette is a separate unresolved coordinate issue. Its
screenshots alone do not identify its draw, stage, or texture inputs. Nor does
successful DLSS evaluation prove every later overlay is positioned correctly.

## Runtime follow-up

The 2026-10-02 run reproduced combat interruptions on v1.25.41. A paired
Safi snapshot identifies the native default radial mask rejected by the
custom-mask-only gate; overlapping radial descriptors also share a command
key that the current planner treats as ambiguous. Alatreon telemetry shows
the same rejection patterns. See the
[runtime result and evidence limits](postprocess-family-validation-result.md).
The proposed validation below is retained as the original run plan.

## Original validation plan

Keep the verified v1.25.41 DLL and choose a fresh output directory. First verify
normal gameplay, then reproduce representative Safi and Alatreon attacks while
recording counter deltas and any rejected graphs/bindings. Check that copy/radial
draws execute and reconstruction remains active during the effect. If a failure
returns, classify its actual recipe rather than its monster name. Retain a
silhouette checkpoint if it can be reproduced in the same run. Camp entry stays
deferred; alternate render ratios and settings UI are not part of this test.

No new hooks, admission changes, or logging inside the game were added for this
audit. Rebuilding the same renderer is unnecessary for this validation. Further
implementation should follow a newly demonstrated contract gap; it should not
start by loosening existing resource/state checks or globally scaling every UV.

## Evidence and identities

Private reproducible audit: `local/postprocess-contract-audit-20261002`:
`audit_native.py`, `registration-callers.json`, `native-catalog.json`,
`type-record-xrefs.json`, bounded function disassemblies, and `identities.json`.
Chained unwind groups are labelled as such; they are not blindly substituted
for existing runtime code-gate spans (some include different alignment padding).
No proprietary code or shader bytecode is included in this document.

Retail executable version `0.0.0.0`, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Installed v1.25.41 DLL SHA-256
`797d937e8203b1c2d5ab8ab0079559fed15cf02ef7c88a536316273f4c548dbc`.

Related: [shared boundary](shared-renderer-boundary.md),
[dependency/auxiliary refresh result](auxiliary-binding-refresh-result.md),
[Gallery result](dof-native-compute-route-result.md),
[older broad run](dlss-long-run-20261001.md),
[future effect controls](future-native-effects.md).
