# Standalone observational DLL. Does not link or initialize a vendor SDK.
add_library(mhw_reflex_boundary_probe SHARED
    src/reflex/frame_boundaries.cpp src/reflex/frame_boundaries_worker.cpp
    src/reflex/frame_boundaries_entry.asm src/projection_binding/identity.cpp
    src/projection_binding/trampoline_pool.cpp src/bridge_probe/bounded_scalar_read.cpp)
target_compile_features(mhw_reflex_boundary_probe PRIVATE cxx_std_20)
target_include_directories(mhw_reflex_boundary_probe PRIVATE src/bridge_probe
    src/projection_binding "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_reflex_boundary_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_reflex_boundary_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)
target_compile_options(mhw_reflex_boundary_probe PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_reflex_boundary_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_reflex_boundary_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Same bounded recorder/ABI, separate version and controls. No vendor SDK.
add_library(mhw_reflex_input_probe SHARED
    src/reflex/frame_boundaries.cpp src/reflex/frame_boundaries_worker.cpp
    src/reflex/frame_boundaries_entry.asm src/projection_binding/identity.cpp
    src/projection_binding/trampoline_pool.cpp src/bridge_probe/bounded_scalar_read.cpp)
target_compile_features(mhw_reflex_input_probe PRIVATE cxx_std_20)
target_include_directories(mhw_reflex_input_probe PRIVATE src/bridge_probe
    src/projection_binding "${minhook_SOURCE_DIR}/src")
target_compile_definitions(mhw_reflex_input_probe PRIVATE MHW_REFLEX_INPUT_TRACE
    WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(mhw_reflex_input_probe PRIVATE minhook bcrypt d3d12 dxgi dxguid)
target_compile_options(mhw_reflex_input_probe PRIVATE
    $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
target_link_options(mhw_reflex_input_probe PRIVATE /guard:cf /OPT:NOICF)
set_target_properties(mhw_reflex_input_probe PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(BUILD_TESTING)
    add_executable(reflex_frame_policy_tests tests/reflex_frame_policy_tests.cpp)
    target_compile_features(reflex_frame_policy_tests PRIVATE cxx_std_20)
    target_include_directories(reflex_frame_policy_tests PRIVATE src)
    target_compile_options(reflex_frame_policy_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME reflex_frame_policy COMMAND reflex_frame_policy_tests)

    add_executable(reflex_frame_stamp_tests tests/reflex_frame_stamp_tests.cpp)
    target_compile_features(reflex_frame_stamp_tests PRIVATE cxx_std_20)
    target_include_directories(reflex_frame_stamp_tests PRIVATE src)
    target_compile_options(reflex_frame_stamp_tests PRIVATE /W4 /WX /permissive- /EHsc /utf-8)
    add_test(NAME reflex_frame_stamp COMMAND reflex_frame_stamp_tests)

    add_executable(reflex_boundary_abi_tests tests/reflex_boundary_abi_tests.cpp src/reflex/frame_boundaries_entry.asm)
    target_compile_features(reflex_boundary_abi_tests PRIVATE cxx_std_20)
    target_compile_definitions(reflex_boundary_abi_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(reflex_boundary_abi_tests PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/utf-8;/guard:cf>)
    target_link_options(reflex_boundary_abi_tests PRIVATE /guard:cf)
    add_test(NAME reflex_boundary_abi COMMAND reflex_boundary_abi_tests)
endif()
