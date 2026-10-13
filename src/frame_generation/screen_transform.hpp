#pragma once
#include <algorithm>
#include <array>
#include <cmath>

// MHW's screen write, applied per channel when the final scene target T
// (linear R11G11B10_FLOAT, UI already composited) is written to the BGRA8
// backbuffer. Source: RenderDoc capture of the unmodified game
// (docs/research/frame-generation-hudless-capture-result.md); g is
// CBSystem.fGammaCorrectEx.x (b3, byte offset 24). DLSS-G's HUD-less input is
// the HUD-free T through this same transform. The HLSL pass mirrors it exactly.
namespace mhw::frame_generation::screen {

// The Brightness slider spans g = 0.1 (minimum) to 3.0 (maximum), measured in
// game (v0.9.2, game-09); this window covers it with margin. Values outside
// it are treated as not CBSystem (the capture keeps its last valid value; the
// transform falls back to the neutral curve g = 1 before any).
constexpr float gamma_min = 0.05f, gamma_max = 4.0f;
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

// HDR10 (v0.24.8): the game's HDR screen write, System_CopyToBackBufferPQSingle
// (and the second output of System_CopyToBackBufferPQ), shader census
// 2026-10-08, disassembled 2026-10-13. Its parameters are in CBRenderFrame
// (b2), not CBSystem: no brightness curve.
//   c = max(T, 0); b = BT.709 -> BT.2020 rotation of c;
//   x = b + fHdrOutputGamutMappingRatio * (c - b);
//   bIsGUIHdrGamma: each channel below 1 -> pow(max(x, 0), fHdrOutputGamma);
//   ST.2084 PQ of saturate(x * fHdrOutputWhiteLevel / 10000), capped at 1.
namespace hdr {
struct Parameters {
    float white{100.0f}, ratio{0.0f}, gamma{1.0f};  // the shader's defaults
    bool gui_gamma{};
};
// Accepted as CBRenderFrame's HDR fields (the capture's layout check).
constexpr float white_min = 10.0f, white_max = 10000.0f, gamma_min = 0.1f, gamma_max = 10.0f;
inline bool plausible(const Parameters &p) noexcept {
    return std::isfinite(p.white) && p.white >= white_min && p.white <= white_max && std::isfinite(p.ratio) && p.ratio >= 0.0f &&
           p.ratio <= 1.0f && std::isfinite(p.gamma) && p.gamma >= gamma_min && p.gamma <= gamma_max;
}
// ST.2084 constants.
constexpr float m1 = 2610.0f / 16384.0f, m2 = 2523.0f / 32.0f, c1 = 3424.0f / 4096.0f, c2 = 2413.0f / 128.0f, c3 = 2392.0f / 128.0f;
inline float pq_encode(float normalized) noexcept {
    const float p = std::pow(std::clamp(normalized, 0.0f, 1.0f), m1);
    return std::min(std::pow((c1 + c2 * p) / (1.0f + c3 * p), m2), 1.0f);
}
// The shader's BT.709 -> BT.2020 rows.
constexpr float rotation[3][3]{{0.627404f, 0.329282f, 0.043314f}, {0.069097f, 0.919540f, 0.011361f}, {0.016392f, 0.088013f, 0.895595f}};
inline std::array<float, 3> encode(std::array<float, 3> linear, const Parameters &p) noexcept {
    for (auto &v : linear) v = std::max(v, 0.0f);
    std::array<float, 3> out{};
    for (unsigned i = 0; i < 3; ++i) {
        const float b = rotation[i][0] * linear[0] + rotation[i][1] * linear[1] + rotation[i][2] * linear[2];
        float x = b + p.ratio * (linear[i] - b);
        if (p.gui_gamma && x < 1.0f) x = std::pow(std::max(x, 0.0f), p.gamma);
        out[i] = pq_encode(x * p.white / 10000.0f);
    }
    return out;
}
}  // namespace hdr

}  // namespace mhw::frame_generation::screen
