#include "ngx_abi.hpp"

#include <MinHook.h>
#include <Windows.h>
#include <bcrypt.h>
#include <d3d12.h>
#include <dxgiformat.h>

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
constexpr DWORD kHashBufferSize = 1U << 20U;

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
  create,
  evaluate,
  release,
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
std::atomic_int g_d3d12_command_state{};
std::atomic_bool g_getter_exception{};
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
                        std::string_view label, bool allow_override) {
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

bool validate_d3d12_command_list(
    ID3D12GraphicsCommandList *command_list) noexcept {
  const int cached = g_d3d12_command_state.load(std::memory_order_acquire);
  if (cached != 0) {
    return cached > 0;
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
      verified->Release();
    }
  } __except (observation_exception_filter(GetExceptionCode())) {
    result = E_FAIL;
    verified = nullptr;
  }

  const int observed = SUCCEEDED(result) && verified != nullptr ? 1 : -1;
  int expected = 0;
  g_d3d12_command_state.compare_exchange_strong(
      expected, observed, std::memory_order_release, std::memory_order_relaxed);
  return g_d3d12_command_state.load(std::memory_order_acquire) > 0;
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
                          ngx::Handle *feature_handle,
                          bool d3d12_verified) noexcept {
  CaptureEvent *event = reserve_event(EventKind::create);
  if (event == nullptr) {
    return;
  }

  event->call_result = result;
  event->d3d12_verified = d3d12_verified;
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
                            bool d3d12_verified) noexcept {
  const std::uint32_t evaluation =
      g_evaluation_calls.fetch_add(1, std::memory_order_relaxed) + 1U;
  if (evaluation > g_evaluation_limit) {
    return;
  }

  CaptureEvent *event = reserve_event(EventKind::evaluate);
  if (event == nullptr) {
    return;
  }

  event->call_result = result;
  event->d3d12_verified = d3d12_verified;
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

ngx::Result __cdecl hook_create_feature(ID3D12GraphicsCommandList *command_list,
                                        ngx::Feature feature,
                                        const ngx::Parameter *parameters,
                                        ngx::Handle **output_handle) {
  const ngx::Result result =
      g_create_feature(command_list, feature, parameters, output_handle);
  const DWORD original_last_error = GetLastError();
  const bool d3d12_verified = validate_d3d12_command_list(command_list);
  ngx::Handle *feature_handle = nullptr;
  if (result == ngx::result_success) {
    feature_handle = safe_load_handle(output_handle);
  }
  capture_create_event(result, command_list, feature, parameters,
                       feature_handle, d3d12_verified);
  SetLastError(original_last_error);
  return result;
}

ngx::Result __cdecl hook_evaluate_feature(
    ID3D12GraphicsCommandList *command_list, const ngx::Handle *feature_handle,
    const ngx::Parameter *parameters, ngx::ProgressCallback callback) {
  const ngx::Result result =
      g_evaluate_feature(command_list, feature_handle, parameters, callback);
  const DWORD original_last_error = GetLastError();
  const bool d3d12_verified = validate_d3d12_command_list(command_list);
  capture_evaluate_event(result, command_list, feature_handle, parameters,
                         d3d12_verified);
  SetLastError(original_last_error);
  return result;
}

ngx::Result __cdecl hook_release_feature(ngx::Handle *feature_handle) {
  const ngx::Result result = g_release_feature(feature_handle);
  const DWORD original_last_error = GetLastError();
  capture_release_event(result, feature_handle);
  SetLastError(original_last_error);
  return result;
}

const char *event_kind_name(EventKind kind) noexcept {
  switch (kind) {
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
  if (MH_Initialize() != MH_OK) {
    write_control_log("probe.abort reason=minhook-initialize-failed");
    return false;
  }

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
    MH_DisableHook(MH_ALL_HOOKS);
    MH_RemoveHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    write_control_log("probe.abort reason=hook-install-transaction-failed");
    return false;
  }
  return true;
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

  write_control_log("probe.start version=0.2.0 mode=feature-boundary-observer");
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
      !verify_module_hash(host, kExpectedHostSha256, "host",
                          override_enabled) ||
      !resolve_host_getters(host)) {
    write_control_log("probe.abort reason=unsupported-host-or-getters");
    g_probe_state.store(-1, std::memory_order_release);
    return 3;
  }

  HMODULE feature_module{};
  while (feature_module == nullptr) {
    feature_module = GetModuleHandleW(L"nvngx_dlss.dll");
    if (feature_module == nullptr) {
      Sleep(100);
    }
  }

  HMODULE retained_feature{};
  if (!GetModuleHandleExW(0, L"nvngx_dlss.dll", &retained_feature) ||
      retained_feature != feature_module) {
    if (retained_feature != nullptr) {
      FreeLibrary(retained_feature);
    }
    write_control_log("probe.abort reason=feature-module-retain-failed");
    g_probe_state.store(-1, std::memory_order_release);
    return 5;
  }

  if (!verify_module_hash(feature_module, kExpectedFeatureSha256, "feature",
                          override_enabled) ||
      !verify_feature_metadata(feature_module)) {
    FreeLibrary(retained_feature);
    write_control_log("probe.abort reason=unsupported-feature-module");
    g_probe_state.store(-1, std::memory_order_release);
    return 5;
  }

  HMODULE pinned_feature{};
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, L"nvngx_dlss.dll",
                          &pinned_feature) ||
      pinned_feature != feature_module) {
    FreeLibrary(retained_feature);
    write_control_log("probe.abort reason=feature-module-pin-failed");
    g_probe_state.store(-1, std::memory_order_release);
    return 5;
  }
  FreeLibrary(retained_feature);

  g_writer_wakeup = CreateEventW(nullptr, FALSE, FALSE, nullptr);
  if (g_writer_wakeup == nullptr ||
      !install_feature_hooks(feature_module, override_enabled)) {
    write_control_log("probe.abort reason=writer-or-hooks-unavailable");
    g_probe_state.store(-1, std::memory_order_release);
    return 6;
  }

  write_control_log("probe.ready hooks=feature-create,evaluate,release "
                    "host-code-patched=false capture-limit=" +
                    std::to_string(g_evaluation_limit) + " parameter-query=" +
                    (g_query_parameters ? "enabled" : "disabled"));
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
    write_control_log("probe.abort reason=unhandled-initialization-exception");
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
