#include "game_session.hpp"
#include "camera_constants.hpp"
#include "input_ring.hpp"
#include "hudless_pass.hpp"
#include "../temporal_present/bindings.hpp"
#include "../temporal_present/observer.hpp"
#include "../reflex/frame_generation_policy.hpp"
#include "../reflex/streamline.hpp"
#include <wrl/client.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <ostream>

namespace mhw::frame_generation::game {
namespace {
using Microsoft::WRL::ComPtr;
constexpr unsigned slot_count = 3, none = InputRing<slot_count>::none;
constexpr auto tag_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;  // DLSS converter output state
const sl::ViewportHandle viewport{0};

enum class Off : unsigned { not_armed, no_token, no_ready, camera, constants, tags, options, count };
constexpr std::array<const char *, static_cast<unsigned>(Off::count)> off_names{
    "not_armed", "no_token", "no_ready", "camera", "constants", "tags", "options"};

struct Images {
    ComPtr<ID3D12Resource> depth, motion, hudless;
};
// Graphics root parameter of the screen write's PS CBSystem (b3): parameter 5
// of its root signature (RenderDoc, EID 36144 of the HUD capture). The screen
// write is the last draw of its list, so the binding is still current when the
// list closes; the capture pass validates the contents on the GPU.
constexpr unsigned system_parameter = 5;
// Lists closed between a HUD-less record and the next Present that may record
// a capture (the screen write's list is one of them).
constexpr unsigned capture_limit = 4;
enum class HudSkip : unsigned { disabled, no_restore, failed, count };
constexpr std::array<const char *, static_cast<unsigned>(HudSkip::count)> hud_skip_names{"disabled", "no_restore", "failed"};
struct Staged {
    bool valid{};
    std::uint64_t frame{}, epoch{};
    ComPtr<ID3D12Resource> depth, motion;
    SlotCamera camera;
};
struct Last {
    bool valid{};
    std::uint64_t frame{}, epoch{};
    SlotCamera camera;
};
struct Sample {
    std::uint64_t present_frame{}, slot_frame{};
    CameraConstants constants;
};

struct Session {
    std::mutex mutex;
    reflex::Streamline *sdk{};
    unsigned output_width{}, output_height{};
    SceneRect scene;  // In the back buffer; the HUD-less image is scene-sized
    // Viewport (x, y, w, h) and scissor (l, t, r, b) of each capture candidate
    // in the latest frame that had captures; the screen write is one of them.
    std::array<std::array<int, 8>, capture_limit> candidates{};
    unsigned candidate_count{};
    std::atomic<bool> armed{}, on{};
    InputRing<slot_count> ring;
    std::array<Images, slot_count> images;
    std::array<bool, slot_count> slot_hudless{};
    Staged staged;
    std::unique_ptr<HudlessPass> hudless;
    bool hudless_failed{};
    std::array<char, 160> hudless_error{};  // First HUD-less failure, JSON-safe
    std::atomic<bool> capture_window{};  // A HUD-less pass was recorded since the last Present
    unsigned captures_this_frame{};
    std::uint64_t captures{}, capture_no_cbv{}, capture_limited{}, capture_failed{};
    std::uint64_t staged_count{}, staged_dropped{}, final_unstaged{}, final_mismatch{}, hudless_recorded{}, hudless_tagged{},
        probe_ok{}, probe_fallback{}, probe_invalid{};
    std::array<std::uint64_t, static_cast<unsigned>(HudSkip::count)> hud_skips{};
    HudlessPass::Probe last_probe{};
    std::array<SlotCamera, slot_count> cameras;
    D3D12_RESOURCE_DESC depth_desc{}, motion_desc{};
    bool created{}, create_failed{};
    unsigned pending{none};
    Last last;
    ID3D12Fence *fence{};  // Owned by the SDK; only GetCompletedValue is called.
    sl::DLSSGState state{};
    bool state_valid{};
    std::uint64_t presents{}, tagged{}, recorded{}, record_failed{}, size_mismatch{}, state_failed{}, mode_changes{},
        null_tags{}, unpresented{}, exceptions{}, lifecycle_calls{}, lifecycle_offs{};
    std::atomic<unsigned> requested_multiplier{2};
    unsigned applied_generated{};  // 0 while Off
    std::uint64_t generated_changes{}, generated_limited{};
    std::array<std::uint64_t, 8> presented_frames{};  // numFramesActuallyPresented per Present: 0..6, other
    std::array<std::uint64_t, static_cast<unsigned>(Off::count)> off{};
    std::array<std::uint64_t, 7> camera_failures{};
    std::array<std::uint64_t, 4> present_delta{};  // present frame - slot frame: 1, 2, 3, other
    std::array<Sample, 8> samples{};
    unsigned sample_count{};
    sl::Result last_sdk_error{sl::Result::eOk};
};
Session session;

sl::float4x4 matrix(const std::array<float, 16> &m) {
    sl::float4x4 r;
    for (unsigned y = 0; y < 4; ++y) r[y] = {m[y * 4], m[y * 4 + 1], m[y * 4 + 2], m[y * 4 + 3]};
    return r;
}

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

bool create(Session &s, ID3D12Resource *depth, ID3D12Resource *motion, ID3D12Resource *scene) {
    ComPtr<ID3D12Device> device;
    if (FAILED(depth->GetDevice(IID_PPV_ARGS(&device)))) return false;
    s.depth_desc = depth->GetDesc();
    s.motion_desc = motion->GetDesc();
    const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
    // HUD-less is optional: any failure here leaves depth/motion-only FG.
    const auto scene_desc = scene->GetDesc();
    try {
        // The HUD-less image covers exactly the scene rectangle.
        if (scene_desc.Width != s.scene.width || scene_desc.Height != s.scene.height)
            throw std::runtime_error("hudless scene extent");
        s.hudless = std::make_unique<HudlessPass>(device.Get(), static_cast<unsigned>(scene_desc.Width), scene_desc.Height);
        D3D12_RESOURCE_DESC h{};
        h.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        h.Width = scene_desc.Width;
        h.Height = scene_desc.Height;
        h.DepthOrArraySize = h.MipLevels = 1;
        h.SampleDesc.Count = 1;
        h.Format = HudlessPass::format;
        h.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        for (auto &i : s.images) {
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &h, tag_state, nullptr, IID_PPV_ARGS(&i.hudless))))
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
           a.MipLevels == b.MipLevels && a.DepthOrArraySize == b.DepthOrArraySize && a.SampleDesc.Count == b.SampleDesc.Count;
}

// Owner thread, under the session lock. Null tags release SDK references.
void turn_off(Session &s, const sl::FrameToken *token, Off reason) {
    ++s.off[static_cast<unsigned>(reason)];
    if (!s.on.load()) return;
    if (token) {
        sl::Extent extent{0, 0, s.output_width, s.output_height};
        sl::ResourceTag tags[]{
            {nullptr, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &extent},
            {nullptr, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &extent},
            {nullptr, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &extent}};
        if (s.sdk->tags(*token, viewport, tags, 3) == sl::Result::eOk) ++s.null_tags;
    }
    const auto result = s.sdk->frame_generation_off(viewport, true);
    if (result != sl::Result::eOk) s.last_sdk_error = result;
    s.on.store(false);
    s.applied_generated = 0;
    ++s.mode_changes;
}

// Owner thread, under the session lock: generated frames for the requested
// multiplier. The capability comes from the last state read on this thread.
unsigned wanted_generated(Session &s) {
    if (!s.state_valid) {
        sl::DLSSGState state{};
        if (s.sdk->frame_generation_state(viewport, state) == sl::Result::eOk) {
            s.state = state;
            s.state_valid = true;
        }
    }
    const auto multiplier = s.requested_multiplier.load();
    const auto maximum = s.state_valid ? s.state.numFramesToGenerateMax : 0;
    auto generated = reflex::generated_frames_for(multiplier, maximum);
    if (!generated) generated = 1;
    if (generated + 1 != multiplier) ++s.generated_limited;
    return generated;
}

}  // namespace

void configure(reflex::Streamline &sdk, unsigned output_width, unsigned output_height, SceneRect scene) noexcept {
    std::lock_guard lock(session.mutex);
    session.sdk = &sdk;
    session.output_width = output_width;
    session.output_height = output_height;
    session.scene = scene;
}

void arm(bool armed) noexcept { session.armed.store(armed); }
bool frame_generation_on() noexcept { return session.on.load(); }
void multiplier(unsigned total) noexcept {
    if (reflex::generated_frames_for(total, 0)) session.requested_multiplier.store(total);
}

void stage(std::uint64_t frame, std::uint64_t epoch, ID3D12Resource *depth, ID3D12Resource *motion,
           const SlotCamera &camera) noexcept {
    auto &s = session;
    if (!s.armed.load()) return;
    try {
        std::lock_guard lock(s.mutex);
        if (s.staged.valid) ++s.staged_dropped;  // previous frame's final draw never qualified
        s.staged = {};
        if (!depth || !motion) {
            ++s.record_failed;
            return;
        }
        s.staged = {true, frame, epoch, depth, motion, camera};
        ++s.staged_count;
    } catch (...) {
        ++session.record_failed;
    }
}

void record(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots, ID3D12Resource *scene,
            std::uint64_t frame, std::uint64_t epoch) noexcept {
    auto &s = session;
    if (!s.armed.load()) return;
    try {
        std::lock_guard lock(s.mutex);
        if (!s.sdk || s.create_failed) return;
        if (!s.staged.valid) {
            ++s.final_unstaged;
            return;
        }
        const Staged staged = std::move(s.staged);
        s.staged = {};
        if (staged.frame != frame || staged.epoch != epoch) {
            ++s.final_mismatch;
            return;
        }
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
        const auto slot = s.ring.acquire(frame, epoch, completed);
        if (slot == none) return;
        // The slot's previous pass is complete (fence-gated): collect its probe.
        if (s.slot_hudless[slot] && s.hudless) {
            const auto probe = s.hudless->probe(slot);
            if (!probe.valid)
                ++s.probe_invalid;
            else {
                ++(probe.parameters_ok ? s.probe_ok : s.probe_fallback);
                s.last_probe = probe;
            }
        }
        s.slot_hudless[slot] = false;
        auto &image = s.images[slot];
        std::array<D3D12_RESOURCE_BARRIER, 4> b{transition(image.depth.Get(), tag_state, D3D12_RESOURCE_STATE_COPY_DEST),
                                               transition(image.motion.Get(), tag_state, D3D12_RESOURCE_STATE_COPY_DEST),
                                               transition(depth, tag_state, D3D12_RESOURCE_STATE_COPY_SOURCE),
                                               transition(motion, tag_state, D3D12_RESOURCE_STATE_COPY_SOURCE)};
        list->ResourceBarrier(4, b.data());
        list->CopyResource(image.depth.Get(), depth);
        list->CopyResource(image.motion.Get(), motion);
        for (auto &v : b) std::swap(v.Transition.StateBefore, v.Transition.StateAfter);
        list->ResourceBarrier(4, b.data());

        // HUD-less colour from the HUD-free final scene; the brightness comes
        // from the last screen write's CBSystem (closing()).
        if (!s.hudless)
            ++s.hud_skips[static_cast<unsigned>(HudSkip::disabled)];
        else {
            std::unique_ptr<temporal_present::RestoreBindings> restore;
            try {
                restore = std::make_unique<temporal_present::RestoreBindings>(list, roots);
            } catch (...) {
                ++s.hud_skips[static_cast<unsigned>(HudSkip::no_restore)];
            }
            if (restore) {
                try {
                    s.hudless->record(list, slot, scene, image.hudless.Get(), tag_state);
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
            }
        }
        s.cameras[slot] = staged.camera;
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

bool capture_wanted() noexcept { return session.armed.load() && session.capture_window.load(); }

void closing(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots) noexcept {
    auto &s = session;
    if (!s.armed.load() || !list) return;
    try {
        std::lock_guard lock(s.mutex);
        if (!s.hudless || !s.capture_window.load()) return;
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
                c = {static_cast<int>(raster.viewport.TopLeftX), static_cast<int>(raster.viewport.TopLeftY),
                     static_cast<int>(raster.viewport.Width), static_cast<int>(raster.viewport.Height), 0, 0, 0, 0};
            if (raster.scissor_known)
                c[4] = raster.scissor.left, c[5] = raster.scissor.top, c[6] = raster.scissor.right, c[7] = raster.scissor.bottom;
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

static void finish_pending(Session &s, bool presented);

bool lifecycle_guard(unsigned) noexcept {
    auto &s = session;
    try {
        std::lock_guard lock(s.mutex);
        ++s.lifecycle_calls;
        if (!s.sdk || !s.on.load()) return false;
        // No frame token here: depth/motion/HUD-less stay tagged until the next
        // Present (eValidUntilPresent) and are re-tagged or nulled there.
        const auto result = s.sdk->frame_generation_off(viewport, true);
        if (result != sl::Result::eOk) s.last_sdk_error = result;
        s.on.store(false);
        s.applied_generated = 0;
        ++s.mode_changes;
        ++s.lifecycle_offs;
        return true;
    } catch (...) {
        ++session.exceptions;
        return false;
    }
}

void present_begin(const sl::FrameToken *token, std::uint64_t present_frame) noexcept {
    auto &s = session;
    try {
        std::lock_guard lock(s.mutex);
        ++s.presents;
        s.capture_window.store(false);
        s.captures_this_frame = 0;
        if (!s.sdk) return;
        // A missed present_end must not leak a tagged slot.
        if (s.pending != none) finish_pending(s, false);
        if (!s.armed.load()) return turn_off(s, token, Off::not_armed);
        if (!token) return turn_off(s, token, Off::no_token);
        const auto slot = s.ring.take(present_frame, static_cast<std::uint32_t>(*token));
        if (slot == none) return turn_off(s, token, Off::no_ready);
        const auto &entry = s.ring.slot(slot);
        const auto delta = present_frame - entry.frame;
        ++s.present_delta[delta >= 1 && delta <= 3 ? delta - 1 : 3];

        const auto &camera = s.cameras[slot];
        CameraFrame frame;
        frame.view = camera.view;
        frame.projection = camera.projection;
        frame.jitter_x = camera.jitter_x;
        frame.jitter_y = camera.jitter_y;
        frame.render_width = static_cast<unsigned>(s.depth_desc.Width);
        frame.render_height = s.depth_desc.Height;
        // Previous camera = the previously tagged frame, only if consecutive.
        frame.has_previous = s.last.valid && s.last.frame + 1 == entry.frame && s.last.epoch + 1 == entry.epoch;
        frame.reset = camera.reset || !frame.has_previous;
        if (frame.has_previous) {
            frame.previous_view = s.last.camera.view;
            frame.previous_projection = s.last.camera.projection;
        }
        CameraConstants c;
        const auto failure = derive_camera_constants(frame, c);
        if (failure != CameraFailure::none) {
            ++s.camera_failures[static_cast<unsigned>(failure)];
            s.ring.untag(slot);
            s.last = {};
            return turn_off(s, token, Off::camera);
        }
        sl::Constants k{};
        k.cameraViewToClip = matrix(c.view_to_clip);
        k.clipToCameraView = matrix(c.clip_to_view);
        k.clipToPrevClip = matrix(c.clip_to_prev_clip);
        k.prevClipToClip = matrix(c.prev_clip_to_clip);
        k.clipToLensClip = matrix({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
        k.jitterOffset = {c.jitter[0], c.jitter[1]};
        k.mvecScale = {c.mvec_scale[0], c.mvec_scale[1]};
        k.cameraPinholeOffset = {0, 0};
        k.cameraPos = {c.position[0], c.position[1], c.position[2]};
        k.cameraUp = {c.up[0], c.up[1], c.up[2]};
        k.cameraRight = {c.right[0], c.right[1], c.right[2]};
        k.cameraFwd = {c.forward[0], c.forward[1], c.forward[2]};
        k.cameraNear = c.near_plane;
        k.cameraFar = c.far_plane;
        k.cameraFOV = c.fov;
        k.cameraAspectRatio = c.aspect;
        k.depthInverted = sl::eTrue;
        k.cameraMotionIncluded = sl::eTrue;
        k.motionVectors3D = sl::eFalse;
        k.reset = c.reset ? sl::eTrue : sl::eFalse;
        k.orthographicProjection = sl::eFalse;
        k.motionVectorsDilated = sl::eFalse;
        k.motionVectorsJittered = sl::eFalse;
        if (const auto r = s.sdk->constants(k, *token, viewport); r != sl::Result::eOk) {
            s.last_sdk_error = r;
            s.ring.untag(slot);
            return turn_off(s, token, Off::constants);
        }
        auto &image = s.images[slot];
        sl::Resource depth{sl::ResourceType::eTex2d, image.depth.Get(), static_cast<std::uint32_t>(tag_state)};
        sl::Resource motion{sl::ResourceType::eTex2d, image.motion.Get(), static_cast<std::uint32_t>(tag_state)};
        sl::Extent render{0, 0, frame.render_width, frame.render_height};
        // The back buffer's scene rectangle; the HUD-less image is that size.
        sl::Extent output{s.scene.x, s.scene.y, s.scene.width, s.scene.height}, hudless_extent{0, 0, s.scene.width, s.scene.height};
        // A slot without HUD-less clears that tag explicitly (null resource).
        const bool with_hudless = s.slot_hudless[slot] && image.hudless;
        sl::Resource hudless{sl::ResourceType::eTex2d, with_hudless ? image.hudless.Get() : nullptr,
                             static_cast<std::uint32_t>(tag_state)};
        sl::ResourceTag tags[]{
            {&depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &render},
            {&motion, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &render},
            {nullptr, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle::eValidUntilPresent, &output},
            {with_hudless ? &hudless : nullptr, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &hudless_extent}};
        if (const auto r = s.sdk->tags(*token, viewport, tags, 4); r != sl::Result::eOk) {
            s.last_sdk_error = r;
            s.ring.untag(slot);
            return turn_off(s, token, Off::tags);
        }
        if (const auto generated = wanted_generated(s); !s.on.load() || generated != s.applied_generated) {
            if (const auto r = s.sdk->frame_generation_generate(viewport, generated, true); r != sl::Result::eOk) {
                s.last_sdk_error = r;
                // The SDK already holds the tags for this frame; keep the slot
                // until its consumption fence like any other tagged frame.
                s.pending = slot;
                ++s.off[static_cast<unsigned>(Off::options)];
                return;
            }
            if (s.on.load())
                ++s.generated_changes;
            else
                ++s.mode_changes;
            s.on.store(true);
            s.applied_generated = generated;
        }
        s.pending = slot;
        s.last = {true, entry.frame, entry.epoch, camera};
        ++s.tagged;
        if (with_hudless) ++s.hudless_tagged;
        if (s.sample_count < s.samples.size() && (s.tagged <= 4 || (s.tagged & (s.tagged - 1)) == 0))
            s.samples[s.sample_count++] = {present_frame, entry.frame, c};
    } catch (...) {
        ++session.exceptions;
    }
}

void present_end(bool presented) noexcept {
    std::lock_guard lock(session.mutex);
    if (session.pending != none && session.sdk) finish_pending(session, presented);
}

static void finish_pending(Session &s, bool presented) {
    if (!presented) ++s.unpresented;
    sl::DLSSGState state{};
    if (s.sdk->frame_generation_state(viewport, state) == sl::Result::eOk) {
        s.state = state;
        s.state_valid = true;
        ++s.presented_frames[std::min<std::uint32_t>(state.numFramesActuallyPresented, 7)];
        s.fence = static_cast<ID3D12Fence *>(state.inputsProcessingCompletionFence);
        s.ring.presented(s.pending, state.lastPresentInputsProcessingCompletionFenceValue);
    } else {
        // Without a consumption fence the slot can never be proven free; keep
        // it consuming forever (fail closed) rather than risk overwriting it.
        ++s.state_failed;
        s.ring.presented(s.pending, UINT64_MAX);
    }
    s.pending = none;
}

void write(std::ostream &o) {
    auto &s = session;
    std::lock_guard lock(s.mutex);
    const auto &r = s.ring.counters();
    o << "{\"schema\":2,\"armed\":" << s.armed.load() << ",\"on\":" << s.on.load() << ",\"configured\":" << (s.sdk != nullptr)
      << ",\"multiplier\":" << s.requested_multiplier.load() << ",\"generated\":" << s.applied_generated
      << ",\"generated_changes\":" << s.generated_changes << ",\"generated_limited\":" << s.generated_limited
      << ",\"hudless\":{\"available\":" << (s.hudless != nullptr) << ",\"failed\":" << s.hudless_failed
      << ",\"recorded\":" << s.hudless_recorded << ",\"tagged\":" << s.hudless_tagged << ",\"skips\":{";
    for (unsigned i = 0; i < s.hud_skips.size(); ++i) o << (i ? "," : "") << '"' << hud_skip_names[i] << "\":" << s.hud_skips[i];
    o << "},\"probe_ok\":" << s.probe_ok << ",\"probe_fallback\":" << s.probe_fallback << ",\"probe_invalid\":" << s.probe_invalid
      << ",\"gamma_ex\":" << s.last_probe.gamma_ex << ",\"gamma\":" << s.last_probe.gamma << ",\"gpu_attempts\":"
      << s.last_probe.attempts << ",\"gpu_matches\":" << s.last_probe.matches << ",\"last_attempt\":["
      << s.last_probe.last_gamma << ',' << s.last_probe.last_gamma_ex << "],\"screen_region\":[" << s.last_probe.region[0] << ','
      << s.last_probe.region[1] << ',' << s.last_probe.region[2] << ',' << s.last_probe.region[3] << "],\"candidates\":[";
    for (unsigned i = 0; i < s.candidate_count; ++i) {
        o << (i ? ",[" : "[");
        for (unsigned j = 0; j < 8; ++j) o << (j ? "," : "") << s.candidates[i][j];
        o << ']';
    }
    o << "],\"captures\":" << s.captures
      << ",\"capture_no_cbv\":" << s.capture_no_cbv << ",\"capture_limited\":" << s.capture_limited
      << ",\"capture_failed\":" << s.capture_failed << ",\"error\":\"" << s.hudless_error.data() << "\"},\"staged\":" << s.staged_count
      << ",\"staged_dropped\":" << s.staged_dropped << ",\"final_unstaged\":" << s.final_unstaged
      << ",\"final_mismatch\":" << s.final_mismatch
      << ",\"presented_frames\":[";
    for (unsigned i = 0; i < s.presented_frames.size(); ++i) o << (i ? "," : "") << s.presented_frames[i];
    o << "]"
      << ",\"created\":" << s.created << ",\"create_failed\":" << s.create_failed << ",\"output\":[" << s.output_width << ','
      << s.output_height << "],\"scene\":[" << s.scene.x << ',' << s.scene.y << ',' << s.scene.width << ',' << s.scene.height
      << "],\"render\":[" << s.depth_desc.Width << ',' << s.depth_desc.Height << "],\"formats\":["
      << s.depth_desc.Format << ',' << s.motion_desc.Format << "],\"presents\":" << s.presents << ",\"tagged\":" << s.tagged
      << ",\"recorded\":" << s.recorded << ",\"record_failed\":" << s.record_failed << ",\"size_mismatch\":" << s.size_mismatch
      << ",\"state_failed\":" << s.state_failed << ",\"mode_changes\":" << s.mode_changes << ",\"null_tags\":" << s.null_tags
      << ",\"unpresented\":" << s.unpresented << ",\"lifecycle_calls\":" << s.lifecycle_calls << ",\"lifecycle_offs\":" << s.lifecycle_offs << ",\"exceptions\":" << s.exceptions << ",\"last_sdk_error\":" << static_cast<unsigned>(s.last_sdk_error) << ",\"off\":{";
    for (unsigned i = 0; i < s.off.size(); ++i) o << (i ? "," : "") << '"' << off_names[i] << "\":" << s.off[i];
    o << "},\"camera_failures\":[";
    for (unsigned i = 0; i < s.camera_failures.size(); ++i) o << (i ? "," : "") << s.camera_failures[i];
    o << "],\"present_delta\":[" << s.present_delta[0] << ',' << s.present_delta[1] << ',' << s.present_delta[2] << ','
      << s.present_delta[3] << "],\"ring\":{\"acquired\":" << r.acquired << ",\"starved\":" << r.starved << ",\"duplicates\":"
      << r.duplicates << ",\"abandoned\":" << r.abandoned << ",\"submitted\":" << r.submitted << ",\"discarded\":" << r.discarded
      << ",\"taken\":" << r.taken << ",\"stale\":" << r.stale << ",\"no_ready\":" << r.no_ready << ",\"consumed\":" << r.consumed
      << ",\"reclaimed\":" << r.reclaimed << "},\"state\":{\"valid\":" << s.state_valid << ",\"status\":"
      << static_cast<unsigned>(s.state.status) << ",\"min_width_or_height\":" << s.state.minWidthOrHeight
      << ",\"frames_presented\":" << s.state.numFramesActuallyPresented << ",\"max_generated\":" << s.state.numFramesToGenerateMax
      << ",\"vram\":" << s.state.estimatedVRAMUsageInBytes << ",\"fence_value\":"
      << s.state.lastPresentInputsProcessingCompletionFenceValue << "},\"samples\":[";
    for (unsigned i = 0; i < s.sample_count; ++i) {
        const auto &x = s.samples[i];
        const auto &c = x.constants;
        o << (i ? "," : "") << "{\"present\":" << x.present_frame << ",\"slot\":" << x.slot_frame << ",\"near\":" << c.near_plane
          << ",\"far\":" << c.far_plane << ",\"far_infinite\":" << c.far_infinite << ",\"fov\":" << c.fov << ",\"aspect\":"
          << c.aspect << ",\"left_handed\":" << c.left_handed << ",\"reset\":" << c.reset << ",\"jitter\":[" << c.jitter[0] << ','
          << c.jitter[1] << "],\"position\":[" << c.position[0] << ',' << c.position[1] << ',' << c.position[2]
          << "],\"forward\":[" << c.forward[0] << ',' << c.forward[1] << ',' << c.forward[2] << "],\"up\":[" << c.up[0] << ','
          << c.up[1] << ',' << c.up[2] << "],\"pinhole\":[" << c.pinhole[0] << ',' << c.pinhole[1] << "],\"clip_to_prev\":[";
        for (unsigned k = 0; k < 16; ++k) o << (k ? "," : "") << c.clip_to_prev_clip[k];
        o << "]}";
    }
    o << "]}";
}

}  // namespace mhw::frame_generation::game
