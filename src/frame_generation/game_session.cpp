// The worker's side of the frame generation session: configure, arm,
// enable, the multiplier and mode, and the Status values.
#include "game_session_state.hpp"

namespace mhw::frame_generation::game {
using namespace detail;

void configure(reflex::Streamline &sdk, unsigned output_width, unsigned output_height, SceneRect scene) noexcept {
    std::lock_guard lock(session.mutex);
    session.sdk = &sdk;
    session.output_width = output_width;
    session.output_height = output_height;
    session.scene = scene;
}

// On applies at once; Off at the next Present (reconcile()), at once while
// DLSS-G is not on.
void arm(bool armed) noexcept {
    session.armed_request.store(armed);
    if (armed || !session.on.load()) session.armed.store(armed);
}
void enable(bool enabled) noexcept {
    session.enabled_request.store(enabled);
    if (enabled || !session.on.load()) session.enabled.store(enabled);
}

void back_buffer(unsigned format) noexcept {
    session.back_buffer_format.store(format);
}
bool frame_generation_on() noexcept {
    return session.on.load();
}
bool inputs_settled() noexcept {
    auto &s = session;
    try {
        std::lock_guard lock(s.mutex);
        if (s.armed.load() || s.on.load() || s.pending != none) return false;
        if (s.fence) {
            const auto completed = s.fence->GetCompletedValue();
            if (completed == UINT64_MAX) return false;
            s.ring.reclaim(completed);
        }
        return s.ring.settled();
    } catch (...) {
        return false;
    }
}
bool release_inputs(bool scene) noexcept {
    auto &s = session;
    try {
        std::lock_guard lock(s.mutex);
        if (s.armed.load() || s.on.load() || s.pending != none || !s.ring.settled()) return false;
        s.ring.release_all();
        for (auto &i : s.images) i.depth.Reset(), i.motion.Reset();
        for (auto &c : s.staging) c = {};
        s.staging_failed = false;
        s.queue.clear();
        s.staged = {};
        s.slot_hudless = {};
        s.depth_desc = s.motion_desc = {};
        if (scene) {
            // A new scene size (configure() follows): the HUD-less pass and
            // images go too, and are created again at the new size.
            s.hudless.reset();
            for (auto &i : s.images) i.hudless.Reset();
            s.hudless_failed = false;
            s.capture_window.store(false);
        }
        // The next record() creates the depth/motion images at the new size
        // (without `scene`, the scene-sized HUD-less pass and images stay).
        s.created = s.create_failed = false;
        ++s.input_releases;
        return true;
    } catch (...) {
        return false;
    }
}
void multiplier(unsigned total) noexcept {
    if (reflex::generated_frames_for(total, 0)) session.requested_multiplier.store(total);
}
void mode(Mode mode, unsigned target_fps) noexcept {
    if (mode != Mode::fixed && mode != Mode::automatic && mode != Mode::dynamic) return;
    if (target_fps > 1000) return;
    // The target first: a Present that sees the new mode sees its target too.
    session.requested_target.store(target_fps);
    session.requested_mode.store(static_cast<unsigned>(mode));
}
Status status() noexcept {
    const auto &s = session;
    return {s.armed.load(),
            s.on.load(),
            s.requested_multiplier.load(),
            s.shown_generated.load(),
            s.shown_maximum.load(),
            s.presented_generated.load(),
            static_cast<Mode>(s.requested_mode.load()),
            s.requested_target.load(),
            s.shown_dynamic.load(),
            s.shown_dynamic_support.load(),
            s.dynamic_refused.load(),
            s.shown_presented.load(),
            s.single_streak.load(),
            s.enabled.load()};
}

} // namespace mhw::frame_generation::game
