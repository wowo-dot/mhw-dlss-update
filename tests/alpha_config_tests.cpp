// mhw_dlss.ini parsing for the alpha build.
#include "../src/alpha/config.hpp"
#include <cstdio>

namespace {
int failures{};
void check(bool ok, const char *what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
}  // namespace

int main() {
    const std::filesystem::path folder = L"D:\\mods\\dlss";
    {
        const auto c = mhw::alpha::parse_config("", folder);
        check(c.frame_generation && c.multiplier == 2 && c.reflex == mhw::alpha::ReflexMode::on && c.warnings.empty(), "defaults");
        check(c.dlss_mode == mhw::dlss_sdk::ReconstructionMode::performance, "Performance by default");
        check(c.runtime_dir == folder && c.log_dir == folder / "logs", "default folders");
    }
    {
        const auto c = mhw::alpha::parse_config(
            "# comment\r\n FRAME_GENERATION = 4X ; inline\r\nreflex=on\r\nruntime_dir = \"C:\\SL Runtime\" \r\nlog_dir=..\\Logs\r\n", folder);
        check(c.frame_generation && c.multiplier == 4 && c.reflex == mhw::alpha::ReflexMode::on && c.warnings.empty(), "values, case, comments, CRLF");
        check(c.runtime_dir == std::filesystem::path(L"C:\\SL Runtime"), "absolute runtime folder keeps its case");
        check(c.log_dir == std::filesystem::path(L"D:\\mods\\Logs"), "relative log folder");
    }
    {
        const auto c = mhw::alpha::parse_config("frame_generation = off\nreflex = off\n", folder);
        check(!c.frame_generation && c.reflex == mhw::alpha::ReflexMode::off && c.warnings.empty(), "reflex off with frame generation off");
    }
    {
        const auto c = mhw::alpha::parse_config("frame_generation = 5x\nreflex = off\nsharpness = 1\nnot a pair\nlog_dir =\n", folder);
        check(c.frame_generation && c.multiplier == 2, "bad multiplier keeps the default");
        check(c.reflex == mhw::alpha::ReflexMode::on, "reflex off with frame generation on: on, with a warning");
        check(c.warnings.size() == 5, "five warnings");
        check(c.log_dir == folder / "logs", "empty folder keeps the default");
    }
    {
        using mhw::dlss_sdk::ReconstructionMode;
        const auto c = mhw::alpha::parse_config("dlss_mode = Quality\n", folder);
        check(c.dlss_mode == ReconstructionMode::quality && c.warnings.empty(), "dlss_mode quality, any case");
        check(mhw::alpha::parse_config("dlss_mode=dlaa", folder).dlss_mode == ReconstructionMode::dlaa, "dlss_mode dlaa");
        check(mhw::alpha::parse_config("dlss_mode = balanced", folder).dlss_mode == ReconstructionMode::balanced, "dlss_mode balanced");
        const auto bad = mhw::alpha::parse_config("dlss_mode = ultra_performance\n", folder);
        check(bad.dlss_mode == ReconstructionMode::performance && bad.warnings.size() == 1, "unknown mode keeps Performance with a warning");
        for (const auto m : {ReconstructionMode::dlaa, ReconstructionMode::quality, ReconstructionMode::balanced, ReconstructionMode::performance}) {
            ReconstructionMode back{};
            check(mhw::alpha::parse_dlss_mode(mhw::alpha::dlss_mode_value(m), back) && back == m, "mode values round-trip");
        }
    }
    {
        using mhw::alpha::ReflexMode;
        check(mhw::alpha::parse_config("reflex = Boost\n", folder).reflex == ReflexMode::boost, "reflex boost, any case");
        const auto early = mhw::alpha::parse_config("reflex = off\nframe_generation = off\n", folder);
        check(early.reflex == ReflexMode::off && early.warnings.empty(), "reflex off before frame_generation off: order does not matter");
        const auto on = mhw::alpha::parse_config("reflex = off\n", folder);
        check(on.reflex == ReflexMode::on && on.warnings.size() == 1, "reflex off with the default frame generation: on");
        const auto bad = mhw::alpha::parse_config("reflex = ultra\n", folder);
        check(bad.reflex == ReflexMode::on && bad.warnings.size() == 1, "unknown reflex value keeps on");
        for (const auto m : {ReflexMode::off, ReflexMode::on, ReflexMode::boost}) {
            ReflexMode back{};
            check(mhw::alpha::parse_reflex(mhw::alpha::reflex_value(m), back) && back == m, "reflex values round-trip");
        }
        check(mhw::alpha::with_reflex("reflex = on   ; comment\r\n", ReflexMode::boost) == "reflex = boost   ; comment\r\n", "reflex edited in place");
        check(mhw::alpha::parse_config(mhw::alpha::with_reflex("frame_generation = off\n", ReflexMode::off), folder).reflex == ReflexMode::off,
              "reflex appended and read back");
    }
    {
        namespace a = mhw::alpha;
        const auto d = a::parse_config("frame_generation = Dynamic\n", folder);
        check(d.frame_generation && d.dynamic && d.multiplier == 2 && d.dynamic_target == 0 && !d.generation_auto && d.warnings.empty(),
              "dynamic, any case; the fixed multiplier stays 2x");
        const auto later = a::parse_config("frame_generation = dynamic\nframe_generation = 3x\n", folder);
        check(later.frame_generation && !later.dynamic && later.multiplier == 3, "the last line wins: 3x");
        const auto off = a::parse_config("frame_generation = dynamic\nframe_generation = off\nreflex = off\n", folder);
        check(!off.frame_generation && !off.dynamic && off.reflex == a::ReflexMode::off && off.warnings.empty(), "dynamic then off");
        check(a::frame_generation_value(true, 4, true) == "dynamic" && a::frame_generation_value(false, 4, true) == "off" &&
                  a::frame_generation_value(true, 4) == "4x",
              "frame generation ini values");

        check(a::parse_config("dynamic_target = display\n", folder).dynamic_target == 0, "target: display");
        const auto t = a::parse_config("dynamic_target = 240 ; mine\n", folder);
        check(t.dynamic_target == 240 && t.warnings.empty(), "target: 240");
        check(a::parse_config("dynamic_target = 30\ndynamic_target = 1000\n", folder).dynamic_target == 1000, "target bounds: 30 and 1000");
        for (const char *bad : {"29", "1001", "abc", "+240", "24 0", "", "99999", "-60"}) {
            const auto c = a::parse_config(std::string("dynamic_target = 144\ndynamic_target = ") + bad + "\n", folder);
            check(c.dynamic_target == 144 && c.warnings.size() == 1, "a bad target keeps the last good one, with a warning");
        }
        for (const unsigned v : {0U, 30U, 138U, 1000U}) {
            unsigned back = 7;
            check(a::parse_dynamic_target(a::dynamic_target_value(v), back) && back == v, "target values round-trip");
        }

        check(a::parse_config("frame_generation_auto = ON\n", folder).generation_auto, "auto on, any case");
        const auto bad_auto = a::parse_config("frame_generation_auto = on\nframe_generation_auto = maybe\n", folder);
        check(bad_auto.generation_auto && bad_auto.warnings.size() == 1, "a bad auto value keeps the last good one");
        check(!a::parse_config("", folder).profile && a::parse_config("profile = on\n", folder).profile, "profile off by default; on");
        const auto bad_profile = a::parse_config("profile = sometimes\n", folder);
        check(!bad_profile.profile && bad_profile.warnings.size() == 1, "a bad profile value warns and stays off");
        const auto ab = a::parse_config("frame_generation = off\nperf_ab = on\n", folder);
        check(ab.perf_ab && ab.warnings.empty(), "perf_ab with frame generation off");
        const auto ab_fg = a::parse_config("frame_generation = 2x\nperf_ab = on\n", folder);
        check(!ab_fg.perf_ab && ab_fg.warnings.size() == 1, "perf_ab refused with frame generation on");

        const auto edited = a::with_value(a::with_value("frame_generation = 2x\r\n", "dynamic_target", "165"), "frame_generation_auto", "on");
        check(edited == "frame_generation = 2x\r\ndynamic_target = 165\r\nframe_generation_auto = on\r\n", "new keys appended with CRLF");
        const auto e = a::parse_config(edited, folder);
        check(e.dynamic_target == 165 && e.generation_auto && e.warnings.empty(), "appended keys read back");
    }
    if (failures) return 1;
    std::printf("alpha config tests passed\n");
    return 0;
}
