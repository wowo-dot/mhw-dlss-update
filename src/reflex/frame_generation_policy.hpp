#pragma once
#include <cstdint>

namespace mhw::reflex {
constexpr bool generated_frames_supported(std::uint32_t requested, std::uint32_t maximum) noexcept {
    return requested != 0 && maximum != 0 && requested <= maximum;
}
}
