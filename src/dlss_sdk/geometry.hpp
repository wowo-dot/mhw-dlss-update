#pragma once
#include <cstdint>
#include <cmath>

namespace mhw::dlss_sdk {
enum class ReconstructionMode { performance, dlaa };
struct Extent {
    unsigned width{},height{};
    constexpr bool operator==(const Extent &) const =default;
};
// Raw allocations can contain an active top-left rectangle. Converted inputs
// are tightly sized to that rectangle; motion is expressed in its pixel units.
// The output extent is independent. Nonzero origins/dynamic resolution are not
// supported by this contract yet. The caller proves frame/rectangle identity.
struct Geometry {
    Extent allocation,render,output;
    ReconstructionMode mode{ReconstructionMode::dlaa};
    constexpr bool operator==(const Geometry &) const =default;
    static constexpr Geometry dlaa(unsigned w,unsigned h) noexcept {
        return {{w,h},{w,h},{w,h},ReconstructionMode::dlaa};
    }
};
// Up to 16,777,216 pixels: 3840x2160, 5120x1440 and 5120x2880 fit; 8K does not.
constexpr std::uint64_t max_pixels=16777216;
constexpr bool valid_extent(Extent e) noexcept {
    return e.width>=32&&e.height>=32&&e.width<=8192&&e.height<=8192&&
        std::uint64_t(e.width)*e.height<=max_pixels;
}
constexpr bool valid_geometry(const Geometry &g) noexcept {
    if(!valid_extent(g.allocation)||!valid_extent(g.render)||!valid_extent(g.output)||
       g.render.width>g.allocation.width||g.render.height>g.allocation.height||
       g.render.width>g.output.width||g.render.height>g.output.height)return false;
    if(g.mode==ReconstructionMode::dlaa)return g.render==g.output;
    if(g.mode!=ReconstructionMode::performance)return false;
    return g.render.width<g.output.width&&g.render.height<g.output.height&&
        std::uint64_t(g.render.width)*g.output.height==std::uint64_t(g.render.height)*g.output.width;
}
// This first upscale route is static. Accept only the SDK's exact recommended
// extent, with sane reported bounds; do not silently resize captured inputs.
constexpr bool optimal_matches(const Geometry &g,Extent optimal,Extent minimum,Extent maximum) noexcept {
    return valid_geometry(g)&&valid_extent(optimal)&&valid_extent(minimum)&&valid_extent(maximum)&&
        g.render==optimal&&minimum.width<=optimal.width&&minimum.height<=optimal.height&&
        maximum.width>=optimal.width&&maximum.height>=optimal.height&&
        maximum.width<=g.output.width&&maximum.height<=g.output.height;
}
struct PixelJitter {float x{},y{};};
// The native projection adds native/viewport to clip coordinates. Convert that
// measured basis to input raster pixels (+Y downward). The caller must prove
// that the projection, active render rectangle and SDK inputs share a frame.
// Do not assume the projection denominator equals the allocation or output.
inline bool projection_jitter(const Geometry &g,PixelJitter native,float viewport_width,float viewport_height,PixelJitter &out) noexcept {
    out={};
    if(!valid_geometry(g)||!std::isfinite(native.x)||!std::isfinite(native.y)||
       std::abs(native.x)>.5F||std::abs(native.y)>.5F||
       !std::isfinite(viewport_width)||!std::isfinite(viewport_height))return false;
    const bool render=viewport_width==g.render.width&&viewport_height==g.render.height;
    const bool allocation=viewport_width==g.allocation.width&&viewport_height==g.allocation.height;
    if(!render&&!allocation)return false;
    out={native.x*static_cast<float>(g.render.width)/(2.F*viewport_width),
        -native.y*static_cast<float>(g.render.height)/(2.F*viewport_height)};
    return true;
}
}
