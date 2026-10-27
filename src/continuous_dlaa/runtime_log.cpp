// The runtime in the log: the summary every heartbeat (dlss_summary) and
// the full state.
#include "runtime_state.hpp"

namespace mhw::continuous_dlaa {
using namespace detail;
namespace {
// The projection capture's first refusal, or null; the caller holds the lock.
void write_projection_refusal(std::ostream &out) {
    const auto &r = projection_refusal;
    if (!r.recorded) {
        out << "null";
        return;
    }
    out << "{\"frame\":" << r.frame << ",\"failed\":" << r.failed << ",\"rejection\":" << r.rejection
        << ",\"checkpoint\":" << r.checkpoint << ",\"reads\":" << r.reads << ",\"words\":" << r.words
        << ",\"constants\":" << r.constants << ",\"readable_end\":" << r.readable_end << ",\"queried\":" << r.queried
        << ",\"region\":{\"base\":" << reinterpret_cast<std::uintptr_t>(r.region.BaseAddress)
        << ",\"allocation\":" << reinterpret_cast<std::uintptr_t>(r.region.AllocationBase)
        << ",\"size\":" << r.region.RegionSize << ",\"state\":" << r.region.State << ",\"protect\":" << r.region.Protect
        << ",\"type\":" << r.region.Type << "}}";
}
} // namespace
void write_summary(std::ostream &out) {
    Lock lock;
    const auto &h = history_telemetry;
    const auto reads = game_memory::read_cache_stats();
    out << "{\"active\":" << accepting.load() << ",\"fault\":" << fault.load() << ",\"frames\":" << frames.load()
        << ",\"skips\":" << skips.load() << ",\"lists\":{\"live\":" << live_lists() << ",\"peak\":" << lists_peak
        << ",\"pruned\":" << lists_pruned << '}' << ",\"reads\":{\"level1_misses\":" << reads.level1_misses
        << ",\"region_hits\":" << reads.region_hits << ",\"refreshes\":" << reads.refreshes
        << ",\"queries\":" << reads.queries << ",\"background_queries\":" << reads.background_queries
        << ",\"host_pages\":" << reads.host_pages << ",\"host_faults\":" << reads.host_faults << '}'
        << ",\"video_memory\":{\"checked\":" << memory_check.done << ",\"read\":" << memory_check.read
        << ",\"passed\":" << memory_check.passed << ",\"budget_mb\":" << (memory_check.budget >> 20)
        << ",\"usage_mb\":" << (memory_check.usage >> 20) << ",\"needed_mb\":" << (memory_check.needed >> 20)
        << ",\"images_mb\":" << ((pass ? pass->allocation_bytes() : 0) >> 20) << '}'
        << ",\"evaluations\":" << h.evaluations << ",\"resets\":" << h.resets << ",\"reasons\":{";
    for (unsigned i = 0; i < reset_reason_count; ++i) {
        if (i) out << ',';
        out << '"' << reset_reason_names[i] << "\":" << h.counts[i];
    }
    out << "},\"projection_changes_retained\":" << h.projection_changes_retained << ",\"skip_reasons\":[";
    for (unsigned i = 0; i < dispatch_reasons.size(); ++i) {
        if (i) out << ',';
        out << dispatch_reasons[i];
    }
    out << ']';
    out << ",\"producer_refusals\":[";
    for (unsigned i = 0; i < producer_refusals.size(); ++i) {
        if (i) out << ',';
        out << producer_refusals[i].load();
    }
    {
        const auto &l = last_producer_refusal; // write_summary holds the lock
        out << "],\"producer_last\":{\"frame\":" << l.frame << ",\"reason\":" << l.reason << ",\"values\":["
            << l.values[0] << ',' << l.values[1] << ',' << l.values[2] << ',' << l.values[3] << "]}";
    }
    out << ",\"projection_refusal\":";
    write_projection_refusal(out);
    // The game's TAA without reprojection, run as its own frames (0.30.2).
    out << ",\"no_reprojection\":";
    no_reprojection_frames.write(out);
    out << ",\"projection_viewports\":[";
    for (unsigned i = 0; i < projection_viewports.size(); ++i) {
        if (i) out << ',';
        out << projection_viewports[i].load();
    }
    out << ']';
    // The camera jitter (0.31.1): the phases, the projections each source
    // jittered, and the mod's own offsets' range in render pixels.
    out << ",\"jitter\":{\"phases\":" << jitter_phases.load() << ",\"ab\":" << dlss_sdk::jitter_ab.load()
        << ",\"own\":" << own_jitters.load() << ",\"table\":" << table_jitters.load() << ",\"x\":["
        << own_jitter_x_min.load() << ',' << own_jitter_x_max.load() << "],\"y\":[" << own_jitter_y_min.load() << ','
        << own_jitter_y_max.load() << "]}";
    // Skip reason 7 is a missing depth lease or a route readiness rejection.
    {
        DepthTraceLock trace_lock;
        out << ",\"depth_missing\":" << scaled_depth_trace.missing_count
            << ",\"route_not_ready\":" << scaled_depth_trace.readiness_rejected;
    }
    // Why the private depth copy was not admitted (DLAA, game-65: every frame),
    // and the depth producer/consumer trace by stage and reason.
    out << ",\"consumer_depth\":";
    depth_consumer_admission.write(out);
    out << ",\"depth_capture\":";
    {
        DepthTraceLock trace_lock;
        scaled_depth_trace.write(out);
    }
    out << ",\"identity_depth\":";
    identity_depth_survey.write(out);
    out << ",\"route\":";
    sr::write_summary(out);
    // The game's own DLSS rules for its AA and resolution scaling options,
    // with the game's per-frame AA choice (Off, TAA, FXAA, both) seen by
    // the owned frames.
    out << ",\"game_options\":";
    game_options::write(out);
    // The sampler table cache's fill and the material bank (texture-mip-bias.md).
    out << ",\"texture_bias\":";
    texture_bias::write(out);
    out << ",\"native_choices\":";
    activation::write_choices(out);
    out << '}';
}
void write(std::ostream &out) {
    Lock lock;
    bool disabled = true;
    for (bool e : enabled) disabled = disabled && !e;
    const auto completed = fence ? fence->GetCompletedValue() : 0;
    // A failed/duplicate native submission is still forwarded. Its old fence
    // may not cover that unexpected use, so freeze all remaining pins on fault.
    if (!fault.load()) recording_pins.collect(completed);
    if (!fault.load()) sr::collect(completed);
    out << std::boolalpha << "{\"event\":\"continuous\",\"schema\":1,\"active\":" << accepting.load()
        << ",\"fault\":" << fault.load() << ",\"pending\":" << pending.load() << ",\"frames\":" << frames.load()
        << ",\"skips\":" << skips.load() << ",\"history_resets\":" << reset_count.load()
        << ",\"jitter_injections\":" << jitter_count.load() << ",\"jitter_restorations\":" << restorations.load()
        << ",\"depth_copies\":" << depth_count.load() << ",\"signal_value\":" << signal_value
        << ",\"completed_value\":" << completed << ",\"last_skip\":" << last_skip.load()
        << ",\"list_count\":" << list_count << ",\"lists\":{\"live\":" << live_lists() << ",\"peak\":" << lists_peak
        << ",\"pruned\":" << lists_pruned << "},\"resource_count\":" << 4 * recording_pins.live()
        << ",\"pending_private_recording\":" << (lease.list != nullptr) << ",\"hooks_disabled\":" << disabled
        << ",\"retention\":\"reset-and-fence-v1\",\"width\":" << runtime.width << ",\"height\":" << runtime.height
        << ",\"format\":" << runtime.format << ",\"luid_low\":" << runtime.luid_low
        << ",\"luid_high\":" << runtime.luid_high << ",\"sdk\":";
    if (pass)
        pass->write(out);
    else
        out << "null";
    out << ",\"history\":";
    history_telemetry.write(out);
    out << ",\"requests\":";
    request_telemetry.write(out);
    out << ",\"recordings\":";
    recording_pins.write(out, completed);
    out << ",\"resolve\":";
    resolve_telemetry.write(out);
    out << ",\"shader_rejection\":";
    shader_rejection.write(out, shader_rejection.bytes
                                    ? pb::bytes_sha256(shader_rejection.bytecode.data(), shader_rejection.bytes)
                                    : std::string{});
    out << ",\"shader_admission\":{\"schema\":1,\"variants\":[";
    for (unsigned i = 0; i < unknown_native_shader; ++i) {
        if (i) out << ',';
        const auto &v = native_shader_variants[i];
        out << "{\"technique\":" << v.technique << ",\"bytes\":" << v.bytes
            << ",\"verified\":" << shader_cache.verified[i] << ",\"accepted\":" << shader_cache.accepted[i] << '}';
    }
    out << "]}";
    out << ",\"no_reprojection\":";
    no_reprojection_frames.write(out);
    out << ",\"preparation_shader_admission\":{\"schema\":1,\"variants\":[";
    for (unsigned i = 0; i < unknown_native_shader; ++i) {
        if (i) out << ',';
        const auto &v = native_shader_variants[i];
        out << "{\"technique\":" << v.technique << ",\"bytes\":" << v.bytes
            << ",\"verified\":" << preparation_shader_cache.verified[i]
            << ",\"accepted\":" << preparation_shader_cache.accepted[i] << '}';
    }
    out << "]}";
    out << ",\"reconstruction\":";
    reconstruction.write(out);
    out << ",\"state_trace\":";
    state_trace.write(out);
    out << ",\"state_admission\":";
    state_admission.write(out);
    out << ",\"depth_eligibility\":";
    depth_eligibility.write(out);
    out << ",\"depth_destination\":";
    depth_destination.write(out);
    out << ",\"unfiltered_lifetime\":";
    unfiltered_lifetime.write(out, !fault.load());
    out << ",\"zero_jitter_projections\":" << zero_jitter_projections.load() << ",\"dispatch_reasons\":[";
    for (unsigned i = 0; i < dispatch_reasons.size(); ++i) out << (i ? "," : "") << dispatch_reasons[i];
    out << ']';
    out << ",\"projection_refusal\":";
    write_projection_refusal(out);
    out << ",\"forwarded_requests\":";
    forwarded_telemetry.write(out);
    out << ",\"activation\":";
    activation::write(out);
    out << ",\"game_options\":";
    game_options::write(out);
    out << ",\"scaled_depth_capture\":";
    {
        DepthTraceLock trace_lock;
        scaled_depth_trace.write(out);
    }
    out << ",\"depth_schedule\":";
    depth_schedule_trace.write(out);
    out << ",\"consumer_depth\":";
    depth_consumer_admission.write(out);
    out << ",\"spatial_recordings\":" << spatial_recordings << ",\"route_only_signals\":" << route_only_signals;
    out << ",\"edge_fill\":{\"column\":" << edge_column << ",\"row\":" << edge_row << ",\"fills\":" << edge_fills
        << '}';
    out << ",\"scaled_route\":";
    sr::write(out);
    out << ",\"render_width\":" << render_width() << ",\"render_height\":" << render_height();
    std::ostringstream scale;
    render_resolution::write(scale);
    auto json = scale.str();
    while (!json.empty() && (json.back() == '\n' || json.back() == '\r')) json.pop_back();
    out << ",\"resolution\":" << json;
    out << "}\n" << std::flush;
}
} // namespace mhw::continuous_dlaa
