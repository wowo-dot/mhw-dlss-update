# Phase 0: Modern DLSS requirements versus Monster Hunter: World

Status: static evidence collected; runtime observation not yet performed.

## Conclusion

Monster Hunter: World already has the core temporal inputs required by modern
DLSS Super Resolution and DLAA somewhere in its renderer. The shipped DLSS
1.1.13 path accepts scene color, dense motion vectors, an output texture, and
optionally depth, history, reset, and motion-vector scale/offset values. The
renderer also contains explicit temporal jitter, exposure passes, and
current/previous camera-matrix plumbing. Runtime capture must still prove which
optional inputs MHW actually supplies and where they sit in the frame.

The likely shortest path is therefore to observe and replace the existing NGX
evaluation boundary, not reconstruct the renderer from `Present()` and not swap
the feature DLL blindly.

Direct NGX is the preferred first backend. Unlike Streamline, native DLSS SR
does not require camera matrices. Frame Generation is a separate later project.

## Reproducible target identity

| Artifact | Version / identity | SHA-256 |
| --- | --- | --- |
| `MonsterHunterWorld.exe` | Steam build 15539686; PE timestamp 2024-08-30; version fields are unusable (`0.0.0.0`) | `C2EBBBD2C49F216D484E31A5219BED419EB1E5E7D206D02CBA040A3AB79D90EA` |
| Shipped `nvngx_dlss.dll` | 1.1.13.0; NGX feature API `0x12`; snippet `0x1010D` | `D4CC11BCEC693DFEFD6EDDBA6DD29BDFC16B4043E7B83150F0097D14B76512D8` |
| Shipped `nvngx_dlisp.dll` | 1.2.17.0; NGX feature API `0x12` | `429771886E2FC88EBA9F0A81B74435086D6196B80A44CB32B7EB96134E26F294` |
| Public production `nvngx_dlss.dll` | SDK 310.7.0; NGX feature API `0x13` | `BE6E434A94CA32499515EB62CA0E6C274526055D568D0426E4C652DCDFB6EE6E` |

The Capcom executable and both shipped NVIDIA feature DLLs have valid
Authenticode signatures. The local install contains a third-party Steam API
replacement, so rendering conclusions should eventually be reproduced against
a Steam-verified baseline. This project does not inspect or modify licensing
components.

## Required-input gap matrix

| Modern native NGX requirement | MHW static evidence | Current assessment | Runtime question |
| --- | --- | --- | --- |
| Jittered pre-tonemap scene color | Old feature requires `Color`; renderer has a dedicated DLSS command list | Strong candidate already passed | Format, dimensions, HDR range, and exact pipeline position |
| Separate output UAV | Old feature requires `Output` | Strong candidate already passed | Format, dimensions, UAV flag, and post-DLSS consumers |
| Same-frame depth | Old feature accepts optional `Depth` and `DepthInverted`; renderer exposes several depth resources | Available in the renderer, but not proven to reach NGX | Presence, format, resolution, reverse-Z, and frame alignment |
| Dense 2D motion vectors | Old feature requires `MotionVectors` and recognizes optional scale/offset keys; renderer names `TemporalAA2_ResolveDenseVelocity` | Strong candidate exists; scale/offset usage is unproven | `RG16F`/`RG32F`, low/full resolution, units, signs, jitter inclusion, sky/foliage coverage |
| Jitter X/Y in render-pixel units | Renderer names `mTUJitter` and `fJitterOffset`; old feature does not request modern `Jitter.Offset.X/Y` | Available but not connected to old NGX | Locate values and verify sequence, sign, axes, and range |
| Actual render-subrect dimensions | Old path has `Width`, `Height`, `ResourceWidth`, `ResourceHeight`, `OutWidth`, and `OutHeight`; renderer has content/dynamic-resolution fields | Likely derivable at create/evaluate | Determine fixed versus dynamic dimensions each frame |
| Exposure or AutoExposure | Renderer names `fExposureValue` and auto-exposure/luminance passes | Required policy for a linear/HDR input | Identify a 1x1 exposure texture and pre-exposure convention, or deliberately create DLSS with AutoExposure |
| History reset | Old feature accepts `Reset`; TAA has reset passes | Likely wired in some form | Verify first frame, loading, camera cuts, teleports, cutscenes, and resolution changes |
| Correct mip bias | No DLSS-specific bias confirmed yet | Unresolved | Locate sampler/material LOD bias control and determine existing DLSS behavior |
| Correct D3D12 states/lifetime | Existing NGX path owns a dedicated command list/allocator | Likely mostly usable | Observe transitions and ensure inputs are non-pixel SRVs and output is UAV at evaluation |

## Old and current NGX overlap

The shipped 1.1.13 feature's own diagnostic strings prove that it recognizes
these evaluation keys; runtime capture must establish which ones MHW supplies:

```text
Color
MotionVectors
PreviousOutput
Depth
DepthInverted
Output
Reset
MV.Scale.X
MV.Scale.Y
MV.Offset.X
MV.Offset.Y
```

The current 310.7 direct D3D12 helper's hard per-evaluation inputs are:

```text
Color
Output
Depth
MotionVectors
Jitter.Offset.X
Jitter.Offset.Y
DLSS.Render.Subrect.Dimensions.Width
DLSS.Render.Subrect.Dimensions.Height
```

`Reset` is required only on a temporal discontinuity. MV scale defaults to
`1,1` and should be set explicitly when conversion to pixel units is needed.
Linear/HDR input additionally needs an exposure texture unless the feature was
created with AutoExposure.

The material differences are explicit jitter, current creation flags, actual
render-subrect dimensions, and exposure policy. `PreviousOutput` is obsolete in
the standard current helper API.

A direct DLL replacement is not a sound architecture: the old feature reports
NGX API `0x12`, the current feature reports `0x13`, and their export sets differ.
The host executable embeds the old NGX core and dynamically loads signed feature
snippets. A replacement also cannot manufacture the missing jitter values.

## Clean observation boundary

`MonsterHunterWorld.exe` exports its embedded NGX API by name. Relevant exports
for the recorded executable hash include:

| Export | RVA |
| --- | ---: |
| `NVSDK_NGX_D3D12_CreateFeature` | `0x273F060` |
| `NVSDK_NGX_D3D12_EvaluateFeature` | `0x273F0D0` |
| `NVSDK_NGX_D3D12_EvaluateFeature_C` | `0x273F140` |
| `NVSDK_NGX_D3D12_ReleaseFeature` | `0x273F430` |
| `NVSDK_NGX_Parameter_SetD3d12Resource` | `0x273E720` |
| `NVSDK_NGX_Parameter_SetF` | `0x273E780` |
| `NVSDK_NGX_Parameter_SetI` | `0x273E7D0` |
| `NVSDK_NGX_Parameter_SetUI` | `0x273E830` |

Those exported bodies are inside the executable's packed/encrypted `.text`
section and are deliberately **not** detoured. The diagnostic resolves only the
four parameter getter exports. It calls them normally only in the explicit
second-stage parameter-query mode, after NVIDIA's evaluation returns.

The actual detour surface is the clean, signed 1.1.13 feature module:

| `nvngx_dlss.dll` export | Recorded RVA |
| --- | ---: |
| `NVSDK_NGX_D3D12_CreateFeature` | `0x187A0` |
| `NVSDK_NGX_D3D12_EvaluateFeature` | `0x187E0` |
| `NVSDK_NGX_D3D12_ReleaseFeature` | `0x187C0` |

Exports are always resolved by name; RVAs are cross-checks only. The probe
refuses unknown host/feature hashes or mismatched API (`0x12`), snippet
(`0x1010D`), and application (`0x0B9D2F5C`) identifiers, and rejects already
modified in-memory target prologues. It calls the original
function first, verifies the command object's D3D12 COM interface, then queues a
bounded snapshot for background logging. This API check is essential because
the feature DLL's D3D11, D3D12, and Vulkan lifecycle export names alias the same
addresses. The default smoke-test mode does not call host parameter getters;
the opt-in second stage stops all further queries on its first getter or
resource-description exception. No GPU resource is mapped, copied,
transitioned, retained, or released.

## Modern native NGX contract to preserve

Creation must supply the queried input and output dimensions and quality mode.
Set only the flags that describe the observed inputs: HDR, low-resolution motion
vectors, jittered motion vectors, inverted depth, and/or auto exposure. Linear
color is HDR for this flag even when the final presentation is SDR.

Each evaluation needs:

- Input color, depth, and motion vectors in
  `D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE`.
- A distinct output created with `D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS`
  and placed in `D3D12_RESOURCE_STATE_UNORDERED_ACCESS`.
- Exact render-pixel jitter in `[-0.5, +0.5]` on each axis and actual
  render-subrect dimensions. Non-origin color/depth/MV/output subrect bases must
  also be provided.
- Correct motion-vector scale and direction: after scaling,
  `previousPosition = currentPosition + MV`, with both camera and object motion.
  Explicitly classify low/full-resolution and jittered/non-jittered vectors;
  full-resolution vectors must be dilated.
- A valid 1x1 exposure texture for linear/HDR color or intentional AutoExposure.
  If MHW pre-exposes color, pass the matching pre-exposure value and exposure
  scale.
- A reset flag when, and only when, a temporal discontinuity occurs.

DLSS should replace the TAA resolve near the beginning of post-processing. The
UI should be composed afterward, and downstream post-processing must consume
output-resolution data. The host must restore command-list pipeline state after
NGX evaluation. NGX access must be serialized, resources must remain resident
through execution, and the feature must be recreated when creation-time
dimensions or formats change.

For each output resolution and mode, query `NGX_DLSS_GET_OPTIMAL_SETTINGS` and
keep dynamic resolution within its inclusive min/max bounds at a constant
aspect ratio. Texture sampling also needs a matching mip bias:
`nativeBias + log2(renderWidth / displayWidth) - 1 + epsilon`.

For DLAA, input and output dimensions are equal and the input contract is
otherwise identical. Its jitter sequence needs at least eight phases. This
remains the smallest visual proof of concept.

## Next gate

1. Reproduce against a Steam-verified installation, then load
   `mhw_dlss_probe.dll` through a documented, reversible loader.
2. Enable the game's shipped DLSS in DX12 and run a short offline lifecycle-only
   smoke test; confirm `api=d3d12` and unchanged behavior.
3. In a separate run, opt into parameter queries and record the actual creation
   values and per-frame resource descriptions.
4. Correlate the known command list with queue submission and `Present`, then
   observe resource barriers for only the identified textures.
5. Locate `fJitterOffset` only after the existing NGX feed is known.
6. Decide whether a direct current-NGX sidecar or a Streamline/manual-hooking
   backend can coexist safely with the embedded legacy NGX core.

Do not integrate Frame Generation during this phase. It additionally requires
Present/swap-chain interception, Reflex, and HUD-less/UI buffers.

## Authoritative references

- [NVIDIA DLSS SDK 310.7.0](https://github.com/NVIDIA/DLSS/tree/v310.7.0)
- [Native D3D12 DLSS evaluation structure](https://github.com/NVIDIA/DLSS/blob/v310.7.0/include/nvsdk_ngx_helpers.h)
- [Native DLSS creation structure](https://github.com/NVIDIA/DLSS/blob/v310.7.0/include/nvsdk_ngx_params.h)
- [Native NGX definitions and creation flags](https://github.com/NVIDIA/DLSS/blob/v310.7.0/include/nvsdk_ngx_defs.h)
- [NVIDIA Streamline 2.12 DLSS guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.12.0/docs/ProgrammingGuideDLSS.md)
- [NVIDIA Streamline 2.12 frame-generation guide](https://github.com/NVIDIA-RTX/Streamline/blob/v2.12.0/docs/ProgrammingGuideDLSS_G.md)
