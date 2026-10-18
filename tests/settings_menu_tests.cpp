// The mod's CMFM pages: the ini edit, the settings model and the status lines.
#include "../src/settings/capabilities.hpp"
#include "../src/settings/config.hpp"
#include "../src/settings/menu_model.hpp"
#include "../src/settings/status.hpp"
#include "../src/continuous_dlaa/game_options.hpp"
#include <algorithm>
#include <cstdio>
#include <vector>

namespace {
int failures{};
void check(bool ok, const char *what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

// tools/package_release.py's INI, as written (CRLF).
constexpr std::string_view shipped =
    "# MHW DLSS / Frame Generation (alpha) configuration.\r\n"
    "# Lines are \"key = value\"; '#' or ';' starts a comment. Folders are relative to this folder.\r\n"
    "dlss_mode = performance    ; dlaa, quality, balanced, performance (applies from the next start)\r\n"
    "frame_generation = 2x      ; off, 2x, 3x, 4x, 6x (3x and above need an RTX 50 series GPU)\r\n"
    "reflex = on                ; on (off is not available yet)\r\n"
    "runtime_dir = .            ; folder with the NVIDIA DLLs shipped in this package\r\n"
    "log_dir = logs             ; one log file per launch\r\n";

void ini_edits() {
    namespace a = mhw::settings;
    const std::filesystem::path folder = L"D:\\mods\\dlss";
    {
        const auto out = a::with_frame_generation(shipped, "off");
        std::string expected(shipped);
        expected.replace(expected.find("= 2x") + 2, 2, "off");
        check(out == expected, "shipped ini: only the value changes, comment and CRLF stay");
        const auto c = a::parse_config(out, folder);
        check(!c.frame_generation && c.warnings.empty(), "shipped ini reads back as off");
        const auto back = a::with_frame_generation(out, "2x");
        check(back == shipped, "off then 2x restores the shipped bytes");
    }
    {
        const auto out = a::with_frame_generation("reflex = on\r\nlog_dir = logs", "4x");
        check(out == "reflex = on\r\nlog_dir = logs\r\nframe_generation = 4x\r\n", "missing key: appended with CRLF");
        const auto lf = a::with_frame_generation("reflex = on\n", "3x");
        check(lf == "reflex = on\nframe_generation = 3x\n", "missing key: appended with LF");
        check(a::with_frame_generation("", "off") == "frame_generation = off\n", "empty file");
    }
    {
        const auto out = a::with_frame_generation("frame_generation = 2x\nFRAME_GENERATION=4X ; mine\n", "6x");
        check(out == "frame_generation = 2x\nFRAME_GENERATION=6x ; mine\n",
              "the last line wins, its case and comment stay");
        const auto c = a::parse_config(out, folder);
        check(c.frame_generation && c.multiplier == 6, "reads back as 6x");
    }
    {
        check(a::with_frame_generation("frame_generation =\r\n", "2x") == "frame_generation =2x\r\n", "empty value");
        check(a::with_frame_generation("frame_generation = off", "3x") == "frame_generation = 3x", "no final newline");
        check(a::with_frame_generation("# frame_generation = 4x\n", "2x") ==
                  "# frame_generation = 4x\nframe_generation = 2x\n",
              "a commented-out line is not a setting");
    }
    check(a::frame_generation_value(false, 4) == "off" && a::frame_generation_value(true, 3) == "3x", "ini values");
    {
        // The shipped ini's dlss_mode changes in place; an ini without one gains it.
        const auto out = a::with_value(shipped, "dlss_mode", "quality");
        std::string expected(shipped);
        expected.replace(expected.find("= performance") + 2, 11, "quality");
        check(out == expected, "dlss_mode replaced in the shipped ini, comment and CRLF kept");
        check(a::with_value("reflex = on\r\n", "dlss_mode", "dlaa") == "reflex = on\r\ndlss_mode = dlaa\r\n",
              "dlss_mode added");
        const auto c = a::parse_config(out, folder);
        check(c.dlss_mode == mhw::dlss_sdk::ReconstructionMode::quality && c.frame_generation && c.warnings.empty(),
              "reads back");
        const auto again =
            a::with_value("dlss_mode = quality   ; dlaa, quality\nframe_generation = off\n", "dlss_mode", "dlaa");
        check(again == "dlss_mode = dlaa   ; dlaa, quality\nframe_generation = off\n",
              "dlss_mode value replaced in place");
    }
}

void settings_model() {
    namespace m = mhw::settings::menu_model;
    namespace c = mhw::settings::capabilities;
    using mhw::dlss_sdk::ReconstructionMode;
    std::string why;
    {
        // The game's own DLSS rules for its graphics menu (game_options.hpp,
        // v0.27.6): the two rows its DLSS locks, with its own help texts.
        namespace go = mhw::continuous_dlaa::game_options;
        check(go::locked_row(15) && go::locked_row(22) && !go::locked_row(16) && !go::locked_row(32) &&
                  !go::locked_row(0),
              "locked rows: resolution scaling and anti-aliasing");
        check(go::locked_help(15, 10123) == 10552 && go::locked_help(22, 10252) == 10553 &&
                  go::locked_help(20, 10300) == 10300,
              "the game's NVIDIA DLSS help texts");
        check(go::anti_aliasing_off == 0 && go::resolution_scaling_high == 2 && go::anti_aliasing_values == 4 &&
                  go::resolution_scaling_values == 5,
              "Off, High and the value ranges");
    }
    // Mode: no Off among the modes any more (DLSS is its own switch, v0.28.0).
    check(m::choice_of(ReconstructionMode::dlaa) == 0 && m::choice_of(ReconstructionMode::performance) == 3,
          "mode choices, best first");
    ReconstructionMode mode{};
    check(m::mode_at(1, mode) && mode == ReconstructionMode::quality && !m::mode_at(4, mode) && !m::mode_at(-1, mode),
          "mode at index");

    // The multipliers follow the card.
    check(m::multiplier_choices(0) == std::vector<unsigned>{2, 3, 4, 6}, "unknown limit: every multiplier");
    check(m::multiplier_choices(2) == std::vector<unsigned>{2}, "RTX 40: 2x");
    check(m::multiplier_choices(5) == std::vector<unsigned>{2, 3, 4}, "a limit of 5: up to 4x");
    check(m::multiplier_choices(6) == std::vector<unsigned>{2, 3, 4, 6}, "RTX 50: up to 6x");
    check(m::multiplier_choices(1) == std::vector<unsigned>{2}, "never empty");
    {
        const std::vector<unsigned> four{2, 3, 4};
        check(m::multiplier_index(four, 4) == 2 && m::multiplier_index(four, 6) == 2 &&
                  m::multiplier_index(four, 2) == 0 && m::multiplier_index(four, 0) == 0,
              "multiplier index: its own, else the largest below");
        check(m::multiplier_name(6) == "6x", "multiplier name");
    }
    {
        // Pacing: Fixed, Auto pause, Dynamic.
        m::Settings s;
        check(m::pacing(s, true) == m::Pacing::fixed && m::generation_mode(s) == 0, "fixed");
        m::set_pacing(s, m::Pacing::auto_pause);
        check(s.generation_auto && !s.dynamic && m::pacing(s, true) == m::Pacing::auto_pause &&
                  m::generation_mode(s) == 1,
              "auto pause");
        m::set_pacing(s, m::Pacing::dynamic);
        check(s.dynamic && s.generation_auto && m::pacing(s, true) == m::Pacing::dynamic && m::generation_mode(s) == 2,
              "Dynamic keeps Auto pause for the way back");
        check(m::pacing(s, false) == m::Pacing::auto_pause, "Dynamic not offered: the fixed pacing shows");
        m::set_pacing(s, m::Pacing::fixed);
        check(!s.dynamic && !s.generation_auto, "fixed again");
        check(m::pacing_count(true) == 3 && m::pacing_count(false) == 2 &&
                  std::string_view(m::pacing_names[2]) == "Dynamic",
              "pacing choices");
    }
    {
        const auto plain = m::target_choices(0);
        check(plain.size() == m::targets.size() && plain.front() == 0 && m::target_index(plain, 240) == 11,
              "target choices");
        const auto custom = m::target_choices(138);
        check(custom.size() == m::targets.size() + 1 && m::target_index(custom, 138) == 7 &&
                  m::target_index(custom, 144) == 8 && std::is_sorted(custom.begin(), custom.end()),
              "an ini target joins the list in order");
        check(m::target_index(plain, 138) == -1, "an unknown target has no index");
        check(m::target_name(0) == "Display refresh" && m::target_name(240) == "240 fps", "target names");
    }

    // What the card offers (capabilities.hpp, v0.27.5).
    c::Capabilities rtx50;
    rtx50.known = true;
    rtx50.card = {"NVIDIA GeForce RTX 5090", c::nvidia, 0x2b85, 32187, 610, 88};
    rtx50.dlss = rtx50.reflex = rtx50.frame_generation = rtx50.dynamic = c::Support::yes;
    rtx50.max_multiplier = 6;
    c::Capabilities rtx40 = rtx50;
    rtx40.card = {"NVIDIA GeForce RTX 4080", c::nvidia, 0x2704, 16376, 610, 88};
    rtx40.max_multiplier = 2;
    rtx40.dynamic = c::Support::no;
    c::Capabilities rtx30 = rtx40;
    rtx30.card = {"NVIDIA GeForce RTX 3080", c::nvidia, 0x2206, 10240, 610, 88};
    rtx30.frame_generation = c::Support::no;
    rtx30.missing = c::Missing::card;
    rtx30.max_multiplier = 0;
    c::Capabilities other;
    other.known = true;
    other.card = {"AMD Radeon RX 7900 XTX", 0x1002, 0x744c, 24560, 0, 0};
    other.dlss = c::Support::no;
    {
        check(c::available(rtx50, c::Option::multiplier, 6, why), "RTX 50: 6x");
        check(!c::available(rtx40, c::Option::multiplier, 3, why) &&
                  why == "This graphics card shows 2x at most; 3x and up need an RTX 50.",
              "RTX 40 reason");
        check(!c::available(rtx40, c::Option::dynamic, 0, why) &&
                  why == "Dynamic needs an RTX 50 graphics card and NVIDIA driver 595.41 or newer.",
              "Dynamic not supported");
        check(!c::available(rtx30, c::Option::frame_generation, 0, why) &&
                  why == "DLSS Frame Generation needs an NVIDIA GeForce RTX 40 or 50 series graphics card.",
              "card reason");
        check(c::available(rtx30, c::Option::dlss, 0, why) && c::available(rtx30, c::Option::reflex, 0, why),
              "DLSS and Reflex stay");
        check(!c::available(other, c::Option::dlss, 0, why) && why == "DLSS needs an NVIDIA GeForce RTX graphics card.",
              "not NVIDIA");
        c::Capabilities unknown;
        check(c::available(unknown, c::Option::multiplier, 6, why) && c::available(unknown, c::Option::dynamic, 0, why),
              "unknown support allows (the mod refuses at runtime)");
        auto old = rtx30;
        old.missing = c::Missing::driver;
        old.driver_required_major = 615;
        old.driver_required_minor = 5;
        check(c::frame_generation_reason(old) ==
                  "DLSS Frame Generation needs NVIDIA driver 615.05 or newer (installed: 610.88).",
              "driver reason");
    }
    {
        // The page's offer, built from the card when it is added.
        const auto o50 = m::offer_for(rtx50), o40 = m::offer_for(rtx40), o30 = m::offer_for(rtx30),
                   unknown = m::offer_for({});
        check(o50.multipliers == std::vector<unsigned>{2, 3, 4, 6} && o50.dynamic, "RTX 50 offer");
        check(o40.multipliers == std::vector<unsigned>{2} && !o40.dynamic, "RTX 40 offer");
        check(o30.multipliers == std::vector<unsigned>{2, 3, 4, 6}, "no frame generation: the list stays (locked)");
        check(unknown.multipliers.size() == 4 && unknown.dynamic, "unknown card: everything");
    }
    {
        // Locks: greyed with the reason.
        const auto lock = [](m::Row row, const m::Settings &s, const c::Capabilities &card, bool requested = true) {
            return m::lock_reason(row, s, card, m::offer_for(card), requested);
        };
        m::Settings s; // DLSS on, frame generation on, 2x, fixed
        for (const auto row :
             {m::Row::dlss, m::Row::mode, m::Row::frame_generation, m::Row::multiplier, m::Row::pacing, m::Row::reflex})
            check(lock(row, s, rtx50).empty(), "RTX 50 with everything on: nothing locked");
        check(lock(m::Row::target, s, rtx50) == "Used only by Dynamic pacing.", "target only for Dynamic");
        m::set_pacing(s, m::Pacing::dynamic);
        check(lock(m::Row::target, s, rtx50).empty() &&
                  lock(m::Row::multiplier, s, rtx50) == "Dynamic pacing picks the number of frames itself.",
              "Dynamic: target open, multiplier locked");
        m::set_pacing(s, m::Pacing::fixed);
        check(lock(m::Row::multiplier, s, rtx40) == "This graphics card shows 2x at most; 3x and up need an RTX 50.",
              "RTX 40: 2x only");
        check(lock(m::Row::frame_generation, s, rtx30) ==
                      "DLSS Frame Generation needs an NVIDIA GeForce RTX 40 or 50 series graphics card." &&
                  lock(m::Row::pacing, s, rtx30) == lock(m::Row::frame_generation, s, rtx30) &&
                  lock(m::Row::dlss, s, rtx30).empty(),
              "RTX 30: every frame generation row with the card's reason; DLSS open");
        check(lock(m::Row::frame_generation, s, rtx50, false) ==
                  "frame_generation_plugin = off in mhw_dlss.ini keeps DLSS Frame Generation unloaded.",
              "plugin off");
        // The player's DLSS Off: its mode and frame generation wait for it.
        auto off = s;
        off.dlss = false;
        check(lock(m::Row::dlss, off, rtx50).empty() && lock(m::Row::mode, off, rtx50) == "Turn DLSS on first.",
              "DLSS off: mode locked");
        check(lock(m::Row::frame_generation, off, rtx50) == "Frame generation needs DLSS; turn DLSS on first." &&
                  lock(m::Row::multiplier, off, rtx50) == lock(m::Row::frame_generation, off, rtx50),
              "DLSS off: frame generation locked");
        check(lock(m::Row::reflex, off, rtx50).empty(), "DLSS off: Reflex open");
        auto fg_off = s;
        fg_off.frame_generation = false;
        check(lock(m::Row::frame_generation, fg_off, rtx50).empty() &&
                  lock(m::Row::pacing, fg_off, rtx50) == "Turn frame generation on first.",
              "frame generation off: its options locked");
        check(lock(m::Row::dlss, s, other) == "DLSS needs an NVIDIA GeForce RTX graphics card." &&
                  lock(m::Row::mode, s, other) == lock(m::Row::dlss, s, other),
              "not NVIDIA: DLSS locked");
    }
    using mhw::settings::ReflexMode;
    check(m::choice_of(ReflexMode::on) == 0 && m::choice_of(ReflexMode::boost) == 1 &&
              m::choice_of(ReflexMode::off) == 2,
          "reflex choices, on first");
    ReflexMode reflex{};
    check(m::reflex_at(2, reflex) && reflex == ReflexMode::off && !m::reflex_at(3, reflex) && !m::reflex_at(-1, reflex),
          "reflex at index");
    why.clear();
    check(m::reflex_allowed(ReflexMode::boost, true, true, why) &&
              m::reflex_allowed(ReflexMode::off, false, false, why) && why.empty(),
          "off without frame generation and without DLSS-G");
    check(!m::reflex_allowed(ReflexMode::off, false, true, why) &&
              why == "Reflex stays on while DLSS Frame Generation is loaded (frame_generation_plugin in mhw_dlss.ini).",
          "off refused while DLSS-G is loaded, frame generation off");
    why.clear();
    check(!m::reflex_allowed(ReflexMode::off, true, true, why) &&
              why == "Frame generation needs Reflex; turn frame generation off first.",
          "off refused with frame generation");
    check(m::reflex_sdk_value(ReflexMode::off) == 0 && m::reflex_sdk_value(ReflexMode::on) == 1 &&
              m::reflex_sdk_value(ReflexMode::boost) == 2,
          "sl::ReflexMode values");

    // The System lines.
    check(c::card_name_line(rtx50) == "NVIDIA GeForce RTX 5090" &&
              c::card_name_line(c::Capabilities{}) == "Not known yet",
          "card name");
    check(c::memory_line(rtx50) == "32 GB" && c::memory_line(rtx40) == "16 GB" && c::memory_line(rtx30) == "10 GB",
          "memory, rounded up");
    check(c::driver_line(rtx50) == "610.88" && c::driver_line(other) == "Not known yet", "driver");
    check(c::generation_support_line(rtx50) == "Up to 6x, Dynamic" && c::generation_support_line(rtx40) == "Up to 2x",
          "support, RTX 50 and 40");
    check(c::generation_support_line(rtx30) == "No: needs an RTX 40 or 50", "support, RTX 30");
    {
        auto plugin = rtx50;
        plugin.frame_generation = c::Support::no;
        plugin.missing = c::Missing::plugin_off;
        check(c::generation_support_line(plugin) == "Not loaded (mhw_dlss.ini)", "support, plugin off");
        auto old = rtx30;
        old.missing = c::Missing::driver;
        old.driver_required_major = 615;
        old.driver_required_minor = 5;
        check(c::generation_support_line(old) == "No: needs driver 615.05", "support, driver");
        auto later = rtx50;
        later.max_multiplier = 0;
        later.dynamic = c::Support::unknown;
        check(c::generation_support_line(later) == "Yes", "support, limits not read yet");
    }
}

void status_lines() {
    namespace s = mhw::settings::status;
    check(s::reason_text("DLSS-G-support:3") == "this graphics card or driver has no DLSS Frame Generation",
          "api reason with code");
    check(s::reason_text("presentation") == "presentation stopped", "exact reason");
    check(s::reason_text("presentation fault") == "a presentation error turned it off for this session",
          "presentation fault, not presentation");
    check(s::reason_text("exclusive fullscreen").starts_with("not available in exclusive fullscreen"),
          "refused switch reason");
    check(s::reason_text("presentation-al-device-factory-chains") ==
              "see the mod's log (presentation-al-device-factory-chains)",
          "a longer reason is not a known one");
    check(s::reason_text("") == "see the mod's log", "no reason");
    check(s::count_text(0) == "0" && s::count_text(999) == "999" && s::count_text(1234567) == "1,234,567", "counts");

    s::View v;
    check(s::mod_line(v) == "Active" && s::dlss_line(v) == "Starting" && s::resolution_line(v) == "Starting" &&
              s::reflex_line(v) == "Starting",
          "start-up");
    check(s::generation_line(v) == "Off" && s::generated_line(v) == "None", "frame generation off in the ini");

    v.dlss = s::Stage::running;
    v.reflex = s::Stage::running;
    check(s::dlss_line(v) == "Performance" && s::resolution_line(v) == "Not known yet", "running, sizes unknown");
    v.render_width = 1280, v.render_height = 720, v.scene_width = 2560, v.scene_height = 1440;
    check(s::resolution_line(v) == "1280x720 to 2560x1440", "running with sizes");
    {
        auto q = v;
        q.dlss_mode = "DLAA";
        q.render_width = 2560, q.render_height = 1440;
        check(s::dlss_line(q) == "DLAA" && s::resolution_line(q) == "2560x1440", "DLAA");
    }
    check(s::reflex_line(v) == "On", "reflex on");
    {
        auto r = v;
        r.reflex_mode = "Boost";
        check(s::reflex_line(r) == "Boost", "reflex boost");
        r.reflex_mode = "Off";
        check(s::reflex_line(r) == "Off" && s::mod_line(r) == "Active", "the player's Off is not a failure");
        r.reflex = s::Stage::off;
        r.reflex_reason = "reflex_frames";
        check(s::reflex_line(r) == "Off: the frame hooks stopped" && s::mod_line(r) == "Degraded: Reflex is off",
              "a failure still is");
    }

    v.generation = s::Generation::armed;
    v.requested = 2;
    check(s::generation_line(v) == "2x, paused", "armed, not generating");
    v.generating = true;
    v.generated = 1;
    v.presented_generated = 12345;
    check(s::generation_line(v) == "2x, generating" && s::generated_line(v) == "12,345", "generating 2x");
    v.requested = 6;
    v.generated = 3;
    check(s::generation_line(v) == "4x (the card's most)", "limited by the card");
    {
        auto d = v;
        d.requested = 2;
        d.generated = 1;
        d.generation_mode = 2;
        d.dynamic_applied = true;
        d.presented_now = 3;
        check(s::generation_line(d) == "Dynamic, showing 3x", "Dynamic");
        {
            auto one = d;
            one.presented_now = 1;
            one.single_streak = s::auto_pause_presents - 1;
            check(s::generation_line(one) == "Dynamic, showing 1x", "Dynamic at 1x for a moment");
            one.single_streak = s::auto_pause_presents;
            check(s::generation_line(one) == "Dynamic, 1x (target reached)", "Dynamic held at 1x by its target");
        }
        d.presented_now = 0;
        check(s::generation_line(d) == "Dynamic, generating", "Dynamic before a frame was counted");
        d.dynamic_applied = false;
        d.dynamic_support = 1;
        check(s::generation_line(d) == "2x (no Dynamic here)", "Dynamic unsupported: 2x");
        d.dynamic_support = 2;
        d.dynamic_refused = true;
        check(s::generation_line(d) == "2x (Dynamic refused)", "Dynamic refused");
        d.dynamic_refused = false;
        check(s::generation_line(d) == "2x, switching to Dynamic", "Dynamic asked for, not applied yet");
        d.generating = false;
        check(s::generation_line(d) == "Dynamic, paused", "Dynamic, not generating");

        auto a = v;
        a.requested = 2;
        a.generated = 1;
        a.generation_mode = 1;
        check(s::generation_line(a) == "2x Auto, generating", "Auto generating");
        a.single_streak = s::auto_pause_presents - 1;
        check(s::generation_line(a) == "2x Auto, generating", "a short run without generated frames");
        a.single_streak = s::auto_pause_presents;
        check(s::generation_line(a) == "2x Auto, paused", "Auto paused by DLSS-G");
    }

    v.generation = s::Generation::unavailable;
    v.generation_reason = "scene_rect";
    check(s::generation_line(v) == "Not available: the game's screen layout is not supported", "unavailable");
    check(s::mod_line(v) == "Degraded: frame generation is off", "unavailable degrades the mod");

    v.dlss_healthy = false;
    check(s::dlss_line(v) == "Stopped (see the log)" && s::resolution_line(v) == "The game's own" &&
              s::mod_line(v) == "Degraded: DLSS is off",
          "DLSS fault");

    v.loader = s::Loader::degraded;
    v.loader_reason = "swap-chain hooks unavailable; DLSS and frame generation stay off";
    check(s::mod_line(v) == "Degraded: swap-chain hooks unavailable; DLSS and frame generation stay off",
          "loader reason first");

    s::View failed;
    failed.dlss = s::Stage::off;
    failed.dlss_reason = "live-frame-al-hook-refused";
    check(s::dlss_line(failed) == "Off: another mod changed the game's frame functions", "DLSS off with a reason");
    failed.generation = s::Generation::stopped;
    failed.generation_reason = "reflex_frames";
    check(s::generation_line(failed) == "Stopped: the frame hooks stopped", "stopped frame generation");
    check(s::generated_line(failed) == "0", "stopped still counts");

    // The player's DLSS Off is not a fault; frame generation pauses with it.
    s::View chosen;
    chosen.dlss = s::Stage::player_off;
    chosen.reflex = s::Stage::running;
    chosen.generation = s::Generation::armed;
    chosen.requested = 2;
    check(s::dlss_line(chosen) == "Off" && s::resolution_line(chosen) == "The game's own" &&
              s::mod_line(chosen) == "Active",
          "the player's Off");
    check(s::generation_line(chosen) == "Paused (DLSS is off)", "frame generation paused while DLSS is off");
    // The player's frame generation Off while DLSS-G is loaded (v0.27.0).
    s::View fg_off;
    fg_off.dlss = s::Stage::running;
    fg_off.reflex = s::Stage::running;
    fg_off.generation = s::Generation::armed;
    fg_off.generation_enabled = false;
    fg_off.presented_generated = 42;
    check(s::generation_line(fg_off) == "Off" && s::mod_line(fg_off) == "Active" && s::generated_line(fg_off) == "42",
          "frame generation turned off in the menu");
    chosen.generation_enabled = false;
    check(s::generation_line(chosen) == "Off", "off wins over paused");
}
} // namespace

int main() {
    ini_edits();
    settings_model();
    status_lines();
    if (failures) return 1;
    std::printf("settings menu tests passed\n");
    return 0;
}
