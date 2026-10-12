#pragma once
#include <cstddef>
#include <span>
#include <vector>
namespace mhw::scaled_temporal {
// Exact captured programs only. Returns private DXBC; no native asset is edited.
// Pass 0 adds group synchronization; pass 3 reads reconstructed colour as the
// area average of each render texel's output footprint, at any render scale
// from 1x to 2x (CBScreen gives both sizes).
std::vector<std::byte> make_dof_compute_shader(std::span<const std::byte>,unsigned pass);
bool dof_compute_identity(std::span<const std::byte>,unsigned pass);
}
