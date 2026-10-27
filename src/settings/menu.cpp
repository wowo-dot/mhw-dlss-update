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
#include "../dlss_sdk/options.hpp"
#include "../sharpen/sharpen.hpp"
#include "../frame_generation/game_session.hpp"
#include "../reflex/reflex_mode.hpp"
#include "../../third_party/cmfm/cmfm.h"

namespace mhw::settings::menu {
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

// Setting ids, unique on their page.
enum Row : std::uint32_t {
    dlss_row = 1,
    mode_row,
    frame_generation_row,
    multiplier_row,
    pacing_row,
    target_row,
    reflex_row,
    dlss_model_row, // 0.32.0
    dlss_preset_row,
    sharpness_row,
    upscaling_section = 20,
    generation_section,
    latency_section,
    // Status page.
    mod_line_row = 40,
    dlss_line_row,
    resolution_line_row,
    generation_line_row,
    generated_line_row,
    reflex_line_row,
    card_line_row,
    memory_line_row,
    driver_line_row,
    support_line_row,
    dlss_version_line_row, // 0.32.0
    now_section = 60,
    system_section,
};
// The model's row for a Settings id.
bool model_row(std::uint32_t id, mm::Row &row) {
    switch (id) {
    case dlss_row: row = mm::Row::dlss; return true;
    case mode_row: row = mm::Row::mode; return true;
    case frame_generation_row: row = mm::Row::frame_generation; return true;
    case multiplier_row: row = mm::Row::multiplier; return true;
    case pacing_row: row = mm::Row::pacing; return true;
    case target_row: row = mm::Row::target; return true;
    case reflex_row: row = mm::Row::reflex; return true;
    case dlss_model_row: row = mm::Row::model; return true;
    case dlss_preset_row: row = mm::Row::preset; return true;
    case sharpness_row: row = mm::Row::sharpness; return true;
    default: return false;
    }
}

// Touched on CMFM's menu thread (and once by add_now before any page exists).
struct State {
    std::mutex mutex;
    const al_api_t *api{};
    al_plugin_t *self{};
    const cmfm_menu_t *menu{};
    std::filesystem::path folder, ini;
    mm::Settings settings; // what the ini holds, or will hold once saved
    bool dirty{};          // changed since the last save
    bool mode_dirty{}, generation_dirty{}, reflex_dirty{}, target_dirty{}, auto_dirty{}; // which keys
    bool model_dirty{}; // dlss_model and dlss_preset (0.32.0)
    bool sharpness_dirty{};
    bool fg_requested{}; // DLSS-G loads at start where supported: frame generation applies live
    // Fixed when the pages are added (CMFM keeps the choices): what this card
    // offers, the target frame rates, and the texts built from them.
    mm::Offer offer;
    std::vector<unsigned> targets;
    std::vector<std::string> names; // multiplier and target names
    std::vector<const char *> multiplier_pointers, target_pointers;
    std::string multiplier_help, pacing_help;
    // The pages wait for the card (card_ready(), which may come first) or
    // 20 s, then are added once.
    std::atomic<HANDLE> ready{};
    std::atomic<bool> card_known{}, added{};
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

std::string windows_error(DWORD error) {
    return "Windows error " + std::to_string(error);
}

// Replaces the ini where the file really is. Under Mod Organizer 2 that is the
// mod's own folder; a new file created through the game folder's path would
// land in MO2's overwrite folder instead. The new text goes to a file next to
// it, is flushed, then moved over the old one.
bool write_file(const std::filesystem::path &file, const std::string &text, std::string &why) {
    std::wstring target = file.wstring();
    if (const HANDLE h =
            CreateFileW(file.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
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
    const HANDLE out =
        CreateFileW(fresh.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
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
    read_file(state.ini, text); // a missing file starts empty
    const auto &s = state.settings;
    const auto value = frame_generation_value(s.frame_generation, s.multiplier, s.dynamic);
    const std::string mode = dlss_setting_value(s.dlss, s.dlss_mode);
    const std::string reflex = reflex_value(s.reflex);
    const auto target = dynamic_target_value(s.dynamic_target);
    const std::string automatic = s.generation_auto ? "on" : "off";
    const std::string model = dlss_model_value(s.cnn), preset = dlss_preset_value(s.preset);
    const auto sharpness = std::to_string(s.sharpness);
    auto updated = text;
    if (state.mode_dirty) updated = with_value(updated, "dlss_mode", mode);
    // Both keys: the preset line may hold 0.31.1's "e" (read as cnn).
    if (state.model_dirty) updated = with_value(with_value(updated, "dlss_model", model), "dlss_preset", preset);
    if (state.sharpness_dirty) updated = with_value(updated, "sharpness", sharpness);
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
        (state.auto_dirty && check.generation_auto != s.generation_auto) ||
        (state.reflex_dirty && check.reflex != s.reflex) ||
        (state.model_dirty && (check.dlss_cnn != s.cnn || check.dlss_preset != s.preset)) ||
        (state.sharpness_dirty && check.sharpness != s.sharpness)) {
        log(AL_LOG_ERROR, "menu: mhw_dlss.ini edited for dlss_mode = " + mode + ", dlss_model = " + model +
                              ", dlss_preset = " + preset + ", sharpness = " + sharpness + ", frame_generation = " +
                              value + ", dynamic_target = " + target + ", frame_generation_auto = " + automatic +
                              ", reflex = " + reflex + " would not read back; not written");
        return;
    }
    std::string why;
    if (updated != text && !write_file(state.ini, updated, why)) {
        log(AL_LOG_WARNING, "menu: mhw_dlss.ini not written: " + why);
        return;
    }
    state.dirty = state.mode_dirty = state.generation_dirty = state.reflex_dirty = state.target_dirty =
        state.auto_dirty = state.model_dirty = state.sharpness_dirty = false;
    log(AL_LOG_INFO, "menu: mhw_dlss.ini saved with dlss_mode = " + mode + ", dlss_model = " + model +
                         ", dlss_preset = " + preset + ", sharpness = " + sharpness + ", frame_generation = " + value +
                         ", dynamic_target = " + target + ", frame_generation_auto = " + automatic +
                         ", reflex = " + reflex);
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
    // Shown as the ini holds it; DLSS takes it at its next rebuild (a change
    // made in the menu rebuilds at once).
    state.settings.cnn = c.dlss_cnn;
    state.settings.preset = c.dlss_preset;
    dlss_sdk::render_preset = ngx_preset(c.dlss_cnn, c.dlss_preset);
    state.settings.sharpness = c.sharpness;
    sharpen::percent(c.sharpness);
    state.settings.frame_generation = c.frame_generation;
    if (c.frame_generation) {
        state.settings.dynamic = c.dynamic;
        if (!c.dynamic) state.settings.multiplier = c.multiplier;
    }
    state.settings.generation_auto = c.generation_auto;
    state.settings.reflex = c.reflex;
    // The target choices are fixed for the page's life: another value applies
    // from the next start, when it joins them.
    if (mm::target_index(state.targets, c.dynamic_target) >= 0)
        state.settings.dynamic_target = c.dynamic_target;
    else if (c.dynamic_target != state.settings.dynamic_target)
        log(AL_LOG_INFO, "menu: dynamic_target = " + dynamic_target_value(c.dynamic_target) +
                             " in mhw_dlss.ini is not in the menu's list; it applies from the next start");
}

// Under the lock, with DLSS-G loaded: the frame generation choice, now.
void apply_generation_live(const mm::Settings &s) {
    if (!s.dynamic) fg::multiplier(s.multiplier);
    fg::mode(static_cast<fg::Mode>(mm::generation_mode(s)), s.dynamic_target);
}

// Under the lock: the dlss_mode key, now while DLSS is installed (the worker
// rebuilds it at the new scale, or hands the game back for Off).
void apply_dlss(const mm::Settings &s) {
    state.dirty = state.mode_dirty = true;
    const bool now = continuous_dlaa::live_modes();
    if (now && s.dlss)
        continuous_dlaa::request_mode(s.dlss_mode);
    else if (now)
        continuous_dlaa::request_off();
    log(AL_LOG_INFO, std::string("menu: dlss_mode = ") + dlss_setting_value(s.dlss, s.dlss_mode) +
                         (now ? " (applied now)" : " (from the next start)"));
}

// Under the lock: the dlss_model and dlss_preset keys (0.32.0). DLSS is
// created with the NGX preset, so a running one is rebuilt at its mode; while
// off or not running yet it starts with it.
void apply_model(const mm::Settings &s) {
    state.dirty = state.model_dirty = true;
    const auto preset = ngx_preset(s.cnn, s.preset);
    const bool changed = dlss_sdk::render_preset.exchange(preset) != preset;
    const bool now = changed && continuous_dlaa::live_modes() && s.dlss && !continuous_dlaa::dlss_off();
    if (now) continuous_dlaa::request_mode(s.dlss_mode);
    log(AL_LOG_INFO, std::string("menu: dlss_model = ") + dlss_model_value(s.cnn) +
                         ", dlss_preset = " + dlss_preset_value(s.preset) +
                         (now       ? " (applied now)"
                          : changed ? " (when DLSS runs)"
                                    : " (no change to DLSS)"));
}

// Under the lock: the frame generation keys, now where DLSS-G loads at start.
void apply_generation(const mm::Settings &s, bool auto_changed) {
    state.dirty = state.generation_dirty = true;
    if (auto_changed) state.auto_dirty = true;
    const bool live = state.fg_requested;
    if (live) apply_generation_live(s);
    log(AL_LOG_INFO, "menu: frame_generation = " + frame_generation_value(s.frame_generation, s.multiplier, s.dynamic) +
                         ", frame_generation_auto = " + (s.generation_auto ? "on" : "off") +
                         (live ? " (applied now)" : " (from the next start)"));
}

cmfm_result_t CMFM_CALL get(void *, std::uint32_t id, cmfm_value_t *value) {
    try {
        std::lock_guard lock(state.mutex);
        const auto &s = state.settings;
        switch (id) {
        case dlss_row: value->i = s.dlss ? 1 : 0; break;
        case mode_row: value->i = mm::choice_of(s.dlss_mode); break;
        case frame_generation_row: value->i = s.frame_generation ? 1 : 0; break;
        case multiplier_row: value->i = mm::multiplier_index(state.offer.multipliers, s.multiplier); break;
        case pacing_row: value->i = static_cast<int>(mm::pacing(s, state.offer.dynamic)); break;
        case target_row: value->i = std::max(0, mm::target_index(state.targets, s.dynamic_target)); break;
        case reflex_row: value->i = mm::choice_of(s.reflex); break;
        case dlss_model_row: value->i = s.cnn ? 1 : 0; break;
        case dlss_preset_row: value->i = mm::preset_index(s.preset); break;
        case sharpness_row: value->i = mm::sharpness_shown(s.sharpness); break;
        default: return CMFM_ERROR;
        }
        return CMFM_OK;
    } catch (...) {
        return CMFM_ERROR;
    }
}

// Also guards CMFM's "Reset to defaults", which sets locked rows too: a value
// the row could not take now is refused with the row's reason.
cmfm_result_t CMFM_CALL validate(void *, std::uint32_t id, const cmfm_value_t *value, char *why,
                                 std::uint32_t why_size) {
    try {
        std::string reason;
        const auto c = card();
        std::lock_guard lock(state.mutex);
        const auto &s = state.settings;
        const auto refuse = [&](std::string_view text) {
            copy(why, why_size, text);
            return CMFM_ERROR;
        };
        switch (id) {
        case dlss_row:
        case mode_row:
            if ((id == mode_row || value->i) && !caps::available(c, caps::Option::dlss, 0, reason))
                return refuse(reason);
            return id == mode_row && (value->i < 0 || value->i >= static_cast<int>(mm::modes.size()))
                       ? refuse("Unknown mode.")
                       : CMFM_OK;
        case frame_generation_row:
            if (!value->i) return CMFM_OK;
            reason = mm::lock_reason(mm::Row::frame_generation, s, c, state.offer, state.fg_requested);
            return reason.empty() ? CMFM_OK : refuse(reason);
        case multiplier_row: {
            if (value->i < 0 || value->i >= static_cast<int>(state.offer.multipliers.size()))
                return refuse("Unknown multiplier.");
            // A limit DLSS-G reported after the page was added.
            const auto m = state.offer.multipliers[static_cast<unsigned>(value->i)];
            return caps::available(c, caps::Option::multiplier, m, reason) ? CMFM_OK : refuse(reason);
        }
        case pacing_row:
            if (value->i < 0 || value->i >= static_cast<int>(mm::pacing_count(state.offer.dynamic)))
                return refuse("Unknown pacing.");
            if (static_cast<mm::Pacing>(value->i) == mm::Pacing::dynamic &&
                !caps::available(c, caps::Option::dynamic, 0, reason))
                return refuse(reason);
            return CMFM_OK;
        case target_row:
            return value->i >= 0 && value->i < static_cast<int>(state.targets.size()) ? CMFM_OK
                                                                                      : refuse("Unknown frame rate.");
        case reflex_row: {
            ReflexMode mode{};
            reason = "Unknown Reflex mode.";
            if (mm::reflex_at(value->i, mode) &&
                mm::reflex_allowed(mode, s.frame_generation, reflex::frame_generation_loaded(), reason))
                return CMFM_OK;
            return refuse(reason);
        }
        // As Mode: kept while DLSS is off. The preset is kept while CNN runs
        // (Reset to defaults sets it then), for the way back to Transformer.
        case dlss_model_row:
        case dlss_preset_row:
            if (!caps::available(c, caps::Option::dlss, 0, reason)) return refuse(reason);
            if (id == dlss_model_row) return value->i == 0 || value->i == 1 ? CMFM_OK : refuse("Unknown model.");
            return value->i >= 0 && value->i < static_cast<int>(mm::presets.size()) ? CMFM_OK
                                                                                    : refuse("Unknown preset.");
        case sharpness_row:
            if (!caps::available(c, caps::Option::dlss, 0, reason)) return refuse(reason);
            return value->i >= 0 && value->i <= static_cast<int>(max_sharpness) ? CMFM_OK : refuse("From 0 to 100 %.");
        default: return CMFM_OK;
        }
    } catch (...) {
        return CMFM_ERROR;
    }
}

cmfm_result_t CMFM_CALL set(void *, std::uint32_t id, const cmfm_value_t *value) {
    try {
        std::lock_guard lock(state.mutex);
        auto &s = state.settings;
        switch (id) {
        case dlss_row:
            s.dlss = value->i != 0;
            apply_dlss(s);
            return CMFM_OK;
        case mode_row:
            if (!mm::mode_at(value->i, s.dlss_mode)) return CMFM_ERROR;
            apply_dlss(s);
            return CMFM_OK;
        case frame_generation_row:
            s.frame_generation = value->i != 0;
            if (s.frame_generation && s.reflex == ReflexMode::off) {
                // Frame generation needs Reflex: on again, at once.
                s.reflex = ReflexMode::on;
                state.dirty = state.reflex_dirty = true;
                reflex::request_mode(mm::reflex_sdk_value(s.reflex));
                log(AL_LOG_INFO, "menu: reflex = on (frame generation needs it)");
            }
            // Live where DLSS-G loads at start: its options first, so the
            // first generated frames use them.
            if (state.fg_requested && s.frame_generation) apply_generation_live(s);
            if (state.fg_requested) fg::enable(s.frame_generation);
            state.dirty = state.generation_dirty = true;
            log(AL_LOG_INFO, std::string("menu: frame_generation = ") +
                                 frame_generation_value(s.frame_generation, s.multiplier, s.dynamic) +
                                 (state.fg_requested ? " (applied now)" : " (from the next start)"));
            return CMFM_OK;
        case multiplier_row:
            if (value->i < 0 || value->i >= static_cast<int>(state.offer.multipliers.size())) return CMFM_ERROR;
            s.multiplier = state.offer.multipliers[static_cast<unsigned>(value->i)];
            apply_generation(s, false);
            return CMFM_OK;
        case pacing_row:
            if (value->i < 0 || value->i >= static_cast<int>(mm::pacing_count(state.offer.dynamic))) return CMFM_ERROR;
            mm::set_pacing(s, static_cast<mm::Pacing>(value->i));
            apply_generation(s, true);
            return CMFM_OK;
        case target_row: {
            if (value->i < 0 || value->i >= static_cast<int>(state.targets.size())) return CMFM_ERROR;
            s.dynamic_target = state.targets[static_cast<unsigned>(value->i)];
            state.dirty = state.target_dirty = true;
            if (state.fg_requested) apply_generation_live(s);
            log(AL_LOG_INFO, "menu: dynamic_target = " + dynamic_target_value(s.dynamic_target) +
                                 (state.fg_requested ? " (applied now)" : " (from the next start)"));
            return CMFM_OK;
        }
        case reflex_row: {
            ReflexMode mode{};
            std::string why;
            const bool loaded = reflex::frame_generation_loaded();
            if (!mm::reflex_at(value->i, mode) || !mm::reflex_allowed(mode, s.frame_generation, loaded, why))
                return CMFM_ERROR;
            s.reflex = mode;
            state.dirty = state.reflex_dirty = true;
            // A loaded DLSS-G needs Reflex until the game closes.
            const bool now = mode != ReflexMode::off || !loaded;
            if (now) reflex::request_mode(mm::reflex_sdk_value(mode));
            log(AL_LOG_INFO, std::string("menu: reflex = ") + reflex_value(mode) +
                                 (now ? " (applied now)" : " (from the next start; frame generation runs until then)"));
            return CMFM_OK;
        }
        case dlss_model_row:
            if (value->i != 0 && value->i != 1) return CMFM_ERROR;
            s.cnn = value->i == 1;
            apply_model(s);
            return CMFM_OK;
        case dlss_preset_row:
            if (!mm::preset_at(value->i, s.preset)) return CMFM_ERROR;
            apply_model(s);
            return CMFM_OK;
        case sharpness_row:
            // Live: the next frame's final scene is sharpened at it (0 off).
            if (value->i < 0 || value->i > static_cast<int>(max_sharpness)) return CMFM_ERROR;
            s.sharpness = static_cast<unsigned>(value->i);
            state.dirty = state.sharpness_dirty = true;
            sharpen::percent(s.sharpness);
            log(AL_LOG_INFO, "menu: sharpness = " + std::to_string(s.sharpness) + " (applied now)");
            return CMFM_OK;
        default: return CMFM_ERROR;
        }
    } catch (...) {
        return CMFM_ERROR;
    }
}

// Greyed with the reason, never hidden (the player sees what exists and why
// it cannot change now).
std::uint32_t CMFM_CALL locked(void *, std::uint32_t id, char *why, std::uint32_t why_size) {
    try {
        mm::Row row{};
        if (!model_row(id, row)) return 0;
        const auto c = card();
        std::lock_guard lock(state.mutex);
        const auto reason = mm::lock_reason(row, state.settings, c, state.offer, state.fg_requested);
        if (reason.empty()) return 0;
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
        std::string line;
        switch (id) {
        case mod_line_row: line = status::mod_line(v); break;
        case dlss_line_row: line = status::dlss_line(v); break;
        case resolution_line_row: line = status::resolution_line(v); break;
        case generation_line_row: line = status::generation_line(v); break;
        case generated_line_row: line = status::generated_line(v); break;
        case reflex_line_row: line = status::reflex_line(v); break;
        case card_line_row: line = caps::card_name_line(v.caps); break;
        case memory_line_row: line = caps::memory_line(v.caps); break;
        case driver_line_row: line = caps::driver_line(v.caps); break;
        case support_line_row: line = caps::generation_support_line(card()); break;
        case dlss_version_line_row: line = status::dlss_version_line(v); break;
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
        if (event == CMFM_EVENT_OPENED)
            reload();
        else if (event == CMFM_EVENT_CLOSED)
            save();
    } catch (...) {}
}

// CMFM refuses a whole page when a text is over its limit (CMFM
// src/menu/menu.cpp: label 64 bytes, help and page help 200, page title 24).
// v0.20.0 lost its Settings page to a 230-byte help line, so every literal
// goes through these checks and an overlong text fails the build instead.
// Help lines stay well under CMFM's limit: the game's help bar squeezes a
// longer one to fit (dev.8's 198-byte Preset help was hard to read).
inline constexpr std::size_t help_fits = 120;
static_assert(help_fits <= 200, "CMFM: a help line is at most 200 bytes");
template <std::size_t N> consteval const char *label_text(const char (&text)[N]) {
    static_assert(N - 1 <= 64, "CMFM: a label is at most 64 bytes");
    return text;
}
template <std::size_t N> consteval const char *help_text(const char (&text)[N]) {
    static_assert(N - 1 <= help_fits, "a help line is at most 120 bytes (the game's help bar)");
    return text;
}
template <std::size_t N> consteval const char *title_text(const char (&text)[N]) {
    static_assert(N - 1 <= 24, "CMFM: a page title is at most 24 bytes");
    return text;
}
// Help lines built at runtime: cut at CMFM's 200 bytes (never needed: the
// longest variant is checked below).
std::string help_built(std::string text) {
    if (text.size() > 200) text.resize(200);
    return text;
}
constexpr std::string_view pacing_help_dynamic =
    "Fixed: always the multiplier. Auto pause: NVIDIA pauses it when it would slow the game. Dynamic: aims at the target.";
constexpr std::string_view pacing_help_fixed =
    "Fixed: always the multiplier. Auto pause: NVIDIA pauses it when it would slow the game.";
constexpr std::string_view multiplier_help_most =
    "Frames shown for each rendered frame; this graphics card shows up to 6x.";
static_assert(pacing_help_dynamic.size() <= help_fits && pacing_help_fixed.size() <= help_fits &&
                  multiplier_help_most.size() <= help_fits,
              "a help line is at most 120 bytes (the game's help bar)");
// CHOICE names: at most 32 bytes each.
constexpr bool choice_names_fit(const char *const *names, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i)
        if (!names[i] || std::char_traits<char>::length(names[i]) > 32) return false;
    return true;
}
static_assert(choice_names_fit(mm::mode_names, std::size(mm::mode_names)) &&
                  choice_names_fit(mm::pacing_names, std::size(mm::pacing_names)) &&
                  choice_names_fit(mm::reflex_names, mm::reflex_modes.size()) &&
                  choice_names_fit(mm::model_names, std::size(mm::model_names)) &&
                  choice_names_fit(mm::preset_names, std::size(mm::preset_names)),
              "CMFM: a choice name is at most 32 bytes");
// CMFM refuses a whole page whose CHOICE default is not one of its choices
// (v0.27.0 lost its Settings page so).
static_assert(mm::choice_of(dlss_sdk::ReconstructionMode::performance) < static_cast<int>(std::size(mm::mode_names)) &&
                  static_cast<unsigned>(mm::Pacing::fixed) < mm::pacing_count(false) &&
                  mm::choice_of(ReflexMode::on) < static_cast<int>(mm::reflex_modes.size()) &&
                  mm::preset_index(Config{}.dlss_preset) < static_cast<int>(mm::presets.size()) &&
                  std::size(mm::model_names) == 2,
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

// Adds both pages once, with the choices this card offers (as known now).
void add_now(unsigned waited_ms) noexcept {
    if (state.added.exchange(true)) return;
    try {
        const auto c = card();
        {
            std::lock_guard lock(state.mutex);
            state.offer = mm::offer_for(c);
            state.names.clear();
            state.names.reserve(state.offer.multipliers.size() + state.targets.size());
            for (const auto m : state.offer.multipliers) state.names.push_back(mm::multiplier_name(m));
            for (const auto t : state.targets) state.names.push_back(mm::target_name(t));
            state.multiplier_pointers.clear();
            state.target_pointers.clear();
            for (std::size_t i = 0; i < state.names.size(); ++i)
                (i < state.offer.multipliers.size() ? state.multiplier_pointers : state.target_pointers)
                    .push_back(state.names[i].c_str());
            const auto most = state.offer.multipliers.back();
            state.multiplier_help = help_built(
                "Frames shown for each rendered frame" +
                (c.max_multiplier ? "; this graphics card shows up to " + mm::multiplier_name(most) : std::string()) +
                ".");
            state.pacing_help = std::string(state.offer.dynamic ? pacing_help_dynamic : pacing_help_fixed);
        }
        cmfm_setting_t settings[]{
            row(upscaling_section, CMFM_TYPE_SECTION, label_text("Upscaling"), nullptr),
            row(dlss_row, CMFM_TYPE_BOOL, label_text("DLSS"),
                help_text("NVIDIA DLSS Super Resolution and DLAA. Off: the game's own anti-aliasing and resolution, as "
                          "without the mod.")),
            row(mode_row, CMFM_TYPE_CHOICE, label_text("Mode"),
                help_text("DLAA renders at full resolution; Quality, Balanced and Performance at 67%, 58% and 50%, "
                          "upscaled by DLSS.")),
            row(dlss_model_row, CMFM_TYPE_CHOICE, label_text("Model"),
                help_text(
                    "Transformer: DLSS 4's model, sharper and steadier. CNN: the older one, cheaper on RTX 20 and 30 "
                    "but softer.")),
            row(dlss_preset_row, CMFM_TYPE_CHOICE, label_text("Preset"),
                help_text(
                    "Recommended: NVIDIA's per mode (K, M at Performance). L and M: less ghosting, slower on RTX 20 "
                    "and 30. J: like K.")),
            row(sharpness_row, CMFM_TYPE_INT, label_text("Sharpness"),
                help_text("NVIDIA's NVSharpen on the game's image while DLSS draws; the HUD is not sharpened. 0 % is "
                          "off.")),
            row(generation_section, CMFM_TYPE_SECTION, label_text("Frame generation"), nullptr),
            row(frame_generation_row, CMFM_TYPE_BOOL, label_text("Frame generation"),
                help_text(
                    "DLSS Frame Generation (RTX 40 and 50) adds frames between the rendered ones; needs DLSS. Its "
                    "model: the NVIDIA App.")),
            row(multiplier_row, CMFM_TYPE_CHOICE, label_text("Multiplier"), nullptr),
            row(pacing_row, CMFM_TYPE_CHOICE, label_text("Pacing"), nullptr),
            row(target_row, CMFM_TYPE_CHOICE, label_text("Target frame rate"),
                help_text(
                    "The frame rate Dynamic pacing aims for: the display's refresh rate or a set rate. It never goes "
                    "past it.")),
            row(latency_section, CMFM_TYPE_SECTION, label_text("Latency"), nullptr),
            row(reflex_row, CMFM_TYPE_CHOICE, label_text("Reflex"),
                help_text(
                    "NVIDIA Reflex shortens the frame queue; Boost also keeps the clocks up. Off only while frame "
                    "generation is off.")),
        };
        const auto setting = [&settings](std::uint32_t id) -> cmfm_setting_t & {
            for (auto &s : settings)
                if (s.id == id) return s;
            throw std::logic_error("menu row");
        };
        // CMFM's "Reset to defaults": the shipped ini's values (Config's
        // defaults): DLSS, frame generation and Reflex off.
        const Config defaults{};
        setting(dlss_row).default_value.i = defaults.dlss ? 1 : 0;
        setting(mode_row).choices = mm::mode_names;
        setting(mode_row).choice_count = static_cast<std::uint32_t>(std::size(mm::mode_names));
        setting(mode_row).default_value.i = mm::choice_of(defaults.dlss_mode);
        setting(frame_generation_row).default_value.i = defaults.frame_generation ? 1 : 0;
        setting(pacing_row).choices = mm::pacing_names;
        setting(reflex_row).choices = mm::reflex_names;
        setting(reflex_row).choice_count = static_cast<std::uint32_t>(mm::reflex_modes.size());
        setting(reflex_row).default_value.i = mm::choice_of(defaults.reflex);
        setting(dlss_model_row).choices = mm::model_names;
        setting(dlss_model_row).choice_count = static_cast<std::uint32_t>(std::size(mm::model_names));
        setting(dlss_model_row).default_value.i = defaults.dlss_cnn ? 1 : 0;
        setting(dlss_preset_row).choices = mm::preset_names;
        setting(dlss_preset_row).choice_count = static_cast<std::uint32_t>(std::size(mm::preset_names));
        setting(dlss_preset_row).default_value.i = mm::preset_index(defaults.dlss_preset);
        setting(sharpness_row).min = 0;
        setting(sharpness_row).max = max_sharpness;
        setting(sharpness_row).step = mm::sharpness_step;
        setting(sharpness_row).unit = "%";
        setting(sharpness_row).default_value.i = mm::sharpness_shown(defaults.sharpness);
        {
            std::lock_guard lock(state.mutex);
            setting(multiplier_row).help = state.multiplier_help.c_str();
            setting(multiplier_row).choices = state.multiplier_pointers.data();
            setting(multiplier_row).choice_count = static_cast<std::uint32_t>(state.multiplier_pointers.size());
            setting(multiplier_row).default_value.i =
                mm::multiplier_index(state.offer.multipliers, defaults.multiplier);
            setting(pacing_row).help = state.pacing_help.c_str();
            setting(pacing_row).choice_count = mm::pacing_count(state.offer.dynamic);
            setting(pacing_row).default_value.i = static_cast<int>(mm::Pacing::fixed);
            setting(target_row).choices = state.target_pointers.data();
            setting(target_row).choice_count = static_cast<std::uint32_t>(state.target_pointers.size());
            setting(target_row).default_value.i = 0;
        }
        const cmfm_setting_t lines[]{
            row(now_section, CMFM_TYPE_SECTION, label_text("Now"), nullptr),
            row(mod_line_row, CMFM_TYPE_TEXT, label_text("Mod"),
                help_text("The mod's state, as Astera Loader shows it."), CMFM_FLAG_LIVE),
            row(dlss_line_row, CMFM_TYPE_TEXT, label_text("DLSS"),
                help_text("The DLSS mode and model. Off: the game's own anti-aliasing and resolution."),
                CMFM_FLAG_LIVE),
            row(resolution_line_row, CMFM_TYPE_TEXT, label_text("Resolution"),
                help_text("The resolution the game renders at, and the one DLSS outputs."), CMFM_FLAG_LIVE),
            row(generation_line_row, CMFM_TYPE_TEXT, label_text("Frame generation"),
                help_text(
                    "Frames shown for each rendered frame now. DLSS Frame Generation generates only while the game window "
                    "has focus."),
                CMFM_FLAG_LIVE),
            row(generated_line_row, CMFM_TYPE_TEXT, label_text("Generated frames"),
                help_text("Generated frames shown since the game started."), CMFM_FLAG_LIVE),
            row(reflex_line_row, CMFM_TYPE_TEXT, label_text("Reflex"),
                help_text("NVIDIA Reflex: On (low latency), Boost (also keeps the clocks up) or Off."), CMFM_FLAG_LIVE),
            row(system_section, CMFM_TYPE_SECTION, label_text("System"), nullptr),
            row(card_line_row, CMFM_TYPE_TEXT, label_text("Graphics card"),
                help_text("The graphics card the game uses.")),
            row(memory_line_row, CMFM_TYPE_TEXT, label_text("Video memory"),
                help_text("The graphics card's own memory.")),
            row(driver_line_row, CMFM_TYPE_TEXT, label_text("Driver"), help_text("The NVIDIA driver version.")),
            row(dlss_version_line_row, CMFM_TYPE_TEXT, label_text("DLSS version"),
                help_text(
                    "The DLSS file that runs: the mod's own, or the one the NVIDIA App's DLSS override picked (310 or "
                    "newer runs)."),
                CMFM_FLAG_LIVE),
            row(support_line_row, CMFM_TYPE_TEXT, label_text("Frame generation support"),
                help_text(
                    "What DLSS Frame Generation can do here: the most frames per rendered frame, and Dynamic pacing."),
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
            if (state.menu->add_page(&page, why, sizeof why))
                ++added;
            else
                log(AL_LOG_WARNING, std::string("menu: CMFM refused the page ") + title + ": " + why);
        };

        page.title = title_text("Settings");
        page.help = help_text("DLSS, frame generation and Reflex. Changes apply at once and are kept in mhw_dlss.ini.");
        page.settings = settings;
        page.setting_count = static_cast<std::uint32_t>(std::size(settings));
        page.get = get;
        page.set = set;
        page.validate = validate;
        page.locked = locked;
        add("Settings");

        page.title = title_text("Status");
        page.help = help_text("What the mod is doing now, and what this graphics card offers.");
        page.settings = lines;
        page.setting_count = static_cast<std::uint32_t>(std::size(lines));
        page.get = nullptr;
        page.set = nullptr;
        page.validate = nullptr;
        page.locked = nullptr;
        add("Status");

        std::string offered;
        for (const auto m : state.offer.multipliers) offered += (offered.empty() ? "" : "/") + mm::multiplier_name(m);
        log(AL_LOG_INFO, "menu: " + std::to_string(added) + " of 2 pages added to CMFM after " +
                             std::to_string(waited_ms) + " ms (card " + (c.known ? "known" : "not known yet") +
                             "; multipliers " + offered + (state.offer.dynamic ? ", Dynamic" : ", no Dynamic") + ")");
    } catch (...) {
        log(AL_LOG_WARNING, "menu: the CMFM pages could not be added");
    }
}

DWORD WINAPI page_thread(void *) {
    const auto began = GetTickCount64();
    // The card is read when the game creates its device, seconds before its
    // title menu; without it (no device yet, or the worker stopped early) the
    // pages come after 20 s with what is known.
    if (!state.card_known.load()) WaitForSingleObject(state.ready.load(), 20000);
    add_now(static_cast<unsigned>(GetTickCount64() - began));
    return 0;
}

} // namespace

void add_pages(const al_api_t *api, al_plugin_t *self, const Config &config, const std::filesystem::path &folder,
               bool fg_requested) noexcept {
    try {
        const auto *menu =
            static_cast<const cmfm_menu_t *>(api->get_interface(self, CMFM_MENU_INTERFACE, CMFM_MENU_VERSION));
        if (!menu || menu->size < sizeof(cmfm_menu_t) || !menu->add_page) {
            api->log(self, AL_LOG_INFO, "CMFM 1.2 is not installed; the settings stay in mhw_dlss.ini");
            return;
        }
        {
            std::lock_guard lock(state.mutex);
            state.api = api;
            state.self = self;
            state.menu = menu;
            state.folder = folder;
            state.ini = folder / L"mhw_dlss.ini";
            state.settings = {config.dlss_mode, config.dlss,        config.frame_generation, config.multiplier,
                              config.reflex,    config.dynamic,     config.dynamic_target,   config.generation_auto,
                              config.dlss_cnn,  config.dlss_preset, config.sharpness};
            state.fg_requested = fg_requested;
            state.targets = mm::target_choices(config.dynamic_target);
        }
        state.ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        const HANDLE thread = state.ready.load() ? CreateThread(nullptr, 0, page_thread, nullptr, 0, nullptr) : nullptr;
        if (thread)
            CloseHandle(thread);
        else
            add_now(0); // no thread: at once, before the card is known
    } catch (...) {
        if (api) api->log(self, AL_LOG_WARNING, "menu: the CMFM pages could not be added");
    }
}

void card_ready() noexcept {
    state.card_known = true;
    if (const auto ready = state.ready.load()) SetEvent(ready);
}

} // namespace mhw::settings::menu
