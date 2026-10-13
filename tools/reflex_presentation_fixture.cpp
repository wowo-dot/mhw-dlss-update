// Exercise the exact native-interface detours outside the game process.
#include "../src/reflex/presentation.hpp"
#include "../src/reflex/native_factory.hpp"
#ifdef MHW_REFLEX_FRAME_FIXTURE
#include "../src/reflex/driver_status.hpp"
#include "../src/reflex/frame_session.hpp"
#endif
#include <wrl/client.h>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
using Microsoft::WRL::ComPtr;
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void hr(HRESULT value, const char* why) { require(SUCCEEDED(value), why); }
void api(sl::Result value, const char* why) { require(value == sl::Result::eOk, why); }
struct Window {
    HINSTANCE instance = GetModuleHandleW(nullptr); HWND hwnd{};
    Window() {
        WNDCLASSW cls{}; cls.lpfnWndProc = DefWindowProcW; cls.hInstance = instance; cls.lpszClassName = L"MhwNativeReflexPresentation";
        require(RegisterClassW(&cls) != 0, "class");
        hwnd = CreateWindowExW(0, cls.lpszClassName, L"Owned presentation fixture", WS_OVERLAPPEDWINDOW,
            0, 0, 320, 240, nullptr, nullptr, instance, nullptr);
        require(hwnd != nullptr, "hidden window");
    }
    ~Window() { if (hwnd) DestroyWindow(hwnd); UnregisterClassW(L"MhwNativeReflexPresentation", instance); }
};
struct Event { HANDLE value = CreateEventW(nullptr, FALSE, FALSE, nullptr); ~Event() { if (value) CloseHandle(value); } };
#ifdef MHW_REFLEX_FRAME_FIXTURE
struct Frames {
    mhw::reflex::FrameSession session;
    mhw::reflex::FrameStamp stamp{};
    unsigned begins{}, ends{}, tests{}, faults{}; DWORD native_error{};
    explicit Frames(mhw::reflex::Streamline& sdk):session(sdk) {}
    static bool begin(void* p,std::uintptr_t,unsigned flags) noexcept {
        auto& f=*static_cast<Frames*>(p);
        // Deliberately alter LastError to test observer isolation.
        SetLastError(972);
        if(flags&DXGI_PRESENT_TEST) { ++f.tests;return false; }
        ++f.begins;
        try { if(f.session.present_begin(GetCurrentThreadId(),f.stamp)==sl::Result::eOk)return true; }
        catch(...) {}
        ++f.faults;return false;
    }
    static void end(void* p,std::uintptr_t,unsigned flags,HRESULT result) noexcept {
        auto& f=*static_cast<Frames*>(p);++f.ends;f.native_error=GetLastError();SetLastError(973);
        try { if(!(flags&DXGI_PRESENT_TEST) && SUCCEEDED(result) &&
            f.session.present_end(GetCurrentThreadId(),f.stamp)==sl::Result::eOk)return; }
        catch(...) {}
        ++f.faults;
    }
};
#endif
void run(int argc, wchar_t** argv) {
#ifdef MHW_REFLEX_FRAME_FIXTURE
    require(argc==3 || (argc==4 && std::wstring_view(argv[3])==L"on"),"usage: frame_fixture sdk_runtime new_output_directory [on]");
    const auto mode=argc==4?sl::ReflexMode::eLowLatency:sl::ReflexMode::eOff;
#else
    require(argc == 3, "usage: presentation_fixture sdk_runtime new_output_directory");
#endif
    const auto output = std::filesystem::absolute(argv[2]); require(!std::filesystem::exists(output), "preserve evidence");
    std::filesystem::create_directories(output);
    std::ofstream log(output / L"events.jsonl", std::ios::binary); require(bool(log), "log"); log << std::boolalpha;
    log << "{\"event\":\"start\",\"schema\":1,\"sdk\":\"2.12.0\",\"graphics_api\":\"D3D12\",\"game_access\":false,"
        "\"sleep\":false,\"markers\":false,\"host_interfaces_native\":true}\n" << std::flush;
    Window window; Event event; require(event.value != nullptr, "event");
    ComPtr<IDXGIFactory2> factory; ComPtr<IDXGIAdapter1> adapter;
    hr(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "native factory");
    for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i) {
        DXGI_ADAPTER_DESC1 desc{}; hr(adapter->GetDesc1(&desc), "adapter desc");
        if (desc.VendorId == 0x10de && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) break;
        adapter.Reset();
    }
    require(adapter != nullptr, "adapter");
    ComPtr<ID3D12Device> device; hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)), "native device");
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<IDXGISwapChain1> swap; ComPtr<IDXGISwapChain3> chain;
    ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12DescriptorHeap> heap; ComPtr<ID3D12Fence> fence;
    std::array<ComPtr<ID3D12Resource>, 2> buffers;
    // Never destroy SDK/proxies under fixture exception unwinding while routes
    // remain installed: catch below stops first, then shuts down and releases.
    mhw::reflex::Streamline sdk; mhw::reflex::Presentation bridge(sdk);
#ifdef MHW_REFLEX_FRAME_FIXTURE
    Frames frames(sdk);
    const auto check_mode=[&](const char* phase,bool expected,bool enforce=true) {
        const auto status=mhw::reflex::driver_sleep_status(device.Get());
        log << "{\"event\":\"driver_sleep_status\",\"phase\":\"" << phase << "\",\"result\":" << status.result
            << ",\"low_latency\":" << status.low_latency << "}\n" << std::flush;
        require(status.result==0 && (!enforce || status.low_latency==expected),"driver mode readback");
    };
#endif
    try {
        auto parent = mhw::reflex::adapter_factory(adapter.Get());
        require(parent.Get() == factory.Get(), "adapter parent is original native factory");
        log << "{\"event\":\"factory_context\",\"source\":\"native-adapter-parent\",\"native_factory_matches\":true,"
               "\"queues_created\":0,\"swapchains_created\":0,\"renderer_global_required\":false}\n" << std::flush;
        sdk.initialize(std::filesystem::canonical(argv[1]), output / L"sdk-logs");
        api(sdk.attach(device.Get()), "attach"); api(sdk.options(sl::ReflexMode::eOff), "Off");
#ifdef MHW_REFLEX_OWNERSHIP_FIXTURE
        const auto diagnostic_image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(diagnostic_image);
        const auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(diagnostic_image+dos->e_lfanew);
        bridge.ownership(diagnostic_image,pe->OptionalHeader.SizeOfImage);
#endif
        bridge.install(device.Get(), parent.Get()); parent.Reset();
#ifdef MHW_REFLEX_FRAME_FIXTURE
        bridge.observe({&frames,Frames::begin,Frames::end});
#endif
        D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "native returned queue");
        ComPtr<ID3D12Device> returned_device; hr(queue->GetDevice(IID_PPV_ARGS(&returned_device)), "queue device");
        require(returned_device.Get() == device.Get(), "native device preserved"); returned_device.Reset();
        DXGI_SWAP_CHAIN_DESC1 sc{}; sc.Width = 320; sc.Height = 240; sc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sc.SampleDesc.Count = 1; sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sc.BufferCount = 2; sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        const auto create_chain = [&] {
            hr(factory->CreateSwapChainForHwnd(queue.Get(), window.hwnd, &sc, nullptr, nullptr, &swap), "native returned swap");
            hr(swap.As(&chain), "swap3");
            const auto stats = bridge.snapshot();
            // v0.27.4: the application holds the identity chain (identity_chain.hpp).
            require(stats.identity && stats.native_swapchain != reinterpret_cast<std::uintptr_t>(chain.Get()) &&
                stats.proxy_swapchain != stats.native_swapchain && stats.installed_hooks == mhw::reflex::Presentation::hook_capacity,
                "identity/native/proxy separation");
        };
        create_chain();
#ifdef MHW_REFLEX_FRAME_FIXTURE
        sl::ReflexState baseline{};api(sdk.state(baseline),"baseline Reflex state");
        require(baseline.lowLatencyAvailable,"baseline Reflex availability");
        check_mode("startup_off",false);
        api(sdk.options(mode),"capture mode");
        check_mode("requested",mode!=sl::ReflexMode::eOff,false);
#endif
        D3D12_DESCRIPTOR_HEAP_DESC hd{}; hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = 2;
        hr(device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap)), "heap");
        const auto stride = device->GetDescriptorHandleIncrementSize(hd.Type);
        const auto start = heap->GetCPUDescriptorHandleForHeapStart();
        const auto acquire = [&] {
            for (UINT i = 0; i < 2; ++i) {
                hr(chain->GetBuffer(i, IID_PPV_ARGS(&buffers[i])), "native buffer");
                device->CreateRenderTargetView(buffers[i].Get(), nullptr, {start.ptr + static_cast<SIZE_T>(i)*stride});
            }
        };
        acquire(); hr(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&allocator)), "allocator");
        hr(device->CreateCommandList(0, q.Type, allocator.Get(), nullptr, IID_PPV_ARGS(&list)), "list");
        hr(list->Close(), "close"); hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence");
        hr(chain->SetFullscreenState(FALSE, nullptr), "windowed");
        hr(chain->Present(0, DXGI_PRESENT_TEST), "test present");
        for (UINT frame = 1; frame <= 144; ++frame) {
#ifdef MHW_REFLEX_OWNERSHIP_FIXTURE
            if(frame==9)bridge.ownership_running();
#endif
            MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
#ifdef MHW_REFLEX_FRAME_FIXTURE
            const auto thread=GetCurrentThreadId();
            api(frames.session.begin(thread,{frame-1,frame-1,(frame-1)&1U,true}),"live token/sleep/SimulationStart");
            if(frame==1)check_mode("first_sleep",mode!=sl::ReflexMode::eOff,false);
            frames.stamp={frame,frame,frame&1U,true};
            api(frames.session.render_begin(thread,frames.stamp),"SimulationEnd/RenderSubmitStart");
#endif
            const auto b = chain->GetCurrentBackBufferIndex(); require(b < 2, "buffer index");
            hr(allocator->Reset(), "reset allocator"); hr(list->Reset(allocator.Get(), nullptr), "reset list");
            D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = buffers[b].Get(); barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            list->ResourceBarrier(1, &barrier);
            const float color[]{0.1f, 0.2f, 0.3f, 1.0f};
            list->ClearRenderTargetView({start.ptr + static_cast<SIZE_T>(b)*stride}, color, 0, nullptr);
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter); list->ResourceBarrier(1, &barrier);
            hr(list->Close(), "close list"); ID3D12CommandList* commands[]{list.Get()}; queue->ExecuteCommandLists(1, commands);
#ifdef MHW_REFLEX_FRAME_FIXTURE
            api(frames.session.render_end(thread,frames.stamp),"RenderSubmitEnd");
#endif
            DXGI_PRESENT_PARAMETERS params{};
            hr((frame & 1U) ? chain->Present(0, 0) : chain->Present1(0, 0, &params), "native presentation");
#ifdef MHW_REFLEX_FRAME_FIXTURE
            require(GetLastError()==frames.native_error,"observer preserves native presentation LastError");
            api(frames.session.end(thread),"complete frame after routed Present");
            if(frame==1)check_mode("first_present",mode!=sl::ReflexMode::eOff,false);
            if(frame==8 || frame==32)check_mode(frame==8?"frame_8":"frame_32",mode!=sl::ReflexMode::eOff,false);
#endif
            hr(queue->Signal(fence.Get(), frame), "signal"); hr(fence->SetEventOnCompletion(frame, event.value), "completion");
            require(WaitForSingleObject(event.value, 10000) == WAIT_OBJECT_0, "GPU completion");
            if (frame == 48 || frame == 96) {
                for (auto& buffer : buffers) buffer.Reset();
                const UINT width = frame == 48 ? 640U : 320U, height = frame == 48 ? 360U : 240U;
                if (frame == 48) hr(chain->ResizeBuffers(2, width, height, sc.Format, 0), "resize");
                else { const UINT masks[]{1,1}; IUnknown* queues[]{queue.Get(), queue.Get()};
                    hr(chain->ResizeBuffers1(2, width, height, sc.Format, 0, masks, queues), "resize1"); }
                DXGI_SWAP_CHAIN_DESC desc{}; hr(chain->GetDesc(&desc), "desc");
                require(desc.BufferDesc.Width == width && desc.BufferDesc.Height == height, "resize desc"); acquire();
                log << "{\"event\":\"resize\",\"frame\":" << frame << ",\"verified\":true}\n" << std::flush;
            }
            if (frame == 120) {
                for (auto& buffer : buffers) buffer.Reset(); chain.Reset(); swap.Reset();
                create_chain(); acquire();
                log << "{\"event\":\"recreation\",\"frame\":" << frame << ",\"verified\":true}\n" << std::flush;
            }
        }
        sl::ReflexState state{}; api(sdk.state(state), "state"); require(state.lowLatencyAvailable, "Reflex available");
        const auto stats = bridge.snapshot();
        require(stats.queues == 1 && stats.swapchains == 2 && stats.presents == 144 && stats.native_presents == 144 && stats.test_presents == 1 &&
            stats.resizes == 2 && stats.faults == 0, "route counters");
#ifdef MHW_REFLEX_FRAME_FIXTURE
        require(frames.begins==144 && frames.ends==144 && frames.tests==1 && frames.faults==0 &&
            frames.session.completed()==144 && frames.session.tokens()==144 && frames.session.sleeps()==144 &&
            frames.session.markers()==864,"one marker pair around each Present/Present1 including resize/recreation");
        frames.session.stop();
        log << "{\"event\":\"frame_session\",\"mode\":" << static_cast<unsigned>(mode) << ",\"completed\":144,\"tokens\":144,\"sleeps\":144,\"markers\":864,"
            "\"present_begin_callbacks\":144,\"present_end_callbacks\":144,\"test_callbacks\":1,\"faults\":0}\n"<<std::flush;
#endif
        require(bridge.stop(), "stop and restore all hooks");
#ifdef MHW_REFLEX_OWNERSHIP_FIXTURE
        bridge.write_ownership(log);
#endif
        require(!bridge.release_after_shutdown(), "reject early pin release");
#ifdef MHW_REFLEX_FRAME_FIXTURE
        check_mode("stopped",mode!=sl::ReflexMode::eOff);
        api(sdk.options(sl::ReflexMode::eOff),"restore Off");
        const auto wait=mhw::reflex::wait_driver_off(device.Get());
        log << "{\"event\":\"driver_sleep_status\",\"phase\":\"restored_off\",\"result\":" << wait.status.result
            << ",\"low_latency\":" << wait.status.low_latency << ",\"queries\":" << wait.queries
            << ",\"elapsed_ms\":" << wait.elapsed_ms << ",\"timeout_ms\":" << mhw::reflex::driver_off_timeout_ms
            << ",\"initial_low_latency\":" << wait.initial_low_latency << "}\n" << std::flush;
        require(wait.status.result==0 && !wait.status.low_latency,"confirmed Off before SDK shutdown");
#endif
        api(sdk.shutdown(), "shutdown"); require(bridge.release_after_shutdown(), "release pins");
        // The native host can keep rendering after SDK shutdown, with no proxy
        // pointer substitution. Test one more Present and descriptor query.
        hr(chain->Present(0, 0), "native present after stop");
#ifdef MHW_REFLEX_FRAME_FIXTURE
        require(frames.begins==144 && frames.ends==144,"no observer calls after hook restoration/SDK shutdown");
#endif
        DXGI_SWAP_CHAIN_DESC desc{}; hr(chain->GetDesc(&desc), "native desc after stop");
        hr(device->GetDeviceRemovedReason(), "device health");
        for (auto& buffer : buffers) buffer.Reset(); list.Reset(); allocator.Reset(); heap.Reset(); fence.Reset();
        chain.Reset(); swap.Reset(); queue.Reset(); adapter.Reset(); factory.Reset();
        const auto refs = device.Detach()->Release(); require(refs == 0, "zero final device refs");
        log << "{\"event\":\"finished\",\"presents\":" << stats.presents << ",\"native_presents\":" << stats.native_presents << ",\"test_presents\":" << stats.test_presents
            << ",\"queues\":" << stats.queues << ",\"swapchains\":" << stats.swapchains << ",\"resizes\":" << stats.resizes
            << ",\"faults\":0,\"hooks_restored\":12,\"sdk_shutdown\":true,\"pins_released\":true,\"native_after_stop\":true,\"device_refs\":0}\n" << std::flush;
    } catch (...) {
        const bool stopped = bridge.stop();
#ifdef MHW_REFLEX_FRAME_FIXTURE
        if(stopped && sdk.initialized())sdk.options(sl::ReflexMode::eOff);
#endif
        const auto shutdown = stopped ? sdk.shutdown() : sl::Result::eErrorInvalidState;
        if (stopped && shutdown == sl::Result::eOk) bridge.release_after_shutdown();
        throw;
    }
}
}
int wmain(int argc, wchar_t** argv) {
    try { run(argc, argv); return 0; } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
