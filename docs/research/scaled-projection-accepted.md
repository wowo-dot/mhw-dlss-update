# Scaled projection and input capture accepted

2026-09-28, v1.23.6, PID 48312 / creation 134350225380327709. The three adjacent
frames 13986–13988 passed the frozen capture reviewer. Native AA and DLSS stayed
Off in saved settings. This run recorded inputs and projection; no NGX evaluation
or displayed DLSS replacement occurred.

## Measured projection mapping

All three frames rendered an active 1280x720 rectangle within 2560x1440 resource
allocations. Their actual projection denominator stayed 2560x1440. The middle
frame used native jitter `[0.34375, -0.48765432834625244]`; its two neighboring
frames were unjittered. Captured jittered/unjittered matrices and matrix products
passed independent arithmetic checks, with exact projection/TAA frame, epoch,
bank, async mode, context and view association.

The existing geometry conversion therefore gives render-pixel SDK jitter
`[0.0859375, 0.12191358208656311]` for this middle frame:

```
sdk_x = native_x * render_width  / (2 * projection_viewport_width)
sdk_y = -native_y * render_height / (2 * projection_viewport_height)
```

This is a measured projection basis plus a matrix-derived raster shift. The
capture reviewer does not claim an independent subpixel color-image fit,
motion-vector jitter-inclusion proof or continuous DLSS image quality. The
implementation must use the measured denominator rather than assume that it
equals the active render extent.

## Inputs, state and lifetime

Nine raw and nine converted color/depth/motion images were saved. GPU submission,
completion and recording retirement passed, as did all 19,353,600 active-region
float comparisons with zero conversion error. The active images show a coherent
full scene, corresponding depth silhouettes and motion around animated objects.
The preview is qualitative and applies display tone mapping/percentile contrast.

The native-current-use color-state proof was exercised and accepted on frame
13986; the following two frames used observed local transitions. This closes the
previous gap where that alternative proof had only offline coverage. Exact
packet/resource identity and generation checks passed the unchanged reviewer.

The startup protections worked: seven unowned projections and 21 pre-baseline
depth requests were skipped. The depth gate opened on the exact baseline stamp.
All three projection receipts were admitted inside the owned activation frame.
One jitter write was restored once. Full scale was restored after 356 overrides.

Final faults and pending callbacks were zero, all hooks were disabled, and all
24 input, 3 native-consumer, 36 depth and 261 journal pins were released.
Successful readback allocations retained by diagnostic policy ended with process
exit. User confirmed normal scene and normal exit; process exit, unchanged
graphics/profile/mod list/mode hashes and the preserved installed DLL were
verified. Keep v1.23.6 installed until another prepared test replaces it.

## Evidence and next stage

Root: `local/scaled-temporal-20260928/v1.23.6/`. DLL SHA
`6d69db8c7a79e7f54c2dc87de842c56a0ff514cb6c4550fdbc4269a828a66156`.
Final log SHA
`740ed4b56363350a43f8fe1b44c30f64343022b1b7f5a8fc0ae01ada25c3243d`.
541-file final archive SHA
`c70cbe40938f74e09f4818808073b30b7cfdc6d7822455a49510b5bcb083a671`.
Frozen source, arm/reviewer tools, per-image hashes, complete review, inspection
preview and normal-exit verification are included.

Next implementation focus is the full-resolution output handoff: qualify the
actual live destination and rectangle of the native TAA final copy, then connect
the already working offscreen modern DLSS output to that path. Static tracing
found that native handoff can copy only the active rectangle; a full-sized output
allocation alone is insufficient. See the
[output routing map](scaled-projection-capture.md) and
[offscreen SDK result](first-scaled-sdk-output.md). Continuous scaled resource
lifetime/recovery and motion/camera-history checks remain integration work;
the previously parked DLAA beta-polish work stays parked.
