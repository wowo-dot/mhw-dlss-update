#pragma once
#include <array>
#include <bit>
#include <cmath>

namespace mhw::scaled_temporal {

// The hidden-surface material values of a late draw that the silhouette
// correction replays at the output size (CBMhMaterialGlobal at pixel register
// b5, as read by silhouette_bindings: [2] bMaterialCustomLighting, [3]
// iMaterialCustomLightingGBufferIdMask, [4] fMaterialHiddenSurfaceDrawIntensity).
// Custom lighting 1 and mask 7680 identify the hidden-surface path. The
// intensity only scales the colour and opacity that the game's own pixel shader
// computes, and the replay binds the same buffer, so any finite, non-negative
// value is accepted. Our captures had 0.1; a player's game had 0.02, and
// requiring 0.1 left every silhouette at the render size (v0.28.6).
inline bool silhouette_material(const std::array<unsigned, 5> &material) noexcept {
    const float intensity = std::bit_cast<float>(material[4]);
    return material[2] == 1 && material[3] == 7680 && std::isfinite(intensity) && intensity >= 0.F;
}

} // namespace mhw::scaled_temporal
