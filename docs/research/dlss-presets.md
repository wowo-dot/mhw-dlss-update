# DLSS presets (v0.17.0, 2026-10-12)

The roadmap's "DLSS ready to ship" step begins with the quality presets:
DLAA, Quality, Balanced and Performance. Ultra Performance is left out by the
user's call. The game's render-scale setter only goes down to 50%, so it would
need a fight with the game for little gain.

Step 1 (this version) picks the preset at start: the ini's `dlss_mode`, or the
Mods menu, where the change is marked "Restart needed". Step 2 will rebuild
the pipeline at a new render size while the game runs. That gives live preset
changes and fixes the live 16:9 / 21:9 stop.

## Presets

| Preset | Scale asked of the game | At 2560x1440 (quantized by the game) | NGX quality value, preset hint |
|---|---|---|---|
| DLAA | 1.0 | 2560x1440 | DLAA, `Hint_Render_Preset_DLAA` |
| Quality | 2/3 | 1696x954 | MaxQuality, `..._Quality` |
| Balanced | 0.58 | 1472x828 | Balanced, `..._Balanced` |
| Performance | 0.5 | 1280x720 | MaxPerf, `..._Performance` |

- Preset K (transformer) is used for every mode, as before.
- The game's setter quantizes the scale to 16-pixel steps of the basis width
  (`render_resolution::plan`). DLSS gets that extent, never a resized one.
- A DLAA basis width that is not a multiple of 16 quantizes just below 100%.
  DLSS then runs that extent as Quality.

## What changed

- **`dlss_sdk::single_scale`.** This is the one rule for every render extent:
  one scale in [0.5, 1] for both axes, truncated per axis.
  - Replaced the "identity or exact half" gates in three places:
    - the DLSS geometry contract;
    - the shader adapters' gate (`valid_filter_geometry`, which covers the
      filter, haze, depth of field, depth colour, effects, screen-shader and
      silhouette adapters);
    - the spatial fallback (`temporal_present::Output`).
  - The adaptations already used the actual ratio render/output.
- **`render_supported`** replaces `optimal_matches`. The render extent must
  lie in the mode's reported range (the NGX minimum and maximum), not within
  one pixel of the optimal extent. Example: Quality at 2560 renders 1696 wide,
  where NGX's optimal is 1708.
- **`continuous_dlaa::reconstruction_mode`** is set by the worker from the
  config before preparing. It replaces the three hard-coded `.5F` scales. The
  scaled resolve no longer insists on Performance.
- **Ini, menu and status.**
  - The ini key is `dlss_mode = dlaa | quality | balanced | performance`
    (default Performance). The log's `alpha_config` event records it.
  - The menu's Settings page has an "Upscaling" section with "DLSS mode"
    (CHOICE, RESTART) above the "Frame generation" section.
  - Status reads "Quality, 1696x954 to 2560x1440" or "DLAA, 2560x1440".

## Checked offline

- **Tests:** 668 of 668 pass, plus 182 of 182 code gates on the clean image.
  New tests:
  - the presets' geometries and range checks (`dlss_sdk_contract`);
  - the plan of every preset on nine common displays and on 21:9 pillarboxed
    in a 32:9 window is a single scale (`render_resolution_tests`);
  - the Quality and Balanced extents in the effect-vertex tests;
  - the ini key, menu model and status lines (`alpha_menu_tests`,
    `alpha_config_tests`).
- **Bug caught by the new test:** a first version of `single_scale` accepted
  touching intervals. That admitted 2560x1439 of 2560x1440, which no single
  scale can produce.

## game-61 (v0.17.0): the first Quality run

**Outline at about 2/3 size and position (confirmed cause).** Every
silhouette draw was rejected with reason 6 (160k). The check compared CBScreen
`[16..17]`, which is `fViewInverseSize` (1 / render), with 2 / output. At
Performance both are 1/1280, so the check had only ever passed by coincidence.
**v0.17.1** compares it with 1 / render. The log now records all four inverse
sizes when the check fails.

**DLSS and frame generation stop after a cutscene (an older bug, not caused
by the presets).**
- **What happened:**
  - Two same-size `resize_buffers` calls came around the cutscene.
  - From then on every plan was rejected: plan rejection 17 (an overlay
    failure), command-gate branch 10 (`input_bindings`), qualification 14,
    which is `dof_qualify`.
  - Readiness then stayed `unbuilt`, DLSS skipped every frame (skip 7), and
    frame generation had no ready frames.
- **It also happened at Performance:** two game-55 runs (v0.15.0) show the
  same rejection, 1,143 and 498 times.
- **Why the depth of field fails is still unknown.** The summary did not
  include the DOF diagnostics. v0.17.1 adds `route.dof`: the failing check,
  the failed pass, and the first failure's misses.

**DLAA would not have started.** The stream route's `prepare` refused identity
extents. v0.17.1 lets it run at identity; every adaptation then uses scale 1.

## game-62 (v0.17.1): Quality again

- **Outline: fixed (the user confirmed).** 115,506 draws were corrected. Only
  the eye and `Simple_Mt_AlphaTest` draws stay native.
- **NGX's range for Quality at 2560x1440:** optimal 1707x960, minimum
  1280x720, maximum 2560x1440. The game's 1696x954 lies inside it.
- **The depth-of-field cause (confirmed by `route.dof`).** Every cutscene DOF
  frame failed check 3. CBScreen's iViewSize, fViewSize and fViewInverseSize
  were 1696x954, while the check expected width/2 (1280x720).
  - The DOF texture layout matched: it is half the output allocation,
    independent of the render scale.
  - DLSS and frame generation resumed after the cutscene. All 1,100 rejected
    frames came during it.
- **Deeper finding.** The adapted DOF passes 0 and 3 average a 2x2 box of
  output colour per render texel, so they are exact only at 2:1.
- **v0.17.2:**
  - The view constants are checked against the render size.
  - At identity (DLAA) no pass is adapted: the native DOF already reads full
    size.
  - Other scales (Quality, Balanced) are rejected explicitly (96). DLSS skips
    frames while DOF is active and resumes afterwards.
  - A resampling DOF adaptation for those scales is open work.

## game-63 (v0.17.2): DLAA never armed

- **What happened:** the log stopped at `temporal_coexistence` "prepared"
  with no `alpha_arm`. The worker waited for the game to qualify for the
  whole session, so there was no DLSS and no frame generation.
- **Cause:** `render_resolution::ready` and `install` refused scale 1.0
  ("reduced extent plan"). They were written only to reduce the extent.
  The runtime still needs the module at 1.0: its per-frame input stamps prove
  which extent each frame rendered.
- **v0.17.3:**
  - Scale 1.0 is allowed. The override then holds the game's own full scale.
  - The wait loop logs `alpha_waiting` with its current reason after 15 s,
    so a game that never qualifies no longer leaves a silent log.

## Not yet proven (hypotheses until the in-game runs)

- The shader adapters were verified on the GPU at 2:1 and at identity (and at
  a fractional 2:3 in `temporal_spatial_fractional_warp`). Real game shaders
  at Quality and Balanced ratios run for the first time in game.
- NGX's reported minimum and maximum for Quality and Balanced have not been
  seen yet. The SDK receipt in the log records the optimal, minimum and
  maximum. If the game's extent falls outside them, DLSS fails closed, with
  the reason "inline SDK render extent outside the mode's range".
