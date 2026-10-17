#pragma once
#include "frame_policy.hpp"
#include <cstdint>
#include <limits>

namespace mhw::reflex {
enum class StampReadStatus : unsigned { stable, changing, unreadable, invalid_bank };
struct StampRead {
    FrameStamp value;
    std::uint64_t epoch_after{};
    unsigned bank_after{};
    StampReadStatus status{StampReadStatus::unreadable};
};
// Each read remains bounded and guarded by the caller. A worker can intersect
// a native counter publication; that observation is not a coherent frame.
// Hook-thread callers still reject it immediately rather than retrying SDK work.
template <class Read> StampRead read_frame_stamp(Read &&read, std::uintptr_t renderer) noexcept {
    StampRead s;
    if (!renderer || renderer > (std::numeric_limits<std::uintptr_t>::max)() - 0x1f210) return s;
    if (!read(renderer + 0x1f208, s.value.epoch) || !read(renderer + 0x1f210, s.value.temporal) ||
        !read(renderer + 0x1f200, s.value.bank))
        return s;
    if (s.value.bank >= 2) {
        s.status = StampReadStatus::invalid_bank;
        return s;
    }
    if (!read(renderer + 0x1f208, s.epoch_after) || !read(renderer + 0x1f200, s.bank_after)) return s;
    if (s.bank_after >= 2) {
        s.status = StampReadStatus::invalid_bank;
        return s;
    }
    s.value.valid = s.value.epoch == s.epoch_after && s.value.bank == s.bank_after;
    s.status = s.value.valid ? StampReadStatus::stable : StampReadStatus::changing;
    return s;
}
inline constexpr unsigned anchor_stamp_attempt_limit = 64;
inline constexpr std::uint64_t anchor_stamp_timeout_ms = 100;
constexpr bool retry_anchor_stamp(StampReadStatus status, unsigned attempts, std::uint64_t elapsed_ms) noexcept {
    return status == StampReadStatus::changing && attempts > 0 && attempts < anchor_stamp_attempt_limit &&
           elapsed_ms < anchor_stamp_timeout_ms;
}
} // namespace mhw::reflex
