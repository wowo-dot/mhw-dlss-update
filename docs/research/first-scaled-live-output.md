# First verified live full-output DLSS frame

Accepted 2026-09-28 using v1.24.5, PID 24448, process creation
134351019970206394. The modern SDK reconstructed one live 1280x720 frame to
2560x1440 in Performance mode, preset K, then the native output route consumed
the full image. This establishes one bounded live output transaction, not
continuous temporal quality or performance.

The executable remains version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
The probe DLL SHA-256 is
`5ca85334d9ce489c2975c621ab4cebacc57229aeb3cab907bc6366b8fa5e3e9c`.
Modern runtime SHA-256 is
`be6e434a94ca32499515eb62ca0e6c274526055d568d0426e4c652dcdfb6ee6e`.
DX12, adapter LUID [0,93935], swap-chain format 87, windowed 2560x1440.
Native AA, native DLSS and MotionBlur remained Off.

## Evidence

Repository-local evidence: `local/scaled-temporal-20260928/v1.24.5`.
Input log: `inputs-24448-134351019970206394-captured.jsonl`, SHA-256
`c40d81f8c6594fd40e274c79d0a636f4c761f6dc001f7d17edfcd0d0fcc7c9e3`.
Final review: `review-24448-markers-fixed.json`.

- Three input receipts, frames 14998,14999,15000. Receipt 2 owns the only SDK
  evaluation, on command list 357905456, Reset generation 2426.
- Startup capabilities, CreateFeature, Evaluate, ReleaseFeature and parameter
  destruction succeeded. Geometry is 1280x720 active input, 2560x1440 output;
  raw native allocations remain full size.
- Color, depth and motion were independently checked against the owned input
  conversions. Motion uses render-pixel units. Jitter matches the selected
  projection frame; the injected value was restored.
- The native TAA dispatch was replaced exactly once. The expanded native copy
  and three full-size motion-filter, color-grade and final-screen draws were
  recorded on the same command list and generation.
- Submission replay verified all four output markers precede Close and queue
  execution. The private GPU fence completed, the command list was retired,
  and the output effects and SDK resources were released. All 286 submission
  references were released. No runtime subsystem fault was reported.
- The saved float output is finite across all 3,686,400 pixels. Its independently
  computed R11G11B10 packing exactly matches every pixel of the saved native
  handoff image. The downstream effects' execution and GPU completion passed;
  the final swap-chain image was not read back.
- Full render scale returned, all hooks disabled, and the user reported
  "Scene normal; game closed normally". The test DLL remains installed.

`dlss-output-preview.png` and its full-resolution variant display the saved
SDK image using a diagnostic Reinhard/sRGB transform. They are before the
game's post-processing and are not screenshots or temporal-quality comparisons.
Visual inspection found a coherent full scene without obvious cropping or
missing regions.

## Reviewer correction

The original review rejected `depth marker coverage`: its shared journal
check assumed every kind-4 native-work marker belonged to a depth operation.
The full-output route adds four markers of that same kind. The runtime had
already completed successfully, saved its images and cleaned up.

`temporal_depth_routes.review_marker_coverage` now validates the full output
header and route contract before assigning those four markers to output.
Depth and output sets must be disjoint, and their union must equal every
kind-4 marker. Unknown, duplicate, overlapping or unsubmitted markers still
reject; earlier captures retain their original depth-only contract. Nine
output reviewer tests, shared reviewer regression and the previously accepted
v1.23.12 live capture passed. The original rejection is preserved alongside
the corrected review and exact reviewer source hashes. The DLL was not changed.

## Next milestone

Continuous Performance DLSS with retained SDK history and frame-aligned scaled
jitter, followed by movement, sauna/area transitions and focus checks. The
existing DLAA lifecycle work is the starting point. One reset-history output
cannot establish temporal convergence, ghosting behavior, transition stability
or FPS cost. Other quality modes, Reflex and frame generation remain later work.
