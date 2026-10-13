#pragma once

// The alpha's version, in one place: Astera Loader's plugin info, the log's
// start event and CMFM's page header. Also a macro, so the start event holds
// it inside one literal: tools/package_release.py checks the DLL for
// "version":"<version>","build":"alpha".
#define MHW_ALPHA_VERSION "0.24.0"

namespace mhw::alpha {
inline constexpr char version[] = MHW_ALPHA_VERSION;
}  // namespace mhw::alpha
