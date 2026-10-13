#pragma once
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include "../dlss_sdk/geometry.hpp"

namespace mhw::alpha {
// mhw_dlss.ini next to the DLL (alpha). Lines are "key = value"; '#' or ';'
// starts a comment; keys and values are case-insensitive; unknown keys and bad
// values are reported as warnings and keep the default. Relative folders are
// relative to the DLL's folder.
//
//   dlss_mode = performance ; off, dlaa, quality, balanced, performance
//   frame_generation = 2x   ; off, 2x, 3x, 4x, 6x, dynamic (the menu turns it on and off live)
//   frame_generation_plugin = auto ; auto: DLSS-G loads where the card supports
//                           ; it; off: never (no frame generation, Reflex Off possible)
//   dynamic_target = display ; display (its refresh rate) or 30 to 1000 (frames per second)
//   frame_generation_auto = off ; on: DLSS-G pauses a fixed multiplier when it would slow the game
//   reflex = on             ; on, boost, off (off only with frame_generation = off)
//   runtime_dir = .         ; Streamline and NVIDIA DLSS DLLs
//   log_dir = logs          ; log files of each launch
//   profile = off           ; on: the CPU stack sampler for performance work
//                           ; ("profile" events in the log; a little overhead)
//   perf_ab = off           ; on: DLSS alternates every 30 s between running and
//                           ; paused (hooks installed), for A/B timing in one
//                           ; launch; needs frame_generation_plugin = off
//   hook_timing = off       ; on: every 64th hooked call is timed ("hook_timing"
//                           ; events every 20 s; perf/hook_timing.hpp)
//
// The DLSS mode sets the render scale asked of the game (Performance 50%,
// Balanced 58%, Quality 67%, DLAA 100%). Off: the mod steps aside and the
// game renders as without it (its own anti-aliasing option, resolution and
// aspect); frame generation pauses (its inputs come from DLSS). The menu
// changes it live.
// Frame generation: a fixed multiplier, or dynamic (DLSS-G picks the count
// toward dynamic_target; RTX 50 and driver 595.41 or newer, else the session
// falls back to the fixed multiplier, 2x from the ini). Auto is NVIDIA's legacy
// mode for fixed multipliers. Dynamic and Auto both turn the game's V-Sync off
// (DLSS-G guide 22.5, 22.8).
// Frame generation (v0.27.0): DLSS-G loads at every start on a card and
// system that support it, so the setting turns it on and off live; a loaded
// DLSS-G paces the swap chain and needs Reflex until the game closes.
// frame_generation_plugin = off keeps it unloaded.
// Reflex: NVIDIA Reflex Low Latency (on), the same with Boost (the GPU keeps
// its clocks up when the CPU is the limit), or off. DLSS Frame Generation
// needs Reflex, so off applies only while frame generation is off, and only
// takes effect without DLSS-G loaded (else Reflex stays on).
enum class ReflexMode : unsigned { off, on, boost };
inline constexpr unsigned min_dynamic_target = 30, max_dynamic_target = 1000;
struct Config {
    dlss_sdk::ReconstructionMode dlss_mode = dlss_sdk::ReconstructionMode::performance;
    bool dlss = true;            // dlss_mode = off: false (dlss_mode keeps its default)
    bool frame_generation = true;
    unsigned multiplier = 2;     // the fixed multiplier (kept while dynamic or off)
    bool dynamic{};              // frame_generation = dynamic
    unsigned dynamic_target{};   // frames per second; 0: the display's refresh rate
    bool generation_auto{};      // frame_generation_auto = on
    bool generation_plugin = true;  // frame_generation_plugin = auto (off: DLSS-G never loads)
    ReflexMode reflex = ReflexMode::on;
    bool profile{};              // profile = on (diagnostic; not in the shipped ini)
    bool perf_ab{};              // perf_ab = on (diagnostic; not in the shipped ini)
    bool hook_timing{};          // hook_timing = on (diagnostic; not in the shipped ini)
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

// The ini value of a DLSS mode, and the mode of an ini value (false if unknown).
inline const char *dlss_mode_value(dlss_sdk::ReconstructionMode m) {
    switch (m) {
    case dlss_sdk::ReconstructionMode::dlaa: return "dlaa";
    case dlss_sdk::ReconstructionMode::quality: return "quality";
    case dlss_sdk::ReconstructionMode::balanced: return "balanced";
    case dlss_sdk::ReconstructionMode::performance: break;
    }
    return "performance";
}
// The mode as players read it (menu and status lines).
inline const char *dlss_mode_name(dlss_sdk::ReconstructionMode m) {
    switch (m) {
    case dlss_sdk::ReconstructionMode::dlaa: return "DLAA";
    case dlss_sdk::ReconstructionMode::quality: return "Quality";
    case dlss_sdk::ReconstructionMode::balanced: return "Balanced";
    case dlss_sdk::ReconstructionMode::performance: break;
    }
    return "Performance";
}
// The ini value of a Reflex mode, the name players read, and the mode of an ini
// value (false if unknown).
inline const char *reflex_value(ReflexMode m) {
    return m == ReflexMode::off ? "off" : m == ReflexMode::boost ? "boost" : "on";
}
inline const char *reflex_name(ReflexMode m) {
    return m == ReflexMode::off ? "Off" : m == ReflexMode::boost ? "Boost" : "On";
}
inline bool parse_reflex(std::string_view value, ReflexMode &m) {
    for (const auto candidate : {ReflexMode::off, ReflexMode::on, ReflexMode::boost})
        if (value == reflex_value(candidate)) {
            m = candidate;
            return true;
        }
    return false;
}
// The ini value of a dynamic target ("display" for 0), and the target of an
// ini value (false if not display or a whole number from 30 to 1000).
inline std::string dynamic_target_value(unsigned target) {
    return target ? std::to_string(target) : "display";
}
inline bool parse_dynamic_target(std::string_view value, unsigned &target) {
    if (value == "display") {
        target = 0;
        return true;
    }
    if (value.empty() || value.size() > 4) return false;
    unsigned n = 0;
    for (const char c : value) {
        if (c < '0' || c > '9') return false;
        n = n * 10 + static_cast<unsigned>(c - '0');
    }
    if (n < min_dynamic_target || n > max_dynamic_target) return false;
    target = n;
    return true;
}
// The ini value of the DLSS setting: "off" or the mode's.
inline const char *dlss_setting_value(bool dlss, dlss_sdk::ReconstructionMode m) { return dlss ? dlss_mode_value(m) : "off"; }
inline bool parse_dlss_mode(std::string_view value, dlss_sdk::ReconstructionMode &m) {
    for (const auto candidate : {dlss_sdk::ReconstructionMode::dlaa, dlss_sdk::ReconstructionMode::quality,
                                 dlss_sdk::ReconstructionMode::balanced, dlss_sdk::ReconstructionMode::performance})
        if (value == dlss_mode_value(candidate)) {
            m = candidate;
            return true;
        }
    return false;
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
        if (key == "dlss_mode") {
            if (value == "off") c.dlss = false;
            else if (parse_dlss_mode(value, c.dlss_mode)) c.dlss = true;
            else c.warnings.push_back(where + "dlss_mode must be off, dlaa, quality, balanced or performance");
        } else if (key == "frame_generation") {
            if (value == "off") c.frame_generation = c.dynamic = false;
            else if (value == "dynamic") c.frame_generation = c.dynamic = true;
            else if (value == "2x" || value == "3x" || value == "4x" || value == "6x") {
                c.frame_generation = true;
                c.dynamic = false;
                c.multiplier = static_cast<unsigned>(value[0] - '0');
            } else c.warnings.push_back(where + "frame_generation must be off, 2x, 3x, 4x, 6x or dynamic");
        } else if (key == "dynamic_target") {
            if (!parse_dynamic_target(value, c.dynamic_target))
                c.warnings.push_back(where + "dynamic_target must be display or a frame rate from 30 to 1000");
        } else if (key == "frame_generation_auto") {
            if (value == "on" || value == "off") c.generation_auto = value == "on";
            else c.warnings.push_back(where + "frame_generation_auto must be on or off");
        } else if (key == "frame_generation_plugin") {
            if (value == "auto" || value == "off") c.generation_plugin = value == "auto";
            else c.warnings.push_back(where + "frame_generation_plugin must be auto or off");
        } else if (key == "hook_timing") {
            if (value == "on" || value == "off") c.hook_timing = value == "on";
            else c.warnings.push_back(where + "hook_timing must be on or off");
        } else if (key == "full_post") {
            // Retired in v0.26.0: after DLSS the game's post chain always runs
            // at the output size (scaled_temporal/full_post.hpp). Older ini
            // files keep the line harmlessly.
        } else if (key == "perf_ab") {
            if (value == "on" || value == "off") c.perf_ab = value == "on";
            else c.warnings.push_back(where + "perf_ab must be on or off");
        } else if (key == "profile") {
            if (value == "on" || value == "off") c.profile = value == "on";
            else c.warnings.push_back(where + "profile must be on or off");
        } else if (key == "reflex") {
            if (!parse_reflex(value, c.reflex)) c.warnings.push_back(where + "reflex must be on, boost or off");
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
    // The keys may come in any order, so this is decided once all are read.
    if (c.perf_ab && c.generation_plugin) {
        c.perf_ab = false;
        c.warnings.push_back("perf_ab needs frame_generation_plugin = off; off");
    }
    if (c.reflex == ReflexMode::off && c.frame_generation) {
        c.warnings.push_back("reflex = off needs frame_generation = off (frame generation needs Reflex); using on");
        c.reflex = ReflexMode::on;
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

// The ini value of the frame generation setting: "off", "2x" to "6x", or "dynamic".
inline std::string frame_generation_value(bool on, unsigned multiplier, bool dynamic = false) {
    return !on ? "off" : dynamic ? "dynamic" : std::to_string(multiplier) + "x";
}

// `text` with `key` set to `value`. Only the value of the last line with that
// key changes (the one parse_config ends up with): its key, spacing, comment
// and line ending stay, as does every other byte. Without such a line, one is
// added at the end with the file's line ending. `key` is lower case.
inline std::string with_value(std::string_view text, std::string_view key, std::string_view value) {
    constexpr auto none = std::string_view::npos;
    std::size_t begin = none, end = none;
    for (std::size_t pos = 0; pos < text.size();) {
        const auto newline = text.find('\n', pos);
        auto line = text.substr(pos, (newline == none ? text.size() : newline) - pos);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (const auto comment = line.find_first_of("#;"); comment != none) line = line.substr(0, comment);
        if (const auto equals = line.find('='); equals != none && trimmed_lower(line.substr(0, equals)) == key) {
            auto first = equals + 1, last = line.size();
            while (first < last && std::isspace(static_cast<unsigned char>(line[first]))) ++first;
            while (last > first && std::isspace(static_cast<unsigned char>(line[last - 1]))) --last;
            begin = pos + first;
            end = pos + last;
        }
        pos = newline == none ? text.size() : newline + 1;
    }
    if (begin != none) return std::string(text.substr(0, begin)).append(value).append(text.substr(end));
    const std::string_view newline = text.find("\r\n") != none ? "\r\n" : "\n";
    std::string out(text);
    if (!out.empty() && out.back() != '\n') out.append(newline);
    return out.append(key).append(" = ").append(value).append(newline);
}
inline std::string with_frame_generation(std::string_view text, std::string_view value) {
    return with_value(text, "frame_generation", value);
}
inline std::string with_reflex(std::string_view text, ReflexMode mode) {
    return with_value(text, "reflex", reflex_value(mode));
}
}  // namespace mhw::alpha
