#pragma once
#include "filter_shader.hpp"
namespace mhw::scaled_temporal {
// Hash-gated adaptation of a caller-supplied native uDepthColorFilter PS
// (distance tint between TAA and the motion filter). The scene color sample
// expands to the full output; the depth load and the view ray use the raster
// position on the render lattice, so depth, CBScreen and CBViewProjection stay
// in render space. This helper does not admit a live recipe or its bindings.
std::vector<std::byte> make_depth_color_shader(std::span<const std::byte> native,FilterGeometry);
}
