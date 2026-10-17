#pragma once
#include <atomic>
#include <cstdint>
#include <ostream>
namespace mhw::continuous_dlaa {
// Reconstruction readiness is independent of renderer ownership. A latched SDK
// failure never becomes native AA or clears merely because inputs return.
class ReconstructionState {
    std::atomic<unsigned> failure_{};
    std::atomic<bool> waiting_{};
    std::atomic<std::uint64_t> suspended_{}, resumed_{};

public:
    bool failed() const noexcept { return failure_.load() != 0; }
    bool waiting() const noexcept { return waiting_.load(); }
    bool jitter() const noexcept { return !failed() && !waiting(); }
    void suspend() noexcept {
        if (!waiting_.exchange(true)) ++suspended_;
    }
    void latch(unsigned reason) noexcept {
        unsigned zero{};
        failure_.compare_exchange_strong(zero, reason);
        suspend();
    }
    bool complete() noexcept {
        if (failed()) return false;
        if (waiting_.exchange(false)) ++resumed_;
        return true;
    }
    void write(std::ostream &o) const {
        o << "{\"schema\":1,\"policy\":\"suspend-or-latch-unfiltered-v1\",\"failure\":" << failure_.load()
          << ",\"waiting\":" << waiting_.load() << ",\"suspensions\":" << suspended_.load()
          << ",\"resumptions\":" << resumed_.load() << '}';
    }
};
} // namespace mhw::continuous_dlaa
