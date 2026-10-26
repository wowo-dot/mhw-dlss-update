#pragma once
// Start-up chaos (0.31.1, diagnostic: the ini's "chaos = on"). marie-3 and
// marie-4 were start-up races that only a slow machine showed: a late hook
// registration and a renderer rebuild during setup. With chaos on, each
// start-up step sleeps a random 0 to 500 ms first, so such races can show on
// a fast machine. The first 32 delays are kept for the log.
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <ostream>

namespace mhw::perf::chaos {
inline constexpr unsigned max_ms = 500, kept = 32;
inline std::atomic<bool> enabled{};
struct Delay {
    const char *where{};
    unsigned ms{};
};
inline std::array<Delay, kept> delays{};
inline std::atomic<unsigned> count{};

// Pure (tests): xorshift64 from a nonzero state, then a delay in [0, max_ms].
inline unsigned next(std::uint64_t &state) noexcept {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return static_cast<unsigned>(state % (max_ms + 1));
}
// A start-up step about to run (any thread).
inline void delay(const char *where) noexcept {
    if (!enabled.load(std::memory_order_relaxed)) return;
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    std::uint64_t state = static_cast<std::uint64_t>(now.QuadPart) | 1;
    next(state);
    const unsigned ms = next(state);
    if (const auto i = count.fetch_add(1); i < kept) delays[i] = {where, ms};
    Sleep(ms);
}
inline void write(std::ostream &out) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    const auto n = count.load();
    out << "{\"event\":\"chaos\",\"count\":" << n << ",\"delays\":[";
    for (unsigned i = 0; i < n && i < kept; ++i)
        out << (i ? "," : "") << "[\"" << (delays[i].where ? delays[i].where : "") << "\"," << delays[i].ms << ']';
    out << "]}\n" << std::flush;
}
} // namespace mhw::perf::chaos
