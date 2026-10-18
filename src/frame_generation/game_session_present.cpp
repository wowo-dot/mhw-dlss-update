// Each Present: the requests take effect, the tags and options for the
// frame, the wind-down before an Off, and window changes.
#include "game_session_state.hpp"

namespace mhw::frame_generation::game {
using namespace detail;
namespace {
sl::float4x4 matrix(const std::array<float, 16> &m) {
    sl::float4x4 r;
    for (unsigned y = 0; y < 4; ++y) r[y] = {m[y * 4], m[y * 4 + 1], m[y * 4 + 2], m[y * 4 + 3]};
    return r;
}

// Owner thread, under the session lock: DLSS-G Off, and the session's applied
// and shown state with it.
void switch_off(Session &s) {
    const auto result = s.sdk->frame_generation_off(viewport, true);
    if (result != sl::Result::eOk) s.last_sdk_error = result;
    s.on.store(false);
    s.applied_generated = 0;
    s.applied_mode = sl::DLSSGMode::eOff;
    s.applied_target = 0;
    s.shown_generated.store(0);
    s.shown_dynamic.store(false);
    s.single_streak.store(0);
    ++s.mode_changes;
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
    switch_off(s);
}

// Owner thread, under the session lock, after a presented tagged Present's
// state read: the pacing counters (Session). Untagged Presents in between
// break the sequence; counting starts again at the next pair.
void note_pacing(Session &s, unsigned count) {
    LARGE_INTEGER now{}, frequency{};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    const bool consecutive =
        s.pacing_qpc && s.presents == s.pacing_present + 1 && now.QuadPart > s.pacing_qpc && frequency.QuadPart > 0;
    if (!consecutive) {
        s.since_switch = s.since_long = ~0U;
    } else {
        if (s.since_switch != ~0U) ++s.since_switch;
        if (s.since_long != ~0U) ++s.since_long;
        if (count >= 1 && s.pacing_count >= 1 && count != s.pacing_count) {
            ++s.count_switches;
            s.since_switch = 0;
            if (s.since_long <= 2 && !s.long_matched) { // a long interval just before
                ++s.long_near_switch;
                s.long_matched = true;
            }
        }
        const auto ticks = static_cast<std::uint64_t>(now.QuadPart - s.pacing_qpc);
        if (ticks * 1000000 / static_cast<std::uint64_t>(frequency.QuadPart) > Session::long_present_us) {
            ++s.long_presents;
            s.since_long = 0;
            s.long_matched = s.since_switch <= 2; // at or just after a change
            if (s.long_matched) ++s.long_near_switch;
        }
    }
    s.pacing_qpc = now.QuadPart;
    s.pacing_present = s.presents;
    s.pacing_count = count;
}

// The state's Dynamic MFG support for status(): 1 no, 2 yes.
void note_state(Session &s, const sl::DLSSGState &state) {
    s.state = state;
    s.state_valid = true;
    s.shown_maximum.store(state.numFramesToGenerateMax);
    s.shown_dynamic_support.store(state.bIsDynamicMFGSupported == sl::Boolean::eTrue ? 2U : 1U);
}

// Owner thread, under the session lock: generated frames for the requested
// multiplier. The capability comes from the last state read on this thread.
void read_state_once(Session &s) {
    if (s.state_valid) return;
    sl::DLSSGState state{};
    if (s.sdk->frame_generation_state(viewport, state) == sl::Result::eOk) note_state(s, state);
}
unsigned wanted_generated(Session &s) {
    read_state_once(s);
    const auto multiplier = s.requested_multiplier.load();
    const auto maximum = s.state_valid ? s.state.numFramesToGenerateMax : 0;
    auto generated = reflex::generated_frames_for(multiplier, maximum);
    if (!generated) generated = 1;
    if (generated + 1 != multiplier) ++s.generated_limited;
    return generated;
}

// Owner thread, under the session lock: the options for the requested mode.
// eDynamic only with the state's support and while DLSS-G has not refused it;
// its numFramesToGenerate is ignored by DLSS-G and set to the maximum allowed.
struct Wanted {
    sl::DLSSGMode mode{};
    unsigned generated{}, target{};
    bool fallback{}; // dynamic asked for, the fixed multiplier used
};
Wanted wanted_options(Session &s) {
    read_state_once(s);
    // Winding down for a planned Off: plain 2x (reconcile()).
    if (s.winding) return {sl::DLSSGMode::eOn, 1, 0, false};
    const auto mode = static_cast<Mode>(s.requested_mode.load());
    if (mode == Mode::dynamic && !s.dynamic_refused.load() && s.state_valid &&
        s.state.bIsDynamicMFGSupported == sl::Boolean::eTrue && s.state.numFramesToGenerateMax)
        return {sl::DLSSGMode::eDynamic, s.state.numFramesToGenerateMax, s.requested_target.load(), false};
    return {mode == Mode::automatic ? sl::DLSSGMode::eAuto : sl::DLSSGMode::eOn, wanted_generated(s), 0,
            mode == Mode::dynamic};
}
} // namespace

static void finish_pending(Session &s, bool presented);

// Owner thread, under the session lock, each Present: the worker's and the
// player's requests become effective. DLSS-G paces a multiplier above 2x with
// a frame limit that stays after it is turned off and unloaded (perf13, RTX
// 5090: a flat 208 fps after 6x, DLSS on or off, until the player's Reflex
// Off; a stretch at 2x before the unload left none). So a planned Off from
// anything but plain 2x first shows plain 2x (wanted_options()), as a player
// stepping down would. Window changes (lifecycle_guard()) and missing inputs
// still turn it Off at once.
void detail::reconcile(Session &s) {
    const bool armed = s.armed_request.load(), enabled = s.enabled_request.load();
    const bool turning_off = (s.armed.load() && !armed) || (s.enabled.load() && !enabled);
    if (turning_off && s.on.load() && s.sdk->frame_generation()) {
        const auto now = GetTickCount64();
        const bool plain = s.applied_mode == sl::DLSSGMode::eOn && s.applied_generated == 1;
        if (!s.winding && !plain) {
            s.winding = true;
            s.wind_presents = 0;
            s.wind_began = now;
            ++s.winds;
        }
        if (s.winding && now - s.wind_began < wind_down_max_ms &&
            (s.wind_presents < wind_down_presents || now - s.wind_began < wind_down_min_ms))
            return;
    }
    if (s.winding) {
        s.winding = false;
        s.wound_presents += s.wind_presents;
    }
    s.armed.store(armed);
    s.enabled.store(enabled);
}

bool lifecycle_guard(unsigned) noexcept {
    auto &s = session;
    try {
        std::lock_guard lock(s.mutex);
        ++s.lifecycle_calls;
        if (!s.sdk || !s.on.load()) return false;
        // No frame token here: depth/motion/HUD-less stay tagged until the next
        // Present (eValidUntilPresent) and are re-tagged or nulled there.
        switch_off(s);
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
        s.hdr_lists_this_frame = 0;
        if (!s.sdk) return;
        // A missed present_end must not leak a tagged slot.
        if (s.pending != none) finish_pending(s, false);
        reconcile(s);
        // v0.27.4: DLSS-G unloaded (fg-live-toggle.md): nothing to tag or set;
        // it is Off with nothing held when it loads again with a new chain.
        if (!s.sdk->frame_generation()) {
            ++s.off[static_cast<unsigned>(Off::unloaded)];
            s.on.store(false);
            s.applied_generated = 0;
            s.applied_mode = sl::DLSSGMode::eOff;
            s.shown_generated.store(0);
            s.freed = true;
            return;
        }
        if (!s.enabled.load()) {
            turn_off(s, token, Off::disabled);
            // A long-term Off (guide 6.4): eOff, then slFreeResources once its
            // last On frames are surely done.
            if (!s.freed && ++s.disabled_presents >= free_after_presents) {
                s.freed = true;
                if (const auto r = s.sdk->frame_generation_free(viewport); r == sl::Result::eOk)
                    ++s.frees;
                else {
                    ++s.free_failed;
                    s.last_sdk_error = r;
                }
            }
            return;
        }
        s.disabled_presents = 0;
        s.freed = false;
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
        // The back buffer's scene rectangle ({top, left, width, height}); the
        // HUD-less image is that size, so its extent matches the subrect.
        static_assert(offsetof(sl::Extent, top) == 0 && offsetof(sl::Extent, left) == 4 &&
                          offsetof(sl::Extent, width) == 8 && offsetof(sl::Extent, height) == 12,
                      "sl::Extent field order");
        const auto fields = extent_fields(s.scene);
        sl::Extent output{fields[0], fields[1], fields[2], fields[3]},
            hudless_extent{0, 0, s.scene.width, s.scene.height};
        // A slot without HUD-less clears that tag explicitly (null resource).
        const bool with_hudless = s.slot_hudless[slot] && image.hudless;
        sl::Resource hudless{sl::ResourceType::eTex2d, with_hudless ? image.hudless.Get() : nullptr,
                             static_cast<std::uint32_t>(tag_state)};
        sl::ResourceTag tags[]{
            {&depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &render},
            {&motion, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &render},
            {nullptr, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle::eValidUntilPresent, &output},
            {with_hudless ? &hudless : nullptr, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent,
             &hudless_extent}};
        if (const auto r = s.sdk->tags(*token, viewport, tags, 4); r != sl::Result::eOk) {
            s.last_sdk_error = r;
            s.ring.untag(slot);
            return turn_off(s, token, Off::tags);
        }
        if (const auto w = wanted_options(s); !s.on.load() || w.mode != s.applied_mode ||
                                              w.generated != s.applied_generated || w.target != s.applied_target) {
            if (const auto r =
                    s.sdk->frame_generation_mode(viewport, w.mode, w.generated, static_cast<float>(w.target), true);
                r != sl::Result::eOk) {
                s.last_sdk_error = r;
                // eDynamic refused although reported supported: the fixed
                // multiplier from the next Present on (fail closed).
                if (w.mode == sl::DLSSGMode::eDynamic) s.dynamic_refused.store(true);
                // The SDK already holds the tags for this frame; keep the slot
                // until its consumption fence like any other tagged frame.
                s.pending = slot;
                ++s.off[static_cast<unsigned>(Off::options)];
                return;
            }
            if (!s.on.load())
                ++s.mode_changes;
            else if (w.mode != s.applied_mode || w.target != s.applied_target)
                ++s.mode_option_changes;
            else
                ++s.generated_changes;
            if (w.fallback) ++s.dynamic_fallbacks;
            s.on.store(true);
            s.applied_generated = w.generated;
            s.applied_mode = w.mode;
            s.applied_target = w.target;
            s.shown_generated.store(w.generated);
            s.shown_dynamic.store(w.mode == sl::DLSSGMode::eDynamic);
        }
        if (s.winding && s.applied_mode == sl::DLSSGMode::eOn && s.applied_generated == 1) ++s.wind_presents;
        s.pending = slot;
        s.last = {true, entry.frame, entry.epoch, camera};
        ++s.tagged;
        if (frame.reset) ++s.reset_tags;
        if (camera.reset) ++s.history_reset_tags;
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
        note_state(s, state);
        ++s.presented_frames[std::min<std::uint32_t>(state.numFramesActuallyPresented, 7)];
        if (state.numFramesActuallyPresented > 1) s.presented_generated.fetch_add(state.numFramesActuallyPresented - 1);
        s.shown_presented.store(state.numFramesActuallyPresented);
        if (presented) note_pacing(s, state.numFramesActuallyPresented);
        if (s.on.load() && state.numFramesActuallyPresented <= 1)
            s.single_streak.store(s.single_streak.load() + 1);
        else
            s.single_streak.store(0);
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

} // namespace mhw::frame_generation::game
