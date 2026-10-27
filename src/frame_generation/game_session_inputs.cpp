// DLSS-G's inputs: the converted depth and motion staged at the TAA
// dispatch, the HUD-less image recorded at the final screen draw, and the
// lists that carry them.
#include "game_session_state.hpp"

namespace mhw::frame_generation::game {
using namespace detail;
namespace {
// Keeps the first failure only; printable ASCII without quotes or backslashes.
void note_hudless_error(Session &s, const char *why) noexcept {
    if (s.hudless_error[0] || !why) return;
    for (std::size_t i = 0; i + 1 < s.hudless_error.size() && why[i]; ++i) {
        const char c = why[i];
        s.hudless_error[i] = c >= 0x20 && c < 0x7f && c != '"' && c != '\\' ? c : ' ';
    }
}

D3D12_RESOURCE_BARRIER transition(ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
    return b;
}

// The HUD-less image covers exactly the scene rectangle.
D3D12_RESOURCE_DESC hudless_desc(const D3D12_RESOURCE_DESC &scene, DXGI_FORMAT format = HudlessPass::format) {
    D3D12_RESOURCE_DESC h{};
    h.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    h.Width = scene.Width;
    h.Height = scene.Height;
    h.DepthOrArraySize = h.MipLevels = 1;
    h.SampleDesc.Count = 1;
    h.Format = format;
    h.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    return h;
}
// The HUD-less image's format for the back buffer's (v0.24.8): DLSS-G takes
// HUD-less colour in the back buffer's colour space. UNKNOWN: none.
DXGI_FORMAT hudless_format(unsigned back_buffer, bool hdr_supported) noexcept {
    switch (static_cast<DXGI_FORMAT>(back_buffer)) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM: return HudlessPass::format;
    case DXGI_FORMAT_R10G10B10A2_UNORM: return hdr_supported ? HudlessPass::hdr_format : DXGI_FORMAT_UNKNOWN;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

bool create(Session &s, ID3D12Resource *depth, ID3D12Resource *motion, ID3D12Resource *scene) {
    ComPtr<ID3D12Device> device;
    if (FAILED(depth->GetDevice(IID_PPV_ARGS(&device)))) return false;
    s.depth_desc = depth->GetDesc();
    s.motion_desc = motion->GetDesc();
    const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
    const auto scene_desc = scene->GetDesc();
    const auto h = hudless_desc(scene_desc);
    // v0.24.0: the depth/motion ring, the keyed staging copies (made on first
    // use) and, when made here, the HUD-less images must fit the video memory
    // budget (gpu_memory/budget.hpp); otherwise frame generation stays off.
    {
        auto d = s.depth_desc, m = s.motion_desc;
        d.Flags = m.Flags = D3D12_RESOURCE_FLAG_NONE;
        const auto bytes = [&](const D3D12_RESOURCE_DESC &x) {
            return device->GetResourceAllocationInfo(0, 1, &x).SizeInBytes;
        };
        const std::uint64_t depth_bytes = bytes(d), motion_bytes = bytes(m), hudless_bytes = bytes(h);
        if (depth_bytes == UINT64_MAX || motion_bytes == UINT64_MAX || hudless_bytes == UINT64_MAX) return false;
        const unsigned copies = slot_count + (perf::on(perf::keyed_staging) && !s.shared_staging ? staging_count : 0);
        s.memory = gpu_memory::check(copies * (depth_bytes + motion_bytes) +
                                     (!s.hudless && !s.hudless_failed ? slot_count * hudless_bytes : 0));
        if (!s.memory.passed) return false;
    }
    // HUD-less is optional: any failure here leaves depth/motion-only FG. It is
    // scene-sized, so a live preset change keeps it (release_inputs()).
    if (!s.hudless && !s.hudless_failed) try {
            if (scene_desc.Width != s.scene.width || scene_desc.Height != s.scene.height)
                throw std::runtime_error("hudless scene extent");
            s.hudless =
                std::make_unique<HudlessPass>(device.Get(), static_cast<unsigned>(scene_desc.Width), scene_desc.Height);
            // In the back buffer's format (SDR or HDR10, the same size); for any
            // other, none until it changes (record() makes a slot's image then).
            const auto wanted = hudless_format(s.back_buffer_format.load(), s.hudless->hdr_supported());
            const auto image = hudless_desc(scene_desc, wanted);
            for (auto &i : s.images) {
                if (wanted == DXGI_FORMAT_UNKNOWN) break;
                if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &image, tag_state, nullptr,
                                                           IID_PPV_ARGS(&i.hudless))))
                    throw std::runtime_error("hudless image");
                i.hudless->SetName(L"mhw-fg-hudless");
            }
        } catch (const std::exception &e) {
            note_hudless_error(s, e.what());
            s.hudless_failed = true;
        } catch (...) {
            note_hudless_error(s, "hudless creation");
            s.hudless_failed = true;
        }
    if (s.hudless_failed) {
        s.hudless.reset();
        for (auto &i : s.images) i.hudless.Reset();
    }
    for (auto &i : s.images) {
        auto d = s.depth_desc, m = s.motion_desc;
        d.Flags = m.Flags = D3D12_RESOURCE_FLAG_NONE;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, tag_state, nullptr,
                                                   IID_PPV_ARGS(&i.depth))) ||
            FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &m, tag_state, nullptr,
                                                   IID_PPV_ARGS(&i.motion))))
            return false;
        i.depth->SetName(L"mhw-fg-depth");
        i.motion->SetName(L"mhw-fg-motion");
    }
    return true;
}

bool same_image(const D3D12_RESOURCE_DESC &a, const D3D12_RESOURCE_DESC &b) {
    return a.Dimension == b.Dimension && a.Width == b.Width && a.Height == b.Height && a.Format == b.Format &&
           a.MipLevels == b.MipLevels && a.DepthOrArraySize == b.DepthOrArraySize &&
           a.SampleDesc.Count == b.SampleDesc.Count;
}

void note_mismatch(Session &s, std::uint64_t staged, std::uint64_t frame) {
    const auto d = static_cast<std::int64_t>(frame - staged);
    ++s.mismatch_delta[d == -2 ? 0 : d == -1 ? 1 : d == 1 ? 2 : d == 2 ? 3 : 4];
}
bool staging_ready(Session &s, ID3D12Resource *depth, ID3D12Resource *motion) {
    if (s.staging_failed) return false;
    if (s.staging[0].depth)
        return same_image(depth->GetDesc(), s.staging[0].depth->GetDesc()) &&
               same_image(motion->GetDesc(), s.staging[0].motion->GetDesc());
    ComPtr<ID3D12Device> device;
    if (FAILED(depth->GetDevice(IID_PPV_ARGS(&device)))) return !(s.staging_failed = true);
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto d = depth->GetDesc(), m = motion->GetDesc();
    d.Flags = m.Flags = D3D12_RESOURCE_FLAG_NONE;
    for (auto &c : s.staging)
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                   nullptr, IID_PPV_ARGS(&c.depth))) ||
            FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &m, D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                   nullptr, IID_PPV_ARGS(&c.motion)))) {
            s.staging = {};
            return !(s.staging_failed = true);
        }
    return true;
}
} // namespace

void stage(ID3D12GraphicsCommandList *list, std::uint64_t frame, std::uint64_t epoch,
           std::span<ID3D12Resource *const> depth_sets, std::span<ID3D12Resource *const> motion_sets,
           const SlotCamera &camera) noexcept {
    auto &s = session;
    if (!active(s)) return;
    try {
        std::lock_guard lock(s.mutex);
        const auto sets = depth_sets.size();
        bool complete = sets && motion_sets.size() == sets;
        for (std::size_t i = 0; complete && i < sets; ++i) complete = depth_sets[i] && motion_sets[i];
        if (!complete) {
            ++s.record_failed;
            return;
        }
        auto *depth = depth_sets[frame % sets], *motion = motion_sets[frame % sets];
        if (!perf::on(perf::keyed_staging)) {
            s.queue.clear();
            if (s.staged.valid) ++s.staged_dropped; // previous frame's final draw never qualified
            s.staged = {true, frame, epoch, depth, motion, camera};
            ++s.staged_count;
            return;
        }
        s.staged = {};
        // 0.32.0: the converter's three sets are the staging copies (the same
        // key, frame % 3, and the same reuse three frames later). A change
        // between shared and own copies comes only with release_inputs().
        const bool shared = sets == staging_count;
        if (shared != s.shared_staging && s.staging[0].depth) {
            ++s.record_failed;
            return;
        }
        if (shared) {
            s.shared_staging = true;
            for (unsigned i = 0; i < staging_count; ++i)
                if (s.staging[i].depth.Get() != depth_sets[i] || s.staging[i].motion.Get() != motion_sets[i])
                    s.staging[i] = {depth_sets[i], motion_sets[i]};
        } else if (!list || !staging_ready(s, depth, motion)) {
            ++s.record_failed;
            return;
        }
        // A final draw recorded before this stage holds an unconfirmed slot for
        // the frame (its copy runs after this one on the GPU): confirm it.
        // Otherwise queue the frame for its final draw.
        auto &copy = s.staging[s.queue.push(frame, epoch, camera)];
        if (!shared) {
            std::array<D3D12_RESOURCE_BARRIER, 2> b{
                transition(copy.depth.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
                transition(copy.motion.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST)};
            list->ResourceBarrier(2, b.data());
            list->CopyResource(copy.depth.Get(), depth);
            list->CopyResource(copy.motion.Get(), motion);
            for (auto &v : b) std::swap(v.Transition.StateBefore, v.Transition.StateAfter);
            list->ResourceBarrier(2, b.data());
            ++s.staging_copies;
        }
        ++s.staged_count;
        if (const auto slot = s.ring.confirm(frame, epoch); slot != none) {
            s.cameras[slot] = camera;
            s.queue.erase(frame, epoch);
            ++s.keyed_confirms;
        }
    } catch (...) {
        ++session.record_failed;
    }
}

void record(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots, ID3D12Resource *scene,
            std::uint64_t frame, std::uint64_t epoch, D3D12_RESOURCE_STATES scene_state) noexcept {
    auto &s = session;
    if (!active(s)) return;
    try {
        std::lock_guard lock(s.mutex);
        if (!s.sdk || s.create_failed) return;
        const bool keyed = perf::on(perf::keyed_staging);
        Staged staged;
        bool confirmed = true;
        if (keyed) {
            const auto newest = s.queue.newest();
            const auto taken = s.queue.take(frame, epoch);
            if (!s.staging[0].depth) { // no frame staged yet
                ++s.final_unstaged;
                return;
            }
            // GPU order: this frame's DLSS list writes its staging copy before
            // this final draw (which samples the DLSS output) reads it.
            const auto &copy = s.staging[frame % staging_count];
            if (taken) {
                staged = {true, frame, epoch, copy.depth, copy.motion, taken->entry.payload};
                ++s.keyed_hits;
            } else {
                // Recorded before the frame's DLSS stage (heavy scenes): the slot
                // waits for stage() to confirm it and supply the camera.
                staged = {true, frame, epoch, copy.depth, copy.motion, {}};
                confirmed = false;
                ++s.keyed_early;
                if (newest) note_mismatch(s, *newest, frame);
            }
        } else {
            if (!s.staged.valid) {
                ++s.final_unstaged;
                return;
            }
            staged = std::move(s.staged);
            s.staged = {};
            if (staged.frame != frame || staged.epoch != epoch) {
                ++s.final_mismatch;
                note_mismatch(s, staged.frame, frame);
                return;
            }
        }
        // The sources are copied from as they are: own staging copies stay in
        // COPY_SOURCE, the converter's images are in staged_state.
        const auto generation = roots.generation;
        auto *depth = staged.depth.Get(), *motion = staged.motion.Get();
        if (!list || !scene || !generation) {
            ++s.record_failed;
            return;
        }
        if (!s.created) {
            s.created = create(s, depth, motion, scene);
            if (!s.created) {
                s.create_failed = true;
                s.hudless.reset();
                for (auto &i : s.images) i = {};
                return;
            }
        }
        if (!same_image(depth->GetDesc(), s.depth_desc) || !same_image(motion->GetDesc(), s.motion_desc)) {
            ++s.size_mismatch;
            return;
        }
        const auto completed = s.fence ? s.fence->GetCompletedValue() : 0;
        const auto slot = s.ring.acquire(frame, epoch, completed, confirmed);
        if (slot == none) return;
        // The slot's previous pass is complete (fence-gated): collect its probe.
        if (s.slot_hudless[slot] && s.hudless) {
            const auto probe = s.hudless->probe(slot);
            if (!probe.valid)
                ++s.probe_invalid;
            else {
                if (probe.hdr)
                    ++(probe.hdr_ok ? s.hdr_probe_ok : s.hdr_probe_default);
                else
                    ++(probe.parameters_ok ? s.probe_ok : s.probe_fallback);
                s.last_probe = probe;
            }
        }
        s.slot_hudless[slot] = false;
        auto &image = s.images[slot];
        std::array<D3D12_RESOURCE_BARRIER, 2> b{
            transition(image.depth.Get(), tag_state, D3D12_RESOURCE_STATE_COPY_DEST),
            transition(image.motion.Get(), tag_state, D3D12_RESOURCE_STATE_COPY_DEST)};
        list->ResourceBarrier(2, b.data());
        list->CopyResource(image.depth.Get(), depth);
        list->CopyResource(image.motion.Get(), motion);
        for (auto &v : b) std::swap(v.Transition.StateBefore, v.Transition.StateAfter);
        list->ResourceBarrier(2, b.data());

        // HUD-less colour from the HUD-free final scene; the brightness comes
        // from the last screen write's CBSystem (closing()).
        if (!s.hudless)
            ++s.hud_skips[static_cast<unsigned>(HudSkip::disabled)];
        else {
            std::unique_ptr<temporal_present::RestoreBindings> restore;
            try {
                // A worker's final-scene list may never set a compute root
                // signature (game-44..46: no_restore rising since v0.12.6),
                // nor any root signature or pipeline state (0.32.0: what it
                // never set stays unset).
                restore = std::make_unique<temporal_present::RestoreBindings>(list, roots, true, true);
                if (!roots.compute.signature) ++s.restore_reasons[5];
                if (!roots.pipeline) ++s.restore_reasons[6];
            } catch (...) {
                ++s.hud_skips[static_cast<unsigned>(HudSkip::no_restore)];
                const bool reasons[]{!roots.known, !roots.valid, !roots.generation, !roots.pipeline,
                                     !roots.graphics.signature};
                for (unsigned i = 0; i < 5; ++i)
                    if (reasons[i]) ++s.restore_reasons[i];
            }
            // v0.24.8: the slot's image follows the back buffer's format (the
            // game switches SDR/HDR10 with its own resizes). The slot is free
            // (fence-gated), so its image can be replaced here.
            const auto wanted = hudless_format(s.back_buffer_format.load(), s.hudless->hdr_supported());
            if (wanted == DXGI_FORMAT_UNKNOWN) {
                ++s.hud_skips[static_cast<unsigned>(HudSkip::format)];
                restore.reset();
            } else if (restore && (!image.hudless || image.hudless->GetDesc().Format != wanted)) {
                ComPtr<ID3D12Device> device;
                const auto desc = hudless_desc(scene->GetDesc(), wanted);
                const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
                image.hudless.Reset();
                if (SUCCEEDED(scene->GetDevice(IID_PPV_ARGS(&device))) &&
                    SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, tag_state, nullptr,
                                                              IID_PPV_ARGS(&image.hudless)))) {
                    image.hudless->SetName(L"mhw-fg-hudless");
                    ++s.hudless_recreated;
                } else {
                    image.hudless.Reset();
                    note_hudless_error(s, "hudless image for a new back buffer format");
                    ++s.hud_skips[static_cast<unsigned>(HudSkip::failed)];
                    restore.reset();
                }
            }
            if (restore) {
                try {
                    s.hudless->record(list, slot, scene, image.hudless.Get(), tag_state, scene_state);
                    s.slot_hudless[slot] = true;
                    ++s.hudless_recorded;
                    s.capture_window.store(true);
                } catch (const std::exception &e) {
                    note_hudless_error(s, e.what());
                    ++s.hud_skips[static_cast<unsigned>(HudSkip::failed)];
                } catch (...) {
                    ++s.hud_skips[static_cast<unsigned>(HudSkip::failed)];
                }
                restore->apply();
                // The pass's pipeline state stays on a fresh list; its mirror
                // is unset again, as the game left it.
                if (!roots.pipeline) temporal_present::observer::unset_injected_pipeline(list, roots);
            }
        }
        if (confirmed) s.cameras[slot] = staged.camera;
        if (s.ring.recorded(slot, reinterpret_cast<std::uintptr_t>(list), generation))
            ++s.recorded;
        else {
            s.ring.abandon(slot);
            ++s.record_failed;
        }
    } catch (...) {
        ++session.record_failed;
    }
}

bool capture_wanted() noexcept {
    return active(session) && session.capture_window.load();
}

void closing(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots) noexcept {
    auto &s = session;
    if (!active(s) || !list) return;
    try {
        std::lock_guard lock(s.mutex);
        if (!s.hudless || !s.capture_window.load()) return;
        // v0.24.8: with an HDR10 back buffer, every CBV root argument of the
        // closing list is offered to the CBRenderFrame capture, which keeps
        // only a layout-checked one (and its root parameter); at most
        // capture_limit lists a frame, as for the SDR capture.
        if (roots.known && roots.valid && s.hdr_lists_this_frame < capture_limit &&
            hudless_format(s.back_buffer_format.load(), s.hudless->hdr_supported()) == HudlessPass::hdr_format) {
            unsigned offered{};
            for (unsigned i = 0; i < roots.graphics.arguments.size() && offered < 16; ++i) {
                const auto &a = roots.graphics.arguments[i];
                if (a.kind != temporal_present::ArgumentKind::cbv || !a.address) continue;
                try {
                    s.hudless->capture_hdr(list, a.address, i);
                    ++s.hdr_captures;
                    ++offered;
                } catch (const std::exception &e) {
                    note_hudless_error(s, e.what());
                    ++s.capture_failed;
                    break;
                }
            }
            if (offered) ++s.hdr_lists_this_frame;
        }
        const auto &system = roots.graphics.arguments[system_parameter];
        if (!roots.known || !roots.valid || system.kind != temporal_present::ArgumentKind::cbv || !system.address) {
            ++s.capture_no_cbv;
            return;
        }
        if (s.captures_this_frame >= capture_limit) {
            ++s.capture_limited;
            return;
        }
        temporal_present::observer::RasterBindings raster;
        auto &c = s.candidates[s.captures_this_frame];
        c = {};
        if (temporal_present::observer::raster_current(list, raster)) {
            if (raster.viewport_known)
                c = {static_cast<int>(raster.viewport.TopLeftX),
                     static_cast<int>(raster.viewport.TopLeftY),
                     static_cast<int>(raster.viewport.Width),
                     static_cast<int>(raster.viewport.Height),
                     0,
                     0,
                     0,
                     0};
            if (raster.scissor_known)
                c[4] = raster.scissor.left, c[5] = raster.scissor.top, c[6] = raster.scissor.right,
                c[7] = raster.scissor.bottom;
        }
        s.candidate_count = s.captures_this_frame + 1;
        try {
            s.hudless->capture(list, system.address);
            ++s.captures_this_frame;
            ++s.captures;
        } catch (const std::exception &e) {
            note_hudless_error(s, e.what());
            ++s.capture_failed;
        }
    } catch (...) {
        ++s.capture_failed;
    }
}

void submitted(std::uint64_t list, std::uint64_t generation, std::uint64_t batch) noexcept {
    std::lock_guard lock(session.mutex);
    session.ring.submitted(list, generation, batch);
}

void reset(std::uint64_t list, std::uint64_t generation) noexcept {
    std::lock_guard lock(session.mutex);
    session.ring.reset(list, generation);
}

} // namespace mhw::frame_generation::game
