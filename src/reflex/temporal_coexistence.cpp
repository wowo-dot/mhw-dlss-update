#include "temporal_coexistence.hpp"
#include "../common/json.hpp"
#include "../common/require.hpp"
#include "../continuous_dlaa/runtime.hpp"
#include "../ngx_startup/startup.hpp"
#include "../perf/toggles.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace mhw::reflex::temporal {
namespace {
namespace cd = mhw::continuous_dlaa;
namespace pb = mhw::projection_binding;
constexpr const char *temporal_version = "1.25.44", *experiment = "continuous-scaled-native-resolve";
// Process-lived log owner: no vendor call or resource destruction in DllMain.
auto &events = *new std::ofstream;
std::filesystem::path directory, runtime;
std::string name;
std::uintptr_t image{};
bool prepared{}, attempted{}, installed{}, finished{}, clean{};
unsigned samples{};
} // namespace
const char *version() noexcept {
    return temporal_version;
}
void prepare(HMODULE module, const std::filesystem::path &output, std::uintptr_t base,
             const std::filesystem::path &ngx_runtime) {
    require(!events.is_open(), "duplicate-temporal-startup");
    directory = output;
    image = base;
    wchar_t path[32768]{};
    const auto length = GetModuleFileNameW(module, path, static_cast<DWORD>(std::size(path)));
    require(length && length < std::size(path), "temporal-module-path");
    FILETIME ct{}, et{}, kt{}, ut{};
    require(GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut) != FALSE, "temporal-process-identity");
    const auto creation = (static_cast<std::uint64_t>(ct.dwHighDateTime) << 32) | ct.dwLowDateTime;
    const auto pid = GetCurrentProcessId();
    name = "inputs-" + std::to_string(pid) + "-" + std::to_string(creation);
    const auto file = CreateFileW((directory / (name + ".jsonl")).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                  CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "preserve-temporal-log");
    CloseHandle(file);
    events.open(directory / (name + ".jsonl"), std::ios::binary | std::ios::app);
    require(bool(events), "temporal-log");
    events
        << std::boolalpha << "{\"event\":\"start\",\"schema\":1,\"version\":\"" << temporal_version
        << "\",\"pid\":" << pid << ",\"creation\":" << creation << ",\"base\":" << base
        << ",\"host_sha256\":" << quoted(pb::host_sha)
        << ",\"host_version\":\"0.0.0.0\",\"dll_sha256\":" << quoted(pb::file_sha256(path))
        << ",\"experiment\":" << quoted(experiment)
        << ",\"graphics_api\":\"D3D12\",\"frame_limit\":null,\"replacement_scope\":\"qualified-native-resolve\",\"exclusive_mode\":false,\"retention\":\"reset-and-fence-v1\",\"request_retirement\":\"validated-native-return-v1\",\"activation\":\"native-postqueue-preparation-v2\",\"reconstruction\":\"suspend-or-latch-unfiltered-v1\",\"state_trace\":\"consumer-local-state-v1\",\"native_state_trace\":\"scheduled-state-evidence-v1\",\"state_admission\":\"native-current-use-v1\",\"copy_telemetry\":\"single-fence-observation-v1\",\"depth_eligibility\":\"depth-eligibility-observation-v1\",\"depth_destination\":\"depth-destination-identity-v1\"}\n"
        << std::flush;
    runtime = ngx_runtime;
    require(runtime.is_absolute() && std::filesystem::is_directory(runtime), "NGX-runtime-directory");
    // Streamline initializes NGX first when DLSS-G is loaded; both share one runtime folder.
    mhw::ngx_startup::decline_game_dlss(); // its own DLSS 1: never with this mod
    const bool started = mhw::ngx_startup::run(module, temporal_version, directory, runtime);
    // NGX loaded another DLSS than the mod's (jgb2003-1): its own reason.
    require(started, mhw::ngx_startup::provider_reason(mhw::ngx_startup::foreign_provider()));
    events << "{\"event\":\"ready\",\"hooks_installed\":false}\n" << std::flush;
    prepared = true;
}
bool ready(std::string &reason) {
    if (!prepared || attempted) {
        reason = "temporal-arm-state";
        return false;
    }
    return cd::ready(image, reason);
}
void arm(ID3D12Device *device) {
    require(prepared && !attempted && device, "temporal-arm-state");
    const auto *owner = mhw::ngx_startup::borrowed_initialization();
    require(owner && owner->validate && owner->validate(device, runtime), "same-native-device-NGX-owner");
    attempted = true;
    std::string reason;
    installed = cd::install(image, runtime, reason);
    events << "{\"event\":\"armed\",\"installed\":" << installed << ",\"reason\":" << quoted(reason) << "}\n"
           << std::flush;
    cd::write(events);
    require(installed, reason.c_str());
    require(bool(events), "temporal-arm-log");
}
bool healthy() noexcept {
    return installed && !finished && cd::healthy() && bool(events);
}
bool scene_extent(unsigned &scene_width, unsigned &scene_height, unsigned &window_width,
                  unsigned &window_height) noexcept {
    return installed && cd::scene_extent(scene_width, scene_height, window_width, window_height);
}
bool render_extent(unsigned &width, unsigned &height) noexcept {
    return installed && cd::render_extent(width, height);
}
void sample() noexcept {
    // Unhealthy: the worker's loop stops on that itself (fault_snapshot).
    if (!healthy()) return;
    ++samples;
    // perf::quiet_status: the dumps hold the render locks while they format and write.
    if (mhw::perf::on(mhw::perf::quiet_status)) return;
    // A write that fails is skipped (up to 0.28.6 it ended the worker).
    try {
        {
            std::ofstream status(directory / (name + ".status.json"), std::ios::binary | std::ios::trunc);
            cd::write(status);
        }
        if (samples <= 12 || (samples & (samples - 1)) == 0) cd::write(events);
    } catch (...) {}
}
void summary(std::ostream &out) {
    if (!installed) {
        out << "null";
        return;
    }
    cd::write_summary(out);
}
void fault_snapshot() noexcept {
    try {
        if (!installed || !events.is_open()) return;
        std::ofstream status(directory / (name + ".status.json"), std::ios::binary | std::ios::trunc);
        cd::write(status);
        events << "{\"event\":\"fault_snapshot\"}\n";
        cd::write(events);
        events << std::flush;
    } catch (...) {}
}
bool finish() noexcept {
    if (finished) return clean;
    finished = true;
    clean = !attempted;
    try {
        if (attempted) {
            cd::finish();
            // Keep Reflex presentation routes live until all temporal GPU
            // recordings and callbacks retire. Never infer completion from
            // CPU hook removal or from a visually normal scene.
            const auto deadline = GetTickCount64() + 5000;
            while (!cd::quiescent() && cd::healthy() && GetTickCount64() < deadline) Sleep(1);
            cd::save_rejected_shader(directory / (name + "-rejected-taa.dxbc"));
            cd::write(events);
            clean = cd::quiescent();
        }
        if (events.is_open())
            events << "{\"event\":\"stopped\",\"armed\":" << installed
                   << ",\"healthy\":" << (clean && (!attempted || cd::healthy())) << ",\"dll_pinned\":true}\n"
                   << std::flush;
        return clean && (!events.is_open() || bool(events));
    } catch (...) {
        clean = false;
        return false;
    }
}
void receipt(std::ostream &log, const char *phase, ID3D12Device *device) {
    log << "{\"event\":\"temporal_coexistence\",\"phase\":" << quoted(phase)
        << ",\"version\":" << quoted(temporal_version) << ",\"input_log\":" << quoted(name + ".jsonl")
        << ",\"startup_log\":" << quoted("startup-" + name.substr(7) + ".jsonl")
        << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device) << ",\"prepared\":" << prepared
        << ",\"installed\":" << installed << ",\"finished\":" << finished << ",\"quiescent\":" << clean
        << ",\"NGX_shutdown_deferred\":true}\n"
        << std::flush;
}
} // namespace mhw::reflex::temporal
