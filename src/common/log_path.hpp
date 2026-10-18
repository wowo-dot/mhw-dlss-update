#pragma once
// A path as the log writes it: UTF-8, with the user's profile folder (it holds
// the Windows user name) written as %USERPROFILE%. The rest stays, so a
// player's log still shows where the mod's folders are (MO2's mods or
// overwrite, the plugin folder). AGENTS.md: no personal paths in logs.
#include <windows.h>
#include <filesystem>
#include <string>
#include <cwchar>

namespace mhw {
inline std::string log_path(const std::filesystem::path &path) {
    std::wstring text = path.wstring();
    wchar_t profile[MAX_PATH]{};
    const auto n = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    if (n > 0 && n < MAX_PATH && text.size() >= n && _wcsnicmp(text.c_str(), profile, n) == 0 &&
        (text.size() == n || text[n] == L'\\' || text[n] == L'/'))
        text = L"%USERPROFILE%" + text.substr(n);
    const auto utf8 = std::filesystem::path(text).u8string();
    return std::string(utf8.begin(), utf8.end());
}
} // namespace mhw
