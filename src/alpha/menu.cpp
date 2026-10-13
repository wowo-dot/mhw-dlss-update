#include "menu.hpp"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "menu_model.hpp"
#include "status.hpp"
#include "version.hpp"
#include "../continuous_dlaa/runtime.hpp"
#include "../frame_generation/display_refresh.hpp"
#include "../frame_generation/game_session.hpp"
#include "../reflex/reflex_mode.hpp"
#include "../../third_party/cmfm/cmfm.h"

namespace mhw::alpha::menu {
namespace {
namespace mm = menu_model;
namespace fg = frame_generation::game;
namespace caps = capabilities;

// What this card offers (capabilities.hpp), completed with the session's
// DLSS-G limits when those were read after the start.
caps::Capabilities card() {
    auto c = status::snapshot().caps;
    const auto session = fg::status();
    if (!c.max_multiplier && session.maximum) c.max_multiplier = session.maximum + 1;
    if (c.dynamic == caps::Support::unknown && session.dynamic_support)
        c.dynamic = session.dynamic_support == 2 ? caps::Support::yes : caps::Support::no;
    return c;
}

enum Row : std::uint32_t {
    frame_generation_row = 1,
    multiplier_row,
    dlss_mode_row,
    reflex_setting_row,
    target_row,
    auto_row,
    upscaling_section = 20,
    generation_section,
    latency_section,
    mod_row = 10,
    dlss_row,
    reflex_row,
    generation_row,
    generated_row,
    card_row,
    features_row,
};

// Touched on CMFM's menu thread (and once by add_pages before any page exists).
struct State {
    std::mutex mutex;
    const al_api_t *api{};
    al_plugin_t *self{};
    std::filesystem::path folder, ini;
    mm::Settings settings;  // what the ini holds, or will hold once saved
    bool dirty{};           // changed since the last save
    bool mode_dirty{}, generation_dirty{}, reflex_dirty{}, target_dirty{}, auto_dirty{};  // which keys
    bool fg_requested{};    // DLSS-G loads at start where supported: frame generation applies live
    // "Target frame rate" choices, fixed when the page is added (CMFM keeps the pointers).
    std::vector<unsigned> targets;
    std::vector<std::string> target_names;
    std::vector<const char *> target_pointers;
};
State state;

void log(al_log_level_t level, const std::string &text) noexcept {
    if (state.api) state.api->log(state.self, level, text.c_str());
}

void copy(char *out, std::uint32_t size, std::string_view text) noexcept {
    if (!out || !size) return;
    const auto n = std::min<std::size_t>(text.size(), size - 1);
    std::memcpy(out, text.data(), n);
    out[n] = '\0';
}

bool read_file(const std::filesystem::path &file, std::string &text) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

std::string windows_error(DWORD error) { return "Windows error " + std::to_string(error); }

// Replaces the ini where the file really is. Under Mod Organizer 2 that is the
// mod's own folder; a new file created through the game folder's path would
// land in MO2's overwrite folder instead. The new text goes to a file next to
// it, is flushed, then moved over the old one.
bool write_file(const std::filesystem::path &file, const std::string &text, std::string &why) {
    std::wstring target = file.wstring();
    if (const HANDLE h = CreateFileW(file.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        h != INVALID_HANDLE_VALUE) {
        std::wstring real(32768, L'\0');
        const auto n = GetFinalPathNameByHandleW(h, real.data(), static_cast<DWORD>(real.size()), FILE_NAME_NORMALIZED);
        const auto error = GetLastError();
        CloseHandle(h);
        if (!n || n >= real.size()) {
            why = "cannot find where mhw_dlss.ini is (" + windows_error(error) + ")";
            return false;
        }
        real.resize(n);
        target = real;
    }
    const auto fresh = target + L".new";
    const HANDLE out = CreateFileW(fresh.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE) {
        why = "cannot write next to mhw_dlss.ini (" + windows_error(GetLastError()) + ")";
        return false;
    }
    DWORD wrote{};
    const bool written = WriteFile(out, text.data(), static_cast<DWORD>(text.size()), &wrote, nullptr) &&
                         wrote == text.size() && FlushFileBuffers(out);
    const auto write_error = GetLastError();
    CloseHandle(out);
    if (!written) {
        DeleteFileW(fresh.c_str());
        why = "writing failed (" + windows_error(write_error) + ")";
        return false;
    }
    if (!MoveFileExW(fresh.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const auto move_error = GetLastError();
        DeleteFileW(fresh.c_str());
        why = "the new mhw_dlss.ini could not replace the old one (" + windows_error(move_error) + ")";
        return false;
    }
    return true;
}

// Page closed: writes the ini if a value changed. The file is read again
// first, so edits made by hand meanwhile to other lines stay. Under the lock.
void save() {
    if (!state.dirty) return;
    std::string text;
    read_file(state.ini, text);  // a missing file starts empty
    const auto &s = state.settings;
    const auto value = frame_generation_value(s.frame_generation, s.multiplier, s.dynamic);
    const std::string mode = dlss_setting_value(s.dlss, s.dlss_mode);
    const std::string reflex = reflex_value(s.reflex);
    const auto target = dynamic_target_value(s.dynamic_target);
    const std::string automatic = s.generation_auto ? "on" : "off";
    auto updated = text;
    if (state.mode_dirty) updated = with_value(updated, "dlss_mode", mode);
    if (state.generation_dirty) updated = with_frame_generation(updated, value);
    if (state.target_dirty) updated = with_value(updated, "dynamic_target", target);
    if (state.auto_dirty) updated = with_value(updated, "frame_generation_auto", automatic);
    if (state.reflex_dirty) updated = with_reflex(updated, s.reflex);
    const auto check = parse_config(updated, state.folder);
    const bool generation_differs =
        check.frame_generation != s.frame_generation ||
        (check.frame_generation && (check.dynamic != s.dynamic || (!s.dynamic && check.multiplier != s.multiplier)));
    if ((state.mode_dirty && (check.dlss != s.dlss || (s.dlss && check.dlss_mode != s.dlss_mode))) ||
        (state.generation_dirty && generation_differs) ||
        (state.target_dirty && check.dynamic_target != s.dynamic_target) ||
        (state.auto_dirty && check.generation_auto != s.generation_auto) || (state.reflex_dirty && check.reflex != s.reflex)) {
        log(AL_LOG_ERROR, "menu: mhw_dlss.ini edited for dlss_mode = " + mode + ", frame_generation = " + value +
                              ", dynamic_target = " + target + ", frame_generation_auto = " + automatic + ", reflex = " + reflex +
                              " would not read back; not written");
        return;
    }
    std::string why;
    if (updated != text && !write_file(state.ini, updated, why)) {
        log(AL_LOG_WARNING, "menu: mhw_dlss.ini not written: " + why);
        return;
    }
    state.dirty = state.mode_dirty = state.generation_dirty = state.reflex_dirty = state.target_dirty = state.auto_dirty = false;
    log(AL_LOG_INFO, "menu: mhw_dlss.ini saved with dlss_mode = " + mode + ", frame_generation = " + value +
                         ", dynamic_target = " + target + ", frame_generation_auto = " + automatic + ", reflex = " + reflex);
}

// Page opened: the ini as it is now (players may edit it while the game runs),
// unless the menu holds a change not saved yet. Under the lock.
void reload() {
    std::string text;
    if (state.dirty || !read_file(state.ini, text)) return;
    const auto c = parse_config(text, state.folder);
    // While off the ini holds "off": the mode to come back with stays.
    state.settings.dlss = c.dlss;
    if (c.dlss) state.settings.dlss_mode = c.dlss_mode;
    state.settings.frame_generation = c.frame_generation;
    if (c.frame_generation) {
        state.settings.dynamic = c.dynamic;
        if (!c.dynamic) state.settings.multiplier = c.multiplier;
    }
    state.settings.generation_auto = c.generation_auto;
    state.settings.reflex = c.reflex;
    // The target choices are fixed for the page's life: another value applies
    // from the next start, when it joins them.
    if (mm::target_index(state.targets, c.dynamic_target) >= 0) state.settings.dynamic_target = c.dynamic_target;
    else if (c.dynamic_target != state.settings.dynamic_target)
        log(AL_LOG_INFO, "menu: dynamic_target = " + dynamic_target_value(c.dynamic_target) +
                             " in mhw_dlss.ini is not in the menu's list; it applies from the next start");
}

// Under the lock, with DLSS-G loaded: the frame generation choice, now.
void apply_generation_live(const mm::Settings &s) {
    if (!s.dynamic) fg::multiplier(s.multiplier);
    fg::mode(static_cast<fg::Mode>(mm::generation_mode(s)), s.dynamic_target);
}

cmfm_result_t CMFM_CALL get(void *, std::uint32_t id, cmfm_value_t *value) {
    try {
        std::lock_guard lock(state.mutex);
        if (id == frame_generation_row) value->i = state.settings.frame_generation ? 1 : 0;
        else if (id == multiplier_row) value->i = mm::generation_choice(state.settings);
        else if (id == target_row) value->i = std::max(0, mm::target_index(state.targets, state.settings.dynamic_target));
        else if (id == auto_row) value->i = state.settings.generation_auto ? 1 : 0;
        else if (id == dlss_mode_row) value->i = mm::dlss_choice(state.settings);
        else if (id == reflex_setting_row) value->i = mm::choice_of(state.settings.reflex);
        else return CMFM_ERROR;
        return CMFM_OK;
    } catch (...) {
        return CMFM_ERROR;
    }
}

cmfm_result_t CMFM_CALL validate(void *, std::uint32_t id, const cmfm_value_t *value, char *why, std::uint32_t why_size) {
    try {
        if (id == reflex_setting_row) {
            std::string reason = "Unknown Reflex mode.";
            ReflexMode mode{};
            bool generation{};
            {
                std::lock_guard lock(state.mutex);
                generation = state.settings.frame_generation;
            }
            if (mm::reflex_at(value->i, mode) && mm::reflex_allowed(mode, generation, reflex::frame_generation_loaded(), reason))
                return CMFM_OK;
            copy(why, why_size, reason);
            return CMFM_ERROR;
        }
        if (id == target_row) {
            bool known{};
            {
                std::lock_guard lock(state.mutex);
                known = value->i >= 0 && value->i < static_cast<int>(state.targets.size());
            }
            if (known) return CMFM_OK;
            copy(why, why_size, "Unknown frame rate.");
            return CMFM_ERROR;
        }
        // What the card offers (capabilities.hpp).
        std::string reason;
        const auto c = card();
        if (id == dlss_mode_row) {
            if (value->i == mm::off_choice || caps::available(c, caps::Option::dlss, 0, reason)) return CMFM_OK;
        } else if (id == frame_generation_row) {
            if (!value->i || caps::available(c, caps::Option::frame_generation, 0, reason)) return CMFM_OK;
        } else if (id == multiplier_row) {
            reason = "Unknown multiplier.";
            if (value->i == mm::dynamic_choice) {
                if (caps::available(c, caps::Option::dynamic, 0, reason)) return CMFM_OK;
            } else if (const auto m = mm::multiplier_at(value->i); m && caps::available(c, caps::Option::multiplier, m, reason)) {
                return CMFM_OK;
            }
        } else {
            return CMFM_OK;
        }
        copy(why, why_size, reason);
        return CMFM_ERROR;
    } catch (...) {
        return CMFM_ERROR;
    }
}

cmfm_result_t CMFM_CALL set(void *, std::uint32_t id, const cmfm_value_t *value) {
    try {
        std::lock_guard lock(state.mutex);
        auto &s = state.settings;
        bool live = false;
        if (id == dlss_mode_row) {
            bool dlss{};
            if (!mm::mode_at(value->i, dlss, s.dlss_mode)) return CMFM_ERROR;
            s.dlss = dlss;
            state.dirty = state.mode_dirty = true;
            // Live while DLSS is installed (the worker rebuilds it at the new
            // scale, or hands the game back for Off).
            const bool now = continuous_dlaa::live_modes();
            if (now && dlss) continuous_dlaa::request_mode(s.dlss_mode);
            else if (now) continuous_dlaa::request_off();
            log(AL_LOG_INFO, std::string("menu: dlss_mode = ") + dlss_setting_value(s.dlss, s.dlss_mode) +
                                 (now ? " (applied now)" : " (from the next start)"));
            return CMFM_OK;
        }
        if (id == reflex_setting_row) {
            ReflexMode mode{};
            std::string why;
            const bool loaded = reflex::frame_generation_loaded();
            if (!mm::reflex_at(value->i, mode) || !mm::reflex_allowed(mode, s.frame_generation, loaded, why)) return CMFM_ERROR;
            s.reflex = mode;
            state.dirty = state.reflex_dirty = true;
            // A loaded DLSS-G needs Reflex until the game closes.
            const bool now = mode != ReflexMode::off || !loaded;
            if (now) reflex::request_mode(mm::reflex_sdk_value(mode));
            log(AL_LOG_INFO, std::string("menu: reflex = ") + reflex_value(mode) +
                                 (now ? " (applied now)" : " (from the next start; frame generation runs until then)"));
            return CMFM_OK;
        }
        if (id == frame_generation_row) {
            s.frame_generation = value->i != 0;
            if (s.frame_generation && s.reflex == ReflexMode::off) {
                // Frame generation needs Reflex: on again, at once.
                s.reflex = ReflexMode::on;
                state.reflex_dirty = true;
                reflex::request_mode(mm::reflex_sdk_value(s.reflex));
                log(AL_LOG_INFO, "menu: reflex = on (frame generation needs it)");
            }
            // Live where DLSS-G loads at start: its options first, so the
            // first generated frames use them.
            live = state.fg_requested;
            if (live) {
                if (s.frame_generation) apply_generation_live(s);
                fg::enable(s.frame_generation);
            }
        } else if (id == multiplier_row) {
            if (value->i == mm::dynamic_choice) {
                s.dynamic = true;
            } else {
                const auto m = mm::multiplier_at(value->i);
                if (!m) return CMFM_ERROR;
                s.multiplier = m;
                s.dynamic = false;
            }
            live = state.fg_requested;
            if (live) apply_generation_live(s);
        } else if (id == target_row || id == auto_row) {
            if (id == target_row) {
                if (value->i < 0 || value->i >= static_cast<int>(state.targets.size())) return CMFM_ERROR;
                s.dynamic_target = state.targets[static_cast<unsigned>(value->i)];
                state.target_dirty = true;
            } else {
                s.generation_auto = value->i != 0;
                state.auto_dirty = true;
            }
            state.dirty = true;
            live = state.fg_requested;
            if (live) apply_generation_live(s);
            log(AL_LOG_INFO, (id == target_row ? "menu: dynamic_target = " + dynamic_target_value(s.dynamic_target)
                                               : std::string("menu: frame_generation_auto = ") + (s.generation_auto ? "on" : "off")) +
                                 (live ? " (applied now)" : " (from the next start)"));
            return CMFM_OK;
        } else {
            return CMFM_ERROR;
        }
        state.dirty = state.generation_dirty = true;
        log(AL_LOG_INFO, "menu: frame_generation = " + frame_generation_value(s.frame_generation, s.multiplier, s.dynamic) +
                             (live ? " (applied now)" : " (from the next start)"));
        return CMFM_OK;
    } catch (...) {
        return CMFM_ERROR;
    }
}

std::uint32_t CMFM_CALL locked(void *, std::uint32_t id, char *why, std::uint32_t why_size) {
    try {
        std::lock_guard lock(state.mutex);
        const auto &s = state.settings;
        if (id != frame_generation_row && id != multiplier_row && id != target_row && id != auto_row) return 0;
        // Known once the game's device exists (before any Mods page opens).
        const auto c = card();
        std::string missing;
        if (!state.fg_requested || c.frame_generation == caps::Support::no)
            missing = c.frame_generation == caps::Support::no ? caps::frame_generation_reason(c)
                                                              : "frame_generation_plugin = off in mhw_dlss.ini keeps DLSS Frame Generation unloaded.";
        const char *reason = !missing.empty()              ? missing.c_str()
                             : id == frame_generation_row      ? nullptr
                             : !s.frame_generation              ? "Turn frame generation on first."
                             : id == target_row && !s.dynamic ? "Used only by the Dynamic multiplier."
                             : id == auto_row && s.dynamic    ? "Dynamic adjusts itself; Auto pause is for the fixed multipliers."
                                                              : nullptr;
        if (!reason) return 0;
        copy(why, why_size, reason);
        return 1;
    } catch (...) {
        return 0;
    }
}

void CMFM_CALL get_text(void *, std::uint32_t id, char *text, std::uint32_t text_size) {
    try {
        auto v = status::snapshot();
        v.dlss_healthy = continuous_dlaa::healthy();
        const auto session = fg::status();
        v.generation_enabled = session.enabled;
        v.generating = session.on;
        v.requested = session.requested;
        v.generated = session.generated;
        v.presented_generated = session.presented_generated;
        v.generation_mode = static_cast<unsigned>(session.mode);
        v.target = session.target;
        v.dynamic_applied = session.dynamic_applied;
        v.dynamic_support = session.dynamic_support;
        v.dynamic_refused = session.dynamic_refused;
        v.presented_now = session.presented_now;
        v.single_streak = session.single_streak;
        {
            // The monitor is looked up at most once a second (get_text runs per row).
            static std::atomic<unsigned> hz{};
            static std::atomic<std::uint64_t> checked{};
            if (const auto now = GetTickCount64(); now - checked.load() >= 1000) {
                checked.store(now);
                hz.store(frame_generation::display_refresh_hz());
            }
            v.display_hz = hz.load();
        }
        std::string line;
        switch (id) {
        case mod_row: line = status::mod_line(v); break;
        case dlss_row: line = status::dlss_line(v); break;
        case reflex_row: line = status::reflex_line(v); break;
        case generation_row: line = status::generation_line(v); break;
        case generated_row: line = status::generated_line(v); break;
        case card_row: line = caps::card_line(v.caps); break;
        case features_row: line = caps::features_line(card()); break;
        default: line = "?";
        }
        copy(text, text_size, line);
    } catch (...) {
        copy(text, text_size, "?");
    }
}

void CMFM_CALL on_event(void *, cmfm_event_t event) {
    try {
        std::lock_guard lock(state.mutex);
        if (event == CMFM_EVENT_OPENED) reload();
        else if (event == CMFM_EVENT_CLOSED) save();
    } catch (...) {
    }
}

// CMFM refuses a whole page when a text is over its limit (CMFM
// src/menu/menu.cpp: label 64 bytes, help and page help 200, page title 24).
// v0.20.0 lost its Settings page to a 230-byte help line, so every literal
// goes through these checks and an overlong text fails the build instead.
template <std::size_t N> consteval const char *label_text(const char (&text)[N]) {
    static_assert(N - 1 <= 64, "CMFM: a label is at most 64 bytes");
    return text;
}
template <std::size_t N> consteval const char *help_text(const char (&text)[N]) {
    static_assert(N - 1 <= 200, "CMFM: a help line is at most 200 bytes");
    return text;
}
template <std::size_t N> consteval const char *title_text(const char (&text)[N]) {
    static_assert(N - 1 <= 24, "CMFM: a page title is at most 24 bytes");
    return text;
}
// CHOICE names: at most 32 bytes each.
constexpr bool choice_names_fit(const char *const *names, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        if (!names[i] || std::char_traits<char>::length(names[i]) > 32) return false;
    return true;
}
static_assert(choice_names_fit(mm::mode_names, std::size(mm::mode_names)) &&
                  choice_names_fit(mm::multiplier_names, std::size(mm::multiplier_names)) &&
                  choice_names_fit(mm::reflex_names, mm::reflex_modes.size()),
              "CMFM: a choice name is at most 32 bytes");
// CMFM refuses a whole page whose CHOICE default is not one of its choices
// (v0.27.0 lost its Settings page so: Off made Performance the fifth choice).
static_assert(mm::choice_of(dlss_sdk::ReconstructionMode::performance) < static_cast<int>(std::size(mm::mode_names)) &&
                  mm::choice_of(2U) < static_cast<int>(std::size(mm::multiplier_names)) &&
                  mm::choice_of(ReflexMode::on) < static_cast<int>(mm::reflex_modes.size()),
              "CMFM: a default is one of its choices");
// The target names are built at start: "Display refresh" or "<n> fps" with n
// up to 1000, and at most CMFM_MAX_CHOICES of them.
static_assert(mm::targets.size() + 1 <= CMFM_MAX_CHOICES && max_dynamic_target <= 9999, "CMFM: target choices");

cmfm_setting_t row(std::uint32_t id, cmfm_type_t type, const char *label, const char *help, cmfm_flags_t flags = 0) {
    cmfm_setting_t s{};
    s.size = sizeof s;
    s.id = id;
    s.type = type;
    s.flags = flags;
    s.label = label;
    s.help = help;
    return s;
}

}  // namespace

void add_pages(const al_api_t *api, al_plugin_t *self, const Config &config, const std::filesystem::path &folder,
               bool fg_requested) noexcept {
    try {
        const auto *menu = static_cast<const cmfm_menu_t *>(api->get_interface(self, CMFM_MENU_INTERFACE, CMFM_MENU_VERSION));
        if (!menu || menu->size < sizeof(cmfm_menu_t) || !menu->add_page) {
            api->log(self, AL_LOG_INFO, "CMFM 1.2 is not installed; the settings stay in mhw_dlss.ini");
            return;
        }
        {
            std::lock_guard lock(state.mutex);
            state.api = api;
            state.self = self;
            state.folder = folder;
            state.ini = folder / L"mhw_dlss.ini";
            state.settings = {config.dlss_mode, config.dlss,           config.frame_generation, config.multiplier,
                              config.reflex,    config.dynamic,        config.dynamic_target,   config.generation_auto};
            state.fg_requested = fg_requested;
            state.targets = mm::target_choices(config.dynamic_target);
            state.target_names.clear();
            for (const auto target : state.targets) state.target_names.push_back(mm::target_name(target));
            state.target_pointers.clear();
            for (const auto &name : state.target_names) state.target_pointers.push_back(name.c_str());
        }
        cmfm_setting_t settings[]{
            row(upscaling_section, CMFM_TYPE_SECTION, label_text("Upscaling"), nullptr),
            row(dlss_mode_row, CMFM_TYPE_CHOICE, label_text("DLSS mode"),
                help_text("Off: the game's own anti-aliasing and resolution. DLAA renders at full resolution; Quality, Balanced "
                          "and Performance at 67%, 58% and 50%, upscaled. Applies at once.")),
            row(generation_section, CMFM_TYPE_SECTION, label_text("Frame generation"), nullptr),
            row(frame_generation_row, CMFM_TYPE_BOOL, label_text("Frame generation"),
                help_text("DLSS Frame Generation shows generated frames between the rendered ones. Needs an RTX 40 or 50 graphics "
                          "card. Applies at once."),
                fg_requested ? 0U : CMFM_FLAG_RESTART),
            row(multiplier_row, CMFM_TYPE_CHOICE, label_text("Multiplier"),
                help_text("Frames shown for each rendered frame; 3x and up need an RTX 50. Dynamic picks the count itself to reach "
                          "the target frame rate (RTX 50). Applies at once.")),
            row(target_row, CMFM_TYPE_CHOICE, label_text("Target frame rate"),
                help_text("The frame rate Dynamic aims for: the display's refresh rate or a set rate. It never goes past it, so it "
                          "adds no frames while the game runs above half of it. Applies at once.")),
            row(auto_row, CMFM_TYPE_BOOL, label_text("Auto pause"),
                help_text("NVIDIA's Auto mode for 2x to 6x: frame generation pauses by itself when the game would run faster "
                          "without it. Applies at once.")),
            row(latency_section, CMFM_TYPE_SECTION, label_text("Latency"), nullptr),
            row(reflex_setting_row, CMFM_TYPE_CHOICE, label_text("Reflex"),
                help_text("NVIDIA Reflex Low Latency shortens the frame queue; Boost also keeps the graphics card's clocks up. "
                          "Off only while frame generation is off (it needs Reflex). Applies at once.")),
        };
        const auto setting = [&settings](std::uint32_t id) -> cmfm_setting_t & {
            for (auto &s : settings)
                if (s.id == id) return s;
            throw std::logic_error("menu row");
        };
        setting(dlss_mode_row).choices = mm::mode_names;
        setting(dlss_mode_row).choice_count = static_cast<std::uint32_t>(std::size(mm::mode_names));
        setting(dlss_mode_row).default_value.i = mm::choice_of(dlss_sdk::ReconstructionMode::performance);
        setting(frame_generation_row).default_value.i = 1;
        setting(multiplier_row).choices = mm::multiplier_names;
        setting(multiplier_row).choice_count = static_cast<std::uint32_t>(std::size(mm::multiplier_names));
        setting(multiplier_row).default_value.i = mm::choice_of(2);
        {
            std::lock_guard lock(state.mutex);
            setting(target_row).choices = state.target_pointers.data();
            setting(target_row).choice_count = static_cast<std::uint32_t>(state.target_pointers.size());
            setting(target_row).default_value.i = 0;
        }
        setting(auto_row).default_value.i = 0;
        setting(reflex_setting_row).choices = mm::reflex_names;
        setting(reflex_setting_row).choice_count = static_cast<std::uint32_t>(mm::reflex_modes.size());
        setting(reflex_setting_row).default_value.i = mm::choice_of(ReflexMode::on);
        const cmfm_setting_t lines[]{
            row(mod_row, CMFM_TYPE_TEXT, label_text("Mod"), help_text("The mod's state, as Astera Loader shows it."), CMFM_FLAG_LIVE),
            row(dlss_row, CMFM_TYPE_TEXT, label_text("DLSS"), help_text("The DLSS mode and the render and output resolutions."), CMFM_FLAG_LIVE),
            row(reflex_row, CMFM_TYPE_TEXT, label_text("Reflex"), help_text("NVIDIA Reflex, as chosen in Settings."), CMFM_FLAG_LIVE),
            row(generation_row, CMFM_TYPE_TEXT, label_text("Frame generation"), help_text("Frames shown for each rendered frame right now."),
                CMFM_FLAG_LIVE),
            row(generated_row, CMFM_TYPE_TEXT, label_text("Generated frames"), help_text("Generated frames shown since the game started."),
                CMFM_FLAG_LIVE),
            row(card_row, CMFM_TYPE_TEXT, label_text("Graphics card"), help_text("The graphics card the game uses, its memory and driver.")),
            row(features_row, CMFM_TYPE_TEXT, label_text("Supports"),
                help_text("What this graphics card, driver and Windows offer this mod. Settings it cannot use are locked with the reason.")),
        };

        cmfm_page_t page{};
        page.size = sizeof page;
        page.plugin = "mhw_dlss";
        page.mod = "DLSS";
        page.version = version;
        page.user = &state;
        page.on_event = on_event;
        page.get_text = get_text;
        unsigned added = 0;
        char why[256]{};
        const auto add = [&](const char *title) {
            if (menu->add_page(&page, why, sizeof why)) ++added;
            else log(AL_LOG_WARNING, std::string("menu: CMFM refused the page ") + title + ": " + why);
        };

        page.title = title_text("Settings");
        page.help = help_text("DLSS, Reflex and frame generation. The settings are kept in mhw_dlss.ini.");
        page.settings = settings;
        page.setting_count = static_cast<std::uint32_t>(std::size(settings));
        page.get = get;
        page.set = set;
        page.validate = validate;
        page.locked = locked;
        add("Settings");

        page.title = title_text("Status");
        page.help = help_text("What the mod is doing right now.");
        page.settings = lines;
        page.setting_count = static_cast<std::uint32_t>(std::size(lines));
        page.get = nullptr;
        page.set = nullptr;
        page.validate = nullptr;
        page.locked = nullptr;
        add("Status");

        log(AL_LOG_INFO, "menu: " + std::to_string(added) + " of 2 pages added to CMFM");
    } catch (...) {
        if (api) api->log(self, AL_LOG_WARNING, "menu: the CMFM pages could not be added");
    }
}

}  // namespace mhw::alpha::menu
