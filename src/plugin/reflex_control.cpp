// Reflex: the player's mode, applied by the worker under sdk_mutex, and the
// driver's sleep status that confirms it.
#include "plugin.hpp"
#include "../alpha/status.hpp"
#include "../reflex/driver_status.hpp"
#include "../reflex/reflex_mode.hpp"

namespace mhw::plugin {
bool driver_mode(const char* phase, bool expected, bool enforce) {
    const auto state=live_frames?live_frames->driver_status(device_pin.Get()):mhw::reflex::driver_sleep_status(device_pin.Get());
    log << "{\"event\":\"driver_sleep_status\",\"phase\":" << quoted(phase)
        << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get())
        << ",\"result\":" << state.result << ",\"low_latency\":" << state.low_latency
        << ",\"query_id\":" << 0xaef96ca1U << ",\"structure_version\":" << (136U | (1U<<16)) << "}\n" << std::flush;
    return state.result==0 && (!enforce || state.low_latency==expected);
}
void mode_options(const char* phase, sl::ReflexMode mode, sl::Result result) {
    log << "{\"event\":\"mode_options\",\"phase\":" << quoted(phase) << ",\"mode\":" << static_cast<unsigned>(mode)
        << ",\"result\":" << static_cast<unsigned>(result)
        << ",\"frame_limit_us\":0,\"use_markers_to_optimize\":false,\"boost_requested\":" << (mode==sl::ReflexMode::eLowLatencyWithBoost) << "}\n" << std::flush;
}
const char* reflex_mode_name(unsigned mode) noexcept {
    return mode==0?"Off":mode==2?"Boost":"On";
}
// The worker, under sdk_mutex: applies the requested mode if it changed. Off
// is refused while DLSS-G is loaded (it needs Reflex); the menu already keeps
// players from asking for that. True when a new mode was applied.
bool apply_reflex_mode(bool loaded) {
    const auto requested=reflex_requested.load();
    if(requested==reflex_applied.load()||requested>2)return false;
    if(requested==0&&loaded){
        log << "{\"event\":\"reflex_mode_refused\",\"mode\":0,\"reason\":\"frame generation needs Reflex\"}\n" << std::flush;
        reflex_requested.store(reflex_applied.load());return false;
    }
    const auto mode=static_cast<sl::ReflexMode>(requested);
    const auto result=sdk.options(mode);mode_options("live",mode,result);
    if(result==sl::Result::eOk){reflex_applied.store(requested);mhw::alpha::status::reflex_mode(reflex_mode_name(requested));return true;}
    reflex_requested.store(reflex_applied.load());return false;
}
}  // namespace mhw::plugin

namespace mhw::reflex {
void request_mode(unsigned mode) noexcept { if(mode<=2)mhw::plugin::reflex_requested.store(mode); }
bool frame_generation_loaded() noexcept { return mhw::plugin::fg_loaded_state.load(); }
}
