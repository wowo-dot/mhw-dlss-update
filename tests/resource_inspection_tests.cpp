#include "resource_inspection.hpp"
#include "presentation_observer.hpp"
#include <MinHook.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>

namespace {
namespace b = mhw::bridge_probe;
using Microsoft::WRL::ComPtr;
void check(bool ok, const char *what) { if (!ok) throw std::runtime_error(what); }
void hr(HRESULT value, const char *what) { check(SUCCEEDED(value), what); }

// A real D3D12 device/list exercise GetDesc's ABI and device identity; these
// fabricated resources separately exercise cleanup and failure latching.
struct OtherIdentity final : IUnknown {
  ULONG refs{1};
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
    *out = nullptr;
    if (iid != __uuidof(IUnknown)) return E_NOINTERFACE;
    *out = this; AddRef(); return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
  ULONG STDMETHODCALLTYPE Release() override { return --refs; }
};
struct FakeResource final : ID3D12Resource {
  ID3D12Device *device{};
  IUnknown *different_device{};
  ULONG refs{1};
  unsigned desc_calls{}, device_calls{};
  bool reject{}, desc_fault{}, release_fault{};
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **out) override {
    *out = nullptr;
    if (reject || (iid != __uuidof(IUnknown) && iid != __uuidof(ID3D12Resource))) return E_NOINTERFACE;
    *out = this; AddRef(); return S_OK;
  }
  ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
  ULONG STDMETHODCALLTYPE Release() override {
    --refs;
    if (release_fault) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    return refs;
  }
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, UINT *, void *) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, UINT, const void *) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID, const IUnknown *) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE GetDevice(REFIID iid, void **out) override {
    ++device_calls;
    return different_device ? different_device->QueryInterface(iid, out) : device->QueryInterface(iid, out);
  }
  HRESULT STDMETHODCALLTYPE Map(UINT, const D3D12_RANGE *, void **) override { return E_NOTIMPL; }
  void STDMETHODCALLTYPE Unmap(UINT, const D3D12_RANGE *) override {}
  D3D12_RESOURCE_DESC STDMETHODCALLTYPE GetDesc() override {
    ++desc_calls;
    if (desc_fault) RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = 32; d.Height = 16; d.DepthOrArraySize = 1; d.MipLevels = 1;
    d.SampleDesc.Count = 1; d.Format = DXGI_FORMAT_R32_FLOAT;
    return d;
  }
  D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE GetGPUVirtualAddress() override { return 0; }
  HRESULT STDMETHODCALLTYPE WriteToSubresource(UINT, const D3D12_BOX *, const void *, UINT, UINT) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE ReadFromSubresource(void *, UINT, UINT, UINT, const D3D12_BOX *) override { return E_NOTIMPL; }
  HRESULT STDMETHODCALLTYPE GetHeapProperties(D3D12_HEAP_PROPERTIES *, D3D12_HEAP_FLAGS *) override { return E_NOTIMPL; }
};
b::EvaluateContractSnapshot contract(const std::array<ID3D12Resource *, 5> &resources) {
  b::EvaluateContractSnapshot result{};
  result.summary.complete = true;
  result.summary.queries_attempted = b::kEvaluateQueryCount;
  for (std::size_t i = 0; i < resources.size(); ++i) {
    auto &v = result.resources[i];
    v.key = static_cast<b::EvaluateResourceKey>(i);
    v.query_attempted = v.callback_completed = v.present = true;
    v.getter_result = 1;
    v.value = reinterpret_cast<std::uintptr_t>(resources[i]);
    v.value_is_null = resources[i] == nullptr;
  }
  return result;
}
void balanced(const b::ResourceInspection &s) {
  check(s.references_acquired == s.releases_attempted && s.releases_attempted == s.releases_completed,
        "every acquired reference released");
  check(s.calls <= 31, "fixed maximum 31 COM calls");
}
} // namespace

int main() {
  try {
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter> warp;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "DXGI factory");
    hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)), "WARP adapter");
    hr(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "WARP D3D12 device");
    check(MH_Initialize() == MH_OK && b::install_presentation_observer(), "system DXGI observer installation");
    check(!b::presentation_creation(device->GetAdapterLuid()).available, "no chain is distinct from unknown format");
    ComPtr<ID3D12CommandQueue> queue;
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&queue)), "presentation queue");
    struct Window {
      HWND value = CreateWindowExW(0, L"STATIC", L"Resource inspection test", WS_OVERLAPPEDWINDOW,
                                    0, 0, 320, 240, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
      ~Window() { if (value) DestroyWindow(value); }
    } window;
    check(window.value != nullptr, "hidden test window");
    DXGI_SWAP_CHAIN_DESC1 chain_desc{};
    chain_desc.Width = 320; chain_desc.Height = 240; chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    chain_desc.SampleDesc.Count = 1; chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    chain_desc.BufferCount = 2; chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> chain;
    hr(factory->CreateSwapChainForHwnd(queue.Get(), window.value, &chain_desc, nullptr, nullptr, &chain), "hidden swap chain");
    const auto presentation = b::presentation_creation(device->GetAdapterLuid());
    check(presentation.available && presentation.observed == 1 && presentation.matching == 1 &&
          presentation.format == DXGI_FORMAT_R8G8B8A8_UNORM && presentation.width == 320 && presentation.height == 240,
          "one matching real swap chain creation format");
    LUID wrong_adapter = device->GetAdapterLuid();
    ++wrong_adapter.LowPart;
    check(!b::presentation_creation(wrong_adapter).available, "other adapter not selected");
    Window second_window;
    ComPtr<IDXGISwapChain1> second_chain;
    hr(factory->CreateSwapChainForHwnd(queue.Get(), second_window.value, &chain_desc, nullptr, nullptr, &second_chain), "second hidden swap chain");
    const auto ambiguous = b::presentation_creation(device->GetAdapterLuid());
    check(!ambiguous.available && ambiguous.matching == 2 && ambiguous.format == DXGI_FORMAT_UNKNOWN,
          "multiple possible presentation targets rejected");
    hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)), "allocator");
    hr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)), "command list");
    std::array<ComPtr<ID3D12Resource>, 4> textures;
    const std::array<DXGI_FORMAT, 4> formats{DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R16G16_FLOAT,
                                          DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT};
    for (std::size_t i = 0; i < textures.size(); ++i) {
      D3D12_RESOURCE_DESC d{};
      d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      d.Width = 32; d.Height = 16; d.DepthOrArraySize = 1; d.MipLevels = 1;
      d.Format = formats[i]; d.SampleDesc.Count = 1;
      d.Flags = i == 3 ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
      D3D12_HEAP_PROPERTIES heap{};
      heap.Type = D3D12_HEAP_TYPE_DEFAULT; heap.CreationNodeMask = heap.VisibleNodeMask = 1;
      hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COMMON,
                                         nullptr, IID_PPV_ARGS(&textures[i])), "texture");
    }
    auto input = contract({textures[0].Get(), textures[1].Get(), nullptr, textures[2].Get(), textures[3].Get()});
    const auto observed = b::inspect_evaluate_resources(list.Get(), input);
    check(observed.complete && !observed.fault_latched && observed.calls == 26 &&
          observed.command_identity_verified && observed.adapter_available, "complete real WARP observation");
    balanced(observed);
    const std::array<std::size_t, 4> indices{0, 1, 3, 4};
    for (std::size_t i = 0; i < indices.size(); ++i) {
      const auto &r = observed.resources[indices[i]];
      check(r.interface_verified && r.description_available && r.same_device && r.description.Format == formats[i] &&
            r.description.Width == 32 && r.description.Height == 16 && r.description.SampleDesc.Count == 1,
            "real texture descriptor and canonical device identity");
    }
    check(!observed.resources[2].attempted, "null PreviousOutput performs no resource query");
    auto with_previous = input;
    with_previous.resources[2].value = input.resources[4].value;
    with_previous.resources[2].value_is_null = false;
    const auto five = b::inspect_evaluate_resources(list.Get(), with_previous);
    check(five.complete && five.calls == 31, "five non-null resources remain bounded");
    balanced(five);
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
      auto invalid = input;
      if (scenario == 0) invalid.summary.complete = false;
      if (scenario == 1) invalid.resources[0].present = false;
      if (scenario == 2) invalid.resources[0].key = b::EvaluateResourceKey::depth;
      if (scenario == 3) invalid.resources[3].value = 0;
      if (scenario == 4) invalid.resources[4].getter_result = 0xBAD00010;
      const auto rejected = b::inspect_evaluate_resources(list.Get(), invalid);
      check(!rejected.complete && rejected.calls == 0 && rejected.stop == b::ResourceInspectionStop::invalid_contract,
            "incomplete or contradictory input rejected before COM calls");
    }
    FakeResource fake;
    fake.device = device.Get();
    OtherIdentity other;
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
      fake.reject = scenario == 1; fake.desc_fault = scenario == 2; fake.release_fault = scenario == 4;
      fake.different_device = scenario == 3 ? &other : nullptr;
      fake.desc_calls = fake.device_calls = 0;
      auto fabricated = input;
      fabricated.resources[0].value = reinterpret_cast<std::uintptr_t>(&fake);
      const auto result = b::inspect_evaluate_resources(list.Get(), fabricated);
      check(fake.refs == 1 && other.refs == 1, "fabricated object references restored");
      check(result.complete == (scenario == 0), "fabricated success/failure result");
      if (scenario != 4) balanced(result);
      if (scenario > 0) check(!result.resources[1].attempted, "stop before the next resource after failure");
      if (scenario == 1) check(fake.desc_calls == 0 && result.stop == b::ResourceInspectionStop::resource_interface, "QI rejection");
      if (scenario == 2) check(fake.device_calls == 0 && result.fault_latched, "GetDesc fault stops queries but cleans ownership");
      if (scenario == 3) check(result.stop == b::ResourceInspectionStop::device_mismatch, "different canonical device rejected");
      if (scenario == 4) check(result.fault_latched && result.releases_attempted == result.references_acquired &&
                               result.releases_completed + 1 == result.releases_attempted, "release fault recorded without a double release");
    }
    check(MH_Uninitialize() == MH_OK, "quiescent fixture hook cleanup");
    std::cout << "PASS: real WARP descriptions/device identity, bounded calls, null/rejected/fault inputs, ownership, system DXGI creation format and ambiguity rejection.\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
