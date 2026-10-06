# Unmodified Microsoft DirectXShaderCompiler checksum implementation, commit
# 69c311af7091953427a6d67c16e811ce65313b93. University of Illinois/NCSA license;
# see third_party/dxc_hash/LICENSE.TXT. No compiler or runtime is downloaded.
set(dxbc_hash_dir "${CMAKE_CURRENT_LIST_DIR}/../third_party/dxc_hash")
function(mhw_check_hash_source name expected)
    file(SHA256 "${dxbc_hash_dir}/${name}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Unexpected DXBC checksum dependency: ${name}")
    endif()
endfunction()
mhw_check_hash_source(DxilHash.cpp d66b631ea28d4c90a8217fac004f9290eaac07f79115451e0c4e954c0b0c94af)
mhw_check_hash_source(DxilHash.h 4cff245da88b69526290495b2e439745a938aa60e48fd565832c9d8d2dfe6918)
mhw_check_hash_source(LICENSE.TXT 27a49e35d1da96eba18fba54bc882667ff0ff8c0254f16f2b6e165d605ba7df8)
add_library(mhw_dxbc_hash STATIC "${dxbc_hash_dir}/DxilHash.cpp")
target_compile_definitions(mhw_dxbc_hash PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
