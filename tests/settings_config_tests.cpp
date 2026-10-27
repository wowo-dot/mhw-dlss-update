// mhw_dlss.ini parsing.
#include "../src/settings/config.hpp"
#include <cstdio>

namespace {
int failures{};
void check(bool ok, const char *what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    const std::filesystem::path folder = L"D:\\mods\\dlss";
    {
        const auto c = mhw::settings::parse_config("", folder);
        check(!c.dlss && !c.frame_generation && c.multiplier == 2 && c.reflex == mhw::settings::ReflexMode::off &&
                  c.warnings.empty(),
              "defaults: the shipped ini's, DLSS, frame generation and Reflex off");
        check(c.dlss_mode == mhw::dlss_sdk::ReconstructionMode::performance, "DLSS turns on in Performance");
        check(c.runtime_dir == folder && c.log_dir == folder / "logs", "default folders");
    }
    {
        const auto c = mhw::settings::parse_config(
            "# comment\r\n FRAME_GENERATION = 4X ; inline\r\nreflex=on\r\nruntime_dir = \"C:\\SL Runtime\" \r\nlog_dir=..\\Logs\r\n",
            folder);
        check(c.frame_generation && c.multiplier == 4 && c.reflex == mhw::settings::ReflexMode::on &&
                  c.warnings.empty(),
              "values, case, comments, CRLF");
        check(c.runtime_dir == std::filesystem::path(L"C:\\SL Runtime"), "absolute runtime folder keeps its case");
        check(c.log_dir == std::filesystem::path(L"D:\\mods\\Logs"), "relative log folder");
    }
    {
        const auto c = mhw::settings::parse_config("frame_generation = off\nreflex = off\n", folder);
        check(!c.frame_generation && c.reflex == mhw::settings::ReflexMode::off && c.warnings.empty(),
              "reflex off with frame generation off");
    }
    {
        const auto c = mhw::settings::parse_config(
            "frame_generation = 5x\nreflex = off\nsharpness = 1\nnot a pair\nlog_dir =\n", folder);
        check(!c.frame_generation && c.multiplier == 2, "bad multiplier keeps the default (off, 2x)");
        check(c.reflex == mhw::settings::ReflexMode::off, "reflex off kept: frame generation is off");
        check(c.warnings.size() == 4, "four warnings");
        check(c.log_dir == folder / "logs", "empty folder keeps the default");
    }
    {
        using mhw::dlss_sdk::ReconstructionMode;
        const auto c = mhw::settings::parse_config("dlss_mode = Quality\n", folder);
        check(c.dlss_mode == ReconstructionMode::quality && c.warnings.empty(), "dlss_mode quality, any case");
        check(mhw::settings::parse_config("dlss_mode=dlaa", folder).dlss_mode == ReconstructionMode::dlaa,
              "dlss_mode dlaa");
        check(mhw::settings::parse_config("dlss_mode = balanced", folder).dlss_mode == ReconstructionMode::balanced,
              "dlss_mode balanced");
        const auto bad = mhw::settings::parse_config("dlss_mode = ultra_performance\n", folder);
        check(bad.dlss_mode == ReconstructionMode::performance && !bad.dlss && bad.warnings.size() == 1,
              "unknown mode keeps the default (off, Performance) with a warning");
        const auto off = mhw::settings::parse_config("dlss_mode = Off\n", folder);
        check(!off.dlss && off.dlss_mode == ReconstructionMode::performance && off.warnings.empty(), "dlss_mode off");
        check(mhw::settings::parse_config("dlss_mode = off\ndlss_mode = quality\n", folder).dlss,
              "a later mode turns DLSS on");
        check(std::string(mhw::settings::dlss_setting_value(false, ReconstructionMode::dlaa)) == "off" &&
                  std::string(mhw::settings::dlss_setting_value(true, ReconstructionMode::dlaa)) == "dlaa",
              "the setting's ini value");
        check(mhw::settings::parse_config("dlss_mode = performance\n", folder).dlss, "a mode turns DLSS on");
        for (const auto m : {ReconstructionMode::dlaa, ReconstructionMode::quality, ReconstructionMode::balanced,
                             ReconstructionMode::performance}) {
            ReconstructionMode back{};
            check(mhw::settings::parse_dlss_mode(mhw::settings::dlss_mode_value(m), back) && back == m,
                  "mode values round-trip");
        }
    }
    {
        using mhw::settings::ReflexMode;
        check(mhw::settings::parse_config("reflex = Boost\n", folder).reflex == ReflexMode::boost,
              "reflex boost, any case");
        const auto early = mhw::settings::parse_config("reflex = off\nframe_generation = off\n", folder);
        check(early.reflex == ReflexMode::off && early.warnings.empty(),
              "reflex off before frame_generation off: order does not matter");
        const auto alone = mhw::settings::parse_config("reflex = off\n", folder);
        check(alone.reflex == ReflexMode::off && alone.warnings.empty(),
              "reflex off with the default frame generation (off)");
        const auto on = mhw::settings::parse_config("frame_generation = 2x\nreflex = off\n", folder);
        check(on.reflex == ReflexMode::on && on.warnings.size() == 1,
              "a written reflex off with frame generation on: on, with a warning");
        const auto follows = mhw::settings::parse_config("frame_generation = 3x\n", folder);
        check(follows.reflex == ReflexMode::on && follows.warnings.empty(),
              "reflex unset follows frame generation, no warning");
        const auto bad = mhw::settings::parse_config("reflex = ultra\n", folder);
        check(bad.reflex == ReflexMode::off && bad.warnings.size() == 1,
              "unknown reflex value keeps the default (off)");
        for (const auto m : {ReflexMode::off, ReflexMode::on, ReflexMode::boost}) {
            ReflexMode back{};
            check(mhw::settings::parse_reflex(mhw::settings::reflex_value(m), back) && back == m,
                  "reflex values round-trip");
        }
        check(mhw::settings::with_reflex("reflex = on   ; comment\r\n", ReflexMode::boost) ==
                  "reflex = boost   ; comment\r\n",
              "reflex edited in place");
        check(
            mhw::settings::parse_config(mhw::settings::with_reflex("frame_generation = off\n", ReflexMode::off), folder)
                    .reflex == ReflexMode::off,
            "reflex appended and read back");
    }
    {
        namespace a = mhw::settings;
        const auto d = a::parse_config("frame_generation = Dynamic\n", folder);
        check(d.frame_generation && d.dynamic && d.multiplier == 2 && d.dynamic_target == 0 && !d.generation_auto &&
                  d.warnings.empty(),
              "dynamic, any case; the fixed multiplier stays 2x");
        const auto later = a::parse_config("frame_generation = dynamic\nframe_generation = 3x\n", folder);
        check(later.frame_generation && !later.dynamic && later.multiplier == 3, "the last line wins: 3x");
        const auto off = a::parse_config("frame_generation = dynamic\nframe_generation = off\nreflex = off\n", folder);
        check(!off.frame_generation && !off.dynamic && off.reflex == a::ReflexMode::off && off.warnings.empty(),
              "dynamic then off");
        check(a::frame_generation_value(true, 4, true) == "dynamic" &&
                  a::frame_generation_value(false, 4, true) == "off" && a::frame_generation_value(true, 4) == "4x",
              "frame generation ini values");

        check(a::parse_config("dynamic_target = display\n", folder).dynamic_target == 0, "target: display");
        const auto t = a::parse_config("dynamic_target = 240 ; mine\n", folder);
        check(t.dynamic_target == 240 && t.warnings.empty(), "target: 240");
        check(a::parse_config("dynamic_target = 30\ndynamic_target = 1000\n", folder).dynamic_target == 1000,
              "target bounds: 30 and 1000");
        for (const char *bad : {"29", "1001", "abc", "+240", "24 0", "", "99999", "-60"}) {
            const auto c = a::parse_config(std::string("dynamic_target = 144\ndynamic_target = ") + bad + "\n", folder);
            check(c.dynamic_target == 144 && c.warnings.size() == 1,
                  "a bad target keeps the last good one, with a warning");
        }
        for (const unsigned v : {0U, 30U, 138U, 1000U}) {
            unsigned back = 7;
            check(a::parse_dynamic_target(a::dynamic_target_value(v), back) && back == v, "target values round-trip");
        }

        check(a::parse_config("frame_generation_auto = ON\n", folder).generation_auto, "auto on, any case");
        const auto bad_auto = a::parse_config("frame_generation_auto = on\nframe_generation_auto = maybe\n", folder);
        check(bad_auto.generation_auto && bad_auto.warnings.size() == 1, "a bad auto value keeps the last good one");
        check(!a::parse_config("", folder).profile && a::parse_config("profile = on\n", folder).profile,
              "profile off by default; on");
        const auto bad_profile = a::parse_config("profile = sometimes\n", folder);
        check(!bad_profile.profile && bad_profile.warnings.size() == 1, "a bad profile value warns and stays off");
        const auto ab = a::parse_config("frame_generation_plugin = off\nperf_ab = on\n", folder);
        check(ab.perf_ab && !ab.generation_plugin && ab.warnings.empty(), "perf_ab with DLSS-G not loaded");
        const auto ab_fg = a::parse_config("frame_generation = off\nperf_ab = on\n", folder);
        check(!ab_fg.perf_ab && ab_fg.warnings.size() == 1,
              "perf_ab refused while DLSS-G loads (frame generation off or not)");
        {
            const auto defaults = a::parse_config("", folder);
            check(defaults.texture_bias && !defaults.texture_bias_ab, "the texture bias on by default, its A/B off");
            const auto bias_off = a::parse_config("texture_bias = Off\n", folder);
            check(!bias_off.texture_bias && bias_off.warnings.empty(), "texture_bias = off");
            const auto both = a::parse_config("texture_bias_ab = on\n", folder);
            check(both.texture_bias && both.texture_bias_ab && both.warnings.empty(), "texture_bias_ab = on");
            const auto lone = a::parse_config("texture_bias = off\ntexture_bias_ab = on\n", folder);
            check(!lone.texture_bias_ab && lone.warnings.size() == 1, "the A/B needs the bias on");
            const auto bad = a::parse_config("texture_bias = sharp\n", folder);
            check(bad.texture_bias && bad.warnings.size() == 1, "a bad texture_bias value warns and stays on");
        }
        {
            // 0.31.1 diagnostics: off, on and off by default.
            const auto defaults = a::parse_config("", folder);
            check(!defaults.gpu_timing && defaults.auto_exposure && !defaults.chaos,
                  "gpu_timing and chaos off, auto exposure on by default");
            const auto set = a::parse_config("gpu_timing = on\nauto_exposure = OFF\nchaos = on\n", folder);
            check(set.gpu_timing && !set.auto_exposure && set.chaos && set.warnings.empty(),
                  "gpu_timing, auto_exposure and chaos parse");
            const auto bad = a::parse_config("chaos = maybe\n", folder);
            check(!bad.chaos && bad.warnings.size() == 1, "a bad chaos value warns and stays off");
            check(defaults.dlss_preset == 11 && !defaults.jitter_ab, "preset K, the jitter A/B off by default");
            check(a::parse_config("jitter_ab = on\n", folder).jitter_ab, "jitter_ab = on");
            check(a::parse_config("dlss_preset = E\n", folder).dlss_preset == 5 &&
                      a::parse_config("dlss_preset = default\n", folder).dlss_preset == 0 &&
                      a::parse_config("dlss_preset = m\n", folder).dlss_preset == 13,
                  "dlss_preset parses E, default and M");
            const auto wrong = a::parse_config("dlss_preset = z\n", folder);
            check(wrong.dlss_preset == 11 && wrong.warnings.size() == 1, "a bad preset warns and stays K");
        }
        check(a::parse_config("", folder).generation_plugin &&
                  a::parse_config("frame_generation_plugin = Auto\n", folder).generation_plugin,
              "DLSS-G loads by default");
        const auto bad_plugin = a::parse_config("frame_generation_plugin = never\n", folder);
        check(bad_plugin.generation_plugin && bad_plugin.warnings.size() == 1,
              "a bad plugin value warns and keeps auto");
        check(!a::parse_config("", folder).hook_timing && a::parse_config("hook_timing = on\n", folder).hook_timing,
              "hook_timing off by default; on");

        const auto edited = a::with_value(a::with_value("frame_generation = 2x\r\n", "dynamic_target", "165"),
                                          "frame_generation_auto", "on");
        check(edited == "frame_generation = 2x\r\ndynamic_target = 165\r\nframe_generation_auto = on\r\n",
              "new keys appended with CRLF");
        const auto e = a::parse_config(edited, folder);
        check(e.dynamic_target == 165 && e.generation_auto && e.warnings.empty(), "appended keys read back");
    }
    if (failures) return 1;
    std::printf("settings config tests passed\n");
    return 0;
}
