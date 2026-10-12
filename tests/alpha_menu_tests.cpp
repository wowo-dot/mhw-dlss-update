// The alpha's CMFM pages: the ini edit, the settings model and the status lines.
#include "../src/alpha/config.hpp"
#include "../src/alpha/menu_model.hpp"
#include "../src/alpha/status.hpp"
#include <cstdio>

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
    namespace a = mhw::alpha;
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
        check(out == "frame_generation = 2x\nFRAME_GENERATION=6x ; mine\n", "the last line wins, its case and comment stay");
        const auto c = a::parse_config(out, folder);
        check(c.frame_generation && c.multiplier == 6, "reads back as 6x");
    }
    {
        check(a::with_frame_generation("frame_generation =\r\n", "2x") == "frame_generation =2x\r\n", "empty value");
        check(a::with_frame_generation("frame_generation = off", "3x") == "frame_generation = 3x", "no final newline");
        check(a::with_frame_generation("# frame_generation = 4x\n", "2x") == "# frame_generation = 4x\nframe_generation = 2x\n",
              "a commented-out line is not a setting");
    }
    check(a::frame_generation_value(false, 4) == "off" && a::frame_generation_value(true, 3) == "3x", "ini values");
    {
        // The shipped ini's dlss_mode changes in place; an ini without one gains it.
        const auto out = a::with_value(shipped, "dlss_mode", "quality");
        std::string expected(shipped);
        expected.replace(expected.find("= performance") + 2, 11, "quality");
        check(out == expected, "dlss_mode replaced in the shipped ini, comment and CRLF kept");
        check(a::with_value("reflex = on\r\n", "dlss_mode", "dlaa") == "reflex = on\r\ndlss_mode = dlaa\r\n", "dlss_mode added");
        const auto c = a::parse_config(out, folder);
        check(c.dlss_mode == mhw::dlss_sdk::ReconstructionMode::quality && c.frame_generation && c.warnings.empty(), "reads back");
        const auto again = a::with_value("dlss_mode = quality   ; dlaa, quality\nframe_generation = off\n", "dlss_mode", "dlaa");
        check(again == "dlss_mode = dlaa   ; dlaa, quality\nframe_generation = off\n", "dlss_mode value replaced in place");
    }
}

void settings_model() {
    namespace m = mhw::alpha::menu_model;
    check(m::choice_of(2) == 0 && m::choice_of(3) == 1 && m::choice_of(4) == 2 && m::choice_of(6) == 3, "choice indices");
    check(m::choice_of(5) == 0 && m::choice_of(0) == 0, "unknown multipliers show 2x");
    check(m::multiplier_at(3) == 6 && m::multiplier_at(-1) == 0 && m::multiplier_at(4) == 0, "multiplier at index");
    std::string why;
    check(m::multiplier_allowed(6, 0, why) && why.empty(), "unknown limit: everything allowed");
    check(m::multiplier_allowed(2, 1, why) && !m::multiplier_allowed(3, 1, why), "RTX 40: 2x only");
    check(why == "This graphics card shows 2x at most; 3x and up need an RTX 50.", "RTX 40 reason");
    check(m::multiplier_allowed(4, 3, why) && !m::multiplier_allowed(6, 3, why), "limit 3: up to 4x");
    check(why == "This graphics card shows 4x at most.", "limit 3 reason");
    check(m::multiplier_allowed(6, 5, why), "limit 5: 6x");
    using mhw::dlss_sdk::ReconstructionMode;
    check(m::choice_of(ReconstructionMode::dlaa) == 0 && m::choice_of(ReconstructionMode::performance) == 3, "mode choices, best first");
    ReconstructionMode mode{};
    check(m::mode_at(1, mode) && mode == ReconstructionMode::quality && !m::mode_at(4, mode) && !m::mode_at(-1, mode), "mode at index");
    using mhw::alpha::ReflexMode;
    check(m::choice_of(ReflexMode::on) == 0 && m::choice_of(ReflexMode::boost) == 1 && m::choice_of(ReflexMode::off) == 2, "reflex choices, on first");
    ReflexMode reflex{};
    check(m::reflex_at(2, reflex) && reflex == ReflexMode::off && !m::reflex_at(3, reflex) && !m::reflex_at(-1, reflex), "reflex at index");
    why.clear();
    check(m::reflex_allowed(ReflexMode::boost, true, why) && m::reflex_allowed(ReflexMode::off, false, why) && why.empty(), "off without frame generation");
    check(!m::reflex_allowed(ReflexMode::off, true, why) && why == "Frame generation needs Reflex; turn frame generation off first.",
          "off refused with frame generation");
    check(m::reflex_sdk_value(ReflexMode::off) == 0 && m::reflex_sdk_value(ReflexMode::on) == 1 && m::reflex_sdk_value(ReflexMode::boost) == 2,
          "sl::ReflexMode values");
}

void status_lines() {
    namespace s = mhw::alpha::status;
    check(s::reason_text("DLSS-G-support:3") == "this graphics card or driver has no DLSS Frame Generation", "api reason with code");
    check(s::reason_text("presentation") == "presentation stopped", "exact reason");
    check(s::reason_text("presentation-al-device-factory-chains") ==
              "see the mod's log (presentation-al-device-factory-chains)", "a longer reason is not a known one");
    check(s::reason_text("") == "see the mod's log", "no reason");
    check(s::count_text(0) == "0" && s::count_text(999) == "999" && s::count_text(1234567) == "1,234,567", "counts");

    s::View v;
    check(s::mod_line(v) == "Active" && s::dlss_line(v) == "Starting" && s::reflex_line(v) == "Starting", "start-up");
    check(s::generation_line(v) == "Off" && s::generated_line(v) == "None", "frame generation off in the ini");

    v.dlss = s::Stage::running;
    v.reflex = s::Stage::running;
    check(s::dlss_line(v) == "Performance", "running, sizes unknown");
    v.render_width = 1280, v.render_height = 720, v.scene_width = 2560, v.scene_height = 1440;
    check(s::dlss_line(v) == "Performance, 1280x720 to 2560x1440", "running with sizes");
    {
        auto q = v;
        q.dlss_mode = "Quality";
        q.render_width = 1696, q.render_height = 954;
        check(s::dlss_line(q) == "Quality, 1696x954 to 2560x1440", "quality line");
        q.dlss_mode = "DLAA";
        q.render_width = 2560, q.render_height = 1440;
        check(s::dlss_line(q) == "DLAA, 2560x1440", "DLAA line");
    }
    check(s::reflex_line(v) == "On (low latency)", "reflex on");
    {
        auto r = v;
        r.reflex_mode = "Boost";
        check(s::reflex_line(r) == "Boost (low latency, GPU clocks kept up)", "reflex boost");
        r.reflex_mode = "Off";
        check(s::reflex_line(r) == "Off" && s::mod_line(r) == "Active", "the player's Off is not a failure");
        r.reflex = s::Stage::off;
        r.reflex_reason = "reflex_frames";
        check(s::reflex_line(r) == "Off: the frame hooks stopped" && s::mod_line(r) == "Degraded: Reflex is off", "a failure still is");
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
    check(s::generation_line(v) == "4x, generating (6x is more than this graphics card allows)", "limited by the card");

    v.generation = s::Generation::unavailable;
    v.generation_reason = "scene_rect";
    check(s::generation_line(v) == "Unavailable: the game's screen layout is not supported", "unavailable");
    check(s::mod_line(v) == "Degraded: frame generation is off", "unavailable degrades the mod");

    v.dlss_healthy = false;
    check(s::dlss_line(v) == "Stopped; the game renders without DLSS" && s::mod_line(v) == "Degraded: DLSS is off", "DLSS fault");

    v.loader = s::Loader::degraded;
    v.loader_reason = "swap-chain hooks unavailable; DLSS and frame generation stay off";
    check(s::mod_line(v) == "Degraded: swap-chain hooks unavailable; DLSS and frame generation stay off", "loader reason first");

    s::View failed;
    failed.dlss = s::Stage::off;
    failed.dlss_reason = "live-frame-al-hook-refused";
    check(s::dlss_line(failed) == "Off: another mod changed the game's frame functions", "DLSS off with a reason");
    failed.generation = s::Generation::stopped;
    failed.generation_reason = "reflex_frames";
    check(s::generation_line(failed) == "Off: the frame hooks stopped", "stopped frame generation");
    check(s::generated_line(failed) == "0", "stopped still counts");
}
}  // namespace

int main() {
    ini_edits();
    settings_model();
    status_lines();
    if (failures) return 1;
    std::printf("alpha menu tests passed\n");
    return 0;
}
