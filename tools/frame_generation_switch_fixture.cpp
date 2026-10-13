// Owned DX12 test of the live frame generation switch (docs/research/
// fg-live-toggle.md, v0.27.4). Never opens the game. Plays the game's part:
// holds the swap chain it is given (the identity chain), sets frame latency 3
// and waits on the chain's waitable at every frame start, renders through
// GetBuffer/GetCurrentBackBufferIndex, and for a switch releases its back
// buffers and calls ResizeBuffers, in which the presentation layer makes the
// real chain again with or without DLSS-G. Phases (fps without an artificial
// interval; synthetic inputs test execution, not image quality):
//   1 DLSS-G loaded, frame generation On (generated frames required);
//   2 loaded, Off (Streamline's Off path);
//   3 switched to unloaded, Off;
//   4 switched to loaded, On (generated frames required again).
#include "../src/reflex/presentation.hpp"
#include "../src/reflex/native_factory.hpp"
#include "../src/reflex/frame_session.hpp"
#include "../src/reflex/driver_status.hpp"
#include <d3d12sdklayers.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void hr(HRESULT value, const char* why) {
    if (FAILED(value)) throw std::runtime_error(std::string(why) + " HRESULT " + std::to_string(static_cast<std::uint32_t>(value)));
}
void api(sl::Result value, const char* why) {
    if (value != sl::Result::eOk) throw std::runtime_error(std::string(why) + " result " + std::to_string(static_cast<unsigned>(value)));
}
struct Window {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND hwnd{};
    Window() {
        WNDCLASSW cls{}; cls.lpfnWndProc = DefWindowProcW; cls.hInstance = instance;
        cls.lpszClassName = L"MhwFrameGenerationSwitch";
        require(RegisterClassW(&cls) != 0, "window class");
        hwnd = CreateWindowExW(0, cls.lpszClassName, L"Frame generation switch fixture", WS_OVERLAPPEDWINDOW,
            0, 0, 1280, 720, nullptr, nullptr, instance, nullptr);
        require(hwnd != nullptr, "window");
        ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); SetFocus(hwnd);
    }
    ~Window() { if (hwnd) DestroyWindow(hwnd); UnregisterClassW(L"MhwFrameGenerationSwitch", instance); }
};
struct Frames {
    mhw::reflex::FrameSession session;
    mhw::reflex::FrameStamp stamp{};
    unsigned begins{}, ends{}, faults{};
    explicit Frames(mhw::reflex::Streamline& sdk) : session(sdk) {}
    static bool begin(void* p, std::uintptr_t, unsigned flags) noexcept {
        auto& f = *static_cast<Frames*>(p);
        if (flags & DXGI_PRESENT_TEST) return false;
        ++f.begins;
        try { if (f.session.present_begin(GetCurrentThreadId(), f.stamp) == sl::Result::eOk) return true; } catch (...) {}
        ++f.faults; return false;
    }
    static void end(void* p, std::uintptr_t, unsigned, HRESULT result) noexcept {
        auto& f = *static_cast<Frames*>(p); ++f.ends;
        try { if (SUCCEEDED(result) && f.session.present_end(GetCurrentThreadId(), f.stamp) == sl::Result::eOk) return; } catch (...) {}
        ++f.faults;
    }
};
#include "frame_generation_fixture_inputs.inl"
bool run(int argc, wchar_t** argv) {
    require(argc == 3, "usage: frame_generation_switch_fixture sdk_runtime new_output_directory");
    const auto output = std::filesystem::absolute(argv[2]);
    require(!std::filesystem::exists(output), "preserve evidence");
    std::filesystem::create_directories(output);
    std::ofstream log(output / L"events.jsonl", std::ios::binary); require(bool(log), "log"); log << std::boolalpha;
    log << "{\"event\":\"start\",\"schema\":1,\"fixture\":\"frame-generation-switch\",\"game_access\":false}\n" << std::flush;
    Window window;
    mhw::reflex::Streamline sdk; mhw::reflex::Presentation bridge(sdk); Frames frames(sdk);
    const sl::ViewportHandle viewport{0};
    ComPtr<IDXGIFactory2> factory; ComPtr<IDXGIAdapter1> adapter; ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> queue; ComPtr<IDXGISwapChain1> swap; ComPtr<IDXGISwapChain3> chain;
    ComPtr<ID3D12GraphicsCommandList> list; ComPtr<ID3D12Fence> fence;
    std::array<ComPtr<ID3D12CommandAllocator>, 3> allocators;
    std::array<UINT64, 3> slot_fence{};
    std::array<ComPtr<ID3D12Resource>, 2> buffers;
    std::unique_ptr<OwnedFgInputs> inputs;
    HANDLE waitable{};  // where the "game" keeps it (the presentation replaces it at a switch)
    const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    require(event != nullptr, "event");
    UINT64 fence_value{};
    try {
        sdk.initialize(std::filesystem::canonical(argv[1]), output / L"sdk-logs",
            mhw::reflex::StreamlineFeatures::reflex_and_frame_generation);
        hr(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)), "factory");
        DXGI_ADAPTER_DESC1 desc{};
        for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i) {
            hr(adapter->GetDesc1(&desc), "adapter desc");
            if (desc.VendorId == 0x10de && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) break;
            adapter.Reset();
        }
        require(adapter != nullptr, "NVIDIA adapter");
        sl::AdapterInfo info{}; info.deviceLUID = reinterpret_cast<std::uint8_t*>(&desc.AdapterLuid);
        info.deviceLUIDSizeInBytes = sizeof(desc.AdapterLuid);
        api(sdk.supported(sl::kFeatureDLSS_G, info), "FG support");
        hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)), "device");
        api(sdk.attach(device.Get()), "attach");
        require(sdk.frame_generation() && sdk.frame_generation_requested(), "DLSS-G loaded at start");
        api(sdk.options(sl::ReflexMode::eLowLatency), "Reflex On");
        api(sdk.frame_generation_off(viewport), "FG Off");
        auto parent = mhw::reflex::adapter_factory(adapter.Get());
        bridge.install(device.Get(), parent.Get(), mhw::reflex::PresentationContract::sdk_paced);
        bridge.observe({&frames, Frames::begin, Frames::end});
        parent.Reset();
        D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "queue");
        DXGI_SWAP_CHAIN_DESC1 sc{}; sc.Width = 1280; sc.Height = 720; sc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sc.SampleDesc.Count = 1; sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sc.BufferCount = 2;
        sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        hr(factory->CreateSwapChainForHwnd(queue.Get(), window.hwnd, &sc, nullptr, nullptr, &swap), "swap chain");
        hr(swap.As(&chain), "swap3");
        auto stats = bridge.snapshot();
        log << "{\"event\":\"created\",\"identity\":" << stats.identity << ",\"identity_refused\":" << stats.identity_refused
            << ",\"native\":" << stats.native_swapchain << ",\"held\":" << reinterpret_cast<std::uintptr_t>(chain.Get()) << "}\n" << std::flush;
        require(stats.identity && stats.native_swapchain != reinterpret_cast<std::uintptr_t>(chain.Get()), "the identity chain");
        // As cD3DRender::initForD3D12: latency 3 and its waitable.
        hr(chain->SetMaximumFrameLatency(3), "frame latency");
        waitable = chain->GetFrameLatencyWaitableObject();
        require(waitable != nullptr, "waitable");
        bridge.waitable_slot(reinterpret_cast<std::uintptr_t>(&waitable));
        HWND routed{}; hr(chain->GetHwnd(&routed), "routed GetHwnd");
        require(routed == window.hwnd, "GetHwnd reaches the real chain");

        hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence");
        for (auto& a : allocators) hr(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&a)), "allocator");
        hr(device->CreateCommandList(0, q.Type, allocators[0].Get(), nullptr, IID_PPV_ARGS(&list)), "list");
        inputs = std::make_unique<OwnedFgInputs>(device.Get(), list.Get());
        hr(list->Close(), "close"); ID3D12CommandList* init[]{list.Get()}; queue->ExecuteCommandLists(1, init);
        const auto idle = [&] {
            hr(queue->Signal(fence.Get(), ++fence_value), "signal");
            hr(fence->SetEventOnCompletion(fence_value, event), "completion");
            require(WaitForSingleObject(event, 10000) == WAIT_OBJECT_0, "GPU idle");
        };
        idle();
        const auto acquire = [&] { for (UINT i = 0; i < 2; ++i) hr(chain->GetBuffer(i, IID_PPV_ARGS(&buffers[i])), "buffer"); };
        acquire();

        unsigned frame = 0;
        struct Phase { const char* name; double fps; UINT64 generated, waits_ms; };
        std::vector<Phase> phases;
        const auto run_phase = [&](const char* name, unsigned count, bool on) {
            if (on) api(sdk.frame_generation_ordinary(viewport), "FG On");
            else if (sdk.frame_generation()) api(sdk.frame_generation_off(viewport, true), "FG Off");
            LARGE_INTEGER f{}, t0{}, t1{}; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
            UINT64 generated{}, wait_ticks{};
            sl::DLSSGState last{};
            for (unsigned n = 0; n < count; ++n) {
                ++frame;
                // Frame generation phases at a game-like pace (as the on
                // fixture); Off phases measure throughput.
                if (on) Sleep(8);
                MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
                // The game's frame start (sub_1423ADE30): its waitable, 1 s at most.
                LARGE_INTEGER w0{}, w1{}; QueryPerformanceCounter(&w0);
                require(WaitForSingleObjectEx(waitable, 1000, TRUE) == WAIT_OBJECT_0, "latency waitable");
                QueryPerformanceCounter(&w1); wait_ticks += static_cast<UINT64>(w1.QuadPart - w0.QuadPart);
                const auto thread = GetCurrentThreadId();
                api(frames.session.begin(thread, {frame - 1, frame - 1, (frame - 1) & 1U, true}), "frame begin");
                frames.stamp = {frame, frame, frame & 1U, true};
                const auto* token = frames.session.input_token(thread);
                require(token != nullptr, "input token");
                // The shared inputs tag frames 2..128 (33 resets the camera,
                // 129 and later are untagged): a cycling number, reset at a
                // phase's first frame.
                const unsigned input_frame = n == 0 ? 33U : 34U + n % 90U;
                if (sdk.frame_generation()) inputs->tag(sdk, *token, viewport, input_frame);
                api(frames.session.render_begin(thread, frames.stamp), "render begin");
                const auto slot = frame % allocators.size();
                if (slot_fence[slot] && fence->GetCompletedValue() < slot_fence[slot]) {
                    hr(fence->SetEventOnCompletion(slot_fence[slot], event), "slot completion");
                    require(WaitForSingleObject(event, 10000) == WAIT_OBJECT_0, "slot GPU");
                }
                const auto index = chain->GetCurrentBackBufferIndex(); require(index < 2, "back buffer index");
                hr(allocators[slot]->Reset(), "allocator reset"); hr(list->Reset(allocators[slot].Get(), nullptr), "list reset");
                inputs->update(list.Get(), input_frame);
                D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                b.Transition.pResource = buffers[index].Get(); b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
                list->ResourceBarrier(1, &b);
                auto s = b; s.Transition.pResource = inputs->textures[2].resource.Get();
                s.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; s.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                list->ResourceBarrier(1, &s); list->CopyResource(buffers[index].Get(), inputs->textures[2].resource.Get());
                std::swap(s.Transition.StateBefore, s.Transition.StateAfter); list->ResourceBarrier(1, &s);
                std::swap(b.Transition.StateBefore, b.Transition.StateAfter); list->ResourceBarrier(1, &b);
                hr(list->Close(), "close"); ID3D12CommandList* lists[]{list.Get()}; queue->ExecuteCommandLists(1, lists);
                hr(queue->Signal(fence.Get(), ++fence_value), "frame signal"); slot_fence[slot] = fence_value;
                api(frames.session.render_end(thread, frames.stamp), "render end");
                hr(chain->Present(0, 0), "present");
                api(frames.session.end(thread), "frame end");
                if (sdk.frame_generation()) {
                    sl::DLSSGState state{}; api(sdk.frame_generation_state(viewport, state), "FG state");
                    if (state.numFramesActuallyPresented > 1) generated += state.numFramesActuallyPresented - 1;
                    last = state;
                    // As the on fixture: the inputs stay until DLSS-G consumed them.
                    if (on && state.inputsProcessingCompletionFence && state.lastPresentInputsProcessingCompletionFenceValue) {
                        auto* consumed = static_cast<ID3D12Fence*>(state.inputsProcessingCompletionFence);
                        hr(consumed->SetEventOnCompletion(state.lastPresentInputsProcessingCompletionFenceValue, event), "FG inputs consumed");
                        require(WaitForSingleObject(event, 10000) == WAIT_OBJECT_0, "inputs consumed");
                    }
                }
            }
            QueryPerformanceCounter(&t1);
            const double seconds = static_cast<double>(t1.QuadPart - t0.QuadPart) / static_cast<double>(f.QuadPart);
            phases.push_back({name, count / seconds, generated, wait_ticks * 1000 / static_cast<UINT64>(f.QuadPart)});
            log << "{\"event\":\"phase\",\"name\":\"" << name << "\",\"frames\":" << count << ",\"fps\":" << count / seconds
                << ",\"generated\":" << generated << ",\"waitable_ms\":" << phases.back().waits_ms
                << ",\"status\":" << static_cast<unsigned>(last.status) << ",\"presented\":" << last.numFramesActuallyPresented
                << ",\"loaded\":" << sdk.frame_generation() << ",\"foreground\":" << (GetForegroundWindow() == window.hwnd) << "}\n" << std::flush;
        };
        const auto switch_to = [&](bool loaded) {
            idle();
            for (auto& b : buffers) b.Reset();
            const auto before = bridge.snapshot();
            const HANDLE old = waitable;
            require(bridge.request_switch(loaded), "switch request");
            LARGE_INTEGER f{}, t0{}, t1{}; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
            hr(chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, sc.Flags), "the game's resize");
            QueryPerformanceCounter(&t1);
            acquire();
            const auto after = bridge.snapshot();
            log << "{\"event\":\"switch\",\"loaded\":" << loaded << ",\"sdk_loaded\":" << sdk.frame_generation()
                << ",\"paced\":" << after.frame_generation_loaded << ",\"reason\":" << after.switch_reason
                << ",\"switches\":" << after.switches << ",\"failures\":" << after.switch_failures
                << ",\"refusals\":" << after.switch_refusals << ",\"waits\":" << after.switch_waits
                << ",\"waitable_replaced\":" << (waitable != old) << ",\"waitable_failures\":" << after.waitable_failures
                << ",\"buffers\":" << after.buffers << ",\"ms\":" << (t1.QuadPart - t0.QuadPart) * 1000 / f.QuadPart << "}\n" << std::flush;
            require(after.switches == before.switches + 1 && after.switch_reason == (loaded ? 1U : 2U), "switch done");
            require(sdk.frame_generation() == loaded && after.frame_generation_loaded == loaded, "DLSS-G state follows");
            require(waitable != old && !after.waitable_failures, "the game's waitable replaced");
            UINT latency{}; hr(chain->GetMaximumFrameLatency(&latency), "latency after switch");
            log << "{\"event\":\"latency_after_switch\",\"value\":" << latency << "}\n" << std::flush;
        };
        // Focus for DLSS-G (it interpolates only for the foreground window).
        const auto deadline = GetTickCount64() + 20000;
        while (GetForegroundWindow() != window.hwnd && GetTickCount64() < deadline) {
            MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            SetForegroundWindow(window.hwnd); Sleep(50);
        }
        run_phase("warmup", 60, false);
        run_phase("loaded_on", 300, true);
        run_phase("loaded_off", 600, false);
        switch_to(false);
        run_phase("unloaded_off", 600, false);
        switch_to(true);
        run_phase("reloaded_on", 300, true);
        run_phase("reloaded_off", 300, false);
        switch_to(false);
        run_phase("unloaded_again", 300, false);
        idle();
        const bool generated = phases[1].generated > 0 && phases[4].generated > 0;
        log << "{\"event\":\"summary\",\"loaded_on_fps\":" << phases[1].fps << ",\"loaded_off_fps\":" << phases[2].fps
            << ",\"unloaded_off_fps\":" << phases[3].fps << ",\"reloaded_on_fps\":" << phases[4].fps
            << ",\"generated_before\":" << phases[1].generated << ",\"generated_after\":" << phases[4].generated
            << ",\"reflex_faults\":" << frames.faults << "}\n" << std::flush;
        frames.session.stop();
        require(bridge.stop(), "hooks restored");
        api(sdk.options(sl::ReflexMode::eOff), "Reflex Off");
        api(sdk.shutdown(), "SDK shutdown");
        require(bridge.release_after_shutdown(), "pins released");
        for (auto& b : buffers) b.Reset();
        inputs->release(); inputs.reset();
        list.Reset(); for (auto& a : allocators) a.Reset(); fence.Reset(); chain.Reset(); swap.Reset(); queue.Reset();
        log << "{\"event\":\"finished\",\"accepted\":" << generated << "}\n" << std::flush;
        CloseHandle(event);
        return generated;
    } catch (...) {
        try { throw; } catch (const std::exception& error) {
            std::cerr << error.what() << '\n' << std::flush;
            log << "{\"event\":\"failure\",\"reason\":" << std::quoted(error.what()) << "}\n" << std::flush;
        } catch (...) { log << "{\"event\":\"failure\",\"reason\":\"non-C++ exception\"}\n" << std::flush; }
        frames.session.stop();
        const bool stopped = bridge.stop();
        if (stopped && sdk.initialized()) { sdk.options(sl::ReflexMode::eOff); sdk.shutdown(); }
        throw;
    }
}
}  // namespace
int wmain(int argc, wchar_t** argv) {
    try { return run(argc, argv) ? 0 : 2; } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
