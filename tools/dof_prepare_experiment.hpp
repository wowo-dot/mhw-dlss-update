#pragma once
#include "../src/scaled_temporal/dof_compute_shader.hpp"
namespace dof_experiment {
inline std::vector<std::byte> prepare(std::span<const std::byte> bytes){return mhw::scaled_temporal::make_dof_compute_shader(bytes,3);}
}
