cmake_minimum_required(VERSION 3.24)

project(mhw_dlss_update VERSION 0.1.0 LANGUAGES CXX C)

if(NOT WIN32)
    message(FATAL_ERROR "This research probe targets 64-bit Windows only")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Configure a 64-bit build")
endif()

include(FetchContent)
include(cmake/NgxReplay.cmake)
include(cmake/StreamlineReflex.cmake)

# MinHook v1.3.4. Pinning the commit keeps diagnostic builds reproducible.
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG c3fcafdc10146beb5919319d0683e44e3c30d537
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(minhook)

  add_library(mhw_dlss_bridge_policy STATIC
    src/bridge_probe/scene_trigger.cpp
    src/bridge_probe/scene_trigger.hpp
    src/bridge_probe/jitter_sequence.cpp
    src/bridge_probe/jitter_sequence.hpp
    src/bridge_probe/frame_packet.cpp
    src/bridge_probe/frame_packet.hpp
    src/bridge_probe/projection_snapshot.cpp
    src/bridge_probe/projection_snapshot.hpp
    src/bridge_probe/submission_completion.cpp
    src/bridge_probe/submission_completion.hpp
    src/bridge_probe/depth_work.cpp
    src/bridge_probe/depth_work.hpp
    src/bridge_probe/handle_lifetime.cpp
    src/bridge_probe/handle_lifetime.hpp
    src/bridge_probe/depth_selection.cpp
    src/bridge_probe/depth_selection.hpp
    src/bridge_probe/bridge_policy.cpp
    src/bridge_probe/bridge_policy.hpp
    src/bridge_probe/contract_snapshot.cpp
    src/bridge_probe/contract_snapshot.hpp
    src/bridge_probe/optimal_settings.cpp
    src/bridge_probe/optimal_settings.hpp
    src/bridge_probe/evaluate_gate.cpp
    src/bridge_probe/evaluate_gate.hpp
    src/bridge_probe/renderer_snapshot.cpp
    src/bridge_probe/renderer_snapshot.hpp
    src/bridge_probe/temporal_snapshot.cpp
    src/bridge_probe/temporal_snapshot.hpp
    src/bridge_probe/bounded_scalar_read.cpp
    src/bridge_probe/resource_inspection.cpp
    src/bridge_probe/resource_inspection.hpp
    src/bridge_probe/checkpoint_review.cpp
    src/bridge_probe/checkpoint_review.hpp
    src/bridge_probe/barrier_history.cpp
    src/bridge_probe/barrier_history.hpp
    src/bridge_probe/capture_mailbox.cpp
    src/bridge_probe/capture_mailbox.hpp
    src/bridge_probe/barrier_journal.cpp
    src/bridge_probe/barrier_journal.hpp
)
target_compile_features(mhw_dlss_bridge_policy PUBLIC cxx_std_20)
target_include_directories(mhw_dlss_bridge_policy PUBLIC src/bridge_probe)
if(MSVC)
    target_compile_options(mhw_dlss_bridge_policy PRIVATE
        /W4 /WX /permissive- /EHsc /utf-8 /guard:cf
    )
endif()

add_library(mhw_dlss_probe SHARED
    src/probe/dllmain.cpp
    src/probe/ngx_abi.hpp
)

target_compile_features(mhw_dlss_probe PRIVATE cxx_std_20)
target_compile_definitions(mhw_dlss_probe PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)
target_link_libraries(mhw_dlss_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)

if(MSVC)
    target_compile_options(mhw_dlss_probe PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
endif()

set_target_properties(mhw_dlss_probe PROPERTIES
    OUTPUT_NAME "mhw_dlss_probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_library(mhw_dlss_unlock SHARED
    src/trace/dllmain.cpp
)

target_compile_features(mhw_dlss_unlock PRIVATE cxx_std_20)
target_compile_definitions(mhw_dlss_unlock PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)
target_link_libraries(mhw_dlss_unlock PRIVATE minhook bcrypt dxgi version shell32 ole32)

if(MSVC)
    target_compile_options(mhw_dlss_unlock PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
    target_link_options(mhw_dlss_unlock PRIVATE /guard:cf /OPT:NOICF)
endif()

set_target_properties(mhw_dlss_unlock PROPERTIES
    OUTPUT_NAME "mhw_dlss_unlock"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Early-load bridge boundary. The implementation intentionally reuses the
# exact resolver/status instrumentation from the preserved v0.4.1 observer,
# with bridge behavior compiled behind a separate definition and output name.
add_library(mhw_dlss_bridge_probe SHARED
    src/bridge_probe/native_depth_probe.cpp
    src/bridge_probe/native_depth_probe.hpp
    src/bridge_probe/native_depth_entry.asm
    src/bridge_probe/native_taa_fallback.cpp
    src/bridge_probe/native_taa_fallback.hpp
    src/trace/dllmain.cpp
    src/bridge_probe/presentation_observer.cpp
    src/bridge_probe/presentation_observer.hpp
    src/bridge_probe/barrier_observer.cpp
    src/bridge_probe/barrier_observer.hpp
    src/bridge_probe/bridge_policy.hpp
    src/bridge_probe/contract_snapshot.hpp
)
target_compile_features(mhw_dlss_bridge_probe PRIVATE cxx_std_20)
target_compile_definitions(mhw_dlss_bridge_probe PRIVATE
    MHW_DLSS_BRIDGE_PROBE=1
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)
target_link_libraries(mhw_dlss_bridge_probe PRIVATE
    mhw_dlss_bridge_policy
    minhook
    bcrypt
    d3d12
    dxgi
    dxguid
    version
    shell32
    ole32
)
if(MSVC)
    target_compile_options(mhw_dlss_bridge_probe PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>
    )
    target_link_options(mhw_dlss_bridge_probe PRIVATE
        /guard:cf /OPT:NOICF /EXPORT:native_depth_helper_entry
    )
endif()
set_target_properties(mhw_dlss_bridge_probe PROPERTIES
    OUTPUT_NAME "mhw_dlss_bridge_probe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# `cmake --install --prefix <MO2 mod directory>` creates the exact virtual
# layout consumed by Stracker's Loader without touching the game installation.
install(TARGETS mhw_dlss_probe
    RUNTIME DESTINATION "nativePC/plugins"
    COMPONENT probe
)

install(TARGETS mhw_dlss_unlock
    # This observer must be configured as an MO2 forced library so its resolver
    # hook is installed before the game's first NGX export lookup.
    RUNTIME DESTINATION "."
    COMPONENT unlock
)

install(TARGETS mhw_dlss_bridge_probe
    # Like the resolver observer, this must be configured as an MO2 forced
    # library. It is mutually exclusive with mhw_dlss_unlock at runtime.
    RUNTIME DESTINATION "."
    COMPONENT bridge-probe
)

include(CTest)

# External, exact-host exception diagnostic. No installation into the game
# or MO2 is needed; output paths are supplied by the attended diagnostic run.
add_executable(mhw_depth_exception_capture
    src/exception_capture/main.cpp
    src/exception_capture/depth_record.cpp
    src/exception_capture/depth_record.hpp
    src/exception_capture/depth_retry.cpp
    src/exception_capture/depth_retry.hpp
    src/exception_capture/capture_retry.hpp
    src/exception_capture/depth_tracker.cpp
    src/exception_capture/depth_tracker.hpp
    src/exception_capture/host_contract.hpp
)
target_compile_features(mhw_depth_exception_capture PRIVATE cxx_std_20)
target_compile_definitions(mhw_depth_exception_capture PRIVATE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_depth_exception_capture PRIVATE bcrypt mhw_dlss_bridge_policy)
if(MSVC)
    enable_language(ASM_MASM)
    add_library(mhw_depth_retry_fixture OBJECT src/exception_capture/retry_fixture.asm)
    target_sources(mhw_depth_exception_capture PRIVATE $<TARGET_OBJECTS:mhw_depth_retry_fixture>)
    target_compile_options(mhw_depth_exception_capture PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
    target_link_options(mhw_depth_exception_capture PRIVATE /guard:cf)
endif()
set_target_properties(mhw_depth_exception_capture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Independent CPU callback recorder. No NGX provider, rendering admission or SDK integration.
add_library(mhw_jitter_callback_probe SHARED
    src/jitter_callbacks/dllmain.cpp src/jitter_callbacks/recorder.cpp
    src/jitter_callbacks/entry.asm src/bridge_probe/bounded_scalar_read.cpp)
target_compile_features(mhw_jitter_callback_probe PRIVATE cxx_std_20)
target_include_directories(mhw_jitter_callback_probe PRIVATE src/jitter_callbacks src/bridge_probe)
target_compile_definitions(mhw_jitter_callback_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_jitter_callback_probe PRIVATE minhook bcrypt dxgi)
if(MSVC)
    target_compile_options(mhw_jitter_callback_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_jitter_callback_probe PRIVATE /guard:cf /OPT:NOICF)
endif()
set_target_properties(mhw_jitter_callback_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
# Standalone typed property writer observer; no NGX or rendering changes.
add_library(mhw_jitter_writer_probe SHARED
    src/jitter_writer/dllmain.cpp src/jitter_writer/recorder.cpp
    src/jitter_writer/entry.asm src/bridge_probe/bounded_scalar_read.cpp)
target_compile_features(mhw_jitter_writer_probe PRIVATE cxx_std_20)
target_include_directories(mhw_jitter_writer_probe PRIVATE src/jitter_writer src/bridge_probe)
target_compile_definitions(mhw_jitter_writer_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_jitter_writer_probe PRIVATE minhook bcrypt dxgi)
if(MSVC)
    target_compile_options(mhw_jitter_writer_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_jitter_writer_probe PRIVATE /guard:cf /OPT:NOICF)
endif()
set_target_properties(mhw_jitter_writer_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(mhw_jitter_activation_probe SHARED src/jitter_activation/dllmain.cpp
    src/jitter_activation/recorder.cpp src/jitter_activation/identity.cpp
    src/bridge_probe/projection_snapshot.cpp src/bridge_probe/bounded_scalar_read.cpp)
target_compile_features(mhw_jitter_activation_probe PRIVATE cxx_std_20)
target_include_directories(mhw_jitter_activation_probe PRIVATE src/jitter_activation src/bridge_probe)
target_compile_definitions(mhw_jitter_activation_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_jitter_activation_probe PRIVATE minhook bcrypt dxgi)
target_compile_options(mhw_jitter_activation_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_jitter_activation_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_jitter_activation_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
# Read-only, one-draw projection/publication/binding recorder. No provider SDK.
set(binding_common src/projection_binding/recorder.cpp src/projection_binding/policy.cpp
    src/projection_binding/provenance.cpp src/projection_binding/provenance_policy.cpp src/projection_binding/depth_readback.cpp
    src/projection_binding/attachments.cpp src/projection_binding/attachment_output.cpp
    src/projection_binding/trampoline_pool.cpp src/projection_binding/contract.cpp src/projection_binding/stimulus.cpp
    src/projection_binding/identity.cpp src/projection_binding/discovery.cpp src/projection_binding/output.cpp
    src/projection_binding/entry.asm src/bridge_probe/projection_snapshot.cpp src/bridge_probe/bounded_scalar_read.cpp)
add_library(mhw_render_resolution_probe SHARED src/render_resolution/dllmain.cpp
    src/render_resolution/runtime.cpp src/render_resolution/resolution_entry.asm ${binding_common})
target_compile_features(mhw_render_resolution_probe PRIVATE cxx_std_20)
target_include_directories(mhw_render_resolution_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_render_resolution_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_render_resolution_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)
target_compile_options(mhw_render_resolution_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_render_resolution_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_render_resolution_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(BUILD_TESTING)
    add_executable(render_resolution_tests tests/render_resolution_tests.cpp src/render_resolution/resolution_entry.asm ${binding_common})
    target_compile_features(render_resolution_tests PRIVATE cxx_std_20)
    target_include_directories(render_resolution_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(render_resolution_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(render_resolution_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
    target_compile_options(render_resolution_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    set_target_properties(render_resolution_tests PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME render_resolution_contract COMMAND render_resolution_tests)
endif()
add_library(mhw_projection_binding_probe SHARED src/projection_binding/dllmain.cpp ${binding_common})
target_compile_features(mhw_projection_binding_probe PRIVATE cxx_std_20)
target_include_directories(mhw_projection_binding_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_projection_binding_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_projection_binding_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)
target_compile_options(mhw_projection_binding_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_projection_binding_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_projection_binding_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Independent-AA scheduling boundary. This bounded build observes and forwards;
# it does not insert a pass or change any native AA state.
add_library(mhw_aa_boundary_probe SHARED src/aa_control/boundary_probe.cpp src/aa_control/post_queue.cpp
    src/aa_control/schedule_entry.asm ${binding_common})
target_compile_features(mhw_aa_boundary_probe PRIVATE cxx_std_20)
target_include_directories(mhw_aa_boundary_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_aa_boundary_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_aa_boundary_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)
target_compile_options(mhw_aa_boundary_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_aa_boundary_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_aa_boundary_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Bounded owned in-place pass at the same boundary; no AA shader or SDK yet.
add_library(mhw_aa_owned_probe SHARED src/aa_control/boundary_probe.cpp src/aa_control/post_queue.cpp
    src/aa_control/owned_pass.cpp src/aa_control/owned_runtime.cpp src/temporal_bindings/inputs.cpp
    src/aa_control/schedule_entry.asm ${binding_common})
target_compile_features(mhw_aa_owned_probe PRIVATE cxx_std_20)
target_include_directories(mhw_aa_owned_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_aa_owned_probe PRIVATE MHW_AA_OWNED_PASS WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_aa_owned_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)
target_compile_options(mhw_aa_owned_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_aa_owned_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_aa_owned_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Owned scheduling plus a neutral, uniquely identified deferred backend packet.
add_library(mhw_aa_deferred_probe SHARED src/aa_control/boundary_probe.cpp src/aa_control/post_queue.cpp
    src/aa_control/owned_pass.cpp src/aa_control/owned_runtime.cpp src/aa_control/deferred_runtime.cpp
    src/temporal_bindings/inputs.cpp src/aa_control/schedule_entry.asm ${binding_common})
target_compile_features(mhw_aa_deferred_probe PRIVATE cxx_std_20)
target_include_directories(mhw_aa_deferred_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_aa_deferred_probe PRIVATE MHW_AA_OWNED_PASS MHW_AA_DEFERRED WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_aa_deferred_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)
target_compile_options(mhw_aa_deferred_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_aa_deferred_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_aa_deferred_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(mhw_aa_gpu_probe SHARED src/aa_control/boundary_probe.cpp src/aa_control/post_queue.cpp
    src/aa_control/owned_pass.cpp src/aa_control/owned_runtime.cpp src/aa_control/deferred_runtime.cpp
    src/aa_control/gpu_trace.cpp src/temporal_bindings/submission.cpp
    src/temporal_bindings/inputs.cpp src/aa_control/schedule_entry.asm ${binding_common})
target_compile_features(mhw_aa_gpu_probe PRIVATE cxx_std_20)
target_include_directories(mhw_aa_gpu_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_aa_gpu_probe PRIVATE MHW_AA_OWNED_PASS MHW_AA_DEFERRED MHW_AA_GPU_TRACE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_aa_gpu_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)
target_compile_options(mhw_aa_gpu_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_aa_gpu_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_aa_gpu_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(mhw_aa_inputs_probe SHARED src/aa_control/boundary_probe.cpp src/aa_control/post_queue.cpp
    src/aa_control/owned_pass.cpp src/aa_control/owned_runtime.cpp src/aa_control/deferred_runtime.cpp
    src/aa_control/gpu_trace.cpp src/aa_control/input_readback.cpp src/temporal_bindings/submission.cpp
    src/temporal_bindings/inputs.cpp src/aa_control/schedule_entry.asm ${binding_common})
target_compile_features(mhw_aa_inputs_probe PRIVATE cxx_std_20)
target_include_directories(mhw_aa_inputs_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_aa_inputs_probe PRIVATE MHW_AA_OWNED_PASS MHW_AA_DEFERRED MHW_AA_GPU_TRACE MHW_AA_INPUT_READBACK WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_aa_inputs_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)
target_compile_options(mhw_aa_inputs_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_aa_inputs_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_aa_inputs_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Independent AA-Off entry: exact projection receipts plus native binding sets.
get_target_property(aa_projection_sources mhw_aa_inputs_probe SOURCES)
add_library(mhw_aa_projection_probe SHARED ${aa_projection_sources}
    src/temporal_bindings/jitter.cpp src/temporal_bindings/jitter_entry.asm)
target_compile_features(mhw_aa_projection_probe PRIVATE cxx_std_20)
target_include_directories(mhw_aa_projection_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_aa_projection_probe PRIVATE MHW_AA_OWNED_PASS MHW_AA_DEFERRED MHW_AA_GPU_TRACE MHW_AA_INPUT_READBACK MHW_AA_BINDINGS MHW_AA_PROJECTION MHW_TEMPORAL_PRESENT_STATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_aa_projection_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_temporal_state)
target_compile_options(mhw_aa_projection_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_aa_projection_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_aa_projection_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Separate TAA request observer with private submission-completion fences.
# Bounded temporal-input diagnostics; runtime budgets are versioned in the log.
set(temporal_depth_sources src/temporal_bindings/depth_route_contract.cpp src/temporal_bindings/depth_routes.cpp src/temporal_bindings/depth_route_entry.asm src/temporal_bindings/owned_inputs.cpp)
add_library(mhw_temporal_inputs_probe SHARED src/temporal_bindings/dllmain.cpp src/temporal_bindings/submission.cpp src/temporal_bindings/motion_readback.cpp src/temporal_bindings/jitter.cpp src/temporal_bindings/jitter_entry.asm
    src/temporal_bindings/inputs.cpp src/temporal_bindings/shader.cpp src/temporal_bindings/consumer.cpp src/temporal_bindings/probe.cpp src/temporal_bindings/temporal_entry.asm ${temporal_depth_sources} ${binding_common})
target_compile_features(mhw_temporal_inputs_probe PRIVATE cxx_std_20)
target_include_directories(mhw_temporal_inputs_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_temporal_inputs_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_temporal_inputs_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_temporal_gpu_inputs)
target_compile_options(mhw_temporal_inputs_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_temporal_inputs_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_temporal_inputs_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

get_target_property(scaled_temporal_sources mhw_temporal_inputs_probe SOURCES)
add_library(mhw_scaled_temporal_probe SHARED ${scaled_temporal_sources}
    src/scaled_temporal/handoff.cpp src/scaled_temporal/downstream.cpp src/scaled_temporal/postfilter.cpp src/scaled_temporal/draw_execution.cpp src/scaled_temporal/handoff_entry.asm
    src/scaled_temporal/runtime.cpp src/render_resolution/runtime.cpp src/render_resolution/resolution_entry.asm
    src/continuous_dlaa/activation.cpp src/continuous_dlaa/activation_entry.asm)
target_compile_features(mhw_scaled_temporal_probe PRIVATE cxx_std_20)
target_include_directories(mhw_scaled_temporal_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_scaled_temporal_probe PRIVATE MHW_TEMPORAL_SCALED WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_scaled_temporal_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_temporal_gpu_inputs)
target_compile_options(mhw_scaled_temporal_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_scaled_temporal_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_scaled_temporal_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(mhw_temporal_gpu_inputs STATIC
    src/temporal_gpu_inputs/contract.cpp src/temporal_gpu_inputs/replay.cpp)
target_compile_features(mhw_temporal_gpu_inputs PUBLIC cxx_std_20)
target_compile_definitions(mhw_temporal_gpu_inputs PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(mhw_temporal_gpu_inputs PUBLIC d3d12 dxgi dxguid d3dcompiler)
target_compile_options(mhw_temporal_gpu_inputs PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
add_library(mhw_temporal_present STATIC src/temporal_present/output.cpp src/temporal_present/bindings.cpp src/temporal_present/unfiltered.cpp)
target_compile_features(mhw_temporal_present PUBLIC cxx_std_20)
target_compile_definitions(mhw_temporal_present PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(mhw_temporal_present PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(mhw_temporal_present PUBLIC d3d12 dxgi dxguid d3dcompiler)
include(cmake/DxbcHash.cmake)
add_library(mhw_scaled_filter_shader STATIC src/scaled_temporal/filter_shader.cpp src/scaled_temporal/haze_shader.cpp src/scaled_temporal/radial_shader.cpp src/scaled_temporal/dof_shader.cpp src/scaled_temporal/dof_compute_shader.cpp src/scaled_temporal/silhouette_shader.cpp)
target_compile_features(mhw_scaled_filter_shader PUBLIC cxx_std_20)
target_compile_definitions(mhw_scaled_filter_shader PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(mhw_scaled_filter_shader PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(mhw_scaled_filter_shader PRIVATE mhw_dxbc_hash bcrypt)
add_executable(silhouette_shader_fixture tools/silhouette_shader_fixture.cpp)
target_compile_features(silhouette_shader_fixture PRIVATE cxx_std_20)
target_compile_definitions(silhouette_shader_fixture PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(silhouette_shader_fixture PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(silhouette_shader_fixture PRIVATE mhw_scaled_effects mhw_temporal_state minhook d3d12 dxgi dxguid d3dcompiler)
set_target_properties(silhouette_shader_fixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(mhw_scaled_filter_fixture tools/scaled_filter_fixture.cpp)
target_compile_features(mhw_scaled_filter_fixture PRIVATE cxx_std_20)
target_compile_options(mhw_scaled_filter_fixture PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(mhw_scaled_filter_fixture PRIVATE mhw_scaled_filter_shader d3d12 dxgi dxguid d3dcompiler)
set_target_properties(mhw_scaled_filter_fixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(mhw_scaled_haze_fixture tools/scaled_haze_fixture.cpp)
target_compile_features(mhw_scaled_haze_fixture PRIVATE cxx_std_20)
target_compile_options(mhw_scaled_haze_fixture PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(mhw_scaled_haze_fixture PRIVATE mhw_scaled_effects d3d12 dxgi dxguid d3dcompiler)
set_target_properties(mhw_scaled_haze_fixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(scaled_dof_fixture tools/scaled_dof_fixture.cpp)
target_compile_features(scaled_dof_fixture PRIVATE cxx_std_20)
target_compile_options(scaled_dof_fixture PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(scaled_dof_fixture PRIVATE mhw_scaled_effects d3d12 dxgi dxguid d3dcompiler)
set_target_properties(scaled_dof_fixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(dof_pipeline_fixture tools/dof_pipeline_fixture.cpp)
target_compile_features(dof_pipeline_fixture PRIVATE cxx_std_20)
target_compile_options(dof_pipeline_fixture PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(dof_pipeline_fixture PRIVATE mhw_scaled_effects d3d12 dxgi dxguid d3dcompiler bcrypt)
set_target_properties(dof_pipeline_fixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(scaled_dependency_fixture tools/scaled_dependency_fixture.cpp)
target_compile_features(scaled_dependency_fixture PRIVATE cxx_std_20)
target_compile_options(scaled_dependency_fixture PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(scaled_dependency_fixture PRIVATE mhw_scaled_effects d3d12 dxgi dxguid d3dcompiler)
set_target_properties(scaled_dependency_fixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(color_dependencies_tests tests/color_dependencies_tests.cpp)
target_compile_features(color_dependencies_tests PRIVATE cxx_std_20)
target_compile_options(color_dependencies_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
set_target_properties(color_dependencies_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME color_dependencies COMMAND color_dependencies_tests)
add_library(mhw_scaled_effects STATIC src/scaled_temporal/effects.cpp src/scaled_temporal/overlay_extent.cpp src/scaled_temporal/silhouette_draw.cpp)
target_compile_features(mhw_scaled_effects PUBLIC cxx_std_20)
target_compile_definitions(mhw_scaled_effects PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(mhw_scaled_effects PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(mhw_scaled_effects PUBLIC mhw_scaled_filter_shader d3d12 dxgi dxguid bcrypt d3dcompiler)
add_executable(mhw_scaled_effect_fixture tools/scaled_effect_fixture.cpp)
target_compile_features(mhw_scaled_effect_fixture PRIVATE cxx_std_20)
target_compile_options(mhw_scaled_effect_fixture PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(mhw_scaled_effect_fixture PRIVATE mhw_scaled_effects mhw_temporal_state d3dcompiler)
set_target_properties(mhw_scaled_effect_fixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_executable(scaled_effect_vertices_tests tests/scaled_effect_vertices_tests.cpp)
target_compile_features(scaled_effect_vertices_tests PRIVATE cxx_std_20)
target_compile_options(scaled_effect_vertices_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(scaled_effect_vertices_tests PRIVATE mhw_scaled_effects)
set_target_properties(scaled_effect_vertices_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME scaled_effect_vertices COMMAND scaled_effect_vertices_tests)
add_executable(overlay_vertex_tests tests/overlay_vertex_tests.cpp)
target_compile_features(overlay_vertex_tests PRIVATE cxx_std_20)
target_compile_options(overlay_vertex_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(overlay_vertex_tests PRIVATE mhw_scaled_effects)
set_target_properties(overlay_vertex_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME overlay_vertices COMMAND overlay_vertex_tests)
add_library(mhw_temporal_state STATIC src/temporal_present/observer.cpp)
target_compile_features(mhw_temporal_state PUBLIC cxx_std_20)
target_compile_options(mhw_temporal_state PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
target_link_libraries(mhw_temporal_state PUBLIC mhw_temporal_present minhook)
add_executable(mhw_temporal_input_replay tools/temporal_input_replay.cpp)
target_link_libraries(mhw_temporal_input_replay PRIVATE mhw_temporal_gpu_inputs)
target_compile_options(mhw_temporal_input_replay PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
set_target_properties(mhw_temporal_input_replay PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(TARGET mhw_dlss_sdk_live)
    add_library(mhw_dlss_inline STATIC src/dlss_sdk/inline.cpp)
    target_include_directories(mhw_dlss_inline SYSTEM PRIVATE "${MHW_NGX_SDK_ROOT}/include")
    target_link_libraries(mhw_dlss_inline PUBLIC mhw_temporal_present mhw_dlss_sdk_live)
    target_compile_options(mhw_dlss_inline PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /external:W0)
    set_target_properties(mhw_dlss_inline PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    add_library(mhw_native_taa_resolve STATIC src/continuous_dlaa/resolve.cpp)
    target_link_libraries(mhw_native_taa_resolve PUBLIC mhw_dlss_inline)
    target_compile_options(mhw_native_taa_resolve PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(mhw_native_taa_resolve PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    add_library(mhw_scaled_resolve STATIC src/scaled_temporal/resolve.cpp)
    target_link_libraries(mhw_scaled_resolve PUBLIC mhw_native_taa_resolve)
    target_compile_options(mhw_scaled_resolve PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(mhw_scaled_resolve PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    add_library(mhw_aa_sdk_pass STATIC src/aa_control/sdk_pass.cpp)
    target_include_directories(mhw_aa_sdk_pass PUBLIC src/projection_binding src/bridge_probe)
    target_link_libraries(mhw_aa_sdk_pass PUBLIC mhw_dlss_inline)
    target_compile_options(mhw_aa_sdk_pass PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(mhw_aa_sdk_pass PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    add_library(mhw_ngx_startup STATIC src/ngx_startup/startup.cpp src/ngx_startup/worker.cpp src/projection_binding/identity.cpp src/bridge_probe/bounded_scalar_read.cpp)
    target_compile_features(mhw_ngx_startup PUBLIC cxx_std_20)
    target_include_directories(mhw_ngx_startup PRIVATE src/bridge_probe)
    target_include_directories(mhw_ngx_startup SYSTEM PRIVATE "${MHW_NGX_SDK_ROOT}/include")
    target_compile_definitions(mhw_ngx_startup PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(mhw_ngx_startup PUBLIC minhook bcrypt d3d12 dxgi dxguid "${MHW_NGX_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib")
    target_compile_options(mhw_ngx_startup PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /external:W0 /guard:cf)
    set_target_properties(mhw_ngx_startup PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    get_target_property(aa_sdk_sources mhw_aa_projection_probe SOURCES)
    add_library(mhw_aa_sdk_probe SHARED ${aa_sdk_sources} src/aa_control/sdk_runtime.cpp)
    target_compile_features(mhw_aa_sdk_probe PRIVATE cxx_std_20)
    target_include_directories(mhw_aa_sdk_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(mhw_aa_sdk_probe PRIVATE MHW_AA_OWNED_PASS MHW_AA_DEFERRED MHW_AA_GPU_TRACE MHW_AA_INPUT_READBACK MHW_AA_BINDINGS MHW_AA_PROJECTION MHW_AA_SDK MHW_TEMPORAL_PRESENT_STATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(mhw_aa_sdk_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_temporal_state mhw_aa_sdk_pass mhw_ngx_startup)
    target_compile_options(mhw_aa_sdk_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_aa_sdk_probe PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(mhw_aa_sdk_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_executable(aa_sdk_fixture tests/aa_sdk_fixture.cpp tests/ngx_startup_entry.asm)
    target_link_libraries(aa_sdk_fixture PRIVATE mhw_aa_sdk_pass mhw_ngx_startup mhw_temporal_state)
    target_compile_options(aa_sdk_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    set_target_properties(aa_sdk_fixture PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_library(mhw_aa_stream_pass STATIC src/aa_control/stream_pass.cpp)
    target_link_libraries(mhw_aa_stream_pass PUBLIC mhw_aa_sdk_pass)
    target_compile_options(mhw_aa_stream_pass PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(mhw_aa_stream_pass PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
    add_library(mhw_aa_stream SHARED src/aa_control/stream_dllmain.cpp src/aa_control/stream_runtime.cpp
        src/aa_control/owned_runtime.cpp src/aa_control/owned_pass.cpp src/aa_control/post_queue.cpp
        src/temporal_bindings/inputs.cpp src/aa_control/schedule_entry.asm src/temporal_bindings/jitter_entry.asm ${binding_common})
    target_compile_features(mhw_aa_stream PRIVATE cxx_std_20)
    target_include_directories(mhw_aa_stream PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(mhw_aa_stream PRIVATE MHW_AA_DEFERRED WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(mhw_aa_stream PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_ngx_startup mhw_temporal_state mhw_aa_stream_pass)
    target_compile_options(mhw_aa_stream PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_aa_stream PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(mhw_aa_stream PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_executable(aa_stream_fixture tests/aa_stream_fixture.cpp tests/ngx_startup_entry.asm)
    target_link_libraries(aa_stream_fixture PRIVATE mhw_aa_stream_pass mhw_ngx_startup mhw_temporal_state)
    target_compile_options(aa_stream_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    set_target_properties(aa_stream_fixture PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_executable(aa_stream_lifetime_tests tests/aa_stream_lifetime_tests.cpp)
    target_compile_features(aa_stream_lifetime_tests PRIVATE cxx_std_20)
    target_compile_options(aa_stream_lifetime_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME aa_stream_lifetime COMMAND aa_stream_lifetime_tests)
    add_executable(aa_stream_runtime_tests tests/aa_stream_runtime_tests.cpp src/aa_control/owned_runtime.cpp src/aa_control/owned_pass.cpp src/aa_control/post_queue.cpp
        src/temporal_bindings/inputs.cpp src/aa_control/schedule_entry.asm src/temporal_bindings/jitter_entry.asm ${binding_common})
    target_compile_features(aa_stream_runtime_tests PRIVATE cxx_std_20)
    target_include_directories(aa_stream_runtime_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(aa_stream_runtime_tests PRIVATE MHW_AA_DEFERRED MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(aa_stream_runtime_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_ngx_startup mhw_temporal_state mhw_aa_stream_pass)
    target_compile_options(aa_stream_runtime_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(aa_stream_runtime_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(aa_stream_runtime_tests PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(case queues early-reset pending-gpu duplicate reset-during-submit admission-diagnostic boundary-diagnostic)
        add_test(NAME aa_stream_runtime_${case} COMMAND aa_stream_runtime_tests ${case})
        set_tests_properties(aa_stream_runtime_${case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_library(mhw_ngx_startup_probe SHARED src/ngx_startup/dllmain.cpp)
    add_executable(ngx_startup_fixture tests/ngx_startup_fixture.cpp tests/ngx_startup_entry.asm)
    foreach(t mhw_ngx_startup_probe ngx_startup_fixture)
        target_link_libraries(${t} PRIVATE mhw_ngx_startup)
        target_include_directories(${t} PRIVATE src/bridge_probe)
        target_compile_options(${t} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
        target_link_options(${t} PRIVATE /guard:cf /OPT:NOICF)
        set_target_properties(${t} PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endforeach()
    add_executable(mhw_dlaa_live_fixture tools/dlaa_live_fixture.cpp tests/ngx_startup_entry.asm)
    add_executable(mhw_dlaa_inline_fixture tools/dlaa_inline_fixture.cpp tests/ngx_startup_entry.asm)
    get_target_property(scaled_output_sources mhw_scaled_temporal_probe SOURCES)
    add_library(mhw_scaled_output_probe SHARED ${scaled_output_sources} src/scaled_temporal/output_route.cpp src/temporal_bindings/inline_pass.cpp)
    target_compile_features(mhw_scaled_output_probe PRIVATE cxx_std_20)
    target_include_directories(mhw_scaled_output_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(mhw_scaled_output_probe PRIVATE MHW_SCALED_OUTPUT MHW_TEMPORAL_SCALED MHW_TEMPORAL_DLSS MHW_TEMPORAL_INLINE MHW_TEMPORAL_PRESENT_STATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(mhw_scaled_output_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_temporal_gpu_inputs mhw_scaled_effects mhw_scaled_resolve mhw_dlss_sdk_live mhw_ngx_startup mhw_temporal_state)
    target_compile_options(mhw_scaled_output_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_scaled_output_probe PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(mhw_scaled_output_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_executable(scaled_output_route_fixture tests/scaled_output_route_fixture.cpp src/scaled_temporal/output_route.cpp ${binding_common})
    target_compile_features(scaled_output_route_fixture PRIVATE cxx_std_20)
    target_include_directories(scaled_output_route_fixture PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(scaled_output_route_fixture PRIVATE MHW_SCALED_ROUTE_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(scaled_output_route_fixture PRIVATE mhw_scaled_effects mhw_temporal_state minhook bcrypt d3d12 dxgi dxguid d3dcompiler)
    target_compile_options(scaled_output_route_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    set_target_properties(scaled_output_route_fixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_executable(scaled_stream_route_fixture tests/scaled_stream_route_fixture.cpp
        src/scaled_temporal/stream_entry.asm src/temporal_bindings/inputs.cpp ${binding_common})
    target_compile_features(scaled_stream_route_fixture PRIVATE cxx_std_20)
    target_include_directories(scaled_stream_route_fixture PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(scaled_stream_route_fixture PRIVATE MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(scaled_stream_route_fixture PRIVATE mhw_scaled_effects mhw_temporal_state minhook bcrypt d3d12 dxgi dxguid d3dcompiler)
    target_compile_options(scaled_stream_route_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    set_target_properties(scaled_stream_route_fixture PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_executable(dof_native_route_fixture tools/dof_pipeline_fixture.cpp
        src/scaled_temporal/stream_entry.asm src/temporal_bindings/inputs.cpp ${binding_common})
    target_compile_features(dof_native_route_fixture PRIVATE cxx_std_20)
    target_include_directories(dof_native_route_fixture PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(dof_native_route_fixture PRIVATE MHW_DOF_NATIVE_ROUTE_FIXTURE MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(dof_native_route_fixture PRIVATE mhw_scaled_effects mhw_temporal_state minhook bcrypt d3d12 dxgi dxguid d3dcompiler)
    target_compile_options(dof_native_route_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    set_target_properties(dof_native_route_fixture PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_executable(scaled_depth_trace_tests tests/scaled_depth_trace_tests.cpp)
    target_compile_features(scaled_depth_trace_tests PRIVATE cxx_std_20)
    target_compile_options(scaled_depth_trace_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(scaled_depth_trace_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME scaled_depth_trace COMMAND scaled_depth_trace_tests)
    add_executable(depth_schedule_trace_tests tests/depth_schedule_trace_tests.cpp src/temporal_bindings/inputs.cpp src/temporal_bindings/depth_route_contract.cpp)
    target_compile_features(depth_schedule_trace_tests PRIVATE cxx_std_20)
    target_include_directories(depth_schedule_trace_tests PRIVATE src/bridge_probe src/projection_binding)
    target_compile_definitions(depth_schedule_trace_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(depth_schedule_trace_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(depth_schedule_trace_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME depth_schedule_trace COMMAND depth_schedule_trace_tests)
    add_executable(depth_consumer_tests tests/depth_consumer_tests.cpp src/temporal_bindings/inputs.cpp src/temporal_bindings/depth_route_contract.cpp)
    target_compile_features(depth_consumer_tests PRIVATE cxx_std_20)
    target_include_directories(depth_consumer_tests PRIVATE src/bridge_probe src/projection_binding)
    target_compile_definitions(depth_consumer_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(depth_consumer_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(depth_consumer_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME depth_consumer COMMAND depth_consumer_tests)
    add_executable(stream_readiness_tests tests/stream_readiness_tests.cpp)
    target_compile_features(stream_readiness_tests PRIVATE cxx_std_20)
    target_compile_options(stream_readiness_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(stream_readiness_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME stream_readiness COMMAND stream_readiness_tests)
    add_executable(output_diagnostics_tests tests/output_diagnostics_tests.cpp)
    target_compile_features(output_diagnostics_tests PRIVATE cxx_std_20)
    target_compile_options(output_diagnostics_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(output_diagnostics_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME output_diagnostics COMMAND output_diagnostics_tests)
    add_executable(output_dependencies_tests tests/output_dependencies_tests.cpp)
    target_compile_features(output_dependencies_tests PRIVATE cxx_std_20)
    target_compile_options(output_dependencies_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(output_dependencies_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME output_dependencies COMMAND output_dependencies_tests)
    add_executable(prepared_queue_tests tests/prepared_queue_tests.cpp src/temporal_bindings/inputs.cpp)
    target_include_directories(prepared_queue_tests PRIVATE src/projection_binding src/bridge_probe)
    target_compile_features(prepared_queue_tests PRIVATE cxx_std_20)
    target_compile_definitions(prepared_queue_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(prepared_queue_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(prepared_queue_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME prepared_queue COMMAND prepared_queue_tests)
    add_executable(frame_handoff_tests tests/frame_handoff_tests.cpp)
    target_compile_features(frame_handoff_tests PRIVATE cxx_std_20)
    target_compile_options(frame_handoff_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(frame_handoff_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME frame_handoff COMMAND frame_handoff_tests)
    add_executable(frame_handoff_gpu tests/frame_handoff_gpu.cpp)
    target_compile_features(frame_handoff_gpu PRIVATE cxx_std_20)
    target_compile_definitions(frame_handoff_gpu PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(frame_handoff_gpu PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    target_link_libraries(frame_handoff_gpu PRIVATE mhw_temporal_state d3d12 dxgi dxguid)
    set_target_properties(frame_handoff_gpu PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_executable(depth_consumer_gpu tests/depth_consumer_gpu.cpp)
    target_compile_features(depth_consumer_gpu PRIVATE cxx_std_20)
    target_compile_definitions(depth_consumer_gpu PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_compile_options(depth_consumer_gpu PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    target_link_libraries(depth_consumer_gpu PRIVATE d3d12 dxgi dxguid)
    set_target_properties(depth_consumer_gpu PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    target_link_libraries(mhw_dlaa_inline_fixture PRIVATE mhw_scaled_resolve mhw_scaled_effects mhw_temporal_gpu_inputs mhw_ngx_startup mhw_temporal_state)
    target_compile_options(mhw_dlaa_inline_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    set_target_properties(mhw_dlaa_inline_fixture PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(TARGET mhw_reflex_presentation)
        add_executable(mhw_reflex_temporal_fixture tools/dlaa_inline_fixture.cpp tests/ngx_startup_entry.asm)
        target_compile_definitions(mhw_reflex_temporal_fixture PRIVATE MHW_REFLEX_TEMPORAL_FIXTURE)
        target_link_libraries(mhw_reflex_temporal_fixture PRIVATE mhw_scaled_resolve mhw_scaled_effects mhw_temporal_gpu_inputs
            mhw_ngx_startup mhw_temporal_state mhw_reflex_presentation user32)
        target_compile_options(mhw_reflex_temporal_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
        set_target_properties(mhw_reflex_temporal_fixture PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endif()
    target_link_libraries(mhw_dlaa_live_fixture PRIVATE mhw_temporal_gpu_inputs mhw_dlss_sdk_live mhw_ngx_startup)
    target_compile_options(mhw_dlaa_live_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    set_target_properties(mhw_dlaa_live_fixture PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    get_target_property(temporal_probe_sources mhw_temporal_inputs_probe SOURCES)
    add_library(mhw_temporal_dlss_probe SHARED ${temporal_probe_sources})
    target_compile_features(mhw_temporal_dlss_probe PRIVATE cxx_std_20)
    target_include_directories(mhw_temporal_dlss_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(mhw_temporal_dlss_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_TEMPORAL_DLSS MHW_TEMPORAL_LONG_HISTORY)
    target_link_libraries(mhw_temporal_dlss_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_temporal_gpu_inputs mhw_dlss_sdk_live mhw_ngx_startup)
    target_compile_options(mhw_temporal_dlss_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_temporal_dlss_probe PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(mhw_temporal_dlss_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_library(mhw_temporal_state_probe SHARED ${temporal_probe_sources})
    target_compile_features(mhw_temporal_state_probe PRIVATE cxx_std_20)
    target_include_directories(mhw_temporal_state_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(mhw_temporal_state_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_TEMPORAL_DLSS MHW_TEMPORAL_LONG_HISTORY MHW_TEMPORAL_PRESENT_STATE)
    target_link_libraries(mhw_temporal_state_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_temporal_gpu_inputs mhw_dlss_sdk_live mhw_ngx_startup mhw_temporal_state)
    target_compile_options(mhw_temporal_state_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_temporal_state_probe PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(mhw_temporal_state_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_library(mhw_temporal_inline_probe SHARED ${temporal_probe_sources} src/temporal_bindings/inline_pass.cpp)
    target_compile_features(mhw_temporal_inline_probe PRIVATE cxx_std_20)
    target_include_directories(mhw_temporal_inline_probe PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(mhw_temporal_inline_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_TEMPORAL_DLSS MHW_TEMPORAL_LONG_HISTORY MHW_TEMPORAL_PRESENT_STATE MHW_TEMPORAL_INLINE)
    target_link_libraries(mhw_temporal_inline_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_temporal_gpu_inputs mhw_dlss_sdk_live mhw_ngx_startup mhw_temporal_state mhw_dlss_inline)
    target_compile_options(mhw_temporal_inline_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_temporal_inline_probe PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(mhw_temporal_inline_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    set(native_activation_sources src/continuous_dlaa/activation.cpp src/continuous_dlaa/activation_entry.asm)
    add_library(mhw_continuous_dlaa SHARED src/continuous_dlaa/dllmain.cpp src/continuous_dlaa/runtime.cpp ${native_activation_sources}
        src/temporal_bindings/inputs.cpp src/temporal_bindings/shader.cpp src/temporal_bindings/depth_route_contract.cpp
        src/temporal_bindings/temporal_entry.asm src/temporal_bindings/depth_route_entry.asm src/temporal_bindings/jitter_entry.asm ${binding_common})
    target_compile_features(mhw_continuous_dlaa PRIVATE cxx_std_20)
    target_include_directories(mhw_continuous_dlaa PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(mhw_continuous_dlaa PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(mhw_continuous_dlaa PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_ngx_startup mhw_temporal_state mhw_native_taa_resolve)
    target_compile_options(mhw_continuous_dlaa PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_continuous_dlaa PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(mhw_continuous_dlaa PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    get_target_property(continuous_scaled_sources mhw_continuous_dlaa SOURCES)
    add_library(mhw_continuous_dlss SHARED ${continuous_scaled_sources}
        src/scaled_temporal/stream_route.cpp src/scaled_temporal/stream_entry.asm
        src/render_resolution/runtime.cpp src/render_resolution/resolution_entry.asm)
    target_compile_features(mhw_continuous_dlss PRIVATE cxx_std_20)
    target_include_directories(mhw_continuous_dlss PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(mhw_continuous_dlss PRIVATE MHW_CONTINUOUS_SCALED WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(mhw_continuous_dlss PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_ngx_startup mhw_temporal_state mhw_native_taa_resolve mhw_scaled_effects mhw_scaled_resolve)
    target_compile_options(mhw_continuous_dlss PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(mhw_continuous_dlss PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(mhw_continuous_dlss PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    if(TARGET mhw_reflex_presentation)
        foreach(temporal_mode IN ITEMS dlaa dlss)
            get_target_property(coexistence_sources mhw_continuous_${temporal_mode} SOURCES)
            list(REMOVE_ITEM coexistence_sources src/continuous_dlaa/dllmain.cpp)
            add_library(mhw_reflex_${temporal_mode}_probe SHARED ${coexistence_sources}
                src/reflex/presentation_probe.cpp src/reflex/live_frames.cpp
                src/reflex/frame_boundaries_entry.asm src/reflex/temporal_coexistence.cpp)
            target_compile_features(mhw_reflex_${temporal_mode}_probe PRIVATE cxx_std_20)
            target_include_directories(mhw_reflex_${temporal_mode}_probe PRIVATE src/projection_binding src/bridge_probe
                "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
            target_compile_definitions(mhw_reflex_${temporal_mode}_probe PRIVATE MHW_REFLEX_FRAME_PROBE MHW_REFLEX_TEMPORAL
                WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
            target_link_libraries(mhw_reflex_${temporal_mode}_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid
                mhw_ngx_startup mhw_temporal_state mhw_native_taa_resolve mhw_reflex_presentation)
            if(temporal_mode STREQUAL "dlss")
                target_compile_definitions(mhw_reflex_${temporal_mode}_probe PRIVATE MHW_CONTINUOUS_SCALED)
                target_link_libraries(mhw_reflex_${temporal_mode}_probe PRIVATE mhw_scaled_effects mhw_scaled_resolve)
            endif()
            target_compile_options(mhw_reflex_${temporal_mode}_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
            target_link_options(mhw_reflex_${temporal_mode}_probe PRIVATE /guard:cf /OPT:NOICF)
            set_target_properties(mhw_reflex_${temporal_mode}_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        endforeach()
    endif()
    if(TARGET mhw_reflex_dlss_probe)
        get_target_property(fg_ownership_sources mhw_reflex_dlss_probe SOURCES)
        add_library(mhw_frame_generation_ownership_probe SHARED ${fg_ownership_sources})
        target_compile_features(mhw_frame_generation_ownership_probe PRIVATE cxx_std_20)
        target_include_directories(mhw_frame_generation_ownership_probe PRIVATE src/projection_binding src/bridge_probe
            "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
        target_compile_definitions(mhw_frame_generation_ownership_probe PRIVATE MHW_REFLEX_FRAME_PROBE MHW_REFLEX_TEMPORAL
            MHW_CONTINUOUS_SCALED MHW_REFLEX_OWNERSHIP_PROBE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        target_link_libraries(mhw_frame_generation_ownership_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid
            mhw_ngx_startup mhw_temporal_state mhw_native_taa_resolve mhw_reflex_presentation mhw_scaled_effects mhw_scaled_resolve)
        target_compile_options(mhw_frame_generation_ownership_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/external:W0;/guard:cf>)
        target_link_options(mhw_frame_generation_ownership_probe PRIVATE /guard:cf /OPT:NOICF)
        set_target_properties(mhw_frame_generation_ownership_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        # Opt-in metadata variant; default observer targets retain their behavior.
        add_library(mhw_fg_temporal_state STATIC src/temporal_present/observer.cpp)
        target_compile_features(mhw_fg_temporal_state PUBLIC cxx_std_20)
        target_compile_definitions(mhw_fg_temporal_state PRIVATE MHW_FG_SCENE_PROBE)
        target_compile_options(mhw_fg_temporal_state PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
        target_link_libraries(mhw_fg_temporal_state PUBLIC mhw_temporal_present minhook)
        # Scene-only presentation variant: CPU swap-chain buffer labels/indices.
        add_library(mhw_fg_reflex_presentation STATIC src/reflex/presentation.cpp)
        target_compile_definitions(mhw_fg_reflex_presentation PRIVATE MHW_FG_SCENE_PROBE)
        target_link_libraries(mhw_fg_reflex_presentation PUBLIC mhw_streamline_reflex minhook d3d12 dxgi dxguid)
        target_compile_options(mhw_fg_reflex_presentation PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /external:W0 /guard:cf)
        add_library(mhw_frame_generation_scene_probe SHARED ${fg_ownership_sources})
        target_compile_features(mhw_frame_generation_scene_probe PRIVATE cxx_std_20)
        target_include_directories(mhw_frame_generation_scene_probe PRIVATE src/projection_binding src/bridge_probe
            "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
        target_compile_definitions(mhw_frame_generation_scene_probe PRIVATE MHW_REFLEX_FRAME_PROBE MHW_REFLEX_TEMPORAL
            MHW_CONTINUOUS_SCALED MHW_REFLEX_OWNERSHIP_PROBE MHW_FG_SCENE_PROBE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        target_link_libraries(mhw_frame_generation_scene_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid
            mhw_ngx_startup mhw_fg_temporal_state mhw_native_taa_resolve mhw_fg_reflex_presentation mhw_scaled_effects mhw_scaled_resolve)
        target_compile_options(mhw_frame_generation_scene_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/external:W0;/guard:cf>)
        target_link_options(mhw_frame_generation_scene_probe PRIVATE /guard:cf /OPT:NOICF)
        set_target_properties(mhw_frame_generation_scene_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        if(BUILD_TESTING)
            add_executable(frame_generation_scene_trace_tests tests/frame_generation_scene_trace_tests.cpp)
            target_compile_features(frame_generation_scene_trace_tests PRIVATE cxx_std_20)
            target_include_directories(frame_generation_scene_trace_tests PRIVATE src)
            target_compile_options(frame_generation_scene_trace_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
            add_test(NAME frame_generation_scene_trace COMMAND frame_generation_scene_trace_tests)
        endif()
    endif()
    if(BUILD_TESTING)
        add_executable(scaled_runtime_route_fixture tests/scaled_stream_route_fixture.cpp ${native_activation_sources}
            src/scaled_temporal/stream_entry.asm src/render_resolution/runtime.cpp src/render_resolution/resolution_entry.asm
            src/temporal_bindings/inputs.cpp src/temporal_bindings/shader.cpp src/temporal_bindings/depth_route_contract.cpp
            src/temporal_bindings/temporal_entry.asm src/temporal_bindings/depth_route_entry.asm src/temporal_bindings/jitter_entry.asm ${binding_common})
        target_compile_features(scaled_runtime_route_fixture PRIVATE cxx_std_20)
        target_include_directories(scaled_runtime_route_fixture PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
        target_compile_definitions(scaled_runtime_route_fixture PRIVATE MHW_STREAM_RUNTIME_FIXTURE MHW_CONTINUOUS_SCALED MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        target_link_libraries(scaled_runtime_route_fixture PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_ngx_startup mhw_temporal_state mhw_native_taa_resolve mhw_scaled_effects mhw_scaled_resolve)
        target_compile_options(scaled_runtime_route_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
        set_target_properties(scaled_runtime_route_fixture PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        add_executable(native_activation_tests tests/native_activation_tests.cpp tests/native_activation_fixture.asm
            src/continuous_dlaa/activation_entry.asm ${binding_common})
        target_compile_features(native_activation_tests PRIVATE cxx_std_20)
        target_include_directories(native_activation_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
        target_compile_definitions(native_activation_tests PRIVATE MHW_CONTINUOUS_SCALED WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
        target_link_libraries(native_activation_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
        target_compile_options(native_activation_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
        set_target_properties(native_activation_tests PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        add_test(NAME native_activation_contract COMMAND native_activation_tests)
        add_executable(continuous_runtime_tests tests/continuous_runtime_tests.cpp ${native_activation_sources}
            src/temporal_bindings/inputs.cpp src/temporal_bindings/shader.cpp src/temporal_bindings/depth_route_contract.cpp
            src/temporal_bindings/temporal_entry.asm src/temporal_bindings/depth_route_entry.asm src/temporal_bindings/jitter_entry.asm ${binding_common})
        target_compile_features(continuous_runtime_tests PRIVATE cxx_std_20)
        target_include_directories(continuous_runtime_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
        target_compile_definitions(continuous_runtime_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_TEMPORAL_FIXTURE)
        target_link_libraries(continuous_runtime_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid mhw_ngx_startup mhw_temporal_state mhw_native_taa_resolve)
        target_compile_options(continuous_runtime_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
        set_target_properties(continuous_runtime_tests PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        foreach(mode continuous jitter_exception jitter_stop jitter_provider jitter_matrix jitter_false_return queues early-reset duplicate pending-retirement unfiltered-copy reconstruction state-trace native-state-trace native-state-admission depth-eligibility depth-destination shutdown-success shutdown-partial
            requests-consumed requests-same-frame requests-wrap requests-blocked requests-unmatched requests-multiple requests-changed requests-reentrant requests-depth resolve-forward
            forwarded-wrap forwarded-overlap forwarded-early-reuse forwarded-stop forwarded-exception
            forwarded-changed-before forwarded-changed-after forwarded-stale-token forwarded-ambiguous forwarded-unmatched forwarded-reentrant)
            add_test(NAME temporal_continuous_${mode} COMMAND continuous_runtime_tests ${mode})
        endforeach()
    endif()
endif()

if(BUILD_TESTING)
    add_executable(native_taa_resolve_tests tests/native_taa_resolve_tests.cpp)
    target_include_directories(native_taa_resolve_tests PRIVATE src/projection_binding src/bridge_probe)
    target_compile_features(native_taa_resolve_tests PRIVATE cxx_std_20)
    target_compile_definitions(native_taa_resolve_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(native_taa_resolve_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME native_taa_resolve COMMAND native_taa_resolve_tests)
    add_executable(scaled_stream_lifetime_tests tests/scaled_stream_lifetime_tests.cpp)
    target_compile_features(scaled_stream_lifetime_tests PRIVATE cxx_std_20)
    target_compile_definitions(scaled_stream_lifetime_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(scaled_stream_lifetime_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME scaled_stream_lifetime COMMAND scaled_stream_lifetime_tests)
    add_executable(recording_states_tests tests/recording_states_tests.cpp)
    target_compile_features(recording_states_tests PRIVATE cxx_std_20)
    target_compile_options(recording_states_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME scaled_recording_states COMMAND recording_states_tests)
    add_executable(compute_resource_trace_tests tests/compute_resource_trace_tests.cpp)
    target_compile_features(compute_resource_trace_tests PRIVATE cxx_std_20)
    target_compile_options(compute_resource_trace_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME compute_resource_trace COMMAND compute_resource_trace_tests)
    add_executable(dependency_binding_trace_tests tests/dependency_binding_trace_tests.cpp)
    target_compile_features(dependency_binding_trace_tests PRIVATE cxx_std_20)
    target_compile_options(dependency_binding_trace_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME dependency_binding_trace COMMAND dependency_binding_trace_tests)
    add_executable(continuous_retention_tests tests/continuous_retention_tests.cpp)
    add_executable(aa_policy_tests tests/aa_policy_tests.cpp)
    target_compile_features(aa_policy_tests PRIVATE cxx_std_20)
    target_compile_options(aa_policy_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME temporal_aa_policy COMMAND aa_policy_tests)
    add_executable(post_queue_tests tests/post_queue_tests.cpp src/aa_control/post_queue.cpp)
    target_compile_features(post_queue_tests PRIVATE cxx_std_20)
    target_include_directories(post_queue_tests PRIVATE src/bridge_probe)
    target_compile_options(post_queue_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME aa_post_queue_contract COMMAND post_queue_tests)
    add_executable(owned_pass_tests tests/owned_pass_tests.cpp src/aa_control/owned_pass.cpp src/aa_control/post_queue.cpp)
    target_compile_features(owned_pass_tests PRIVATE cxx_std_20)
    target_include_directories(owned_pass_tests PRIVATE src/bridge_probe)
    target_compile_options(owned_pass_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME aa_owned_pass_contract COMMAND owned_pass_tests)

    add_executable(deferred_contract_tests tests/deferred_contract_tests.cpp)
    target_compile_features(deferred_contract_tests PRIVATE cxx_std_20)
    target_compile_options(deferred_contract_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME aa_deferred_contract COMMAND deferred_contract_tests)

    add_executable(aa_gpu_state_tests tests/aa_gpu_state_tests.cpp)
    target_compile_features(aa_gpu_state_tests PRIVATE cxx_std_20)
    target_compile_options(aa_gpu_state_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME aa_gpu_state_contract COMMAND aa_gpu_state_tests)

    add_executable(aa_gpu_trace_tests tests/aa_gpu_trace_tests.cpp src/aa_control/gpu_trace.cpp src/temporal_bindings/submission.cpp ${binding_common})
    target_compile_features(aa_gpu_trace_tests PRIVATE cxx_std_20)
    target_include_directories(aa_gpu_trace_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(aa_gpu_trace_tests PRIVATE MHW_AA_GPU_TRACE MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(aa_gpu_trace_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
    target_compile_options(aa_gpu_trace_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(aa_gpu_trace_tests PRIVATE /guard:cf /OPT:NOICF)
    foreach(case success missing_after_reset no_retire signal_failure)
        add_test(NAME aa_gpu_trace_${case} COMMAND aa_gpu_trace_tests ${case})
        set_tests_properties(aa_gpu_trace_${case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(aa_input_readback_tests tests/aa_input_readback_tests.cpp src/aa_control/gpu_trace.cpp
        src/aa_control/input_readback.cpp src/temporal_bindings/submission.cpp ${binding_common})
    target_compile_features(aa_input_readback_tests PRIVATE cxx_std_20)
    target_include_directories(aa_input_readback_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(aa_input_readback_tests PRIVATE MHW_AA_INPUT_READBACK MHW_AA_GPU_TRACE MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(aa_input_readback_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
    target_compile_options(aa_input_readback_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(aa_input_readback_tests PRIVATE /guard:cf /OPT:NOICF)
    foreach(case success missing_depth wrong_pair no_retire signal_failure)
        add_test(NAME aa_input_readback_${case} COMMAND aa_input_readback_tests ${case})
        set_tests_properties(aa_input_readback_${case} PROPERTIES TIMEOUT 30)
    endforeach()
    get_target_property(aa_binding_sources aa_input_readback_tests SOURCES)
    add_executable(aa_binding_readback_tests ${aa_binding_sources})
    target_compile_features(aa_binding_readback_tests PRIVATE cxx_std_20)
    target_include_directories(aa_binding_readback_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(aa_binding_readback_tests PRIVATE MHW_AA_INPUT_READBACK MHW_AA_GPU_TRACE MHW_AA_BINDINGS MHW_TEMPORAL_PRESENT_STATE MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(aa_binding_readback_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid d3dcompiler mhw_temporal_state)
    target_compile_options(aa_binding_readback_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(aa_binding_readback_tests PRIVATE /guard:cf /OPT:NOICF)
    foreach(case success missing_depth wrong_pair no_retire signal_failure missing_compute_root missing_bindings_generation
        trailing_traffic delayed_gpu late_disable pending_close early_cutoff active_overflow)
        add_test(NAME aa_binding_readback_${case} COMMAND aa_binding_readback_tests ${case})
        set_tests_properties(aa_binding_readback_${case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(owned_queue_scope_tests tests/owned_queue_scope_tests.cpp src/aa_control/owned_runtime.cpp
        src/aa_control/owned_pass.cpp src/aa_control/post_queue.cpp src/temporal_bindings/inputs.cpp src/bridge_probe/bounded_scalar_read.cpp)
    target_compile_features(owned_queue_scope_tests PRIVATE cxx_std_20)
    target_include_directories(owned_queue_scope_tests PRIVATE src/bridge_probe)
    target_compile_definitions(owned_queue_scope_tests PRIVATE MHW_AA_OWNED_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(owned_queue_scope_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    target_link_libraries(owned_queue_scope_tests PRIVATE d3d12 dxguid)
    add_test(NAME aa_owned_queue_scope COMMAND owned_queue_scope_tests)
    add_executable(post_queue_abi_tests tests/post_queue_abi_tests.cpp tests/post_queue_entry.asm
        src/aa_control/schedule_entry.asm src/projection_binding/entry.asm)
    target_compile_features(post_queue_abi_tests PRIVATE cxx_std_20)
    target_include_directories(post_queue_abi_tests PRIVATE src/bridge_probe)
    target_compile_definitions(post_queue_abi_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(post_queue_abi_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    add_test(NAME aa_post_queue_abi COMMAND post_queue_abi_tests)
    target_compile_features(continuous_retention_tests PRIVATE cxx_std_20)
    target_compile_definitions(continuous_retention_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(continuous_retention_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME temporal_continuous_retention COMMAND continuous_retention_tests)
    add_executable(continuous_history_tests tests/continuous_history_tests.cpp)
    target_compile_features(continuous_history_tests PRIVATE cxx_std_20)
    target_compile_options(continuous_history_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME temporal_continuous_history COMMAND continuous_history_tests)
    add_executable(history_comparison_tests tests/history_comparison_tests.cpp)
    target_compile_features(history_comparison_tests PRIVATE cxx_std_20)
    target_compile_options(history_comparison_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME temporal_history_comparison COMMAND history_comparison_tests)
    add_executable(temporal_present_bindings_tests tests/temporal_present_bindings_tests.cpp)
    add_executable(mhw_temporal_present_fixture tools/temporal_present_fixture.cpp)
    target_link_libraries(mhw_temporal_present_fixture PRIVATE mhw_temporal_state)
    foreach(t temporal_present_bindings_tests mhw_temporal_present_fixture)
        target_link_libraries(${t} PRIVATE mhw_temporal_present)
        target_compile_options(${t} PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
        set_target_properties(${t} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endforeach()
    add_test(NAME temporal_present_bindings COMMAND temporal_present_bindings_tests)
    add_test(NAME temporal_present_warp COMMAND mhw_temporal_present_fixture --warp)
    add_test(NAME temporal_spatial_warp COMMAND mhw_temporal_present_fixture --spatial)
    add_test(NAME temporal_spatial_fractional_warp COMMAND mhw_temporal_present_fixture --spatial-fractional)
    add_test(NAME temporal_spatial_identity_warp COMMAND mhw_temporal_present_fixture --spatial-identity)
    set_tests_properties(temporal_present_warp PROPERTIES TIMEOUT 30)
    add_executable(ngx_readiness_tests tests/ngx_readiness_tests.cpp)
    target_compile_features(ngx_readiness_tests PRIVATE cxx_std_20)
    target_compile_options(ngx_readiness_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(ngx_readiness_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME ngx_readiness COMMAND ngx_readiness_tests)
    add_executable(dlss_sdk_contract_tests tests/dlss_sdk_contract_tests.cpp)
    target_compile_features(dlss_sdk_contract_tests PRIVATE cxx_std_20)
    target_compile_options(dlss_sdk_contract_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(dlss_sdk_contract_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME dlss_sdk_contract COMMAND dlss_sdk_contract_tests)

    add_executable(temporal_gpu_contract_tests tests/temporal_gpu_contract_tests.cpp)
    target_link_libraries(temporal_gpu_contract_tests PRIVATE mhw_temporal_gpu_inputs)
    target_compile_options(temporal_gpu_contract_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    set_target_properties(temporal_gpu_contract_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME temporal_gpu_contract COMMAND temporal_gpu_contract_tests)
    add_test(NAME temporal_gpu_typed_warp COMMAND mhw_temporal_input_replay --self-test --warp)
    set_tests_properties(temporal_gpu_typed_warp PROPERTIES TIMEOUT 30)
    add_executable(temporal_inputs_tests tests/temporal_inputs_tests.cpp tests/temporal_fixture.asm
        src/temporal_bindings/inputs.cpp src/temporal_bindings/shader.cpp src/temporal_bindings/depth_route_contract.cpp src/projection_binding/policy.cpp src/temporal_bindings/temporal_entry.asm src/projection_binding/entry.asm)
    target_compile_features(temporal_inputs_tests PRIVATE cxx_std_20)
    target_include_directories(temporal_inputs_tests PRIVATE src/bridge_probe)
    target_compile_options(temporal_inputs_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8>)
    set_target_properties(temporal_inputs_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME temporal_inputs_contract COMMAND temporal_inputs_tests)

    add_executable(temporal_probe_tests tests/temporal_probe_tests.cpp src/temporal_bindings/inputs.cpp src/temporal_bindings/submission.cpp src/temporal_bindings/motion_readback.cpp src/temporal_bindings/jitter.cpp src/temporal_bindings/jitter_entry.asm
        src/temporal_bindings/shader.cpp src/temporal_bindings/consumer.cpp src/temporal_bindings/probe.cpp src/temporal_bindings/temporal_entry.asm ${temporal_depth_sources} ${binding_common})
    target_compile_features(temporal_probe_tests PRIVATE cxx_std_20)
    target_include_directories(temporal_probe_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(temporal_probe_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_TEMPORAL_FIXTURE)
    target_link_libraries(temporal_probe_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid d3dcompiler mhw_temporal_gpu_inputs)
    target_compile_options(temporal_probe_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(temporal_probe_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(temporal_probe_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    get_target_property(scaled_fixture_sources temporal_probe_tests SOURCES)
    add_executable(scaled_handoff_tests tests/scaled_handoff_tests.cpp tests/scaled_handoff_fixture.asm
        src/scaled_temporal/handoff.cpp src/scaled_temporal/downstream.cpp src/scaled_temporal/postfilter.cpp src/scaled_temporal/draw_execution.cpp src/scaled_temporal/handoff_entry.asm src/temporal_bindings/inputs.cpp ${binding_common})
    target_compile_features(scaled_handoff_tests PRIVATE cxx_std_20)
    target_include_directories(scaled_handoff_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(scaled_handoff_tests PRIVATE MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(scaled_handoff_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
    target_compile_options(scaled_handoff_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(scaled_handoff_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(scaled_handoff_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(case success exception stop nested opcode mode rectangle packet_source packet_destination extra_packet owner_change attachment_change frame_change queue_overflow foreign duplicate_publish source_identity wrong_frame alias draw_branch description)
        add_test(NAME scaled_handoff_${case} COMMAND scaled_handoff_tests ${case})
        set_tests_properties(scaled_handoff_${case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(scaled_downstream_tests tests/scaled_downstream_tests.cpp tests/scaled_downstream_fixture.asm
        src/scaled_temporal/downstream.cpp src/scaled_temporal/handoff.cpp src/scaled_temporal/postfilter.cpp src/scaled_temporal/draw_execution.cpp src/scaled_temporal/handoff_entry.asm src/temporal_bindings/inputs.cpp ${binding_common})
    target_compile_features(scaled_downstream_tests PRIVATE cxx_std_20)
    target_include_directories(scaled_downstream_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(scaled_downstream_tests PRIVATE MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(scaled_downstream_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
    target_compile_options(scaled_downstream_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(scaled_downstream_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(scaled_downstream_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(case success exception stop nested overflow viewport_change frame_change attachment_change foreign unreadable triangle late_frame owner_change packet_change key_change filter_overflow wrong_destination missing_taa thread)
        add_test(NAME scaled_downstream_${case} COMMAND scaled_downstream_tests ${case})
        set_tests_properties(scaled_downstream_${case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(scaled_postfilter_tests tests/scaled_postfilter_tests.cpp tests/scaled_postfilter_fixture.asm
        src/scaled_temporal/downstream.cpp src/scaled_temporal/handoff.cpp src/scaled_temporal/postfilter.cpp src/scaled_temporal/draw_execution.cpp src/scaled_temporal/handoff_entry.asm src/temporal_bindings/inputs.cpp ${binding_common})
    target_compile_features(scaled_postfilter_tests PRIVATE cxx_std_20)
    target_include_directories(scaled_postfilter_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(scaled_postfilter_tests PRIVATE MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(scaled_postfilter_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid d3dcompiler)
    target_compile_options(scaled_postfilter_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(scaled_postfilter_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(scaled_postfilter_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(case success exception stop nested overflow viewport_change frame_change vertices_change owner_change foreign unreadable stride shader_size no_draw thread grade_missing grade_duplicate grade_endpoint grade_owner grade_vertices publish_draws)
        add_test(NAME scaled_postfilter_${case} COMMAND scaled_postfilter_tests ${case})
        set_tests_properties(scaled_postfilter_${case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(scaled_draw_execution_tests tests/scaled_draw_execution_tests.cpp
        src/scaled_temporal/draw_execution.cpp src/scaled_temporal/handoff_entry.asm
        src/scaled_temporal/handoff.cpp src/scaled_temporal/downstream.cpp src/scaled_temporal/postfilter.cpp
        src/temporal_bindings/inputs.cpp ${binding_common})
    target_compile_features(scaled_draw_execution_tests PRIVATE cxx_std_20)
    target_include_directories(scaled_draw_execution_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(scaled_draw_execution_tests PRIVATE MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(scaled_draw_execution_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid d3dcompiler)
    target_compile_options(scaled_draw_execution_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(scaled_draw_execution_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(scaled_draw_execution_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(case success exception stop nested cache changed shader_change packet_change device resource arguments shader_size duplicate ambiguous layout_change layout_budget layout_unreadable extra_stage)
        add_test(NAME scaled_draw_execution_${case} COMMAND scaled_draw_execution_tests ${case})
        set_tests_properties(scaled_draw_execution_${case} PROPERTIES TIMEOUT 20)
    endforeach()
    list(REMOVE_ITEM scaled_fixture_sources tests/temporal_probe_tests.cpp)
    add_executable(scaled_temporal_fixture tests/scaled_temporal_fixture.cpp ${scaled_fixture_sources}
        src/scaled_temporal/handoff.cpp src/scaled_temporal/downstream.cpp src/scaled_temporal/postfilter.cpp src/scaled_temporal/draw_execution.cpp src/scaled_temporal/handoff_entry.asm)
    target_compile_features(scaled_temporal_fixture PRIVATE cxx_std_20)
    target_include_directories(scaled_temporal_fixture PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(scaled_temporal_fixture PRIVATE MHW_TEMPORAL_SCALED MHW_TEMPORAL_FIXTURE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(scaled_temporal_fixture PRIVATE minhook bcrypt d3d12 dxgi dxguid d3dcompiler mhw_temporal_gpu_inputs)
    target_compile_options(scaled_temporal_fixture PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(scaled_temporal_fixture PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(scaled_temporal_fixture PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(case owned_success owned_wrong_state owned_no_retire owned_signal_failure draw_success draw_multi draw_pixel draw_pixel_multi draw_admission draw_admission_multi draw_admission_stale draw_admission_alias draw_caller draw_arguments draw_changed draw_nested draw_no_retire draw_signal_failure jdraw_pixel_multi jdraw_admission_multi hdraw_pixel_multi hdraw_admission_multi)
        add_test(NAME scaled_temporal_${case} COMMAND scaled_temporal_fixture ${case})
        set_tests_properties(scaled_temporal_${case} PROPERTIES TIMEOUT 20)
    endforeach()
    foreach(case success changed opcode exception foreign timeout slow reentrant duplicate shader_changed recipe_changed wrong_stage unreadable_shader shader_budget bad_binding
        consumer_success consumer_unknown consumer_split consumer_alias consumer_conflict consumer_changed consumer_wrong_call consumer_wrong_groups
        consumer_duplicate consumer_no_dispatch consumer_exception consumer_api_exception consumer_barrier_change consumer_overflow consumer_reentrant
        submission_success submission_hooks submission_two_queues submission_signal_failure submission_duplicate submission_no_reset
        submission_no_close submission_double_close submission_early_reset submission_no_retire submission_unpolled submission_late_completion submission_marker_reasons
        motion_success motion_hooks motion_wrong_state motion_wrong_format motion_no_retire motion_map_failure
        pair_success pair_hooks pair_wrong_state pair_wrong_format pair_alias pair_no_retire pair_map_failure pair_signal_failure pair_frame_skip jpair_success jpair_mismatch jpair_delay
        scene_success scene_state scene_no_attachment scene_missing scene_producer_changed scene_consumer_changed scene_dispatch_changed
        route_success route_multi route_wrong_call route_exception route_frame_changed route_packet_changed
        depth_success depth_multi depth_wrong_state depth_duplicate depth_no_selection depth_no_retire depth_map_failure
        owned_success owned_multi owned_wrong_state owned_duplicate owned_no_selection owned_no_retire owned_map_failure
        jowned_success jowned_multi jowned_wrong_state jowned_duplicate jowned_no_selection jowned_no_retire jowned_map_failure)
        add_test(NAME temporal_probe_${case} COMMAND temporal_probe_tests ${case})
        set_tests_properties(temporal_probe_${case} PROPERTIES TIMEOUT 20)
    endforeach()
endif()
if(BUILD_TESTING)
    add_executable(temporal_jitter_tests tests/temporal_jitter_tests.cpp src/temporal_bindings/jitter.cpp src/temporal_bindings/jitter_entry.asm
        src/temporal_bindings/submission.cpp src/temporal_bindings/motion_readback.cpp ${binding_common})
    target_compile_features(temporal_jitter_tests PRIVATE cxx_std_20)
    target_include_directories(temporal_jitter_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(temporal_jitter_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_TEMPORAL_FIXTURE)
    target_link_libraries(temporal_jitter_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
    target_compile_options(temporal_jitter_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(temporal_jitter_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(temporal_jitter_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    get_target_property(aa_jitter_sources temporal_jitter_tests SOURCES)
    add_executable(aa_projection_tests ${aa_jitter_sources})
    target_compile_features(aa_projection_tests PRIVATE cxx_std_20)
    target_include_directories(aa_projection_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(aa_projection_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_TEMPORAL_FIXTURE MHW_AA_GPU_TRACE MHW_AA_PROJECTION)
    target_link_libraries(aa_projection_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
    target_compile_options(aa_projection_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(aa_projection_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(aa_projection_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(case success exception stop provider competing_write reentrant matrix false_return frame_skip nonzero table timeout foreign match unmatched duplicate_ack
        selection_delay selection_threshold selection_limit selection_changed link_frame link_epoch link_bank link_async link_context link_view)
        add_test(NAME aa_projection_${case} COMMAND aa_projection_tests jitter_${case})
        set_tests_properties(aa_projection_${case} PROPERTIES TIMEOUT 20)
    endforeach()
    foreach(case success exception stop provider competing_write reentrant matrix false_return frame_skip nonzero table timeout foreign match unmatched duplicate_ack
        selection_delay selection_xsmall selection_ysmall selection_threshold selection_invalid selection_unreadable selection_limit selection_last selection_changed scaled_render scaled_allocation scope_wait scope_lost)
        add_test(NAME temporal_jitter_${case} COMMAND temporal_jitter_tests jitter_${case})
        set_tests_properties(temporal_jitter_${case} PROPERTIES TIMEOUT 20)
    endforeach()
    foreach(kind probe jitter)
        get_target_property(long_sources temporal_${kind}_tests SOURCES)
        add_executable(temporal_long_${kind}_tests ${long_sources})
        target_compile_features(temporal_long_${kind}_tests PRIVATE cxx_std_20)
        target_include_directories(temporal_long_${kind}_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
        target_compile_definitions(temporal_long_${kind}_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_TEMPORAL_FIXTURE MHW_TEMPORAL_LONG_HISTORY)
        target_link_libraries(temporal_long_${kind}_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid d3dcompiler mhw_temporal_gpu_inputs)
        target_compile_options(temporal_long_${kind}_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
        target_link_options(temporal_long_${kind}_tests PRIVATE /guard:cf /OPT:NOICF)
        set_target_properties(temporal_long_${kind}_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endforeach()
    foreach(case jowned_success jowned_multi jowned_wrong_state jowned_duplicate jowned_no_selection jowned_no_retire jowned_map_failure
        jowned_reuse jowned_reuse_duplicate jowned_reuse_unconsumed jowned_reuse_active jowned_reuse_epoch jowned_reuse_frame jowned_reuse_overlap
        jowned_cancel_marker jowned_cancel_drain jowned_cancel_exception jowned_cancel_timeout jowned_marker_closed
        jowned_shutdown_traffic jowned_cancel_traffic jowned_shutdown_pending)
        add_test(NAME temporal_long_${case} COMMAND temporal_long_probe_tests ${case})
        set_tests_properties(temporal_long_${case} PROPERTIES TIMEOUT 30)
    endforeach()
    get_target_property(state_fixture_sources temporal_probe_tests SOURCES)
    add_executable(temporal_state_probe_tests ${state_fixture_sources})
    target_compile_features(temporal_state_probe_tests PRIVATE cxx_std_20)
    target_include_directories(temporal_state_probe_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(temporal_state_probe_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_TEMPORAL_FIXTURE MHW_TEMPORAL_LONG_HISTORY MHW_TEMPORAL_PRESENT_STATE)
    target_link_libraries(temporal_state_probe_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid d3dcompiler mhw_temporal_gpu_inputs mhw_temporal_state)
    target_compile_options(temporal_state_probe_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(temporal_state_probe_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(temporal_state_probe_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(case jowned_success jowned_reuse jowned_shutdown_pending jowned_inline)
        add_test(NAME temporal_state_${case} COMMAND temporal_state_probe_tests ${case})
        set_tests_properties(temporal_state_${case} PROPERTIES TIMEOUT 30)
    endforeach()
    foreach(case success late_exception late_duplicate stop frame_skip unmatched)
        add_test(NAME temporal_long_jitter_${case} COMMAND temporal_long_jitter_tests jitter_${case})
        set_tests_properties(temporal_long_jitter_${case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(projection_depth_readback_tests tests/depth_readback_tests.cpp src/projection_binding/depth_readback.cpp)
    target_compile_features(projection_depth_readback_tests PRIVATE cxx_std_20)
    target_include_directories(projection_depth_readback_tests PRIVATE src/projection_binding)
    target_compile_definitions(projection_depth_readback_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_BINDING_FIXTURE)
    target_link_libraries(projection_depth_readback_tests PRIVATE d3d12 dxgi dxguid)
    target_compile_options(projection_depth_readback_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
    target_link_options(projection_depth_readback_tests PRIVATE /guard:cf)
    set_target_properties(projection_depth_readback_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(readback_case extent description identity pixels signal_failure wrong_generation cancel map_failure)
        add_test(NAME projection_depth_readback_${readback_case} COMMAND projection_depth_readback_tests ${readback_case})
        set_tests_properties(projection_depth_readback_${readback_case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(projection_provenance_tests tests/provenance_tests.cpp tests/projection_binding_fixture.asm ${binding_common})
    target_compile_features(projection_provenance_tests PRIVATE cxx_std_20)
    target_include_directories(projection_provenance_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(projection_provenance_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_BINDING_FIXTURE)
    target_link_libraries(projection_provenance_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
    target_compile_options(projection_provenance_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(projection_provenance_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(projection_provenance_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(provenance_case policy roundtrip prewatch reuse destroyed overwrite copy_simple copy_ranges copy_implicit
            readonly no_reset no_barrier split reset_after_barrier closed heap_changed device_changed expired
            changed exception pending capacity history_capacity pin_capacity concurrent deadline_slot
            submit_batch submit_reversed submit_reset submit_timeout
            submit_badcount submit_badread submit_duplicate submit_pending submit_exception
            sync_chain sync_cpu_signal sync_shared sync_incomplete sync_reuse sync_resubmit
            gpu_copy_whole gpu_copy_texture gpu_copy_footprint copy_bad_location copy_null_resource copy_exception copy_capacity copy_arguments
            copy_concurrent copy_concurrent_exception copy_merge_desc copy_merge_identity copy_merge_device copy_merge_capacity
            readback_pixels readback_twice readback_combined readback_view_changed readback_discard readback_resubmit
            readback_frame_pixels readback_frame_reuse readback_frame_stamp readback_frame_changed readback_frame_orphan
            readback_frame_capacity readback_frame_exception readback_frame_empty readback_frame_self
            readback_aligned_pixels readback_aligned_separate readback_aligned_wrong_frame readback_aligned_no_source readback_aligned_source_changed readback_aligned_view_changed
            readback_triplet_pixels readback_triplet_missing_baseline readback_triplet_wrong_recovery readback_triplet_view_changed readback_triplet_extra_copy
            readback_sequence_pixels readback_sequence_missing_baseline readback_sequence_wrong_recovery
            readback_sequence_view_changed readback_sequence_extra_copy readback_sequence_wrong_middle
            readback_sequence_history_pressure readback_sequence_history_overflow)
        add_test(NAME projection_provenance_${provenance_case} COMMAND projection_provenance_tests ${provenance_case})
        set_tests_properties(projection_provenance_${provenance_case} PROPERTIES TIMEOUT 25)
    endforeach()
    add_executable(projection_binding_tests tests/projection_binding_tests.cpp tests/projection_binding_fixture.asm ${binding_common})
    target_compile_features(projection_binding_tests PRIVATE cxx_std_20)
    target_include_directories(projection_binding_tests PRIVATE src/projection_binding src/bridge_probe "${minhook_SOURCE_DIR}/src/hde" "${minhook_SOURCE_DIR}/src")
    target_compile_definitions(projection_binding_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE MHW_BINDING_FIXTURE)
    target_link_libraries(projection_binding_tests PRIVATE minhook bcrypt d3d12 dxgi dxguid)
    target_compile_options(projection_binding_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(projection_binding_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(projection_binding_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(binding_case policy forwarding exception draw_exception chain cached changed foreign deadline stop_pending reuse unsafe_prefix
            unrelated_lock relevant_lock rollover_parallel pool_unhooked pool_exhausted pool_starved
            contract_policy matrix_layout matrix_nonfinite root_changed root_wrong matrix_id_changed
            jitter_chain jitter_recovery_first jitter_exception jitter_conflict jitter_provider jitter_exit_lock
            jitter_pending jitter_changed jitter_zero jitter_readonly jitter_recovery_changed
            jitter_deferred jitter_deferral_cap jitter_consume_collision
            jitter_sequence_chain jitter_sequence_deferred jitter_sequence_wait_cap jitter_sequence_repeat jitter_sequence_zero
            jitter_sequence_exception jitter_sequence_conflict jitter_sequence_provider jitter_sequence_stop jitter_sequence_recovery_changed
            attach_depth attach_color_depth attach_no_depth attach_null_color attach_cached attach_same_handles attach_created
            attach_interface attach_device attach_desc_changed attach_rotation attach_count attach_state_changed attach_pointer
            attach_reset attach_split attach_overflow attach_total_overflow attach_pso_changed attach_exception
            attach_create_iid attach_create_bytes attach_create_output attach_create_hr attach_create_changed
            attach_identity_adjusted attach_identity_mismatch attach_identity_roundtrip attach_identity_roundtrip_mismatch
            attach_identity_hr attach_identity_null attach_identity_self_hr attach_identity_roundtrip_hr attach_identity_exception
            attach_identity_draw_change attach_identity_return_change attach_identity_partial_fail resource_sdk)
        add_test(NAME projection_binding_${binding_case} COMMAND projection_binding_tests ${binding_case})
        set_tests_properties(projection_binding_${binding_case} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(jitter_writer_tests tests/jitter_writer_tests.cpp tests/jitter_writer_fixture.asm
        src/jitter_writer/recorder.cpp src/jitter_writer/entry.asm src/bridge_probe/bounded_scalar_read.cpp)
    target_compile_features(jitter_writer_tests PRIVATE cxx_std_20)
    target_include_directories(jitter_writer_tests PRIVATE src/jitter_writer src/bridge_probe)
    target_compile_definitions(jitter_writer_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX MHW_WRITER_OWNED_FIXTURE)
    target_link_libraries(jitter_writer_tests PRIVATE minhook bcrypt)
    if(MSVC)
        target_compile_options(jitter_writer_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
        target_link_options(jitter_writer_tests PRIVATE /guard:cf /OPT:NOICF)
    endif()
    set_target_properties(jitter_writer_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(writer_case roundtrip capacity deadline invalid read_failure
            track_roundtrip track_changed track_foreign track_bounds track_read_failure track_owner track_overflow)
        add_test(NAME jitter_writer_${writer_case} COMMAND jitter_writer_tests ${writer_case})
        set_tests_properties(jitter_writer_${writer_case} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(jitter_activation_tests tests/jitter_activation_tests.cpp tests/jitter_activation_fixture.asm
        src/jitter_activation/recorder.cpp src/jitter_activation/identity.cpp
        src/bridge_probe/projection_snapshot.cpp src/bridge_probe/bounded_scalar_read.cpp)
    target_compile_features(jitter_activation_tests PRIVATE cxx_std_20)
    target_include_directories(jitter_activation_tests PRIVATE src/jitter_activation src/bridge_probe)
    target_compile_definitions(jitter_activation_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX MHW_ACTIVATION_OWNED_FIXTURE)
    target_link_libraries(jitter_activation_tests PRIVATE minhook bcrypt)
    target_compile_options(jitter_activation_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(jitter_activation_tests PRIVATE /guard:cf /OPT:NOICF)
    set_target_properties(jitter_activation_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(activation_case normal baseline_mismatch exception conflict deadline invalid readonly concurrent changed_camera same_frame stop_pending wrapper_present sizing_enabled backend_absent wrapper_during_stimulus install_failure)
        add_test(NAME jitter_activation_${activation_case} COMMAND jitter_activation_tests ${activation_case})
        set_tests_properties(jitter_activation_${activation_case} PROPERTIES TIMEOUT 15)
    endforeach()
    add_executable(jitter_callbacks_tests tests/jitter_callbacks_tests.cpp tests/jitter_callbacks_fixture.asm
        src/jitter_callbacks/recorder.cpp src/jitter_callbacks/entry.asm src/bridge_probe/bounded_scalar_read.cpp)
    target_compile_features(jitter_callbacks_tests PRIVATE cxx_std_20)
    target_include_directories(jitter_callbacks_tests PRIVATE src/jitter_callbacks src/bridge_probe)
    target_compile_definitions(jitter_callbacks_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX MHW_JITTER_OWNED_FIXTURE)
    target_link_libraries(jitter_callbacks_tests PRIVATE minhook bcrypt)
    if(MSVC)
        target_compile_options(jitter_callbacks_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
        target_link_options(jitter_callbacks_tests PRIVATE /guard:cf /OPT:NOICF)
    endif()
    set_target_properties(jitter_callbacks_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME jitter_callbacks_tests COMMAND jitter_callbacks_tests)
    set_tests_properties(jitter_callbacks_tests PROPERTIES TIMEOUT 30)
    add_executable(depth_work_tests tests/depth_work_tests.cpp tests/depth_work_fixture.asm
        tests/native_taa_fallback_fixture.asm src/bridge_probe/native_depth_probe.cpp
        src/bridge_probe/native_depth_entry.asm src/bridge_probe/native_taa_fallback.cpp)
    target_compile_features(depth_work_tests PRIVATE cxx_std_20)
    target_compile_definitions(depth_work_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX MHW_OWNED_DEPTH_TEST MHW_OWNED_FALLBACK_TEST)
    target_link_libraries(depth_work_tests PRIVATE mhw_dlss_bridge_policy minhook bcrypt)
    if(MSVC)
        target_compile_options(depth_work_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
        target_link_options(depth_work_tests PRIVATE /guard:cf /OPT:NOICF)
    endif()
    set_target_properties(depth_work_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    foreach(scenario success bad-producer bad-preparation bad-consumer bad-frame-preparation bad-frame-consumer
        advanced-counters bad-publication early-preparation early-thread duplicate-publication missing-projection jitter-sequence)
        add_test(NAME depth_work_${scenario} COMMAND depth_work_tests ${scenario})
        set_tests_properties(depth_work_${scenario} PROPERTIES TIMEOUT 20)
    endforeach()
    add_executable(native_taa_fallback_tests
        src/bridge_probe/native_depth_probe.cpp
        src/bridge_probe/native_depth_entry.asm
        tests/native_taa_fallback_tests.cpp
        tests/native_taa_fallback_fixture.asm
        src/bridge_probe/native_taa_fallback.cpp)
    target_compile_features(native_taa_fallback_tests PRIVATE cxx_std_20)
    target_compile_definitions(native_taa_fallback_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX MHW_OWNED_FALLBACK_TEST)
    target_link_libraries(native_taa_fallback_tests PRIVATE mhw_dlss_bridge_policy minhook bcrypt)
    if(MSVC)
        target_compile_options(native_taa_fallback_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
        target_link_options(native_taa_fallback_tests PRIVATE /guard:cf /OPT:NOICF)
    endif()
    set_target_properties(native_taa_fallback_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME native_taa_fallback_tests COMMAND native_taa_fallback_tests)
    add_executable(depth_selection_tests tests/depth_selection_tests.cpp)
    target_compile_features(depth_selection_tests PRIVATE cxx_std_20)
    target_compile_definitions(depth_selection_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(depth_selection_tests PRIVATE mhw_dlss_bridge_policy bcrypt)
    if(MSVC)
        target_compile_options(depth_selection_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
        target_link_options(depth_selection_tests PRIVATE /guard:cf)
    endif()
    set_target_properties(depth_selection_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME depth_selection_tests COMMAND depth_selection_tests)
    add_executable(capture_mailbox_tests tests/capture_mailbox_tests.cpp)
    target_compile_features(capture_mailbox_tests PRIVATE cxx_std_20)
    target_compile_definitions(capture_mailbox_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(capture_mailbox_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(capture_mailbox_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
        target_link_options(capture_mailbox_tests PRIVATE /guard:cf)
    endif()
    set_target_properties(capture_mailbox_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME capture_mailbox_tests COMMAND capture_mailbox_tests)
    set_tests_properties(capture_mailbox_tests PROPERTIES TIMEOUT 30)
    add_executable(checkpoint_review_tests tests/checkpoint_review_tests.cpp)
    target_compile_features(checkpoint_review_tests PRIVATE cxx_std_20)
    target_compile_definitions(checkpoint_review_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(checkpoint_review_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(checkpoint_review_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
        target_link_options(checkpoint_review_tests PRIVATE /guard:cf)
    endif()
    set_target_properties(checkpoint_review_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME checkpoint_review_tests COMMAND checkpoint_review_tests)
    add_executable(depth_tracker_tests tests/depth_tracker_tests.cpp src/exception_capture/depth_tracker.cpp)
    target_compile_features(depth_tracker_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(depth_tracker_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
        target_link_options(depth_tracker_tests PRIVATE /guard:cf)
    endif()
    set_target_properties(depth_tracker_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME depth_tracker_tests COMMAND depth_tracker_tests)
    set_tests_properties(depth_tracker_tests PROPERTIES TIMEOUT 30)
    add_executable(barrier_observer_tests tests/barrier_observer_tests.cpp src/bridge_probe/barrier_observer.cpp)
    add_executable(submission_completion_tests tests/submission_completion_tests.cpp)
    target_link_libraries(submission_completion_tests PRIVATE mhw_dlss_bridge_policy)
    target_compile_features(submission_completion_tests PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(submission_completion_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
        target_link_options(submission_completion_tests PRIVATE /guard:cf)
    endif()
    set_target_properties(submission_completion_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME submission_completion_tests COMMAND submission_completion_tests)
    target_compile_features(barrier_observer_tests PRIVATE cxx_std_20)
    target_compile_definitions(barrier_observer_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX MHW_OWNED_OBSERVER_TEST)
    target_link_libraries(barrier_observer_tests PRIVATE mhw_dlss_bridge_policy minhook d3d12 dxgi dxguid)
    if(MSVC)
        target_compile_options(barrier_observer_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
        target_link_options(barrier_observer_tests PRIVATE /guard:cf)
    endif()
    set_target_properties(barrier_observer_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME barrier_history_and_warp_observer COMMAND barrier_observer_tests)
    set_tests_properties(barrier_history_and_warp_observer PROPERTIES TIMEOUT 30)
    add_test(NAME depth_queue_warp_observer COMMAND barrier_observer_tests --queue)
    set_tests_properties(depth_queue_warp_observer PROPERTIES TIMEOUT 30)
    add_test(NAME post_evaluate_warp_journal COMMAND barrier_observer_tests --continuation)
    add_test(NAME completion_bounded_warp_journal COMMAND barrier_observer_tests --completion)
    add_test(NAME completion_drain_warp_journal COMMAND barrier_observer_tests --completion-drain)
    set_tests_properties(completion_drain_warp_journal PROPERTIES TIMEOUT 30)
    set_tests_properties(completion_bounded_warp_journal PROPERTIES TIMEOUT 30)
    set_tests_properties(post_evaluate_warp_journal PROPERTIES TIMEOUT 30)
    foreach(bound call-limit batch-limit description-limit expiry)
        add_test(NAME barrier_observer_${bound} COMMAND barrier_observer_tests --${bound})
        set_tests_properties(barrier_observer_${bound} PROPERTIES TIMEOUT 30)
    endforeach()
    add_executable(resource_inspection_tests tests/resource_inspection_tests.cpp src/bridge_probe/presentation_observer.cpp)
    target_compile_features(resource_inspection_tests PRIVATE cxx_std_20)
    target_compile_definitions(resource_inspection_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(resource_inspection_tests PRIVATE mhw_dlss_bridge_policy minhook d3d12 dxgi dxguid user32)
    if(MSVC)
        target_compile_options(resource_inspection_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /guard:cf)
        target_link_options(resource_inspection_tests PRIVATE /guard:cf)
    endif()
    set_target_properties(resource_inspection_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME resource_inspection_tests COMMAND resource_inspection_tests)
    set_tests_properties(resource_inspection_tests PROPERTIES TIMEOUT 30)
    add_test(NAME depth_exception_capture_self_test COMMAND mhw_depth_exception_capture --self-test)
    set_tests_properties(depth_exception_capture_self_test PROPERTIES TIMEOUT 30)
    add_executable(bridge_policy_tests tests/bridge_policy_tests.cpp)
    target_compile_features(bridge_policy_tests PRIVATE cxx_std_20)
    target_link_libraries(bridge_policy_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(bridge_policy_tests PRIVATE
            /W4 /WX /permissive- /EHsc /utf-8
        )
    endif()
    set_target_properties(bridge_policy_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME bridge_policy_tests COMMAND bridge_policy_tests)

    add_executable(contract_snapshot_tests
        tests/contract_snapshot_tests.cpp
    )
    target_compile_features(contract_snapshot_tests PRIVATE cxx_std_20)
    target_link_libraries(contract_snapshot_tests PRIVATE
        mhw_dlss_bridge_policy
    )
    if(MSVC)
        target_compile_options(contract_snapshot_tests PRIVATE
            /W4 /WX /permissive- /EHsc /utf-8
        )
    endif()
    set_target_properties(contract_snapshot_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME contract_snapshot_tests COMMAND contract_snapshot_tests)

    add_executable(optimal_settings_tests tests/optimal_settings_tests.cpp)
    target_compile_features(optimal_settings_tests PRIVATE cxx_std_20)
    target_link_libraries(optimal_settings_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(optimal_settings_tests PRIVATE
            /W4 /WX /permissive- /EHsc /utf-8
        )
    endif()
    set_target_properties(optimal_settings_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME optimal_settings_tests COMMAND optimal_settings_tests)

    add_executable(evaluate_gate_tests tests/evaluate_gate_tests.cpp)
    target_compile_features(evaluate_gate_tests PRIVATE cxx_std_20)
    target_link_libraries(evaluate_gate_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(evaluate_gate_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(evaluate_gate_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME evaluate_gate_tests COMMAND evaluate_gate_tests)

    add_executable(renderer_snapshot_tests tests/renderer_snapshot_tests.cpp)
    target_compile_features(renderer_snapshot_tests PRIVATE cxx_std_20)
    target_link_libraries(renderer_snapshot_tests PRIVATE mhw_dlss_bridge_policy)
    target_compile_definitions(renderer_snapshot_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(renderer_snapshot_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(renderer_snapshot_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME renderer_snapshot_tests COMMAND renderer_snapshot_tests)

    add_executable(projection_snapshot_tests tests/projection_snapshot_tests.cpp)
    target_compile_features(projection_snapshot_tests PRIVATE cxx_std_20)
    target_link_libraries(projection_snapshot_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(projection_snapshot_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(projection_snapshot_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME projection_snapshot_tests COMMAND projection_snapshot_tests)

    add_executable(jitter_sequence_tests tests/jitter_sequence_tests.cpp)
    target_compile_features(jitter_sequence_tests PRIVATE cxx_std_20)
    target_link_libraries(jitter_sequence_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(jitter_sequence_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(jitter_sequence_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME jitter_sequence_tests COMMAND jitter_sequence_tests)

    add_executable(scene_trigger_tests tests/scene_trigger_tests.cpp)
    target_compile_features(scene_trigger_tests PRIVATE cxx_std_20)
    target_compile_definitions(scene_trigger_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(scene_trigger_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(scene_trigger_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(scene_trigger_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME scene_trigger_tests COMMAND scene_trigger_tests)

    add_executable(temporal_snapshot_tests tests/temporal_snapshot_tests.cpp)
    target_compile_features(temporal_snapshot_tests PRIVATE cxx_std_20)
    target_link_libraries(temporal_snapshot_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(temporal_snapshot_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(temporal_snapshot_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME temporal_snapshot_tests COMMAND temporal_snapshot_tests)

    add_executable(handle_lifetime_tests tests/handle_lifetime_tests.cpp)
    target_compile_features(handle_lifetime_tests PRIVATE cxx_std_20)
    target_link_libraries(handle_lifetime_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(handle_lifetime_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(handle_lifetime_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_test(NAME handle_lifetime_tests COMMAND handle_lifetime_tests)
    set_tests_properties(handle_lifetime_tests PROPERTIES TIMEOUT 20)

    add_executable(capability_snapshot_tests tests/capability_snapshot_tests.cpp)
    target_compile_features(capability_snapshot_tests PRIVATE cxx_std_20)
    target_link_libraries(capability_snapshot_tests PRIVATE mhw_dlss_bridge_policy)
    if(MSVC)
        target_compile_options(capability_snapshot_tests PRIVATE
            /W4 /WX /permissive- /EHsc /utf-8
        )
    endif()
    set_target_properties(capability_snapshot_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME capability_snapshot_tests COMMAND capability_snapshot_tests)

    add_library(probe_mock_feature SHARED
        tests/probe_mock_feature.cpp
        src/probe/ngx_abi.hpp
    )
    target_compile_features(probe_mock_feature PRIVATE cxx_std_20)
    target_compile_definitions(probe_mock_feature PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(probe_mock_feature PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(probe_mock_feature PROPERTIES
        OUTPUT_NAME "nvngx_dlss"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/mock"
    )

    add_executable(probe_mock_host tests/probe_mock_host.cpp src/probe/ngx_abi.hpp)
    target_compile_features(probe_mock_host PRIVATE cxx_std_20)
    target_compile_definitions(probe_mock_host PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_link_libraries(probe_mock_host PRIVATE d3d12 dxgi user32)
    if(MSVC)
        target_compile_options(probe_mock_host PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(probe_mock_host PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_dependencies(probe_mock_host mhw_dlss_probe probe_mock_feature)

    add_executable(unlock_policy_tests
        tests/unlock_policy_tests.cpp
        src/unlock/unlock_policy.cpp
        src/unlock/unlock_policy.hpp
    )
    target_compile_features(unlock_policy_tests PRIVATE cxx_std_20)
    target_include_directories(unlock_policy_tests PRIVATE src/unlock)
    if(MSVC)
        target_compile_options(unlock_policy_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    endif()
    set_target_properties(unlock_policy_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME unlock_policy_tests COMMAND unlock_policy_tests)

    add_test(
        NAME probe_mock_host
        COMMAND probe_mock_host
            "$<TARGET_FILE:probe_mock_feature>"
            "$<TARGET_FILE:mhw_dlss_probe>"
            "${CMAKE_BINARY_DIR}/probe-mock-host.log"
    )
    add_test(
        NAME probe_rejects_unknown_host
        COMMAND probe_mock_host
            "$<TARGET_FILE:probe_mock_feature>"
            "$<TARGET_FILE:mhw_dlss_probe>"
            "${CMAKE_BINARY_DIR}/probe-reject.log"
            hash-reject
    )
    add_test(
        NAME probe_rejects_bad_metadata
        COMMAND probe_mock_host
            "$<TARGET_FILE:probe_mock_feature>"
            "$<TARGET_FILE:mhw_dlss_probe>"
            "${CMAKE_BINARY_DIR}/probe-metadata-reject.log"
            metadata-reject
    )
    add_test(
        NAME probe_rejects_non_d3d12_commands
        COMMAND probe_mock_host
            "$<TARGET_FILE:probe_mock_feature>"
            "$<TARGET_FILE:mhw_dlss_probe>"
            "${CMAKE_BINARY_DIR}/probe-api-reject.log"
            api-reject
    )
    add_test(
        NAME probe_stops_after_getter_exception
        COMMAND probe_mock_host
            "$<TARGET_FILE:probe_mock_feature>"
            "$<TARGET_FILE:mhw_dlss_probe>"
            "${CMAKE_BINARY_DIR}/probe-getter-fault.log"
            getter-fault
    )
    add_test(
        NAME probe_rejects_ambiguous_swap_chain_identity
        COMMAND probe_mock_host
            "$<TARGET_FILE:probe_mock_feature>"
            "$<TARGET_FILE:mhw_dlss_probe>"
            "${CMAKE_BINARY_DIR}/probe-identity-ambiguous.log"
            identity-ambiguous
    )
endif()

include(cmake/ReflexBoundaries.cmake)
