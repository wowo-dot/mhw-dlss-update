#include "driver_status.hpp"
#include <windows.h>
#include <cstddef>
#include "../../third_party/nvapi_sleep_status.hpp"

namespace mhw::reflex {
DriverSleepStatus driver_sleep_status(IUnknown *device) noexcept {
    if (!device) return {};
    // The SDK initialized NVAPI. Own a temporary loader reference while reading
    // its public interface; do not initialize/unload NVAPI's driver context.
    HMODULE module{};
    if (!GetModuleHandleExW(0, L"nvapi64.dll", &module)) return {};
    struct Pin {
        HMODULE value;
        ~Pin() { FreeLibrary(value); }
    } pin{module};
    using Query = void *(__cdecl *)(std::uint32_t);
    using Status = std::int32_t(__cdecl *)(IUnknown *, nvapi_abi::SleepStatus *);
#pragma warning(push)
#pragma warning(disable : 4191)
    const auto query = reinterpret_cast<Query>(GetProcAddress(module, "nvapi_QueryInterface"));
#pragma warning(pop)
    if (!query) return {};
    const auto get = reinterpret_cast<Status>(query(nvapi_abi::sleep_status_id));
    if (!get) return {};
    nvapi_abi::SleepStatus status{};
    status.version = nvapi_abi::sleep_status_version;
    const auto result = get(device, &status);
    if (result != 0) return {result, false};
    if (status.low_latency > 1) return {-10002, false};
    return {0, status.low_latency != 0};
}
DriverOffWait wait_driver_off(IUnknown *device) noexcept {
    DriverOffWait result;
    const auto started = GetTickCount64();
    for (unsigned i = 0; i < 101; ++i) {
        result.status = driver_sleep_status(device);
        ++result.queries;
        if (i == 0) result.initial_low_latency = result.status.low_latency;
        result.elapsed_ms = GetTickCount64() - started;
        if (result.status.result != 0 || !result.status.low_latency || result.elapsed_ms >= driver_off_timeout_ms)
            break;
        Sleep(10);
    }
    return result;
}
} // namespace mhw::reflex
