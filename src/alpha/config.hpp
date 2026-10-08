#pragma once
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace mhw::alpha {
// mhw_dlss.ini next to the DLL (alpha). Lines are "key = value"; '#' or ';'
// starts a comment; keys and values are case-insensitive; unknown keys and bad
// values are reported as warnings and keep the default. Relative folders are
// relative to the DLL's folder.
//
//   frame_generation = 2x   ; off, 2x, 3x, 4x, 6x (DLSS-G not loaded when off)
//   reflex = on             ; on (off is not available yet)
//   runtime_dir = .         ; Streamline and NVIDIA DLSS DLLs
//   log_dir = logs          ; log files of each launch
//
// DLSS runs at Performance (half resolution); DLAA and other modes come with
// later builds.
struct Config {
    bool frame_generation = true;
    unsigned multiplier = 2;
    bool reflex = true;
    std::filesystem::path runtime_dir, log_dir;
    std::vector<std::string> warnings;
};

inline std::string trimmed_lower(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    std::string out(s);
    for (auto &c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

inline Config parse_config(std::string_view text, const std::filesystem::path &dll_folder) {
    Config c;
    std::string runtime = ".", logs = "logs";
    unsigned line_number = 0;
    while (!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        ++line_number;
        if (const auto comment = line.find_first_of("#;"); comment != std::string_view::npos) line = line.substr(0, comment);
        if (trimmed_lower(line).empty()) continue;
        const auto equals = line.find('=');
        const auto where = "line " + std::to_string(line_number) + ": ";
        if (equals == std::string_view::npos) {
            c.warnings.push_back(where + "expected key = value");
            continue;
        }
        const auto key = trimmed_lower(line.substr(0, equals));
        const auto raw = line.substr(equals + 1);
        const auto value = trimmed_lower(raw);
        if (key == "frame_generation") {
            if (value == "off") c.frame_generation = false;
            else if (value == "2x" || value == "3x" || value == "4x" || value == "6x") {
                c.frame_generation = true;
                c.multiplier = static_cast<unsigned>(value[0] - '0');
            } else c.warnings.push_back(where + "frame_generation must be off, 2x, 3x, 4x or 6x");
        } else if (key == "reflex") {
            if (value == "off") c.warnings.push_back(where + "reflex = off is not available yet; using on");
            else if (value != "on") c.warnings.push_back(where + "reflex must be on");
        } else if (key == "runtime_dir" || key == "log_dir") {
            // Folder names keep their case; only surrounding spaces and quotes go.
            std::string folder(raw);
            while (!folder.empty() && (std::isspace(static_cast<unsigned char>(folder.back())) || folder.back() == '"')) folder.pop_back();
            std::size_t first = 0;
            while (first < folder.size() && (std::isspace(static_cast<unsigned char>(folder[first])) || folder[first] == '"')) ++first;
            folder = folder.substr(first);
            if (folder.empty()) c.warnings.push_back(where + key + " is empty");
            else (key == "runtime_dir" ? runtime : logs) = folder;
        } else {
            c.warnings.push_back(where + "unknown key " + key);
        }
    }
    const auto resolve = [&](const std::string &folder) {
        const std::filesystem::path p(std::u8string(folder.begin(), folder.end()));  // the file is UTF-8
        auto r = (p.is_absolute() ? p : dll_folder / p).lexically_normal();
        if (!r.has_filename() && r.has_relative_path()) r = r.parent_path();  // "folder\." -> "folder"
        return r;
    };
    c.runtime_dir = resolve(runtime);
    c.log_dir = resolve(logs);
    return c;
}
}  // namespace mhw::alpha
