#pragma once
// v0.30.1 (jgb2003-1): which DLSS NGX loaded when it was not this mod's
// provider. NVIDIA's NGX keeps its own DLSS files under an NVIDIA\NGX folder
// (ProgramData\NVIDIA\NGX\models); the NVIDIA App's DLSS override has NGX load
// one of those instead of the game's. Anywhere else, a file named
// nvngx_dlss*.dll (a replaced one in the game's folder, another DLSS tool).
// Pure, for tests/ngx_providers_tests.cpp.
#include <string>
#include <string_view>

namespace mhw::ngx_startup {
enum class Provider : unsigned { none, nvidia_store, file };

inline std::wstring lower_ascii(std::wstring_view text) {
    std::wstring out(text);
    for (auto &c : out)
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    return out;
}

// A loaded module's full path: a DLSS provider candidate, and where from.
inline Provider provider_kind(std::wstring_view path) {
    auto text = lower_ascii(path);
    for (auto &c : text)
        if (c == L'/') c = L'\\';
    if (text.find(L"\\nvidia\\ngx\\") != std::wstring::npos) return Provider::nvidia_store;
    const auto slash = text.find_last_of(L'\\');
    const std::wstring_view name = std::wstring_view(text).substr(slash == std::wstring::npos ? 0 : slash + 1);
    return name.starts_with(L"nvngx_dlss") && name.ends_with(L".dll") ? Provider::file : Provider::none;
}

// The one to name when several are loaded: NVIDIA's own store first.
constexpr Provider stronger(Provider a, Provider b) noexcept {
    return a == Provider::nvidia_store || b == Provider::nvidia_store ? Provider::nvidia_store
           : a == Provider::file || b == Provider::file               ? Provider::file
                                                                      : Provider::none;
}

// The reason the worker stops on (status.hpp words it for players).
constexpr const char *provider_reason(Provider p) noexcept {
    return p == Provider::nvidia_store ? "dlss-override"
           : p == Provider::file       ? "dlss-provider-foreign"
                                       : "coexistence-NGX-startup";
}
} // namespace mhw::ngx_startup
