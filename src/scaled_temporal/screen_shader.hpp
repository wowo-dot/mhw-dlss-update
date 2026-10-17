#pragma once
#include "filter_shader.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace mhw::scaled_temporal {
// Generated full-output variant of a single-pass screen filter PS (shader
// census family "filter VS"). The native draw ran at render size; the variant
// runs over the whole output after DLSS. Three rules, applied to the token
// stream of an SM5 pixel shader:
//
//  1. Every read of the raster position (SV_Position input) is replaced by
//     the position on the render lattice (v0.xy * render/output), so auxiliary
//     render-size inputs, CBScreen and projection math stay in render space.
//     z/w are copied as declared; a neg/abs source modifier stays attached.
//     Also used alone for the hidden-surface silhouette (silhouette_shader).
//  2. Every UV sample of the scene register (sample, sample_l, sample_b,
//     sample_c, sample_c_lz, gather4) is expanded by output/render: native UVs
//     address the render rectangle; the scene now covers the whole output.
//  3. A load of the scene register is admitted only at the raster position
//     (ftoi r.xy, v0.xy; mov r.zw, 0 earlier in the same block) and reads the
//     full-output texel instead.
//
// Anything else touching the scene register or the position input (gradient
// or offset samples, resinfo, sample_info, lod, ld_ms, eval_*, relative
// indexing) throws: the shader is unsupported, never approximated. A filter
// that reads no scene (UINT_MAX) gets only rule 1.
// How each texture register is read: 1 sampled at a coordinate (UV), 2 loaded
// at the raster position, 4 any other access. Diagnostic and for fixtures.
enum ScreenAccess : unsigned char { access_sample = 1, access_raster = 2, access_other = 4 };
struct ScreenShaderEdits {
    unsigned position_reads{}, scene_samples{}, scene_loads{};
    std::array<unsigned char, 128> access{};
    bool operator==(const ScreenShaderEdits &) const = default;
};
std::vector<std::byte> make_screen_shader(std::span<const std::byte> native, FilterGeometry, unsigned scene_register,
                                          ScreenShaderEdits &);
} // namespace mhw::scaled_temporal
