// Owned DX12 ordinary FG / fixed MFG execution test. Never opens the game.
// The explicit SDK-paced contract never duplicates a host Present when the
// SDK presents on an internal chain. Synthetic inputs test execution, not game quality.
#include "../src/reflex/presentation.hpp"
#include "../src/reflex/native_factory.hpp"
#include "../src/reflex/frame_session.hpp"
#include "../src/reflex/driver_status.hpp"
#include "../src/reflex/owned_interface.hpp"
#include <d3d12sdklayers.h>
#include <dxgidebug.h>
#include <wrl/client.h>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <DirectXMath.h>
#include <memory>

namespace {
using Microsoft::WRL::ComPtr;
#ifdef MHW_FG_DIRECT_FIXTURE
constexpr bool direct_mode = true;
#else
constexpr bool direct_mode = false;
#endif
void CALLBACK gpu_message(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity,
                          D3D12_MESSAGE_ID id, LPCSTR text, void*) {
    static std::atomic<unsigned> count{};
    if (severity <= D3D12_MESSAGE_SEVERITY_WARNING && count.fetch_add(1) < 16)
        std::cerr << "D3D12 callback " << id << ": " << text << '\n' << std::flush;
}
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void hr(HRESULT value, const char* why) {
    if (FAILED(value)) throw std::runtime_error(std::string(why) + " HRESULT " +
        std::to_string(static_cast<std::uint32_t>(value)));
}
void api(sl::Result value, const char* why) {
    if (value != sl::Result::eOk)
        throw std::runtime_error(std::string(why) + " result " + std::to_string(static_cast<unsigned>(value)));
}
struct Window {
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HWND hwnd{};
    Window() {
        WNDCLASSW cls{}; cls.lpfnWndProc = DefWindowProcW; cls.hInstance = instance;
        cls.lpszClassName = L"MhwOwnedFrameGenerationOn";
        require(RegisterClassW(&cls) != 0, "window class");
        hwnd = CreateWindowExW(0, cls.lpszClassName, L"Owned ordinary FG fixture", WS_OVERLAPPEDWINDOW,
            0, 0, 1280, 720, nullptr, nullptr, instance, nullptr);
        require(hwnd != nullptr, "hidden window");
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd); SetFocus(hwnd);
    }
    ~Window() { if (hwnd) DestroyWindow(hwnd); UnregisterClassW(L"MhwOwnedFrameGenerationOn", instance); }
};
struct Event {
    HANDLE value = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ~Event() { if (value) CloseHandle(value); }
};
struct Frames {
    mhw::reflex::FrameSession session;
    mhw::reflex::FrameStamp stamp{};
    unsigned begins{}, ends{}, tests{}, faults{};
    explicit Frames(mhw::reflex::Streamline& sdk) : session(sdk) {}
    static bool begin(void* p, std::uintptr_t, unsigned flags) noexcept {
        auto& f = *static_cast<Frames*>(p);
        if (flags & DXGI_PRESENT_TEST) { ++f.tests; return false; }
        ++f.begins;
        try { if (f.session.present_begin(GetCurrentThreadId(), f.stamp) == sl::Result::eOk) return true; }
        catch (...) {}
        ++f.faults; return false;
    }
    static void end(void* p, std::uintptr_t, unsigned, HRESULT result) noexcept {
        auto& f = *static_cast<Frames*>(p); ++f.ends;
        try { if (SUCCEEDED(result) && f.session.present_end(GetCurrentThreadId(), f.stamp) == sl::Result::eOk) return; }
        catch (...) {}
        ++f.faults;
    }
};
void diagnostics(ID3D12InfoQueue* messages, std::ofstream& log, const char* phase) {
    unsigned warnings{}, errors{};
    for (UINT64 i = 0; i < messages->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i) {
        SIZE_T size{}; hr(messages->GetMessage(i, nullptr, &size), "message size");
        std::vector<std::byte> data(size); auto* msg = reinterpret_cast<D3D12_MESSAGE*>(data.data());
        hr(messages->GetMessage(i, msg, &size), "message");
        if (msg->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) ++errors;
        if (msg->Severity == D3D12_MESSAGE_SEVERITY_WARNING) ++warnings;
        if (msg->Severity <= D3D12_MESSAGE_SEVERITY_WARNING && warnings + errors <= 16)
            std::cerr << msg->ID << ": " << msg->pDescription << '\n';
    }
    log << "{\"event\":\"gpu_diagnostics\",\"phase\":\"" << phase << "\",\"warnings\":" << warnings
        << ",\"errors\":" << errors << "}\n" << std::flush;
    require(!warnings && !errors, "DX12 validation");
}
void dxgi_diagnostics(IDXGIInfoQueue* messages, std::ofstream& log, const char* phase) {
    unsigned warnings{}, errors{};
    for (UINT64 i = 0; i < messages->GetNumStoredMessages(DXGI_DEBUG_ALL); ++i) {
        SIZE_T size{}; hr(messages->GetMessage(DXGI_DEBUG_ALL, i, nullptr, &size), "DXGI message size");
        std::vector<std::byte> data(size); auto* msg = reinterpret_cast<DXGI_INFO_QUEUE_MESSAGE*>(data.data());
        hr(messages->GetMessage(DXGI_DEBUG_ALL, i, msg, &size), "DXGI message");
        if (msg->Severity <= DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR) ++errors;
        if (msg->Severity == DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING) ++warnings;
        if (msg->Severity <= DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING && warnings + errors <= 16)
            std::cerr << "DXGI " << msg->ID << ": " << msg->pDescription << '\n';
    }
    log << "{\"event\":\"dxgi_diagnostics\",\"phase\":\"" << phase << "\",\"warnings\":" << warnings
        << ",\"errors\":" << errors << "}\n" << std::flush;
    require(!warnings && !errors, "DXGI validation");
}
#include "frame_generation_fixture_inputs.inl"
bool run(int argc, wchar_t** argv) {
    require(argc >= 3, "usage: frame_generation_on_fixture sdk_runtime new_output_directory [--wait-for-focus] [--generate 2|3|5]");
    bool wait_for_focus{}; unsigned requested_generated{1};
    for (int i = 3; i < argc; ++i) {
        const std::wstring_view arg(argv[i]);
        if (arg == L"--wait-for-focus" && !wait_for_focus) { wait_for_focus = true; continue; }
        if (arg == L"--generate" && requested_generated == 1 && i + 1 < argc) {
            const std::wstring_view count(argv[++i]);
            require(count == L"2" || count == L"3" || count == L"5", "owned fixed MFG count");
            requested_generated = static_cast<unsigned>(count[0] - L'0'); continue;
        }
        require(false, "owned FG fixture argument");
    }
    const auto output = std::filesystem::absolute(argv[2]);
    require(!std::filesystem::exists(output), "preserve evidence");
    std::filesystem::create_directories(output);
    std::ofstream log(output / L"events.jsonl", std::ios::binary); require(bool(log), "log"); log << std::boolalpha;
    log << "{\"event\":\"start\",\"schema\":1,\"version\":\"0.3.2\",\"sdk\":\"2.12.0\","
        "\"graphics_api\":\"D3D12\",\"game_access\":false,\"fg_mode\":\"On-fixed\",\"requested_generated\":"
        << requested_generated << ",\"multiplier\":" << requested_generated + 1 << ",\"static_inputs\":false,"
        "\"resource_tags\":5,\"visible_window\":true,\"gpu_validation\":true,\"direct_sdk\":" << direct_mode
        << ",\"wait_for_focus\":" << wait_for_focus << "}\n" << std::flush;
    Window window; Event event; require(event.value != nullptr, "event");
    // SDK initialization precedes any DXGI/D3D12 call in this fixture.
    mhw::reflex::Streamline sdk; mhw::reflex::Presentation bridge(sdk); Frames frames(sdk);
    ComPtr<ID3D12Debug> debug;
    ComPtr<ID3D12Debug1> validation;
    ComPtr<IDXGIFactory2> factory; ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIFactory2> direct_factory; ComPtr<ID3D12Device> direct_device;
    ComPtr<IDXGIInfoQueue> dxgi_messages;
    ComPtr<ID3D12Device> device; ComPtr<ID3D12InfoQueue> messages;
    ComPtr<ID3D12InfoQueue1> callbacks; DWORD callback_cookie{};
    ComPtr<ID3D12CommandQueue> queue; ComPtr<IDXGISwapChain1> swap; ComPtr<IDXGISwapChain3> chain;
    ComPtr<ID3D12CommandQueue> direct_native_queue;
    ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> list;
    ComPtr<ID3D12Resource> upload; ComPtr<ID3D12Fence> fence;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    std::array<ComPtr<ID3D12Resource>, 2> buffers;
    std::unique_ptr<OwnedFgInputs> inputs;
    const sl::ViewportHandle viewport{0};
    try {
        require(sdk.frame_generation_off(viewport) == sl::Result::eErrorNotInitialized, "reject FG before initialization");
        sdk.initialize(std::filesystem::canonical(argv[1]), output / L"sdk-logs",
            mhw::reflex::StreamlineFeatures::reflex_and_frame_generation);
        log << "{\"event\":\"sdk_initialized\",\"before_dxgi_device\":true,\"features\":[\"Reflex\",\"PCL\",\"DLSS_G\"]}\n" << std::flush;
        hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)), "debug layer"); debug->EnableDebugLayer();
        hr(debug.As(&validation), "GPU validation"); validation->SetEnableGPUBasedValidation(TRUE);
        hr(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_messages)), "DXGI info queue");
        hr(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)), "native factory");
        DXGI_ADAPTER_DESC1 desc{};
        for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter)); ++i) {
            hr(adapter->GetDesc1(&desc), "adapter desc");
            if (desc.VendorId == 0x10de && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) break;
            adapter.Reset();
        }
        require(adapter != nullptr, "NVIDIA adapter");
        sl::AdapterInfo info{}; info.deviceLUID = reinterpret_cast<std::uint8_t*>(&desc.AdapterLuid);
        info.deviceLUIDSizeInBytes = sizeof(desc.AdapterLuid);
        sl::FeatureRequirements requirements{};
        const auto support = sdk.supported(sl::kFeatureDLSS_G, info);
        const auto required = sdk.requirements(sl::kFeatureDLSS_G, requirements);
        log << "{\"event\":\"feature_support\",\"result\":" << static_cast<unsigned>(support)
            << ",\"requirements_result\":" << static_cast<unsigned>(required)
            << ",\"requirement_flags\":" << static_cast<unsigned>(requirements.flags)
            << ",\"max_viewports\":" << requirements.maxNumViewports << ",\"required_tags\":" << requirements.numRequiredTags
            << ",\"adapter_luid_low\":" << desc.AdapterLuid.LowPart << ",\"adapter_luid_high\":" << desc.AdapterLuid.HighPart
            << ",\"vendor_id\":" << desc.VendorId << ",\"device_id\":" << desc.DeviceId << "}\n" << std::flush;
        api(support, "FG support"); api(required, "FG requirements");
        hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)), "native device");
        hr(device.As(&messages), "DX12 info queue");
        hr(device.As(&callbacks), "DX12 message callback");
        hr(callbacks->RegisterMessageCallback(gpu_message, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &callback_cookie),
            "register GPU message callback");
        api(sdk.attach(device.Get()), "attach FG/Reflex");
        api(sdk.options(sl::ReflexMode::eOff), "Reflex startup Off");
        api(sdk.frame_generation_off(viewport), "FG Off");
        auto parent = mhw::reflex::adapter_factory(adapter.Get());
        require(parent.Get() == factory.Get(), "native parent factory");
        auto* creation_device = device.Get(); auto* creation_factory = factory.Get();
#ifdef MHW_FG_DIRECT_FIXTURE
        direct_device = device; direct_factory = parent;
        api(mhw::reflex::upgrade_owned(direct_device, [&](void** out) { return sdk.upgrade(out); }), "direct SDK device");
        api(mhw::reflex::upgrade_owned(direct_factory, [&](void** out) { return sdk.upgrade(out); }), "direct SDK factory");
        creation_device = direct_device.Get(); creation_factory = direct_factory.Get();
        log << "{\"event\":\"direct_sdk_control\",\"native_hooks\":0}\n" << std::flush;
#else
        bridge.install(device.Get(), parent.Get(), mhw::reflex::PresentationContract::sdk_paced);
        bridge.observe({&frames, Frames::begin, Frames::end});
#endif
        parent.Reset();
        D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr(creation_device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "returned queue");
#ifdef MHW_FG_DIRECT_FIXTURE
        void* native_queue{}; api(sdk.native(queue.Get(), &native_queue), "direct native queue");
        require(native_queue != nullptr, "owned native queue reference");
        ComPtr<IUnknown> native_queue_owned; native_queue_owned.Attach(static_cast<IUnknown*>(native_queue));
        hr(native_queue_owned.As(&direct_native_queue), "direct native queue type");
        native_queue_owned.Reset();
#endif
        DXGI_SWAP_CHAIN_DESC1 sc{}; sc.Width = 1280; sc.Height = 720; sc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sc.SampleDesc.Count = 1; sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sc.BufferCount = 2;
        sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGIFactory5> factory5; hr(factory.As(&factory5), "tearing feature query");
        BOOL tearing{}; hr(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing)), "tearing support");
        require(tearing != FALSE, "SDK pacer tearing support"); factory5.Reset();
        // The SDK's windowed pacer submits ALLOW_TEARING even in Off mode.
        // The creation and resize flags must permit that presentation flag.
        sc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT | DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        hr(creation_factory->CreateSwapChainForHwnd(queue.Get(), window.hwnd, &sc, nullptr, nullptr, &swap), "returned swap");
        hr(swap.As(&chain), "swap3");
        sl::DLSSGState initial{}; api(sdk.frame_generation_state(viewport, initial), "initial FG state");
        log << "{\"event\":\"fg_capability\",\"max_generated_frames\":" << initial.numFramesToGenerateMax
            << ",\"minimum_dimension\":" << initial.minWidthOrHeight << ",\"status\":" << static_cast<unsigned>(initial.status)
            << ",\"dynamic_mfg\":" << (initial.bIsDynamicMFGSupported == sl::eTrue)
            << ",\"vsync_supported\":" << (initial.bIsVsyncSupportAvailable == sl::eTrue)
            << ",\"width\":1280,\"height\":720,\"swapchain_format\":" << static_cast<unsigned>(sc.Format)
            << ",\"swapchain_flags\":" << sc.Flags << "}\n" << std::flush;
        require(initial.numFramesToGenerateMax >= requested_generated, "requested FG/MFG capability");
        require(sc.Width >= initial.minWidthOrHeight && sc.Height >= initial.minWidthOrHeight, "FG minimum dimensions");
        api(sdk.options(sl::ReflexMode::eLowLatency), "Reflex On");
        // Copy a host-owned pattern; clearing SDK-owned offscreen buffers
        // without their optimized clear value would trigger debug warning 820.
        D3D12_HEAP_PROPERTIES hp{}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC ud{}; ud.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ud.Width = 1920ULL * 1080 * 4; ud.Height = 1; ud.DepthOrArraySize = 1; ud.MipLevels = 1;
        ud.SampleDesc.Count = 1; ud.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        hr(device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &ud, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&upload)), "upload pattern");
        const auto acquire = [&] {
            for (UINT i = 0; i < 2; ++i) {
                hr(chain->GetBuffer(i, IID_PPV_ARGS(&buffers[i])), "routed buffer");
            }
            const auto texture = buffers[0]->GetDesc();
            UINT rows{}; UINT64 row_bytes{}, total{};
            device->GetCopyableFootprints(&texture, 0, 1, 0, &footprint, &rows, &row_bytes, &total);
            require(total <= ud.Width && rows == texture.Height && row_bytes == texture.Width * 4, "pattern footprint");
            void* mapped{}; const D3D12_RANGE no_reads{0, 0};
            hr(upload->Map(0, &no_reads, &mapped), "map pattern");
            for (UINT row = 0; row < rows; ++row) {
                auto* pixels = reinterpret_cast<std::uint32_t*>(static_cast<std::byte*>(mapped) +
                    footprint.Offset + static_cast<SIZE_T>(row) * footprint.Footprint.RowPitch);
                for (UINT col = 0; col < texture.Width; ++col) pixels[col] = 0xff224466;
            }
            const D3D12_RANGE written{0, static_cast<SIZE_T>(total)}; upload->Unmap(0, &written);
        };
        acquire(); hr(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&allocator)), "allocator");
        hr(device->CreateCommandList(0, q.Type, allocator.Get(), nullptr, IID_PPV_ARGS(&list)), "list");
        hr(list->Close(), "initial close");
        hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fence");
        const auto wait = [&](UINT64 value) {
            hr(queue->Signal(fence.Get(), value), "signal");
            hr(fence->SetEventOnCompletion(value, event.value), "completion");
            require(WaitForSingleObject(event.value, 10000) == WAIT_OBJECT_0, "GPU completion");
        };
        hr(allocator->Reset(), "input allocator reset"); hr(list->Reset(allocator.Get(), nullptr), "input list reset");
        inputs = std::make_unique<OwnedFgInputs>(device.Get(), list.Get());
        hr(list->Close(), "input list close"); ID3D12CommandList* initialization[]{list.Get()};
        queue->ExecuteCommandLists(1, initialization); wait(1);
        log << "{\"event\":\"owned_inputs_ready\",\"width\":1280,\"height\":720,\"depth_format\":41,"
            "\"motion_format\":16,\"hudless_format\":87,\"ui_alpha_format\":61,\"resource_state\":"
            << static_cast<unsigned>(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) << ","
            "\"static_camera\":true,\"moving_object\":true,\"ui_empty\":true,\"initialized_gpu_complete\":true}\n" << std::flush;
        log << "{\"event\":\"ready_for_test_present\"}\n" << std::flush;
        hr(chain->Present(0, DXGI_PRESENT_TEST), "test presentation");
#ifdef MHW_FG_DIRECT_FIXTURE
        require(!Frames::begin(&frames, reinterpret_cast<std::uintptr_t>(chain.Get()), DXGI_PRESENT_TEST), "direct test observer");
#endif
        log << "{\"event\":\"test_present_returned\"}\n" << std::flush;
        constexpr UINT count = 144;
        UINT64 reported{}, generated{}, fence_waits{}, focused_on_frames{}, full_multiplier_frames{};
        for (UINT frame = 1; frame <= count; ++frame) {
            // A synthetic host interval gives the pacer space between real
            // frames. This is an execution test, not a performance sample.
            Sleep(16);
            MSG msg{}; while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
            if (frame == 33) {
                // SDK setup can outlast our initial foreground grant. Retry
                // only for this owned test window and record actual focus.
                const auto focus_requested = SetForegroundWindow(window.hwnd);
                SetFocus(window.hwnd);
                if (wait_for_focus) {
                    log << "{\"event\":\"focus_wait_started\",\"ceiling_seconds\":60}\n" << std::flush;
                    const auto deadline = GetTickCount64() + 60000;
                    while (GetForegroundWindow() != window.hwnd && GetTickCount64() < deadline) {
                        MSG pending{};
                        while (PeekMessageW(&pending, nullptr, 0, 0, PM_REMOVE)) {
                            TranslateMessage(&pending); DispatchMessageW(&pending);
                        }
                        Sleep(10);
                    }
                    require(GetForegroundWindow() == window.hwnd, "owned FG window focus not acquired");
                }
                const auto on = mhw::reflex::driver_sleep_status(device.Get());
                require(on.result == 0 && on.low_latency, "Reflex active before FG On");
                api(requested_generated == 1 ? sdk.frame_generation_ordinary(viewport) :
                    sdk.frame_generation_multi(viewport, requested_generated), "fixed FG/MFG On");
                log << "{\"event\":\"fg_mode\",\"frame\":33,\"mode\":\"On\",\"generate\":" << requested_generated
                    << ",\"focus_request_succeeded\":"
                    << (focus_requested != FALSE) << ",\"foreground\":"
                    << (GetForegroundWindow() == window.hwnd) << "}\n" << std::flush;
            }
            if (frame == 129) {
                api(sdk.frame_generation_off(viewport), "FG restore Off");
                log << "{\"event\":\"fg_mode\",\"frame\":129,\"mode\":\"Off\"}\n" << std::flush;
            }
            const auto thread = GetCurrentThreadId();
            if (frame >= 33 && frame <= 128 && GetForegroundWindow() == window.hwnd) ++focused_on_frames;
            api(frames.session.begin(thread, {frame - 1, frame - 1, (frame - 1) & 1U, true}), "frame begin");
            frames.stamp = {frame, frame, frame & 1U, true};
            const auto* token = frames.session.input_token(thread);
            require(token && static_cast<std::uint32_t>(*token) == frames.session.id(), "borrow same Reflex input token");
            inputs->tag(sdk, *token, viewport, frame);
            api(frames.session.render_begin(thread, frames.stamp), "render begin");
            const auto index = chain->GetCurrentBackBufferIndex(); require(index < 2, "backbuffer index");
            hr(allocator->Reset(), "reset allocator"); hr(list->Reset(allocator.Get(), nullptr), "reset list");
            inputs->update(list.Get(), frame);
            D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = buffers[index].Get(); barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            list->ResourceBarrier(1, &barrier);
            auto source_barrier = barrier; source_barrier.Transition.pResource = inputs->textures[2].resource.Get();
            source_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            source_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            list->ResourceBarrier(1, &source_barrier); list->CopyResource(buffers[index].Get(), inputs->textures[2].resource.Get());
            std::swap(source_barrier.Transition.StateBefore, source_barrier.Transition.StateAfter); list->ResourceBarrier(1, &source_barrier);
            std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter); list->ResourceBarrier(1, &barrier);
            hr(list->Close(), "close list"); ID3D12CommandList* lists[]{list.Get()}; queue->ExecuteCommandLists(1, lists);
            api(frames.session.render_end(thread, frames.stamp), "render end");
            DXGI_PRESENT_PARAMETERS params{};
#ifdef MHW_FG_DIRECT_FIXTURE
            require(Frames::begin(&frames, reinterpret_cast<std::uintptr_t>(chain.Get()), 0), "direct Present begin");
#endif
            hr((frame & 1U) ? chain->Present(0, 0) : chain->Present1(0, 0, &params), "routed presentation");
#ifdef MHW_FG_DIRECT_FIXTURE
            Frames::end(&frames, reinterpret_cast<std::uintptr_t>(chain.Get()), 0, S_OK);
#endif
            api(frames.session.end(thread), "frame end");
            sl::DLSSGState state{}; api(sdk.frame_generation_state(viewport, state), "FG state");
            log << "{\"event\":\"fg_frame\",\"frame\":" << frame << ",\"token_id\":" << frames.session.id()
                << ",\"actual_presentations\":" << state.numFramesActuallyPresented
                << ",\"foreground\":" << (GetForegroundWindow() == window.hwnd)
                << ",\"status\":" << static_cast<unsigned>(state.status) << "}\n" << std::flush;
            require(state.status == sl::DLSSGStatus::eOk, "FG runtime state");
            reported += state.numFramesActuallyPresented;
            if (frame >= 33 && frame <= 128 && state.numFramesActuallyPresented > 1)
                generated += state.numFramesActuallyPresented - 1;
            if (frame >= 33 && frame <= 128 && state.numFramesActuallyPresented == requested_generated + 1)
                ++full_multiplier_frames;
            if (state.inputsProcessingCompletionFence && state.lastPresentInputsProcessingCompletionFenceValue) {
                auto* consumed = static_cast<ID3D12Fence*>(state.inputsProcessingCompletionFence);
                hr(consumed->SetEventOnCompletion(state.lastPresentInputsProcessingCompletionFenceValue, event.value), "FG inputs consumed");
                require(WaitForSingleObject(event.value, 10000) == WAIT_OBJECT_0, "SDK input consumption completed"); ++fence_waits;
            }
            wait(frame + 1);

        }
        const auto stats = bridge.snapshot();
#ifndef MHW_FG_DIRECT_FIXTURE
        require(stats.presents == count && stats.sdk_paced_presents == count && stats.test_presents == 1 && stats.resizes == 0 &&
            stats.queues == 1 && stats.swapchains == 1 && !stats.faults && stats.installed_hooks == mhw::reflex::Presentation::hook_capacity &&
            stats.identity && stats.native_swapchain != reinterpret_cast<std::uintptr_t>(chain.Get()) &&
            stats.proxy_swapchain != stats.native_swapchain, "FG Off native routing");
#endif
        require(frames.session.completed() == count && frames.session.tokens() == count && frames.session.sleeps() == count &&
            frames.session.markers() == count * 6 && frames.begins == count && frames.ends == count &&
            frames.tests == 1 && !frames.faults, "Reflex complete frames with FG Off");
        const auto driver_on = mhw::reflex::driver_sleep_status(device.Get());
        require(driver_on.result == 0 && driver_on.low_latency, "driver On confirmed");
        const bool actual_fg = generated >= requested_generated && reported > count && full_multiplier_frames != 0;
        // A negative execution result must still follow the complete normal
        // teardown. SDK acceptance alone is not an FG success.
        frames.session.stop();
        require(bridge.stop(), "restore hooks"); require(!bridge.release_after_shutdown(), "reject early pin release");
        api(sdk.options(sl::ReflexMode::eOff), "Reflex restore Off");
        const auto off = mhw::reflex::wait_driver_off(device.Get());
        require(off.status.result == 0 && !off.status.low_latency, "driver Off confirmed");
        diagnostics(messages.Get(), log, "before_shutdown");
        dxgi_diagnostics(dxgi_messages.Get(), log, "before_shutdown");
        log << "{\"event\":\"sdk_shutdown_begin\"}\n" << std::flush;
        api(sdk.shutdown(), "SDK shutdown");
        log << "{\"event\":\"sdk_shutdown_complete\"}\n" << std::flush;
        require(bridge.release_after_shutdown(), "release SDK pins");
        direct_device.Reset(); direct_factory.Reset();
#ifdef MHW_FG_DIRECT_FIXTURE
        // Native DXGI creation after shutdown cannot accept a stale SDK queue
        // proxy. Borrow its native interface while the SDK is still alive.
        queue.Reset(); queue = direct_native_queue; direct_native_queue.Reset();
#endif
        log << "{\"event\":\"pins_released\"}\n" << std::flush;
        // A FG-created chain belongs to the SDK's pacer queue. Presenting it
        // after SDK shutdown submits fresh work to that queue; fencing the
        // host queue cannot prove that work finished. Return to a newly
        // created native chain on the host's queue instead.
        log << "{\"event\":\"old_fg_chain_release_begin\"}\n" << std::flush;
        for (auto& buffer : buffers) buffer.Reset();
        log << "{\"event\":\"old_fg_buffers_released\"}\n" << std::flush;
        chain.Reset(); swap.Reset();
        log << "{\"event\":\"old_fg_chain_released\"}\n" << std::flush;
        hr(factory->CreateSwapChainForHwnd(queue.Get(), window.hwnd, &sc, nullptr, nullptr, &swap),
            "new native chain after SDK shutdown");
        hr(swap.As(&chain), "new native swap3"); acquire();
        const auto index = chain->GetCurrentBackBufferIndex(); require(index < 2, "new native index");
        hr(allocator->Reset(), "native reset allocator"); hr(list->Reset(allocator.Get(), nullptr), "native reset list");
        D3D12_RESOURCE_BARRIER native_barrier{}; native_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        native_barrier.Transition.pResource = buffers[index].Get();
        native_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        native_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        native_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        list->ResourceBarrier(1, &native_barrier);
        D3D12_TEXTURE_COPY_LOCATION native_dst{}, native_src{};
        native_dst.pResource = buffers[index].Get(); native_dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        native_src.pResource = upload.Get(); native_src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        native_src.PlacedFootprint = footprint; list->CopyTextureRegion(&native_dst, 0, 0, 0, &native_src, nullptr);
        std::swap(native_barrier.Transition.StateBefore, native_barrier.Transition.StateAfter);
        list->ResourceBarrier(1, &native_barrier); hr(list->Close(), "native close");
        ID3D12CommandList* native_lists[]{list.Get()}; queue->ExecuteCommandLists(1, native_lists);
        hr(chain->Present(0, 0), "new native presentation after stop"); wait(count + 2);
        log << "{\"event\":\"native_post_stop_present_complete\"}\n" << std::flush;
        require(frames.begins == count && frames.ends == count, "no observer after shutdown");
        hr(device->GetDeviceRemovedReason(), "device health");
        for (auto& buffer : buffers) buffer.Reset();
        inputs->release(); inputs.reset();
        list.Reset(); allocator.Reset(); upload.Reset(); fence.Reset(); chain.Reset(); swap.Reset(); queue.Reset();
        diagnostics(messages.Get(), log, "after_swap_destruction");
        dxgi_diagnostics(dxgi_messages.Get(), log, "after_swap_destruction");
        callbacks->UnregisterMessageCallback(callback_cookie); callbacks.Reset();
        messages.Reset(); adapter.Reset(); factory.Reset();
        const auto refs = device.Detach()->Release(); require(refs == 0, "zero final device references");
        log << "{\"event\":\"finished\",\"accepted\":" << actual_fg << ",\"fg_mode\":\"On-fixed\",\"generated_frames\":" << generated << ","
            "\"requested_generated\":" << requested_generated << ",\"multiplier\":" << requested_generated + 1
            << ",\"full_multiplier_frames\":" << full_multiplier_frames << ","
            "\"host_frames\":" << count << ",\"native_presentations\":" << stats.native_presents
            << ",\"sdk_reported_presentations\":" << reported << ",\"reflex_frames\":" << frames.session.completed()
            << ",\"sdk_paced_host_calls\":" << stats.sdk_paced_presents
            << ",\"tokens\":" << frames.session.tokens() << ",\"sleeps\":" << frames.session.sleeps()
            << ",\"markers\":" << frames.session.markers() << ",\"input_fence_waits\":" << fence_waits
            << ",\"focused_on_frames\":" << focused_on_frames << ",\"resizes\":0,\"faults\":0,\"hooks_restored\":true,\"installed_hooks_during_test\":" << stats.installed_hooks << ","
            "\"driver_on_verified\":true,\"driver_off_verified\":true,\"sdk_shutdown\":true,\"pins_released\":true,"
            "\"native_after_stop\":true,\"native_chain_recreated\":true,\"device_refs\":0,\"debug_warnings\":0,\"debug_errors\":0}\n" << std::flush;
        require(bool(log), "save result");
        return actual_fg;
    } catch (...) {
        // Preserve the initiating exception before teardown can diagnose a
        // different resource still in flight. No acceptance on this path.
        try { throw; } catch (const std::exception& error) {
            std::cerr << error.what() << '\n' << std::flush;
            log << "{\"event\":\"failure\",\"reason\":" << std::quoted(error.what()) << "}\n" << std::flush;
        } catch (...) { log << "{\"event\":\"failure\",\"reason\":\"non-C++ exception\"}\n" << std::flush; }
        if (queue && fence && event.value && SUCCEEDED(queue->Signal(fence.Get(), 1000)) &&
            SUCCEEDED(fence->SetEventOnCompletion(1000, event.value))) {
            const bool idle = WaitForSingleObject(event.value, 10000) == WAIT_OBJECT_0;
            log << "{\"event\":\"failure_gpu_drain\",\"completed\":" << idle << "}\n" << std::flush;
        }
        if (messages) { try { diagnostics(messages.Get(), log, "failure"); } catch (...) {} }
        if (dxgi_messages) { try { dxgi_diagnostics(dxgi_messages.Get(), log, "failure"); } catch (...) {} }
        frames.session.stop();
        const bool stopped = bridge.stop();
        if (stopped && sdk.initialized()) sdk.options(sl::ReflexMode::eOff);
        const auto shutdown = stopped ? sdk.shutdown() : sl::Result::eErrorInvalidState;
        const bool released = stopped && shutdown == sl::Result::eOk && bridge.release_after_shutdown();
        log << "{\"event\":\"failure_cleanup\",\"hooks_restored\":" << stopped << ",\"shutdown_result\":"
            << static_cast<unsigned>(shutdown) << ",\"pins_released\":" << released << "}\n" << std::flush;
        throw;
    }
}
}
int wmain(int argc, wchar_t** argv) {
    try { return run(argc, argv) ? 0 : 2; } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
