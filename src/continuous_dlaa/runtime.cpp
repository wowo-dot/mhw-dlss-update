// Continuous DLSS on the game's TAA pass: the runtime's qualification,
// install, stop and finish (runtime_state.hpp lists the other parts).
#include "runtime_state.hpp"
#include "../game_memory/layout.hpp"

extern "C" void temporal_dispatch_entry();
extern "C" void temporal_consumer_entry();
extern "C" void temporal_depth_producer_entry();
extern "C" void temporal_depth_consumer_entry();
extern "C" void temporal_jitter_entry();
namespace mhw::continuous_dlaa::detail {
// Whether `needed` more bytes fit the video memory budget; keeps the check for
// the summary and, on a refusal, the reason.
bool memory_fits(std::uint64_t needed, gpu_memory::Check &c, std::string &why) {
    c = gpu_memory::check(needed);
    {
        Lock lock;
        memory_check = c;
    }
    if (c.passed) return true;
    why = c.read ? "video memory: " + std::to_string(needed >> 20) + " MB needed, " +
                       std::to_string(gpu_memory::room({c.budget, c.usage}) >> 20) + " MB left in the budget"
                 : "video memory budget unavailable";
    return false;
}
namespace {
// The game's adapter for the video memory budget: its swap chain's factory
// enumerates it by the device's LUID (no second factory). Once a process.
bool pin_adapter(ID3D12Device *device, std::uintptr_t swapchain) noexcept {
    if (gpu_memory::pinned.load()) return true;
    ComPtr<IDXGISwapChain> chain;
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter3> adapter;
    return device && swapchain &&
           SUCCEEDED(reinterpret_cast<IUnknown *>(swapchain)->QueryInterface(IID_PPV_ARGS(&chain))) &&
           SUCCEEDED(chain->GetParent(IID_PPV_ARGS(&factory))) &&
           SUCCEEDED(factory->EnumAdapterByLuid(device->GetAdapterLuid(), IID_PPV_ARGS(&adapter))) &&
           gpu_memory::pin(adapter.Get());
}
bool qualified(std::uintptr_t image, pb::Runtime &found, std::string &reason) {
    if (!pb::discover(image, found, reason) ||
        !pb::verify_code(image, 0x2392150, 4221, "d2c6c0f6a5ed39c998bcf62afed19132e6dd0d7cfbb19e18b80c15531af2bf91") ||
        !pb::verify_code(image, tb::dispatch_rva, 321,
                         "d073b70ae437d281596861720893c3b3cc9f82155fa0c83f2c3ae6918fd2f3a2") ||
        !pb::verify_code(image, 0x259cc20, 1283, "6921c7cbfc7b277f4b076a68de8165bd54b3f6571ca574fdb16d5c35cbe154da") ||
        !pb::verify_code(image, 0x23cd170, 1711, "8103c87804f325a15bbeb3529f3b575cc14e3bae986156d53e1b796fbbec6d31") ||
        !pb::verify_code(image, 0x259ba50, 495, "f560506e570fff4dd21df27e5d68db204420e82e3eed1bb5aae2cc3ad1441ee5") ||
        !pb::verify_code(image, 0x259df30, 681, "e84ae8742c4a0ef334692595bb89c04c8be80306d4416c20304566cc1e5dd85a") ||
        !pb::verify_code(image, 0x228f3b0, 2000, "df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373") ||
        !pb::verify_code(image, 0x2292610, 304, "4adcc8e99a9ff16ba9af27c8db165b998958561d82da0d3c9e7bcdcb82cd37f9") ||
        !pb::verify_code(image, 0x22927f0, 736, "bef3f834e34a78dce1392d047062123d7c2d5633de7390aa0216cb654519e2ed") ||
        !pb::verify_code(image, 0x229cee0, 7756, "11f7cc4d48598967f439659c87142839c4ad3fb900cfafb7fab3631655691af3") ||
        !pb::verify_code(image, 0x24e7a10, 2620, "7ea61c6f4bbb21c5d8a25d5801a92476b825595c96937ac57d1e44870caea6fb")) {
        if (reason.empty()) reason = "continuous-code-gate";
        return false;
    }
    return true;
}
} // namespace
} // namespace mhw::continuous_dlaa::detail

namespace mhw::continuous_dlaa {
using namespace detail;
bool ready(std::uintptr_t image, std::string &reason) {
    try {
        if (pass) {
            reason = "continuous-already-prepared";
            return false;
        }
        pb::Runtime found;
        if (!qualified(image, found, reason)) return false;
        if (!render_resolution::ready(image, dlss_sdk::requested_scale(configured_mode), reason)) return false;
        return true;
    } catch (const std::exception &e) {
        reason = e.what();
        return false;
    }
}
bool install(std::uintptr_t image, const std::filesystem::path &sdk, std::string &reason) {
    try {
        if (pass) {
            reason = "continuous-already-prepared";
            return false;
        }
        if (!qualified(image, runtime, reason)) return false;
        cfg = {image, 0, runtime.backend, runtime.device};
        if (!pb::read(image + layout::image::renderer, cfg.renderer) || !cfg.renderer)
            throw std::runtime_error("continuous renderer");
        auto *device = reinterpret_cast<ID3D12Device *>(cfg.device);
        {
            const auto planned = render_resolution::plan({runtime.width, runtime.height},
                                                         runtime.scene_override ? runtime.window_width : runtime.width,
                                                         dlss_sdk::requested_scale(configured_mode));
            if (!planned.valid) throw std::runtime_error("scaled render plan");
            scaled_render = {planned.render.width, planned.render.height};
            edge_column =
                static_cast<float>(runtime.width) * planned.quantized > static_cast<float>(planned.render.width);
            edge_row =
                static_cast<float>(runtime.height) * planned.quantized > static_cast<float>(planned.render.height);
        }
        const unsigned planned_width = render_width(), planned_height = render_height();
        // The images must fit the video memory budget (gpu_memory/budget.hpp).
        if (!pin_adapter(device, runtime.swapchain)) throw std::runtime_error("video memory: the game's adapter");
        gpu_memory::Check memory;
        if (std::string why; !memory_fits(
                gpu_memory::dlss_bytes(runtime.width, runtime.height, planned_width, planned_height), memory, why))
            throw std::runtime_error(why);
        const auto byte_limit = gpu_memory::room({memory.budget, memory.usage});
        constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_UINT};
        for (unsigned i = 0; i < 3; ++i) {
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.Width = runtime.width;
            desc.Height = runtime.height;
            desc.DepthOrArraySize = desc.MipLevels = 1;
            desc.Format = formats[i];
            desc.SampleDesc.Count = 1;
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type = D3D12_HEAP_TYPE_DEFAULT;
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                       D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&raw[i]))))
                throw std::runtime_error("continuous raw allocation");
        }
        const auto *initialization = ngx_startup::borrowed_initialization();
        if (!initialization) throw std::runtime_error("continuous startup ownership");
        sdk_runtime = sdk;
        // DLAA renders at the scene size. A basis width that is not a multiple
        // of 16 pixels quantizes 100% just below it; DLSS then upscales that
        // extent as Quality.
        auto mode = configured_mode;
        if (mode == dlss_sdk::ReconstructionMode::dlaa &&
            (render_width() != runtime.width || render_height() != runtime.height))
            mode = dlss_sdk::ReconstructionMode::quality;
        scaled_geometry = {
            {runtime.width, runtime.height}, {render_width(), render_height()}, {runtime.width, runtime.height}, mode};
        pass = std::make_unique<dlss_sdk::InlineFrame>(
            device, scaled_geometry, std::array<ID3D12Resource *, 3>{raw[0].Get(), raw[1].Get(), raw[2].Get()}, sdk,
            *initialization, byte_limit);
        spatial = std::make_unique<temporal_present::Output>(
            device, temporal_present::SpatialSource{runtime.width, runtime.height, render_width(), render_height()},
            runtime.width, runtime.height, true, true);
        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
            throw std::runtime_error("continuous fence");
        if (!unfiltered_lifetime.initialize(device)) throw std::runtime_error("unfiltered lifetime fence");
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> list;
        ComPtr<ID3D12CommandQueue> queue;
        D3D12_COMMAND_QUEUE_DESC q{};
        q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        // The discovery queue is the mod's own: never proxied for the presentation (v0.28.6).
        {
            astera::Internal internal;
            if (FAILED(device->CreateCommandAllocator(q.Type, IID_PPV_ARGS(&allocator))) ||
                FAILED(device->CreateCommandList(0, q.Type, allocator.Get(), nullptr, IID_PPV_ARGS(&list))) ||
                FAILED(list->Close()) || FAILED(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue))))
                throw std::runtime_error("continuous method discovery");
        }
        targets[0] = image + tb::dispatch_rva;
        targets[1] = image + 0x259cc20;
        targets[2] = image + 0x23cd170;
        targets[3] = image + 0x259df30;
        targets[4] = image + 0x228f3b0;
        constexpr unsigned slots[]{10, 9, 10, 26, 14, 17, 12, 13};
        for (unsigned i = 5; i < hook_count; ++i)
            if (!pb::system_method(reinterpret_cast<std::uintptr_t>(i == 7 ? static_cast<IUnknown *>(queue.Get())
                                                                           : static_cast<IUnknown *>(list.Get())),
                                   slots[i - 5], targets[i]))
                throw std::runtime_error("continuous API gate");
        const std::array<void *, hook_count> detours{reinterpret_cast<void *>(temporal_dispatch_entry),
                                                     reinterpret_cast<void *>(temporal_consumer_entry),
                                                     reinterpret_cast<void *>(temporal_depth_producer_entry),
                                                     reinterpret_cast<void *>(temporal_depth_consumer_entry),
                                                     reinterpret_cast<void *>(temporal_jitter_entry),
                                                     reinterpret_cast<void *>(reset_hook),
                                                     reinterpret_cast<void *>(close_hook),
                                                     reinterpret_cast<void *>(execute_hook),
                                                     reinterpret_cast<void *>(barrier_hook),
                                                     reinterpret_cast<void *>(dispatch_hook),
                                                     reinterpret_cast<void *>(copy_hook),
                                                     reinterpret_cast<void *>(draw_hook),
                                                     reinterpret_cast<void *>(indexed_hook)};
        // The sizing gates must be checked before activation modifies their callees.
        // dlss_mode = off: everything is installed, nothing taken over.
        // The game's own DLSS rules (game_options.hpp), before the render
        // scale is taken: its start-up check needs the game's scale at 1.
        // Without them DLSS runs as before, over the player's own settings.
        std::string why;
        const bool options = game_options::install(image, why);
        texture_bias::install(image);
        if (!off_at_start) {
            // Without CAS's part (its code differs) the player's CAS stays
            // on, and its content scale refuses the render scale.
            if (unsigned char cas{};
                !game_options::cas_handled() && pb::read(cfg.renderer + layout::renderer::cas, cas) && cas)
                throw std::runtime_error("fidelityfx-cas");
            if (options) {
                game_options::take_over();
                if (!wait_for([&] { return render_resolution::full_scale(image); }, 2000))
                    throw std::runtime_error("the game did not apply resolution scaling High and CAS Off");
            }
        }
        if (!render_resolution::install(image, dlss_sdk::requested_scale(configured_mode), reason, off_at_start))
            throw std::runtime_error(reason);
        if (!obs::install(device, pb::system_method, reason, render_resolution::observe_viewports))
            throw std::runtime_error(reason);
        if (!sr::prepare(cfg, runtime.width, runtime.height, render_width(), render_height(), reason))
            throw std::runtime_error(reason);
        sr::observe_final_scene(&fg_final_scene);
        for (unsigned i = 0; i < targets.size(); ++i) {
            if (i < first_api_hook) {
                auto &h = game_hooks[i];
                h.name = hook_names[i];
                h.pattern = hook_patterns[i];
                h.target = reinterpret_cast<void *>(targets[i]);
                if (!astera::hook_owned(h, detours[i], &originals[i]))
                    throw std::runtime_error("continuous hook creation");
                enabled[i] = true;
                continue;
            }
            if (!originals[i]) throw std::runtime_error("continuous AL chain");
            enabled[i] = true;
        }
        // Each hook is live as it goes in (al.hooks): the player's Off at start
        // must hold before activation can own a camera frame.
        activation::yield(off_at_start);
        if (!activation::prepare(image, cfg.renderer, reason)) throw std::runtime_error(reason);
        obs::begin();
        if (off_at_start)
            off.store(true);
        else
            sr::begin();
        accepting.store(!off_at_start);
        live_installed.store(true);
        chain_live.store(true, std::memory_order_release);
        sr::chains_live(true);
        return true;
    } catch (const std::exception &e) {
        reason = e.what();
        stop();
        return false;
    }
}
void stop() noexcept {
    accepting.store(false);
    live_installed.store(false);
    activation::stop();
    sr::stop();
    render_resolution::stop();
    give_back_options();
}
bool healthy() noexcept {
    return fault.load() == 0 && obs::healthy() && activation::healthy() && sr::healthy() &&
           render_resolution::healthy();
}
void finish() noexcept {
    stop();
    const auto deadline = GetTickCount64() + 2000;
    while ((!pending.zero() || projection_busy.test() || !activation::drained()) && GetTickCount64() < deadline)
        Sleep(1);
    if (!pending.zero() || projection_busy.test() || !activation::drained()) {
        fail(24);
        return;
    }
    // Restore native sizing while the render-thread callback is still installed.
    // Fault 47 (44 up to 0.28.6, which also meant a failed consumer depth copy).
    if (!render_resolution::finish()) {
        fail(47);
        return;
    }
    while (sr::healthy() && !sr::drained() && GetTickCount64() < deadline + 6000) Sleep(1);
    if (!sr::drained()) {
        fail(45);
        // A rejected frame cannot finish its logical chain. Retain uncertain
        // pins, but still remove interception once route callbacks are idle.
        bool abandoned = sr::abandon();
        while (!abandoned && GetTickCount64() < deadline + 6000) {
            Sleep(1);
            abandoned = sr::abandon();
        }
        if (!abandoned) return;
    }
    // The game hooks come out of their chains at once (OwnedHook); the D3D12
    // chains stay, with every detour forwarding once chain_live is off.
    const bool route_unhooked = sr::unhook();
    if (!route_unhooked) fail(45);
    if (!activation::unhook()) fail(25);
    std::array<bool, targets.size()> disable_queued{};
    for (unsigned i = 0; i < targets.size(); ++i)
        if (enabled[i]) {
            disable_queued[i] = i >= first_api_hook || astera::unhook_owned(game_hooks[i]);
            if (!disable_queued[i]) fail(25);
        }
    chain_live.store(false, std::memory_order_release);
    for (unsigned i = 0; i < targets.size(); ++i)
        if (disable_queued[i]) enabled[i] = false;
    if (route_unhooked) sr::disabled(true);
    // A forwarded callback may have entered before the patch was removed.
    // Keep the observer and all trampolines available until it returns.
    const auto return_deadline = GetTickCount64() + 2000;
    const auto callbacks_busy = []() {
        return !pending.zero() || projection_busy.test() || !activation::drained() || !sr::callbacks_idle();
    };
    while (callbacks_busy() && GetTickCount64() < return_deadline) Sleep(1);
    if (callbacks_busy()) {
        fail(24);
        return;
    }
    if (!obs::finish()) fail(27);
    // Completed AND reset recordings can release their resource pins. Remaining
    // recordings, private SDK state and list registry stay pinned at stop.
    // No inference of retirement, game-thread wait, unload or SDK Shutdown.
}
void save_rejected_shader(const std::filesystem::path &path) noexcept {
    try {
        Lock lock;
        if (!shader_rejection.present || !shader_rejection.bytes || accepting.load() || !pending.zero() ||
            !activation::drained())
            return;
        const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        DWORD written{};
        shader_rejection.file_saved =
            WriteFile(file, shader_rejection.bytecode.data(), shader_rejection.bytes, &written, nullptr) &&
            written == shader_rejection.bytes && FlushFileBuffers(file);
        CloseHandle(file);
    } catch (...) { /* Artifact errors never change renderer recovery. */
    }
}
bool quiescent() noexcept {
    Lock lock;
    if (accepting.load() || !pending.zero() || projection_busy.test() || !activation::drained() || !obs::settled() ||
        !obs::hooks_disabled() || lease.list ||
        (fence && (fence->GetCompletedValue() == UINT64_MAX || fence->GetCompletedValue() < signal_value)))
        return false;
    for (bool e : enabled)
        if (e) return false;
    if (!sr::callbacks_idle() || !sr::drained()) return false;
    return healthy();
}
} // namespace mhw::continuous_dlaa
