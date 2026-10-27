# The public NVIDIA DLSS SDK, supplied locally (README, "Building"); nothing is
# downloaded. NVIDIA/DLSS tag v310.9.16, commit
# 9981c6350dc58c0ce21f024fd65e31e851c31689 (0.32.0; before it a291cc7d, 310.7.0).
# License: NVIDIA RTX SDKs License (2024-03-14), the SDK's LICENSE.txt.
set(MHW_NGX_SDK_ROOT "" CACHE PATH "Local pinned public NVIDIA DLSS SDK")
if(NOT MHW_NGX_SDK_ROOT)
    message(FATAL_ERROR "Set MHW_NGX_SDK_ROOT to the NVIDIA DLSS SDK (README, Building)")
endif()

# Headers are hashed with the line endings of NVIDIA's repository (LF), so a
# clone with CRLF and the GitHub archive both pass; the library as it is.
function(mhw_check_ngx_file relative expected)
    if(relative MATCHES "\\.h$")
        file(READ "${MHW_NGX_SDK_ROOT}/${relative}" text)
        string(REPLACE "\r\n" "\n" text "${text}")
        string(SHA256 actual "${text}")
    else()
        file(SHA256 "${MHW_NGX_SDK_ROOT}/${relative}" actual)
    endif()
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Unexpected public NGX dependency: ${relative}")
    endif()
endfunction()
mhw_check_ngx_file(include/nvsdk_ngx.h dc38e7467cf415379c9d12ae1b6e4a494c453ed92720fb53e92aecb523e7b848)
mhw_check_ngx_file(include/nvsdk_ngx_defs.h ea23f33497cd274860d1c25a97644fce807dcb0037c594547203343103fad03e)
mhw_check_ngx_file(include/nvsdk_ngx_params.h 943bc8cc5cdae03b6303016fbad3183636f2335ae27a2d18776798c3b4efabbc)
mhw_check_ngx_file(include/nvsdk_ngx_helpers.h 5bcbadfe7478b802cf6d3aca4dc5ddd7d0889b99726e69c63f9e9bd555f44471)
mhw_check_ngx_file(include/nvsdk_ngx_helpers_d3d.h ec75224f36ed6580baaf250fa83620405ad81a1d98c2c436654a0c9cf6a6b8ba)
mhw_check_ngx_file(include/nvsdk_ngx_helpers_cuda.h f2851a76107bdf4fbcb7b261f3c35573c312d8ecb4bc18fb02d9e2cec7705042)
mhw_check_ngx_file(lib/Windows_x86_64/x64/nvsdk_ngx_d.lib bc43df9ec032004fde681d3e3afd01cde239e03a0125b822e88674c84287f99c)

# The SDK's NGX library: NGX start-up and the DLSS frame link it.
set(MHW_NGX_LIBRARY "${MHW_NGX_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib")
