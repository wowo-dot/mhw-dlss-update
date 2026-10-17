# The public NVIDIA DLSS SDK, supplied locally (README, "Building"); nothing is
# downloaded. NVIDIA/DLSS commit a291cc7d2cc642a51566f3dfd5376f635cd1b284.
# License: NVIDIA RTX SDKs License (2024-03-14), the SDK's LICENSE.txt.
set(MHW_NGX_SDK_ROOT "" CACHE PATH "Local pinned public NVIDIA DLSS SDK")
if(NOT MHW_NGX_SDK_ROOT)
    message(FATAL_ERROR "Set MHW_NGX_SDK_ROOT to the NVIDIA DLSS SDK (README, Building)")
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

# The SDK's NGX library: NGX start-up and the DLSS frame link it.
set(MHW_NGX_LIBRARY "${MHW_NGX_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib")
