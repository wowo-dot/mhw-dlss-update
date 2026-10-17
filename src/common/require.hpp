#pragma once
// Set-up checks that throw: the caller catches, records the reason and fails
// closed (the game's own path stays in place).
#include <windows.h>
#include <stdexcept>

namespace mhw {
inline void require(bool ok, const char *why) {
    if (!ok) throw std::runtime_error(why);
}
inline void hr(HRESULT code, const char *why) {
    require(SUCCEEDED(code), why);
}
} // namespace mhw
