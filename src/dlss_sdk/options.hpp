#pragma once
#include <atomic>

namespace mhw::dlss_sdk {
// DLSS's own exposure estimate (NVSDK_NGX_DLSS_Feature_Flags_AutoExposure),
// read when the feature is created. On by default; the ini's
// auto_exposure = off (0.31.1, diagnostic) creates DLSS without it, so a run
// can show its GPU cost. The evaluation passes exposure 1 and no exposure
// texture, and the game's colour there is before tone mapping, so the image
// may suffer: it is for timing, not for play.
inline std::atomic<bool> auto_exposure{true};
// The DLSS model preset for every mode (NVSDK_NGX_DLSS_Hint_Render_Preset_*:
// 0 NVIDIA's default, 5 E and 6 F the CNN model, 10 J and 11 K the
// transformer, 12 L and 13 M the second transformer), read when the feature
// is created. K by default; the ini's dlss_preset (0.31.1, diagnostic) picks
// another to time it.
inline std::atomic<unsigned> render_preset{11};
// The camera jitter while DLSS draws. The game's table (scene +0xe6a0, 20
// Halton values in -0.5..0.5) moves the image by half its value in render
// pixels (projection_snapshot.cpp's relation), so its samples cover only the
// middle half of each pixel; DLSS expects the whole pixel. The ini's
// full_pixel_jitter = on (0.31.1, diagnostic) writes the table's values
// doubled, so they span -0.5..0.5 pixels.
inline std::atomic<bool> full_pixel_jitter{false};
} // namespace mhw::dlss_sdk
