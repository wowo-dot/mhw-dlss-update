#pragma once

#include <cstdint>

struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

namespace mhw::ngx {

// Minimal ABI declarations for MHW's legacy NGX client boundary. Keeping the
// types opaque avoids distributing NVIDIA SDK headers and limits the probe to
// observation only.
using Result = std::uint32_t;
using Feature = std::int32_t;

struct Parameter;
struct Handle;

using ProgressCallback = void(__cdecl *)(float current_progress,
                                         bool &should_cancel);
using ProgressCallbackC = void(__cdecl *)(float current_progress,
                                          bool *should_cancel);

using CreateFeatureFn =
    Result(__cdecl *)(ID3D12GraphicsCommandList *command_list, Feature feature,
                      const Parameter *parameters, Handle **output_handle);

using EvaluateFeatureFn = Result(__cdecl *)(
    ID3D12GraphicsCommandList *command_list, const Handle *feature_handle,
    const Parameter *parameters, ProgressCallback callback);

using EvaluateFeatureCFn = Result(__cdecl *)(
    ID3D12GraphicsCommandList *command_list, const Handle *feature_handle,
    const Parameter *parameters, ProgressCallbackC callback);

using ReleaseFeatureFn = Result(__cdecl *)(Handle *feature_handle);

using GetD3d12ResourceFn = Result(__cdecl *)(Parameter *parameters,
                                             const char *name,
                                             ID3D12Resource **value);
using GetFloatFn = Result(__cdecl *)(Parameter *parameters, const char *name,
                                     float *value);
using GetIntFn = Result(__cdecl *)(Parameter *parameters, const char *name,
                                   int *value);
using GetUIntFn = Result(__cdecl *)(Parameter *parameters, const char *name,
                                    unsigned int *value);
using GetMetadataFn = std::uint32_t(__cdecl *)();

inline constexpr Result result_success = 0x00000001U;
inline constexpr Feature feature_super_sampling = 1;

} // namespace mhw::ngx
