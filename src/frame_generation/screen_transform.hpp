#pragma once
#include <algorithm>
#include <cmath>

// MHW's screen write, applied per channel when the final scene target T
// (linear R11G11B10_FLOAT, UI already composited) is written to the BGRA8
// backbuffer. Source: RenderDoc capture of the unmodified game
// (docs/research/frame-generation-hudless-capture-result.md); g is
// CBSystem.fGammaCorrectEx.x (b3, byte offset 24). DLSS-G's HUD-less input is
// the HUD-free T through this same transform. The HLSL pass mirrors it exactly.
namespace mhw::frame_generation::screen {

// The brightness setting's range is unknown; outside this window the pass
// falls back to the neutral curve (g = 1) and reports it.
constexpr float gamma_min = 0.25f, gamma_max = 4.0f;
inline bool gamma_plausible(float g) noexcept { return std::isfinite(g) && g >= gamma_min && g <= gamma_max; }

inline float brightness(float c, float g) noexcept {
    c = std::clamp(c, 0.0f, 1.0f);
    const float k = std::min(0.05f * g + 0.95f, 1.0f);
    return 1.0f - std::pow(1.0f - k * c, g);
}

// IEC 61966-2-1 encode of [0, 1].
inline float srgb_encode(float c) noexcept {
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.0031308f ? 12.92f * c : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

inline float channel(float linear, float g) noexcept {
    return srgb_encode(brightness(linear, gamma_plausible(g) ? g : 1.0f));
}

}  // namespace mhw::frame_generation::screen
