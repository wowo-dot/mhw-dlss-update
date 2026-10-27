// The frame generation session in the log: fg_status's "session" (each
// heartbeat) and the session end.
#include "game_session_state.hpp"

namespace mhw::frame_generation::game {
using namespace detail;

// A raw GPU attempt can read NaN (game-45: the last attempt was -nan); JSON has
// no non-finite numbers, so those are written as null.
struct Number {
    float value;
    friend std::ostream &operator<<(std::ostream &o, Number n) {
        return std::isfinite(n.value) ? o << n.value : o << "null";
    }
};

void write(std::ostream &o) {
    auto &s = session;
    std::lock_guard lock(s.mutex);
    const auto &r = s.ring.counters();
    o << "{\"schema\":2,\"armed\":" << s.armed.load() << ",\"on\":" << s.on.load()
      << ",\"configured\":" << (s.sdk != nullptr) << ",\"enabled\":" << s.enabled.load() << ",\"frees\":" << s.frees
      << ",\"free_failed\":" << s.free_failed << ",\"winds\":" << s.winds << ",\"wound_presents\":" << s.wound_presents
      << ",\"loaded\":" << (s.sdk && s.sdk->frame_generation()) << ",\"multiplier\":" << s.requested_multiplier.load()
      << ",\"generated\":" << s.applied_generated << ",\"generated_changes\":" << s.generated_changes
      << ",\"generated_limited\":" << s.generated_limited << ",\"mode\":{\"requested\":" << s.requested_mode.load()
      << ",\"target\":" << s.requested_target.load() << ",\"applied\":" << static_cast<unsigned>(s.applied_mode)
      << ",\"applied_target\":" << s.applied_target << ",\"changes\":" << s.mode_option_changes
      << ",\"dynamic_fallbacks\":" << s.dynamic_fallbacks << ",\"dynamic_refused\":" << s.dynamic_refused.load()
      << ",\"single_streak\":" << s.single_streak.load() << '}'
      << ",\"pacing\":{\"count_switches\":" << s.count_switches << ",\"long\":" << s.long_presents
      << ",\"long_near_switch\":" << s.long_near_switch << ",\"long_us\":" << Session::long_present_us << '}'
      << ",\"hudless\":{\"available\":" << (s.hudless != nullptr) << ",\"failed\":" << s.hudless_failed
      << ",\"recorded\":" << s.hudless_recorded << ",\"tagged\":" << s.hudless_tagged << ",\"skips\":{";
    for (unsigned i = 0; i < s.hud_skips.size(); ++i)
        o << (i ? "," : "") << '"' << hud_skip_names[i] << "\":" << s.hud_skips[i];
    o << "},\"restore_reasons\":{";
    constexpr const char *restore_names[]{"unknown",     "invalid",       "no_generation", "no_pipeline",
                                          "no_graphics", "graphics_only", "fresh"};
    for (unsigned i = 0; i < s.restore_reasons.size(); ++i)
        o << (i ? "," : "") << '"' << restore_names[i] << "\":" << s.restore_reasons[i];
    o << "},\"probe_ok\":" << s.probe_ok << ",\"probe_fallback\":" << s.probe_fallback
      << ",\"probe_invalid\":" << s.probe_invalid << ",\"gamma_ex\":" << Number{s.last_probe.gamma_ex}
      << ",\"gamma\":" << Number{s.last_probe.gamma} << ",\"gpu_attempts\":" << s.last_probe.attempts
      << ",\"gpu_matches\":" << s.last_probe.matches << ",\"last_attempt\":[" << Number{s.last_probe.last_gamma} << ','
      << Number{s.last_probe.last_gamma_ex} << "],\"screen_region\":[" << s.last_probe.region[0] << ','
      << s.last_probe.region[1] << ',' << s.last_probe.region[2] << ',' << s.last_probe.region[3]
      << "],\"candidates\":[";
    for (unsigned i = 0; i < s.candidate_count; ++i) {
        o << (i ? ",[" : "[");
        for (unsigned j = 0; j < 8; ++j) o << (j ? "," : "") << s.candidates[i][j];
        o << ']';
    }
    o << "],\"captures\":" << s.captures << ",\"capture_no_cbv\":" << s.capture_no_cbv
      << ",\"capture_limited\":" << s.capture_limited << ",\"capture_failed\":" << s.capture_failed
      << ",\"back_buffer_format\":" << s.back_buffer_format.load() << ",\"recreated\":" << s.hudless_recreated
      << ",\"hdr\":{\"captures\":" << s.hdr_captures << ",\"probe_ok\":" << s.hdr_probe_ok
      << ",\"probe_default\":" << s.hdr_probe_default << ",\"used\":" << s.last_probe.hdr
      << ",\"valid\":" << s.last_probe.hdr_ok << ",\"white\":" << Number{s.last_probe.white}
      << ",\"ratio\":" << Number{s.last_probe.ratio} << ",\"gamma\":" << Number{s.last_probe.hdr_gamma}
      << ",\"gui_gamma\":" << s.last_probe.gui_gamma << ",\"gpu_attempts\":" << s.last_probe.hdr_attempts
      << ",\"gpu_matches\":" << s.last_probe.hdr_matches << ",\"parameter\":" << s.last_probe.hdr_parameter
      << ",\"last_attempt\":[" << Number{s.last_probe.last_white} << ',' << Number{s.last_probe.last_ratio} << ','
      << Number{s.last_probe.last_hdr_gamma} << ',' << s.last_probe.last_gui << "]}"
      << ",\"error\":\"" << s.hudless_error.data() << "\"},\"staged\":" << s.staged_count
      << ",\"staged_dropped\":" << s.staged_dropped + s.queue.dropped << ",\"final_unstaged\":" << s.final_unstaged
      << ",\"final_mismatch\":" << s.final_mismatch << ",\"mismatch_delta\":[" << s.mismatch_delta[0] << ','
      << s.mismatch_delta[1] << ',' << s.mismatch_delta[2] << ',' << s.mismatch_delta[3] << ',' << s.mismatch_delta[4]
      << "],\"keyed\":{\"on\":" << perf::on(perf::keyed_staging) << ",\"hits\":" << s.keyed_hits
      << ",\"copies\":" << s.staging_copies << ",\"early\":" << s.keyed_early << ",\"confirms\":" << s.keyed_confirms
      << ",\"failed\":" << s.staging_failed << '}' << ",\"presented_frames\":[";
    for (unsigned i = 0; i < s.presented_frames.size(); ++i) o << (i ? "," : "") << s.presented_frames[i];
    o << "]"
      << ",\"created\":" << s.created << ",\"create_failed\":" << s.create_failed
      << ",\"video_memory\":{\"checked\":" << s.memory.done << ",\"read\":" << s.memory.read
      << ",\"passed\":" << s.memory.passed << ",\"budget_mb\":" << (s.memory.budget >> 20)
      << ",\"usage_mb\":" << (s.memory.usage >> 20) << ",\"needed_mb\":" << (s.memory.needed >> 20) << '}'
      << ",\"output\":[" << s.output_width << ',' << s.output_height << "],\"scene\":[" << s.scene.x << ',' << s.scene.y
      << ',' << s.scene.width << ',' << s.scene.height << "],\"render\":[" << s.depth_desc.Width << ','
      << s.depth_desc.Height << "],\"formats\":[" << s.depth_desc.Format << ',' << s.motion_desc.Format
      << "],\"presents\":" << s.presents << ",\"tagged\":" << s.tagged << ",\"recorded\":" << s.recorded
      << ",\"record_failed\":" << s.record_failed << ",\"size_mismatch\":" << s.size_mismatch
      << ",\"state_failed\":" << s.state_failed << ",\"mode_changes\":" << s.mode_changes
      << ",\"reset_tags\":" << s.reset_tags << ",\"history_reset_tags\":" << s.history_reset_tags
      << ",\"null_tags\":" << s.null_tags << ",\"unpresented\":" << s.unpresented
      << ",\"lifecycle_calls\":" << s.lifecycle_calls << ",\"lifecycle_offs\":" << s.lifecycle_offs
      << ",\"exceptions\":" << s.exceptions << ",\"last_sdk_error\":" << static_cast<unsigned>(s.last_sdk_error)
      << ",\"off\":{";
    for (unsigned i = 0; i < s.off.size(); ++i) o << (i ? "," : "") << '"' << off_names[i] << "\":" << s.off[i];
    o << "},\"camera_failures\":[";
    for (unsigned i = 0; i < s.camera_failures.size(); ++i) o << (i ? "," : "") << s.camera_failures[i];
    o << "],\"present_delta\":[" << s.present_delta[0] << ',' << s.present_delta[1] << ',' << s.present_delta[2] << ','
      << s.present_delta[3] << "],\"ring\":{\"acquired\":" << r.acquired << ",\"starved\":" << r.starved
      << ",\"duplicates\":" << r.duplicates << ",\"abandoned\":" << r.abandoned << ",\"submitted\":" << r.submitted
      << ",\"discarded\":" << r.discarded << ",\"taken\":" << r.taken << ",\"stale\":" << r.stale
      << ",\"no_ready\":" << r.no_ready << ",\"consumed\":" << r.consumed << ",\"reclaimed\":" << r.reclaimed
      << ",\"unconfirmed\":" << r.unconfirmed << ",\"released\":" << r.released
      << "},\"input_releases\":" << s.input_releases << ",\"state\":{\"valid\":" << s.state_valid
      << ",\"status\":" << static_cast<unsigned>(s.state.status)
      << ",\"min_width_or_height\":" << s.state.minWidthOrHeight
      << ",\"frames_presented\":" << s.state.numFramesActuallyPresented
      << ",\"max_generated\":" << s.state.numFramesToGenerateMax
      << ",\"dynamic_supported\":" << static_cast<int>(s.state.bIsDynamicMFGSupported)
      << ",\"vram\":" << s.state.estimatedVRAMUsageInBytes
      << ",\"fence_value\":" << s.state.lastPresentInputsProcessingCompletionFenceValue << "},\"samples\":[";
    for (unsigned i = 0; i < s.sample_count; ++i) {
        const auto &x = s.samples[i];
        const auto &c = x.constants;
        o << (i ? "," : "") << "{\"present\":" << x.present_frame << ",\"slot\":" << x.slot_frame
          << ",\"near\":" << c.near_plane << ",\"far\":" << c.far_plane << ",\"far_infinite\":" << c.far_infinite
          << ",\"fov\":" << c.fov << ",\"aspect\":" << c.aspect << ",\"left_handed\":" << c.left_handed
          << ",\"reset\":" << c.reset << ",\"jitter\":[" << c.jitter[0] << ',' << c.jitter[1] << "],\"position\":["
          << c.position[0] << ',' << c.position[1] << ',' << c.position[2] << "],\"forward\":[" << c.forward[0] << ','
          << c.forward[1] << ',' << c.forward[2] << "],\"up\":[" << c.up[0] << ',' << c.up[1] << ',' << c.up[2]
          << "],\"pinhole\":[" << c.pinhole[0] << ',' << c.pinhole[1] << "],\"clip_to_prev\":[";
        for (unsigned k = 0; k < 16; ++k) o << (k ? "," : "") << c.clip_to_prev_clip[k];
        o << "]}";
    }
    o << "]}";
}

} // namespace mhw::frame_generation::game
