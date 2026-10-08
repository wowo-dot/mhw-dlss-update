#pragma once
#include "screen_shader.hpp"
#include <array>
#include <span>
#include <string_view>

namespace mhw::scaled_temporal {
// Approved single-pass screen filters (docs/research/shader-census-result.md).
// Identity, binding contract and generator edits come from the shader census
// (game-43); each entry passed tools/screen_shader_fixture (exact native
// oracle or render-lattice comparison, WARP and RTX 5090). A filter is admitted
// only by its exact pixel- and vertex-shader bytes; its PS binding words must
// match exactly at plan and draw time.
struct ScreenFilter {
    std::string_view technique;
    std::string_view ps_sha256;unsigned ps_size{};
    std::string_view vs_sha256;unsigned vs_size{};
    unsigned scene_register{};               // tBaseMap's t-register (the chain input)
    std::array<unsigned,12> words{};unsigned word_count{};  // PS binding words, native order
    unsigned position_reads{},scene_samples{},scene_loads{}; // generator edits
};
std::span<const ScreenFilter> screen_filters() noexcept;
const ScreenFilter *screen_filter(std::string_view ps_sha256) noexcept;
// The approved entry for these exact PS and VS bytes, or null.
const ScreenFilter *screen_filter(std::span<const std::byte> ps,std::span<const std::byte> vs);
// PS t-registers the entry binds (bit per register), from its words.
unsigned screen_filter_textures(const ScreenFilter &) noexcept;
// The generated full-output PS for an approved filter; throws unless the bytes
// are the approved ones and the generator makes exactly the recorded edits.
std::vector<std::byte> make_screen_filter_shader(std::span<const std::byte> native,FilterGeometry);
}
