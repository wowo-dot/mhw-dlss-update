# Phase 0: Modern DLSS requirements versus Monster Hunter: World

Latest result: [bare-profile run 2632](jitter-bare-profile.md)
retains the zero jitter byte/stored camera jitter in both scenes with no enabled
MO2 mods or detected bridge/mod-loader DLL. Focused consistency passes 11/16
attempts per phase. This narrows the search to native assignment/callback order
without establishing that the sampled layer is active or native TAA is broken.
Normal exit and unchanged settings/profile are verified; no changes require
rollback. GPU/raster behavior and modern SDK integration remain open.

Previous result: [disabled-provider run 8164](jitter-startup-control.md#run-8164-menu-and-gameplay-remain-zero-in-the-control)
retains jitter-enable 0 and zero stored camera jitter in menu and gameplay
without synthetic startup substitution. The backend context is absent, yet
the sampled gate remains Off. Focused consistency passes 12 menu/8 gameplay
attempts; no CPU observation qualifies raster jitter or temporal inputs.
The 30-second passive trace covers startup, not the later samples or shutdown.
Normal exit is reported, settings unchanged and original sidecar restored.
Next is the flag's scene/property assignment or remaining mod/observer effects;
active startup behavior is not required for the zero state. Modern integration
remains inactive and no native source/binary changed.

Previous result: [pre-enable run 5836](jitter-pre-enable-observation.md)
observes zero stored jitter and a disabled TAA jitter flag in menu and gameplay
before any Create/Evaluate, with DLSS left Off. Eleven focused gameplay samples
retain matching counters/epoch and stable scalar reads, without proving GPU
coherence. First-Create hooks are not required for this state; the bridge's
startup callbacks, game properties and other mods remain unresolved influences.
Next is a control isolating bridge startup behavior or an earlier assignment
observation. Saved Off/TAA settings are unchanged; shutdown is unobserved after
trace timeout. No native implementation changed and modern integration remains
inactive.

Previous result: [v0.15.0 run 27200](jitter-scene-probe.md#run-27200-menu-and-loaded-scene-cpu-samples-accepted)
accepts 16 menu and 16 triggered loaded-scene CPU samples. A different TAA
instance in gameplay still has zero jitter and the enable byte Off; every
other captured predicate clause passes. The assignment clearing it remains
unresolved. One Evaluate and a 517-record completion journal are retained,
but an overlapping call keeps the local checkpoint unqualified. Recreation
is refused by the single-use diagnostic; normal exit/settings are verified.
The unchanged build retains 48 native and 78 Python passing tests. Next is
the flag's assignment/pre-enable state; GPU upload, raster jitter, temporal
contents and modern SDK integration remain unverified or inactive.

Latest result: [v0.14.0 run 30924](jitter-sequence-probe.md#run-30924-both-cpu-phases-accepted)
accepts 32 CPU jitter/gate samples with the one-Evaluate budget unchanged.
Both wrapper phases have zero jitter and identical matrices; `mJitterEnable`
is the only false captured predicate clause. The live reset vector is zero.
Clean shutdown, original settings and staged identities are verified; the
unchanged code retains 47 native and 64 Python passing tests. Next is tracing
the scene/property control and preparing a loaded-scene comparison. Main-menu
CPU evidence does not establish earlier callback execution or raster jitter.

Latest result: [v0.13.1 run 21636](camera-type-probe.md#run-21636-cpu-projection-observation-accepted)
accepts source-stamped CPU projection observation with the derived camera,
matching viewport, zero stored jitter and identical matrices. Both table
candidates are nonzero, so table lookup alone is not applied-jitter evidence.
One guarded Evaluate/completion stop and clean Off/apply/exit succeed. The
unchanged build retains 45 native and 52 Python tests. Next is a bounded CPU
sequence of native jitter gates/projection values, now implemented above. Shader/
raster jitter, GPU image alignment, motion/depth and modern integration remain
open; CPU matrix agreement alone does not qualify them.

Latest result: [v0.12.0 run 4768](publication-receipt-probe.md#run-4768-publication-binding-and-completion-stop-accepted)
accepts one CPU publication-bound work item through Evaluate and the recorder's
completion stop/drain with zero journal flags. Stable counters 10791 -> 10792
and bank 1 -> 0 accompany the same descriptor/payload and resource identities.
All 17 queries complete; Off/apply/exit and cleanup succeed with original
settings. The unchanged code retains its 43 native and 42 Python tests, plus
binary/assembly and D3D12 completion checks. Actual shader jitter, motion/depth
conventions, content alignment and complete GPU qualification remain open.
The offline projection trace now motivates v0.13.0 above; no unchanged rerun
is needed. GPU readback and shader execution validation remain future work.

Latest: [v0.11.0 run 17204](frame-packet-probe.md#run-17204-producerpreparation-counter-mismatch)
rejects the first preparation because the global renderer counter advances
from 24291 to 24292 after producer capture. Both snapshots are stable and their
renderer, rectangle and reset bits match. No Evaluate occurs; the markerless
journal overflows. Off/apply/exit and cleanup succeed with original settings.
The [offline queue trace](queued-work-provenance.md) now identifies a direct
producer-to-descriptor link and the separate epoch/command-bank roles; 96 owned
enqueue replays pass. This motivated the receipt now implemented in v0.12.0.
The historical v0.11.0 build passed 38 CTests and 37 Python tests; shader
jitter/content and completion-bound game acceptance remain open.

[Current result: v0.10.0 run 47820](guarded-depth-probe.md#run-47820-one-corrected-item-recovery-and-clean-exit)
accepts one guarded depth correction, four preparation calls, one completed
consumer/Evaluate and subsequent main-menu TAA fallback. All 17 parameter
queries completed. The user turned DLSS Off and exited normally; Release and
both Shutdown calls succeeded. All 33 tests and binary audits still apply.
GPU state/order remains unqualified: the Evaluate barrier snapshot is incomplete
and the post-Evaluate journal reached capacity. The subsequent
[offline review](submission-order-and-temporal-inputs.md) reconstructs expected
state declarations and a same-queue signal/completed-value chain while
preserving those coverage flags. Frame-indexed jitter tables are located;
live values, units, contents and frame alignment remain unverified. The next
diagnostic design binds these inputs to one work item and stops the journal
after its bound completion observation. That design is implemented in v0.11.0 above.

[Current result: run 45100](native-taa-fallback.md#run-45100-recreation-and-menu-toggle-accepted)
accepts v0.9.2 fallback recreation and menu toggling: three generations retired,
34/34 sizing successes and selectable On after the requested save/menu return
and deliberate Off/On cycle. The user exited normally with original Off/TAA
settings. All 29 CTests pass, including 256 generations and concurrent cleanup.
Alt-Tab and wider display/device transitions remain untested. The depth
selector and modern SDK are still inactive; guarded depth/producer integration
is the next implementation milestone.

[Earlier result: run 19484](native-taa-fallback.md) verifies the v0.9.1 sizing
fix in game. The user reports normal rendering, but automatic Release still
leaves DLSS unavailable. Offline replay confirms post-Release sizing/Create
rejections and the retained handle binding. This motivated v0.9.2; full
menu-lifecycle acceptance remains pending. The backing selector
and modern reconstruction remain inactive. Older decisions and evidence follow.

[Earlier decision after run 17772](current-state.md): the next three-Evaluate
attempt stopped on the previously observed tracker state 0x8 before applying
any correction. The checkpoint revision was not exercised. Keep the game
closed for offline backing-selection and fallback design; do not repeat the
unchanged test. The accepted evidence and historical gap discussion follow.

Current status (2026-09-22): [run 46868](three-frame-result.md) accepts three
consecutive depth-argument corrections, first-Evaluate binding to four real
2560x1440 resources, and closed/submitted depth recordings in all three CPU
intervals. Their continuous declaration chains end at combined shader read
`0xC0`; the fourth fault was forwarded when the correction budget was spent.
The 807-call journal is complete within its bounded scope. Later Evaluate
contracts were not captured, GPU completion/contents are unverified, and modern
DLSS is not invoked. Next work is bounded per-Evaluate input capture with fence
completion evidence, then motion/jitter, depth convention and exposure validation.
The [first v0.8.0 run](multi-evaluate-first-result.md) captures one Evaluate and natural fence queries, but an unrelated parallel barrier causes its global checkpoint to reject the next correction. The [v0.8.1 checkpoint revision](scoped-checkpoint-review.md) passes the saved-run replay and all 26 local tests; three-Evaluate game acceptance and captured-submission completion remain pending. Historical progress through v0.7.2 follows.

Historical status: static inputs mapped; v0.5.2 confirms DLSS capability delivery through
the MHW return. Create/Evaluate resources have not been captured. An earlier
gate was the [separate ISP lifecycle](isp-prerequisite.md): v0.5.3 confirms
ISP availability `0` and Init failure while DLSS capability delivery succeeds.
Version 0.6.0 implements the
[two-provider Create capture](two-provider-create-capture.md). Its tests passed
ISP publication but crashed before Create in a parameter-published callback.
The [v0.6.1 containment](optimal-settings-callback-crash.md) passed in run 34496:
the local sizing callback captured four inputs, DLSS became visible but
unavailable, and both bridge shutdown paths completed. The
[v0.6.2 sizing diagnostic](diagnostic-optimal-settings.md) now returns controlled
1:1 output metadata. Run 20164 confirms four successful sizing responses and
a selectable DLSS menu setting, with both bridge shutdown paths complete.
[Run 19800](first-create-capture.md) left Options while On and captured the first
DLSS Create with a verified D3D12 command list and all 11 scalar queries completed.
Nine keys were present; ResourceWidth and ResourceHeight were absent. The
intentional no-handle failure and both bridge shutdown paths passed. Evaluate
inputs and resource descriptions remain uncaptured. Version 0.7.0 prepares a
[one-Evaluate diagnostic](one-evaluate-capture.md) after static review of the
core's copied handle and the game's cleanup path. [Run 32832](first-handle-lifetime.md)
now accepts Create success and copied-handle Release before both Shutdowns,
with zero Evaluate calls. [Loaded-scene runs 37452 and 28340](loaded-scene-evaluate-gate.md)
also have zero Evaluate calls; the upstream scheduling gate and frame-input
capture remain unresolved. DLSS can only be changed from the main menu.
Version 0.7.1 adds [read-only renderer gate observation](renderer-gate-observation.md)
after Create; all 13 tests pass. Run 35100 records two valid early samples with
all selector gates ready, but no Evaluate before early Release. Its post-Release
sizing rejections explain the diagnostic's terminal menu availability; the
precise Release reason and loaded-scene behavior remain unresolved. The next
attempt started Off and enabled once at the main menu. Run 43380 then reached
a loaded save and captured 12 ready-gate samples over a 23.2-second lifetime,
without sizing rejection or exhaustion, followed by clean exit cleanup. Zero
Evaluate persists. Temporal-layer activation and enqueueing are now the next
investigation; their execution is not instrumented by this build.
Version 0.7.2 adds [read-only historical cache and deferral observations](temporal-activation-observation.md),
with all 14 tests passing and live acceptance pending. These can guide the
next boundary investigation but do not count pass or enqueue executions.

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
| Jitter X/Y in render-pixel units | `mTUJitter` selects frame-indexed 18/20-entry sources for `fJitterOffset`; old feature does not request modern `Jitter.Offset.X/Y` | Static address formulas located; live values and units unverified | Capture active mode/value with producer frame; verify actual shader binding, sign, axes, units and range |
| Actual render-subrect dimensions | Old path has `Width`, `Height`, `ResourceWidth`, `ResourceHeight`, `OutWidth`, and `OutHeight`; renderer has content/dynamic-resolution fields | Likely derivable at create/evaluate | Determine fixed versus dynamic dimensions each frame |
| Exposure or AutoExposure | Renderer names `fExposureValue` and auto-exposure/luminance passes | Required policy for a linear/HDR input | Identify a 1x1 exposure texture and pre-exposure convention, or deliberately create DLSS with AutoExposure |
| History reset | Old feature accepts `Reset`; TAA has reset passes | Likely wired in some form | Verify first frame, loading, camera cuts, teleports, cutscenes, and resolution changes |
| Correct mip bias | No DLSS-specific bias confirmed yet | Done in 0.31.0 (2026-10-19): NVIDIA's log2(scale) - 1 from the game's own sampler banks, live (`texture-mip-bias.md`) | Locate sampler/material LOD bias control and determine existing DLSS behavior |
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

The probe also observes two system-DXGI COM methods so a run can identify its
actual presentation target without deriving a swap-chain pointer from protected
game code:

| Interface method | Vtable slot | Calling convention |
| --- | ---: | --- |
| `IDXGIFactory::CreateSwapChain` | 10 | `STDMETHODCALLTYPE` |
| `IDXGIFactory2::CreateSwapChainForHwnd` | 15 | `STDMETHODCALLTYPE` |

Targets are resolved from a private `IDXGIFactory2` instance. Each must be an
executable `MEM_IMAGE` address owned by the loaded system `dxgi.dll`; otherwise
the probe aborts. The hooks are armed after the exact host hash is accepted and
before waiting for `nvngx_dlss.dll`, because DXGI cannot enumerate an already
created swap chain. Both detours call the original first, preserve `LastError`,
and inspect only a successful, non-null output. They accept a D3D12 creation as
such only when the supplied object supports `ID3D12CommandQueue`.

The NGX command list and swap-chain queue each yield an `ID3D12Device` adapter
LUID. The writer thread resolves that LUID to `DXGI_ADAPTER_DESC1` and reports a
complete runtime identity only when the LUIDs match, adapter lookup succeeds,
the owning window belongs to the current process, dimensions are nonzero, and
the format is not `DXGI_FORMAT_UNKNOWN`. COM references acquired for inspection
are released immediately. Only fixed-size snapshots cross the detours; string
conversion, adapter enumeration, and file output remain on the writer thread.
Multiple same-process candidates make identity explicitly ambiguous rather than
selecting one by guess. Swap-chain pointers and HWNDs are logged only as
process-local correlation values. Swap-chain resize observation is deliberately
deferred; an identity record therefore labels its descriptor as a creation-time
observation.

The DLL and its hook trampolines have process lifetime. The loader must retain
its initial module reference until the worker self-pins the probe; hot-unload is
unsupported. After any possibly-live hook transaction fails, observation is
atomically disabled and MinHook is asked to disable all targets, but code,
trampolines, module references, and the writer event deliberately remain valid
until process exit so an already-entered detour cannot return through freed
memory.

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

1. Stage `mhw_dlss_probe.dll` as a distinct, disabled MO2 mod through the
   documented CMake install target.
2. Reproduce against a Steam-verified installation, disable or explicitly
   record unrelated forced-library injections, then run a short offline
   lifecycle-only smoke test with the game's shipped DLSS in DX12. Require
   `runtime.identity complete=true` and unchanged behavior.
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
