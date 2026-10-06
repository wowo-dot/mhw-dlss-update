# Bounded live offscreen DLAA — v1.11.0

Completed result: [run 52904 and initialization-order controls](../research/live-sdk-capability-result.md).
Inputs pass, SDK capability gate fails before Create/Evaluate. The previous
DLL is restored. This document describes the frozen v1.11.0 experiment;
subsequent source logging corrections are not a prepared/deployed next test.

Prepared 2026-09-25. This optional diagnostic connects the verified
[live-owned inputs](temporal-owned-inputs.md) to the public SDK tested by the
[offscreen replay](../research/first-dlaa-sdk-output.md). It saves three private
DLAA outputs. It does not replace native TAA, introduce jitter, enable the
game's DLSS option or claim continuous reconstruction.

## Build and unchanged native observation

With the pinned local `MHW_NGX_SDK_ROOT`, build `mhw_temporal_dlss_probe`.
The existing `mhw_temporal_inputs_probe` remains an SDK-free target. The new
target defines `MHW_TEMPORAL_DLSS` and links `mhw_dlss_sdk_live`; NVIDIA
dependencies, hashes and license are specified in `cmake/NgxReplay.cmake`
and the first-output research note. No provider is copied into the game.

All ten native hooks, calling conventions and 51 executable code gates remain
those documented for v1.10.0. Tested host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Decoded image SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
The three selected frames, two-second observation, 150 ms warmup and 36
added native commands are unchanged. Native sources return to their original
states and original TAA always forwards. No SDK call occurs in a hook.

## Handoff and lifetime

After every hook is disabled/drained, the worker requires original producer
generations, completed fences, recording retirement and nine saved raw images.
It then converts the private GPU inputs and saves nine float references.
Converted color/depth/motion must satisfy the DLAA extent, finite-value,
alpha and normalized-depth contract. Only then can SDK work begin.

`Live::converted()` and `completed_fence()` expose nothing before successful
conversion, completion, retirement and CPU readback. All nine converted
textures remain in COPY_SOURCE. The SDK rechecks each texture's device,
format/extent/sample/mip/flags, rejects aliases, and requires the exact
completed conversion fence value 1 and a retired producer. It retains COM
references to all nine inputs and three producer fences.

The SDK owns a separate DIRECT queue/list/allocator/fence, output, readback
and two clear descriptors. Input textures transition COPY_SOURCE to
NON_PIXEL_SHADER_RESOURCE for evaluation, then return to COPY_SOURCE.
Output begins UAV, is cleared to a sentinel and ordered with a UAV barrier,
then transitions to COPY_SOURCE for readback. Subsequent evaluations reuse
the same output only after completion/retirement. Four private Executes and
Signals cover one feature creation and three evaluations. Every wait is
bounded at 15 seconds and checks device removal. SDK calls themselves are
synchronous vendor calls; this is not a deadline guarantee for CreateFeature.

The feature uses native resolution, DLAA mode, requested preset K, HDR,
inverted depth, nondilated input-resolution motion, automatic exposure,
motion scale (1,1), zero jitter and **reset requested on each evaluation**.
Pre-exposure/exposure scale 1 and 16.666666 ms are control values. The output
format is RGBA32_FLOAT; input formats are RGBA32_FLOAT, R32_FLOAT, RG32_FLOAT.
No original-game descriptors, pipeline state or device debug settings change.

After the final fence and retirement, ReleaseFeature and DestroyParameters
must succeed. The worker deliberately does **not call Shutdown1 on the
borrowed game device**. The SDK initialization, pinned diagnostic/provider
and private COM references remain until process exit. This bounded retention
also applies on failure or an uncertain partial recording; it is not the
final production lifecycle. Image allocation is capped at 144 MiB, excluding
SDK-internal memory. At 2560x1440 it is 121,896,960 bytes. SDK outputs total
176,947,200 bytes, within the 192 MiB output budget.

## Provider coexistence

The worker reads an absolute provider directory from `sdk-runtime-dir.txt`
inside its repository evidence directory. The provider must have the pinned
310.7.0.0 SHA-256. An already-loaded copy at that exact path is rejected because
its ownership is unknown. After public project initialization, the loaded
module is identified by its **full path**, hash checked and pinned. It must be
distinct from the previously observed legacy module. The legacy module's
handle and file hash must remain unchanged after evaluation.

This follows Microsoft's [module lookup guidance](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-getmodulehandlew)
for duplicate DLL basenames. The public SDK's
[device shutdown contract](https://github.com/NVIDIA/DLSS/blob/a291cc7d2cc642a51566f3dfd5376f635cd1b284/include/nvsdk_ngx.h)
motivates deferring shutdown on the borrowed device rather than assuming
ownership of every NGX instance there.

An offline hardware fixture loads the game's original **1.1.13.0** DLL
(SHA-256 `d4cc11bcec693dfefd6eddba6dd29bdfc16b4043e7b83150f0097d14b76512d8`)
before using the same live conversion and SDK implementation. Modern/legacy
providers remain distinct, three evaluations and feature/parameter release
succeed, and D3D12 debug/GPU validation is clean. This tests loaded-module
coexistence; coexistence with the running game's NGX initialization is still
the purpose of the live run.

The first and third outputs exactly match the earlier fresh-feature replay.
The middle output differs: RGB mean absolute difference 0.000044289,
correlation 0.99999234, maximum 0.316114 at a bright pixel. All values remain
finite. The cause is unresolved; do not assume a reset request erases every
SDK internal state or infer temporal quality from this fixture. The first
live result is a resource/lifecycle test, with raw outputs preserved for review.

Four hardware negative fixtures reject missing retirement, incomplete fence,
aliased inputs and wrong format before any SDK initialization. Nine reviewer
tests reject damaged SDK identity/parameters, producer receipts, API failures,
incomplete retirement, private/native aliasing, output restoration failures,
unexpected shutdown and incomplete cleanup. Existing owned-input reviewer
tests remain applicable and pass.

The completed native regression run passes all **376** CTest cases. The capture
reviewer passes 61 tests (nine jitter-only skips), the SDK event reviewer passes
nine tests, and the full SDK artifact review checks 33,177,600 RGB values.
A copied output changed to NaN and rehashed is rejected by pixel validation;
the copied file is then restored and its original hash verified.

## Run and rollback

Use `barebonesdlss`, the existing **Monster Hunter: World - DLSS 1.2.110 test**
entry, DX12 On, native DLSS Off, TAA On and 2560x1440 fullscreen. The entry name
does not establish which NVIDIA DLL is loaded; the capture records that hash.
Load one save, stay still and report from the phone. No option toggle or pan
is needed. The agent verifies the exact process generation and installed DLL,
then arms the named event. The recorder waits up to 30 minutes for arming.

Expect the normal TAA scene, possibly a brief pause. The worker saves a
separate `.sdk.jsonl` and three `.dlaa-{0,1,2}.rgba32f` files. The full reviewer
requires all original capture evidence plus exact private-input identities,
SDK ordering/results/fences, output hashes and finite/written RGB pixels.
Native capture success alone cannot accept a v1.11.0 result. The reviewer
does not claim game debug-layer validation or temporal quality.

Evidence/deployment/rollback: `local/dlaa-live-20260925/v1.11.0/`.
After capture, leave DLSS Off and close the game normally. `deploy.ps1 -Action
Restore` verifies the installed test DLL, preserved exact previous DLL and
sidecar, restores the previous slot and removes/restores the sidecar. The
SDK directory pointer and all artifacts remain under the repository.
