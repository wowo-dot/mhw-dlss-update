#include "../src/probe/ngx_abi.hpp"

#include <Windows.h>

#include <atomic>
#include <cstdint>

namespace {

using namespace mhw;

std::atomic_uint g_original_calls{};
std::atomic_uint g_argument_failures{};
std::atomic_uint g_callback_calls{};
std::atomic_uintptr_t g_parameters{};
std::atomic_uintptr_t g_command_list{};

constexpr ngx::Result kCreateResult = 0xBAD00021U;
constexpr ngx::Result kEvaluateResult = 0xBAD00022U;
constexpr ngx::Result kReleaseResult = 0xBAD00023U;
constexpr DWORD kCreateLastError = 0x201U;
constexpr DWORD kEvaluateLastError = 0x202U;
constexpr DWORD kReleaseLastError = 0x203U;
constexpr std::uintptr_t kFeatureHandle = 0x1234U;

bool bad_metadata_requested() noexcept {
  wchar_t value[2]{};
  return GetEnvironmentVariableW(L"MHW_PROBE_MOCK_BAD_METADATA", value,
                                 static_cast<DWORD>(_countof(value))) == 1 &&
         value[0] == L'1';
}

} // namespace

extern "C" __declspec(dllexport) std::uint32_t __cdecl
NVSDK_NGX_GetAPIVersion() {
  return bad_metadata_requested() ? 0x13U : 0x12U;
}

extern "C" __declspec(dllexport) std::uint32_t __cdecl
NVSDK_NGX_GetSnippetVersion() {
  return 0x001010DU;
}

extern "C" __declspec(dllexport) std::uint32_t __cdecl
NVSDK_NGX_GetApplicationId() {
  return 0x0B9D2F5CU;
}

extern "C" __declspec(dllexport) __declspec(noinline) ngx::Result __cdecl
NVSDK_NGX_D3D12_CreateFeature(ID3D12GraphicsCommandList *command_list,
                              ngx::Feature feature,
                              const ngx::Parameter *parameters,
                              ngx::Handle **output_handle) {
  ++g_original_calls;
  g_parameters.store(reinterpret_cast<std::uintptr_t>(parameters));
  g_command_list.store(reinterpret_cast<std::uintptr_t>(command_list));
  if (command_list == nullptr || feature != ngx::feature_super_sampling ||
      parameters == nullptr || output_handle == nullptr) {
    ++g_argument_failures;
  }
  if (output_handle != nullptr) {
    *output_handle = reinterpret_cast<ngx::Handle *>(kFeatureHandle);
  }
  SetLastError(kCreateLastError);
  return kCreateResult;
}

extern "C" __declspec(dllexport) __declspec(noinline) ngx::Result __cdecl
NVSDK_NGX_D3D12_EvaluateFeature(ID3D12GraphicsCommandList *command_list,
                                const ngx::Handle *feature_handle,
                                const ngx::Parameter *parameters,
                                ngx::ProgressCallback callback) {
  ++g_original_calls;
  if (reinterpret_cast<std::uintptr_t>(command_list) != g_command_list.load() ||
      reinterpret_cast<std::uintptr_t>(feature_handle) != kFeatureHandle ||
      reinterpret_cast<std::uintptr_t>(parameters) != g_parameters.load() ||
      callback == nullptr) {
    ++g_argument_failures;
  }
  if (callback != nullptr) {
    bool should_cancel = false;
    callback(0.5F, should_cancel);
    ++g_callback_calls;
  }
  SetLastError(kEvaluateLastError);
  return kEvaluateResult;
}

extern "C" __declspec(dllexport) __declspec(noinline) ngx::Result __cdecl
NVSDK_NGX_D3D12_ReleaseFeature(ngx::Handle *feature_handle) {
  ++g_original_calls;
  if (reinterpret_cast<std::uintptr_t>(feature_handle) != kFeatureHandle) {
    ++g_argument_failures;
  }
  SetLastError(kReleaseLastError);
  return kReleaseResult;
}

extern "C" __declspec(dllexport) unsigned int __cdecl MockGetOriginalCalls() {
  return g_original_calls.load();
}

extern "C" __declspec(dllexport) unsigned int __cdecl
MockGetArgumentFailures() {
  return g_argument_failures.load();
}

extern "C" __declspec(dllexport) unsigned int __cdecl MockGetCallbackCalls() {
  return g_callback_calls.load();
}
