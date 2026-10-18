cmake_minimum_required(VERSION 3.24)

project(mhw_dlss_update VERSION 0.1.0 LANGUAGES CXX C)

if(NOT WIN32)
    message(FATAL_ERROR "MHW DLSS targets 64-bit Windows only")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Configure a 64-bit build")
endif()
# The hook entry stubs are MASM (src/*/*_entry.asm).
enable_language(ASM_MASM)
# Six of them share the frame macros of src/common/native_frame.inc.
set_source_files_properties(src/projection_binding/entry.asm src/reflex/frame_boundaries_entry.asm
    src/scaled_temporal/stream_entry.asm src/temporal_bindings/depth_route_entry.asm
    src/temporal_bindings/jitter_entry.asm src/temporal_bindings/temporal_entry.asm
    PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/src/common")

# The shipped mod (mhw_dlss.dll in the release package) and the tests of its
# code. The research probes, fixtures and tools it grew out of were retired on
# 2026-10-17; the tag research-final builds every one of them
# (docs/research/history/retired-targets.md).

# --- Build settings ---------------------------------------------------------

# The project's warning level and language settings for C++ targets. New
# warnings in project code are defects (AGENTS.md).
function(mhw_cxx target)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/external:W0>)
    set_target_properties(${target} PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

# A library linked into the shipped DLL: also Control Flow Guard.
function(mhw_library target)
    mhw_cxx(${target})
    target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/guard:cf>)
endfunction()

# --- Dependencies -------------------------------------------------------------

# The two public NVIDIA SDKs, local and pinned by SHA-256 (README, "Building").
include(cmake/NvidiaDlssSdk.cmake)
include(cmake/NvidiaStreamline.cmake)
include(cmake/DxbcHash.cmake)

# --- The shipped mod's libraries ----------------------------------------------

# Astera Loader's al.d3d12 chains and al.hooks (src/astera).
add_library(mhw_astera_hooks STATIC src/astera/astera_hooks.cpp)
mhw_library(mhw_astera_hooks)
target_compile_features(mhw_astera_hooks PUBLIC cxx_std_20)

# Binding restore, the spatial recovery pass and unfiltered copies.
add_library(mhw_temporal_present STATIC src/temporal_present/output.cpp src/temporal_present/bindings.cpp src/temporal_present/unfiltered.cpp)
mhw_library(mhw_temporal_present)
target_compile_features(mhw_temporal_present PUBLIC cxx_std_20)
target_compile_definitions(mhw_temporal_present PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(mhw_temporal_present PUBLIC d3d12 dxgi dxguid d3dcompiler)

# The command-list observer: each list's bindings, raster state and predicate.
add_library(mhw_temporal_state STATIC src/temporal_present/observer.cpp)
mhw_library(mhw_temporal_state)
target_compile_features(mhw_temporal_state PUBLIC cxx_std_20)
target_link_libraries(mhw_temporal_state PUBLIC mhw_temporal_present mhw_astera_hooks)

# Shader adapters for the full-size post chain: geometry checks, silhouettes,
# the screen-position rewriter.
add_library(mhw_scaled_filter_shader STATIC src/scaled_temporal/filter_shader.cpp src/scaled_temporal/silhouette_shader.cpp
    src/scaled_temporal/screen_shader.cpp)
mhw_library(mhw_scaled_filter_shader)
target_compile_features(mhw_scaled_filter_shader PUBLIC cxx_std_20)
target_compile_definitions(mhw_scaled_filter_shader PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(mhw_scaled_filter_shader PRIVATE mhw_dxbc_hash bcrypt)

# The full-size post chain's own GPU work: upscaled inputs, silhouette draws.
add_library(mhw_scaled_effects STATIC src/scaled_temporal/silhouette_draw.cpp src/scaled_temporal/full_post_inputs.cpp)
mhw_library(mhw_scaled_effects)
target_compile_features(mhw_scaled_effects PUBLIC cxx_std_20)
target_compile_definitions(mhw_scaled_effects PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(mhw_scaled_effects PUBLIC mhw_scaled_filter_shader d3d12 dxgi dxguid bcrypt d3dcompiler)

# DLSS evaluation on the game's TAA pass.
add_library(mhw_dlss_inline STATIC src/dlss_sdk/inline.cpp)
mhw_library(mhw_dlss_inline)
target_include_directories(mhw_dlss_inline SYSTEM PRIVATE "${MHW_NGX_SDK_ROOT}/include")
target_compile_definitions(mhw_dlss_inline PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(mhw_dlss_inline PUBLIC mhw_temporal_present mhw_astera_hooks "${MHW_NGX_LIBRARY}" d3d12 dxgi dxguid)
add_library(mhw_scaled_resolve STATIC src/scaled_temporal/resolve.cpp)
mhw_library(mhw_scaled_resolve)
target_link_libraries(mhw_scaled_resolve PUBLIC mhw_dlss_inline)

# NGX start-up inside the game's own NGX init; the game's DLSS 1 declined.
add_library(mhw_ngx_startup STATIC src/ngx_startup/startup.cpp src/ngx_startup/worker.cpp src/projection_binding/identity.cpp
    src/game_memory/bounded_scalar_read.cpp)
mhw_library(mhw_ngx_startup)
target_compile_features(mhw_ngx_startup PUBLIC cxx_std_20)
target_include_directories(mhw_ngx_startup PRIVATE src/game_memory)
target_include_directories(mhw_ngx_startup SYSTEM PRIVATE "${MHW_NGX_SDK_ROOT}/include")
target_compile_definitions(mhw_ngx_startup PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_ngx_startup PUBLIC mhw_astera_hooks bcrypt d3d12 dxgi dxguid "${MHW_NGX_LIBRARY}")

# Presentation through Streamline: the identity chain, the live FG switch.
add_library(mhw_fg_reflex_presentation STATIC src/reflex/presentation.cpp)
mhw_library(mhw_fg_reflex_presentation)
target_link_libraries(mhw_fg_reflex_presentation PUBLIC mhw_streamline_reflex d3d12 dxgi dxguid mhw_astera_hooks)

# --- The shipped mod ------------------------------------------------------------

# mhw_dlss.dll in the release package (tools/package_release.py), an Astera
# Loader plugin: DLSS/DLAA, frame generation and Reflex, configured by
# mhw_dlss.ini and the optional CMFM pages (docs/architecture.md).
# The DLSS runtime (continuous_dlaa/runtime_state.hpp lists the parts).
set(continuous_runtime_sources src/continuous_dlaa/runtime.cpp src/continuous_dlaa/runtime_projection.cpp
    src/continuous_dlaa/runtime_requests.cpp src/continuous_dlaa/runtime_lists.cpp src/continuous_dlaa/runtime_resolve.cpp
    src/continuous_dlaa/runtime_native.cpp src/continuous_dlaa/runtime_modes.cpp src/continuous_dlaa/runtime_log.cpp)
add_library(mhw_dlss_alpha SHARED
    # Continuous DLSS on the game's TAA pass.
    ${continuous_runtime_sources}
    src/continuous_dlaa/activation.cpp src/continuous_dlaa/activation_entry.asm
    src/temporal_bindings/inputs.cpp src/temporal_bindings/shader.cpp src/temporal_bindings/depth_route_contract.cpp
    src/temporal_bindings/temporal_entry.asm src/temporal_bindings/depth_route_entry.asm src/temporal_bindings/jitter_entry.asm
    src/projection_binding/memory.cpp src/projection_binding/policy.cpp src/projection_binding/attachments.cpp
    src/projection_binding/contract.cpp src/projection_binding/stimulus.cpp
    src/projection_binding/identity.cpp src/projection_binding/discovery.cpp src/projection_binding/entry.asm src/game_memory/projection_snapshot.cpp src/game_memory/bounded_scalar_read.cpp
    src/scaled_temporal/stream_route.cpp src/scaled_temporal/stream_entry.asm
    src/render_resolution/runtime.cpp src/render_resolution/resolution_entry.asm
    # The plugin: Astera Loader's entry, the game's device, the worker and
    # its session (src/plugin/plugin.hpp).
    src/plugin/plugin.cpp src/plugin/device.cpp src/plugin/worker.cpp src/plugin/reflex_control.cpp
    src/plugin/generation.cpp src/plugin/dlss_control.cpp src/plugin/log_events.cpp
    # Reflex frames and the presentation's temporal side.
    src/reflex/live_frames.cpp
    src/reflex/frame_boundaries_entry.asm src/reflex/temporal_coexistence.cpp
    # Frame generation.
    src/frame_generation/game_session.cpp src/frame_generation/game_session_inputs.cpp
    src/frame_generation/game_session_present.cpp src/frame_generation/game_session_log.cpp src/frame_generation/hudless_pass.cpp
    src/frame_generation/crash_log.cpp src/perf/sampler.cpp
    # The status lines and the optional CMFM pages (third_party/cmfm); the
    # game's own DLSS rules for its AA and resolution scaling options.
    src/settings/status.cpp src/settings/menu.cpp src/continuous_dlaa/game_options.cpp)
mhw_cxx(mhw_dlss_alpha)
target_compile_options(mhw_dlss_alpha PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/guard:cf>)
target_include_directories(mhw_dlss_alpha PRIVATE src/projection_binding src/game_memory)
target_compile_definitions(mhw_dlss_alpha PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_dlss_alpha PRIVATE bcrypt d3d12 dxgi dxguid
    mhw_ngx_startup mhw_temporal_state mhw_dlss_inline mhw_fg_reflex_presentation mhw_scaled_effects mhw_scaled_resolve)
# The map file names this DLL's functions in crash stacks and stack-sampler
# profiles (tools/profile_report.py; keep it with each handed-out build).
target_link_options(mhw_dlss_alpha PRIVATE /guard:cf /OPT:NOICF /MAP)

# --- Tests ----------------------------------------------------------------------

include(CTest)

# mhw_test(<executable> SOURCES ... [LIBS ...] [DEFINES ...] [INCLUDES ...] [TEST <name>] [NO_TEST])
# One test executable with the project's settings, registered as the CTest
# test <name> (default: the executable's name), run without arguments.
# NO_TEST: a fixture run by hand, or one whose cases are added separately.
function(mhw_test executable)
    cmake_parse_arguments(T "NO_TEST" "TEST" "SOURCES;LIBS;DEFINES;INCLUDES" ${ARGN})
    add_executable(${executable} ${T_SOURCES})
    mhw_cxx(${executable})
    target_compile_definitions(${executable} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE ${T_DEFINES})
    target_include_directories(${executable} PRIVATE src ${T_INCLUDES})
    if(T_LIBS)
        target_link_libraries(${executable} PRIVATE ${T_LIBS})
    endif()
    if(NOT T_NO_TEST)
        if(NOT T_TEST)
            set(T_TEST ${executable})
        endif()
        add_test(NAME ${T_TEST} COMMAND ${executable})
    endif()
endfunction()

if(BUILD_TESTING)
    # MinHook v1.3.4 (third_party/minhook, BSD-2, commit
    # c3fcafdc10146beb5919319d0683e44e3c30d537, vendored as in Astera Loader)
    # for the tests' stand-in loader (tests/fake_astera.cpp). The shipped DLL
    # links no hooking library: Astera Loader holds every hook.
    add_library(minhook STATIC
        third_party/minhook/src/buffer.c third_party/minhook/src/hook.c
        third_party/minhook/src/trampoline.c third_party/minhook/src/hde/hde64.c)
    target_include_directories(minhook PUBLIC third_party/minhook/include PRIVATE third_party/minhook/src third_party/minhook/src/hde)

    # The game-hook code under test compiles with these, as in the shipped DLL.
    set(binding_includes src/projection_binding src/game_memory)
    set(binding_common src/projection_binding/memory.cpp src/projection_binding/policy.cpp src/projection_binding/attachments.cpp
        src/projection_binding/contract.cpp src/projection_binding/stimulus.cpp
        src/projection_binding/identity.cpp src/projection_binding/discovery.cpp src/projection_binding/entry.asm src/game_memory/projection_snapshot.cpp src/game_memory/bounded_scalar_read.cpp)
    set(temporal_binding_sources src/temporal_bindings/inputs.cpp src/temporal_bindings/shader.cpp src/temporal_bindings/depth_route_contract.cpp
        src/temporal_bindings/temporal_entry.asm src/temporal_bindings/depth_route_entry.asm src/temporal_bindings/jitter_entry.asm)

    # Settings, menu and status.
    mhw_test(settings_config_tests SOURCES tests/settings_config_tests.cpp)
    mhw_test(settings_menu_tests SOURCES tests/settings_menu_tests.cpp)

    # The DLSS runtime (runtime.cpp under test): requests, forwarded requests,
    # the list registry, fences, shutdown, depth eligibility.
    mhw_test(continuous_runtime_tests SOURCES tests/continuous_runtime_tests.cpp ${continuous_runtime_sources} src/continuous_dlaa/activation.cpp
        src/continuous_dlaa/activation_entry.asm tests/runtime_neighbours.cpp tests/fake_astera.cpp ${temporal_binding_sources} ${binding_common}
        INCLUDES ${binding_includes} DEFINES MHW_TEMPORAL_FIXTURE
        LIBS minhook mhw_astera_hooks bcrypt d3d12 dxgi dxguid mhw_ngx_startup mhw_temporal_state mhw_dlss_inline NO_TEST)
    foreach(mode continuous jitter_exception jitter_stop jitter_provider jitter_matrix jitter_false_return queues early-reset duplicate
            pending-retirement released-lists held-lists unfiltered-copy reconstruction state-trace native-state-trace
            native-state-admission depth-eligibility depth-destination shutdown-success shutdown-partial
            requests-consumed requests-same-frame requests-wrap requests-blocked requests-unmatched requests-multiple requests-changed
            requests-reentrant requests-depth resolve-forward
            forwarded-wrap forwarded-overlap forwarded-early-reuse forwarded-stop forwarded-exception
            forwarded-changed-before forwarded-changed-after forwarded-stale-token forwarded-ambiguous forwarded-unmatched forwarded-reentrant)
        add_test(NAME temporal_continuous_${mode} COMMAND continuous_runtime_tests ${mode})
    endforeach()
    mhw_test(continuous_history_tests SOURCES tests/continuous_history_tests.cpp TEST temporal_continuous_history)
    mhw_test(continuous_retention_tests SOURCES tests/continuous_retention_tests.cpp TEST temporal_continuous_retention)
    mhw_test(history_comparison_tests SOURCES tests/history_comparison_tests.cpp TEST temporal_history_comparison)
    mhw_test(native_taa_resolve_tests SOURCES tests/native_taa_resolve_tests.cpp INCLUDES src/projection_binding src/game_memory
        TEST native_taa_resolve)
    mhw_test(gpu_memory_tests SOURCES tests/gpu_memory_tests.cpp LIBS dxgi)
    mhw_test(file_digest_tests SOURCES tests/file_digest_tests.cpp INCLUDES src/projection_binding src/game_memory
        LIBS mhw_ngx_startup TEST file_digest)
    mhw_test(ngx_readiness_tests SOURCES tests/ngx_readiness_tests.cpp TEST ngx_readiness)

    # Activation (AA ownership, TAA2's comparison stubs) and render resolution.
    mhw_test(native_activation_tests SOURCES tests/native_activation_tests.cpp tests/native_activation_fixture.asm tests/fake_astera.cpp
        src/continuous_dlaa/activation_entry.asm ${binding_common}
        INCLUDES ${binding_includes} LIBS minhook mhw_astera_hooks bcrypt d3d12 dxgi dxguid
        TEST native_activation_contract)
    mhw_test(render_resolution_tests SOURCES tests/render_resolution_tests.cpp src/render_resolution/resolution_entry.asm ${binding_common}
        INCLUDES ${binding_includes} LIBS mhw_astera_hooks bcrypt d3d12 dxgi dxguid TEST render_resolution_contract)

    # Game inputs: TAA bindings, depth routes and schedules, projection snapshots.
    mhw_test(temporal_inputs_tests SOURCES tests/temporal_inputs_tests.cpp tests/temporal_fixture.asm
        src/temporal_bindings/inputs.cpp src/temporal_bindings/shader.cpp src/temporal_bindings/depth_route_contract.cpp
        src/projection_binding/policy.cpp src/temporal_bindings/temporal_entry.asm src/projection_binding/entry.asm
        INCLUDES src/game_memory TEST temporal_inputs_contract)
    mhw_test(depth_schedule_trace_tests SOURCES tests/depth_schedule_trace_tests.cpp src/temporal_bindings/inputs.cpp
        src/temporal_bindings/depth_route_contract.cpp INCLUDES src/game_memory src/projection_binding TEST depth_schedule_trace)
    mhw_test(depth_consumer_tests SOURCES tests/depth_consumer_tests.cpp src/temporal_bindings/inputs.cpp
        src/temporal_bindings/depth_route_contract.cpp INCLUDES src/game_memory src/projection_binding TEST depth_consumer)
    mhw_test(depth_lease_tests SOURCES tests/depth_lease_tests.cpp TEST depth_lease)
    mhw_test(depth_consumer_gpu SOURCES tests/depth_consumer_gpu.cpp LIBS d3d12 dxgi dxguid NO_TEST)  # run by hand
    mhw_test(projection_snapshot_tests SOURCES tests/projection_snapshot_tests.cpp src/game_memory/projection_snapshot.cpp
        src/game_memory/bounded_scalar_read.cpp INCLUDES src/game_memory src/projection_binding LIBS bcrypt)

    # The full-size post chain: plans, readiness, recording states, the frame
    # handoff, the upscaled inputs, silhouettes.
    mhw_test(scaled_depth_trace_tests SOURCES tests/scaled_depth_trace_tests.cpp TEST scaled_depth_trace)
    mhw_test(stream_readiness_tests SOURCES tests/stream_readiness_tests.cpp TEST stream_readiness)
    mhw_test(output_diagnostics_tests SOURCES tests/output_diagnostics_tests.cpp TEST output_diagnostics)
    mhw_test(scaled_stream_lifetime_tests SOURCES tests/scaled_stream_lifetime_tests.cpp TEST scaled_stream_lifetime)
    mhw_test(recording_states_tests SOURCES tests/recording_states_tests.cpp TEST scaled_recording_states)
    mhw_test(frame_handoff_tests SOURCES tests/frame_handoff_tests.cpp TEST frame_handoff)
    mhw_test(frame_handoff_gpu SOURCES tests/frame_handoff_gpu.cpp LIBS mhw_temporal_state d3d12 dxgi dxguid NO_TEST)  # run by hand
    mhw_test(full_post_inputs_gpu_tests SOURCES tests/full_post_inputs_gpu_tests.cpp
        LIBS mhw_scaled_effects mhw_temporal_present d3d12 dxgi dxguid d3dcompiler TEST full_post_inputs_gpu)
    mhw_test(silhouette_material_tests SOURCES tests/silhouette_material_tests.cpp TEST silhouette_material)
    # The silhouette shaders on WARP or a GPU (run by hand).
    mhw_test(silhouette_shader_fixture SOURCES tools/silhouette_shader_fixture.cpp tests/fake_astera.cpp
        LIBS mhw_scaled_effects mhw_temporal_state minhook d3d12 dxgi dxguid d3dcompiler NO_TEST)

    # Binding restore, the observer and the spatial recovery pass.
    mhw_test(temporal_present_bindings_tests SOURCES tests/temporal_present_bindings_tests.cpp LIBS mhw_temporal_present
        TEST temporal_present_bindings)
    mhw_test(restore_bindings_gpu_tests SOURCES tests/restore_bindings_gpu_tests.cpp
        LIBS mhw_temporal_present d3d12 dxgi dxguid d3dcompiler TEST restore_bindings_gpu)
    mhw_test(observer_slots_tests SOURCES tests/observer_slots_tests.cpp tests/fake_astera.cpp
        LIBS mhw_temporal_state minhook d3d12 dxgi dxguid TEST observer_list_slots)
    # The observer's per-call cost on WARP (run by hand).
    mhw_test(observer_bench SOURCES tests/observer_bench.cpp tests/fake_astera.cpp
        LIBS mhw_temporal_state minhook d3d12 dxgi dxguid NO_TEST)
    target_compile_options(observer_bench PRIVATE /O2)
    mhw_test(mhw_temporal_present_fixture SOURCES tools/temporal_present_fixture.cpp tests/fake_astera.cpp
        LIBS mhw_temporal_state mhw_temporal_present minhook NO_TEST)
    add_test(NAME temporal_present_warp COMMAND mhw_temporal_present_fixture --warp)
    add_test(NAME temporal_spatial_warp COMMAND mhw_temporal_present_fixture --spatial)
    add_test(NAME temporal_spatial_fractional_warp COMMAND mhw_temporal_present_fixture --spatial-fractional)
    add_test(NAME temporal_spatial_identity_warp COMMAND mhw_temporal_present_fixture --spatial-identity)
    set_tests_properties(temporal_present_warp PROPERTIES TIMEOUT 30)

    # Frame generation: the session's model and the HUD-less pass.
    mhw_test(frame_generation_game_tests SOURCES tests/frame_generation_game_tests.cpp TEST frame_generation_game)
    mhw_test(hudless_pass_warp SOURCES tests/hudless_pass_warp.cpp src/frame_generation/hudless_pass.cpp
        LIBS d3d12 dxgi dxguid d3dcompiler)

    # Reflex and presentation: frame policies, the boundary stubs' ABI, Present
    # results, the identity chain, owned interfaces.
    mhw_test(reflex_frame_policy_tests SOURCES tests/reflex_frame_policy_tests.cpp TEST reflex_frame_policy)
    mhw_test(reflex_frame_stamp_tests SOURCES tests/reflex_frame_stamp_tests.cpp TEST reflex_frame_stamp)
    mhw_test(reflex_boundary_abi_tests SOURCES tests/reflex_boundary_abi_tests.cpp src/reflex/frame_boundaries_entry.asm
        TEST reflex_boundary_abi)
    target_compile_options(reflex_boundary_abi_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/guard:cf>)
    target_link_options(reflex_boundary_abi_tests PRIVATE /guard:cf)
    mhw_test(reflex_present_result_tests SOURCES tests/reflex_present_result_tests.cpp LIBS mhw_astera_hooks TEST reflex_present_result)
    # The identity chain shares the window chain's class (WARP).
    mhw_test(identity_chain_tests SOURCES tests/identity_chain_tests.cpp LIBS d3d12 dxgi dxguid user32 TEST identity_chain)
    mhw_test(reflex_owned_interface_tests SOURCES tests/reflex_owned_interface_tests.cpp
        INCLUDES "${MHW_STREAMLINE_SDK_ROOT}/include" TEST reflex_owned_interface)
    # Streamline's loader and Reflex on the local GPU, without the game (run by hand).
    mhw_test(mhw_reflex_sdk_fixture SOURCES tools/reflex_sdk_fixture.cpp LIBS mhw_streamline_reflex d3d12 dxgi dxguid user32 NO_TEST)
    mhw_test(mhw_reflex_startup_fixture SOURCES tools/reflex_startup_fixture.cpp LIBS mhw_streamline_reflex d3d12 dxgi dxguid NO_TEST)

    # Diagnostics: perf toggles, hook timing, the stack sampler.
    mhw_test(perf_tests SOURCES tests/perf_tests.cpp src/game_memory/bounded_scalar_read.cpp src/projection_binding/identity.cpp
        INCLUDES src/projection_binding src/game_memory LIBS bcrypt)
    mhw_test(sampler_tests SOURCES tests/sampler_tests.cpp src/perf/sampler.cpp)

    # The log and release tools' Python tests (tests/test_*.py): fg_game,
    # reflex_frames, package_release, presentmon_summary.
    find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
    add_test(NAME python_tools COMMAND "${Python3_EXECUTABLE}" -B -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -p "test_*.py")

    # Test tiers. "heavy": over a second each, because they run a whole WARP
    # pipeline; each reserves 4 processor slots, so a parallel run does not push
    # them past their timeouts. "quick": everything else. While iterating:
    # ctest -L quick -j 32 plus the changed area's tests; before a build is
    # handed out: the whole suite, ctest -j 32.
    get_property(mhw_registered_tests DIRECTORY PROPERTY TESTS)
    set(mhw_heavy_tests)
    set(mhw_quick_tests)
    foreach(mhw_registered IN LISTS mhw_registered_tests)
        if(mhw_registered MATCHES "^temporal_(present|spatial|spatial_fractional|spatial_identity)_warp$")
            list(APPEND mhw_heavy_tests "${mhw_registered}")
        else()
            list(APPEND mhw_quick_tests "${mhw_registered}")
        endif()
    endforeach()
    set_tests_properties(${mhw_heavy_tests} PROPERTIES LABELS heavy PROCESSORS 4)
    set_tests_properties(${mhw_quick_tests} PROPERTIES LABELS quick)
    # A rate claim (at least 300 stack samples in 1.5 s): only a quiet machine
    # can keep the sampler thread at about 1 kHz, so it runs alone.
    set_tests_properties(sampler_tests PROPERTIES LABELS heavy RUN_SERIAL TRUE)
endif()
