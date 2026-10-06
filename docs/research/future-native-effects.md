# Future side project: native post-processing controls

Recorded at the user's request, 2026-09-30. **Parked for future exploration.**
This is a map of useful discoveries from the temporal-upscaling work, not a new
implementation milestone. Continue the shared DLSS output work first; revisit
effect customization separately. A future CMFM integration could expose proven
controls, but no settings framework or menu work is requested here.

2026-10-02 update: the [shared-family audit](postprocess-family-audit.md) now
links 15 native type names to their registration/Execute paths, including the
base and effect-system radial types sharing one rendering callback. Radial
upscaling and the six-compute Gallery DOF sequence have since passed their
representative live tests. This does not establish editable user controls or
coverage of every shader technique.

## What “effect names” meant

The prototype recognizes particular rendering passes through native unit
identity, shader hashes, resource connections and pipeline layouts. It does not
look for text such as “sauna” or “rain” to decide what to patch.

Some names really do come from native types or shader reflection: `uHazeFilter`,
`CBHazeFilter`, `CBMotionBlurReconstruction`, and `CBToneMapping`, for example.
Others, such as “final-screen pass,” describe the role we observed. A name is
a clue, not proof of the active shader branch, bound resources, or a writable
game setting. One pass can serve several effects or scene types.

The ordinary captured color chain is:

`temporal resolve/copy -> motion filter -> color grading -> final-screen draw`

The sauna capture adds a haze pass between motion and grading. This is a
confirmed path for the tested scenes, not a complete catalog of the renderer.
The later rain-reported failure did not establish a separate rain shader as its
cause; its retained evidence included a depth-state admission problem.

## Starting points we have actually mapped

| Pass | Confirmed behavior / inputs | Existing implementation and evidence | Possible future experiment — not implemented |
| --- | --- | --- | --- |
| Motion filter | Reads color, depth, packed motion, fur/additional motion and neighboring motion; contains runtime-dependent reconstruction/blur branches. The pass still appeared with MotionBlur Off, so it is not proven redundant in that setting. | [Adapter](../../src/scaled_temporal/filter_shader.cpp), [captured construction](scaled-postfilter-result.md), [independent GPU tests](scaled-filter-adapter.md). | Map live blur constants and separate their visual contribution from the pass's other responsibilities; then consider a strength control or a narrowly qualified bypass. |
| Haze / distortion | `uHazeFilter`; scene color, depth, 3D noise and a filter texture. The captured shader has five technique paths and distance/height/noise/intensity-related calculations. | [Adapter](../../src/scaled_temporal/haze_shader.cpp), [shader findings](scaled-haze-shader-result.md), [adapter tests](scaled-haze-adapter.md), [successful live sauna route](scaled-haze-queue-timing-result.md). | Observe live technique/constant selection and explore a scoped haze or distortion strength control. Its use outside the tested scene still needs mapping. |
| Radial blur | During wingdrake travel, one draw copies scene color to a temporary texture and a second samples it back into scene color. Reflection identifies `CBRadialBlurFilter` and `CBRadialBlurFunction`; scene and auxiliary UV sets are separate. | [Two-draw capture](prepared-draw-dependencies-result.md), followed by [successful live dependency and auxiliary-refresh handling](auxiliary-binding-refresh-result.md). | Map actual bound strength, center, sample-count and curve constants before considering a control. Reflected defaults are not live values, and exact monster-attack GPU recipes still require confirmation. |
| Color grading / tone mapping | Scene color plus a 3D LUT, with `CBRenderFrame` and `CBToneMapping`; the captured shader has tone-mapping and HDR-related logic. It does not read depth, motion or SV_POSITION. | [Three-frame capture](scaled-colorgrade-result.md), [effect pipeline](../../src/scaled_temporal/effects.cpp), [offline effect tests](scaled-full-effects.md). | Test a private LUT substitution or controlled grading adjustments after confirming the active color space and constants. Do not assume reflected defaults are live values. |
| Final-screen transfer | Fullscreen triangle sampling the preceding color output, with a source-mip constant and a measured half-texel convention. It participates in scaling to the display. | [Final source lifetime](continuous-scaled-final-source.md), [effect geometry/pipeline](../../src/scaled_temporal/effects.cpp), [common boundary audit](shared-renderer-boundary.md). | Investigate a final presentation adjustment, or a separate post-process insertion point, after establishing placement relative to UI/HUD and HDR conversion for the intended path. |

The current adapters preserve the effects' native look while correcting their
color sampling for upscaled output. They are not existing user-facing controls
for disabling haze, changing LUTs, or adjusting blur strength.

## Native landmarks for resuming research

These are RVAs from the pinned executable, not portable signatures or permission
to hook another build. Revalidate code identity and surrounding structure first.

| Landmark | Recorded identity |
| --- | --- |
| Shared post-filter scheduler | Function RVA `0x24f1fc0`; assigns queue resources, then calls unit callbacks. |
| Motion filter | Unit vtable RVA `0x3537a70`; Execute RVA `0x24ca1f0`; observed phase 15 / order 100. |
| Haze | Unit vtable RVA `0x35372b0`; Execute RVA `0x24c7760`; observed phase 15 / order 20000. |
| Grading-related AutoExposure unit | Vtable RVA `0x3538400`; Execute RVA `0x24d0780`; setup descriptor phase 15 / order 0; deferred grading endpoint at unit `+0x238`, order `0x7fffff1`. |
| Fullscreen draw preparation helper | RVA `0x2297e20`; final-screen caller return `0x23c7b08`, haze caller return `0x24c7ff9`. The helper is shared, so its address alone does not identify an effect. |

Pixel shader identities currently required by `effects.cpp`:

| Pass | Bytes | SHA-256 |
| --- | ---: | --- |
| Motion | 10,008 | `3eed9b57bbbdf38d14d9a94596fc93a107aa99eab275af7118b95febbbc1ba18` |
| Haze | 14,596 | `4f890ffa89ea59a218db7e191d8aab48e9d5fe4be200c50a606b22594bb57f69` |
| Grading | 5,824 | `174a09e5b23c7e01fb6c47ce86b61250d695113109abb4ffae7219117c31a8cb` |
| Final screen | 1,080 | `be6127ba48008790fd0e93558b8248cb8acd7c0577899bf50d28e5618ae37b79` |

Host version: `0.0.0.0`. Host SHA-256:
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded analysis image SHA-256:
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
Function hashes and the existing hook contracts remain in
[stream_route.cpp](../../src/scaled_temporal/stream_route.cpp) and the linked
research notes. Proprietary shader blobs and disassembly stay in ignored local
evidence, not this document or distributable source.

## Reusable research tools

- [Prepared color-chain recorder](prepared-color-chain.md): captures generic
  native descriptor links, allocation sizes, active rectangles and command
  counts without requiring an effect whitelist. It is implemented in v1.25.16;
  [its game run is archived](prepared-color-chain-result.md). It does not discover complete
  shader semantics or prove GPU bindings by itself.
- [Effect pipeline component](../../src/scaled_temporal/effects.cpp): constructs
  private pipelines and vertices from qualified native recipes, restores the
  modified API bindings, and retains GPU references until safe retirement.
- [Motion fixture](../../tools/scaled_filter_fixture.cpp),
  [haze fixture](../../tools/scaled_haze_fixture.cpp), and
  [effect-chain fixture](../../tools/scaled_effect_fixture.cpp): compare private
  variants with captured native shaders using controlled textures/constants,
  reducing how often experiments require launching the game.

Existing local evidence entry points:
`local/scaled-temporal-20260928/v1.23.9/`,
`local/scaled-filter-20260928/`, `local/scaled-effects-20260928/`,
`local/scaled-haze-20260930/`, `local/scaled-haze-adapter-20260930/`, and
`local/prepared-color-chain-20260930/`. Read each manifest/result for its exact
scope; early shader extraction is weaker evidence than the later live binding
and execution captures.

## Sensible first experiment later

Choose one visual parameter, capture its actual bound constants in a known
scene, and reproduce its effect in an isolated fixture. Preserve an identity
mode before trying a small override. Only then connect that override to a
qualified live pass and compare the result with a recording.

Keep color and auxiliary coordinate systems separate: enlarging color UVs must
not silently change depth, motion or noise lookups. Likewise, removing a draw
requires preserving its output/resource contract; skipping a visually unwanted
effect is not automatically equivalent to deleting its GPU work. Save settings
or a future CMFM control can follow once that behavior is established.

Still unmapped here: a complete catalog of all screen effects, their live
parameter offsets/ranges, universal enable switches, and the behavior of all
techniques during combat, cutscenes, HDR or alternate resolutions. The
[Gallery DOF integration](dof-native-compute-route-result.md) and overlay extent
work establish specific rendering contracts, not general customization controls
for DOF, vignette, bloom, or all related techniques.
