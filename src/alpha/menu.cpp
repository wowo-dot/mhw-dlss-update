#include "menu.hpp"
#include <windows.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include "menu_model.hpp"
#include "status.hpp"
#include "version.hpp"
#include "../continuous_dlaa/runtime.hpp"
#include "../frame_generation/game_session.hpp"
#include "../../third_party/cmfm/cmfm.h"

namespace mhw::alpha::menu {
namespace {
namespace mm = menu_model;
namespace fg = frame_generation::game;

enum Row : std::uint32_t {
    frame_generation_row = 1,
    multiplier_row,
    mod_row = 10,
    dlss_row,
    reflex_row,
    generation_row,
    generated_row,
};

// Touched on CMFM's menu thread (and once by add_pages before any page exists).
struct State {
    std::mutex mutex;
    const al_api_t *api{};
    al_plugin_t *self{};
    std::filesystem::path folder, ini;
    mm::Settings settings;  // what the ini holds, or will hold once saved
    bool dirty{};           // changed since the last save
    bool fg_loaded{};       // DLSS-G loaded at start: the multiplier applies live
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
    const auto value = frame_generation_value(state.settings.frame_generation, state.settings.multiplier);
    const auto updated = with_frame_generation(text, value);
    const auto check = parse_config(updated, state.folder);
    if (check.frame_generation != state.settings.frame_generation ||
        (check.frame_generation && check.multiplier != state.settings.multiplier)) {
        log(AL_LOG_ERROR, "menu: mhw_dlss.ini edited for frame_generation = " + value + " would not read back; not written");
        return;
    }
    std::string why;
    if (updated != text && !write_file(state.ini, updated, why)) {
        log(AL_LOG_WARNING, "menu: mhw_dlss.ini not written: " + why);
        return;
    }
    state.dirty = false;
    log(AL_LOG_INFO, "menu: mhw_dlss.ini saved with frame_generation = " + value);
}

// Page opened: the ini as it is now (players may edit it while the game runs),
// unless the menu holds a change not saved yet. Under the lock.
void reload() {
    std::string text;
    if (state.dirty || !read_file(state.ini, text)) return;
    const auto c = parse_config(text, state.folder);
    state.settings.frame_generation = c.frame_generation;
    if (c.frame_generation) state.settings.multiplier = c.multiplier;
}

cmfm_result_t CMFM_CALL get(void *, std::uint32_t id, cmfm_value_t *value) {
    try {
        std::lock_guard lock(state.mutex);
        if (id == frame_generation_row) value->i = state.settings.frame_generation ? 1 : 0;
        else if (id == multiplier_row) value->i = mm::choice_of(state.settings.multiplier);
        else return CMFM_ERROR;
        return CMFM_OK;
    } catch (...) {
        return CMFM_ERROR;
    }
}

cmfm_result_t CMFM_CALL validate(void *, std::uint32_t id, const cmfm_value_t *value, char *why, std::uint32_t why_size) {
    try {
        if (id != multiplier_row) return CMFM_OK;
        std::string reason = "Unknown multiplier.";
        if (const auto m = mm::multiplier_at(value->i); m && mm::multiplier_allowed(m, fg::status().maximum, reason))
            return CMFM_OK;
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
        if (id == frame_generation_row) {
            s.frame_generation = value->i != 0;
        } else if (id == multiplier_row) {
            const auto m = mm::multiplier_at(value->i);
            if (!m) return CMFM_ERROR;
            s.multiplier = m;
            live = state.fg_loaded;
            if (live) fg::multiplier(m);
        } else {
            return CMFM_ERROR;
        }
        state.dirty = true;
        log(AL_LOG_INFO, "menu: frame_generation = " + frame_generation_value(s.frame_generation, s.multiplier) +
                             (live ? " (multiplier applied now)" : " (from the next start)"));
        return CMFM_OK;
    } catch (...) {
        return CMFM_ERROR;
    }
}

std::uint32_t CMFM_CALL locked(void *, std::uint32_t id, char *why, std::uint32_t why_size) {
    try {
        std::lock_guard lock(state.mutex);
        if (id != multiplier_row || state.settings.frame_generation) return 0;
        copy(why, why_size, "Turn frame generation on first.");
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
        v.generating = session.on;
        v.requested = session.requested;
        v.generated = session.generated;
        v.presented_generated = session.presented_generated;
        std::string line;
        switch (id) {
        case mod_row: line = status::mod_line(v); break;
        case dlss_row: line = status::dlss_line(v); break;
        case reflex_row: line = status::reflex_line(v); break;
        case generation_row: line = status::generation_line(v); break;
        case generated_row: line = status::generated_line(v); break;
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
               bool fg_loaded) noexcept {
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
            state.settings = {config.frame_generation, config.multiplier};
            state.fg_loaded = fg_loaded;
        }
        cmfm_setting_t settings[]{
            row(frame_generation_row, CMFM_TYPE_BOOL, "Frame generation",
                "DLSS Frame Generation shows generated frames between the rendered ones. Needs an RTX 40 or 50 graphics card.",
                CMFM_FLAG_RESTART),
            row(multiplier_row, CMFM_TYPE_CHOICE, "Multiplier",
                "Frames shown for each rendered frame; 3x and up need an RTX 50. A change applies at once while frame "
                "generation runs."),
        };
        settings[0].default_value.i = 1;
        settings[1].choices = mm::multiplier_names;
        settings[1].choice_count = static_cast<std::uint32_t>(mm::multipliers.size());
        settings[1].default_value.i = mm::choice_of(2);
        const cmfm_setting_t lines[]{
            row(mod_row, CMFM_TYPE_TEXT, "Mod", "The mod's state, as Astera Loader shows it.", CMFM_FLAG_LIVE),
            row(dlss_row, CMFM_TYPE_TEXT, "DLSS", "Upscaling: this version runs DLSS at Performance (half resolution).",
                CMFM_FLAG_LIVE),
            row(reflex_row, CMFM_TYPE_TEXT, "Reflex", "NVIDIA Reflex Low Latency, always on.", CMFM_FLAG_LIVE),
            row(generation_row, CMFM_TYPE_TEXT, "Frame generation", "Frames shown for each rendered frame right now.",
                CMFM_FLAG_LIVE),
            row(generated_row, CMFM_TYPE_TEXT, "Generated frames", "Generated frames shown since the game started.",
                CMFM_FLAG_LIVE),
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

        page.title = "Settings";
        page.help = "DLSS, Reflex and frame generation. The settings are kept in mhw_dlss.ini.";
        page.settings = settings;
        page.setting_count = static_cast<std::uint32_t>(std::size(settings));
        page.get = get;
        page.set = set;
        page.validate = validate;
        page.locked = locked;
        add("Settings");

        page.title = "Status";
        page.help = "What the mod is doing right now.";
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
