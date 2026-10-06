#pragma once
#include "filter_shader.hpp"
#include <array>

namespace mhw::scaled_temporal {
// Private variant of the captured two-UV radial shader. Native constants and
// UVs remain unchanged. A private scaled inverse size preserves render-space
// position calculations; only final t0 color samples expand. t1/t2 stay native.
std::vector<std::byte> make_radial_shader(std::span<const std::byte>,FilterGeometry);
struct DependencyVertex {
 std::array<float,2> position{},scene_uv{},auxiliary_uv{};
 unsigned color{0xffffffffU};
};
static_assert(sizeof(DependencyVertex)==28);
// Four vertices in captured strip order. The copy samples the expanded scene
// over its whole allocation. The radial draw preserves both native UV domains.
std::array<DependencyVertex,4> dependency_vertices(FilterGeometry,bool copy);
}
