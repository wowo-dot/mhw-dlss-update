#pragma once
#include "filter_shader.hpp"
namespace mhw::scaled_temporal {
// Hash-gated adaptation of a caller-supplied native haze PS. Only final color
// samples expand; auxiliary inputs and native reconstruction remain unchanged.
// This helper does not admit a live recipe or prove its resource bindings.
std::vector<std::byte> make_haze_shader(std::span<const std::byte> native,FilterGeometry);
}
