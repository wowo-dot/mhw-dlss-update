#pragma once
// The platform in the startup log (0.30.1-diag.1). Under Wine/Proton: Wine's
// version and build and the host's name and release (Wine's ntdll exports
// them; Windows' has none of these). Everywhere: the folder NGX's core is
// registered in and whether its two DLLs are there (devilzavacado-1: NGX
// refused with 0xBAD00001 under some Proton versions only). Last, the
// processor (0.31.1, devilzavacado-2: "no fps difference" between DLSS modes
// is a CPU limit): CPUID's brand string, its cores and threads, and whether
// the cores differ in efficiency class (P and E cores).
#include "json.hpp"
#include "log_path.hpp"
#include <windows.h>
#include <intrin.h>
#include <bit>
#include <cstddef>
#include <filesystem>
#include <ostream>
#include <string>
#include <vector>

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
    int regs[4]{};
    char brand[49]{};
    __cpuid(regs, static_cast<int>(0x80000000));
    if (static_cast<unsigned>(regs[0]) >= 0x80000004)
        for (int i = 0; i < 3; ++i) __cpuid(reinterpret_cast<int *>(brand + 16 * i), static_cast<int>(0x80000002 + i));
    std::string cpu(brand);
    cpu.erase(0, cpu.find_first_not_of(' '));
    cpu.erase(cpu.find_last_not_of(' ') + 1);
    unsigned cores{}, classes{};
    DWORD length{};
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length);
    std::vector<unsigned char> relations(length);
    if (length && GetLogicalProcessorInformationEx(
                      RelationProcessorCore,
                      reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(relations.data()), &length))
        for (DWORD at = 0; at + offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor) < length;) {
            const auto *core = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(&relations[at]);
            if (!core->Size) break;
            ++cores;
            classes |= 1U << (core->Processor.EfficiencyClass & 31);
            at += core->Size;
        }
    log << ",\"cpu\":" << quoted(cpu) << ",\"cpu_cores\":" << cores
        << ",\"cpu_threads\":" << GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)
        << ",\"cpu_hybrid\":" << (std::popcount(classes) > 1);
    log << "}\n" << std::flush;
}
} // namespace mhw
