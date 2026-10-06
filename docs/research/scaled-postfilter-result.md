# Accepted post-filter construction capture

2026-09-28, v1.23.9. PID 49960 / creation 134350275210098440. Frames
24926, 24927, 24928. The frozen reviewer accepted the run. The user reported
a normal scene and normal exit; process exit was independently verified.
The current DLL remains installed. Settings, profile, mode and modlist are
unchanged, including MotionBlur Off and native AA/DLSS Off.

Evidence: `local/scaled-temporal-20260928/v1.23.9/`. Installed DLL SHA-256
`c6b22619dc59a4ae78f06608e31e3b3b6b279c191dcfda6664d5641d9d8380d1`.
Final log SHA-256
`d7b33bff22b5377ea00b192c27b58d92519a4aea2bfc2cf1fa51bd2c0e8ed2b2`.
Verified 588-file archive SHA-256
`be209cb3150617a5ce66ba8900fdf0a969d940ca921658e8baadbed0a6145fc7`.
The archive, shaders, extracted assembly and frozen sources are local artifacts.

## New observations

All three same-frame Execute callbacks at RVA 0x24ca1f0 returned through the
postqueue caller 0x24f228b. Each appended six native commands with opcodes
11, 6, 6, 6, 11, 0: three compute commands plus a pixel draw and two state
commands. The compute commands have key 805306366; the draw has key
2013266020 (stage 15, order 100). CPU construction is verified; execution of
these downstream commands on the GPU is not independently traced.

The draw uses the same 10,008-byte ps_5_0 shader on all three frames, SHA-256
`3eed9b57bbbdf38d14d9a94596fc93a107aa99eab275af7118b95febbbc1ba18`.
The shader's resource declarations and instruction stream reference color,
depth, packed motion, an additional motion/fur texture, and neighbor motion.
Its constant buffers are CBViewProjection, CBScreen and
CBMotionBlurReconstruction. There are runtime-dependent color-sampling and
blur branches. The live constant-buffer values and branch outcomes were not
captured, so MotionBlur Off still must not be described as a proven copy.
Reflection defaults are not live values.

The filter's context rectangle remained 1280x720. Its four vertices have
normalized UVs from 0 to 1. The final stage-16 triangle has the measured
half-allocation coordinates, including the existing negative half-texel
offsets: its far U/V values are approximately 0.999609 and 0.999306. The
actual 48/64-byte vertex data agrees with the statically derived native
vertex addressing formula and remained unchanged across the filter call.

TAA's copied output feeds the filter. The filter's output and final draw's
source have different engine resource pointers on all three frames:

| Role | Engine owner | Resource pointer |
| --- | ---: | ---: |
| Filter input / TAA copied output | 5368212416 | 1966366768 |
| Filter output | 1957307104 | 1966376560 |
| Final draw source | 4969952272 | 1416345904 |

All three report engine allocation dimensions 2560x1440 and engine format 34.
These are engine metadata, not a new GetDesc/COM-identity verification. The
intervening transfer from filter output to final source is still unresolved.
The assigned final endpoint entry alone does not prove that a copy occurred.

## Existing capture and shutdown

All nine raw and nine converted color/depth/motion images were saved. The
19,353,600 active-region float comparisons were exact. GPU completion and
recording retirement passed. Preview inspection shows complete color scenes,
qualitatively aligned depth and motion concentrated around the animated
character/weapon; this does not establish motion units or subpixel jitter.

Projection viewport was 2560x1440 for active render size 1280x720. The single
scoped native jitter was [0.34375, -0.48765432834625244], restored afterward;
its derived SDK render-pixel shift is [0.0859375, 0.12191358208656311]. Three
projection/frame matches passed, with 11 pre-selection scope waits and 45
depth startup waits. All color-state proofs used local transitions.

483 scale overrides were restored. All 25 hooks were disabled, all final
fault/pending counts were zero, and all pins were released: 24 input,
3 consumer, 36 depth, 327 submission, 6 final-copy. The new observer made no
mutations or COM acquisitions. Successful readbacks remained allocated until
the verified process exit. No SDK evaluation or full-output replacement ran.

## Implementation consequence

Proceed with full-size DLSS routing using this captured shader/geometry.
Account for the downstream shader's different color and depth/motion sampling
coordinates and establish the final-source transfer. Enlarging only the TAA
copy or treating MotionBlur Off as proof of a removable pass is insufficient.
The existing native temporal inputs remain the integration point. Reflex and
FG/MFG follow DLSS; DLAA beta polish and CMFM menu work remain parked.
