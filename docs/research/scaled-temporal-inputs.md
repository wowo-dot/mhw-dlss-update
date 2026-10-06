# Scaled native temporal inputs: v1.23.0

Prepared 2026-09-27 after the successful v1.22.1 native 0.5-scale run.
This is a three-frame input capture, not a DLSS evaluation. It combines the
proven native active-scale setter with the existing native temporal-stage
activation and GPU input recorder. The continuous DLAA runtime is unchanged.

## Question and scope

Native active scale 0.5 produced 1280x720 viewports at 2560x1440 while allocation
and content scale stayed 1. We must establish the temporal dispatch rectangle,
resource dimensions and pixel region before passing smaller inputs to a
full-size DLSS output. Changing the current DLAA dimensions blindly would not
establish that relationship.

The capture records seven native bindings, scene depth, three raw color/depth/
motion image sets, the compute shader, dispatch groups, queue/submission/fence
evidence and matching native scale stamps before and after each producer call.
Existing typed GPU conversions are also retained for comparison. Those use
allocation dimensions: their arithmetic can be checked without claiming that
their motion-vector units are correct for scaled DLSS. No NGX startup or SDK
evaluation occurs. Native TAA temporarily executes so its existing resource
preparation and routing can be observed; this diagnostic does not bypass it.

## Hook and lifetime contract

Tested executable identity is SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`,
file version 0.0.0.0. Offline code is from captured executable SHA-256
`d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.
All native spans are rehashed before hook installation. Addresses are RVAs for
this exact build, not portable signatures.

The 20 hooks reuse documented x64 ABI and lifetime contracts:

| Component | Hooks | Contract |
|---|---:|---|
| Native scale request and viewport observer | 2 | [Scale control](dlss-render-resolution-control.md): renderer policy at RVA 0x229ed30, original RCX and qualified caller; D3D12 viewport observer |
| Native frame, queue, TAA, comparison and jitter callbacks | 8 | [Native activation](native-aa-activation.md): original argument/register forwarding, scoped queue/context ownership, no saved preference writes |
| TAA producer, consumer, barriers, submission and depth routes | 10 | [Owned inputs](owned-temporal-gpu-inputs.md) and [recorder](../runtime/temporal-owned-inputs.md): producer RVA 0x23c9980/caller 0x2392f31, pinned resources/lists, packet generations, bounded readback and GPU retirement |

Installation first enables scale control, waits at most five seconds for at
least 32 applied observations, and queues the activation hooks without applying
them. The input recorder checks original code and installs its observers before
activation is applied. This preserves its whole-TAA-function code gate. The
scale hook is outside those checked functions.

The producer requires an identical view/frame/display/render/content-scale
stamp on both sides of the native call. Capture admits three frames within its
existing two-second window. Shutdown drains capture callbacks and GPU work,
closes the journal before removing observers, disables activation after its
scoped context drains, then restores native scale through the original render
thread path. Trampolines and the DLL stay pinned. Failures are logged; a failed
restoration must not be described as a successful cleanup.

The resolution record's zero injected-command count describes the scale
component only. The input recorder separately logs its bounded GPU copies and
conversions; the combined experiment does perform that GPU work.

## Verification and live procedure

Build uses C++20, /W4 and /WX. Native tests cover scale geometry/ownership/ABI,
activation, and WARP GPU-copy success, wrong state, absent retirement and signal
failure. Retained WARP pixels are reviewed under the new header, packet-generation
and image checks. WARP supplies synthetic game identity and dimensions; it is
not evidence that scaled game routing works.

Use the existing bare MO2 profile/launch entry, native AA and DLSS Off, fixed
2560x1440 window. Load a save and stay still. Agent checks the exact process
generation and DLL hash, then arms through the frozen `arm_scaled_temporal.py`.
Namespace is `MhwTemporalInputs-v1`, output `inputs-PID-CREATION.jsonl`.
It captures and restores automatically; no manual AA toggle is needed.

Review with frozen `review_scaled_temporal.py`. Require all three input records,
matching scale stamps, released references, completed GPU work, disabled hooks
and restored full native scale. Inspect actual image regions before choosing
DLSS input subrectangles and motion-vector scale. Keep the current diagnostic
DLL installed after normal exit, as requested. Exact prior DLL and sidecar are
preserved under `local/scaled-temporal-20260927/v1.23.0`; that directory's
`deploy.ps1 -Action Restore` provides an explicit rollback with identity checks.

Live verification remains pending. Do not claim upscaling or resumed DLAA.
