#include "../src/probe/ngx_abi.hpp"

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <atomic>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using namespace mhw;

struct MockParameterState {
  unsigned int marker = 0x4D485750U;
};

constexpr ngx::Result kCreateResult = 0xBAD00021U;
constexpr ngx::Result kEvaluateResult = 0xBAD00022U;
constexpr ngx::Result kReleaseResult = 0xBAD00023U;
constexpr DWORD kCreateLastError = 0x201U;
constexpr DWORD kEvaluateLastError = 0x202U;
constexpr DWORD kReleaseLastError = 0x203U;
constexpr unsigned int kEvaluationCount = 4U;

std::atomic_uint g_host_callback_calls{};
std::atomic_uint g_get_device_calls{};
std::atomic_uint g_window_serial{};
bool g_accept_d3d12 = true;
ID3D12Device *g_fake_device{};

struct FakeCommandList;
using QueryInterfaceFn = HRESULT(STDMETHODCALLTYPE *)(FakeCommandList *, REFIID,
                                                      void **);
using AddRefFn = ULONG(STDMETHODCALLTYPE *)(FakeCommandList *);
using ReleaseFn = ULONG(STDMETHODCALLTYPE *)(FakeCommandList *);
using GetDeviceFn = HRESULT(STDMETHODCALLTYPE *)(FakeCommandList *, REFIID,
                                                 void **);

struct FakeCommandListVtable {
  QueryInterfaceFn query_interface;
  AddRefFn add_ref;
  ReleaseFn release;
  void *get_private_data;
  void *set_private_data;
  void *set_private_data_interface;
  void *set_name;
  GetDeviceFn get_device;
};

struct FakeCommandList {
  FakeCommandListVtable *vtable;
  std::atomic_ulong references{1};
};

ULONG STDMETHODCALLTYPE fake_add_ref(FakeCommandList *self) {
  return self->references.fetch_add(1) + 1;
}

ULONG STDMETHODCALLTYPE fake_release(FakeCommandList *self) {
  return self->references.fetch_sub(1) - 1;
}

HRESULT STDMETHODCALLTYPE fake_query_interface(FakeCommandList *self,
                                               REFIID interface_id,
                                               void **object) {
  if (object == nullptr) {
    return E_POINTER;
  }
  *object = nullptr;
  if (!IsEqualIID(interface_id, __uuidof(IUnknown)) &&
      !IsEqualIID(interface_id, __uuidof(ID3D12GraphicsCommandList))) {
    return E_NOINTERFACE;
  }
  if (!g_accept_d3d12 &&
      IsEqualIID(interface_id, __uuidof(ID3D12GraphicsCommandList))) {
    return E_NOINTERFACE;
  }
  *object = self;
  fake_add_ref(self);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE fake_get_device(FakeCommandList *,
                                          REFIID interface_id, void **object) {
  ++g_get_device_calls;
  if (object == nullptr) {
    return E_POINTER;
  }
  *object = nullptr;
  if (g_fake_device == nullptr) {
    return E_NOINTERFACE;
  }
  return g_fake_device->QueryInterface(interface_id, object);
}

FakeCommandListVtable g_fake_command_list_vtable{fake_query_interface,
                                                 fake_add_ref,
                                                 fake_release,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 nullptr,
                                                 fake_get_device};

class WarpSwapChainFixture {
public:
  WarpSwapChainFixture() = default;
  WarpSwapChainFixture(const WarpSwapChainFixture &) = delete;
  WarpSwapChainFixture &operator=(const WarpSwapChainFixture &) = delete;

  ~WarpSwapChainFixture() {
    if (g_fake_device == device_.Get()) {
      g_fake_device = nullptr;
    }
    swap_chain_.Reset();
    command_queue_.Reset();
    device_.Reset();
    adapter_.Reset();
    factory_.Reset();
    if (window_ != nullptr) {
      DestroyWindow(window_);
      window_ = nullptr;
    }
    if (window_class_ != 0) {
      UnregisterClassW(class_name_.c_str(), process_module_);
      window_class_ = 0;
    }
  }

  bool create() {
    HRESULT result = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory_));
    if (FAILED(result)) {
      return report_failure("CreateDXGIFactory2", result);
    }

    result = factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter_));
    if (FAILED(result)) {
      return report_failure("EnumWarpAdapter", result);
    }

    result = D3D12CreateDevice(adapter_.Get(), D3D_FEATURE_LEVEL_11_0,
                               IID_PPV_ARGS(&device_));
    if (FAILED(result)) {
      return report_failure("D3D12CreateDevice", result);
    }

    D3D12_COMMAND_QUEUE_DESC queue_description{};
    queue_description.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    result = device_->CreateCommandQueue(&queue_description,
                                         IID_PPV_ARGS(&command_queue_));
    if (FAILED(result)) {
      return report_failure("CreateCommandQueue", result);
    }

    process_module_ = GetModuleHandleW(nullptr);
    class_name_ = L"MhwDlssProbeMockWindow-" +
                  std::to_wstring(GetCurrentProcessId()) + L"-" +
                  std::to_wstring(g_window_serial.fetch_add(1));
    WNDCLASSEXW window_class{};
    window_class.cbSize = static_cast<UINT>(sizeof(window_class));
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = process_module_;
    window_class.lpszClassName = class_name_.c_str();
    window_class_ = RegisterClassExW(&window_class);
    if (window_class_ == 0) {
      std::cerr << "RegisterClassExW failed: " << GetLastError() << '\n';
      return false;
    }

    window_ =
        CreateWindowExW(0, class_name_.c_str(), L"MHW DLSS probe mock",
                        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 320,
                        240, nullptr, nullptr, process_module_, nullptr);
    if (window_ == nullptr) {
      std::cerr << "CreateWindowExW failed: " << GetLastError() << '\n';
      return false;
    }

    DXGI_SWAP_CHAIN_DESC1 swap_chain_description{};
    swap_chain_description.Width = 320;
    swap_chain_description.Height = 240;
    swap_chain_description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_description.SampleDesc.Count = 1;
    swap_chain_description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_description.BufferCount = 2;
    swap_chain_description.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_description.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    result = factory_->CreateSwapChainForHwnd(command_queue_.Get(), window_,
                                              &swap_chain_description, nullptr,
                                              nullptr, &swap_chain_);
    if (FAILED(result)) {
      return report_failure("CreateSwapChainForHwnd", result);
    }

    g_fake_device = device_.Get();
    return true;
  }

private:
  static bool report_failure(const char *operation, HRESULT result) {
    std::cerr << operation << " failed: 0x" << std::hex
              << static_cast<std::uint32_t>(result) << std::dec << '\n';
    return false;
  }

  Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
  Microsoft::WRL::ComPtr<IDXGIAdapter> adapter_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  HMODULE process_module_{};
  HWND window_{};
  ATOM window_class_{};
  std::wstring class_name_;
};

void __cdecl progress_callback(float progress, bool &should_cancel) {
  if (progress == 0.5F) {
    ++g_host_callback_calls;
  }
  should_cancel = false;
}

bool known_name(const char *name) noexcept {
  return name != nullptr && name[0] != '\0';
}

std::string read_text_file(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

} // namespace

extern "C" __declspec(dllexport) ngx::Result __cdecl
NVSDK_NGX_Parameter_GetD3d12Resource(ngx::Parameter *, const char *name,
                                     ID3D12Resource **value) {
  if (!known_name(name) || value == nullptr) {
    return 0;
  }
  *value = nullptr;
  return ngx::result_success;
}

extern "C" __declspec(dllexport) ngx::Result __cdecl
NVSDK_NGX_Parameter_GetF(ngx::Parameter *, const char *name, float *value) {
  if (!known_name(name) || value == nullptr) {
    return 0;
  }
  *value = std::strcmp(name, "Sharpness") == 0 ? 0.25F : 1.0F;
  return ngx::result_success;
}

extern "C" __declspec(dllexport) ngx::Result __cdecl
NVSDK_NGX_Parameter_GetI(ngx::Parameter *, const char *name, int *value) {
  if (!known_name(name) || value == nullptr) {
    return 0;
  }
  *value = std::strcmp(name, "Reset") == 0 ? 1 : 0;
  return ngx::result_success;
}

extern "C" __declspec(dllexport) ngx::Result __cdecl
NVSDK_NGX_Parameter_GetUI(ngx::Parameter *, const char *name,
                          unsigned int *value) {
  if (!known_name(name) || value == nullptr) {
    return 0;
  }
  wchar_t fault[2]{};
  if (std::strcmp(name, "Width") == 0 &&
      GetEnvironmentVariableW(L"MHW_PROBE_MOCK_GETTER_FAULT", fault,
                              static_cast<DWORD>(_countof(fault))) == 1 &&
      fault[0] == L'1') {
    RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
  }
  *value = std::strcmp(name, "Height") == 0 ? 1080U : 1920U;
  return ngx::result_success;
}

int wmain(int argc, wchar_t **argv) {
  if (argc != 4 && argc != 5) {
    std::cerr << "usage: probe_mock_host <feature.dll> <probe.dll> <log-path> "
                 "[hash-reject|metadata-reject|api-reject|getter-fault|"
                 "identity-ambiguous]\n";
    return 2;
  }

  const std::wstring_view mode = argc == 5 ? argv[4] : L"";
  const bool expect_hash_rejection = mode == L"hash-reject";
  const bool expect_metadata_rejection = mode == L"metadata-reject";
  const bool expect_api_rejection = mode == L"api-reject";
  const bool expect_getter_fault = mode == L"getter-fault";
  const bool expect_identity_ambiguity = mode == L"identity-ambiguous";
  const bool expect_probe_rejection =
      expect_hash_rejection || expect_metadata_rejection;
  if (argc == 5 && !expect_probe_rejection && !expect_api_rejection &&
      !expect_getter_fault && !expect_identity_ambiguity) {
    std::cerr << "unknown test mode\n";
    return 2;
  }

  const auto feature_path = std::filesystem::absolute(argv[1]);
  const auto probe_path = std::filesystem::absolute(argv[2]);
  const auto log_path = std::filesystem::absolute(argv[3]);
  g_accept_d3d12 = !expect_api_rejection;
  std::filesystem::remove(log_path);
  SetEnvironmentVariableW(L"MHW_DLSS_PROBE_LOG", log_path.c_str());
  SetEnvironmentVariableW(L"MHW_DLSS_PROBE_TEST_OVERRIDE",
                          expect_hash_rejection ? nullptr : L"I_UNDERSTAND");
  SetEnvironmentVariableW(L"MHW_PROBE_MOCK_BAD_METADATA",
                          expect_metadata_rejection ? L"1" : nullptr);
  SetEnvironmentVariableW(L"MHW_PROBE_MOCK_GETTER_FAULT",
                          expect_getter_fault ? L"1" : nullptr);
  SetEnvironmentVariableW(L"MHW_DLSS_PROBE_CAPTURE_EVALUATIONS", L"8");
  SetEnvironmentVariableW(L"MHW_DLSS_PROBE_QUERY_PARAMETERS", L"1");

  HMODULE feature_module = LoadLibraryW(feature_path.c_str());
  if (feature_module == nullptr) {
    std::cerr << "feature LoadLibrary failed: " << GetLastError() << '\n';
    return 3;
  }

  HMODULE probe_module = LoadLibraryW(probe_path.c_str());
  if (probe_module == nullptr) {
    std::cerr << "probe LoadLibrary failed: " << GetLastError() << '\n';
    return 4;
  }

  using GetStateFn = int(__cdecl *)();
  const auto get_state = reinterpret_cast<GetStateFn>(
      GetProcAddress(probe_module, "MhwDlssProbeGetState"));
  if (get_state == nullptr) {
    std::cerr << "probe state export missing\n";
    return 5;
  }

  for (int attempt = 0; attempt < 500 && get_state() == 0; ++attempt) {
    Sleep(10);
  }
  if (expect_probe_rejection) {
    if (get_state() != -1) {
      std::cerr << "probe accepted an unsupported host\n";
      return 6;
    }
    const std::string expected_reason =
        expect_hash_rejection ? "reason=host-hash-mismatch"
                              : "reason=unsupported-feature-module";
    std::string rejection_log = read_text_file(log_path);
    if (expect_metadata_rejection) {
      for (int attempt = 0;
           attempt < 100 &&
           rejection_log.find("probe.failure hooks-disabled-status=") ==
               std::string::npos;
           ++attempt) {
        Sleep(1);
        rejection_log = read_text_file(log_path);
      }
    }
    if (rejection_log.find(expected_reason) == std::string::npos) {
      std::cerr << "rejection reason missing\n" << rejection_log;
      return 6;
    }
    if (expect_metadata_rejection &&
        rejection_log.find("probe.failure hooks-disabled-status=MH_OK "
                           "lifetime=process-retained") == std::string::npos) {
      std::cerr << "live-hook failure policy missing\n" << rejection_log;
      return 6;
    }
    if (!FreeLibrary(feature_module) ||
        GetModuleHandleW(L"nvngx_dlss.dll") != nullptr) {
      std::cerr << "rejected feature module was pinned or leaked\n";
      return 6;
    }
    return 0;
  }
  if (get_state() != 1) {
    std::cerr << "probe did not become ready\n";
    if (std::filesystem::exists(log_path)) {
      std::cerr << read_text_file(log_path);
    }
    return 6;
  }

  WarpSwapChainFixture swap_chain_fixture;
  WarpSwapChainFixture second_swap_chain_fixture;
  if ((mode.empty() || expect_identity_ambiguity) &&
      !swap_chain_fixture.create()) {
    return 7;
  }
  if (expect_identity_ambiguity && !second_swap_chain_fixture.create()) {
    return 7;
  }

  const auto create_feature = reinterpret_cast<ngx::CreateFeatureFn>(
      GetProcAddress(feature_module, "NVSDK_NGX_D3D12_CreateFeature"));
  const auto evaluate_feature = reinterpret_cast<ngx::EvaluateFeatureFn>(
      GetProcAddress(feature_module, "NVSDK_NGX_D3D12_EvaluateFeature"));
  const auto release_feature = reinterpret_cast<ngx::ReleaseFeatureFn>(
      GetProcAddress(feature_module, "NVSDK_NGX_D3D12_ReleaseFeature"));
  using GetOriginalCallsFn = unsigned int(__cdecl *)();
  const auto get_original_calls = reinterpret_cast<GetOriginalCallsFn>(
      GetProcAddress(feature_module, "MockGetOriginalCalls"));
  const auto get_argument_failures = reinterpret_cast<GetOriginalCallsFn>(
      GetProcAddress(feature_module, "MockGetArgumentFailures"));
  const auto get_callback_calls = reinterpret_cast<GetOriginalCallsFn>(
      GetProcAddress(feature_module, "MockGetCallbackCalls"));
  if (create_feature == nullptr || evaluate_feature == nullptr ||
      release_feature == nullptr || get_original_calls == nullptr ||
      get_argument_failures == nullptr || get_callback_calls == nullptr) {
    std::cerr << "mock feature export missing\n";
    return 7;
  }

  MockParameterState state{};
  auto *parameters = reinterpret_cast<ngx::Parameter *>(&state);
  FakeCommandList command_list{&g_fake_command_list_vtable};
  auto *command_list_interface =
      reinterpret_cast<ID3D12GraphicsCommandList *>(&command_list);
  ngx::Handle *handle{};
  SetLastError(0);
  if (create_feature(command_list_interface, ngx::feature_super_sampling,
                     parameters, &handle) != kCreateResult ||
      GetLastError() != kCreateLastError) {
    std::cerr << "create forwarding or LastError mismatch\n";
    return 8;
  }

  std::atomic_uint evaluation_failures{};
  std::vector<std::thread> evaluation_threads;
  evaluation_threads.reserve(kEvaluationCount);
  for (unsigned int index = 0; index < kEvaluationCount; ++index) {
    evaluation_threads.emplace_back([&] {
      SetLastError(0);
      if (evaluate_feature(command_list_interface, handle, parameters,
                           progress_callback) != kEvaluateResult ||
          GetLastError() != kEvaluateLastError) {
        ++evaluation_failures;
      }
    });
  }
  for (auto &thread : evaluation_threads) {
    thread.join();
  }
  if (evaluation_failures.load() != 0U) {
    std::cerr << "evaluate forwarding or LastError mismatch\n";
    return 8;
  }

  SetLastError(0);
  if (release_feature(handle) != kReleaseResult ||
      GetLastError() != kReleaseLastError) {
    std::cerr << "release forwarding or LastError mismatch\n";
    return 8;
  }

  const unsigned int expected_original_calls = kEvaluationCount + 2U;
  if (get_original_calls() != expected_original_calls) {
    std::cerr << "original call count mismatch: " << get_original_calls()
              << '\n';
    return 9;
  }
  if (get_argument_failures() != 0U ||
      get_callback_calls() != kEvaluationCount ||
      g_host_callback_calls.load() != kEvaluationCount ||
      command_list.references.load() != 1U) {
    std::cerr << "argument, callback, or COM lifetime mismatch\n";
    return 9;
  }

  std::string log;
  for (int attempt = 0; attempt < 500; ++attempt) {
    if (std::filesystem::exists(log_path)) {
      log = read_text_file(log_path);
      const bool feature_events_complete =
          log.find("event=create") != std::string::npos &&
          log.find("event=evaluate") != std::string::npos &&
          log.find("event=release") != std::string::npos;
      const bool identity_complete =
          !mode.empty() ||
          log.find("runtime.identity complete=true") != std::string::npos;
      if (feature_events_complete && identity_complete) {
        break;
      }
    }
    Sleep(10);
  }
  if (expect_api_rejection) {
    if (log.find("api=unverified-rejected parameter_query=blocked-api") ==
            std::string::npos ||
        log.find("parameter.resource") != std::string::npos ||
        g_get_device_calls.load() != 0U) {
      std::cerr << "non-D3D12 parameter query was not blocked\n" << log;
      return 10;
    }
    return 0;
  }
  if (expect_getter_fault) {
    if (log.find("parameter_query=stopped-exception") == std::string::npos ||
        log.find("parameter.resource") != std::string::npos) {
      std::cerr << "getter exception did not stop later queries\n" << log;
      return 10;
    }
    return 0;
  }
  if (log.find("event=create") == std::string::npos ||
      log.find("event=evaluate") == std::string::npos ||
      log.find("event=release") == std::string::npos ||
      log.find("name=MotionVectors") == std::string::npos ||
      log.find("name=MV.Scale.X") == std::string::npos ||
      log.find("api=d3d12") == std::string::npos ||
      log.find("result=0xbad00022") == std::string::npos) {
    std::cerr << "expected capture records missing\n" << log;
    return 10;
  }
  if (expect_identity_ambiguity) {
    if (log.find("runtime.identity complete=false") == std::string::npos ||
        log.find("reason=multiple-swap-chain-candidates") ==
            std::string::npos ||
        log.find("runtime.identity complete=true") != std::string::npos ||
        g_get_device_calls.load() != kEvaluationCount + 1U) {
      std::cerr << "ambiguous identity was accepted\n" << log;
      return 10;
    }
    return 0;
  }
  const std::string expected_swap_chain =
      "creation=hwnd api=d3d12 desc=available width=320 height=240 format=28 "
      "buffers=2";
  const std::string expected_identity =
      "runtime.identity complete=true executable_sha256=";
  const std::string expected_identity_adapter = "api=d3d12 adapter_luid=";
  const std::string expected_identity_swap_chain =
      "swap_chain_width=320 swap_chain_height=240 swap_chain_format=28";
  if (log.find("event=swap-chain") == std::string::npos ||
      log.find(expected_swap_chain) == std::string::npos ||
      log.find(expected_identity) == std::string::npos ||
      log.find(expected_identity_adapter) == std::string::npos ||
      log.find(expected_identity_swap_chain) == std::string::npos ||
      log.find("window_process=self swap_effect=4 flags=0x0") ==
          std::string::npos ||
      log.find("observation=creation experiment=legacy-parameter-capture") ==
          std::string::npos ||
      g_get_device_calls.load() != kEvaluationCount + 1U) {
    std::cerr << "runtime identity capture missing or incomplete\n" << log;
    return 10;
  }

  return 0;
}
