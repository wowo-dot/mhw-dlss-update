#pragma once
#include "filter_shader.hpp"
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace mhw::scaled_temporal {
// Hidden-surface material pixel shaders: player and Uber materials whose
// CBMhMaterialGlobal hidden-surface branch draws the see-through hunter after
// grading. Admitted by complete SHA-256 and size against an allowlist generated
// from the shader census, each entry verified by tools/silhouette_shader_fixture
// (silhouette_allowlist.inc, tools/gen_silhouette_allowlist.py). The adaptation
// is the screen-shader generator's rule 1: every raster-position read moves to
// the render lattice (x/y scaled by render/output, z kept), so G-buffer mask,
// depth and light-tile lookups stay in render space while coverage spans the
// output. No game bytecode is embedded or changed in place. The caller admits
// the draw, its bindings/resources and its enlarged viewport separately.
using ShaderHash = std::array<unsigned char, 32>;
bool shader_sha256(std::span<const std::byte>, ShaderHash &) noexcept;
bool known_silhouette_shader(std::span<const std::byte>) noexcept;
std::size_t known_silhouette_shader_count() noexcept;
// Generator only, without the allowlist: the fixture's candidate runs.
std::vector<std::byte> adapt_silhouette_shader(std::span<const std::byte>, FilterGeometry);
std::vector<std::byte> make_silhouette_shader(std::span<const std::byte>, FilterGeometry);
} // namespace mhw::scaled_temporal
