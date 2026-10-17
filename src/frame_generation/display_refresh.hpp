#pragma once
#include <windows.h>

// The refresh rate of the monitor showing the game, in Hz (0 if unknown):
// what DLSS-G's Dynamic mode aims for with a target of 0 (DLSS-G guide 6.3:
// the window's placement picks the monitor). Read-only Win32 calls; any
// thread. The game's window is this process's largest visible top-level one.
namespace mhw::frame_generation {

inline HWND game_window() noexcept {
    struct Search {
        DWORD pid;
        HWND best;
        long long area;
    } search{GetCurrentProcessId(), nullptr, 0};
    EnumWindows(
        [](HWND w, LPARAM p) -> BOOL {
            auto &s = *reinterpret_cast<Search *>(p);
            DWORD pid{};
            GetWindowThreadProcessId(w, &pid);
            RECT r{};
            if (pid != s.pid || !IsWindowVisible(w) || GetWindow(w, GW_OWNER) || !GetClientRect(w, &r)) return TRUE;
            const long long area = static_cast<long long>(r.right - r.left) * (r.bottom - r.top);
            if (area > s.area) {
                s.area = area;
                s.best = w;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.best;
}

inline unsigned display_refresh_hz() noexcept {
    const HWND window = game_window();
    if (!window) return 0;
    MONITORINFOEXW info{};
    info.cbSize = sizeof info;
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info)) return 0;
    DEVMODEW mode{};
    mode.dmSize = sizeof mode;
    if (!EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode)) return 0;
    // 0 and 1 mean "the hardware's default"; not a rate.
    return mode.dmDisplayFrequency > 1 ? mode.dmDisplayFrequency : 0;
}

} // namespace mhw::frame_generation
