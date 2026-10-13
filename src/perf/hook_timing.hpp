#pragma once
#include <windows.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <ostream>

namespace mhw::perf::hook_timing {
// Hook timing (v0.23.7, diagnostic: the alpha's "hook_timing = on"). Every
// 64th hooked call of a thread is timed with the time-stamp counter: the
// hook's own time (entry to exit, less the forwarded native call) and the
// forwarded call itself. Calls are estimated at 64 per timed call. Untimed
// calls cost one relaxed load and a thread-local increment; timed calls add
// three relaxed atomic adds. No locks.
enum Category : unsigned {
    observer,      // binding observer: every state setter on every command list
    reset_close,   // command-list Reset and Close
    execute,       // ExecuteCommandLists
    barrier,       // ResourceBarrier
    draw,          // DrawInstanced
    indexed,       // DrawIndexedInstanced (the stream route's draw admission)
    dispatch,      // Dispatch
    copy,          // CopyResource
    category_count
};
inline constexpr const char *names[category_count]{
    "observer", "reset_close", "execute", "barrier", "draw", "indexed", "dispatch", "copy"};
inline constexpr unsigned period = 64;

inline std::atomic<bool> enabled{};
struct Totals {
    std::atomic<std::uint64_t> timed{}, own{}, forwarded{};
};
inline std::array<Totals, category_count> totals{};
// Calibration: the time-stamp counter against QueryPerformanceCounter.
inline std::atomic<std::uint64_t> base_tsc{}, base_qpc{};

struct Sample {
    std::uint64_t start{}, forward_start{}, forwarded{};
    unsigned category{};
    bool on{};
};
inline Sample begin(Category c) noexcept {
    Sample s;
    s.category = c;
    if (!enabled.load(std::memory_order_relaxed)) return s;
    thread_local unsigned calls{};
    if ((++calls % period) != 0) return s;
    s.on = true;
    s.start = __rdtsc();
    return s;
}
inline void forward_begin(Sample &s) noexcept {
    if (s.on) s.forward_start = __rdtsc();
}
inline void forward_end(Sample &s) noexcept {
    if (s.on) s.forwarded += __rdtsc() - s.forward_start;
}
inline void end(const Sample &s) noexcept {
    if (!s.on) return;
    const auto total = __rdtsc() - s.start;
    auto &t = totals[s.category];
    t.timed.fetch_add(1, std::memory_order_relaxed);
    t.forwarded.fetch_add(s.forwarded, std::memory_order_relaxed);
    t.own.fetch_add(total > s.forwarded ? total - s.forwarded : 0, std::memory_order_relaxed);
}
// For hooks without SEH blocks.
class Scope {
public:
    explicit Scope(Category c) noexcept : sample(begin(c)) {}
    ~Scope() { end(sample); }
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
    Sample sample;
};
struct ForwardScope {
    explicit ForwardScope(Sample &s) noexcept : sample(s) { forward_begin(sample); }
    ~ForwardScope() { forward_end(sample); }
    ForwardScope(const ForwardScope &) = delete;
    ForwardScope &operator=(const ForwardScope &) = delete;
    Sample &sample;
};

inline void enable() noexcept {
    LARGE_INTEGER q{};
    QueryPerformanceCounter(&q);
    base_qpc.store(static_cast<std::uint64_t>(q.QuadPart));
    base_tsc.store(__rdtsc());
    enabled.store(true);
}
// One "hook_timing" event for the window since the last call: per category,
// estimated calls per frame and the hook's own and forwarded microseconds per
// frame. The writer is one thread (the worker).
inline void write(std::ostream &out, std::uint64_t frames) {
    if (!enabled.load()) return;
    static std::array<std::array<std::uint64_t, 3>, category_count> last{};
    static std::uint64_t last_frames{};
    LARGE_INTEGER q{}, f{};
    QueryPerformanceCounter(&q);
    QueryPerformanceFrequency(&f);
    const auto qpc = static_cast<std::uint64_t>(q.QuadPart) - base_qpc.load();
    const auto tsc = __rdtsc() - base_tsc.load();
    // Ticks of the time-stamp counter per microsecond.
    const double per_us = qpc && f.QuadPart ? double(tsc) / (double(qpc) * 1e6 / double(f.QuadPart)) : 0.0;
    const auto window = frames > last_frames ? frames - last_frames : 0;
    out << "{\"event\":\"hook_timing\",\"frames\":" << window << ",\"tsc_mhz\":" << per_us << ",\"period\":" << period
        << ",\"categories\":{";
    for (unsigned i = 0; i < category_count; ++i) {
        const std::array<std::uint64_t, 3> now{totals[i].timed.load(), totals[i].own.load(), totals[i].forwarded.load()};
        const auto timed = now[0] - last[i][0], own = now[1] - last[i][1], forwarded = now[2] - last[i][2];
        last[i] = now;
        const double scale = window && per_us > 0 ? double(period) / double(window) : 0.0;
        out << (i ? "," : "") << '"' << names[i] << "\":{\"calls_per_frame\":" << double(timed) * scale
            << ",\"own_us_per_frame\":" << (per_us > 0 ? double(own) / per_us * scale : 0.0)
            << ",\"forwarded_us_per_frame\":" << (per_us > 0 ? double(forwarded) / per_us * scale : 0.0) << '}';
    }
    out << "}}\n";
    last_frames = frames;
}
}  // namespace mhw::perf::hook_timing
