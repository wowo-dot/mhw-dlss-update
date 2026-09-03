#include "ngx_abi.hpp"

#include <MinHook.h>
#include <Windows.h>
#include <bcrypt.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <string_view>

namespace {

using namespace mhw;

constexpr std::uint32_t kExpectedFeatureApi = 0x12U;
constexpr std::uint32_t kExpectedSnippetVersion = 0x001010DU;
constexpr std::uint32_t kExpectedApplicationId = 0x0B9D2F5CU;
constexpr std::uint32_t kDefaultEvaluationLimit = 16U;
constexpr std::uint32_t kMaximumEvaluationLimit = 64U;
constexpr std::size_t kMaximumEvents = 192U;
constexpr std::size_t kMaximumSwapChainCandidates = 8U;
constexpr DWORD kHashBufferSize = 1U << 20U;
constexpr std::size_t kCreateSwapChainVtableIndex = 10U;
constexpr std::size_t kCreateSwapChainForHwndVtableIndex = 15U;

constexpr std::array<unsigned char, 32> kExpectedHostSha256{
    0xC2, 0xEB, 0xBB, 0xD2, 0xC4, 0x9F, 0x21, 0x6D, 0x48, 0x4E, 0x31,
    0xA5, 0x21, 0x9B, 0xED, 0x41, 0x9E, 0xB1, 0xE5, 0xE7, 0xD2, 0x06,
    0xD0, 0x2C, 0xBA, 0x04, 0x0A, 0x3A, 0xB7, 0x9D, 0x90, 0xEA,
};

constexpr std::array<unsigned char, 32> kExpectedFeatureSha256{
    0xD4, 0xCC, 0x11, 0xBC, 0xEC, 0x69, 0x3D, 0xFE, 0xFD, 0x6E, 0xDD,
    0xBA, 0x6D, 0xD2, 0x9B, 0xDF, 0xC1, 0x6B, 0x40, 0x43, 0xE7, 0xB8,
    0x31, 0x50, 0xF0, 0x09, 0x7D, 0x14, 0xB7, 0x65, 0x12, 0xD8,
};

constexpr std::array<unsigned char, 16> kExpectedCreatePrologue{
    0x48, 0x83, 0x3D, 0x00, 0x7E, 0x54, 0x00, 0x00,
    0x75, 0x06, 0xB8, 0x07, 0x00, 0xD0, 0xBA, 0xC3,
};
constexpr std::array<unsigned char, 14> kExpectedEvaluatePrologue{
    0x48, 0x83, 0xEC, 0x38, 0x48, 0x83, 0x3D,
    0xBC, 0x7D, 0x54, 0x00, 0x00, 0x75, 0x0A,
};
constexpr std::array<unsigned char, 16> kExpectedReleasePrologue{
    0x48, 0x83, 0x3D, 0xE0, 0x7D, 0x54, 0x00, 0x00,
    0x75, 0x06, 0xB8, 0x07, 0x00, 0xD0, 0xBA, 0xC3,
};

constexpr std::array<const char *, 5> kEvaluationResourceNames{
    "Color", "MotionVectors", "PreviousOutput", "Depth", "Output"};
constexpr std::array<const char *, 6> kEvaluationFloatNames{
    "Sharpness",   "MV.Scale.X",  "MV.Scale.Y",
    "MV.Offset.X", "MV.Offset.Y", "HDRClamp"};
constexpr std::array<const char *, 6> kEvaluationIntNames{
    "DepthInverted",   "Reset",        "DLAAMode",
    "Hint.RunPrePass", "Hint.RunPass", "Hint.RunPostPass"};
constexpr std::array<const char *, 8> kCreationUIntNames{"Width",
                                                         "Height",
                                                         "OutWidth",
                                                         "OutHeight",
                                                         "ResourceWidth",
                                                         "ResourceHeight",
                                                         "CreationNodeMask",
                                                         "VisibilityNodeMask"};
constexpr std::array<const char *, 3> kCreationIntNames{"PerfQualityValue",
                                                        "RTXValue", "Hint.HDR"};

enum class EventKind : std::uint32_t {
  swap_chain,
  create,
  evaluate,
  release,
};

enum class SwapChainCreationKind : std::uint32_t {
  none,
  legacy,
  hwnd,
};

struct AdapterSnapshot {
  bool available{};
  LUID luid{};
};

struct SwapChainSnapshot {
  SwapChainCreationKind creation_kind{};
  bool description_available{};
  bool window_owned_by_process{};
  std::uintptr_t pointer{};
  std::uintptr_t window{};
  UINT width{};
  UINT height{};
  DXGI_FORMAT format{DXGI_FORMAT_UNKNOWN};
  UINT buffer_count{};
  DXGI_SWAP_EFFECT swap_effect{};
  UINT flags{};
};

struct ResourceSnapshot {
  const char *name{};
  ngx::Result getter_result{};
  std::uintptr_t pointer{};
  bool description_available{};
  D3D12_RESOURCE_DESC description{};
};

template <typename Value> struct ScalarSnapshot {
  const char *name{};
  ngx::Result getter_result{};
  Value value{};
};

struct CaptureEvent {
  std::atomic_uint32_t ready{};
  std::uint64_t sequence{};
  std::uint64_t tick{};
  DWORD thread_id{};
  EventKind kind{};
  bool d3d12_verified{};
  bool parameter_query_attempted{};
  bool getter_exception{};
  AdapterSnapshot adapter{};
  SwapChainSnapshot swap_chain{};
  ngx::Result call_result{};
  ngx::Feature feature{};
  std::uintptr_t command_list{};
  std::uintptr_t parameters{};
  std::uintptr_t feature_handle{};
  std::array<ResourceSnapshot, kEvaluationResourceNames.size()> resources{};
  std::array<ScalarSnapshot<float>, kEvaluationFloatNames.size()> floats{};
  std::array<ScalarSnapshot<int>, kEvaluationIntNames.size()> ints{};
  std::array<ScalarSnapshot<unsigned int>, kCreationUIntNames.size()> uints{};
};

std::ofstream g_log_file;
HANDLE g_writer_wakeup{};
HMODULE g_probe_module{};
std::atomic_int g_probe_state{};
std::atomic_uint32_t g_reserved_events{};
std::atomic_uint32_t g_evaluation_calls{};
std::atomic_bool g_getter_exception{};
std::atomic_bool g_hook_engine_initialized{};
std::atomic_bool g_observer_enabled{};
std::array<unsigned char, 32> g_observed_host_sha256{};
std::uint32_t g_evaluation_limit = kDefaultEvaluationLimit;
bool g_query_parameters{};
std::array<CaptureEvent, kMaximumEvents> g_events{};
std::uint32_t g_next_event_to_write{};

ngx::GetD3d12ResourceFn g_get_resource{};
ngx::GetFloatFn g_get_float{};
ngx::GetIntFn g_get_int{};
ngx::GetUIntFn g_get_uint{};
ngx::CreateFeatureFn g_create_feature{};
ngx::EvaluateFeatureFn g_evaluate_feature{};
ngx::ReleaseFeatureFn g_release_feature{};

using CreateSwapChainFn = HRESULT(STDMETHODCALLTYPE *)(IDXGIFactory *,
                                                       IUnknown *,
                                                       DXGI_SWAP_CHAIN_DESC *,
                                                       IDXGISwapChain **);
using CreateSwapChainForHwndFn = HRESULT(STDMETHODCALLTYPE *)(
    IDXGIFactory2 *, IUnknown *, HWND, const DXGI_SWAP_CHAIN_DESC1 *,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *, IDXGIOutput *, IDXGISwapChain1 **);

CreateSwapChainFn g_create_swap_chain{};
CreateSwapChainForHwndFn g_create_swap_chain_for_hwnd{};
HMODULE g_dxgi_module{};

struct SwapChainCandidate {
  bool occupied{};
  AdapterSnapshot adapter{};
  SwapChainSnapshot swap_chain{};
};

enum class IdentityReportState : std::uint32_t {
  none,
  awaiting_adapter,
  adapter_unresolved,
  awaiting_swap_chain,
  ambiguous,
  complete,
};

struct WriterIdentity {
  bool ngx_adapter_available{};
  LUID ngx_adapter{};
  bool ngx_adapter_description_available{};
  std::array<SwapChainCandidate, kMaximumSwapChainCandidates> candidates{};
  bool candidate_overflow{};
  IdentityReportState last_report_state{};
  std::uintptr_t last_complete_swap_chain{};
  UINT last_complete_width{};
  UINT last_complete_height{};
  DXGI_FORMAT last_complete_format{DXGI_FORMAT_UNKNOWN};
  std::uint64_t revision{};
};

WriterIdentity g_writer_identity{};

void safe_release(IUnknown *object) noexcept;
AdapterSnapshot
capture_adapter_from_device_child(ID3D12DeviceChild *device_child) noexcept;

std::filesystem::path get_log_path() {
  wchar_t configured[32'768]{};
  const DWORD configured_size =
      GetEnvironmentVariableW(L"MHW_DLSS_PROBE_LOG", configured,
                              static_cast<DWORD>(_countof(configured)));
  if (configured_size > 0 && configured_size < _countof(configured)) {
    return configured;
  }

  wchar_t local_app_data[32'768]{};
  const DWORD local_app_data_size =
      GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data,
                              static_cast<DWORD>(_countof(local_app_data)));
  if (local_app_data_size > 0 &&
      local_app_data_size < _countof(local_app_data)) {
    return std::filesystem::path(local_app_data) / L"MhwDlssProbe" /
           (L"probe-" + std::to_wstring(GetCurrentProcessId()) + L".log");
  }

  return std::filesystem::temp_directory_path() /
         (L"mhw-dlss-probe-" + std::to_wstring(GetCurrentProcessId()) +
          L".log");
}

void write_control_log(std::string_view message) {
  if (!g_log_file.is_open()) {
    return;
  }
  g_log_file << GetTickCount64() << "\ttid=" << GetCurrentThreadId() << '\t'
             << message << '\n';
  g_log_file.flush();
}

std::filesystem::path module_path(HMODULE module) {
  wchar_t path[32'768]{};
  const DWORD length =
      GetModuleFileNameW(module, path, static_cast<DWORD>(_countof(path)));
  if (length == 0 || length >= _countof(path)) {
    return {};
  }
  return std::filesystem::path(path, path + length);
}

bool sha256_file(const std::filesystem::path &path,
                 std::array<unsigned char, 32> *output) noexcept {
  if (output == nullptr) {
    return false;
  }

  BCRYPT_ALG_HANDLE algorithm{};
  BCRYPT_HASH_HANDLE hash{};
  HANDLE file = INVALID_HANDLE_VALUE;
  unsigned char *hash_object{};
  unsigned char *buffer{};
  DWORD object_size{};
  DWORD result_size{};
  DWORD hash_size{};
  bool success = false;

  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) != 0) {
    goto cleanup;
  }

  if (BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                        reinterpret_cast<PUCHAR>(&object_size),
                        sizeof(object_size), &result_size, 0) != 0) {
    goto cleanup;
  }

  if (BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                        reinterpret_cast<PUCHAR>(&hash_size), sizeof(hash_size),
                        &result_size, 0) != 0 ||
      hash_size != output->size()) {
    goto cleanup;
  }

  hash_object =
      static_cast<unsigned char *>(HeapAlloc(GetProcessHeap(), 0, object_size));
  if (hash_object == nullptr ||
      BCryptCreateHash(algorithm, &hash, hash_object, object_size, nullptr, 0,
                       0) != 0) {
    goto cleanup;
  }

  file = CreateFileW(path.c_str(), GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                     OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    goto cleanup;
  }

  buffer = static_cast<unsigned char *>(
      HeapAlloc(GetProcessHeap(), 0, kHashBufferSize));
  if (buffer == nullptr) {
    goto cleanup;
  }
  for (;;) {
    DWORD bytes_read{};
    if (!ReadFile(file, buffer, kHashBufferSize, &bytes_read, nullptr)) {
      goto cleanup;
    }
    if (bytes_read == 0) {
      break;
    }
    if (BCryptHashData(hash, buffer, bytes_read, 0) != 0) {
      goto cleanup;
    }
  }

  success = BCryptFinishHash(hash, output->data(),
                             static_cast<ULONG>(output->size()), 0) == 0;

cleanup:
  if (file != INVALID_HANDLE_VALUE) {
    CloseHandle(file);
  }
  if (hash != nullptr) {
    BCryptDestroyHash(hash);
  }
  if (algorithm != nullptr) {
    BCryptCloseAlgorithmProvider(algorithm, 0);
  }
  if (buffer != nullptr) {
    HeapFree(GetProcessHeap(), 0, buffer);
  }
  if (hash_object != nullptr) {
    HeapFree(GetProcessHeap(), 0, hash_object);
  }
  return success;
}

std::string hash_to_hex(const std::array<unsigned char, 32> &hash) {
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string result;
  result.resize(hash.size() * 2U);
  for (std::size_t index = 0; index < hash.size(); ++index) {
    result[index * 2U] = digits[hash[index] >> 4U];
    result[index * 2U + 1U] = digits[hash[index] & 0x0FU];
  }
  return result;
}

bool test_override_enabled() noexcept {
  wchar_t value[64]{};
  const DWORD size =
      GetEnvironmentVariableW(L"MHW_DLSS_PROBE_TEST_OVERRIDE", value,
                              static_cast<DWORD>(_countof(value)));
  return size > 0 && size < _countof(value) &&
         std::wcscmp(value, L"I_UNDERSTAND") == 0;
}

bool verify_module_hash(HMODULE module,
                        const std::array<unsigned char, 32> &expected,
                        std::string_view label, bool allow_override,
                        std::array<unsigned char, 32> *observed = nullptr) {
  const auto path = module_path(module);
  if (path.empty()) {
    write_control_log(std::string("probe.abort reason=") + std::string(label) +
                      "-path-unavailable");
    return false;
  }

  std::array<unsigned char, 32> actual{};
  if (!sha256_file(path, &actual)) {
    write_control_log(std::string("probe.abort reason=") + std::string(label) +
                      "-hash-failed");
    return false;
  }
  if (observed != nullptr) {
    *observed = actual;
  }

  write_control_log(std::string("binary.sha256 name=") + std::string(label) +
                    " value=" + hash_to_hex(actual));
  if (actual == expected) {
    return true;
  }
  if (allow_override) {
    write_control_log(std::string("binary.override name=") +
                      std::string(label) + " warning=explicit-test-override");
    return true;
  }

  write_control_log(std::string("probe.abort reason=") + std::string(label) +
                    "-hash-mismatch");
  return false;
}

std::uint32_t read_evaluation_limit() noexcept {
  wchar_t value[32]{};
  const DWORD size =
      GetEnvironmentVariableW(L"MHW_DLSS_PROBE_CAPTURE_EVALUATIONS", value,
                              static_cast<DWORD>(_countof(value)));
  if (size == 0 || size >= _countof(value)) {
    return kDefaultEvaluationLimit;
  }

  wchar_t *end{};
  const unsigned long parsed = std::wcstoul(value, &end, 10);
  if (end == value || *end != L'\0' || parsed == 0 ||
      parsed > kMaximumEvaluationLimit) {
    return kDefaultEvaluationLimit;
  }
  return static_cast<std::uint32_t>(parsed);
}

bool read_parameter_query_setting() noexcept {
  wchar_t value[2]{};
  return GetEnvironmentVariableW(L"MHW_DLSS_PROBE_QUERY_PARAMETERS", value,
                                 static_cast<DWORD>(_countof(value))) == 1 &&
         value[0] == L'1';
}

int observation_exception_filter(unsigned int code) noexcept {
  switch (code) {
  case EXCEPTION_ACCESS_VIOLATION:
  case EXCEPTION_DATATYPE_MISALIGNMENT:
  case EXCEPTION_ILLEGAL_INSTRUCTION:
  case EXCEPTION_IN_PAGE_ERROR:
  case EXCEPTION_PRIV_INSTRUCTION:
    return EXCEPTION_EXECUTE_HANDLER;
  default:
    return EXCEPTION_CONTINUE_SEARCH;
  }
}

int getter_exception_filter(unsigned int code) noexcept {
  const int disposition = observation_exception_filter(code);
  if (disposition == EXCEPTION_EXECUTE_HANDLER) {
    g_getter_exception.store(true, std::memory_order_release);
  }
  return disposition;
}

bool try_get_resource_description(ID3D12Resource *resource,
                                  D3D12_RESOURCE_DESC *description) noexcept {
  if (resource == nullptr || description == nullptr) {
    return false;
  }
  __try {
    *description = resource->GetDesc();
    return true;
  } __except (getter_exception_filter(GetExceptionCode())) {
    return false;
  }
}

ngx::Result safe_get_resource(const ngx::Parameter *parameters,
                              const char *name,
                              ID3D12Resource **value) noexcept {
  if (g_getter_exception.load(std::memory_order_acquire)) {
    return 0;
  }
  __try {
    return g_get_resource(const_cast<ngx::Parameter *>(parameters), name,
                          value);
  } __except (getter_exception_filter(GetExceptionCode())) {
    return 0;
  }
}

ngx::Result safe_get_float(const ngx::Parameter *parameters, const char *name,
                           float *value) noexcept {
  if (g_getter_exception.load(std::memory_order_acquire)) {
    return 0;
  }
  __try {
    return g_get_float(const_cast<ngx::Parameter *>(parameters), name, value);
  } __except (getter_exception_filter(GetExceptionCode())) {
    return 0;
  }
}

ngx::Result safe_get_int(const ngx::Parameter *parameters, const char *name,
                         int *value) noexcept {
  if (g_getter_exception.load(std::memory_order_acquire)) {
    return 0;
  }
  __try {
    return g_get_int(const_cast<ngx::Parameter *>(parameters), name, value);
  } __except (getter_exception_filter(GetExceptionCode())) {
    return 0;
  }
}

ngx::Result safe_get_uint(const ngx::Parameter *parameters, const char *name,
                          unsigned int *value) noexcept {
  if (g_getter_exception.load(std::memory_order_acquire)) {
    return 0;
  }
  __try {
    return g_get_uint(const_cast<ngx::Parameter *>(parameters), name, value);
  } __except (getter_exception_filter(GetExceptionCode())) {
    return 0;
  }
}

ngx::Handle *safe_load_handle(ngx::Handle **output_handle) noexcept {
  if (output_handle == nullptr) {
    return nullptr;
  }
  __try {
    return *output_handle;
  } __except (observation_exception_filter(GetExceptionCode())) {
    return nullptr;
  }
}

bool validate_d3d12_command_list(ID3D12GraphicsCommandList *command_list,
                                 AdapterSnapshot *adapter) noexcept {
  if (adapter != nullptr) {
    *adapter = {};
  }
  if (command_list == nullptr) {
    return false;
  }

  ID3D12GraphicsCommandList *verified{};
  HRESULT result = E_FAIL;
  __try {
    result = command_list->QueryInterface(__uuidof(ID3D12GraphicsCommandList),
                                          reinterpret_cast<void **>(&verified));
    if (SUCCEEDED(result) && verified != nullptr) {
      if (adapter != nullptr) {
        *adapter = capture_adapter_from_device_child(verified);
      }
    }
  } __except (observation_exception_filter(GetExceptionCode())) {
    result = E_FAIL;
  }
  const bool valid = SUCCEEDED(result) && verified != nullptr;
  safe_release(verified);
  return valid;
}

void safe_release(IUnknown *object) noexcept {
  if (object == nullptr) {
    return;
  }
  __try {
    object->Release();
  } __except (observation_exception_filter(GetExceptionCode())) {
  }
}

AdapterSnapshot
capture_adapter_from_device_child(ID3D12DeviceChild *device_child) noexcept {
  AdapterSnapshot snapshot{};
  if (device_child == nullptr) {
    return snapshot;
  }

  ID3D12Device *device{};
  HRESULT result = E_FAIL;
  __try {
    result = device_child->GetDevice(__uuidof(ID3D12Device),
                                     reinterpret_cast<void **>(&device));
    if (SUCCEEDED(result) && device != nullptr) {
      snapshot.luid = device->GetAdapterLuid();
      snapshot.available = true;
    }
  } __except (observation_exception_filter(GetExceptionCode())) {
    snapshot = {};
  }
  safe_release(device);
  return snapshot;
}

AdapterSnapshot
capture_adapter_from_unknown(IUnknown *device_or_queue) noexcept {
  AdapterSnapshot snapshot{};
  if (device_or_queue == nullptr) {
    return snapshot;
  }

  ID3D12CommandQueue *queue{};
  HRESULT result = E_FAIL;
  __try {
    result = device_or_queue->QueryInterface(__uuidof(ID3D12CommandQueue),
                                             reinterpret_cast<void **>(&queue));
  } __except (observation_exception_filter(GetExceptionCode())) {
    result = E_FAIL;
  }
  if (SUCCEEDED(result) && queue != nullptr) {
    snapshot = capture_adapter_from_device_child(queue);
  }
  safe_release(queue);
  return snapshot;
}

bool capture_legacy_swap_chain_description(
    IDXGISwapChain *swap_chain, SwapChainSnapshot *snapshot) noexcept {
  if (swap_chain == nullptr || snapshot == nullptr) {
    return false;
  }
  __try {
    DXGI_SWAP_CHAIN_DESC description{};
    if (FAILED(swap_chain->GetDesc(&description))) {
      return false;
    }
    snapshot->creation_kind = SwapChainCreationKind::legacy;
    snapshot->description_available = true;
    snapshot->pointer = reinterpret_cast<std::uintptr_t>(swap_chain);
    snapshot->window =
        reinterpret_cast<std::uintptr_t>(description.OutputWindow);
    DWORD window_process_id{};
    GetWindowThreadProcessId(description.OutputWindow, &window_process_id);
    snapshot->window_owned_by_process =
        window_process_id == GetCurrentProcessId();
    snapshot->width = description.BufferDesc.Width;
    snapshot->height = description.BufferDesc.Height;
    snapshot->format = description.BufferDesc.Format;
    snapshot->buffer_count = description.BufferCount;
    snapshot->swap_effect = description.SwapEffect;
    snapshot->flags = description.Flags;
    return true;
  } __except (observation_exception_filter(GetExceptionCode())) {
    return false;
  }
}

bool capture_hwnd_swap_chain_description(IDXGISwapChain1 *swap_chain,
                                         HWND window,
                                         SwapChainSnapshot *snapshot) noexcept {
  if (swap_chain == nullptr || snapshot == nullptr) {
    return false;
  }
  __try {
    DXGI_SWAP_CHAIN_DESC1 description{};
    if (FAILED(swap_chain->GetDesc1(&description))) {
      return false;
    }
    snapshot->creation_kind = SwapChainCreationKind::hwnd;
    snapshot->description_available = true;
    snapshot->pointer = reinterpret_cast<std::uintptr_t>(swap_chain);
    snapshot->window = reinterpret_cast<std::uintptr_t>(window);
    DWORD window_process_id{};
    GetWindowThreadProcessId(window, &window_process_id);
    snapshot->window_owned_by_process =
        window_process_id == GetCurrentProcessId();
    snapshot->width = description.Width;
    snapshot->height = description.Height;
    snapshot->format = description.Format;
    snapshot->buffer_count = description.BufferCount;
    snapshot->swap_effect = description.SwapEffect;
    snapshot->flags = description.Flags;
    return true;
  } __except (observation_exception_filter(GetExceptionCode())) {
    return false;
  }
}

CaptureEvent *reserve_event(EventKind kind) noexcept {
  const std::uint32_t index =
      g_reserved_events.fetch_add(1, std::memory_order_relaxed);
  if (index >= g_events.size()) {
    return nullptr;
  }

  CaptureEvent &event = g_events[index];
  event.sequence = static_cast<std::uint64_t>(index) + 1U;
  event.tick = GetTickCount64();
  event.thread_id = GetCurrentThreadId();
  event.kind = kind;
  return &event;
}

void publish_event(CaptureEvent *event) noexcept {
  if (event == nullptr) {
    return;
  }
  event->ready.store(1U, std::memory_order_release);
  if (g_writer_wakeup != nullptr) {
    SetEvent(g_writer_wakeup);
  }
}

void capture_resource(ResourceSnapshot *snapshot,
                      const ngx::Parameter *parameters,
                      const char *name) noexcept {
  snapshot->name = name;
  ID3D12Resource *resource{};
  snapshot->getter_result = safe_get_resource(parameters, name, &resource);
  snapshot->pointer = reinterpret_cast<std::uintptr_t>(resource);
  if (snapshot->getter_result == ngx::result_success && resource != nullptr) {
    snapshot->description_available =
        try_get_resource_description(resource, &snapshot->description);
  }
}

void capture_float(ScalarSnapshot<float> *snapshot,
                   const ngx::Parameter *parameters,
                   const char *name) noexcept {
  snapshot->name = name;
  snapshot->getter_result = safe_get_float(parameters, name, &snapshot->value);
}

void capture_int(ScalarSnapshot<int> *snapshot,
                 const ngx::Parameter *parameters, const char *name) noexcept {
  snapshot->name = name;
  snapshot->getter_result = safe_get_int(parameters, name, &snapshot->value);
}

void capture_uint(ScalarSnapshot<unsigned int> *snapshot,
                  const ngx::Parameter *parameters, const char *name) noexcept {
  snapshot->name = name;
  snapshot->getter_result = safe_get_uint(parameters, name, &snapshot->value);
}

void capture_create_event(ngx::Result result,
                          ID3D12GraphicsCommandList *command_list,
                          ngx::Feature feature,
                          const ngx::Parameter *parameters,
                          ngx::Handle *feature_handle, bool d3d12_verified,
                          AdapterSnapshot adapter) noexcept {
  CaptureEvent *event = reserve_event(EventKind::create);
  if (event == nullptr) {
    return;
  }

  event->call_result = result;
  event->d3d12_verified = d3d12_verified;
  event->adapter = adapter;
  event->feature = feature;
  event->command_list = reinterpret_cast<std::uintptr_t>(command_list);
  event->parameters = reinterpret_cast<std::uintptr_t>(parameters);
  event->feature_handle = reinterpret_cast<std::uintptr_t>(feature_handle);
  event->parameter_query_attempted =
      d3d12_verified && g_query_parameters &&
      !g_getter_exception.load(std::memory_order_acquire);
  if (event->parameter_query_attempted) {
    for (std::size_t index = 0; index < kCreationUIntNames.size(); ++index) {
      capture_uint(&event->uints[index], parameters, kCreationUIntNames[index]);
    }
    for (std::size_t index = 0; index < kCreationIntNames.size(); ++index) {
      capture_int(&event->ints[index], parameters, kCreationIntNames[index]);
    }
  }
  event->getter_exception = g_getter_exception.load(std::memory_order_acquire);
  publish_event(event);
}

void capture_evaluate_event(ngx::Result result,
                            ID3D12GraphicsCommandList *command_list,
                            const ngx::Handle *feature_handle,
                            const ngx::Parameter *parameters,
                            bool d3d12_verified,
                            AdapterSnapshot adapter) noexcept {
  CaptureEvent *event = reserve_event(EventKind::evaluate);
  if (event == nullptr) {
    return;
  }

  event->call_result = result;
  event->d3d12_verified = d3d12_verified;
  event->adapter = adapter;
  event->command_list = reinterpret_cast<std::uintptr_t>(command_list);
  event->parameters = reinterpret_cast<std::uintptr_t>(parameters);
  event->feature_handle = reinterpret_cast<std::uintptr_t>(feature_handle);
  event->parameter_query_attempted =
      d3d12_verified && g_query_parameters &&
      !g_getter_exception.load(std::memory_order_acquire);
  if (event->parameter_query_attempted) {
    for (std::size_t index = 0; index < kEvaluationResourceNames.size();
         ++index) {
      capture_resource(&event->resources[index], parameters,
                       kEvaluationResourceNames[index]);
    }
    for (std::size_t index = 0; index < kEvaluationFloatNames.size(); ++index) {
      capture_float(&event->floats[index], parameters,
                    kEvaluationFloatNames[index]);
    }
    for (std::size_t index = 0; index < kEvaluationIntNames.size(); ++index) {
      capture_int(&event->ints[index], parameters, kEvaluationIntNames[index]);
    }
  }
  event->getter_exception = g_getter_exception.load(std::memory_order_acquire);
  publish_event(event);
}

void capture_release_event(ngx::Result result,
                           ngx::Handle *feature_handle) noexcept {
  CaptureEvent *event = reserve_event(EventKind::release);
  if (event == nullptr) {
    return;
  }
  event->call_result = result;
  event->feature_handle = reinterpret_cast<std::uintptr_t>(feature_handle);
  publish_event(event);
}

void capture_swap_chain_event(HRESULT result, IUnknown *device_or_queue,
                              SwapChainSnapshot snapshot) noexcept {
  CaptureEvent *event = reserve_event(EventKind::swap_chain);
  if (event == nullptr) {
    return;
  }
  event->call_result = static_cast<ngx::Result>(result);
  event->adapter = capture_adapter_from_unknown(device_or_queue);
  event->swap_chain = snapshot;
  publish_event(event);
}

HRESULT STDMETHODCALLTYPE hook_create_swap_chain(
    IDXGIFactory *factory, IUnknown *device_or_queue,
    DXGI_SWAP_CHAIN_DESC *description, IDXGISwapChain **output_swap_chain) {
  const HRESULT result = g_create_swap_chain(factory, device_or_queue,
                                             description, output_swap_chain);
  const DWORD original_last_error = GetLastError();
  if (!g_observer_enabled.load(std::memory_order_acquire)) {
    SetLastError(original_last_error);
    return result;
  }
  SwapChainSnapshot snapshot{};
  snapshot.creation_kind = SwapChainCreationKind::legacy;
  IDXGISwapChain *swap_chain{};
  if (SUCCEEDED(result) && output_swap_chain != nullptr) {
    __try {
      swap_chain = *output_swap_chain;
    } __except (observation_exception_filter(GetExceptionCode())) {
      swap_chain = nullptr;
    }
  }
  capture_legacy_swap_chain_description(swap_chain, &snapshot);
  if (SUCCEEDED(result)) {
    capture_swap_chain_event(result, device_or_queue, snapshot);
  }
  SetLastError(original_last_error);
  return result;
}

HRESULT STDMETHODCALLTYPE hook_create_swap_chain_for_hwnd(
    IDXGIFactory2 *factory, IUnknown *device_or_queue, HWND window,
    const DXGI_SWAP_CHAIN_DESC1 *description,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *fullscreen_description,
    IDXGIOutput *restrict_to_output, IDXGISwapChain1 **output_swap_chain) {
  const HRESULT result = g_create_swap_chain_for_hwnd(
      factory, device_or_queue, window, description, fullscreen_description,
      restrict_to_output, output_swap_chain);
  const DWORD original_last_error = GetLastError();
  if (!g_observer_enabled.load(std::memory_order_acquire)) {
    SetLastError(original_last_error);
    return result;
  }
  SwapChainSnapshot snapshot{};
  snapshot.creation_kind = SwapChainCreationKind::hwnd;
  IDXGISwapChain1 *swap_chain{};
  if (SUCCEEDED(result) && output_swap_chain != nullptr) {
    __try {
      swap_chain = *output_swap_chain;
    } __except (observation_exception_filter(GetExceptionCode())) {
      swap_chain = nullptr;
    }
  }
  capture_hwnd_swap_chain_description(swap_chain, window, &snapshot);
  if (SUCCEEDED(result)) {
    capture_swap_chain_event(result, device_or_queue, snapshot);
  }
  SetLastError(original_last_error);
  return result;
}

ngx::Result __cdecl hook_create_feature(ID3D12GraphicsCommandList *command_list,
                                        ngx::Feature feature,
                                        const ngx::Parameter *parameters,
                                        ngx::Handle **output_handle) {
  const ngx::Result result =
      g_create_feature(command_list, feature, parameters, output_handle);
  const DWORD original_last_error = GetLastError();
  if (!g_observer_enabled.load(std::memory_order_acquire)) {
    SetLastError(original_last_error);
    return result;
  }
  AdapterSnapshot adapter{};
  const bool d3d12_verified =
      validate_d3d12_command_list(command_list, &adapter);
  ngx::Handle *feature_handle = nullptr;
  if (result == ngx::result_success) {
    feature_handle = safe_load_handle(output_handle);
  }
  capture_create_event(result, command_list, feature, parameters,
                       feature_handle, d3d12_verified, adapter);
  SetLastError(original_last_error);
  return result;
}

ngx::Result __cdecl hook_evaluate_feature(
    ID3D12GraphicsCommandList *command_list, const ngx::Handle *feature_handle,
    const ngx::Parameter *parameters, ngx::ProgressCallback callback) {
  const ngx::Result result =
      g_evaluate_feature(command_list, feature_handle, parameters, callback);
  const DWORD original_last_error = GetLastError();
  if (!g_observer_enabled.load(std::memory_order_acquire)) {
    SetLastError(original_last_error);
    return result;
  }
  const std::uint32_t evaluation =
      g_evaluation_calls.fetch_add(1, std::memory_order_relaxed) + 1U;
  if (evaluation <= g_evaluation_limit) {
    AdapterSnapshot adapter{};
    const bool d3d12_verified =
        validate_d3d12_command_list(command_list, &adapter);
    capture_evaluate_event(result, command_list, feature_handle, parameters,
                           d3d12_verified, adapter);
  }
  SetLastError(original_last_error);
  return result;
}

ngx::Result __cdecl hook_release_feature(ngx::Handle *feature_handle) {
  const ngx::Result result = g_release_feature(feature_handle);
  const DWORD original_last_error = GetLastError();
  if (!g_observer_enabled.load(std::memory_order_acquire)) {
    SetLastError(original_last_error);
    return result;
  }
  capture_release_event(result, feature_handle);
  SetLastError(original_last_error);
  return result;
}

const char *event_kind_name(EventKind kind) noexcept {
  switch (kind) {
  case EventKind::swap_chain:
    return "swap-chain";
  case EventKind::create:
    return "create";
  case EventKind::evaluate:
    return "evaluate";
  case EventKind::release:
    return "release";
  default:
    return "unknown";
  }
}

bool luid_equal(const LUID &left, const LUID &right) noexcept {
  return left.LowPart == right.LowPart && left.HighPart == right.HighPart;
}

void write_luid(const LUID &luid) {
  g_log_file << std::hex << std::setfill('0') << std::setw(8)
             << static_cast<std::uint32_t>(luid.HighPart) << ':' << std::setw(8)
             << luid.LowPart << std::setfill(' ') << std::dec;
}

std::string adapter_description_to_utf8(const wchar_t *description) {
  if (description == nullptr || *description == L'\0') {
    return {};
  }
  const int required =
      WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, description, -1,
                          nullptr, 0, nullptr, nullptr);
  if (required <= 1) {
    return {};
  }
  std::string result(static_cast<std::size_t>(required), '\0');
  if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, description, -1,
                          result.data(), required, nullptr, nullptr) == 0) {
    return {};
  }
  result.pop_back();
  for (char &character : result) {
    if (character == '\t' || character == '\r' || character == '\n') {
      character = ' ';
    }
  }
  return result;
}

bool resolve_adapter_description(const LUID &luid,
                                 DXGI_ADAPTER_DESC1 *description) noexcept {
  if (description == nullptr) {
    return false;
  }
  IDXGIFactory4 *factory{};
  IDXGIAdapter1 *adapter{};
  const HRESULT factory_result = CreateDXGIFactory2(
      0, __uuidof(IDXGIFactory4), reinterpret_cast<void **>(&factory));
  HRESULT adapter_result = E_FAIL;
  HRESULT description_result = E_FAIL;
  if (SUCCEEDED(factory_result) && factory != nullptr) {
    adapter_result = factory->EnumAdapterByLuid(
        luid, __uuidof(IDXGIAdapter1), reinterpret_cast<void **>(&adapter));
  }
  if (SUCCEEDED(adapter_result) && adapter != nullptr) {
    description_result = adapter->GetDesc1(description);
  }
  safe_release(adapter);
  safe_release(factory);
  return SUCCEEDED(description_result);
}

bool record_adapter_identity(std::string_view source,
                             const AdapterSnapshot &adapter) {
  if (!adapter.available) {
    return false;
  }

  DXGI_ADAPTER_DESC1 description{};
  const bool resolved = resolve_adapter_description(adapter.luid, &description);
  g_log_file << '\t' << "adapter.identity source=" << source << " luid=";
  write_luid(adapter.luid);
  g_log_file << " resolved=" << (resolved ? "true" : "false");
  if (resolved) {
    g_log_file << " description=\""
               << adapter_description_to_utf8(description.Description) << '"'
               << " vendor=0x" << std::hex << description.VendorId
               << " device=0x" << description.DeviceId << " subsys=0x"
               << description.SubSysId << " revision=0x" << description.Revision
               << std::dec << " dedicated_video_memory="
               << description.DedicatedVideoMemory;
  }
  g_log_file << '\n';
  return resolved;
}

void remember_swap_chain_candidate(const CaptureEvent &event) noexcept {
  const SwapChainSnapshot &swap_chain = event.swap_chain;
  if (!event.adapter.available || !swap_chain.description_available ||
      !swap_chain.window_owned_by_process || swap_chain.pointer == 0 ||
      swap_chain.width == 0 || swap_chain.height == 0 ||
      swap_chain.format == DXGI_FORMAT_UNKNOWN) {
    return;
  }

  for (SwapChainCandidate &candidate : g_writer_identity.candidates) {
    if (candidate.occupied &&
        candidate.swap_chain.pointer == swap_chain.pointer) {
      candidate.adapter = event.adapter;
      candidate.swap_chain = swap_chain;
      return;
    }
  }
  for (SwapChainCandidate &candidate : g_writer_identity.candidates) {
    if (!candidate.occupied) {
      candidate.occupied = true;
      candidate.adapter = event.adapter;
      candidate.swap_chain = swap_chain;
      return;
    }
  }
  g_writer_identity.candidate_overflow = true;
}

const char *identity_state_reason(IdentityReportState state) noexcept {
  switch (state) {
  case IdentityReportState::awaiting_adapter:
    return "awaiting-ngx-adapter";
  case IdentityReportState::adapter_unresolved:
    return "adapter-description-unresolved";
  case IdentityReportState::awaiting_swap_chain:
    return "awaiting-unambiguous-swap-chain";
  case IdentityReportState::ambiguous:
    return "multiple-swap-chain-candidates";
  default:
    return "unknown";
  }
}

void maybe_write_runtime_identity() {
  IdentityReportState state = IdentityReportState::awaiting_adapter;
  const SwapChainCandidate *selected{};
  std::size_t matching_candidates{};
  if (g_writer_identity.ngx_adapter_available) {
    state = g_writer_identity.ngx_adapter_description_available
                ? IdentityReportState::awaiting_swap_chain
                : IdentityReportState::adapter_unresolved;
    if (g_writer_identity.ngx_adapter_description_available) {
      for (const SwapChainCandidate &candidate : g_writer_identity.candidates) {
        if (candidate.occupied && candidate.adapter.available &&
            luid_equal(candidate.adapter.luid, g_writer_identity.ngx_adapter)) {
          ++matching_candidates;
          selected = &candidate;
        }
      }
      if (g_writer_identity.candidate_overflow || matching_candidates > 1U) {
        state = IdentityReportState::ambiguous;
        selected = nullptr;
      } else if (matching_candidates == 1U) {
        state = IdentityReportState::complete;
      }
    }
  }

  const bool complete_changed =
      state == IdentityReportState::complete && selected != nullptr &&
      (g_writer_identity.last_complete_swap_chain !=
           selected->swap_chain.pointer ||
       g_writer_identity.last_complete_width != selected->swap_chain.width ||
       g_writer_identity.last_complete_height != selected->swap_chain.height ||
       g_writer_identity.last_complete_format != selected->swap_chain.format);
  if (state == g_writer_identity.last_report_state && !complete_changed) {
    return;
  }

  ++g_writer_identity.revision;
  if (state != IdentityReportState::complete || selected == nullptr) {
    g_log_file << "runtime.identity complete=false revision="
               << g_writer_identity.revision
               << " reason=" << identity_state_reason(state) << " experiment="
               << (g_query_parameters ? "legacy-parameter-capture"
                                      : "legacy-lifecycle")
               << '\n';
  } else {
    g_log_file << "runtime.identity complete=true executable_sha256="
               << hash_to_hex(g_observed_host_sha256)
               << " revision=" << g_writer_identity.revision
               << " api=d3d12 adapter_luid=";
    write_luid(g_writer_identity.ngx_adapter);
    g_log_file << " swap_chain=0x" << std::hex << selected->swap_chain.pointer
               << " hwnd=0x" << selected->swap_chain.window << std::dec
               << " swap_chain_width=" << selected->swap_chain.width
               << " swap_chain_height=" << selected->swap_chain.height
               << " swap_chain_format="
               << static_cast<unsigned int>(selected->swap_chain.format)
               << " observation=creation experiment="
               << (g_query_parameters ? "legacy-parameter-capture"
                                      : "legacy-lifecycle")
               << '\n';
    g_writer_identity.last_complete_swap_chain = selected->swap_chain.pointer;
    g_writer_identity.last_complete_width = selected->swap_chain.width;
    g_writer_identity.last_complete_height = selected->swap_chain.height;
    g_writer_identity.last_complete_format = selected->swap_chain.format;
  }
  g_writer_identity.last_report_state = state;
}

void write_scalar(const ScalarSnapshot<float> &value) {
  g_log_file << "\tparameter.float name=" << value.name << " getter=0x"
             << std::hex << value.getter_result << std::dec;
  if (value.getter_result == ngx::result_success) {
    g_log_file << " value=" << std::setprecision(9) << value.value;
  }
  g_log_file << '\n';
}

void write_scalar(const ScalarSnapshot<int> &value) {
  g_log_file << "\tparameter.int name=" << value.name << " getter=0x"
             << std::hex << value.getter_result << std::dec;
  if (value.getter_result == ngx::result_success) {
    g_log_file << " value=" << value.value;
  }
  g_log_file << '\n';
}

void write_scalar(const ScalarSnapshot<unsigned int> &value) {
  g_log_file << "\tparameter.uint name=" << value.name << " getter=0x"
             << std::hex << value.getter_result << std::dec;
  if (value.getter_result == ngx::result_success) {
    g_log_file << " value=" << value.value;
  }
  g_log_file << '\n';
}

void write_resource(const ResourceSnapshot &resource) {
  g_log_file << "\tparameter.resource name=" << resource.name << " getter=0x"
             << std::hex << resource.getter_result << " ptr=0x"
             << resource.pointer << std::dec;
  if (resource.description_available) {
    const D3D12_RESOURCE_DESC &description = resource.description;
    g_log_file << " dimension="
               << static_cast<unsigned int>(description.Dimension)
               << " alignment=" << description.Alignment
               << " width=" << description.Width
               << " height=" << description.Height
               << " depth_or_array=" << description.DepthOrArraySize
               << " mips=" << description.MipLevels
               << " format=" << static_cast<unsigned int>(description.Format)
               << " sample_count=" << description.SampleDesc.Count
               << " sample_quality=" << description.SampleDesc.Quality
               << " layout=" << static_cast<unsigned int>(description.Layout)
               << " flags=0x" << std::hex
               << static_cast<unsigned int>(description.Flags) << std::dec;
  }
  g_log_file << '\n';
}

void write_event(const CaptureEvent &event) {
  if (event.kind == EventKind::swap_chain) {
    g_log_file << event.tick << "\ttid=" << event.thread_id
               << "\tevent=swap-chain sequence=" << event.sequence
               << " result=0x" << std::hex << event.call_result << std::dec
               << " creation="
               << (event.swap_chain.creation_kind ==
                           SwapChainCreationKind::legacy
                       ? "legacy"
                       : "hwnd")
               << " api=" << (event.adapter.available ? "d3d12" : "unverified")
               << " desc="
               << (event.swap_chain.description_available ? "available"
                                                          : "unavailable");
    if (event.swap_chain.description_available) {
      g_log_file << " width=" << event.swap_chain.width
                 << " height=" << event.swap_chain.height << " format="
                 << static_cast<unsigned int>(event.swap_chain.format)
                 << " buffers=" << event.swap_chain.buffer_count
                 << " swap_chain=0x" << std::hex << event.swap_chain.pointer
                 << " hwnd=0x" << event.swap_chain.window << std::dec
                 << " window_process="
                 << (event.swap_chain.window_owned_by_process ? "self"
                                                              : "other")
                 << " swap_effect="
                 << static_cast<unsigned int>(event.swap_chain.swap_effect)
                 << " flags=0x" << std::hex << event.swap_chain.flags
                 << std::dec;
    }
    g_log_file << '\n';
    record_adapter_identity("swap-chain", event.adapter);
    remember_swap_chain_candidate(event);
    maybe_write_runtime_identity();
    return;
  }

  g_log_file << event.tick << "\ttid=" << event.thread_id
             << "\tevent=" << event_kind_name(event.kind)
             << " sequence=" << event.sequence << " result=0x" << std::hex
             << event.call_result << " cmd=0x" << event.command_list
             << " params=0x" << event.parameters << " handle=0x"
             << event.feature_handle << std::dec;
  if (event.kind != EventKind::release) {
    g_log_file << " api="
               << (event.d3d12_verified ? "d3d12" : "unverified-rejected");
    if (!event.d3d12_verified) {
      g_log_file << " parameter_query=blocked-api";
    } else if (event.getter_exception) {
      g_log_file << " parameter_query=stopped-exception";
    } else if (event.parameter_query_attempted) {
      g_log_file << " parameter_query=queried";
    } else {
      g_log_file << " parameter_query=disabled";
    }
  }
  if (event.kind == EventKind::create) {
    g_log_file << " feature=" << event.feature;
  }
  g_log_file << '\n';

  if (event.kind == EventKind::create && event.parameter_query_attempted) {
    for (const auto &value : event.uints) {
      write_scalar(value);
    }
    for (std::size_t index = 0; index < kCreationIntNames.size(); ++index) {
      write_scalar(event.ints[index]);
    }
  } else if (event.kind == EventKind::evaluate &&
             event.parameter_query_attempted) {
    for (const auto &resource : event.resources) {
      write_resource(resource);
    }
    for (const auto &value : event.floats) {
      write_scalar(value);
    }
    for (const auto &value : event.ints) {
      write_scalar(value);
    }
  }

  if (event.adapter.available) {
    const bool adapter_changed =
        !g_writer_identity.ngx_adapter_available ||
        !luid_equal(g_writer_identity.ngx_adapter, event.adapter.luid);
    if (adapter_changed) {
      g_writer_identity.ngx_adapter_available = true;
      g_writer_identity.ngx_adapter = event.adapter.luid;
      g_writer_identity.ngx_adapter_description_available = false;
    }
    if (adapter_changed ||
        !g_writer_identity.ngx_adapter_description_available) {
      g_writer_identity.ngx_adapter_description_available =
          record_adapter_identity("ngx-command-list", event.adapter);
    }
  }
  maybe_write_runtime_identity();
}

void drain_events() {
  bool wrote_any = false;
  while (g_next_event_to_write < g_events.size()) {
    CaptureEvent &event = g_events[g_next_event_to_write];
    if (event.ready.load(std::memory_order_acquire) == 0) {
      break;
    }
    write_event(event);
    ++g_next_event_to_write;
    wrote_any = true;
  }
  if (wrote_any) {
    g_log_file.flush();
  }
}

bool resolve_host_getters(HMODULE host) {
  g_get_resource = reinterpret_cast<ngx::GetD3d12ResourceFn>(
      GetProcAddress(host, "NVSDK_NGX_Parameter_GetD3d12Resource"));
  g_get_float = reinterpret_cast<ngx::GetFloatFn>(
      GetProcAddress(host, "NVSDK_NGX_Parameter_GetF"));
  g_get_int = reinterpret_cast<ngx::GetIntFn>(
      GetProcAddress(host, "NVSDK_NGX_Parameter_GetI"));
  g_get_uint = reinterpret_cast<ngx::GetUIntFn>(
      GetProcAddress(host, "NVSDK_NGX_Parameter_GetUI"));
  return g_get_resource != nullptr && g_get_float != nullptr &&
         g_get_int != nullptr && g_get_uint != nullptr;
}

bool verify_feature_metadata(HMODULE feature_module) {
  const auto get_api = reinterpret_cast<ngx::GetMetadataFn>(
      GetProcAddress(feature_module, "NVSDK_NGX_GetAPIVersion"));
  const auto get_snippet = reinterpret_cast<ngx::GetMetadataFn>(
      GetProcAddress(feature_module, "NVSDK_NGX_GetSnippetVersion"));
  const auto get_application = reinterpret_cast<ngx::GetMetadataFn>(
      GetProcAddress(feature_module, "NVSDK_NGX_GetApplicationId"));
  if (get_api == nullptr || get_snippet == nullptr ||
      get_application == nullptr) {
    write_control_log("probe.abort reason=feature-metadata-export-missing");
    return false;
  }

  const std::uint32_t api = get_api();
  const std::uint32_t snippet = get_snippet();
  const std::uint32_t application = get_application();
  g_log_file << GetTickCount64() << "\ttid=" << GetCurrentThreadId()
             << "\tfeature.metadata api=0x" << std::hex << api << " snippet=0x"
             << snippet << " application=0x" << application << std::dec << '\n';
  g_log_file.flush();
  return api == kExpectedFeatureApi && snippet == kExpectedSnippetVersion &&
         application == kExpectedApplicationId;
}

bool live_bytes_match(void *address, const unsigned char *expected,
                      std::size_t size) noexcept {
  if (address == nullptr || expected == nullptr || size == 0) {
    return false;
  }
  __try {
    const auto *actual = static_cast<const unsigned char *>(address);
    for (std::size_t index = 0; index < size; ++index) {
      if (actual[index] != expected[index]) {
        return false;
      }
    }
    return true;
  } __except (observation_exception_filter(GetExceptionCode())) {
    return false;
  }
}

bool create_and_queue_hook(HMODULE feature_module, const char *export_name,
                           const unsigned char *expected_prologue,
                           std::size_t expected_prologue_size,
                           bool allow_test_override, void *detour,
                           void **original) {
  void *target =
      reinterpret_cast<void *>(GetProcAddress(feature_module, export_name));
  if (target == nullptr) {
    write_control_log(
        std::string("probe.abort reason=feature-export-missing name=") +
        export_name);
    return false;
  }

  if (!live_bytes_match(target, expected_prologue, expected_prologue_size)) {
    if (!allow_test_override) {
      write_control_log(
          std::string("probe.abort reason=target-prologue-mismatch name=") +
          export_name);
      return false;
    }
    write_control_log(std::string("target.prologue.override name=") +
                      export_name + " warning=explicit-test-override");
  }

  const MH_STATUS create_status = MH_CreateHook(target, detour, original);
  if (create_status != MH_OK) {
    write_control_log(
        std::string("probe.abort reason=hook-create-failed name=") +
        export_name + " status=" + MH_StatusToString(create_status));
    return false;
  }
  const MH_STATUS queue_status = MH_QueueEnableHook(target);
  if (queue_status != MH_OK) {
    write_control_log(
        std::string("probe.abort reason=hook-queue-failed name=") +
        export_name + " status=" + MH_StatusToString(queue_status));
    return false;
  }
  return true;
}

bool install_feature_hooks(HMODULE feature_module, bool allow_test_override) {
  bool success = create_and_queue_hook(
      feature_module, "NVSDK_NGX_D3D12_CreateFeature",
      kExpectedCreatePrologue.data(), kExpectedCreatePrologue.size(),
      allow_test_override, reinterpret_cast<void *>(&hook_create_feature),
      reinterpret_cast<void **>(&g_create_feature));
  success =
      success &&
      create_and_queue_hook(
          feature_module, "NVSDK_NGX_D3D12_EvaluateFeature",
          kExpectedEvaluatePrologue.data(), kExpectedEvaluatePrologue.size(),
          allow_test_override, reinterpret_cast<void *>(&hook_evaluate_feature),
          reinterpret_cast<void **>(&g_evaluate_feature));
  success = success && create_and_queue_hook(
                           feature_module, "NVSDK_NGX_D3D12_ReleaseFeature",
                           kExpectedReleasePrologue.data(),
                           kExpectedReleasePrologue.size(), allow_test_override,
                           reinterpret_cast<void *>(&hook_release_feature),
                           reinterpret_cast<void **>(&g_release_feature));

  if (!success || MH_ApplyQueued() != MH_OK) {
    write_control_log("probe.abort reason=feature-hook-install-failed");
    return false;
  }
  return true;
}

bool is_executable_protection(DWORD protection) noexcept {
  switch (protection & 0xFFU) {
  case PAGE_EXECUTE:
  case PAGE_EXECUTE_READ:
  case PAGE_EXECUTE_READWRITE:
  case PAGE_EXECUTE_WRITECOPY:
    return true;
  default:
    return false;
  }
}

bool looks_like_existing_detour(void *target) noexcept {
  if (target == nullptr) {
    return true;
  }
  __try {
    const auto *bytes = static_cast<const unsigned char *>(target);
    return bytes[0] == 0xE9U || bytes[0] == 0xEBU ||
           (bytes[0] == 0xFFU && bytes[1] == 0x25U) ||
           (bytes[0] == 0x48U && bytes[1] == 0xB8U && bytes[10] == 0xFFU &&
            bytes[11] == 0xE0U) ||
           (bytes[0] == 0x49U && bytes[1] == 0xBBU && bytes[10] == 0x41U &&
            bytes[11] == 0xFFU && bytes[12] == 0xE3U);
  } __except (observation_exception_filter(GetExceptionCode())) {
    return true;
  }
}

bool is_system_dxgi_target(void *target) {
  if (target == nullptr) {
    return false;
  }

  MEMORY_BASIC_INFORMATION memory{};
  if (VirtualQuery(target, &memory, sizeof(memory)) != sizeof(memory) ||
      memory.Type != MEM_IMAGE || !is_executable_protection(memory.Protect) ||
      looks_like_existing_detour(target)) {
    return false;
  }

  HMODULE owner{};
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                          reinterpret_cast<LPCWSTR>(target), &owner)) {
    return false;
  }

  wchar_t system_directory[32'768]{};
  const UINT system_directory_size = GetSystemDirectoryW(
      system_directory, static_cast<UINT>(_countof(system_directory)));
  const auto owner_path = module_path(owner);
  const auto expected_path =
      system_directory_size > 0 &&
              system_directory_size < _countof(system_directory)
          ? std::filesystem::path(system_directory) / L"dxgi.dll"
          : std::filesystem::path{};
  const bool valid = memory.AllocationBase == owner && !owner_path.empty() &&
                     !expected_path.empty() &&
                     _wcsicmp(owner_path.c_str(), expected_path.c_str()) == 0;
  if (!valid) {
    FreeLibrary(owner);
    return false;
  }

  if (g_dxgi_module == nullptr) {
    g_dxgi_module = owner;
  } else {
    const bool same_module = g_dxgi_module == owner;
    FreeLibrary(owner);
    if (!same_module) {
      return false;
    }
  }
  return true;
}

bool create_and_queue_dxgi_hook(void *target, const char *name, void *detour,
                                void **original) {
  if (!is_system_dxgi_target(target)) {
    write_control_log(
        std::string("probe.abort reason=dxgi-target-rejected name=") + name);
    return false;
  }
  const MH_STATUS create_status = MH_CreateHook(target, detour, original);
  if (create_status != MH_OK) {
    write_control_log(
        std::string("probe.abort reason=dxgi-hook-create-failed name=") + name +
        " status=" + MH_StatusToString(create_status));
    return false;
  }
  const MH_STATUS queue_status = MH_QueueEnableHook(target);
  if (queue_status != MH_OK) {
    write_control_log(
        std::string("probe.abort reason=dxgi-hook-queue-failed name=") + name +
        " status=" + MH_StatusToString(queue_status));
    return false;
  }
  return true;
}

bool install_dxgi_hooks() {
  IDXGIFactory2 *factory{};
  const HRESULT result = CreateDXGIFactory2(
      0, __uuidof(IDXGIFactory2), reinterpret_cast<void **>(&factory));
  if (FAILED(result) || factory == nullptr) {
    write_control_log("probe.abort reason=private-dxgi-factory-unavailable");
    return false;
  }

  void *create_swap_chain_target{};
  void *create_swap_chain_for_hwnd_target{};
  __try {
    void **vtable = *reinterpret_cast<void ***>(factory);
    create_swap_chain_target = vtable[kCreateSwapChainVtableIndex];
    create_swap_chain_for_hwnd_target =
        vtable[kCreateSwapChainForHwndVtableIndex];
  } __except (observation_exception_filter(GetExceptionCode())) {
    create_swap_chain_target = nullptr;
    create_swap_chain_for_hwnd_target = nullptr;
  }
  safe_release(factory);

  const bool success =
      create_and_queue_dxgi_hook(
          create_swap_chain_target, "CreateSwapChain",
          reinterpret_cast<void *>(&hook_create_swap_chain),
          reinterpret_cast<void **>(&g_create_swap_chain)) &&
      create_and_queue_dxgi_hook(
          create_swap_chain_for_hwnd_target, "CreateSwapChainForHwnd",
          reinterpret_cast<void *>(&hook_create_swap_chain_for_hwnd),
          reinterpret_cast<void **>(&g_create_swap_chain_for_hwnd));
  if (!success) {
    write_control_log("probe.abort reason=dxgi-hook-install-failed");
    return false;
  }
  g_observer_enabled.store(true, std::memory_order_release);
  if (MH_ApplyQueued() != MH_OK) {
    write_control_log("probe.abort reason=dxgi-hook-apply-failed");
    return false;
  }
  write_control_log(
      "dxgi.observer ready=true hooks=CreateSwapChain,CreateSwapChainForHwnd "
      "target=system-dxgi");
  return true;
}

[[noreturn]] void remain_loaded_after_hook_failure() noexcept {
  g_observer_enabled.store(false, std::memory_order_release);
  g_probe_state.store(-1, std::memory_order_release);
  const MH_STATUS disable_status = MH_DisableHook(MH_ALL_HOOKS);
  try {
    g_log_file << GetTickCount64() << "\ttid=" << GetCurrentThreadId()
               << "\tprobe.failure hooks-disabled-status="
               << MH_StatusToString(disable_status)
               << " lifetime=process-retained\n";
    g_log_file.flush();
  } catch (...) {
  }
  for (;;) {
    if (g_writer_wakeup != nullptr) {
      WaitForSingleObject(g_writer_wakeup, 1'000U);
    } else {
      Sleep(1'000U);
    }
    try {
      drain_events();
    } catch (...) {
    }
  }
}

DWORD initialize_probe_impl() {
  try {
    const auto path = get_log_path();
    if (!path.parent_path().empty()) {
      std::filesystem::create_directories(path.parent_path());
    }
    g_log_file.open(path, std::ios::out | std::ios::trunc);
  } catch (...) {
    g_probe_state.store(-1, std::memory_order_release);
    return 1;
  }
  if (!g_log_file.is_open()) {
    g_probe_state.store(-1, std::memory_order_release);
    return 1;
  }

  write_control_log("probe.start version=0.3.0 mode=feature-boundary-observer");
  g_evaluation_limit = read_evaluation_limit();
  g_query_parameters = read_parameter_query_setting();

  HMODULE pinned_probe{};
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_PIN,
                          reinterpret_cast<LPCWSTR>(g_probe_module),
                          &pinned_probe)) {
    write_control_log("probe.abort reason=probe-pin-failed");
    g_probe_state.store(-1, std::memory_order_release);
    return 2;
  }

  HMODULE host = GetModuleHandleW(nullptr);
  const bool override_enabled = test_override_enabled();
  if (host == nullptr ||
      !verify_module_hash(host, kExpectedHostSha256, "host", override_enabled,
                          &g_observed_host_sha256) ||
      !resolve_host_getters(host)) {
    write_control_log("probe.abort reason=unsupported-host-or-getters");
    g_probe_state.store(-1, std::memory_order_release);
    return 3;
  }

  g_writer_wakeup = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (g_writer_wakeup == nullptr) {
    write_control_log("probe.abort reason=writer-event-unavailable");
    g_probe_state.store(-1, std::memory_order_release);
    return 4;
  }
  if (MH_Initialize() != MH_OK) {
    CloseHandle(g_writer_wakeup);
    g_writer_wakeup = nullptr;
    write_control_log("probe.abort reason=minhook-initialize-failed");
    g_probe_state.store(-1, std::memory_order_release);
    return 4;
  }
  g_hook_engine_initialized.store(true, std::memory_order_release);
  if (!install_dxgi_hooks()) {
    write_control_log(
        "probe.abort reason=dxgi-observer-unavailable action=remain-loaded");
    remain_loaded_after_hook_failure();
  }

  HMODULE feature_module{};
  while (feature_module == nullptr) {
    feature_module = GetModuleHandleW(L"nvngx_dlss.dll");
    if (feature_module == nullptr) {
      WaitForSingleObject(g_writer_wakeup, 100U);
      drain_events();
    }
  }

  HMODULE retained_feature{};
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                          reinterpret_cast<LPCWSTR>(feature_module),
                          &retained_feature) ||
      retained_feature != feature_module) {
    if (retained_feature != nullptr) {
      FreeLibrary(retained_feature);
    }
    write_control_log("probe.abort reason=feature-module-retain-failed");
    remain_loaded_after_hook_failure();
  }

  if (!verify_module_hash(feature_module, kExpectedFeatureSha256, "feature",
                          override_enabled) ||
      !verify_feature_metadata(feature_module)) {
    FreeLibrary(retained_feature);
    write_control_log("probe.abort reason=unsupported-feature-module");
    remain_loaded_after_hook_failure();
  }

  if (!install_feature_hooks(feature_module, override_enabled)) {
    write_control_log(
        "probe.abort reason=feature-hooks-unavailable action=remain-loaded");
    remain_loaded_after_hook_failure();
  }

  HMODULE pinned_feature{};
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_PIN,
                          reinterpret_cast<LPCWSTR>(feature_module),
                          &pinned_feature) ||
      pinned_feature != feature_module) {
    write_control_log(
        "probe.abort reason=feature-module-pin-failed action=remain-loaded");
    remain_loaded_after_hook_failure();
  }
  FreeLibrary(retained_feature);

  write_control_log("probe.ready hooks=feature-create,evaluate,release "
                    "host-code-patched=false capture-limit=" +
                    std::to_string(g_evaluation_limit) + " parameter-query=" +
                    (g_query_parameters ? "enabled" : "disabled"));
  write_control_log(
      std::string(
          "runtime.identity complete=false status=awaiting-observations ") +
      "experiment=" +
      (g_query_parameters ? "legacy-parameter-capture" : "legacy-lifecycle"));
  g_probe_state.store(1, std::memory_order_release);

  for (;;) {
    WaitForSingleObject(g_writer_wakeup, 1'000U);
    drain_events();
  }
}

DWORD WINAPI initialize_probe(void *) noexcept {
  try {
    return initialize_probe_impl();
  } catch (...) {
    try {
      write_control_log(
          "probe.abort reason=unhandled-initialization-exception");
    } catch (...) {
    }
    if (g_hook_engine_initialized.load(std::memory_order_acquire)) {
      remain_loaded_after_hook_failure();
    }
    g_probe_state.store(-1, std::memory_order_release);
    return 99;
  }
}

} // namespace

extern "C" __declspec(dllexport) int __cdecl MhwDlssProbeGetState() {
  return g_probe_state.load(std::memory_order_acquire);
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_probe_module = module;
    DisableThreadLibraryCalls(module);
    if (HANDLE thread =
            CreateThread(nullptr, 0, initialize_probe, nullptr, 0, nullptr)) {
      CloseHandle(thread);
    } else {
      g_probe_state.store(-1, std::memory_order_release);
    }
  }
  return TRUE;
}
