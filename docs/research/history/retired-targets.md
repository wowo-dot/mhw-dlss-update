# Retired research targets (2026-10-17, 0.29.0 cleanup)

## What happened

The shipped mod grew out of a long series of research probes: one-purpose DLLs
loaded into the game, capture recorders, offline fixtures, and the Python
tools that armed and reviewed them. On 2026-10-17 everything the shipped mod
and its tests do not use left `main` (the user's go: "as long as we don't
block our ability to fix the bug reports from the logs they provide").

## Getting them back

Nothing is lost:
- The tag **`research-final`** (commit `27dd888`, 0.29.0-dev.2) is the last
  commit where every research target builds and every test passes: 188
  targets, 672 tests, 205 code gates.
- To rebuild a probe, check out that tag in its own worktree and configure it
  with the same two SDK roots as today (README, "Building"):

  ```powershell
  git worktree add ../dlssupdate-research research-final
  cmake -S ../dlssupdate-research -B ../dlssupdate-research/build -A x64 -DMHW_NGX_SDK_ROOT=<dlss sdk> -DMHW_STREAMLINE_SDK_ROOT=<streamline sdk>
  ```
- `docs/research/` and `docs/runtime/` keep every note, and the git history
  keeps every file.

## What stayed

- **The shipped DLL** `mhw_dlss_alpha` and its 13 libraries.
- **84 C++ tests**: settings and menu, the DLSS runtime (42 cases), activation,
  render resolution, game inputs and depth, the full-size post chain,
  binding restore and the observer, frame generation, Reflex and
  presentation, diagnostics.
- **One Python suite**, as a CTest test (`python_tools`).
- **The tools that read players' logs and build releases:** `fg_game.py`,
  `reflex_frames.py`, `review_reflex_presentation.py`, `profile_report.py`,
  `perf/`, `fg_runtime.py`, `package_release.py`, plus the RenderDoc scripts in
  `tools/rdc/`.
- **Six hand-run fixtures:**
  - `silhouette_shader_fixture`, `observer_bench`;
  - `frame_handoff_gpu`, `depth_consumer_gpu`;
  - `mhw_reflex_sdk_fixture`, `mhw_reflex_startup_fixture`.

## What left

**Files:** 478 (156 under `src/`, 173 under `tests/`, 149 under `tools/`):
- the C++ sources of the targets below;
- 122 Python tools and 87 Python tests;
- the `src/` folders `aa_control`, `exception_capture`, `jitter_activation`,
  `jitter_callbacks`, `jitter_writer`, `probe`, `trace` and `unlock`;
- `cmake/ReflexBoundaries.cmake`.

`cmake/NgxReplay.cmake` and `cmake/StreamlineReflex.cmake` became
`NvidiaDlssSdk.cmake` and `NvidiaStreamline.cmake`, which keep only the
pinned SDK checks and the one library each that the mod links.

**Probe DLLs (36):**
- `mhw_aa_boundary_probe`, `mhw_aa_deferred_probe`, `mhw_aa_gpu_probe`,
  `mhw_aa_inputs_probe`, `mhw_aa_owned_probe`, `mhw_aa_projection_probe`,
  `mhw_aa_sdk_probe`, `mhw_aa_stream`
- `mhw_continuous_dlaa`, `mhw_continuous_dlss`
- `mhw_dlss_bridge_probe`, `mhw_dlss_probe`, `mhw_dlss_unlock`
- `mhw_fg_game_probe`, `mhw_frame_generation_ownership_probe`,
  `mhw_frame_generation_scene_probe`
- `mhw_jitter_activation_probe`, `mhw_jitter_callback_probe`,
  `mhw_jitter_writer_probe`
- `mhw_ngx_startup_probe`, `mhw_projection_binding_probe`
- `mhw_reflex_boundary_probe`, `mhw_reflex_dlaa_probe`,
  `mhw_reflex_dlss_probe`, `mhw_reflex_frame_probe`, `mhw_reflex_input_probe`,
  `mhw_reflex_presentation_probe`, `mhw_reflex_startup_probe`
- `mhw_render_resolution_probe`, `mhw_scaled_output_probe`,
  `mhw_scaled_temporal_probe`
- `mhw_temporal_dlss_probe`, `mhw_temporal_inline_probe`,
  `mhw_temporal_inputs_probe`, `mhw_temporal_state_probe`
- `probe_mock_feature`

**Libraries (7):** `mhw_aa_sdk_pass`, `mhw_aa_stream_pass`,
`mhw_depth_retry_fixture`, `mhw_dlss_bridge_policy`, `mhw_fg_temporal_state`,
`mhw_reflex_presentation`, `mhw_temporal_gpu_inputs`.

**Test executables (60), about 590 CTest tests:**
- the AA-control probes: `aa_binding_readback`, `aa_gpu_state`,
  `aa_gpu_trace`, `aa_input_readback`, `aa_policy`, `aa_projection`,
  `aa_stream_lifetime`, `aa_stream_runtime`, `owned_pass`, `owned_queue_scope`,
  `post_queue`, `post_queue_abi`, `deferred_contract`;
- the bridge-probe policies: `barrier_observer`, `bridge_policy`,
  `capability_snapshot`, `capture_mailbox`, `checkpoint_review`,
  `contract_snapshot`, `depth_selection`, `depth_tracker`, `depth_work`,
  `evaluate_gate`, `handle_lifetime`, `jitter_sequence`, `native_taa_fallback`,
  `optimal_settings`, `renderer_snapshot`, `resource_inspection`,
  `scene_trigger`, `submission_completion`, `temporal_snapshot`;
- the projection capture machinery: `projection_binding`,
  `projection_depth_readback`, `projection_provenance` (175 tests; they test
  research code, not the helpers the mod uses);
- the jitter experiments: `jitter_activation`, `jitter_callbacks`,
  `jitter_writer`;
- the per-effect route from before v0.26.0: `scaled_downstream`,
  `scaled_draw_execution`, `scaled_handoff`, `scaled_postfilter`,
  `scaled_temporal_fixture`, `scaled_effect_vertices`, `overlay_vertex`,
  `screen_filters`, `mhw_screen_shader_fixture`;
- the temporal probes: `temporal_probe`, `temporal_long_probe`,
  `temporal_state_probe`, `temporal_jitter`, `temporal_long_jitter`,
  `temporal_gpu_contract`, `mhw_temporal_input_replay`;
- the rest: `dlss_sdk_contract`, `frame_generation_scene_trace`,
  `reflex_presentation_ownership`, `mhw_depth_exception_capture`,
  `probe_mock_host`, `unlock_policy`.

**Fixtures and tools (25):**
- the FG and Reflex hardware fixtures, which drive presentation without Astera
  Loader: `mhw_frame_generation_{direct,off,on,switch}_fixture` and
  `mhw_reflex_{frame,ownership,presentation,temporal}_fixture`;
- the NGX and DLAA fixtures: `ngx_startup_fixture`, `mhw_dlaa_inline_fixture`,
  `mhw_dlaa_live_fixture`, `mhw_dlaa_replay`, `mhw_ngx_capability_probe`;
- the AA fixtures: `aa_sdk_fixture`, `aa_stream_fixture`;
- the effect fixtures: `mhw_scaled_{depth_color,effect,filter,haze}_fixture`,
  `scaled_dof_fixture`, `dof_pipeline_fixture`, `dof_prepare_scale_fixture`;
- the route fixtures built on the effects harness:
  `scaled_output_route_fixture`, `scaled_runtime_route_fixture`,
  `scaled_stream_route_fixture`.

## From the shipped libraries

**Taken out:** the per-effect shader adapters (`effects.cpp`,
`overlay_extent.cpp`, the haze, radial, depth-of-field, depth-colour and
screen-filter shaders). The DLL had not called them since v0.26.0.

**Then from the shipped files:** the research-only code paths, which were
compiled in but never run by the mod, each with its own commit:
- the projection recorder's capture machinery and its buffers;
- the research `#if` branches, resolved to their shipped values, with the
  FG scene probe (`scene_probe.inl`, `scene_trace.hpp`) they guarded;
- the MinHook paths for running without Astera Loader: MinHook is now only
  in the tests, behind a stand-in loader (`tests/fake_astera.cpp`).
