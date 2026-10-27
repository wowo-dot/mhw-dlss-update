#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#pragma warning(push, 0)
#include "../../third_party/nis/NIS_Config.h"
#pragma warning(pop)

// NVSharpen's constants (0.32.0, docs/research/nvsharpen.md): the player's
// Sharpness (0 to 100 %, the ini's sharpness) through NIS's own
// NVSharpenUpdateConfig, for the final scene target in SDR (LDR mode on its
// sRGB encoding) or HDR10 (PQ mode on its PQ encoding at the game's white
// level, renderer +0x21C). Pure (tests/sharpen_tests.cpp).
namespace mhw::sharpen {

// NISOptimizer(false, NVIDIA_Generic): NVSharpen's block and group sizes.
inline constexpr unsigned block_width = 32, block_height = 32, thread_group = 128;
static_assert(block_width * block_height % thread_group == 0);
inline constexpr unsigned max_percent = 100, percent_step = 5; // the menu's slider

// The game's HDR white level (CBRenderFrame fHdrOutputWhiteLevel) is plausible
// in [10, 10000] nits; outside it the frame is not sharpened.
inline constexpr float white_min = 10.0f, white_max = 10000.0f;
inline bool white_plausible(float white) noexcept {
    return std::isfinite(white) && white >= white_min && white <= white_max;
}

// The shader's constant buffer: NIS's NISConfig, whose reserved0 carries the
// white level (HDR10) and reserved1 1/white. Root constants, 28 values.
struct Constants {
    NISConfig nis{};
};
inline constexpr unsigned constant_count = 28;
static_assert(offsetof(NISConfig, reserved1) / 4 + 1 == constant_count);

// Whether `percent` sharpens at all (0 records nothing).
constexpr bool sharpening(unsigned percent) noexcept {
    return percent > 0 && percent <= max_percent;
}
// The constants for one frame; false for an extent NIS refuses or an HDR
// white level that is not plausible.
inline bool constants(unsigned percent, unsigned width, unsigned height, bool hdr, float white, Constants &out) {
    out = {};
    if (!sharpening(percent) || !width || !height || (hdr && !white_plausible(white))) return false;
    if (!NVSharpenUpdateConfig(out.nis, static_cast<float>(percent) / max_percent, 0, 0, width, height, width, height,
                               0, 0, hdr ? NISHDRMode::PQ : NISHDRMode::None))
        return false;
    out.nis.reserved0 = hdr ? white : 1.0f;
    out.nis.reserved1 = hdr ? 1.0f / white : 1.0f;
    return true;
}

} // namespace mhw::sharpen
