// Reflex: the player's mode, applied by the worker under sdk_mutex, and the
// driver's sleep status that confirms it.
#include "plugin.hpp"
#include "../settings/status.hpp"
#include "../reflex/driver_status.hpp"
#include "../reflex/reflex_mode.hpp"

namespace mhw::plugin {
bool driver_mode(const char *phase, bool expected, bool enforce) {
    const auto state =
        live_frames ? live_frames->driver_status(device_pin.Get()) : mhw::reflex::driver_sleep_status(device_pin.Get());
    log << "{\"event\":\"driver_sleep_status\",\"phase\":" << quoted(phase)
        << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get()) << ",\"result\":" << state.result
        << ",\"low_latency\":" << state.low_latency << ",\"query_id\":" << 0xaef96ca1U
        << ",\"structure_version\":" << (136U | (1U << 16)) << ",\"sleep_interval_us\":" << state.sleep_interval_us
        << ",\"fg_multiplier\":" << unsigned(state.fg_multiplier) << ",\"dfg_control\":" << unsigned(state.dfg_control)
        << ",\"dfg_frame_time_target_us\":" << state.dfg_frame_time_target_us
        << ",\"fullscreen_vrr\":" << state.fullscreen_vrr << ",\"control_panel_vsync\":" << state.control_panel_vsync
        << ",\"use_game_sleep\":" << state.use_game_sleep << "}\n"
        << std::flush;
    return state.result == 0 && (!enforce || state.low_latency == expected);
}
void mode_options(const char *phase, sl::ReflexMode mode, sl::Result result) {
    log << "{\"event\":\"mode_options\",\"phase\":" << quoted(phase) << ",\"mode\":" << static_cast<unsigned>(mode)
        << ",\"result\":" << static_cast<unsigned>(result)
        << ",\"frame_limit_us\":0,\"use_markers_to_optimize\":false,\"boost_requested\":"
        << (mode == sl::ReflexMode::eLowLatencyWithBoost) << "}\n"
        << std::flush;
}
// sl::ReflexMode is numbered as the ini's modes (off, on, boost), so the
// applied mode reads with the ini mode's name.
static_assert(unsigned(sl::ReflexMode::eOff) == unsigned(mhw::settings::ReflexMode::off) &&
              unsigned(sl::ReflexMode::eLowLatency) == unsigned(mhw::settings::ReflexMode::on) &&
              unsigned(sl::ReflexMode::eLowLatencyWithBoost) == unsigned(mhw::settings::ReflexMode::boost));
const char *reflex_mode_name(unsigned mode) noexcept {
    return mhw::settings::reflex_name(static_cast<mhw::settings::ReflexMode>(mode));
}
// The worker, under sdk_mutex: applies the requested mode if it changed. Off
// is refused while DLSS-G is loaded (it needs Reflex); the menu already keeps
// players from asking for that. True when a new mode was applied.
bool apply_reflex_mode(bool loaded) {
    const auto requested = reflex_requested.load();
    if (requested == reflex_applied.load() || requested > 2) return false;
    if (requested == 0 && loaded) {
        log << "{\"event\":\"reflex_mode_refused\",\"mode\":0,\"reason\":\"frame generation needs Reflex\"}\n"
            << std::flush;
        reflex_requested.store(reflex_applied.load());
        return false;
    }
    const auto mode = static_cast<sl::ReflexMode>(requested);
    const auto result = sdk.options(mode);
    mode_options("live", mode, result);
    if (result == sl::Result::eOk) {
        reflex_applied.store(requested);
        mhw::settings::status::reflex_mode(reflex_mode_name(requested));
        return true;
    }
    reflex_requested.store(reflex_applied.load());
    return false;
}
// The worker, while the session runs: the driver did not confirm the applied
// mode (driver_mode logged its status). Up to 0.28.6 this ended the worker,
// which left DLSS running but stopped the menu and the log. Now, as for a
// presentation fault: with On or Boost, Reflex shows Off and frame generation,
// which needs it, is off for the session; the worker goes on.
void reflex_unconfirmed(const char *phase) {
    const auto mode = reflex_applied.load();
    log << "{\"event\":\"reflex_unconfirmed\",\"phase\":" << quoted(phase) << ",\"mode\":" << mode
        << ",\"generation_off\":" << (mode != 0 && !generation_fault) << "}\n"
        << std::flush;
    if (mode == 0) return; // Off asked for; the driver kept low latency on
    mhw::settings::status::reflex(mhw::settings::status::Stage::off, "reflex unconfirmed");
    loader_log(AL_LOG_WARNING, std::string("the graphics driver did not confirm Reflex (") + phase + ")");
    if (generation_fault) return;
    generation_fault = true;
    namespace fg = mhw::frame_generation::game;
    if (fg::status().armed) fg::arm(false);
    if (fg_plugin) mhw::settings::status::generation(mhw::settings::status::Generation::stopped, "reflex unconfirmed");
}
} // namespace mhw::plugin

namespace mhw::reflex {
void request_mode(unsigned mode) noexcept {
    if (mode <= 2) mhw::plugin::reflex_requested.store(mode);
}
bool frame_generation_loaded() noexcept {
    return mhw::plugin::fg_loaded_state.load();
}
} // namespace mhw::reflex
