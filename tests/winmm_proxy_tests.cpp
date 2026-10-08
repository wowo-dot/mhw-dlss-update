// The winmm.dll pass-through built on its own (bin/winmm_proxy/winmm.dll):
// every system winmm export resolves through it by name or ordinal, calls
// reach the system implementation, and it is a separate module from it.
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {
struct Export { const char *name; unsigned ordinal; };
constexpr Export exports[]{
#include "../src/winmm_proxy/winmm_exports.inc"
};
void check(bool value, const char *why) { if (!value) { std::cerr << why << '\n'; std::exit(1); } }
template<class F> F get(HMODULE m, const char *name) { return reinterpret_cast<F>(GetProcAddress(m, name)); }
}

int wmain(int argc, wchar_t **argv) {
    check(argc == 2, "usage: winmm_proxy_tests <path to proxy winmm.dll>");
    const HMODULE proxy = LoadLibraryW(argv[1]);
    std::array<wchar_t, MAX_PATH> path{}; const auto n = GetSystemDirectoryW(path.data(), MAX_PATH);
    check(n > 0, "system directory"); std::wstring system_path(path.data(), n); system_path += L"\\winmm.dll";
    const HMODULE system = LoadLibraryW(system_path.c_str());
    check(proxy && system && proxy != system, "proxy and system winmm are separate modules");
    unsigned resolved = 0;
    for (const auto &e : exports) {
        const auto *key = e.name ? e.name : MAKEINTRESOURCEA(e.ordinal);
        if (!GetProcAddress(system, key)) continue;  // absent on this Windows build
        check(GetProcAddress(proxy, key) != nullptr, e.name ? e.name : "ordinal-only export");
        ++resolved;
    }
    check(resolved == std::size(exports), "every export of the generating winmm resolves on this system");
    using U = UINT(WINAPI *)(); using P = MMRESULT(WINAPI *)(UINT); using S = BOOL(WINAPI *)(LPCWSTR, HMODULE, DWORD);
    using F = FOURCC(WINAPI *)(LPCSTR, UINT);
    check(get<U>(proxy, "waveOutGetNumDevs")() == get<U>(system, "waveOutGetNumDevs")(), "waveOutGetNumDevs passes through");
    check(get<P>(proxy, "timeBeginPeriod")(1) == TIMERR_NOERROR && get<P>(proxy, "timeEndPeriod")(1) == TIMERR_NOERROR, "timer period");
    const auto t0 = get<U>(system, "timeGetTime")(); const auto t1 = get<U>(proxy, "timeGetTime")();
    check(t1 - t0 < 1000, "timeGetTime passes through");
    check(get<F>(proxy, "mmioStringToFOURCCA")("RIFF", 0) == mmioFOURCC('R', 'I', 'F', 'F'), "mmioStringToFOURCCA");
    check(get<S>(proxy, "PlaySoundW")(nullptr, nullptr, 0) == TRUE, "PlaySoundW stop request");
    std::cout << "PASS winmm pass-through: " << resolved << " exports resolve by name or ordinal; calls reach the system winmm\n";
}
