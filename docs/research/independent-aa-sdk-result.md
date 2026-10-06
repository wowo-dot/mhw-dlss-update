# Independent AA-Off private DLAA v0.7.0 result

2026-09-26, PID 56588 / creation 134349264954825425. **Capture accepted:**
eight consecutive real DLAA evaluations at the independent AA-Off input read
point, with history retained after the initial reset. All 24 captured images
passed the frozen reviewer. Native TAA, FXAA and DLSS remained Off. This verifies
private DLAA computation; this build did not write the result into native color.

The user exited normally. The exact previous diagnostic DLL was restored at
23:04:11.834053 +03, its matching output-directory sidecar removed, and all four
configuration hashes remained unchanged. No next build is installed or armed.

## Confirmed observations

Source frames 16038-16045, Windowed 2560x1440, DX12, device 216074608,
adapter LUID 98931:0, swap-chain format 87. Host version 0.0.0.0, SHA-256
`c2ebbbd2c49f216d484e31a5219bed419eb1e5e7d206d02cba040a3ab79d90ea`.
Test DLL SHA-256
`5a1c00ae61f1af11db006d1f31b76cff2675c70796598b87ff663eba3b9ab2b5`.
Hook targets, ABI, resource ownership and code gates are documented in the
[runtime contract](../runtime/independent-aa-sdk.md) and its linked contracts.

- The separately reviewed early startup transaction preserved native
  initialization and modern capabilities. Its exact process, build and device
  matched the capture; the startup hook was disabled before capture readiness.
- Eight owned callbacks restored the native queue exactly. All eight source
  projection/request/GPU associations and native binding snapshots passed.
  The sampled native TAA and FXAA flags were zero; native parameters were
  unchanged. Six nonzero projection stimuli were restored; the first and last
  frames were zero-jitter controls.
- The SDK received the matching raster-jitter convention `(rawX/2, -rawY/2)` on
  each frame. One feature evaluated all eight frames successfully, with one
  initial history reset. Each reuse required observed completion and Reset
  retirement of the preceding selected recording.
- Color, depth and motion were in PS-SRV state 128 at every read point. Native
  copies restored that state. Private conversion, SDK work and readback were
  followed by exact native binding restoration, checked against the saved
  mirror. Native output was not written.
- All eight private fence values completed and all eight selected recordings
  retired. Completion collection closed before hook removal. The journal closed
  at 3,213 events and released all 494 pins. No fault or pending work remained;
  all capture hooks were disabled and drained, descriptors detached, jitter
  restored, and retained input/readback references released. SDK feature release
  and parameter destruction succeeded after completion proof.

## Image verification and limits

Receipts 0, 3 and 7 each contain three native input images and five private
converted/DLAA/packed images: 24 images total. The reviewer verified exact input
conversion for 11,059,200 pixels and exact packing of the same number of DLAA
output pixels. SDK RGB was finite and clear-sentinel-free, with values changed
from the input. These are actual SDK results, not an unchanged-input copy.

The paired preview shows a coherent matching scene for all three sampled
input/output pairs. It is a visual sanity check, not a temporal-quality verdict.
The frozen review explicitly accepts `independent_dlaa_private_output_verified`
while keeping presentation, temporal quality, measured color-jitter response,
pixel freshness and cross-queue input-writer proof unverified. Frame association
and the projection-derived SDK jitter mapping do not erase those limits.

The eight-frame test uses fixed 16.666666ms frame time and bounded waits before
history reuse. Those are diagnostic constraints, not the intended continuous
rendering design.

## Next integration

Feed the verified packed DLAA output into native color at this same independent
AA-Off read point. Respect its observed PS-SRV state: the historical TAA-entry
handoff assumes UAV and cannot be applied unchanged. Qualify the output copy,
restore the native state and bindings, and verify downstream consumption. On
failure before handoff, preserve the original unfiltered color and latch the
feature off without silently enabling native AA or retrying.

Continuous lifetime/history handling, other-AA suppression and mode switching
remain integration work. Do not reactivate the historical TAA-dependent route
as the independent implementation.

## Evidence

Ignored session: `local/independent-aa-sdk-20260926/v0.7.0/`.
361 frozen source files and 445 prelaunch files. Prelaunch SHA-256
`866a75f8de62beb513a19c51960d04df552310b0c6c90d7efbbf15d47aceaf7e`.
Before deployment: /W4 /WX build, 48 native checks, 46 AA Python checks and 15
startup checks passed. Seven real NVIDIA SDK fixtures passed GPU validation
with zero errors or warnings, including full-resolution success and five
failure cases. Both successful fixtures passed exact conversion/packing checks.

Capture `capture-56588.jsonl`: 860,981 bytes, SHA-256
`f18e4a59c85fdb3066980c286f48b915e4b009c63daa0a435f2bd3da78560ac7`.
Startup receipt SHA-256
`deb262055c2d2c5c0888eea746ac88530f8379fc3e2d3a30eb3606d680b5b326`.
`review-56588.json` records full scoped acceptance. Additional image inspection
is saved in `visual-review-56588.json` and `private-dlaa-preview.png`.
`restored.json` records rollback and unchanged configuration hashes. Restored
DLL SHA-256
`689bef7cf00ff71e4518f452f0467edf6c3d877f2a6dd090553388d8b5f288cc`.
No native source, tests or tools changed during capture or closure.
