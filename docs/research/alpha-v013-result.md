# Config alpha: v0.13.0 to v0.13.4

Runs game-38 to game-42, 2026-10-08. Windowed 2560x1440 at 16:9, DLSS
Performance (1280x720), FG 2x, Reflex On. RTX 5090; display 240 Hz.

## The alpha build

`mhw_dlss_alpha` is the FG game build with `MHW_ALPHA`:

- **Config:** `mhw_dlss.ini` next to the DLL (`src/alpha/config.hpp`):
  `frame_generation` (off, 2x, 3x, 4x, 6x), `reflex` (on), `runtime_dir`,
  `log_dir`. The DLSS-G plugin loads only with FG enabled.
- **Arming:** the session arms itself once the swap chain has been unchanged
  for 3 s and the continuous runtime qualifies (`temporal::ready`). In every run
  it armed about 5.4 s after launch, on the first attempt.
- **Lean paths:** all perf switches on, Streamline at its default log level, no
  arm markers or session time limit.
- **Logging:** every 60 s the FG status, the frame and latency windows, and a
  `dlss_summary` line. The summary holds DLSS history resets by reason, skip
  reasons, plan rejections, dependency draws per kind, and the latest rejected
  plan graph when new rejections occurred.

## Findings

**Crash on a Velkhana attack (game-38, fixed in v0.13.1).**
- **The fault:** an access violation in our DLL, found through the Windows
  event log and the map file. The readiness trace's rejection snapshot read
  the effect draws of an unbuilt plan.
- **The cause:** a build that failed after counting haze and overlays had no
  stage order. The 4-entry haze stage table was then indexed past its end.
- **The fix:**
  - The snapshot reads built plans only.
  - Unordered stages come from `effect_order.hpp`.
  - A crash anywhere, including this DLL, now writes `.crash.jsonl` through an
    unhandled-exception filter.

**FG drops on the windrake ride (fixed in v0.13.1).**
- **The cause:** DLSS history reset on any change to a projection bit. In
  earlier runs nearly every reset was a 0.003% change to m00/m11, i.e. FOV
  animation, and DLSS-G presents no generated frame on a reset.
- **The fix:** a projection change is now a cut only when an element appears,
  disappears, changes sign, is not finite or moves by more than 10% in one
  frame.
- **Result (game-39):** 48,212 frames kept history that the old rule would
  have reset, and about 100 resets remained in 30 minutes. The user saw FG
  hold on the windrake ride.

**DLSS skips in the Fatalis fight smoke (game-39 to game-42, fixed in v0.13.4).**
- **game-40:** v0.13.2's summary showed 1,939 plan rejections, all with graph
  detail 3. `uDepthColorFilter` (vtable RVA `0x3423850`, phase 15, order
  `0x50`) sat between TAA and the motion unit, which then read the filter's
  output instead of the TAA copy.
- **game-41:** v0.13.3 captured the filter's draw near the smoke. The TCS
  charge alone did not trigger it in the training area.
  - **Geometry:** one screen triangle with haze's native vertices and the
    motion VS.
  - **What the pixel shader does:**
    1. Samples the scene at the native UV.
    2. Loads depth at `SV_Position`.
    3. Converts depth to view distance through `fProjI`.
    4. Tints toward `fDepthColorColor` with a 256-entry distance curve, in one
       of four blend modes.
  - **Bindings:** s0, t0 (scene), t1 (R32 depth), and constant buffers b0
    (CBViewProjection), b1 (CBScreen) and b3 (CBDepthColor).
- **game-42 (v0.13.4):** 2,194 depth-colour draws at full output in the smoke,
  with no plan rejections and no fault. The user: "looked correct".

## The depth-colour adapter (v0.13.4)

- **Shader** (`depth_color_shader.cpp`): a hash-gated DXBC edit.
  - The scene sample's UV expands to the full output.
  - The depth load and the view ray use the raster position on the render
    lattice, so depth, CBScreen and the projection stay in render space.
- **Fixture** (`tools/scaled_depth_color_fixture.cpp`, private shaders by path):
  - **The test:** the adapted shader is fed what the game binds after DLSS. An
    oracle runs the unmodified native shader at full output with consistent
    inputs: color padded into a double-size allocation, 2x2 nearest depth, and
    CBScreen for the full output.
  - **Result:** the two agree bit for bit in all five blend modes, on WARP and
    the RTX 5090 with GPU-based validation: 204,800 comparisons, and all 13
    altered or invalid inputs were refused.
- **Route:**
  - **Filter chain:** the filter is admitted between the TAA copy and the
    motion unit, and the motion unit must read its output.
  - **The draw** takes an overlay slot as dependency kind 4:
    - It must use the exact binding words, and t0 must be the TAA copy.
    - t1 must be native R32 depth (resource role 9).
    - Its output is pinned and state-tracked.
    - It must come first in native order.
  - **Admission:** `effect_admitted()` checks the pipeline state when the plan
    is built, so an unexpected recipe stays native instead of faulting the
    route.

## Next

The agreed order:

1. **Shader census:** dump every post-filter shader the game loads, and sort
   each by how it addresses the screen (UV, pixel position, compute,
   multi-pass).
2. **Generated adapters:** generate hash-gated adapters per family, each
   verified by a fixture like the one above.
3. **Flexible chain:** admit an approved filter at any chain position.

Suspects from the registration audit (`postprocess-family-audit.md`):

- `uColorCorrectCube`
- `uBlinkFilter`
- `uImagePlaneFilter`
- `uCircularDOFFilter`
- `uMotionBlurFilter`
- `uTemporalAAFilter3`
- the Safi and Alatreon radial-blur variants
