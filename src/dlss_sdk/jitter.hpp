#pragma once
#include "geometry.hpp"
#include <cmath>
#include <cstdint>

// The camera jitter while DLSS draws (0.31.1). The game's own table (sScene
// +0xe6a0, filled by 0x227ff00 with Halton(2,3) indices 20 to 39 minus 0.5)
// moves the image by half its value in render pixels: the projection adds
// value / viewport to clip space (projection_snapshot.cpp's relation), and a
// pixel is 2 / viewport there. Its samples cover only the middle half of each
// pixel, in 20 phases. DLSS wants offsets over the whole pixel, -0.5 to 0.5
// render pixels, from a Halton(2,3) sequence of 8 x (output / render)^2 phases
// (DLSS Programming Guide 3.7). Pure (tests/jitter_tests.cpp).
namespace mhw::dlss_sdk {
// The radical inverse of `index` in `base`: the Halton sequence, in [0, 1).
constexpr float halton(std::uint32_t index, std::uint32_t base) noexcept {
    float result = 0.F, fraction = 1.F;
    while (index) {
        fraction /= static_cast<float>(base);
        result += fraction * static_cast<float>(index % base);
        index /= base;
    }
    return result;
}
// The phases for a geometry: 8 x the output's pixels per render pixel, rounded
// up (DLAA 8; at 2560x1440 Quality 19, Balanced 25, Performance 32); 0 for a
// geometry that isn't valid.
inline constexpr unsigned base_phases = 8, max_phases = 128;
inline unsigned jitter_phases(const Geometry &g) noexcept {
    if (!valid_geometry(g)) return 0;
    const double ratio = (static_cast<double>(g.output.width) * g.output.height) /
                         (static_cast<double>(g.render.width) * g.render.height);
    const auto phases = static_cast<unsigned>(std::ceil(base_phases * ratio - 1e-6));
    return phases < base_phases ? base_phases : phases > max_phases ? max_phases : phases;
}
// A frame's offset in render pixels, each axis in [-0.5, 0.5): Halton(2,3)
// from index 1 (index 0 is the corner).
struct JitterOffset {
    float x{}, y{};
};
constexpr JitterOffset jitter_offset(std::uint64_t frame, unsigned phases) noexcept {
    if (!phases) return {};
    const auto index = static_cast<std::uint32_t>(1 + frame % phases);
    return {halton(index, 2) - .5F, halton(index, 3) - .5F};
}
// The value the game's camera takes for an offset (sCamera +0x38): its
// projection moves the image by half of it.
constexpr PixelJitter camera_jitter(JitterOffset o) noexcept {
    return {2.F * o.x, 2.F * o.y};
}
} // namespace mhw::dlss_sdk
