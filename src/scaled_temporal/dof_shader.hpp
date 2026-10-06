#pragma once
#include "filter_shader.hpp"
namespace mhw::scaled_temporal {
// Hash-gated private variant of the caller-supplied native DOF composite.
// Native constants and auxiliary sampling stay in render space; only t2
// (reconstructed HDR color) expands to the full output allocation.
std::vector<std::byte> make_dof_shader(std::span<const std::byte> native,FilterGeometry);
}
