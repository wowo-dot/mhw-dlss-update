// Live changes from the worker: a preset change and the game's aspect change
// (one rebuild), and the player's Off. Each pauses admission, drains, and
// resumes or stops.
#include "runtime_state.hpp"

namespace mhw::continuous_dlaa::detail {
// The player's anti-aliasing, CAS and resolution scaling again
// (game_options.hpp), once the render thread restored the owned scale and a
// visit saw the game's own scale whole: the player's CAS (content scale 0.8)
// refuses those visits from then, and finish() needs one.
void give_back_options() noexcept {
    if (!game_options::taken()) return;
    const auto seen = render_resolution::restorations();
    wait_for(render_resolution::released, 1000);
    wait_for([&] { return render_resolution::restorations() > seen; }, 1000);
    game_options::give_back();
}
namespace {
// Observes `count` at zero once within `ms`. Every hook counts itself before
// it reads `accepting`, so after a stop one zero proves that no callback that
// could still have seen the runtime accepting is running.
bool observed_zero(const perf::InFlightCount &count, std::uint64_t ms) {
    // One flush (zero()), then polling the slots.
    if (count.zero()) return true;
    const auto end = GetTickCount64() + ms;
    while (!count.idle()) {
        if (GetTickCount64() >= end) return false;
        SwitchToThread();
    }
    return true;
}
template <class F> bool observed_true(F &&idle, std::uint64_t ms) {
    const auto end = GetTickCount64() + ms;
    while (!idle()) {
        if (GetTickCount64() >= end) return false;
        SwitchToThread();
    }
    return true;
}
// After a pause: nothing of ours in flight (the live rebuild's step 2).
bool drained_now() {
    Lock lock;
    const auto completed = fence ? fence->GetCompletedValue() : 0;
    if (completed == UINT64_MAX) return false;
    sr::collect(completed);
    return !publishing.load() && !projection_busy.test() && !lease.list && completed >= signal_value && sr::idle();
}
// change_mode() and change_scene(): the same rebuild. A scene change also
// rediscovers the scene, reallocates the scene-size copies and resumes the
// render-scale override with rebase() instead of retarget().
ModeChange rebuild(dlss_sdk::ReconstructionMode requested, bool scene) noexcept {
    ModeChange r;
    r.scene = scene;
    r.mode = requested;
    // From the player's Off: the game's own AA and scale are taken over again
    // at step 4; before it, a failure leaves the game on its own.
    const bool from_off = off.load();
    LARGE_INTEGER began{}, frequency{};
    QueryPerformanceCounter(&began);
    QueryPerformanceFrequency(&frequency);
    const auto mark = [&](unsigned phase) {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        r.phase = phase;
        r.us[phase] = frequency.QuadPart > 0
                          ? static_cast<std::uint64_t>((now.QuadPart - began.QuadPart) * 1000000 / frequency.QuadPart)
                          : 0;
    };
    // Nothing changed: admit frames again (or stay off), unless a fault
    // arrived meanwhile.
    const auto resume = [&](const char *why) {
        r.reason = why;
        if (!healthy()) {
            off.store(false);
            stop();
            return r;
        }
        if (from_off) {
            r.off = true;
            return r;
        }
        sr::begin();
        accepting.store(true);
        r.resumed = true;
        return r;
    };
    bool changed = false;
    try {
        if (!live_modes() || !pass || !spatial || reconstruction.failed()) {
            r.reason = "DLSS is not running";
            return r;
        }
        // The scene: the current one, or the game's new one (read again; the
        // device, swap chain, arena and window must be the installed ones).
        pb::Runtime found = runtime;
        if (scene) {
            if (!render_resolution::scene_changed()) {
                r.reason = "no scene change pending";
                return r;
            }
            std::string why;
            if (!pb::refresh_scene(cfg.image, runtime, found, why)) {
                r.reason = "scene: " + why;
                r.retry = true;
                return r;
            }
            if (found.window_width != runtime.window_width || found.window_height != runtime.window_height ||
                found.format != runtime.format)
                throw std::runtime_error("the window or swap chain changed");
        } else if (render_resolution::scene_changed()) {
            r.reason = "the screen layout is changing";
            r.retry = true;
            return r;
        }
        r.scene_width = found.width;
        r.scene_height = found.height;
        const unsigned basis = found.scene_override ? found.window_width : found.width;
        const auto planned =
            render_resolution::plan({found.width, found.height}, basis, dlss_sdk::requested_scale(requested));
        if (!planned.valid) {
            if (scene) throw std::runtime_error("extent plan");
            r.reason = "extent plan";
            return r;
        }
        // As install(): a 100% plan just below the scene runs as Quality.
        auto mode = requested;
        if (mode == dlss_sdk::ReconstructionMode::dlaa &&
            (planned.render.width != found.width || planned.render.height != found.height))
            mode = dlss_sdk::ReconstructionMode::quality;
        r.mode = mode;
        r.render_width = planned.render.width;
        r.render_height = planned.render.height;
        if (!scene && !from_off && mode == scaled_geometry.mode && planned.render.width == render_width() &&
            planned.render.height == render_height()) {
            configured_mode = requested;
            r.ok = r.unchanged = true;
            return r;
        }
        // The new images must fit the video memory budget while the old ones
        // still exist (they are released after the swap); nothing changed yet.
        // A preset the budget refuses leaves the current one running; a scene
        // change stops DLSS (the game's new scene needs them).
        const bool new_copies = found.width != runtime.width || found.height != runtime.height;
        gpu_memory::Check memory;
        if (std::string why; !memory_fits(gpu_memory::dlss_bytes(found.width, found.height, planned.render.width,
                                                                 planned.render.height, new_copies),
                                          memory, why)) {
            if (scene) throw std::runtime_error(why);
            r.reason = why;
            r.memory = r.resumed = true;
            return r;
        }
        // 1 pause.
        accepting.store(false);
        sr::stop();
        mark(1);
        // 2 drain.
        if (!wait_for(drained_now, 3000)) {
            r.retry = true;
            return resume("drain timeout (is the game rendering?)");
        }
        if (!observed_zero(pending, 2000) || !observed_true(sr::callbacks_idle, 2000)) {
            r.retry = true;
            return resume("callbacks still running");
        }
        if (!drained_now()) {
            r.retry = true;
            return resume("drain changed");
        }
        mark(2);
        // 3 queue barrier.
        {
            SubmissionGuard guard;
            ComPtr<ID3D12CommandQueue> queue;
            {
                Lock lock;
                queue = last_queue;
                if (queue) r.barrier = ++signal_value;
            }
            if (queue && FAILED(queue->Signal(fence.Get(), r.barrier))) return resume("queue barrier signal");
        }
        if (r.barrier && !wait_for(
                             [&] {
                                 const auto v = fence->GetCompletedValue();
                                 return v != UINT64_MAX && v >= r.barrier;
                             },
                             2000)) {
            r.retry = true;
            return resume("queue barrier timeout");
        }
        mark(3);
        // 4 the render scale: retarget, or rebase on the new scene. From here
        // a failure stops the runtime.
        // From Off: the game's own DLSS rules again (anti-aliasing Off, CAS
        // Off, resolution scaling High) before the scale is taken. The render
        // thread applies CAS's content scale on its next update: two visits
        // must read it at 1 before the override can own a frame.
        if (from_off) {
            const auto seen = render_resolution::full_content_visits();
            game_options::take_over();
            if (!wait_for([&] { return render_resolution::full_content_visits() >= seen + 2; }, 2000)) {
                give_back_options();
                return resume("fidelityfx-cas");
            }
        }
        if (scene) {
            if (std::string why; !render_resolution::rebase({found.width, found.height}, basis,
                                                            dlss_sdk::requested_scale(requested), why))
                throw std::runtime_error("render scale: " + why);
        } else if (std::string why; from_off
                                        ? !render_resolution::resume(dlss_sdk::requested_scale(requested), why)
                                        : !render_resolution::retarget(dlss_sdk::requested_scale(requested), why)) {
            if (from_off) give_back_options();
            return resume(("render scale: " + why).c_str());
        }
        changed = true;
        // The game's AA is taken over again from its next camera frame.
        if (from_off) activation::yield(false);
        if (!wait_for([] { return render_resolution::applied_visits() >= 4; }, 3000))
            throw std::runtime_error("the game did not apply the render scale");
        mark(4);
        // 5 rebuild. Nothing records with the old feature, output, copies or
        // route effects any more, and the barrier proved their GPU work done.
        auto *device = reinterpret_cast<ID3D12Device *>(cfg.device);
        const auto *initialization = ngx_startup::borrowed_initialization();
        if (!initialization) throw std::runtime_error("NGX startup ownership");
        {
            Lock lock;
            if (spatial->recorded() && !spatial->retired()) spatial->retire(fence.Get(), r.barrier, true);
            pass->retire(fence.Get(), r.barrier);
        }
        // The private copies of the game's inputs are scene-sized.
        std::array<ComPtr<ID3D12Resource>, 3> copies{raw[0], raw[1], raw[2]};
        if (new_copies) {
            constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_R32_UINT};
            for (unsigned i = 0; i < 3; ++i) {
                D3D12_RESOURCE_DESC desc{};
                desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                desc.Width = found.width;
                desc.Height = found.height;
                desc.DepthOrArraySize = desc.MipLevels = 1;
                desc.Format = formats[i];
                desc.SampleDesc.Count = 1;
                D3D12_HEAP_PROPERTIES heap{};
                heap.Type = D3D12_HEAP_TYPE_DEFAULT;
                copies[i].Reset();
                if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                           D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                           IID_PPV_ARGS(&copies[i]))))
                    throw std::runtime_error("continuous raw allocation");
            }
        }
        const dlss_sdk::Geometry geometry{{found.width, found.height},
                                          {planned.render.width, planned.render.height},
                                          {found.width, found.height},
                                          mode};
        auto fresh = std::make_unique<dlss_sdk::InlineFrame>(
            device, geometry, std::array<ID3D12Resource *, 3>{copies[0].Get(), copies[1].Get(), copies[2].Get()},
            sdk_runtime, *initialization, gpu_memory::room({memory.budget, memory.usage}));
        auto fresh_spatial = std::make_unique<temporal_present::Output>(
            device,
            temporal_present::SpatialSource{found.width, found.height, planned.render.width, planned.render.height},
            found.width, found.height, true, true);
        if (!sr::resize(planned.render.width, planned.render.height, found.width, found.height, r.reason))
            throw std::runtime_error(r.reason);
        {
            Lock lock;
            pass = std::move(fresh);
            spatial = std::move(fresh_spatial);
            raw = copies;
            runtime = found;
            scaled_render = {planned.render.width, planned.render.height};
            edge_column =
                static_cast<float>(runtime.width) * planned.quantized > static_cast<float>(planned.render.width);
            edge_row =
                static_cast<float>(runtime.height) * planned.quantized > static_cast<float>(planned.render.height);
            scaled_geometry = geometry;
            configured_mode = requested;
            // Published requests, depths and projections belong to frames at
            // the old extent; none is consumed now. The next frame resets.
            for (auto &x : requests) x = {};
            for (auto &x : depths) x = {};
            for (auto &x : projections) x = {};
            previous = {};
            history_depth.Reset();
        }
        mark(5);
        // 6 resume.
        off.store(false);
        sr::begin();
        accepting.store(true);
        mark(6);
        r.ok = true;
        return r;
    } catch (const std::exception &e) {
        r.reason = e.what();
    } catch (...) {
        r.reason = "rebuild";
    }
    if (!changed && !scene && !fault.load()) {
        const std::string why = r.reason.empty() ? "mode change" : r.reason;
        return resume(why.c_str());
    }
    // The runtime stops: a fault, not the player's Off.
    r.retry = false;
    off.store(false);
    fail(scene ? 48 : 47);
    stop();
    return r;
}
} // namespace
} // namespace mhw::continuous_dlaa::detail

namespace mhw::continuous_dlaa {
using namespace detail;
void request_mode(dlss_sdk::ReconstructionMode m) noexcept {
    mode_request.store(static_cast<int>(m));
}
void request_off() noexcept {
    mode_request.store(off_request);
}
bool requested_mode(ModeRequest &r) noexcept {
    const int v = mode_request.exchange(-1);
    if (v < 0) return false;
    r.off = v == off_request;
    r.mode = r.off ? configured_mode_value() : static_cast<dlss_sdk::ReconstructionMode>(v);
    return true;
}
bool live_modes() noexcept {
    return live_installed.load() && healthy();
}
// From Off after the game's aspect switch: the scene rebuild, at the new mode.
ModeChange change_mode(dlss_sdk::ReconstructionMode requested) noexcept {
    return rebuild(requested, off.load() && render_resolution::scene_changed());
}
ModeChange change_scene() noexcept {
    // While off the switch waits for change_mode() (the game renders on its own).
    if (off.load()) {
        ModeChange r;
        r.off = true;
        r.reason = "DLSS is off";
        return r;
    }
    return rebuild(configured_mode, true);
}
ModeChange turn_off() noexcept {
    ModeChange r;
    r.off = true;
    r.mode = configured_mode;
    LARGE_INTEGER began{}, frequency{};
    QueryPerformanceCounter(&began);
    QueryPerformanceFrequency(&frequency);
    const auto mark = [&](unsigned phase) {
        LARGE_INTEGER now{};
        QueryPerformanceCounter(&now);
        r.phase = phase;
        r.us[phase] = frequency.QuadPart > 0
                          ? static_cast<std::uint64_t>((now.QuadPart - began.QuadPart) * 1000000 / frequency.QuadPart)
                          : 0;
    };
    try {
        if (off.load()) {
            r.ok = r.unchanged = true;
            return r;
        }
        r.off = false;
        if (!live_modes() || !pass || !spatial || reconstruction.failed()) {
            r.reason = "DLSS is not running";
            return r;
        }
        if (render_resolution::scene_changed()) {
            r.reason = "the screen layout is changing";
            r.retry = true;
            return r;
        }
        // 1 pause, 2 drain: as the live rebuild. Nothing is released: the
        // feature and images stay for the way back (change_mode rebuilds them).
        accepting.store(false);
        sr::stop();
        mark(1);
        const auto again = [&](const char *why) {
            r.reason = why;
            r.retry = true;
            if (!healthy()) {
                stop();
                return r;
            }
            sr::begin();
            accepting.store(true);
            r.resumed = true;
            return r;
        };
        if (!wait_for(drained_now, 3000)) return again("drain timeout (is the game rendering?)");
        if (!observed_zero(pending, 2000) || !observed_true(sr::callbacks_idle, 2000))
            return again("callbacks still running");
        if (!drained_now()) return again("drain changed");
        mark(2);
        // 3 the game's own AA and scale from its next frame.
        activation::yield(true);
        render_resolution::yield();
        give_back_options();
        off.store(true);
        r.off = r.ok = true;
        mark(3);
        return r;
    } catch (const std::exception &e) {
        r.reason = e.what();
    } catch (...) {
        r.reason = "turn off";
    }
    r.off = false;
    fail(49);
    stop();
    return r;
}
bool dlss_off() noexcept {
    return off.load();
}
void start_off(bool o) noexcept {
    if (!pass) off_at_start = o;
}
dlss_sdk::ReconstructionMode configured_mode_value() noexcept {
    return configured_mode;
}
bool diagnostic_pause(bool pause) noexcept {
    if (!live_modes() || off.load()) return false;
    if (pause) {
        accepting.store(false);
        sr::stop();
        return true;
    }
    if (!healthy()) return false;
    sr::begin();
    accepting.store(true);
    return true;
}
void reconstruction_mode(dlss_sdk::ReconstructionMode m) noexcept {
    if (!pass) configured_mode = m;
}
// Worker: the mode DLSS runs (after install; it may differ from the configured
// one, see install) or will run.
dlss_sdk::ReconstructionMode reconstruction_mode() noexcept {
    return pass ? scaled_geometry.mode : configured_mode;
}
bool scene_extent(unsigned &scene_width, unsigned &scene_height, unsigned &window_width,
                  unsigned &window_height) noexcept {
    if (!pass) return false;
    scene_width = runtime.width;
    scene_height = runtime.height;
    window_width = runtime.window_width;
    window_height = runtime.window_height;
    return true;
}
bool render_extent(unsigned &width, unsigned &height) noexcept {
    if (!pass) return false;
    width = render_width();
    height = render_height();
    return true;
}
// The worker's thread, as the rebuild's: `pass`, `runtime` and the render
// size change only there.
float texture_scale() noexcept {
    if (!pass || off.load() || !live_modes() || !runtime.width) return 0.F;
    return static_cast<float>(render_width()) / static_cast<float>(runtime.width);
}
} // namespace mhw::continuous_dlaa
