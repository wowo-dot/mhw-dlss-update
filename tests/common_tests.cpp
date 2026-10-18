// The shared helpers in src/common: require/hr, quoted, log_path, platform.
#include "../src/common/require.hpp"
#include "../src/common/json.hpp"
#include "../src/common/log_path.hpp"
#include "../src/common/platform.hpp"
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace {
void check(bool ok, const char *what) {
    if (!ok) {
        std::fprintf(stderr, "failed: %s\n", what);
        std::exit(1);
    }
}
} // namespace

int main() {
    bool threw = false;
    try {
        mhw::require(false, "why");
    } catch (const std::runtime_error &e) {
        threw = std::string(e.what()) == "why";
    }
    check(threw, "require throws its reason");
    mhw::require(true, "unused");
    threw = false;
    try {
        mhw::hr(E_FAIL, "hr");
    } catch (const std::runtime_error &) {
        threw = true;
    }
    check(threw, "hr throws on a failed HRESULT");
    mhw::hr(S_OK, "unused");

    check(mhw::quoted("a\"b\\c\n") == "\"a\\\"b\\\\c\\u000a\"",
          "quoted escapes quotes, backslashes, control characters");

    check(SetEnvironmentVariableW(L"USERPROFILE", L"C:\\Users\\Player") != 0, "set the profile");
    check(mhw::log_path(L"C:\\Users\\Player\\MO2\\mods\\x") == "%USERPROFILE%\\MO2\\mods\\x",
          "the profile folder is written as %USERPROFILE%");
    check(mhw::log_path(L"c:\\users\\player") == "%USERPROFILE%", "case-insensitive, the profile itself");
    check(mhw::log_path(L"C:\\Users\\Player2\\x") == "C:\\Users\\Player2\\x", "only the whole folder name");
    check(mhw::log_path(L"D:\\MO2\\MHW\\mods") == "D:\\MO2\\MHW\\mods", "other paths unchanged");

    // On Windows: no Wine exports, then the NGX core, all on one line.
    std::ostringstream platform;
    platform << std::boolalpha;
    mhw::write_platform(platform);
    const auto line = platform.str();
    check(line.rfind("{\"event\":\"platform\",\"wine\":false,\"ngx_core_registered\":", 0) == 0,
          "platform: wine false, then the NGX core");
    check(line.ends_with("}\n") && line.find('\n') == line.size() - 1, "platform is one line");
    std::printf("%s", line.c_str());
    std::printf("common tests passed\n");
}
