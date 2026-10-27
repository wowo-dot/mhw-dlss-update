#pragma once

// The mod's version, in one place: Astera Loader's plugin info, the log's
// start event and CMFM's page header. Also a macro, so the start event holds
// it inside one literal: tools/package_release.py checks the DLL for
// {"event":"start","schema":1,"version":"<version>",
#define MHW_VERSION "0.31.1-dev.21"

namespace mhw::settings {
inline constexpr char version[] = MHW_VERSION;
} // namespace mhw::settings
