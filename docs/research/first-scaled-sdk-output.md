# First scaled SDK output — owned-device replay

Accepted 2026-09-27: the modern SDK reconstructed three saved 1280x720 scene
inputs to 2560x1440 using Performance mode and preset K. This is an offscreen
GPU integration milestone. No new probe was deployed and no game was launched.
Live scaled jitter, retained upscale history and the game's output routing are
still pending. The installed v1.23.3 DLL remains in place, stopped/unarmed.

## Geometry and implementation

`src/dlss_sdk/geometry.hpp` separates raw allocation, active render and output
extents. The initial route supports a top-left static rectangle and accepts
only the SDK's exact recommended Performance input size. Invalid bounds, modes,
aspect ratios, undersized allocations and inconsistent optimal ranges reject.
The existing InlineFrame width/height constructor delegates to equal-size DLAA.

For this capture, raw copies remain 2560x1440 while converted color/depth/motion
are tightly allocated at 1280x720. The existing converter loads only that active
rectangle and decodes motion in render-pixel units. SDK output, packed output,
handoff, output readbacks and downstream fixture consumers are 2560x1440.
SDK motion scale stays [1,1]. Input and output dimensions are logged separately.
The Performance optimal query returned 1280x720, minimum 1280x720 and maximum
2560x1440. Create, Evaluate, ReleaseFeature and DestroyParameters all succeeded.
Existing DLAA initialization does not gain a new optimal-settings dependency.

The SDK/header/static-library pins remain unchanged: public NVIDIA/DLSS commit
`a291cc7d2cc642a51566f3dfd5376f635cd1b284`, modern runtime SHA
`be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e`.
The fixture uses the established borrowed startup initializer on its own device,
with adapter LUID [0,98547] and GPU validation enabled. It does not access a game
process. Legacy provider SHA remains
`d4cc11bcec693dfefd6eddba6dd29bdfc16b4043e7b83150f0097d14b76512d8`.

## Motion and jitter findings

Source: accepted v1.23.3 frames 15673–15675, host version 0.0.0.0, SHA
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The captured TAA shader is unchanged from the established DLAA route: SHA
`2d7b26c742c27db2c83546c6cf6a1dfda47d7cfd4336e69f8c2302de5802b0d1`.
It decodes packed half motion into a clip-space displacement, subtracts it for
history reprojection and writes only the active dispatch rectangle. Its constant
buffer values were not captured; shader names alone do not prove their units.

Independent image reprojection compared two adjacent color/motion pairs over
three blur levels and three motion thresholds. All 18 clipped-mean fits favored
0.5 times the old allocation-sized pixel motion; 17 median fits favored 0.5 and
one favored 0.375. Eighteen synthetic direction/scale controls recovered the
known shifts exactly. This supports render-size motion for animated objects in
this capture; it is not controlled camera-pan or full-scene temporal validation.

The recorded projection math gives the expected SDK jitter conversion:
`(native_x * render_width / (2 * projection_viewport_width),
 -native_y * render_height / (2 * projection_viewport_height))`.
The scaled projection viewport must be read at the matching projection callback
before using this live. Do not blindly halve the full-resolution jitter values.
The pinned SDK guide's sections 3.6–3.7 specify render-pixel jitter and distinguish
low-resolution motion from output-resolution motion. Streamline's normalized
motion convention must not be substituted for this direct NGX pixel contract.

Each replay starts a new feature, resets history and supplies zero jitter. The
source capture injected no jitter, but did not establish the actual scaled
projection jitter. Consequently this replay proves resource transport, SDK
operation and full output coverage, not temporal reconstruction quality.

## Verification

- Build passed /W4 /WX after fixing one fixture variable-shadow warning.
- Five relevant native test targets passed, including geometry rejection cases.
- Three Performance replays passed: 19,353,600 converted input values exactly
  match active reference crops (motion multiplied by 0.5), and 33,177,600 output
  RGB values are finite and overwrite the output sentinel.
- All packed output pixels match independent R11G11B10 rounding. Subsequent
  compute and graphics draws reuse the restored bindings and match exactly.
- A fourth replay replaced every inactive raw pixel with poison/NaNs. All five
  resulting images, including the SDK and packed output, are bit-identical to
  the unmodified first replay. Padding is excluded from conversion.
- DLAA regression: one-shot success, invalid output, invalid jitter and a
  600-frame native-resolve replacement run passed. The latter used 60 output
  allocations and retired/released all 600 recordings, with no native TAA calls.
- All eight hardware fixture cases completed without GPU warnings/errors.

First output SHA:
`74cc8b38ecd5dbc6b6f5a886c1c47552e3d297ff914593bededf116d3d64df8e`.
Images show a coherent complete scene without obvious cropping or padding
contamination. The preview uses a shared illustrative HDR curve before the
game's tone mapping; it is not a final quality or FPS comparison. Fixture record
times include startup/compilation and GPU validation, not steady-state game cost.

The first fixture invocation mistakenly supplied the runtime DLL rather than its
directory. It rejected at the existing startup hash gate before SDK work. That
failed attempt and the corrected successful run are retained.

## Remaining live integration

1. Bind the scaled projection viewport/jitter to the exact input frame and keep
   the existing projection write/restore proof.
2. Connect the scaled final depth draw route to continuous input ownership.
3. Establish how the downstream game pass consumes the scaled TAA output. Native
   TAA writes only the active rectangle; replacing its entire allocation does
   not prove downstream code will consume a full-size image. Qualify the output
   routing and a full-output recovery path before installing a live upscale.

These are implementation requirements, not the parked DLAA beta-polish backlog.
Do not arm v1.23.3 expecting DLSS: it remains an input recorder. No next live
build has been prepared. Keep native AA/DLSS Off and current settings unchanged.

Evidence root: `local/scaled-input-contract-20260927/`. It contains shader
disassembly, motion analysis and synthetic controls, build/test logs, eight GPU
fixture cases, independent image review, previews, source freeze and manifest.
The original v1.23.3 capture archive is unchanged.
