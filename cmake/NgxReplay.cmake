# Optional, locally supplied public NVIDIA DLSS SDK. No SDK code/binaries are
# downloaded, copied into the game, or installed by this target.
# NVIDIA/DLSS commit a291cc7d2cc642a51566f3dfd5376f635cd1b284.
# License: NVIDIA RTX SDKs License (2024-03-14), SDK LICENSE.txt.
set(MHW_NGX_SDK_ROOT "" CACHE PATH "Local pinned public NVIDIA DLSS SDK for standalone replay")
if(NOT MHW_NGX_SDK_ROOT)
    return()
endif()

function(mhw_check_ngx_file relative expected)
    file(SHA256 "${MHW_NGX_SDK_ROOT}/${relative}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Unexpected public NGX dependency: ${relative}")
    endif()
endfunction()
mhw_check_ngx_file(include/nvsdk_ngx.h 35d19dba363786d27d11536a4628a430596477f39dbfed5eae482132ae6e794a)
mhw_check_ngx_file(include/nvsdk_ngx_defs.h a9481cbc78d53ee56639f0a573ec095c6c1ea1e5f296d139c20c42bf342c2e1b)
mhw_check_ngx_file(include/nvsdk_ngx_helpers.h c46628f150c6edee245758c878fc0af2b70f7ce582744729647cdb1969c21895)
mhw_check_ngx_file(include/nvsdk_ngx_params.h 51a1f94116cf82f95c63a2c197371ed22e08bddb09918d36cc6e2ac712753c80)
mhw_check_ngx_file(lib/Windows_x86_64/x64/nvsdk_ngx_d.lib 31e82b4ec3242ec6e5b42c73e1f5f5e260338a6ee213bd64444f6c2fa364aa84)

add_executable(mhw_dlaa_replay tools/dlaa_replay.cpp)
target_compile_features(mhw_dlaa_replay PRIVATE cxx_std_20)
target_compile_definitions(mhw_dlaa_replay PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_include_directories(mhw_dlaa_replay SYSTEM PRIVATE "${MHW_NGX_SDK_ROOT}/include")
target_link_libraries(mhw_dlaa_replay PRIVATE
    "${MHW_NGX_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib"
    d3d12 dxgi dxguid bcrypt)
target_compile_options(mhw_dlaa_replay PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /external:W0)
set_target_properties(mhw_dlaa_replay PROPERTIES
    MSVC_RUNTIME_LIBRARY MultiThreadedDLL
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(mhw_dlss_sdk_live STATIC src/dlss_sdk/live.cpp)
target_compile_features(mhw_dlss_sdk_live PUBLIC cxx_std_20)
target_compile_definitions(mhw_dlss_sdk_live PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(mhw_dlss_sdk_live SYSTEM PRIVATE "${MHW_NGX_SDK_ROOT}/include")
target_link_libraries(mhw_dlss_sdk_live PUBLIC "${MHW_NGX_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib" d3d12 dxgi dxguid bcrypt)
target_compile_options(mhw_dlss_sdk_live PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /external:W0)
set_target_properties(mhw_dlss_sdk_live PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)

add_executable(mhw_ngx_capability_probe tools/ngx_capability_probe.cpp)
target_compile_features(mhw_ngx_capability_probe PRIVATE cxx_std_20)
target_compile_definitions(mhw_ngx_capability_probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_include_directories(mhw_ngx_capability_probe SYSTEM PRIVATE "${MHW_NGX_SDK_ROOT}/include")
target_link_libraries(mhw_ngx_capability_probe PRIVATE "${MHW_NGX_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib" d3d12 dxgi dxguid)
target_compile_options(mhw_ngx_capability_probe PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /external:W0)
set_target_properties(mhw_ngx_capability_probe PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
