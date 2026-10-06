# Native temporal resolve replacement, v1.19.0

2026-09-27. Implements the per-invocation replacement from the
[native TAA assessment](native-taa-reuse-assessment.md). The user requested the
temporal path and bypass first, with mode state and UI integration afterward.
**Built and verified offline; not deployed or tested in the game.**

## Implemented

`src/continuous_dlaa/runtime.cpp` now replaces the admitted main TAA Dispatch
instead of always executing TAA before DLAA. Native preparation, depth-copy
capture, output ring, enclosing compute consumer, barriers and downstream routing
remain intact. The existing frame/epoch/view matching and resource retention
checks still apply. Native reset initialization and optional sharpening remain
unchanged at this stage.

Before claiming a dispatch, the runtime verifies the exact captured shader and
six-SRV/one-UAV binding recipe, full output extent, input/output states, matching
depth lease, retained resources, history decision and unfiltered recovery
eligibility. Shader bytes are copied and compared on every admitted check; their
hash is verified on first acceptance. A matching pointer alone cannot reuse a
previous validation. Unknown variants reject before replacement.

`src/continuous_dlaa/resolve.cpp` is the shared GPU transaction used by the runtime
and hardware fixture. It records conversion, DLAA and a full write into the native
R11G11B10 output. It never has access to the original TAA callback. An SDK failure,
including failure after output handoff, restores bindings and copies the complete
unfiltered current color over the output. Unknown restoration state is escalated;
it never causes a retry through native TAA. An explicit stop after a dispatch is
claimed still supplies unfiltered output for that invocation.

`dispatch_resolve` calls the original Dispatch only for an unclaimed invocation.
Successful, failed and exceptional claimed replacements cannot fall through to
that call. Runtime telemetry separates candidates, unclaimed native dispatches,
reconstructions, intentional unfiltered writes, recovered failures and unresolved
failures. It explicitly reports `exclusive_mode=false`.

The unfiltered copy helper now exposes read-only eligibility validation so the
recovery route can be checked before any SDK commands. It still checks same-device
objects, format, dimensions, single subresource, DIRECT command list and supported
source-state contract; the caller must prove actual GPU states and lifetime.

## Verification

Release build with `/W4 /WX` passes. The first test build exposed a missing include
directory and an always-throwing test lambda's unreachable return; both were fixed
without disabling warnings. Logs are retained.

All **25 focused native checks** pass, including recipe/alias rejection, no native
retry for every claimed outcome, actual callback forwarding for unrelated and
unqualified work, jitter restoration, request lifecycle, history, two-queue
submission and Reset/fence retirement. Existing selection-policy tests remain
passing; they do not imply integration of that controller into this runtime.

Six RTX hardware cases use the same `record_resolve` and `dispatch_resolve` code:

| Case | SDK attempts | Native TAA resolves | Unfiltered copies |
| --- | ---: | ---: | ---: |
| Normal replacement | 1 | 0 | 0 |
| Stop after claim | 0 | 0 | 1 |
| Failure after conversion | 1 | 0 | 1 |
| Simulated failing Evaluate result | 1 | 0 | 1 |
| Failure after native-output handoff | 1 | 0 | 1 |
| Continuous replacement | 600 | 0 | 0 |

Every case passes D3D12 GPU validation with zero warnings/errors and binding
restoration checks. Each final readback checks all 3,686,400 output pixels and
following compute/draw results. Successful output matches independently packed
DLAA RGB; stop/recovery output equals archived raw color exactly. The continuous
case uses two queues, three list/allocator slots and 60 output allocations; all
600 injected recordings retire and release, peak three retained and zero left.
Incomplete bindings, wrong output format and source/output aliasing reject before
SDK recording. Failure injection is an owned fixture, not a manufactured real
vendor crash or a device-loss test.

Fixtures use private archived game inputs and owned D3D12 resources with synthetic
temporal metadata. They do not inspect a running game or establish live temporal
alignment, visual quality, sauna coverage, or an FPS gain. Continuous final pixel
verification is a final-frame readback, not a readback of every intermediate frame.
Ignored evidence: `local/native-resolve-20260927/`.

## Remaining integration boundary

This is a qualified-dispatch implementation, **not yet a complete exclusive-AA
mode**. Unqualified invocations still follow the old native rendering path; a
diagnostic stop disables hooks and restores that path. Do not deploy this build
as the agreed latched-off/unfiltered feature. Those behaviors must be replaced by
mode-wide ownership before a feature test. SDK failure recovery here supplies the
current invocation only; it cannot remove jitter already used for rasterization.

The next stage must connect internal activation independent of the player's AA
choice, suppression of other AA, one jitter owner, sharpening/output selection,
and persistent failure/suspension handling. The warning popup and menu lockout
remain deferred. No new mode/configuration was installed, native AA and DLSS remain
Off, and the exact previous diagnostic DLL remains in the MO2 slot.

The integration retains the host identity and complete code gates documented in
the assessment; no new game offsets or native hooks were introduced.
