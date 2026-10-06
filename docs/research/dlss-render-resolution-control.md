# DLSS render-resolution control

Active milestone from user decision 2026-09-27. The working experimental DLAA
baseline is v1.21.5. Intermittent recovered depth-copy gaps are now in the
[beta backlog](beta-backlog.md), not a gate for this work.

The requested sequence is scene render-resolution control, modern DLSS
upscaling, Reflex integration/verification, frame generation/MFG, then broader integrated beta testing and
polish. Player-facing configuration, popup and lockout work still waits for
CMFM. Keep existing diagnostics throughout implementation.

## Current code constraints

`continuous_dlaa/runtime.cpp` uses one width/height pair for native-resource
qualification, active rectangle, dispatch groups, private color/depth/motion
allocations, unfiltered recovery and SDK construction. `dlss_sdk/inline.cpp`
creates NGX with identical input/output dimensions and DLAA mode, then hands
off to a same-size native destination. Reducing a size field alone is not an
implemented upscaler and must not be tested with those assumptions unchecked.

Implementation must distinguish display dimensions, allocated texture extents,
active render rectangle and reconstruction output. Motion conversion/jitter
must use their actual pixel-coordinate convention. The final game output and
UI should retain display resolution. Mode/dimension changes require matching
resource recreation and temporal history reset.

## Confirmed static observations

Host and captured executable were hashed before this exploration:

- Host version 0.0.0.0 SHA-256
  `c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
- Captured executable SHA-256
  `d78611731e4a5f214e306d1d577d4782b59adaf1c75de54d2983caa45074884c`.

Local-only disassembly and candidate inventory:
`local/render-resolution-20260927/`. Matching a field displacement in a scan
does not establish object identity; only the renderer-linked observations
below are used. PE unwind regions can split a function, so exported region
starts must not be assumed to be callable entries.

The renderer singleton is host RVA `0x51c4480`. In the native scale-update
block, `+0x22982e7` compares renderer `+0x204` against `+0x200`. On change it
sets update flag `+0x1f548 |= 0x40`, resets `+0x1f0` to 1 and sets
`+0x7b3b3`. The following block compares `+0x1f0` with `+0x1f4`, calls
`+0x255d700` on the main view when they differ, preserves the old value in
`+0x1f8`, copies `+0x200` to `+0x204`, and stores a clamped product in `+0x208`.
These establish two coupled scale channels, not one interchangeable slider.

Renderer view creation at `+0x2293f10` passes display dimensions `+0x198/+0x19c`
and scales `+0x1f4/+0x204` to `+0x255ba70`. That constructor stores scale values
at view `+0x6b0/+0x6b4`, their clamped product at `+0x6b8`, and computes resource
dimensions from display size times the second scale. It also quantizes the
first scale through integer pixel width. The update helper `+0x255d700`
changes the first scale and product and marks view `+0x6bc` dirty.

Temporal allocation at `+0x2391f30` reads display dimensions and renderer
`+0x204`, multiplies each in scalar float, truncates to integers, and allocates
three temporal textures at that resulting size. Therefore changing `+0x204`
also changes those temporal allocation dimensions. It cannot be assumed to
preserve the current native resolve target at display resolution.

The older native NGX sizing path `+0x229a8c0` requires an existing native NGX
wrapper, publishes callback scale/flags at `+0xb0/+0xb4`, and writes a clamped
scale into `+0x200`. That wrapper is absent in the current AA-Off activation
contract. Re-enabling the legacy DLSS provider route is not established as a
requirement for the modern path.

## Runtime route to prove

Prefer an engine-owned sizing/update boundary that keeps camera/view dimensions,
texture allocation and active render rectangle coherent. Compare the first
scale's viewport behavior with the second scale's allocation behavior before
choosing the control point. Trace the final full-size destination and downstream
postprocessing/UI route, then qualify resources by separate input/output
extents. Avoid ad hoc global size writes during command recording.

A first controlled half-size input at the current 2560x1440 display would give
1280x720; this is a diagnostic target, not a hard-coded DLSS quality preset.
Modern SDK mode recommendations must be queried/validated when connected.

## v1.22.1 focused scale probe

Implemented as a separate `mhw_render_resolution_probe` target. It has no NGX
initialization, DLAA injection, AA activation, GPU copies or submitted GPU work.
The previous v1.21.5 DLAA DLL is preserved before replacing the MO2 test slot.
Only the new probe runs in that slot; saved graphics/profile settings stay intact.
Runtime evidence and deployment/restore script live under
`local/render-resolution-20260927/v1.22.1/`. Leave this test DLL installed after
the run unless the user requests restoration.

The live hypothesis is that native active scale can reduce scene rendering
while allocation/content scale remains 1, preserving the full-size output.
This still needs an in-game test. Reduced viewport observations alone do not
prove every scene pass, full-resolution UI, or a valid DLSS reconstruction route.

### Hook contract

All offsets are relative to the pinned host above. Six relevant code spans are
SHA-256 gated against the captured executable and then checked in live memory
before installation; this version intentionally supports only that executable.
Existing structural DX12 discovery validates renderer/backend, device,
swapchain, arena ownership and system API implementation identities.

- Native scale policy update: RVA `0x229ed30`, 493 bytes. Windows x64 ABI, RCX
  renderer, no stack arguments. The wrapper preserves integer/vector arguments
  and return registers. Only the call returning to RVA `0x22982e7`, with the
  exact renderer singleton, may acquire a scale override. Other calls pass through.
- Native scale setter: RVA `0x229bd70`, 105 bytes, Windows x64 RCX renderer,
  XMM1 float scale, void return. Called on that render thread after the original
  policy update, before the original caller applies the scale to the view.
  It truncates display-width times scale, aligns width down to 16 pixels, and
  divides by that width. The test requires allocation/content scale 1. When
  config `+0x2cda` is 1, config `+0x2cd0/+0x2cd4` must exactly equal the fixed
  renderer/display dimensions; otherwise only the regular display-width route
  is accepted. Both source dimensions and the flag are recorded in snapshots.
- `ID3D12GraphicsCommandList::RSSetViewports`: system implementation discovered
  from vtable slot 21 of an owned, closed, never-submitted command list. The
  typed hook forwards unchanged arguments once, then records bounded samples
  only from objects with that same system method. It does not keep COM pointers,
  alter viewports or identify which scene draw consumes a viewport.

Engine renderer/view pointers are borrowed inside its own update invocation;
root, backend and view identity are checked using bounded reads. No worker
writes renderer data. The saved pending request is restored before the original
update on the next invocation so cached native policy does not adopt our
previous override as its own request. A different request written by the game
wins over stale restoration. After 32 full-size baseline visits, the native
setter requests the configured reduced scale each admitted invocation.

Stop/fault stops new overrides, waits for the original caller to apply the
latest native request, then disables both hooks and drains callbacks. The DLL
and trampolines remain pinned until process exit. If restoration cannot be
confirmed within five seconds, cleanup reports failure and leaves the recovery
hook present; it does not claim a successful restoration. The probe has a
five-minute armed limit and a distinct process-generation-specific arm/stop
event namespace. It is not armed merely by loading the DLL.

### First test and interpretation

Use the existing bare MO2 profile, native AA Off, native DLSS Off, DX12, fixed
2560x1440 window. Load a save before arming. At configured scale 0.5 the native
active rectangle should become 1280x720. Check the still scene and a short pan,
then stop and verify restoration before exiting. No area transition, resize or
quality-mode change is needed for this first sizing test.

The reviewer requires full-size baseline, reduced engine/view scales,
unchanged display/content scale, full-size depth **CPU metadata**, both full and
half-size D3D12 viewport observations, and clean restoration. Actual depth/color
GPU resource descriptions and exact viewport-to-scene attribution remain
explicitly unverified. This is resolution control, not a completed DLSS test.

Offline verification: warning-clean `/W4 /WX` Release build; native geometry,
scale ownership, engine forwarding/return ABI, stop, foreign-request and SEH
recovery checks; Python reviewer and process-generation rejection tests. Live
visual/resolution validation remains pending.

### First live admission result

v1.22.0, PID 43364, process creation 134350040609261239, stopped before any hook
installation or scale override. Structural discovery and all six live code
gates passed, but the initial snapshot rejected an enabled alternate width
source. A generation-checked, read-only inspection found flag 1 and configured
dimensions 2560x1440, equal to renderer/display and depth CPU metadata. All nine
active/content scale readings were 1; native provider was absent, view flags
passed, renderer/view pointers remained stable. Thus this was an overly narrow
admission assumption, not evidence of a failed reduced-rendering path.

v1.22.1 admits that source only at an exact dimension match, checks it on every
snapshot, and records its basis. Owned-memory tests exercise both native width
routes and reject wrong width, wrong height and unknown flags; stop/exception
checks now also run with the observed alternate route enabled. No native code
gates, control point, quantization or restoration policy were loosened. No
reduced resolution has yet been demonstrated in the game.
