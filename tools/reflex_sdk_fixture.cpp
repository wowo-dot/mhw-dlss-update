// Owned DX12/Streamline test. Never opens or changes the game process.
#include "../src/reflex/streamline.hpp"
#include "../src/reflex/frame_session.hpp"
#include "../src/reflex/owned_interface.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using Microsoft::WRL::ComPtr;
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void hr(HRESULT value, const char* why) { require(SUCCEEDED(value), why); }
void api(sl::Result value, const char* why) {
    if (value != sl::Result::eOk)
        throw std::runtime_error(std::string(why) + " result " + std::to_string(static_cast<unsigned>(value)));
}
struct Window {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND handle{};
    Window() {
        WNDCLASSW cls{}; cls.lpfnWndProc = DefWindowProcW; cls.hInstance = instance;
        cls.lpszClassName = L"MhwOwnedReflexFixture";
        require(RegisterClassW(&cls) != 0, "window class");
        handle = CreateWindowExW(0, cls.lpszClassName, L"Owned Reflex fixture", WS_OVERLAPPEDWINDOW,
                                 0, 0, 320, 240, nullptr, nullptr, instance, nullptr);
        require(handle != nullptr, "hidden window");
    }
    ~Window() { if (handle) DestroyWindow(handle); UnregisterClassW(L"MhwOwnedReflexFixture", instance); }
};
struct Event {
    HANDLE handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ~Event() { if (handle) CloseHandle(handle); }
};
template<class T> void upgrade(mhw::reflex::Streamline& sdk, ComPtr<T>& owned) {
    api(mhw::reflex::upgrade_owned(owned, [&](void** value) { return sdk.upgrade(value); }), "upgrade owned interface");
}
template<class T> ComPtr<T> native(mhw::reflex::Streamline& sdk, IUnknown* proxy) {
    void* value{}; api(sdk.native(proxy, &value), "owned native interface");
    require(value != nullptr, "native interface result");
    ComPtr<IUnknown> base; base.Attach(static_cast<IUnknown*>(value));
    ComPtr<T> result; hr(base.As(&result), "native interface type"); return result;
}
void report(std::ostream& log, mhw::reflex::Streamline& sdk, unsigned mode) {
    sl::ReflexState state{}; api(sdk.state(state), "ReflexGetState");
    unsigned complete{};
    for (const auto& f : state.frameReport) {
        if (!f.simStartTime || !f.simEndTime || !f.renderSubmitStartTime || !f.renderSubmitEndTime ||
            !f.presentStartTime || !f.presentEndTime) continue;
        require(f.simStartTime <= f.simEndTime && f.simEndTime <= f.renderSubmitStartTime &&
                f.renderSubmitStartTime <= f.renderSubmitEndTime && f.renderSubmitEndTime <= f.presentStartTime &&
                f.presentStartTime <= f.presentEndTime, "driver report marker ordering");
        ++complete;
        log << "{\"event\":\"driver-frame\",\"mode\":" << mode << ",\"frame\":" << f.frameID
            << ",\"simulation_start_us\":" << f.simStartTime << ",\"simulation_end_us\":" << f.simEndTime
            << ",\"submit_start_us\":" << f.renderSubmitStartTime << ",\"submit_end_us\":" << f.renderSubmitEndTime
            << ",\"present_start_us\":" << f.presentStartTime << ",\"present_end_us\":" << f.presentEndTime << "}\n";
    }
    log << "{\"event\":\"mode-state\",\"mode\":" << mode << ",\"low_latency_available\":" << state.lowLatencyAvailable
        << ",\"reports_available\":" << state.latencyReportAvailable << ",\"complete_reports\":" << complete << "}\n" << std::flush;
    require(state.lowLatencyAvailable, "Reflex low latency unavailable on fixture adapter");
}
void run(int argc, wchar_t** argv) {
    require(argc == 3, "usage: reflex_sdk_fixture runtime_directory new_output_directory");
    const auto output = std::filesystem::absolute(argv[2]);
    require(!std::filesystem::exists(output), "preserve existing fixture evidence");
    std::filesystem::create_directories(output);
    std::ofstream log(output/L"events.jsonl", std::ios::binary);
    require(bool(log), "fixture log"); log << std::boolalpha;
    log << "{\"event\":\"start\",\"schema\":1,\"sdk\":\"2.12.0\",\"graphics_api\":\"D3D12\","
           "\"game_process_access\":false,\"manual_hooking\":true,\"ota\":false,\"frame_generation\":false,"
           "\"frame_policy\":true,\"synthetic_fixture_stamps\":true,\"native_rendering_interfaces\":true,"
           "\"upgrade_input_reference_released\":true}\n" << std::flush;
    Window window; Event event; require(event.handle != nullptr, "fence event");
    ComPtr<IDXGIFactory4> factory; ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device; ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12Device> proxy_device; ComPtr<IDXGIFactory4> proxy_factory;
    ComPtr<ID3D12CommandQueue> proxy_queue;
    ComPtr<IDXGISwapChain1> swap; ComPtr<IDXGISwapChain3> chain;
    ComPtr<IDXGISwapChain3> native_chain;
    ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12DescriptorHeap> heap; ComPtr<ID3D12Fence> fence;
    std::array<ComPtr<ID3D12Resource>, 2> buffers;
    // Declared after borrowed COM objects so SDK shutdown precedes their
    // destruction on exception as well as success.
    mhw::reflex::Streamline sdk;
    sdk.initialize(std::filesystem::canonical(argv[1]), output/L"sdk-logs");
    hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "factory");
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i) {
        DXGI_ADAPTER_DESC1 desc{}; hr(adapter->GetDesc1(&desc), "adapter description");
        if (desc.VendorId == 0x10de && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) break;
        adapter.Reset();
    }
    require(adapter != nullptr, "NVIDIA fixture adapter");
    DXGI_ADAPTER_DESC1 desc{}; hr(adapter->GetDesc1(&desc), "adapter description");
    log << "{\"event\":\"adapter\",\"vendor\":" << desc.VendorId << ",\"device\":" << desc.DeviceId
        << ",\"luid_low\":" << desc.AdapterLuid.LowPart << ",\"luid_high\":" << desc.AdapterLuid.HighPart
        << ",\"swapchain_format\":" << static_cast<unsigned>(DXGI_FORMAT_B8G8R8A8_UNORM) << "}\n" << std::flush;
    sl::AdapterInfo info{}; info.deviceLUID = reinterpret_cast<std::uint8_t*>(&desc.AdapterLuid);
    info.deviceLUIDSizeInBytes = sizeof(LUID);
    api(sdk.supported(sl::kFeatureReflex, info), "Reflex support");
    api(sdk.supported(sl::kFeaturePCL, info), "PCL support");
    hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "device");
    api(sdk.attach(device.Get()), "attach native device");
    proxy_device = device; proxy_factory = factory;
    upgrade(sdk, proxy_device); upgrade(sdk, proxy_factory);
    require(native<ID3D12Device>(sdk, proxy_device.Get()).Get() == device.Get(), "native device identity");
    require(native<IDXGIFactory4>(sdk, proxy_factory.Get()).Get() == factory.Get(), "native factory identity");
    D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr(proxy_device->CreateCommandQueue(&q, IID_PPV_ARGS(&proxy_queue)), "queue");
    queue = native<ID3D12CommandQueue>(sdk, proxy_queue.Get());
    DXGI_SWAP_CHAIN_DESC1 sc{}; sc.Width = 320; sc.Height = 240; sc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sc.SampleDesc.Count = 1; sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.BufferCount = 2; sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    hr(proxy_factory->CreateSwapChainForHwnd(proxy_queue.Get(), window.handle, &sc, nullptr, nullptr, &swap), "swapchain");
    hr(swap.As(&chain), "swapchain3");
    native_chain = native<IDXGISwapChain3>(sdk, chain.Get());
    require(native_chain.Get() != chain.Get(), "presentation proxy present");
    D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 2;
    hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)), "RTV heap");
    const auto stride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const auto start = heap->GetCPUDescriptorHandleForHeapStart();
    const auto acquire_buffers = [&] {
        for (UINT i = 0; i < 2; ++i) {
            hr(native_chain->GetBuffer(i, IID_PPV_ARGS(&buffers[i])), "native backbuffer");
            device->CreateRenderTargetView(buffers[i].Get(), nullptr, {start.ptr+static_cast<SIZE_T>(i)*stride});
        }
    };
    acquire_buffers();
    hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)), "allocator");
    hr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)), "list");
    hr(list->Close(), "initial close"); hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence");
    mhw::reflex::FrameSession frames(sdk);
    std::uint32_t index{};
    const auto owner = GetCurrentThreadId();
    for (unsigned mode = 0; mode < 3; ++mode) {
        api(sdk.options(static_cast<sl::ReflexMode>(mode)), "Reflex mode");
        for (unsigned n = 0; n < 96; ++n) {
            // Only the owned fixture synthesizes renderer stamps. A game
            // integration must supply actual qualified native observations.
            const mhw::reflex::FrameStamp before{index,index,index & 1U,true};
            ++index;
            const mhw::reflex::FrameStamp after{index,index,index & 1U,true};
            api(frames.begin(owner,before), "begin Reflex frame");
            require(frames.id() == index, "owned policy frame identity");
            MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            api(frames.render_begin(owner,after), "simulation end/render start");
            const auto b = chain->GetCurrentBackBufferIndex(); require(b < 2, "buffer index");
            hr(allocator->Reset(), "allocator reset"); hr(list->Reset(allocator.Get(), nullptr), "list reset");
            D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = buffers[b].Get(); barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            list->ResourceBarrier(1, &barrier);
            const float color[]{0.05f, 0.1f, static_cast<float>(mode)*0.2f, 1.f};
            list->ClearRenderTargetView({start.ptr+static_cast<SIZE_T>(b)*stride}, color, 0, nullptr);
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter); list->ResourceBarrier(1, &barrier);
            hr(list->Close(), "list close");
            ID3D12CommandList* commands[]{list.Get()}; queue->ExecuteCommandLists(1, commands);
            api(frames.render_end(owner,after), "submit end");
            api(frames.present_begin(owner,after), "present start");
            hr(chain->Present(0, 0), "present"); // proxy runs common presentation maintenance
            api(frames.present_end(owner,after), "present end");
            api(frames.end(owner), "complete Reflex frame");
            hr(queue->Signal(fence.Get(), index), "signal");
            hr(fence->SetEventOnCompletion(index, event.handle), "completion event");
            require(WaitForSingleObject(event.handle, 10000) == WAIT_OBJECT_0, "GPU completion");
        }
        report(log, sdk, mode);
        if (mode < 2) {
            for (auto& buffer : buffers) buffer.Reset();
            const UINT width = mode == 0 ? 640 : 320, height = mode == 0 ? 360 : 240;
            hr(chain->ResizeBuffers(2, width, height, sc.Format, 0), "proxy resize");
            DXGI_SWAP_CHAIN_DESC1 resized{}; hr(native_chain->GetDesc1(&resized), "native resize description");
            require(resized.Width == width && resized.Height == height && resized.Format == sc.Format, "resize identity");
            acquire_buffers();
            log << "{\"event\":\"resize\",\"after_frame\":" << index << ",\"width\":" << width
                << ",\"height\":" << height << ",\"verified\":true}\n" << std::flush;
        }
    }
    api(sdk.options(sl::ReflexMode::eOff), "disable Reflex");
    require(frames.completed() == index && frames.tokens() == index && frames.sleeps() == index &&
            frames.markers() == index*6, "policy/SDK completion counts");
    frames.stop();
    hr(device->GetDeviceRemovedReason(), "device health");
    api(sdk.shutdown(), "Streamline shutdown");
    const auto gpu_completed = fence->GetCompletedValue();
    for (auto& buffer : buffers) buffer.Reset();
    list.Reset(); allocator.Reset(); heap.Reset(); fence.Reset();
    native_chain.Reset(); chain.Reset(); swap.Reset(); queue.Reset(); proxy_queue.Reset();
    proxy_device.Reset(); proxy_factory.Reset(); adapter.Reset(); factory.Reset();
    auto* final_device = device.Detach(); const auto remaining_device_refs = final_device->Release();
    log << "{\"event\":\"device-release\",\"remaining_refs\":" << remaining_device_refs << "}\n" << std::flush;
    require(remaining_device_refs == 0, "owned device teardown references");
    log << "{\"event\":\"finished\",\"frames\":" << index << ",\"gpu_completed\":" << gpu_completed
        << ",\"modes\":3,\"sleep_calls\":" << frames.sleeps() << ",\"markers\":" << frames.markers()
        << ",\"policy_completed\":" << frames.completed() << ",\"frame_tokens\":" << frames.tokens()
        << ",\"resizes\":2,\"remaining_device_refs\":0,\"game_reflex_implemented\":false}\n" << std::flush;
}
}
int wmain(int argc, wchar_t** argv) {
    try { run(argc, argv); return 0; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
