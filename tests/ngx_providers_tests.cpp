// Which DLSS NGX loaded instead of the mod's (src/ngx_startup/providers.hpp).
#include "../src/ngx_startup/providers.hpp"
#include "../src/common/dlss_files.hpp"
#include <iostream>
using namespace mhw::ngx_startup;

static_assert(stronger(Provider::none, Provider::file) == Provider::file);
static_assert(stronger(Provider::file, Provider::nvidia_store) == Provider::nvidia_store);
static_assert(stronger(Provider::none, Provider::none) == Provider::none);
static_assert(provider_reason(Provider::none) == std::string_view("coexistence-NGX-startup"));

int main() {
    int failures{};
    const auto check = [&](bool ok, const char *what) {
        if (!ok) {
            std::cerr << "failed: " << what << '\n';
            ++failures;
        }
    };
    check(provider_kind(L"C:\\ProgramData\\NVIDIA\\NGX\\models\\dlss\\versions\\1\\files\\model.bin") ==
              Provider::nvidia_store,
          "the NVIDIA App's override: NGX's own store, any file name");
    check(provider_kind(L"c:/programdata/nvidia/ngx/models/dlss/nvngx_dlss.dll") == Provider::nvidia_store,
          "any case, forward slashes");
    check(provider_kind(L"D:\\Steam\\steamapps\\common\\Monster Hunter World\\nvngx_dlss.dll") == Provider::file,
          "a DLSS file in the game's folder");
    check(provider_kind(L"D:\\Tools\\NVNGX_DLSSG.DLL") == Provider::file, "another DLSS file name");
    check(provider_kind(L"C:\\Windows\\System32\\DriverStore\\FileRepository\\nv_dispi.inf_amd64\\_nvngx.dll") ==
              Provider::none,
          "NGX's core is not a provider");
    check(provider_kind(L"C:\\Windows\\System32\\nvngx_dlss.dll.mui") == Provider::none, "not a DLL");
    check(provider_kind(L"nvngx_dlss.dll") == Provider::file, "a bare name");
    check(provider_kind(L"") == Provider::none, "empty");

    // 0.32.0: which DLSS files run (src/common/dlss_files.hpp). The run of
    // 2026-10-27 with the NVIDIA App's override on: NVIDIA's store copy,
    // 310.9.16, signed by NVIDIA Corporation, and the shipped 310.7.
    using mhw::DlssFile;
    const auto file = [](const char *internal, mhw::FileVersion v, bool shipped, bool signed_) {
        return DlssFile{internal, v, shipped, signed_, signed_ ? "NVIDIA Corporation" : ""};
    };
    check(mhw::dlss_accepted(file("DLSS", {310, 9, 16, 0}, false, true), mhw::dlss_super_resolution),
          "the NVIDIA App's override: NVIDIA's signed 310.9.16");
    check(mhw::dlss_accepted(file("DLSS", {310, 7, 0, 0}, true, false), mhw::dlss_super_resolution),
          "the shipped file by its hash alone (Wine has no signature check)");
    check(!mhw::dlss_accepted(file("DLSS", {310, 9, 16, 0}, false, false), mhw::dlss_super_resolution),
          "not NVIDIA's and not the shipped file");
    check(!mhw::dlss_accepted(file("DLSS", {3, 8, 10, 0}, false, true), mhw::dlss_super_resolution),
          "NVIDIA's, but older than the 310 API");
    check(!mhw::dlss_accepted(file("DLSS-G", {310, 9, 16, 0}, false, true), mhw::dlss_super_resolution) &&
              mhw::dlss_accepted(file("DLSS-G", {310, 9, 16, 0}, false, true), mhw::dlss_frame_generation),
          "frame generation's file for frame generation only");
    check(!mhw::dlss_accepted(file("", {}, false, false), mhw::dlss_super_resolution), "no version resource");
    check(mhw::version_text({310, 9, 16, 0}) == "310.9.16.0" &&
              mhw::FileVersion{310, 10, 0, 0} > mhw::FileVersion{310, 9, 16, 0},
          "versions compare per part");
    if (failures) return 1;
    std::cout << "NGX provider classification passed\n";
    return 0;
}
