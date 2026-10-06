# First offscreen DLAA SDK output

2026-09-25: **three native-resolution NVIDIA DLAA evaluations pass** using
the verified live-owned input files from [run 51256](live-owned-temporal-inputs-result.md).
The public SDK initializes, creates a DLAA feature, evaluates, completes GPU
work, produces a full image, releases the feature/parameters and shuts down.
D3D12 debug and GPU-based validation report **zero warnings and errors**.

This is the first verified modern SDK output on these inputs. Each saved frame
uses a separate process, fresh feature and reset history. It proves the bounded
SDK resource/lifecycle path; it does **not** establish temporal accumulation,
final image quality or in-game DLAA. No game process was opened, DLL deployed,
game setting changed or original asset modified during this experiment.

## Public SDK and reproducible build

Optional target `mhw_dlaa_replay`, enabled only by `MHW_NGX_SDK_ROOT`.
`cmake/NgxReplay.cmake` verifies the four included headers and linked library
against the public NVIDIA/DLSS commit
`a291cc7d2cc642a51566f3dfd5376f635cd1b284`.
The SDK is supplied locally and stays ignored; no SDK files are vendored.

- Release provider: `lib/Windows_x86_64/rel/nvngx_dlss.dll`, version **310.7.0.0**,
  58,977,904 bytes. Authenticode signature verified valid before use.
  SHA-256 `be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e`.
- Static NGX library: `lib/Windows_x86_64/x64/nvsdk_ngx_d.lib`, the documented
  `/MD` variant; this target explicitly uses the matching runtime.
  SHA-256 `31e82b4ec3242ec6e5b42c73e1f5f5e260338a6ee213bd64444f6c2fa364aa84`.
- License: NVIDIA RTX SDKs License, dated 2024-03-14, SDK `LICENSE.txt`.
  SHA-256 `dc2778a3283427285984cdb5b3f7f03eae7d8a06057f672f2999c5ee7fd4f67d`.
- Public `NVSDK_NGX_D3D12_Init_with_ProjectID`, custom engine version `0.1.0`,
  project GUID `956cdd19-7d3c-4660-b664-91c20d2bc9bd`. No original game's AppID
  or private native feature entry points are used. API version is **21**.

The provider search path is explicit. The harness checks the provider hash
before initialization and verifies the actual loaded module's path and hash
after the capability query. Availability is 1, driver-update requirement 0,
and feature initialization result 1. SDK logging uses a count-only callback;
raw SDK messages are not retained.

The [pinned NVIDIA programming guide](https://github.com/NVIDIA/DLSS/blob/a291cc7d2cc642a51566f3dfd5376f635cd1b284/doc/DLSS_Programming_Guide_Release.pdf)
documents these formats, resource states, custom project initialization and
the create/evaluate contracts. Calls are serialized. Private command lists
avoid changing the game's pipeline bindings.

## Parameter and ownership contract

| Setting | Value and scope |
| --- | --- |
| API / adapter | D3D12, captured LUID 0:97816, vendor 4318, device 11141 |
| Mode / extent | DLAA (enum 5), input = output = 2560x1440 |
| Requested preset | K (enum 11); actual model-weight identity is not introspected |
| Color | R32G32B32A32_FLOAT, original linear RGB, alpha 1 |
| Depth | R32_FLOAT, inverted depth flag, original normalized values |
| Motion | R32G32_FLOAT, converted current-to-previous pixel displacement |
| Motion flags | Input-resolution nondilated vectors, scale (1,1), jitter excluded |
| Output | Private R32G32B32A32_FLOAT UAV; no presentation |
| Exposure | HDR + automatic exposure; pre-exposure/exposure scale 1 |
| History | Reset on every independent saved-frame evaluation |
| Jitter | (0,0) for this reset control; not a live jitter submission claim |
| Frame interval | 16.666666 ms control value, not captured frame timing |

Exposure and zero jitter are explicit control choices. These captures do not
include a per-frame projection-jitter receipt, so this result cannot certify
the final live jitter/exposure contract. Reset also prevents this experiment
from testing how motion vectors affect accumulated history.

The standalone process owns its device, direct queue, allocator/list, fence,
three input textures/uploads, output texture, readback and descriptor heaps.
Uploads transition COPY_DEST to NON_PIXEL_SHADER_RESOURCE and complete at
fence 1. Feature creation completes at fence 2. Evaluation starts with input
textures in NON_PIXEL_SHADER_RESOURCE and output in UNORDERED_ACCESS.
Output RGB is first cleared to a sentinel with separate CPU/GPU UAV descriptors
and an ordering barrier, following Microsoft's
[clear contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12graphicscommandlist-clearunorderedaccessviewfloat).
After Evaluate, output transitions to COPY_SOURCE for readback. Fence 3 must
complete and the list/allocator must retire before mapping or SDK destruction.

All three fence waits and retirement operations succeeded in every process.
ReleaseFeature, DestroyParameters and Shutdown1 all returned success. On an
uncertain partial recording or submission, the standalone failure policy
retains its context until process exit. This is not a live feature failure path.

## Independent output verification

`tools/replay_dlaa_capture.py` verified all **232 files** in the sealed source
archive before running the preserved executable. The source executable hash
and swap-chain format 87 are retained in the receipt. The standalone harness
creates no swap chain. `tools/review_dlaa_replay.py` separately checks input and
output hashes, API call ordering/results, GPU completion/retirement, diagnostics
and every output RGB value against the sentinel and finite-value requirements.

| Source frame | Changed RGB values / 11,059,200 | Mean absolute RGB difference |
| --- | ---: | ---: |
| 25383 | 11,058,794 | 0.0015634387 |
| 25384 | 11,058,777 | 0.0015579199 |
| 25385 | 11,058,760 | 0.0015354247 |

All **33,177,600 RGB values** are finite and overwritten. The outputs are
nonconstant and distinct from input, with 0.986 approximate RGB correlation.
All alpha values happen to be 1; alpha is excluded from the required RGB check.
Three output files total **176,947,200 bytes**. Difference/correlation measure
output behavior, not anti-aliasing quality. The largest changes are bright HDR
outliers; no quantitative quality acceptance is inferred from them.

Previews use the same Reinhard + sRGB diagnostic display transform on both
input and output; they do not reproduce the game's post-processing. Inspection
shows the scene intact without a blank output or obvious whole-frame corruption.
Full-size raw outputs remain available for later comparisons.

The target builds in Release with `/W4 /WX`. Pure contract tests cover invalid
extents, incomplete planes, nonfinite color/motion, invalid depth and alpha.
Three relevant CTest entries pass: `dlss_sdk_contract`, `temporal_gpu_contract`
and `temporal_gpu_typed_warp`. No recorder source changed; its previously
completed native regression suite was not repeated.

## Evidence and next step

Private evidence: `local/dlaa-sdk-replay-20260925/v1/`, including preserved
executable/source, preflight/result, three native event logs/raw outputs,
independent `verification.json`, previews and `archive.json`.
The original live capture remains sealed and unchanged. The exact previous
MO2 DLL remains restored; no new diagnostic is installed or armed.

Next implement the same public SDK contract against the existing live-owned
GPU textures on the game's device, initially keeping output private. This
checks coexistence with the game's legacy NGX path and borrowed-device lifetime.
Then enable measured per-frame jitter, persistent history and TAA output
replacement, followed by motion/occlusion, menu, camera-cut, resolution,
alt-tab and recreation checks. Another input-inventory capture is unnecessary.
