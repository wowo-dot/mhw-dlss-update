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

add_library(mhw_dlss_bridge_policy STATIC
    src/bridge_probe/bridge_policy.cpp
    src/bridge_probe/bridge_policy.hpp
    src/bridge_probe/contract_snapshot.cpp
    src/bridge_probe/contract_snapshot.hpp
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
    src/trace/dllmain.cpp
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
        /W4 /WX /permissive- /EHsc /utf-8 /guard:cf
    )
    target_link_options(mhw_dlss_bridge_probe PRIVATE
        /guard:cf /OPT:NOICF
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
if(BUILD_TESTING)
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
