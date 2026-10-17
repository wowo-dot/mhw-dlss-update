#pragma once
#include <cstdint>
#include <string_view>

namespace mhw::reflex {
// Generated frames per host frame for a total multiplier (2x = 1), limited to
// the reported capability. An unknown capability (0) admits ordinary FG only.
// 0 means the multiplier is unusable.
constexpr std::uint32_t generated_frames_for(std::uint32_t multiplier, std::uint32_t maximum) noexcept {
    if (multiplier < 2 || multiplier > 8) return 0;
    if (maximum == 0) return 1;
    return multiplier - 1 < maximum ? multiplier - 1 : maximum;
}
// "2x".."8x" (case-insensitive x); 0 for anything else.
constexpr std::uint32_t parse_multiplier(std::string_view text) noexcept {
    if (text.size() != 2 || (text[1] != 'x' && text[1] != 'X') || text[0] < '2' || text[0] > '8') return 0;
    return static_cast<std::uint32_t>(text[0] - '0');
}
}
