// Which DLSS NGX loaded instead of the mod's (src/ngx_startup/providers.hpp).
#include "../src/ngx_startup/providers.hpp"
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
    if (failures) return 1;
    std::cout << "NGX provider classification passed\n";
    return 0;
}
