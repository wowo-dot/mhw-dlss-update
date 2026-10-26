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
} // namespace mhw::dlss_sdk
