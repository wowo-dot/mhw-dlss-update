# Gallery intermediate pass: v1.25.33 result

2026-10-01. The diagnostic run captured two complete, matching CPU recipes for
the missing Gallery color pass. The user confirmed clean DLSS recovery after
leaving the Gallery and a normal scene and exit after the intentional stop.
This identifies the unsupported pass; it does not yet fix Gallery rendering.
The current v1.25.33 DLL stays installed.

## Confirmed observations

The native descriptor connects the motion-filter output to the grading input.
Its stage is 15, order 26005, key `0x78006595`; the descriptor type has RVA
`0x3526650` on executable SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The two captures, frames 18662 and 18722, each contain one four-vertex draw.
Both double snapshots and their frame stamps agreed, with two attempts and
zero capture errors. The draw targets one format-26 color attachment, has no
depth attachment, and uses rectangle `(0,0,1280,720)`.

Shader reflection and disassembly identify a depth-of-field composition pass
with optional vignette. These names come from the captured shader metadata,
not a guess based on the cutscene's appearance:

| Pixel binding | Native shader name | Captured engine extent | Role established by shader |
| --- | --- | --- | --- |
| t0 | CoC | 1280x720 | Contributes to the blur blend factor |
| t1 | Image | 1280x720 | Filtered color blended with t2 |
| t2 | HDRImage | 2560x1440 | Base color; owner equals the descriptor source |
| t3 | Neighbor | 128x72 | Contributes to the blur blend factor |
| t4 | DOFAImage | 1280x720 | Modulates the blur blend factor |

The shader uses point and linear clamp samplers, `CBScreen` at b1 and
`CBNewDOFFilter` at b3. It derives texture coordinates from pixel position and
screen inverse size, blends t1 with t2 using the other inputs, and conditionally
applies the vignette. The effect constants include `pcoc`,
`dof_single_pixel_radius` and vignette controls.

The vertex shader forwards clip-space XY and the first texture coordinate.
The pixel shader only consumes pixel position. The 28-byte input layout has
POSITION0, TEXCOORD0, TEXCOORD1 and COLOR0; captured positions cover the full
clip rectangle, TEXCOORD0 covers 0..0.5 and TEXCOORD1 covers 0..1. Changing these
vertex UVs alone cannot correct this pixel shader's sampling domain.

Shader identities, with all extracted bytes/disassembly retained only under
ignored local evidence:

- VS, 644 bytes: `730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd`
- PS, 4604 bytes: `9f04832cb0a0ff800169964919f7ebecba353e7e3c147c2b314e76af7372e3aa`

## Why the current route suspends

The route currently requires grading input to be the motion-filter output, or
the admitted haze output. This native DOF composition introduces another owner
between those endpoints. The exact connection is now captured, and the existing
reason-6/detail-4 rejection correctly records that unsupported chain.

There were three suspension/recovery periods and 2,183 skipped dispatches,
including warmup. Dispatch counts are not a count of displayed bad frames.
No runtime or SDK fault occurred. This result does not establish that unrelated
monster effects or the hunter silhouette use the same pass.

## Next integration boundary

Extend the shared color-dependency route to represent an intermediate color
consumer/producer, then qualify this recipe by its shaders, bindings, geometry,
resource identities and native ordering. Avoid cutscene-name exceptions and
do not bypass the DOF effect or simply widen the endpoint comparison.

The reconstructed color binding and native auxiliary textures need separate
coordinate treatment. Preserve the native blur inputs and artistic constants;
derive the display-to-render pixel conversion before adapting the reconstructed
t2 lookup. A global UV multiplier or a shared inverse-size change would also
change the other four inputs and vignette position.

These CPU captures do not contain live constant-buffer bytes or GPU resource
descriptions/states. Reflection defaults are not live values. Packed engine
extents are not proof of initialized texture regions, and the native GPU-format
field was UINT_MAX. Before live admission, validate actual resource descriptions,
SRV formats, state transitions, constant-buffer contents/ownership and producer
submission order. Reuse the existing binding, fence, retention and WARP fixture
machinery. Unknown or unstable recipes must continue to use native rendering.

The next concrete work is an offline DOF adapter/route contract and GPU fixture
using this saved recipe, with any missing live values captured at the relevant
draw boundary. Camp-entry investigation remains deferred.

## Verification and closure

Full frozen v1.25.33 review and all ten checkpoint component reviews passed.
There were 3,449 DLSS evaluations, zero faults, and three successful resumptions.
All 5,633 main GPU signals/recordings and 2,183 unfiltered recordings completed
and released; all 3,475 output recordings released. All 9,249 jitter changes
and the full render scale were restored. Read-only live checks verified 40
complete native code spans and 18 hook heads restored before normal exit.

The collector ended on the healthy native terminal with empty stderr. Game and
collector absence, installation/configuration hashes and the 637-file prelaunch
manifest are checked by the closure receipt. SDK shutdown remains deferred while
the diagnostic DLL is pinned, as recorded by the existing runtime policy; this
is not a claim that the entire SDK was unloaded by the stop operation.

Evidence root: `local/gallery-followup-20261001`. See `final-review-52460.json`,
`gallery-analysis-52460.json`, `gallery-extracted-52460/`,
`runtime-result-52460.json` and `archive-final.json`. Proprietary shader bytes,
native addresses and binaries stay in this ignored local archive.
