#pragma once
#include <cstdint>
#include <cmath>

namespace mhw::dlss_sdk {
// The DLSS presets. Upscaling modes render below the output, DLAA at it.
enum class ReconstructionMode { performance, dlaa, balanced, quality };
constexpr bool upscales(ReconstructionMode m) noexcept {
    return m == ReconstructionMode::performance || m == ReconstructionMode::balanced ||
           m == ReconstructionMode::quality;
}
// The render scale each mode asks of the game. The game's setter quantizes it
// to 16-pixel steps of the basis width (render_resolution::plan), so DLSS gets
// the game's extent, not the SDK's optimal one.
constexpr float requested_scale(ReconstructionMode m) noexcept {
    switch (m) {
    case ReconstructionMode::quality: return 2.F / 3.F;
    case ReconstructionMode::balanced: return .58F;
    case ReconstructionMode::dlaa: return 1.F;
    case ReconstructionMode::performance: break;
    }
    return .5F;
}
// One scale q in [0.5, 1] for both axes, each truncated by the game:
// render = floor(output * q). Holds when the per-axis intervals [r/o, (r+1)/o)
// overlap and reach 0.5 (2r + 1 >= o). Exact 2:1 sizes satisfy it, as do a
// 3413x1440 scene at 1706x720, and the presets' quantized extents (Quality
// 1696x954 and Balanced 1472x828 of 2560x1440; render_resolution_tests checks
// the plans of common displays). The game multiplies in single-precision
// float (cvtdq2ps, mulss, cvttss2si; view rectangles at RVA 0x26352b0), so
// where output * q is exactly an integer it can truncate one pixel lower: the
// intervals' upper ends are closed (5120x2880 at Quality renders 3408x1916,
// not 1917; v0.24.0). A full side means q is exactly 1, so both are full.
constexpr bool single_scale(std::uint64_t rw, std::uint64_t rh, std::uint64_t ow, std::uint64_t oh) noexcept {
    if (!rw || !rh || rw > ow || rh > oh || 2 * rw + 1 < ow || 2 * rh + 1 < oh) return false;
    if (rw == ow || rh == oh) return rw == ow && rh == oh;
    return rw * oh <= (rh + 1) * ow && rh * ow <= (rw + 1) * oh;
}
struct Extent {
    unsigned width{}, height{};
    constexpr bool operator==(const Extent &) const = default;
};
// Raw allocations can contain an active top-left rectangle. Converted inputs
// are tightly sized to that rectangle; motion is expressed in its pixel units.
// The output extent is independent. Nonzero origins/dynamic resolution are not
// supported by this contract yet. The caller proves frame/rectangle identity.
struct Geometry {
    Extent allocation, render, output;
    ReconstructionMode mode{ReconstructionMode::dlaa};
    constexpr bool operator==(const Geometry &) const = default;
    static constexpr Geometry dlaa(unsigned w, unsigned h) noexcept {
        return {{w, h}, {w, h}, {w, h}, ReconstructionMode::dlaa};
    }
};
// Up to D3D12's largest 2D texture side (D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION).
// v0.24.0 replaced the 16,777,216-pixel ceiling with it and a video memory
// check (gpu_memory/budget.hpp), so 8K and DSR 4x fit when the memory does.
// The game packs texture heights in 14 bits (temporal_bindings/inputs.cpp), so
// its scenes are at most 16383 tall.
inline constexpr unsigned max_side = 16384;
constexpr bool valid_extent(Extent e) noexcept {
    return e.width >= 32 && e.height >= 32 && e.width <= max_side && e.height <= max_side;
}
constexpr bool valid_geometry(const Geometry &g) noexcept {
    if (!valid_extent(g.allocation) || !valid_extent(g.render) || !valid_extent(g.output) ||
        g.render.width > g.allocation.width || g.render.height > g.allocation.height ||
        g.render.width > g.output.width || g.render.height > g.output.height)
        return false;
    if (g.mode == ReconstructionMode::dlaa) return g.render == g.output;
    if (!upscales(g.mode) || g.render == g.output) return false;
    return single_scale(g.render.width, g.render.height, g.output.width, g.output.height);
}
// This upscale route is static: the render extent is the game's, quantized to
// 16-pixel steps (Quality at 2560 renders 1696 wide where the SDK's optimal is
// 1708). DLSS accepts any input extent within the minimum and maximum the SDK
// reports for the mode (its dynamic-resolution range); the render extent must
// lie inside it (one pixel of slack below, as the SDK rounds where the game
// truncates), inside sane reported bounds. Captured inputs are never resized.
constexpr bool render_supported(const Geometry &g, Extent optimal, Extent minimum, Extent maximum) noexcept {
    return valid_geometry(g) && valid_extent(optimal) && valid_extent(minimum) && valid_extent(maximum) &&
           minimum.width <= optimal.width && minimum.height <= optimal.height && maximum.width >= optimal.width &&
           maximum.height >= optimal.height && maximum.width <= g.output.width && maximum.height <= g.output.height &&
           g.render.width + 1 >= minimum.width && g.render.height + 1 >= minimum.height &&
           g.render.width <= maximum.width && g.render.height <= maximum.height;
}
struct PixelJitter {
    float x{}, y{};
};
// The native projection adds native/viewport to clip coordinates. Convert that
// measured basis to input raster pixels (+Y downward). The caller must prove
// that the projection, active render rectangle and SDK inputs share a frame.
// Do not assume the projection denominator equals the allocation or output.
inline bool projection_jitter(const Geometry &g, PixelJitter native, float viewport_width, float viewport_height,
                              PixelJitter &out) noexcept {
    out = {};
    // The game's table spans -0.5..0.5; full_pixel_jitter doubles it (options.hpp).
    if (!valid_geometry(g) || !std::isfinite(native.x) || !std::isfinite(native.y) || std::abs(native.x) > 1.F ||
        std::abs(native.y) > 1.F || !std::isfinite(viewport_width) || !std::isfinite(viewport_height))
        return false;
    const bool render = viewport_width == g.render.width && viewport_height == g.render.height;
    const bool allocation = viewport_width == g.allocation.width && viewport_height == g.allocation.height;
    if (!render && !allocation) return false;
    out = {native.x * static_cast<float>(g.render.width) / (2.F * viewport_width),
           -native.y * static_cast<float>(g.render.height) / (2.F * viewport_height)};
    return true;
}
} // namespace mhw::dlss_sdk
