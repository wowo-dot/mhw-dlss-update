#pragma once
#include "filter_shader.hpp"
#include <span>
#include <vector>

namespace mhw::scaled_temporal {
// Five captured hidden-surface material variants, gated by complete SHA-256.
// Native SV_POSITION.xy reads address render-space depth/G-buffer allocations.
// Remap those reads for an output-sized viewport; preserve z, interpolation,
// material calculations, root bindings and shader signatures. No game bytecode
// is embedded or changed in place. Caller must admit the hidden-surface draw,
// its current bindings/resources and its enlarged viewport separately.
bool known_silhouette_shader(std::span<const std::byte>) noexcept;
std::vector<std::byte> make_silhouette_shader(std::span<const std::byte>,FilterGeometry);
}
