// NVSharpen's constants (src/sharpen/sharpen_config.hpp) and the colour
// matrices the HDR10 shader inverts (src/sharpen/sharpen_pass.cpp).
#include "../src/sharpen/sharpen_config.hpp"
#include <cmath>
#include <iostream>
using namespace mhw::sharpen;

static_assert(!sharpening(0) && sharpening(1) && sharpening(100) && !sharpening(101));

int main() {
    int failures{};
    const auto check = [&](bool ok, const char *what) {
        if (!ok) {
            std::cerr << "failed: " << what << '\n';
            ++failures;
        }
    };
    const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-5F; };

    Constants c;
    check(!constants(0, 2560, 1440, false, 0, c), "0 %: nothing to record");
    check(!constants(50, 0, 1440, false, 0, c) && !constants(50, 2560, 0, false, 0, c), "an empty extent");
    check(!constants(50, 2560, 1440, true, 5.0F, c) && !constants(50, 2560, 1440, true, 20000.0F, c) &&
              !constants(50, 2560, 1440, true, std::nanf(""), c),
          "HDR10 with an implausible white level");

    // SDR: NIS's own NVSharpenUpdateConfig at the slider's value, no viewport.
    check(constants(50, 2560, 1440, false, 0, c), "SDR at 50 %");
    NISConfig nis{};
    check(NVSharpenUpdateConfig(nis, 0.5F, 0, 0, 2560, 1440, 2560, 1440, 0, 0, NISHDRMode::None), "NIS at 0.5");
    check(near(c.nis.kSharpStrengthMin, nis.kSharpStrengthMin) && near(c.nis.kSharpLimitScale, nis.kSharpLimitScale) &&
              near(c.nis.kDetectThres, nis.kDetectThres) && near(c.nis.kSharpStartY, 0.45F),
          "SDR: NIS's constants (LDR)");
    check(c.nis.kInputViewportWidth == 2560 && c.nis.kInputViewportHeight == 1440 && near(c.nis.kScaleX, 1.0F) &&
              near(c.nis.kSrcNormX, 1.0F / 2560) && near(c.nis.kSrcNormY, 1.0F / 1440),
          "the whole target, no scaling");
    check(c.nis.reserved0 == 1.0F && c.nis.reserved1 == 1.0F, "SDR: no white level");
    // 100 %: NIS's upper range (0.4 + 0.5 x 1.25 x 1.2).
    check(constants(100, 2560, 1440, false, 0, c) && near(c.nis.kSharpStrengthMin, 1.15F), "100 %");
    // 0 % in NIS is almost none; the mod records nothing there instead.
    NISConfig zero{};
    NVSharpenUpdateConfig(zero, 0.0F, 0, 0, 2560, 1440, 2560, 1440, 0, 0, NISHDRMode::None);
    check(zero.kSharpStrengthMin == 0.0F && zero.kSharpStrengthScale < 0.03F, "NIS's 0 is near none");

    // HDR10: PQ mode, the game's white level in reserved0.
    check(constants(50, 3840, 2160, true, 720.69F, c), "HDR10 at game-109's white level");
    check(near(c.nis.kSharpStartY, 0.35F) && near(c.nis.kDetectThres, 32.0F / 1024) && near(c.nis.reserved0, 720.69F) &&
              near(c.nis.reserved1 * 720.69F, 1.0F),
          "HDR10: NIS's PQ constants and the white level");
    check(constants(50, 2560, 1440, true, white_min, c) && constants(50, 2560, 1440, true, white_max, c),
          "the plausible range's ends");

    // The shader's BT.709 -> BT.2020 rotation (the game's PQ screen write) and
    // the inverse it decodes with: the product is the identity.
    constexpr float to[3][3]{
        {0.627404F, 0.329282F, 0.043314F}, {0.069097F, 0.919540F, 0.011361F}, {0.016392F, 0.088013F, 0.895595F}};
    constexpr float from[3][3]{{1.660490360F, -0.587639061F, -0.072852475F},
                               {-0.124549969F, 1.132899890F, -0.008347655F},
                               {-0.018151890F, -0.100578206F, 1.118729895F}};
    float worst{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float v{};
            for (int k = 0; k < 3; ++k) v += from[i][k] * to[k][j];
            worst = std::fmax(worst, std::fabs(v - (i == j ? 1.0F : 0.0F)));
        }
    check(worst < 2e-6F, "BT.2020 rotation round trip");

    if (failures) return 1;
    std::cout << "sharpen tests passed\n";
    return 0;
}
