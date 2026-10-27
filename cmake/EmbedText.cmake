# Text files compiled into the DLL as byte arrays: `#include "<name>"` inside
# an initializer's braces gives the file's bytes, one per line. Written at
# configure time into the build tree's generated folder, and again whenever
# the source changes (CMAKE_CONFIGURE_DEPENDS).
set(MHW_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")

function(mhw_embed_text source name)
    file(READ "${source}" hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1,\n" bytes "${hex}")
    file(WRITE "${MHW_GENERATED_DIR}/${name}.tmp" "${bytes}")
    file(COPY_FILE "${MHW_GENERATED_DIR}/${name}.tmp" "${MHW_GENERATED_DIR}/${name}" ONLY_IF_DIFFERENT)
    file(REMOVE "${MHW_GENERATED_DIR}/${name}.tmp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source}")
endfunction()

# NVIDIA Image Scaling's NVSharpen (third_party/nis, MIT): src/sharpen.
mhw_embed_text("${CMAKE_CURRENT_LIST_DIR}/../third_party/nis/NIS_Scaler.h" nis_scaler.inc)
