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
    // One scale for both axes, each truncated by the game: render = floor(output*q).
    // That holds when the intervals [r/o,(r+1)/o) of both axes overlap. Exact
    // 2:1 sizes satisfy it; so does a 3413x1440 scene rendered at 1706x720.
    const std::uint64_t rw=g.render.width,rh=g.render.height,ow=g.output.width,oh=g.output.height;
    return rw<ow&&rh<oh&&rw*oh<(rh+1)*ow&&rh*ow<(rw+1)*oh;
}
// This upscale route is static: the render extent is the game's. Accept the
// SDK's recommended extent within one pixel per axis (the SDK rounds a half
// pixel where the game truncates), inside sane reported bounds; do not
// silently resize captured inputs.
constexpr bool optimal_matches(const Geometry &g,Extent optimal,Extent minimum,Extent maximum) noexcept {
    const auto within_one=[](unsigned a,unsigned b){return a+1>=b&&b+1>=a;};
    return valid_geometry(g)&&valid_extent(optimal)&&valid_extent(minimum)&&valid_extent(maximum)&&
        within_one(g.render.width,optimal.width)&&within_one(g.render.height,optimal.height)&&
        minimum.width<=optimal.width&&minimum.height<=optimal.height&&
        maximum.width>=optimal.width&&maximum.height>=optimal.height&&
        maximum.width<=g.output.width&&maximum.height<=g.output.height&&
        g.render.width+1>=minimum.width&&g.render.height+1>=minimum.height&&
        g.render.width<=maximum.width&&g.render.height<=maximum.height;
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
