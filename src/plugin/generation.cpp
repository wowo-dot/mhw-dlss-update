// Frame generation in the session: DLSS-G is loaded exactly while frame
// generation is on and DLSS runs (the live switch, v0.27.4, fg-live-toggle.md),
// a presentation fault turns it off for the session, and an aspect change
// gives it a new scene rectangle.
#include "plugin.hpp"
#include "../settings/status.hpp"
#include "../projection_binding/memory.hpp"
#include "../reflex/temporal_coexistence.hpp"
#include <intrin.h>

namespace mhw::plugin {
namespace pb = mhw::projection_binding;
// v0.27.4 (fg-live-toggle.md): the game's own "make the buffers again"
// request and its latency waitable, checked once at arming.
// - sub_14229C320 (the HDR switch) sets 0x4000 in sRender+0x1F548 when the
//   chain's buffers must be made again; sub_14229B020 (gated by
//   frame_boundaries.cpp) consumes it at the next frame and resizes;
// - sub_1423ADE30 waits on cD3DRender+0x10F0 (sRender+0x78), which
//   cD3DRender::initForD3D12 (gated) fills from GetFrameLatencyWaitableObject.
bool game_chain_qualified() noexcept {
    std::uintptr_t renderer{}, backend{};
    return pb::verify_code(image, 0x229c320, 0x44,
                           "4d688f8b830ca460e4ec196111c5f0cd73dbc95089b2a7ebcb6c2bbc4a22a0c0") &&
           pb::verify_code(image, 0x23ade30, 0x47,
                           "6c9808bc6ea7ff818d46b1e8d91ff3abfd6e0db181b1f9b503370707746b1900") &&
           pb::verify_code(image, 0x229b020, 2420,
                           "a4a09901e108fe38dabfb6d9df3a52f9d5ccd68a12b8b8e6f7a9ea671cbf5e90") &&
           pb::verify_code(image, 0x23a48e0, 11101,
                           "8757a054cc1c203128fdfb73ac88975dda7dc73ef5ef1a53a80f22cd0eb8e500") &&
           pb::read(image + 0x51c4480, renderer) && renderer && pb::read(renderer + 0x78, backend) && backend;
}
std::uintptr_t game_waitable_slot() noexcept {
    std::uintptr_t renderer{}, backend{};
    return pb::read(image + 0x51c4480, renderer) && renderer && pb::read(renderer + 0x78, backend) && backend
               ? backend + 0x10F0
               : 0;
}
bool game_resize_request() noexcept {
    std::uintptr_t renderer{};
    if (!pb::read(image + 0x51c4480, renderer) || !renderer) return false;
    __try {
        _InterlockedOr(reinterpret_cast<volatile long *>(renderer + 0x1F548), 0x4000);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
namespace {
std::uint64_t switch_outcomes(const mhw::reflex::PresentationStats &v) noexcept {
    return v.switches + v.switch_failures + v.switch_refusals;
}
} // namespace
// Worker, every loop: DLSS-G is loaded exactly while frame generation is on
// and DLSS runs. A change asks the presentation for a switch and the game for
// its buffer remake, in which the switch happens; frame generation is Off
// with its inputs settled before an unload, armed again after a load.
void sync_generation(GenerationSwitch &g) {
    namespace fg = mhw::frame_generation::game;
    namespace cd = mhw::continuous_dlaa;
    if (!fg_plugin || !game_chain_ready || !fg_ready) return;
    const auto v = presentation->snapshot();
    if (!v.identity) return;
    const auto now = GetTickCount64();
    if (g.active) {
        if (switch_outcomes(v) == g.outcomes) {
            if (now - g.asked_at < 1500) return;
            // The game did not resize (paused, minimized): ask again, up to 20 times.
            if (++g.tries < 20) {
                game_resize_request();
                g.asked_at = now;
                return;
            }
            presentation->request_switch(fg_loaded); // nothing to do at a later resize
            log << "{\"event\":\"fg_switch\",\"want\":" << g.want << ",\"done\":false,\"reason\":\"no resize\"}\n"
                << std::flush;
            g = {};
            return;
        }
        fg_loaded = v.frame_generation_loaded;
        fg_loaded_state.store(fg_loaded);
        log << "{\"event\":\"fg_switch\",\"want\":" << g.want << ",\"done\":true,\"loaded\":" << fg_loaded
            << ",\"reason\":" << v.switch_reason << ",\"switches\":" << v.switches
            << ",\"failures\":" << v.switch_failures << ",\"refusals\":" << v.switch_refusals
            << ",\"hr\":" << v.switch_last_hr << ",\"final_hr\":" << v.switch_final_hr
            << ",\"last_resort\":" << v.last_resort << ",\"waitable_failures\":" << v.waitable_failures
            << ",\"tries\":" << g.tries << ",\"ms\":" << now - g.asked_at << ",\"buffers\":" << v.buffers
            << ",\"queues\":" << v.queues << ",\"late_queues\":" << v.late_queues << "}\n"
            << std::flush;
        mhw::settings::status::reflex_mode(reflex_mode_name(reflex_applied.load()));
        if (fg_loaded != g.want) {
            // Refused (exclusive fullscreen, the game's waitable moved) or failed:
            // not asked again until the player changes it or the game changes
            // its fullscreen state (log_lifecycle).
            switch_unreached = g.want;
            if (g.want && !generation_fault)
                mhw::settings::status::generation(mhw::settings::status::Generation::unavailable,
                                               v.switch_reason == 4   ? "exclusive fullscreen"
                                               : v.switch_reason == 5 ? "latency waitable"
                                                                      : "switch failed");
        } else if (g.want && !generation_fault)
            mhw::settings::status::generation(mhw::settings::status::Generation::armed);
        if (fg_loaded && !generation_fault && cd::live_modes() && !cd::dlss_off()) fg::arm(true);
        g = {};
        return;
    }
    const bool want = !generation_fault && fg::status().enabled && cd::live_modes() && !cd::dlss_off();
    if (want == fg_loaded) {
        // An unload that did not happen, then the player's On again: armed again.
        if (switch_unreached == 0 && want && !fg::status().armed) fg::arm(true);
        switch_unreached = -1;
        return;
    }
    if (switch_unreached == static_cast<int>(want)) return;
    if (!want) {
        // DLSS-G Off at the next Present and no input slot in use.
        if (fg::status().armed) fg::arm(false);
        if (fg::status().on || !fg::inputs_settled()) return;
    }
    if (!presentation->request_switch(want)) return;
    g = {now, 0, switch_outcomes(v), want, true};
    game_resize_request();
}
// Worker: a presentation fault that kept routing (v0.28.6, tifa-6), logged at
// once with where and why. Frame generation is off for the session: Off now,
// DLSS-G unloaded by sync_generation's switch; the worker goes on.
void presentation_fault(const mhw::reflex::PresentationStats &v) {
    namespace fg = mhw::frame_generation::game;
    // Bounded: a fault that repeats (a paced Present failing each frame) is
    // logged at counts 1, 2, 4, 8... or when its site changes.
    static std::uint32_t logged_site = ~0U;
    presentation_faults = v.faults;
    const std::string why(v.fault_why.data());
    if ((v.faults & (v.faults - 1)) != 0 && v.fault_site == logged_site) return;
    logged_site = v.fault_site;
    log << "{\"event\":\"presentation_fault\",\"faults\":" << v.faults << ",\"site\":" << v.fault_site
        << ",\"hr\":" << v.fault_hr << ",\"why\":" << quoted(why) << ",\"thread\":" << v.fault_thread
        << ",\"routing\":" << v.routing << ",\"identity\":" << v.identity
        << ",\"frame_generation_loaded\":" << v.frame_generation_loaded << ",\"last_resort\":" << v.last_resort
        << ",\"switch_reason\":" << v.switch_reason << ",\"queues\":" << v.queues
        << ",\"late_queues\":" << v.late_queues << ",\"queue_overflows\":" << v.queue_overflows
        << ",\"queue_failures\":" << v.queue_failures << ",\"unroutable\":" << v.unroutable_calls << "}\n"
        << std::flush;
    if (generation_fault) return;
    generation_fault = true;
    if (fg::status().armed) fg::arm(false);
    if (fg_plugin) mhw::settings::status::generation(mhw::settings::status::Generation::stopped, "presentation fault");
    loader_log(AL_LOG_WARNING, "frame generation turned off for this session: presentation fault (" + why + ")");
}
// Worker: after a scene change, frame generation takes the new scene
// rectangle (centred in the back buffer); false if it does not fit.
bool configure_generation_scene() {
    unsigned sw{}, sh{}, ww{}, wh{};
    mhw::reflex::temporal::scene_extent(sw, sh, ww, wh);
    const auto chain = presentation->snapshot();
    if (!sw || !sh || sw > chain.width || sh > chain.height || ww != chain.width || wh != chain.height) return false;
    mhw::frame_generation::game::back_buffer(chain.format);
    mhw::frame_generation::game::configure(sdk, chain.width, chain.height,
                                           {(chain.width - sw) / 2, (chain.height - sh) / 2, sw, sh});
    return true;
}
} // namespace mhw::plugin
