#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace mhw::temporal_gpu_inputs {
inline constexpr std::uint64_t input_byte_limit = 16ULL * 1024 * 1024;
struct Frame {
    unsigned width{}, height{};
    std::span<const std::uint32_t> color, depth, motion;
};
// Input files are tightly packed copies, not engine descriptor memory.
bool validate(const Frame &, std::string &reason) noexcept;
std::array<float, 4> color_reference(std::uint32_t) noexcept;
std::array<float, 2> motion_reference(std::uint32_t, unsigned, unsigned) noexcept;
}
