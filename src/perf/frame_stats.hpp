#pragma once
#include <array>
#include <atomic>
#include <cstdint>

namespace mhw::perf {
// Host frame intervals (game Present to game Present, so generated frames are
// not counted) in 0.25 ms buckets up to 100 ms, an overflow bucket and the
// maximum. The presenting thread writes; a worker takes a window every few
// seconds. Counters are relaxed: a frame at a window edge lands in either one.
class FrameStats {
public:
    static constexpr unsigned bucket_us = 250, bucket_count = 400;
    struct Window {
        std::uint64_t frames{}, total_us{}, max_us{}, over_50ms{};
        std::uint64_t p50_us{}, p95_us{}, p99_us{};
    };
    void interval(std::uint64_t us) noexcept {
        const auto bucket = us / bucket_us;
        buckets_[bucket < bucket_count ? bucket : bucket_count].fetch_add(1, std::memory_order_relaxed);
        frames_.fetch_add(1, std::memory_order_relaxed);
        total_us_.fetch_add(us, std::memory_order_relaxed);
        if (us > 50000) over_50ms_.fetch_add(1, std::memory_order_relaxed);
        auto seen = max_us_.load(std::memory_order_relaxed);
        while (us > seen && !max_us_.compare_exchange_weak(seen, us, std::memory_order_relaxed)) {}
    }
    // qpc/frequency from QueryPerformanceCounter/Frequency. The first present
    // after construction or restart() only sets the reference point.
    void present(std::int64_t qpc, std::int64_t frequency) noexcept {
        const auto previous = last_.exchange(qpc, std::memory_order_relaxed);
        if (!previous || qpc <= previous || frequency <= 0) return;
        const auto ticks = static_cast<std::uint64_t>(qpc - previous);
        const auto f = static_cast<std::uint64_t>(frequency);
        interval(ticks / f * 1000000 + ticks % f * 1000000 / f);
    }
    void restart() noexcept { last_.store(0, std::memory_order_relaxed); }
    // Percentiles are the upper edge of the bucket holding the rank; a rank in
    // the overflow bucket reports the window maximum.
    Window take() noexcept {
        Window w;
        std::array<std::uint64_t, bucket_count + 1> counts{};
        std::uint64_t counted{};
        for (unsigned i = 0; i <= bucket_count; ++i) counted += counts[i] = buckets_[i].exchange(0, std::memory_order_relaxed);
        w.frames = frames_.exchange(0, std::memory_order_relaxed);
        w.total_us = total_us_.exchange(0, std::memory_order_relaxed);
        w.max_us = max_us_.exchange(0, std::memory_order_relaxed);
        w.over_50ms = over_50ms_.exchange(0, std::memory_order_relaxed);
        const auto rank = [&](std::uint64_t per_mille) -> std::uint64_t {
            if (!counted) return 0;
            const auto wanted = (counted * per_mille + 999) / 1000;
            std::uint64_t seen{};
            for (unsigned i = 0; i < bucket_count; ++i)
                if ((seen += counts[i]) >= wanted) return std::uint64_t(i + 1) * bucket_us;
            return w.max_us;
        };
        w.p50_us = rank(500);
        w.p95_us = rank(950);
        w.p99_us = rank(990);
        return w;
    }

private:
    std::array<std::atomic<std::uint64_t>, bucket_count + 1> buckets_{};
    std::atomic<std::uint64_t> frames_{}, total_us_{}, max_us_{}, over_50ms_{};
    std::atomic<std::int64_t> last_{};
};

// One instance per DLL: the routed host Present feeds it, the probe worker logs it.
inline FrameStats host_frames;
// Thread of the latest host Present (the game's render-submission thread).
inline std::atomic<unsigned long> present_thread{};
}  // namespace mhw::perf
