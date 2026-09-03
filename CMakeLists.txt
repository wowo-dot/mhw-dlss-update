cmake_minimum_required(VERSION 3.24)

project(mhw_dlss_update VERSION 0.1.0 LANGUAGES CXX C)

if(NOT WIN32)
    message(FATAL_ERROR "This research probe targets 64-bit Windows only")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Configure a 64-bit build")
endif()

include(FetchContent)

# MinHook v1.3.4. Pinning the commit keeps diagnostic builds reproducible.
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG c3fcafdc10146beb5919319d0683e44e3c30d537
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(minhook)

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

# `cmake --install --prefix <MO2 mod directory>` creates the exact virtual
# layout consumed by Stracker's Loader without touching the game installation.
install(TARGETS mhw_dlss_probe
    RUNTIME DESTINATION "nativePC/plugins"
    COMPONENT probe
)

include(CTest)
if(BUILD_TESTING)
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
