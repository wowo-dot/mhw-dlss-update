# The public Streamline 2.12.0 SDK, supplied locally (README, "Building");
# NVIDIA-RTX/Streamline e8aaa6eaac968711fb62473d4ae8256dde20919b. Source:
# MIT-style NVIDIA license.txt; Reflex SDK runtime: reflex.license.txt.
# No downloads, deployment, import-library linking or game changes by CMake.
set(MHW_STREAMLINE_SDK_ROOT "" CACHE PATH "Local pinned Streamline 2.12.0 SDK")
if(NOT MHW_STREAMLINE_SDK_ROOT)
    message(FATAL_ERROR "Set MHW_STREAMLINE_SDK_ROOT to the Streamline 2.12.0 SDK (README, Building)")
endif()
function(mhw_check_streamline_file relative expected)
    file(SHA256 "${MHW_STREAMLINE_SDK_ROOT}/${relative}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Unexpected Streamline dependency: ${relative}")
    endif()
endfunction()
mhw_check_streamline_file(include/sl.h e1e81a7428d15b30db37587e9469bd68a56d630d820a4accec0aee3b17e157dd)
mhw_check_streamline_file(include/sl_core_api.h 328dc3a2c1dee579c200ca97e2d5b1ec38c893be4ba2745ef5ad506af7172e81)
mhw_check_streamline_file(include/sl_core_types.h d0a4bc9d2efac5aa2d6171eb93f10e59a856c334a786536c0a871608ee8a877f)
mhw_check_streamline_file(include/sl_consts.h 17de74afda2cb96204ff380464bed926f660d92015fe0304dff4e329509c88b9)
mhw_check_streamline_file(include/sl_struct.h 885d694bb26a6319279efa74ee16b7c6eaad23febe3251b0a6623d53ca028bb9)
mhw_check_streamline_file(include/sl_version.h 46c827879cd81f459db908544f054c5620640a6f3801763aeebb12cc29f17807)
mhw_check_streamline_file(include/sl_result.h 2a0f6c12863bdc00b38910a5ec85d1f083c5671ee817a920afe626ac2a9100f7)
mhw_check_streamline_file(include/sl_appidentity.h 1337385ac9867d66fa6beb34c750e79aaf25a74a156a47e83f24937299dade87)
mhw_check_streamline_file(include/sl_device_wrappers.h af7741305b1a468c3a83efaa2005ecd53d2ffd7aadb8ec6868c9dcb4a8a8e3a1)
mhw_check_streamline_file(include/sl_pcl.h f43c5135fc8d5349ccd345e424f8cf1f61953a9f17e9897204b0741a3ab7b0fb)
mhw_check_streamline_file(include/sl_reflex.h 3b623a1189e04a686384d224a58c4ad9974c4e6e3204077676f6ec529475164c)
mhw_check_streamline_file(include/sl_dlss_g.h 1fc18cbe004e280df1f787276d08a1b28b8a8c4c65856fbaa659f56dff6a915d)

# Streamline's loader (pinned SHA-256 and signature check), Reflex and PCL
# markers, the driver's sleep status.
add_library(mhw_streamline_reflex STATIC src/reflex/streamline.cpp src/reflex/frame_session.cpp src/reflex/driver_status.cpp)
target_compile_features(mhw_streamline_reflex PUBLIC cxx_std_20)
target_compile_definitions(mhw_streamline_reflex PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_include_directories(mhw_streamline_reflex SYSTEM PUBLIC "${MHW_STREAMLINE_SDK_ROOT}/include")
target_link_libraries(mhw_streamline_reflex PUBLIC bcrypt wintrust crypt32 version)
target_compile_options(mhw_streamline_reflex PRIVATE /W4 /WX /permissive- /EHsc /utf-8 /external:W0 /guard:cf)
