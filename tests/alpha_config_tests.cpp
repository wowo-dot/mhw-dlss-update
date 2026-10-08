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
        check(c.frame_generation && c.multiplier == 2 && c.reflex && c.warnings.empty(), "defaults");
        check(c.runtime_dir == folder && c.log_dir == folder / "logs", "default folders");
    }
    {
        const auto c = mhw::alpha::parse_config(
            "# comment\r\n FRAME_GENERATION = 4X ; inline\r\nreflex=on\r\nruntime_dir = \"C:\\SL Runtime\" \r\nlog_dir=..\\Logs\r\n", folder);
        check(c.frame_generation && c.multiplier == 4 && c.reflex && c.warnings.empty(), "values, case, comments, CRLF");
        check(c.runtime_dir == std::filesystem::path(L"C:\\SL Runtime"), "absolute runtime folder keeps its case");
        check(c.log_dir == std::filesystem::path(L"D:\\mods\\Logs"), "relative log folder");
    }
    {
        const auto c = mhw::alpha::parse_config("frame_generation = off\nreflex = off\n", folder);
        check(!c.frame_generation && c.reflex && c.warnings.size() == 1, "frame generation off; reflex stays on with a warning");
    }
    {
        const auto c = mhw::alpha::parse_config("frame_generation = 5x\nreflex = off\nsharpness = 1\nnot a pair\nlog_dir =\n", folder);
        check(c.frame_generation && c.multiplier == 2, "bad multiplier keeps the default");
        check(c.reflex, "reflex stays on");
        check(c.warnings.size() == 5, "five warnings");
        check(c.log_dir == folder / "logs", "empty folder keeps the default");
    }
    if (failures) return 1;
    std::printf("alpha config tests passed\n");
    return 0;
}
