// The session log's recurring events: swap-chain lifecycle calls, frame
// timing, Reflex latency, the presentation buffers the crash log also writes,
// and the text helpers they share.
#include "plugin.hpp"
#include "../frame_generation/crash_log.hpp"
#include "../perf/frame_stats.hpp"
#include "../perf/toggles.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>

namespace mhw::plugin {
std::string code_site(std::uintptr_t address) {
    char text[320]{};
    mhw::frame_generation::crash_log::code_site(text, static_cast<int>(sizeof text), address);
    return text;
}
// Swap-chain view the host received through the routed calls (diagnostic).
int fg_buffers_json(char *out, int size) noexcept {
    mhw::frame_generation::PresentationBuffers b{};
    if (!mhw::reflex::scene_presentation_buffers(b)) return 0;
    return std::snprintf(
        out, static_cast<size_t>(size),
        "{\"generation\":%llu,\"known_mask\":%u,\"game_index\":%u,\"game_index_max\":%u,\"game_index_calls\":%llu,"
        "\"out_of_range\":%llu,\"conflicts\":%llu,\"foreign\":%llu,\"present_calls\":%llu,\"present_ok\":%llu,"
        "\"present_other\":%llu,\"first_results\":[%u,%u,%u,%u],\"first_result_at\":[%llu,%llu,%llu,%llu]}",
        static_cast<unsigned long long>(b.generation), b.known_mask, b.game_index, b.game_index_max,
        static_cast<unsigned long long>(b.game_index_calls), static_cast<unsigned long long>(b.out_of_range),
        static_cast<unsigned long long>(b.conflicts), static_cast<unsigned long long>(b.foreign),
        static_cast<unsigned long long>(b.present_calls), static_cast<unsigned long long>(b.present_ok),
        static_cast<unsigned long long>(b.present_other), b.present_results[0], b.present_results[1],
        b.present_results[2], b.present_results[3], static_cast<unsigned long long>(b.present_result_at[0]),
        static_cast<unsigned long long>(b.present_result_at[1]),
        static_cast<unsigned long long>(b.present_result_at[2]),
        static_cast<unsigned long long>(b.present_result_at[3]));
}
void log_fg_buffers(std::ostream &out, const char *phase) {
    char text[512]{};
    const int n = fg_buffers_json(text, sizeof text);
    out << "{\"event\":\"fg_buffers\",\"phase\":\"" << phase << "\",\"buffers\":" << (n > 0 ? text : "null") << "}\n"
        << std::flush;
}
namespace {
std::uint64_t lifecycle_dropped_logged{};
}
// Host swap-chain lifecycle calls recorded by the presentation hooks.
void log_lifecycle(std::ostream &out) {
    if (!presentation) return;
    std::array<mhw::reflex::LifecycleEvent, mhw::reflex::Presentation::lifecycle_capacity> events{};
    std::uint64_t dropped{};
    const auto count = presentation->drain_lifecycle(events, dropped);
    if (!count && dropped == lifecycle_dropped_logged) return;
    LARGE_INTEGER frequency{};
    QueryPerformanceFrequency(&frequency);
    constexpr const char *names[]{"",
                                  "create_swap_chain",
                                  "create_for_hwnd",
                                  "create_for_core_window",
                                  "",
                                  "",
                                  "set_fullscreen_state",
                                  "",
                                  "resize_buffers",
                                  "",
                                  "",
                                  "resize_buffers1",
                                  "resize_target"};
    for (unsigned i = 0; i < count; ++i) {
        const auto &e = events[i];
        // A fullscreen change: a switch refused in exclusive fullscreen may work now.
        if (e.method == 6) switch_unreached = -1;
        out << "{\"event\":\"swap_lifecycle\",\"sequence\":" << e.sequence << ",\"method\":" << e.method
            << ",\"name\":\"" << (e.method < std::size(names) ? names[e.method] : "") << "\",\"thread\":" << e.thread
            << ",\"host_thread\":" << e.host_thread << ",\"current_chain\":" << e.current_chain
            << ",\"routed\":" << e.routed << ",\"guard_off\":" << e.guard_off << ",\"result\":" << e.result
            << ",\"qpc\":" << e.began_qpc << ",\"ms\":"
            << (frequency.QuadPart ? double(e.ended_qpc - e.began_qpc) * 1000.0 / double(frequency.QuadPart) : 0.0)
            << ",\"args\":[";
        for (unsigned n = 0; n < e.args.size(); ++n) out << (n ? "," : "") << e.args[n];
        out << "],\"after\":";
        if (e.described)
            out << "{\"width\":" << e.width << ",\"height\":" << e.height << ",\"format\":" << e.format
                << ",\"buffers\":" << e.buffers << ",\"flags\":" << e.flags << ",\"windowed\":" << e.windowed << "}";
        else
            out << "null";
        out << "}\n";
    }
    if (dropped != lifecycle_dropped_logged) {
        out << "{\"event\":\"swap_lifecycle_dropped\",\"dropped\":" << dropped << "}\n";
        lifecycle_dropped_logged = dropped;
    }
    out << std::flush;
}
// Overhead measurements (v0.12): host frame intervals for the last window,
// labelled with the active perf switches. Armed runs add the driver's own
// per-frame report (Reflex markers), which separates CPU and GPU time.
void log_frames(std::ostream &out, const char *phase) {
    const auto w = mhw::perf::host_frames.take();
    out << "{\"event\":\"frame_stats\",\"phase\":\"" << phase << "\",\"perf\":" << mhw::perf::toggles.load()
        << ",\"frames\":" << w.frames << ",\"total_us\":" << w.total_us
        << ",\"fps\":" << (w.total_us ? double(w.frames) * 1e6 / double(w.total_us) : 0.0) << ",\"p50_us\":" << w.p50_us
        << ",\"p95_us\":" << w.p95_us << ",\"p99_us\":" << w.p99_us << ",\"max_us\":" << w.max_us
        << ",\"over_50ms\":" << w.over_50ms << "}\n"
        << std::flush;
}
// The first Reflex frame fault: its reason and context (LiveFrameStats::Fault).
void write_reflex_fault(std::ostream &out, const mhw::reflex::LiveFrameStats &frames) {
    const auto &f = frames.fault;
    out << "\"reflex_fault\":\"" << (frames.fault_reason ? frames.fault_reason : "")
        << "\",\"reflex_fault_detail\":{\"call\":" << f.call << ",\"thread\":" << f.thread << ",\"owner\":" << f.owner
        << ",\"links\":" << f.links << ",\"code\":" << f.code << ",\"stage\":" << f.stage << ",\"frame\":" << f.frame
        << ",\"in_frame\":" << f.in_frame << '}';
}
namespace {
template <std::size_t N> std::uint64_t median_of(std::array<std::uint64_t, N> &values, unsigned count) {
    if (!count) return 0;
    std::nth_element(values.begin(), values.begin() + count / 2, values.begin() + count);
    return values[count / 2];
}
template <std::size_t N> std::uint64_t max_of(const std::array<std::uint64_t, N> &values, unsigned count) {
    return count ? *std::max_element(values.begin(), values.begin() + count) : 0;
}
} // namespace
void log_latency(std::ostream &out) {
    auto state = std::make_unique<sl::ReflexState>();
    sl::Result result{};
    {
        std::lock_guard lock(sdk_mutex);
        result = sdk.state(*state);
    }
    out << "{\"event\":\"latency\",\"result\":" << static_cast<unsigned>(result)
        << ",\"available\":" << state->latencyReportAvailable;
    if (result == sl::Result::eOk && state->latencyReportAvailable) {
        std::array<const sl::ReflexReport *, sl::kReflexFrameReportCount> frames{};
        unsigned n{};
        for (const auto &f : state->frameReport)
            if (f.frameID && f.simEndTime >= f.simStartTime && f.renderSubmitEndTime >= f.renderSubmitStartTime &&
                f.presentEndTime >= f.presentStartTime && f.driverEndTime >= f.driverStartTime &&
                f.osRenderQueueEndTime >= f.osRenderQueueStartTime && f.gpuRenderEndTime >= f.gpuRenderStartTime)
                frames[n++] = &f;
        std::sort(frames.begin(), frames.begin() + n, [](auto a, auto b) { return a->frameID < b->frameID; });
        // Microseconds as the driver reports them; interval = consecutive frames' present starts.
        std::array<std::array<std::uint64_t, sl::kReflexFrameReportCount>, 9> v{};
        std::array<unsigned, 9> c{};
        for (unsigned i = 0; i < n; ++i) {
            const auto &f = *frames[i];
            if (i && f.frameID == frames[i - 1]->frameID + 1 && f.presentStartTime >= frames[i - 1]->presentStartTime)
                v[0][c[0]++] = f.presentStartTime - frames[i - 1]->presentStartTime;
            v[1][c[1]++] = f.simEndTime - f.simStartTime;
            v[2][c[2]++] = f.renderSubmitEndTime - f.renderSubmitStartTime;
            v[3][c[3]++] = f.presentEndTime - f.presentStartTime;
            v[4][c[4]++] = f.driverEndTime - f.driverStartTime;
            v[5][c[5]++] = f.osRenderQueueEndTime - f.osRenderQueueStartTime;
            v[6][c[6]++] = f.gpuRenderEndTime - f.gpuRenderStartTime;
            v[7][c[7]++] = f.gpuActiveRenderTimeUs;
            v[8][c[8]++] = f.gpuFrameTimeUs;
        }
        constexpr std::array<const char *, 9> names{"interval", "simulation", "submit",     "present",  "driver",
                                                    "queue",    "gpu",        "gpu_active", "gpu_frame"};
        out << ",\"frames\":" << n;
        for (unsigned k = 0; k < names.size(); ++k) {
            const auto top = max_of(v[k], c[k]);
            out << ",\"" << names[k] << "\":[" << median_of(v[k], c[k]) << ',' << top << ']';
        }
    }
    out << "}\n" << std::flush;
}
// Live perf switches: "<log>.perf-N" holds "<creation> <mask>", sequenced and
// generation-checked like the multiplier markers. The window before a switch
// is logged first, so every frame_stats line covers one mask.
void poll_perf(std::ostream &out, const std::filesystem::path &folder, const std::string &stem, std::uint64_t creation,
               unsigned &requests) {
    const auto marker = folder / (stem + ".perf-" + std::to_string(requests + 1));
    if (!std::filesystem::exists(marker) || std::filesystem::file_size(marker) > 64) return;
    std::ifstream input(marker);
    std::uint64_t generation{};
    unsigned mask{};
    input >> generation >> mask;
    const bool accepted = bool(input) && generation == creation && mask <= mhw::perf::toggle_mask;
    const auto previous = mhw::perf::toggles.load();
    ++requests;
    if (accepted) {
        log_frames(out, "switch");
        mhw::perf::toggles.store(mask);
    }
    out << "{\"event\":\"perf_toggles\",\"request\":" << requests << ",\"mask\":" << mask
        << ",\"previous\":" << previous << ",\"accepted\":" << accepted << "}\n"
        << std::flush;
}
} // namespace mhw::plugin
