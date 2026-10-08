#pragma once
#include "../src/perf/toggles.hpp"
#include <windows.h>
#include <cwchar>

// MHW_PERF_TOGGLES=<mask> replays a fixture on the lean paths (v0.12); unset
// keeps the original paths.
inline void apply_perf_env() {
    wchar_t text[32]{};
    const auto n = GetEnvironmentVariableW(L"MHW_PERF_TOGGLES", text, 32);
    if (n && n < 32) mhw::perf::toggles.store(static_cast<unsigned>(std::wcstoul(text, nullptr, 0)));
}
