#pragma once
#include <cstddef>
#include <span>
#include <vector>
namespace mhw::scaled_temporal {
// Exact captured programs only. Returns private DXBC; no native asset is edited.
// Pass 0 adds group synchronization, pass 3 adapts 2:1 reconstructed color loads.
std::vector<std::byte> make_dof_compute_shader(std::span<const std::byte>,unsigned pass);
bool dof_compute_identity(std::span<const std::byte>,unsigned pass);
}
