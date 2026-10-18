#pragma once
// The platform in the startup log (0.30.1-diag.1). Under Wine/Proton: Wine's
// version and build and the host's name and release (Wine's ntdll exports
// them; Windows' has none of these). Everywhere: the folder NGX's core is
// registered in and whether its two DLLs are there (devilzavacado-1: NGX
// refused with 0xBAD00001 under some Proton versions only).
#include "json.hpp"
#include "log_path.hpp"
#include <windows.h>
#include <filesystem>
#include <ostream>

namespace mhw {
inline void write_platform(std::ostream &log) {
    using Text = const char *(__cdecl *)();
    using Host = void(__cdecl *)(const char **, const char **);
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto export_of = [ntdll](const char *name) { return ntdll ? GetProcAddress(ntdll, name) : nullptr; };
    const auto version = reinterpret_cast<Text>(export_of("wine_get_version"));
    const auto build = reinterpret_cast<Text>(export_of("wine_get_build_id"));
    const auto host = reinterpret_cast<Host>(export_of("wine_get_host_version"));
    const auto text = [](const char *s) { return quoted(s ? s : ""); };
    log << "{\"event\":\"platform\",\"wine\":" << (version != nullptr);
    if (version) log << ",\"wine_version\":" << text(version());
    if (build) log << ",\"wine_build\":" << text(build());
    if (host) {
        const char *name{}, *release{};
        host(&name, &release);
        log << ",\"host\":" << text(name) << ",\"host_release\":" << text(release);
    }
    wchar_t folder[MAX_PATH]{};
    DWORD bytes = sizeof folder;
    const bool registered = RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\NVIDIA Corporation\\Global\\NGXCore",
                                         L"FullPath", RRF_RT_REG_SZ, nullptr, folder, &bytes) == ERROR_SUCCESS;
    log << ",\"ngx_core_registered\":" << registered;
    if (registered) {
        const std::filesystem::path core(folder);
        const auto present = [&core](const wchar_t *name) {
            return GetFileAttributesW((core / name).c_str()) != INVALID_FILE_ATTRIBUTES;
        };
        log << ",\"ngx_core\":" << quoted(log_path(core)) << ",\"nvngx\":" << present(L"nvngx.dll")
            << ",\"_nvngx\":" << present(L"_nvngx.dll");
    }
    log << "}\n" << std::flush;
}
} // namespace mhw
