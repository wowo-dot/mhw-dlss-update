// Native presentation transaction; starts Off, live frame probe arms Reflex On.
// Public API detours use declared Windows x64
// signatures. No vendor call executes under DllMain/loader lock. Hash-qualified
// callers identify the retail game's factory observations and main DX12 device.
// Manual SL initialization occurs after the native main device is created and
// before returning to the initializer's queue/swap-chain creation instructions.
// Public creation calls preserve arguments/results/Win32 last-error.
#include "streamline.hpp"
#include "presentation.hpp"
#include "native_factory.hpp"
#ifdef MHW_REFLEX_TEMPORAL
#include "temporal_coexistence.hpp"
#ifdef MHW_FG_SCENE_PROBE
#include "../scaled_temporal/stream_route.hpp"
#endif
#endif
#ifdef MHW_FG_GAME
#include "../frame_generation/game_session.hpp"
#include "../frame_generation/crash_log.hpp"
#include "frame_generation_policy.hpp"
#include "../perf/frame_stats.hpp"
#include "../perf/toggles.hpp"
#include "../perf/sampler.hpp"
#include <algorithm>
#ifdef MHW_ALPHA
#include "../alpha/config.hpp"
#include <iterator>
// Alpha build: mhw_dlss.ini replaces the research sidecars, the session arms
// itself once the game is ready and runs until exit, with light logging.
namespace {
mhw::alpha::Config alpha_config;
// The shipped layout's mhw_dlss folder (empty under MO2 force-load): the only
// place inside the game folder the alpha reads its runtime from and logs to.
std::wstring alpha_home;
}
#endif
#include <cstdio>
#include <memory>
// "<dll>.fg-mode": "off" runs without DLSS-G (A/B diagnostic); "2x".."8x" sets
// the starting frame multiplier (default 2x). Live changes: "<log>.mfg-<n>".
namespace { bool fg_plugin = true; unsigned fg_multiplier = 2; }
#endif
#ifdef MHW_REFLEX_FRAME_PROBE
#include "live_frames.hpp"
#include "driver_status.hpp"
#endif
#include "../projection_binding/memory.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <array>
#include <fstream>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <string_view>

namespace {
namespace pb = mhw::projection_binding;
using FactoryFn = HRESULT(WINAPI*)(UINT, REFIID, void**);
using DeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
HMODULE module{};
FactoryFn factory_original{}; DeviceFn device_original{};
std::array<void*, 2> targets{};
// Own one process-lived loader reference per native API module. Hooks and
// forwarding trampolines may outlive the worker; do not unload their targets.
std::array<HMODULE, 2> creation_modules{};
std::array<std::array<std::uint64_t, 4>, 2> original_heads{};
std::array<bool, 2> created{}, enabled{};
std::atomic<unsigned> active{};
std::atomic<bool> accepting{}, completed{};
std::uintptr_t image{};
std::filesystem::path runtime, output;
std::ofstream log;
std::mutex sdk_mutex;
// These are process-lived. A failed shutdown retains the native device pin;
// no borrowed object is destroyed while the SDK could still refer to it.
// Explicit worker cleanup only. Never call vendor shutdown from CRT/DllMain
// teardown if the process exits before the bounded test has stopped.
auto& device_pin = *new Microsoft::WRL::ComPtr<ID3D12Device>;
auto& sdk = *new mhw::reflex::Streamline;
mhw::reflex::Presentation* presentation{};
#ifdef MHW_REFLEX_OWNERSHIP_PROBE
bool ownership_written{};
#endif
std::array<mhw::reflex::PresentationHook, mhw::reflex::Presentation::hook_capacity> presentation_heads{};
static_assert(sizeof(mhw::reflex::PresentationHook) == 48, "versioned retained-head metadata");
std::filesystem::path stop_file;
#ifdef MHW_REFLEX_FRAME_PROBE
std::filesystem::path arm_file;
mhw::reflex::LiveFrames* live_frames{};
bool frames_written{};
#ifdef MHW_REFLEX_TEMPORAL
bool temporal_cleanup_written{};
bool finish_temporal() noexcept {
    const bool clean=mhw::reflex::temporal::finish();
    try {
        if(!temporal_cleanup_written){
            mhw::reflex::temporal::receipt(log,"drained",device_pin.Get());
            temporal_cleanup_written=true;
        }
    }catch(...){return false;}
    return clean;
}
#endif
bool mode_change_attempted{}, mode_restored{};
constexpr auto capture_mode = sl::ReflexMode::eLowLatency;
constexpr unsigned capture_seconds = 600;
#endif
bool initialized{}, device_checked{}, success{};
unsigned owner{}, qualified_factories{}, qualified_devices{};
std::string failure;

void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
void api(sl::Result result, const char* reason) {
    if (result != sl::Result::eOk)
        throw std::runtime_error(std::string(reason) + ":" + std::to_string(static_cast<unsigned>(result)));
}
std::string quoted(const std::string& value) {
    std::ostringstream out; out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
        else out << c;
    }
    return out.str() + '"';
}
struct Call { Call() { active.fetch_add(1); } ~Call() { active.fetch_sub(1); } };
bool gate(unsigned rva) {
    if (rva == 0x23a15fd)
        return pb::verify_code(image, 0x23a0950, 4158, "138af20800e364c70b5a9228f6d294fbcccad1cf106e0013e84d008b2481e4bc");
    return pb::verify_code(image, 0x23a48e0, 11101, "8757a054cc1c203128fdfb73ac88975dda7dc73ef5ef1a53a80f22cd0eb8e500");
}
#ifdef MHW_REFLEX_FRAME_PROBE
bool driver_mode(const char* phase, bool expected, bool enforce=true) {
    const auto state=live_frames?live_frames->driver_status(device_pin.Get()):mhw::reflex::driver_sleep_status(device_pin.Get());
    log << "{\"event\":\"driver_sleep_status\",\"phase\":" << quoted(phase)
        << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get())
        << ",\"result\":" << state.result << ",\"low_latency\":" << state.low_latency
        << ",\"query_id\":" << 0xaef96ca1U << ",\"structure_version\":" << (136U | (1U<<16)) << "}\n" << std::flush;
    return state.result==0 && (!enforce || state.low_latency==expected);
}
void mode_options(const char* phase, sl::ReflexMode mode, sl::Result result) {
    log << "{\"event\":\"mode_options\",\"phase\":" << quoted(phase) << ",\"mode\":" << static_cast<unsigned>(mode)
        << ",\"result\":" << static_cast<unsigned>(result)
        << ",\"frame_limit_us\":0,\"use_markers_to_optimize\":false,\"boost_requested\":false}\n" << std::flush;
}
#endif
bool close_sdk() noexcept {
#ifdef MHW_REFLEX_TEMPORAL
    const bool temporal_clean=finish_temporal();
#endif
#ifdef MHW_REFLEX_FRAME_PROBE
    if(live_frames) {
        const bool restored=live_frames->stop();
        if(!frames_written) { try { live_frames->write(log);frames_written=true; } catch(...) { return false; } }
        if(!restored)return false;
    }
#endif
    if (presentation) {
        const bool restored = presentation->stop();
#ifdef MHW_REFLEX_OWNERSHIP_PROBE
        try { if(!ownership_written){presentation->write_ownership(log);ownership_written=true;} }catch(...){return false;}
#endif
        presentation_heads = presentation->hooks();
        try {
            for (unsigned i = 0; i < presentation_heads.size(); ++i) {
                const auto& head = presentation_heads[i]; if (!head.created) continue;
                log << "{\"event\":\"presentation_hook_restoration\",\"kind\":" << i << ",\"target\":" << head.target
                    << ",\"restored\":" << head.restored << ",\"original_head_sha256\":"
                    << quoted(pb::bytes_sha256(head.original.data(), 32)) << "}\n";
            }
            log << "{\"event\":\"presentation_cleanup\",\"restored\":" << restored << ",\"heads_rva\":"
                << reinterpret_cast<std::uintptr_t>(&presentation_heads) - reinterpret_cast<std::uintptr_t>(module)
                << ",\"head_record_bytes\":" << sizeof(mhw::reflex::PresentationHook) << "}\n" << std::flush;
        } catch (...) { return false; }
        if (!restored) return false;
    }
#ifdef MHW_REFLEX_FRAME_PROBE
    if(mode_change_attempted && !mode_restored) {
        // Every callback is stopped and drained before changing mode. Apply Off
        // even when the On call or its readback failed partway through setup.
        const auto result=sdk.options(sl::ReflexMode::eOff);
        try {
            mode_options("restore_off",sl::ReflexMode::eOff,result);
            // Frame and presentation callbacks have drained. Keep the device
            // pinned while the accepted Off request becomes observable.
            const auto wait=mhw::reflex::wait_driver_off(device_pin.Get());
            log << "{\"event\":\"driver_sleep_status\",\"phase\":\"restored_off\",\"device\":"
                << reinterpret_cast<std::uintptr_t>(device_pin.Get()) << ",\"result\":" << wait.status.result
                << ",\"low_latency\":" << wait.status.low_latency << ",\"query_id\":" << 0xaef96ca1U
                << ",\"structure_version\":" << (136U | (1U<<16)) << ",\"queries\":" << wait.queries
                << ",\"elapsed_ms\":" << wait.elapsed_ms << ",\"timeout_ms\":" << mhw::reflex::driver_off_timeout_ms
                << ",\"initial_low_latency\":" << wait.initial_low_latency << "}\n" << std::flush;
            const bool off=wait.status.result==0 && !wait.status.low_latency;
            mode_restored=result==sl::Result::eOk && off;
        } catch(...) { return false; }
        if(!mode_restored)return false;
    }
#endif
#ifdef MHW_REFLEX_TEMPORAL
    // Remove Reflex callbacks even on temporal failure, but retain SDK/device
    // ownership if temporal GPU work or hook removal is not proven complete.
    if(!temporal_clean)return false;
#endif
    const auto result = sdk.shutdown();
    if (result != sl::Result::eOk) return false;
    if (presentation && !presentation->release_after_shutdown()) return false;
    initialized = false; device_pin.Reset(); return true;
}
void failed(const std::exception& error) noexcept {
    try {
        failure = error.what(); accepting.store(false);
        // Worker drains creation callbacks before stopping routes or SDK.
        log << "{\"event\":\"failed\",\"reason\":" << quoted(failure) << "}\n" << std::flush;
    } catch (...) { accepting.store(false); }
    completed.store(true);
}
HRESULT WINAPI factory_hook(UINT flags, REFIID iid, void** result) {
    Call call; const auto error = GetLastError();
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (accepting.load() && (caller == image + 0x23a15fd || caller == image + 0x23a49c6)) {
        std::lock_guard lock(sdk_mutex);
        if (accepting.load()) {
            try {
                const auto rva = static_cast<unsigned>(caller - image);
                require(gate(rva), "native-factory-code-gate");
                ++qualified_factories;
                // Observation only. Missing an earlier factory callback must
                // not prevent attaching the qualified main native device.
            } catch (const std::exception& e) { failed(e); }
        }
    }
    SetLastError(error);
    return factory_original(flags, iid, result);
}
HRESULT WINAPI device_hook(IUnknown* adapter, D3D_FEATURE_LEVEL level, REFIID iid, void** out) {
    Call call;
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    const auto result = device_original(adapter, level, iid, out);
    const auto error = GetLastError();
    if (accepting.load() && caller == image + 0x23a4e2e) {
        std::lock_guard lock(sdk_mutex);
        if (accepting.load()) {
            try {
                require(gate(0x23a4e2e), "native-device-code-gate");
                require(!initialized && !device_checked && !qualified_devices, "duplicate-startup-device");
                ++qualified_devices;
                require(SUCCEEDED(result) && out && *out, "native-device-creation");
                auto* created_device = static_cast<IUnknown*>(*out);
                require(SUCCEEDED(created_device->QueryInterface(IID_PPV_ARGS(&device_pin))), "native-device-interface");
                require(device_pin->GetNodeCount() == 1 && SUCCEEDED(device_pin->GetDeviceRemovedReason()), "native-device-health");
                owner = GetCurrentThreadId();
#ifdef MHW_FG_GAME
                // FG target only: load DLSS-G with Reflex/PCL; the default stays Reflex-only.
                sdk.initialize(runtime, output / L"sdk-logs", fg_plugin ? mhw::reflex::StreamlineFeatures::reflex_and_frame_generation
                                                                         : mhw::reflex::StreamlineFeatures::reflex_only);
#else
                sdk.initialize(runtime, output / L"sdk-logs");
#endif
                initialized = true;
                log << "{\"event\":\"sdk_initialized\",\"before_factory\":false,\"after_native_device_creation\":true,"
                       "\"before_native_queues_and_swapchain\":true,\"caller_rva\":" << 0x23a4e2e
                    << ",\"thread\":" << owner << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get())
                    << ",\"sleep\":false,\"markers\":false}\n" << std::flush;
                auto luid = device_pin->GetAdapterLuid();
                sl::AdapterInfo info{}; info.deviceLUID = reinterpret_cast<std::uint8_t*>(&luid);
                info.deviceLUIDSizeInBytes = sizeof(luid);
                api(sdk.supported(sl::kFeatureReflex, info), "Reflex-support");
                api(sdk.supported(sl::kFeaturePCL, info), "PCL-support");
#ifdef MHW_FG_GAME
                if (fg_plugin) {
                    const auto fg_support = sdk.supported(sl::kFeatureDLSS_G, info);
                    sl::FeatureRequirements fg_requirements{};
                    const auto fg_requirements_result = sdk.requirements(sl::kFeatureDLSS_G, fg_requirements);
                    log << "{\"event\":\"fg_support\",\"supported\":" << static_cast<int>(fg_support)
                        << ",\"requirements\":" << static_cast<int>(fg_requirements_result)
                        << ",\"flags\":" << static_cast<unsigned>(fg_requirements.flags)
                        << ",\"max_viewports\":" << fg_requirements.maxNumViewports
                        << ",\"required_tags\":" << fg_requirements.numRequiredTags << "}\n" << std::flush;
                    api(fg_support, "DLSS-G-support"); api(fg_requirements_result, "DLSS-G-requirements");
                }
#endif
                api(sdk.attach(device_pin.Get()), "attach-native-device");
#ifdef MHW_FG_GAME
                if (fg_plugin) api(sdk.frame_generation_off(sl::ViewportHandle{0}, true), "DLSS-G-initially-Off");
#endif
                api(sdk.options(sl::ReflexMode::eOff), "Reflex-Off");
                sl::ReflexState state{}; api(sdk.state(state), "Reflex-state");
                device_checked = true;
                log << "{\"event\":\"device_checked\",\"caller_rva\":" << 0x23a4e2e
                    << ",\"thread\":" << owner << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get())
                    << ",\"luid_low\":" << luid.LowPart << ",\"luid_high\":" << luid.HighPart
                    << ",\"feature_level\":" << static_cast<unsigned>(level)
                    << ",\"low_latency_available\":" << state.lowLatencyAvailable
                    << ",\"mode\":0,\"rendering_modified\":false,\"sleep_calls\":0,\"markers\":0}\n" << std::flush;
                require(state.lowLatencyAvailable, "Reflex-unavailable");
                auto factory_pin = mhw::reflex::adapter_factory(adapter);
                // This hash-qualified call writes its device to [rdi+0x18].
                // The constructor's renderer global is not published until
                // RVA 0x23a72d3, after queues and the swap chain already exist.
                const auto device_output = reinterpret_cast<std::uintptr_t>(out);
                require(device_output >= 0x18, "startup-native-device-output");
                const auto backend = device_output - 0x18;
                std::uintptr_t factory{}, selected_adapter{}, stored_device{}, published{};
                require(pb::read(backend, factory) && factory && pb::read(backend + 0x10, selected_adapter) &&
                    selected_adapter == reinterpret_cast<std::uintptr_t>(adapter) &&
                    pb::read(backend + 0x18, stored_device) && stored_device == reinterpret_cast<std::uintptr_t>(*out),
                    "startup-native-output-owner");
                Microsoft::WRL::ComPtr<IDXGIFactory2> owner_factory;
                require(SUCCEEDED(reinterpret_cast<IUnknown*>(factory)->QueryInterface(IID_PPV_ARGS(&owner_factory))) &&
                    owner_factory.Get() == factory_pin.Get(), "startup-native-factory-identity");
                require(pb::read(image + 0x51c7050, published), "startup-native-publication-slot");
                log << "{\"event\":\"factory_context\",\"source\":\"native-adapter-parent\",\"output_owner_matches\":true,"
                       "\"factory\":" << reinterpret_cast<std::uintptr_t>(factory_pin.Get()) << ",\"backend\":" << backend
                    << ",\"global_backend_published\":" << (published == backend) << "}\n" << std::flush;
                presentation = new mhw::reflex::Presentation(sdk);
#ifdef MHW_REFLEX_OWNERSHIP_PROBE
                presentation->ownership(image,0x55d3000);
#endif
#ifdef MHW_FG_GAME
                // Streamline owns pacing, generated presentation and the host-visible buffers.
                presentation->install(device_pin.Get(), factory_pin.Get(), fg_plugin ? mhw::reflex::PresentationContract::sdk_paced
                                                                                      : mhw::reflex::PresentationContract::synchronous_native);
                // DLSS-G Off before any host window manipulation (guide 12.0).
                presentation->lifecycle({presentation, [](void*, unsigned method) noexcept {
                    return mhw::frame_generation::game::lifecycle_guard(method); }});
#else
                presentation->install(device_pin.Get(), factory_pin.Get());
#endif
                success = true; accepting.store(false); completed.store(true);
                log << "{\"event\":\"presentation_installed\",\"factory\":" << reinterpret_cast<std::uintptr_t>(factory_pin.Get())
                    << ",\"device_native\":true,\"returned_interfaces_native\":true,\"creation_routes\":4,\"mode\":0}\n" << std::flush;
            } catch (const std::exception& e) { failed(e); }
        }
    }
    SetLastError(error); return result;
}
std::filesystem::path path_setting(const std::filesystem::path& file) {
    require(std::filesystem::file_size(file) > 0 && std::filesystem::file_size(file) <= 4096, "configuration-size");
    std::ifstream in(file, std::ios::binary); std::string value; std::getline(in, value);
    if (!value.empty() && value.back() == '\r') value.pop_back();
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    require(count > 0, "configuration-utf8"); std::wstring wide(static_cast<std::size_t>(count), L'\0');
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), wide.data(), count) == count, "configuration-utf8");
    const std::filesystem::path path(wide); require(path.is_absolute() && std::filesystem::is_directory(path), "configuration-directory");
    return std::filesystem::canonical(path);
}
bool clean_hooks() noexcept {
    accepting.store(false); bool ok = true;
    for (unsigned i = 0; i < targets.size(); ++i)
        if (enabled[i]) {
            const auto status = MH_DisableHook(targets[i]);
            if (status == MH_OK || status == MH_ERROR_DISABLED) enabled[i] = false; else ok = false;
        }
    const auto began = GetTickCount64();
    while (active.load() && GetTickCount64() - began < 30000) Sleep(10);
    if (active.load()) return false;
    for (unsigned i = 0; i < targets.size(); ++i) if (created[i]) {
        std::array<std::uint64_t, 4> current{}; bool read = true;
        for (unsigned n = 0; n < current.size(); ++n)
            read = pb::read(reinterpret_cast<std::uintptr_t>(targets[i]) + n*8ULL, current[n]) && read;
        const bool restored = read && current == original_heads[i];
        ok = ok && restored;
        try { log << "{\"event\":\"hook_restoration\",\"kind\":" << i << ",\"target\":" << reinterpret_cast<std::uintptr_t>(targets[i])
            << ",\"restored\":" << restored << ",\"original_head_sha256\":" << quoted(pb::bytes_sha256(original_heads[i].data(), 32))
            << "}\n" << std::flush; } catch (...) { ok = false; }
    }
    // Keep the DLL/trampolines pinned for originals already fetched by callers.
    // Do not remove hooks, uninitialize MinHook or unload the interposer here.
    return ok;
}
bool query_swap(std::uintptr_t address, IDXGISwapChain** owned, DXGI_SWAP_CHAIN_DESC* desc) noexcept {
    __try {
        return SUCCEEDED(reinterpret_cast<IUnknown*>(address)->QueryInterface(__uuidof(IDXGISwapChain), reinterpret_cast<void**>(owned))) &&
               SUCCEEDED((*owned)->GetDesc(desc));
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool describe_swap() {
    const auto began = GetTickCount64();
    while (GetTickCount64() - began < 60000) {
        std::uintptr_t backend{}, swap{}, table{}, qi{}, release{}, desc_method{}, again{};
        if (pb::read(image + 0x51c7050, backend) && backend && pb::read(backend + 0x1470, swap) && swap &&
            pb::read(swap, table) && table && pb::read(table, qi) && pb::read(table + 16, release) && pb::read(table + 96, desc_method)) {
            bool native = true;
            for (const auto method : {qi, release, desc_method}) {
                MEMORY_BASIC_INFORMATION memory{};
                native = VirtualQuery(reinterpret_cast<void*>(method), &memory, sizeof(memory)) == sizeof(memory) &&
                    memory.Type == MEM_IMAGE && memory.AllocationBase == GetModuleHandleW(L"dxgi.dll") && native;
            }
            Microsoft::WRL::ComPtr<IDXGISwapChain> pin; DXGI_SWAP_CHAIN_DESC desc{};
            if (native && query_swap(swap, &pin, &desc) && pb::read(backend + 0x1470, again) && again == swap) {
                log << "{\"event\":\"swapchain_snapshot\",\"native_interface\":true,\"swapchain\":" << swap
                    << ",\"width\":" << desc.BufferDesc.Width << ",\"height\":" << desc.BufferDesc.Height
                    << ",\"format\":" << static_cast<unsigned>(desc.BufferDesc.Format) << ",\"buffers\":" << desc.BufferCount
                    << ",\"windowed\":" << (desc.Windowed != FALSE) << "}\n" << std::flush;
                return desc.BufferDesc.Width && desc.BufferDesc.Height && desc.BufferDesc.Format != DXGI_FORMAT_UNKNOWN;
            }
        }
        Sleep(50);
    }
    return false;
}
void load_creation_api(unsigned kind, const wchar_t* name, const char* symbol) {
    const bool was_loaded = GetModuleHandleW(name) != nullptr;
    wchar_t system[32768]{}, actual[32768]{};
    const auto length = GetSystemDirectoryW(system, static_cast<UINT>(std::size(system)));
    require(length && length < std::size(system), "system-directory");
    const auto path = std::filesystem::path(system) / name;
    creation_modules[kind] = LoadLibraryExW(path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    require(creation_modules[kind] != nullptr, "load-native-creation-module");
    const auto count = GetModuleFileNameW(creation_modules[kind], actual, static_cast<DWORD>(std::size(actual)));
    require(count && count < std::size(actual) && std::filesystem::equivalent(path, actual), "native-module-path");
    targets[kind] = reinterpret_cast<void*>(GetProcAddress(creation_modules[kind], symbol));
    require(targets[kind] && pb::executable(reinterpret_cast<std::uintptr_t>(targets[kind]),
        reinterpret_cast<std::uintptr_t>(creation_modules[kind])), "native-creation-export");
    log << "{\"event\":\"api_module\",\"kind\":" << kind << ",\"was_loaded\":" << was_loaded
        << ",\"loaded_from_system32\":true,\"module\":" << reinterpret_cast<std::uintptr_t>(creation_modules[kind])
        << ",\"target\":" << reinterpret_cast<std::uintptr_t>(targets[kind]) << "}\n" << std::flush;
}
#ifdef MHW_FG_GAME
std::atomic<bool> boot_monitor_stop{};
// Diagnostic boot timeline: text table 0 (message table array at host
// +0x5012DC0), the UI object's state at [[engine+0xAF800]+0x51ACC], and the
// swap-chain view, sampled every 20 ms and written when anything changes.
void boot_monitor(std::filesystem::path path) noexcept {
    try {
        std::ofstream out(path, std::ios::binary); if (!out) return;
        const auto began = GetTickCount64();
        std::uint64_t last_key = ~0ULL; std::uintptr_t last_table{}, last_ui{};
        while (!boot_monitor_stop.load() && GetTickCount64() - began < 180000) {
            std::uintptr_t table{}, engine{}, ui{}; unsigned state = ~0U, state_b = ~0U;
            pb::read(image + 0x5012DC0, table);
            if (pb::read(image + 0x51c4148, engine) && engine && pb::read(engine + 0xAF800, ui) && ui) {
                pb::read(ui + 0x51ACC, state); pb::read(ui + 0x51AB0, state_b);
            }
            mhw::frame_generation::PresentationBuffers b{}; mhw::reflex::scene_presentation_buffers(b);
            const std::uint64_t key = (std::uint64_t(state) << 32) ^ (std::uint64_t(state_b) << 16) ^ (table != 0) ^
                (b.generation << 8) ^ (b.present_other << 20) ^ (std::uint64_t(ui != 0) << 1);
            if (key != last_key || (table != 0) != (last_table != 0) || ui != last_ui) {
                out << "{\"t_ms\":" << GetTickCount64() - began << ",\"table0\":" << (table != 0) << ",\"ui\":" << (ui != 0)
                    << ",\"state\":" << static_cast<int>(state) << ",\"state_b\":" << static_cast<int>(state_b)
                    << ",\"generation\":" << b.generation << ",\"present_calls\":" << b.present_calls
                    << ",\"present_other\":" << b.present_other << "}\n" << std::flush;
                last_key = key; last_table = table; last_ui = ui;
            }
            Sleep(20);
        }
    } catch (...) {}
}
// Swap-chain view the host received through the routed calls (diagnostic).
int fg_buffers_json(char* out, int size) noexcept {
    mhw::frame_generation::PresentationBuffers b{};
    if (!mhw::reflex::scene_presentation_buffers(b)) return 0;
    return std::snprintf(out, static_cast<size_t>(size),
        "{\"generation\":%llu,\"known_mask\":%u,\"game_index\":%u,\"game_index_max\":%u,\"game_index_calls\":%llu,"
        "\"out_of_range\":%llu,\"conflicts\":%llu,\"foreign\":%llu,\"present_calls\":%llu,\"present_ok\":%llu,"
        "\"present_other\":%llu,\"first_results\":[%u,%u,%u,%u],\"first_result_at\":[%llu,%llu,%llu,%llu]}",
        static_cast<unsigned long long>(b.generation), b.known_mask, b.game_index, b.game_index_max,
        static_cast<unsigned long long>(b.game_index_calls), static_cast<unsigned long long>(b.out_of_range),
        static_cast<unsigned long long>(b.conflicts), static_cast<unsigned long long>(b.foreign),
        static_cast<unsigned long long>(b.present_calls), static_cast<unsigned long long>(b.present_ok),
        static_cast<unsigned long long>(b.present_other), b.present_results[0], b.present_results[1], b.present_results[2],
        b.present_results[3], static_cast<unsigned long long>(b.present_result_at[0]),
        static_cast<unsigned long long>(b.present_result_at[1]), static_cast<unsigned long long>(b.present_result_at[2]),
        static_cast<unsigned long long>(b.present_result_at[3]));
}
void log_fg_buffers(std::ostream& out, const char* phase) {
    char text[512]{}; const int n = fg_buffers_json(text, sizeof text);
    out << "{\"event\":\"fg_buffers\",\"phase\":\"" << phase << "\",\"buffers\":" << (n > 0 ? text : "null") << "}\n" << std::flush;
}
// Host swap-chain lifecycle calls recorded by the presentation hooks.
std::uint64_t lifecycle_dropped_logged{};
const char* stop_reason="not_armed";  // Why the FG worker loop ended.
void log_lifecycle(std::ostream& out) {
    if (!presentation) return;
    std::array<mhw::reflex::LifecycleEvent, mhw::reflex::Presentation::lifecycle_capacity> events{};
    std::uint64_t dropped{};
    const auto count = presentation->drain_lifecycle(events, dropped);
    if (!count && dropped == lifecycle_dropped_logged) return;
    LARGE_INTEGER frequency{}; QueryPerformanceFrequency(&frequency);
    constexpr const char* names[]{"", "create_swap_chain", "create_for_hwnd", "create_for_core_window", "", "", "set_fullscreen_state",
                                  "", "resize_buffers", "", "", "resize_buffers1", "resize_target"};
    for (unsigned i = 0; i < count; ++i) {
        const auto& e = events[i];
        out << "{\"event\":\"swap_lifecycle\",\"sequence\":" << e.sequence << ",\"method\":" << e.method << ",\"name\":\""
            << (e.method < std::size(names) ? names[e.method] : "") << "\",\"thread\":" << e.thread << ",\"host_thread\":" << e.host_thread
            << ",\"current_chain\":" << e.current_chain << ",\"routed\":" << e.routed << ",\"guard_off\":" << e.guard_off
            << ",\"result\":" << e.result << ",\"qpc\":" << e.began_qpc << ",\"ms\":"
            << (frequency.QuadPart ? double(e.ended_qpc - e.began_qpc) * 1000.0 / double(frequency.QuadPart) : 0.0) << ",\"args\":[";
        for (unsigned n = 0; n < e.args.size(); ++n) out << (n ? "," : "") << e.args[n];
        out << "],\"after\":";
        if (e.described)
            out << "{\"width\":" << e.width << ",\"height\":" << e.height << ",\"format\":" << e.format << ",\"buffers\":" << e.buffers
                << ",\"flags\":" << e.flags << ",\"windowed\":" << e.windowed << "}";
        else out << "null";
        out << "}\n";
    }
    if (dropped != lifecycle_dropped_logged) {
        out << "{\"event\":\"swap_lifecycle_dropped\",\"dropped\":" << dropped << "}\n";
        lifecycle_dropped_logged = dropped;
    }
    out << std::flush;
}
// Overhead measurements (v0.12): host frame intervals for the last window,
// labelled with the active perf switches. Armed runs add the driver's own
// per-frame report (Reflex markers), which separates CPU and GPU time.
void log_frames(std::ostream& out, const char* phase) {
    const auto w = mhw::perf::host_frames.take();
    out << "{\"event\":\"frame_stats\",\"phase\":\"" << phase << "\",\"perf\":" << mhw::perf::toggles.load() << ",\"frames\":" << w.frames
        << ",\"total_us\":" << w.total_us << ",\"fps\":" << (w.total_us ? double(w.frames) * 1e6 / double(w.total_us) : 0.0)
        << ",\"p50_us\":" << w.p50_us << ",\"p95_us\":" << w.p95_us << ",\"p99_us\":" << w.p99_us << ",\"max_us\":" << w.max_us
        << ",\"over_50ms\":" << w.over_50ms << "}\n" << std::flush;
}
template<std::size_t N> std::uint64_t median_of(std::array<std::uint64_t, N>& values, unsigned count) {
    if (!count) return 0;
    std::nth_element(values.begin(), values.begin() + count / 2, values.begin() + count);
    return values[count / 2];
}
template<std::size_t N> std::uint64_t max_of(const std::array<std::uint64_t, N>& values, unsigned count) {
    return count ? *std::max_element(values.begin(), values.begin() + count) : 0;
}
void log_latency(std::ostream& out) {
    auto state = std::make_unique<sl::ReflexState>();
    sl::Result result{};
    { std::lock_guard lock(sdk_mutex); result = sdk.state(*state); }
    out << "{\"event\":\"latency\",\"result\":" << static_cast<unsigned>(result) << ",\"available\":" << state->latencyReportAvailable;
    if (result == sl::Result::eOk && state->latencyReportAvailable) {
        std::array<const sl::ReflexReport*, sl::kReflexFrameReportCount> frames{};
        unsigned n{};
        for (const auto& f : state->frameReport)
            if (f.frameID && f.simEndTime >= f.simStartTime && f.renderSubmitEndTime >= f.renderSubmitStartTime && f.presentEndTime >= f.presentStartTime &&
                f.driverEndTime >= f.driverStartTime && f.osRenderQueueEndTime >= f.osRenderQueueStartTime && f.gpuRenderEndTime >= f.gpuRenderStartTime)
                frames[n++] = &f;
        std::sort(frames.begin(), frames.begin() + n, [](auto a, auto b) { return a->frameID < b->frameID; });
        // Microseconds as the driver reports them; interval = consecutive frames' present starts.
        std::array<std::array<std::uint64_t, sl::kReflexFrameReportCount>, 9> v{};
        std::array<unsigned, 9> c{};
        for (unsigned i = 0; i < n; ++i) {
            const auto& f = *frames[i];
            if (i && f.frameID == frames[i - 1]->frameID + 1 && f.presentStartTime >= frames[i - 1]->presentStartTime)
                v[0][c[0]++] = f.presentStartTime - frames[i - 1]->presentStartTime;
            v[1][c[1]++] = f.simEndTime - f.simStartTime;
            v[2][c[2]++] = f.renderSubmitEndTime - f.renderSubmitStartTime;
            v[3][c[3]++] = f.presentEndTime - f.presentStartTime;
            v[4][c[4]++] = f.driverEndTime - f.driverStartTime;
            v[5][c[5]++] = f.osRenderQueueEndTime - f.osRenderQueueStartTime;
            v[6][c[6]++] = f.gpuRenderEndTime - f.gpuRenderStartTime;
            v[7][c[7]++] = f.gpuActiveRenderTimeUs;
            v[8][c[8]++] = f.gpuFrameTimeUs;
        }
        constexpr std::array<const char*, 9> names{"interval", "simulation", "submit", "present", "driver", "queue", "gpu", "gpu_active", "gpu_frame"};
        out << ",\"frames\":" << n;
        for (unsigned k = 0; k < names.size(); ++k) {
            const auto top = max_of(v[k], c[k]);
            out << ",\"" << names[k] << "\":[" << median_of(v[k], c[k]) << ',' << top << ']';
        }
    }
    out << "}\n" << std::flush;
}
// Live perf switches: "<log>.perf-N" holds "<creation> <mask>", sequenced and
// generation-checked like the multiplier markers. The window before a switch
// is logged first, so every frame_stats line covers one mask.
void poll_perf(std::ostream& out, const std::filesystem::path& folder, const std::string& stem, std::uint64_t creation, unsigned& requests) {
    const auto marker = folder / (stem + ".perf-" + std::to_string(requests + 1));
    if (!std::filesystem::exists(marker) || std::filesystem::file_size(marker) > 64) return;
    std::ifstream input(marker); std::uint64_t generation{}; unsigned mask{}; input >> generation >> mask;
    const bool accepted = bool(input) && generation == creation && mask <= mhw::perf::toggle_mask;
    const auto previous = mhw::perf::toggles.load();
    ++requests;
    if (accepted) { log_frames(out, "switch"); mhw::perf::toggles.store(mask); }
    out << "{\"event\":\"perf_toggles\",\"request\":" << requests << ",\"mask\":" << mask << ",\"previous\":" << previous
        << ",\"accepted\":" << accepted << "}\n" << std::flush;
}
#endif
DWORD WINAPI worker(void*) noexcept {
    HMODULE pin{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&worker), &pin)) return 1;
    {   // One active copy per process: a second one (e.g. force-loaded and also
        // present as winmm.dll) stays inert. The handle lives as long as the process.
        const auto name = L"Local\\mhw_dlss.instance." + std::to_wstring(GetCurrentProcessId());
        const HANDLE instance = CreateMutexW(nullptr, FALSE, name.c_str());
        if (!instance || GetLastError() == ERROR_ALREADY_EXISTS) return 0;
    }
    try {
        wchar_t dll_name[32768]{}, host_name[32768]{};
        const auto dll_length = GetModuleFileNameW(module, dll_name, static_cast<DWORD>(std::size(dll_name)));
        const auto host_length = GetModuleFileNameW(nullptr, host_name, static_cast<DWORD>(std::size(host_name)));
        require(dll_length && dll_length < std::size(dll_name), "module-path");
        require(host_length && host_length < std::size(host_name), "host-path");
        const std::filesystem::path dll(dll_name);
#ifdef MHW_ALPHA
        {
            // Shipped layout: winmm.dll in the game folder, its configuration,
            // NVIDIA runtime and logs in the mhw_dlss folder next to it.
            auto base = dll.parent_path(); std::error_code missing;
            if (std::filesystem::is_regular_file(base / L"mhw_dlss" / L"mhw_dlss.ini", missing)) {
                base /= L"mhw_dlss"; alpha_home = std::filesystem::weakly_canonical(base, missing).wstring();
                if (missing) alpha_home.clear();
            }
            std::ifstream in(base / L"mhw_dlss.ini", std::ios::binary);
            const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            alpha_config = mhw::alpha::parse_config(text, base);
            output = alpha_config.log_dir; std::filesystem::create_directories(output);
            runtime = alpha_config.runtime_dir;
            fg_plugin = alpha_config.frame_generation; fg_multiplier = alpha_config.multiplier;
            mhw::perf::toggles.store(mhw::perf::all_toggles);
            mhw::reflex::sdk_verbose_log = false;
        }
#else
        auto data = dll; data += L".output-dir"; output = path_setting(data);
        auto provider = dll; provider += L".runtime-dir"; runtime = path_setting(provider);
#endif
#if defined(MHW_FG_GAME) && !defined(MHW_ALPHA)
        {   // Diagnostic A/B switch: "<dll>.fg-mode" containing "off" runs this
            // binary without the DLSS-G plugin (Reflex-only, synchronous routing).
            auto mode_file = dll; mode_file += L".fg-mode";
            std::ifstream mode_in(mode_file); std::string mode_text; mode_in >> mode_text;
            fg_plugin = mode_text != "off";
            if (const auto m = mhw::reflex::parse_multiplier(mode_text)) fg_multiplier = m;
            // "<dll>.perf": initial perf-switch mask (v0.12; 0 = original paths).
            auto perf_file = dll; perf_file += L".perf";
            std::ifstream perf_in(perf_file); unsigned mask{};
            if (perf_in >> mask && mask <= mhw::perf::toggle_mask) mhw::perf::toggles.store(mask);
        }
#endif
        const auto game_root = std::filesystem::canonical(std::filesystem::path(host_name).parent_path()).wstring();
        const auto under = [](const std::wstring& text, const std::wstring& root) {
            return text.size() >= root.size() && CompareStringOrdinal(text.data(), static_cast<int>(root.size()),
                root.data(), static_cast<int>(root.size()), TRUE) == CSTR_EQUAL &&
                (text.size() == root.size() || text[root.size()] == L'\\' || text[root.size()] == L'/');
        };
        for (const auto& path : {output, runtime}) {
            const auto text = path.wstring();
#ifdef MHW_ALPHA
            // The shipped layout keeps the NVIDIA runtime and logs in the mod's own
            // mhw_dlss folder inside the game folder; nothing else there is used.
            if (!alpha_home.empty() && under(text, alpha_home)) continue;
#endif
            require(!under(text, game_root), "configuration-in-game-directory");
        }
        image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)); std::string reason;
        require(pb::host_identity(image, reason), "retail-host-identity");
        FILETIME ct{}, et{}, kt{}, ut{}; require(GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut) != FALSE, "process-identity");
        const auto creation = (static_cast<std::uint64_t>(ct.dwHighDateTime) << 32) | ct.dwLowDateTime;
#ifdef MHW_FG_GAME
        unsigned perf_requests{};
#endif
#ifdef MHW_REFLEX_FRAME_PROBE
        const auto name = "reflex-frames-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(creation) + ".jsonl";
        arm_file=output/(name+".arm");require(!std::filesystem::exists(arm_file),"preserve-arm-file");
#else
        const auto name = "reflex-presentation-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(creation) + ".jsonl";
#endif
        stop_file = output / (name + ".stop");
        require(!std::filesystem::exists(stop_file), "preserve-stop-file");
        HANDLE file = CreateFileW((output/name).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        require(file != INVALID_HANDLE_VALUE, "preserve-existing-log"); CloseHandle(file);
        log.open(output/name, std::ios::binary | std::ios::app); require(bool(log), "log"); log << std::boolalpha;
#ifdef MHW_FG_GAME
        const bool crash_log_installed = mhw::frame_generation::crash_log::install(output/(name+".crash.jsonl"), fg_buffers_json);
#ifndef MHW_ALPHA
        if (const auto monitor = CreateThread(nullptr, 0, [](void* p) -> DWORD {
                std::unique_ptr<std::filesystem::path> path(static_cast<std::filesystem::path*>(p)); boot_monitor(*path); return 0; },
                new std::filesystem::path(output/(name+".boot.jsonl")), 0, nullptr)) CloseHandle(monitor);
#endif
#endif
#ifdef MHW_REFLEX_TEMPORAL
#if defined(MHW_FG_GAME)
#ifdef MHW_ALPHA
        log << "{\"event\":\"start\",\"schema\":1,\"version\":\"0.13.8\",\"build\":\"alpha\",\"experiment\":\"frame-generation-game\",\"target_mode\":1,\"fg_plugin_loaded\":" << fg_plugin
#else
        log << "{\"event\":\"start\",\"schema\":1,\"version\":\"0.12.13\",\"experiment\":\"frame-generation-game\",\"target_mode\":1,\"fg_plugin_loaded\":" << fg_plugin
#endif
            << ",\"fg_multiplier\":" << fg_multiplier << ",\"fg_enabled\":false,\"pid\":" << GetCurrentProcessId()
#elif defined(MHW_FG_SCENE_PROBE)
        log << "{\"event\":\"start\",\"schema\":7,\"version\":\"0.6.5\",\"experiment\":\"frame-generation-scene-boundary\",\"target_mode\":1,\"fg_plugin_loaded\":false,\"fg_enabled\":false,\"pid\":" << GetCurrentProcessId()
#elif defined(MHW_REFLEX_OWNERSHIP_PROBE)
        log << "{\"event\":\"start\",\"schema\":4,\"version\":\"0.6.0\",\"experiment\":\"frame-generation-game-ownership\",\"target_mode\":1,\"fg_plugin_loaded\":false,\"fg_enabled\":false,\"pid\":" << GetCurrentProcessId()
#else
        log << "{\"event\":\"start\",\"schema\":3,\"version\":\"0.5.1\",\"experiment\":\"reflex-temporal-coexistence\",\"target_mode\":1,\"pid\":" << GetCurrentProcessId()
#endif
#elif defined(MHW_REFLEX_FRAME_PROBE)
        log << "{\"event\":\"start\",\"schema\":2,\"version\":\"0.4.2\",\"experiment\":\"reflex-live-frames\",\"target_mode\":1,\"pid\":" << GetCurrentProcessId()
#else
        log << "{\"event\":\"start\",\"schema\":1,\"version\":\"0.2.1\",\"experiment\":\"reflex-persistent-presentation\",\"pid\":" << GetCurrentProcessId()
#endif
            << ",\"creation\":" << creation << ",\"host_sha256\":" << quoted(pb::host_sha)
            << ",\"host_version\":\"0.0.0.0\",\"dll_sha256\":" << quoted(pb::file_sha256(dll.wstring()))
            << ",\"sdk\":\"2.12.0\",\"graphics_api\":\"D3D12\",\"mode\":0,\"sleep\":false,\"markers\":false"
#ifdef MHW_REFLEX_TEMPORAL
            << ",\"upscaling\":true,\"temporal_version\":" << quoted(mhw::reflex::temporal::version())
#else
            << ",\"upscaling\":false"
#endif
            << "}\n" << std::flush;
#ifdef MHW_ALPHA
        log << "{\"event\":\"alpha_config\",\"frame_generation\":" << alpha_config.frame_generation << ",\"multiplier\":" << alpha_config.multiplier
            << ",\"reflex\":" << alpha_config.reflex << ",\"runtime_dir\":" << quoted(alpha_config.runtime_dir.string())
            << ",\"log_dir\":" << quoted(alpha_config.log_dir.string()) << ",\"warnings\":[";
        for (unsigned i = 0; i < alpha_config.warnings.size(); ++i) log << (i ? "," : "") << quoted(alpha_config.warnings[i]);
        log << "]}\n" << std::flush;
#endif
        // Refuse a late load; public API hooks are installed before first use.
        std::uintptr_t backend{}; require(pb::read(image + 0x51c7050, backend), "backend-slot");
        if (backend) { std::uintptr_t device{}; require(pb::read(backend + 0x18, device) && !device, "main-device-already-created"); }
        // The native constructor loads both modules dynamically. They are not
        // static imports of this probe, so GetModuleHandle alone races startup.
        // Resolve exact system modules before installing either creation hook.
        load_creation_api(0, L"dxgi.dll", "CreateDXGIFactory2");
        load_creation_api(1, L"d3d12.dll", "D3D12CreateDevice");
#ifdef MHW_REFLEX_FRAME_PROBE
        const bool pool_ready=mhw::reflex::LiveFrames::reserve(image,reason);
        const auto& pool=pb::pool_state();
        log << "{\"event\":\"frame_pool\",\"ready\":" << pool_ready << ",\"attempted\":" << pool.attempted
            << ",\"policy_available\":" << pool.policy_available << ",\"policy_flags\":" << pool.policy_flags
            << ",\"status\":" << pool.status << ",\"win32\":" << pool.win32 << ",\"origin\":" << pool.origin
            << ",\"slot\":" << pool.slot << ",\"page\":" << pool.page << ",\"reason\":" << quoted(reason) << "}\n" << std::flush;
        require(pool_ready,reason.c_str());
        require(MH_Initialize() == MH_ERROR_ALREADY_INITIALIZED, "hook-initialize-after-reservation");
#else
        require(MH_Initialize() == MH_OK, "hook-initialize");
#endif
        for (unsigned i = 0; i < targets.size(); ++i) {
            for (unsigned n = 0; n < original_heads[i].size(); ++n)
                require(pb::read(reinterpret_cast<std::uintptr_t>(targets[i]) + n*8ULL, original_heads[i][n]), "original-api-head");
            void* original{};
            require(MH_CreateHook(targets[i], i == 0 ? reinterpret_cast<void*>(&factory_hook) : reinterpret_cast<void*>(&device_hook), &original) == MH_OK, "creation-hook");
            created[i] = true;
            if (i == 0) factory_original = reinterpret_cast<FactoryFn>(original); else device_original = reinterpret_cast<DeviceFn>(original);
        }
        for (unsigned i = 0; i < targets.size(); ++i) require(MH_QueueEnableHook(targets[i]) == MH_OK, "queue-creation-hook");
        log << "{\"event\":\"prepared\",\"queued_api_hooks\":2,\"sdk_initialized\":false,\"original_heads_rva\":"
            << reinterpret_cast<std::uintptr_t>(&original_heads) - reinterpret_cast<std::uintptr_t>(module)
            << ",\"targets\":[" << reinterpret_cast<std::uintptr_t>(targets[0]) << ','
            << reinterpret_cast<std::uintptr_t>(targets[1]) << "]}\n" << std::flush;
        accepting.store(true);
        enabled.fill(true); // cleanup also covers a partially applied batch
        require(MH_ApplyQueued() == MH_OK, "enable-creation-hooks");
#ifdef MHW_REFLEX_TEMPORAL
        // Native API callbacks run on the game thread while this worker admits
        // the later NGX initializer. One early pool, one worker, two SDK owners.
#ifdef MHW_ALPHA
        mhw::reflex::temporal::prepare(module,output,image,runtime);
#else
        mhw::reflex::temporal::prepare(module,output,image);
#endif
        { std::lock_guard lock(sdk_mutex);
          mhw::reflex::temporal::receipt(log,"prepared",device_pin.Get()); }
#endif
        { std::lock_guard lock(sdk_mutex);
          log << "{\"event\":\"ready\",\"early_api_hooks\":2,\"startup_completed\":" << completed.load() << "}\n" << std::flush; }
        const auto began = GetTickCount64();
        while (!completed.load() && GetTickCount64() - began < 180000) Sleep(20);
        const bool hooks_clean = clean_hooks();
        const bool swap_observed = success && hooks_clean && describe_swap();
        if (swap_observed) {
#ifdef MHW_REFLEX_FRAME_PROBE
            require(driver_mode("startup_off",false),"driver-startup-Off");
            log << "{\"event\":\"frames_ready\",\"mode\":0,\"target_mode\":1,\"sleep_calls\":0,\"markers\":0,\"armed\":false,\"duration_seconds\":" << capture_seconds << "}\n" << std::flush;
#ifdef MHW_FG_GAME
            log << "{\"event\":\"fg_crash_log\",\"installed\":" << crash_log_installed << "}\n";
            log_fg_buffers(log, "frames_ready");
#endif
            const auto wait_start=GetTickCount64();
#ifdef MHW_FG_GAME
            log << "{\"event\":\"perf_toggles\",\"request\":0,\"mask\":" << mhw::perf::toggles.load() << ",\"previous\":0,\"accepted\":true}\n" << std::flush;
            mhw::perf::host_frames.take();
#ifndef MHW_ALPHA
            auto window_start=wait_start;
            mhw::perf::sampler::start();
#endif
#endif
#ifdef MHW_ALPHA
            // Arm once the swap chain has been unchanged for 3 s (startup creates
            // it and resizes it several times) and the game qualifies; until
            // then the game renders natively. The first and the latest reason
            // the game was not ready yet are logged when it arms.
            bool arm_now=false;
            {
                auto stats=presentation->snapshot();auto stable_since=GetTickCount64();unsigned attempts{};
                std::string not_ready,first_reason;
                while(stats.routing && !stats.faults) {
                    Sleep(250);
                    const auto now_stats=presentation->snapshot();
                    if(now_stats.swapchains!=stats.swapchains||now_stats.resizes!=stats.resizes)stable_since=GetTickCount64();
                    stats=now_stats;
                    log_lifecycle(log);
                    if(GetTickCount64()-stable_since<3000)continue;
                    not_ready.clear();
                    if(mhw::reflex::temporal::ready(not_ready)){arm_now=true;break;}
                    if(!attempts++)first_reason=not_ready;
                }
                log << "{\"event\":\"alpha_arm\",\"ready\":" << arm_now << ",\"attempts\":" << attempts << ",\"waited_ms\":"
                    << GetTickCount64()-wait_start << ",\"first_reason\":" << quoted(first_reason) << ",\"last_reason\":" << quoted(not_ready)
                    << ",\"routing\":" << stats.routing << ",\"faults\":" << stats.faults << "}\n" << std::flush;
            }
            if(arm_now) {
#else
            while(!std::filesystem::exists(arm_file) && !std::filesystem::exists(stop_file) && GetTickCount64()-wait_start<600000) {
                const auto stats=presentation->snapshot();if(!stats.routing || stats.faults)break;
#ifdef MHW_FG_GAME
                log_lifecycle(log);
                poll_perf(log,output,name,creation,perf_requests);
                if(GetTickCount64()-window_start>=5000){log_frames(log,"unarmed");mhw::perf::sampler::write(log);window_start=GetTickCount64();}
#endif
                Sleep(100);
            }
            if(std::filesystem::exists(arm_file) && !std::filesystem::exists(stop_file)) {
#endif
#ifdef MHW_REFLEX_OWNERSHIP_PROBE
                presentation->ownership_running();
#endif
#ifdef MHW_REFLEX_TEMPORAL
                // Qualify temporal gates before the three Reflex frame gates
                // are patched. Its own observer continues to own submission.
                mhw::reflex::temporal::arm(device_pin.Get());
                mhw::reflex::temporal::receipt(log,"armed",device_pin.Get());
#endif
                {
                    std::lock_guard lock(sdk_mutex);
                    mode_change_attempted=true;
                    const auto result=sdk.options(capture_mode);
                    mode_options("activate_on",capture_mode,result);api(result,"Reflex-On");
                    // Owned runs report Off immediately and after frame 8, On
                    // by frame 32. Keep this observation, require On at the
                    // first five-second checkpoint and again after drain.
                    require(driver_mode("requested_on",true,false),"driver-On-query");
                }
                live_frames=new mhw::reflex::LiveFrames(sdk);
                presentation->observe(live_frames->observer());
                live_frames->install(image,reinterpret_cast<std::uintptr_t>(device_pin.Get()),log);live_frames->describe(log,static_cast<unsigned>(capture_mode));
                log << "{\"event\":\"frames_armed\",\"mode\":1,\"duration_seconds\":" << capture_seconds << ",\"sleep_calls_enabled\":true,\"markers_enabled\":true}\n" << std::flush;
#ifdef MHW_FG_GAME
                // With MHW's aspect-ratio option the scene is letterboxed or
                // pillarboxed in the back buffer, centred (checked against the
                // screen write's iRegion and viewport in the FG status).
                unsigned scene_width{}, scene_height{}, window_width{}, window_height{};
                const bool scene_known = mhw::reflex::temporal::scene_extent(scene_width, scene_height, window_width, window_height);
                if (!fg_plugin) {
                    // "<dll>.fg-mode" off: DLSS and Reflex only; the DLSS-G plugin is not loaded.
                    log << "{\"event\":\"fg_unsupported\",\"reason\":\"plugin_off\"}\n" << std::flush;
                } else if (const auto chain = presentation->snapshot(); !scene_known || !scene_width || !scene_height ||
                    scene_width > chain.width || scene_height > chain.height || window_width != chain.width || window_height != chain.height) {
                    log << "{\"event\":\"fg_unsupported\",\"reason\":\"scene_rect\",\"scene_known\":" << scene_known
                        << ",\"scene\":[" << scene_width << ',' << scene_height << "],\"window\":[" << window_width << ',' << window_height
                        << "],\"chain\":[" << chain.width << ',' << chain.height << "]}\n" << std::flush;
                } else {
                    const mhw::frame_generation::game::SceneRect scene{(chain.width - scene_width) / 2, (chain.height - scene_height) / 2,
                                                                       scene_width, scene_height};
                    mhw::frame_generation::game::configure(sdk, chain.width, chain.height, scene);
                    mhw::frame_generation::game::multiplier(fg_multiplier);
                    mhw::frame_generation::game::arm(true);
                    sl::DLSSGState fg_state{};
                    const auto fg_state_result = sdk.frame_generation_state(sl::ViewportHandle{0}, fg_state);
                    log << "{\"event\":\"fg_armed\",\"width\":" << chain.width << ",\"height\":" << chain.height
                        << ",\"scene\":[" << scene.x << ',' << scene.y << ',' << scene.width << ',' << scene.height << ']'
                        << ",\"state_result\":" << static_cast<int>(fg_state_result) << ",\"status\":" << static_cast<unsigned>(fg_state.status)
                        << ",\"max_generated\":" << fg_state.numFramesToGenerateMax << ",\"min_width_or_height\":" << fg_state.minWidthOrHeight
                        << ",\"vsync_support\":" << static_cast<int>(fg_state.bIsVsyncSupportAvailable) << "}\n" << std::flush;
                }
#endif
                const auto started=GetTickCount64();auto last=started;
#ifdef MHW_FG_SCENE_PROBE
                unsigned scene_requested{};
#endif
#ifdef MHW_FG_GAME
#ifndef MHW_ALPHA
                unsigned multiplier_requests{};
#endif
                stop_reason="duration";
#endif
#ifdef MHW_ALPHA
                // 20 s, so a session closed soon after an event still logs it.
                constexpr std::uint64_t heartbeat_ms=20000;
                for(;;) {
#else
                constexpr std::uint64_t heartbeat_ms=5000;
                while(!std::filesystem::exists(stop_file) && GetTickCount64()-started<capture_seconds*1000ULL) {
#endif
                    const auto frames=live_frames->snapshot();const auto stats=presentation->snapshot();
#ifdef MHW_FG_GAME
                    log_lifecycle(log);
                    if(!frames.running || frames.faults){stop_reason="reflex_frames";break;}
                    if(!stats.routing || stats.faults){stop_reason="presentation";break;}
#endif
                    if(!frames.running || frames.faults || !stats.routing || stats.faults)break;
#ifdef MHW_FG_SCENE_PROBE
                    if(frames.completed&&scene_requested<4) {
                        const unsigned request=scene_requested+1;
                        const auto marker=output/(name+".scene-"+std::to_string(request));
                        bool valid=request==1;
                        if(!valid&&std::filesystem::exists(marker)){
                            require(std::filesystem::file_size(marker)<=64,"bounded-scene-request");
                            std::ifstream input(marker);std::uint64_t generation{};input>>generation;
                            valid=bool(input)&&generation==creation;
                            require(valid,"scene-request-generation");
                        }
                        // Request-time host GetBuffer labels seed presentation-only
                        // diagnostic matching; Present rechecks their generation.
                        mhw::frame_generation::PresentationBuffers buffers{};
                        const bool buffers_read=valid&&mhw::reflex::scene_presentation_buffers(buffers);
                        if(valid&&mhw::scaled_temporal::stream_route::scene_request(request,buffers)){
                            scene_requested=request;
                            log<<"{\"event\":\"scene_capture_requested\",\"request\":"<<request<<",\"generation_checked\":true"
                               <<",\"presentation_buffers_read\":"<<buffers_read<<",\"presentation_generation\":"<<buffers.generation
                               <<",\"presentation_known_mask\":"<<buffers.known_mask<<"}\n"<<std::flush;
                        }
                    }
#endif
#ifdef MHW_FG_GAME
#ifndef MHW_ALPHA
                    poll_perf(log,output,name,creation,perf_requests);
                    {   // Sequenced, generation-checked multiplier requests: "<creation> <total>".
                        const auto marker=output/(name+".mfg-"+std::to_string(multiplier_requests+1));
                        if(std::filesystem::exists(marker)&&std::filesystem::file_size(marker)<=64){
                            std::ifstream input(marker);std::uint64_t generation{};unsigned total{};input>>generation>>total;
                            const bool accepted=bool(input)&&generation==creation&&mhw::reflex::generated_frames_for(total,0);
                            if(accepted)mhw::frame_generation::game::multiplier(total);
                            ++multiplier_requests;
                            log<<"{\"event\":\"fg_multiplier\",\"request\":"<<multiplier_requests<<",\"multiplier\":"<<total
                               <<",\"accepted\":"<<accepted<<"}\n"<<std::flush;
                        }
                    }
#endif
#endif
#ifdef MHW_REFLEX_TEMPORAL
                    if(!mhw::reflex::temporal::healthy()){
#ifdef MHW_FG_GAME
                        stop_reason="temporal_unhealthy";mhw::reflex::temporal::fault_snapshot();
#endif
                        break;
                    }
#endif
                    const auto now=GetTickCount64();
                    if(now-last>=heartbeat_ms) {
#ifdef MHW_REFLEX_TEMPORAL
                        mhw::reflex::temporal::sample();
#ifdef MHW_FG_GAME
                        log << "{\"event\":\"fg_status\",\"session\":";mhw::frame_generation::game::write(log);log << "}\n" << std::flush;
#ifdef MHW_ALPHA
                        log << "{\"event\":\"dlss_summary\",\"runtime\":";mhw::reflex::temporal::summary(log);log << "}\n" << std::flush;
#endif
                        log_fg_buffers(log, "status");
                        log_frames(log, "armed");log_latency(log);mhw::perf::sampler::write(log);
#endif
#endif
                        if(last==started)require(driver_mode("running_on",true),"driver-On-not-enabled");
                        log << "{\"event\":\"frame_heartbeat\",\"completed\":" << frames.completed << ",\"tokens\":" << frames.tokens
                            << ",\"sleeps\":" << frames.sleeps << ",\"markers\":" << frames.markers << ",\"faults\":" << frames.faults
                            << ",\"owner\":" << frames.owner << ",\"presents\":" << stats.presents << ",\"native_presents\":" << stats.native_presents << "}\n" << std::flush;
                        last=now;
                    }
                    Sleep(100);
                }
            }
#else
            log << "{\"event\":\"persistent_ready\",\"mode\":0,\"sleep_calls\":0,\"markers\":0,\"ceiling_seconds\":600}\n" << std::flush;
            const auto started = GetTickCount64(); auto last = started;
            while (!std::filesystem::exists(stop_file) && GetTickCount64() - started < 600000) {
                const auto stats = presentation->snapshot();
                if (!stats.routing || stats.faults) break;
                std::uintptr_t current_backend{}, current_device{};
                if (!pb::read(image + 0x51c7050, current_backend) || !current_backend ||
                    !pb::read(current_backend + 0x18, current_device) || current_device != stats.native_device) {
                    log << "{\"event\":\"runtime_rejection\",\"reason\":\"native-device-changed\"}\n" << std::flush;
                    success = false; break;
                }
                const auto now = GetTickCount64();
                if (now - last >= 5000) {
                    log << "{\"event\":\"heartbeat\",\"presents\":" << stats.presents << ",\"native_presents\":" << stats.native_presents
                        << ",\"queues\":" << stats.queues << ",\"swapchains\":" << stats.swapchains << ",\"resizes\":" << stats.resizes
                        << ",\"faults\":" << stats.faults << ",\"native_swapchain\":" << stats.native_swapchain
                        << ",\"proxy_swapchain\":" << stats.proxy_swapchain << ",\"width\":" << stats.width << ",\"height\":" << stats.height
                        << ",\"format\":" << stats.format << ",\"buffers\":" << stats.buffers << ",\"installed_hooks\":" << stats.installed_hooks << "}\n" << std::flush;
                    last = now;
                }
                Sleep(100);
            }
#endif
        }
#ifdef MHW_FG_GAME
        // Under SDK pacing the game renders into Streamline's buffers and the
        // physical chain belongs to its pacer; neither can be retired under MHW.
        // Stop therefore means FG Off; DLSS, Reflex and routing stay live until exit.
        if(live_frames) {
            mhw::frame_generation::game::arm(false);
            const auto fg_off_began=GetTickCount64();
            while(mhw::frame_generation::game::frame_generation_on() && GetTickCount64()-fg_off_began<3000)Sleep(10);
            const auto frames=live_frames->snapshot();
            if(stop_reason==std::string_view("duration")&&std::filesystem::exists(stop_file))stop_reason="stop_request";
            log_lifecycle(log);
            log << "{\"event\":\"fg_stopped\",\"reason\":\"" << stop_reason << "\",\"off\":" << !mhw::frame_generation::game::frame_generation_on()
                << ",\"reflex_frames\":" << frames.completed << ",\"reflex_faults\":" << frames.faults << ",\"session\":";
            mhw::frame_generation::game::write(log);log << "}\n";
        }
        log << "{\"event\":\"fg_runtime_retained\",\"sdk_alive\":true,\"presentation_routed\":true,\"temporal_active\":true}\n" << std::flush;
        // Keep recording host lifecycle calls (resizes after FG ended) with a
        // presentation heartbeat on change, until exit or 30 minutes.
        {
            const auto observed_from=GetTickCount64();std::uint64_t last_resizes=~0ULL,last_swapchains=~0ULL;
            auto window_start=observed_from;log_frames(log,"stop");
            while(presentation&&GetTickCount64()-observed_from<1800000ULL){
                log_lifecycle(log);
                poll_perf(log,output,name,creation,perf_requests);
                if(GetTickCount64()-window_start>=5000){log_frames(log,"fg_off");log_latency(log);mhw::perf::sampler::write(log);window_start=GetTickCount64();}
                const auto stats=presentation->snapshot();
                if(stats.resizes!=last_resizes||stats.swapchains!=last_swapchains){
                    log<<"{\"event\":\"presentation_state\",\"swapchains\":"<<stats.swapchains<<",\"resizes\":"<<stats.resizes
                       <<",\"width\":"<<stats.width<<",\"height\":"<<stats.height<<",\"buffers\":"<<stats.buffers<<",\"faults\":"<<stats.faults
                       <<",\"routing\":"<<stats.routing<<",\"presents\":"<<stats.presents<<"}\n"<<std::flush;
                    last_resizes=stats.resizes;last_swapchains=stats.swapchains;
                }
                Sleep(100);
            }
        }
        return 0;
#endif
#ifdef MHW_REFLEX_TEMPORAL
        const bool temporal_clean=finish_temporal();
#endif
        std::lock_guard lock(sdk_mutex);
#ifdef MHW_REFLEX_FRAME_PROBE
        const bool frames_clean=!live_frames || live_frames->stop();
        const auto frame_stats=live_frames?live_frames->snapshot():mhw::reflex::LiveFrameStats{};
        if(live_frames) { live_frames->write(log);frames_written=true; }
        const bool latency_routed=!presentation || presentation->stop();
        if(initialized && frames_clean && latency_routed) {
            if(mode_change_attempted)require(driver_mode("stopped_on",true),"driver-On-not-retained");
            sl::ReflexState state{};api(sdk.state(state),"final-Reflex-state");
            unsigned reports{};
            for(const auto& f:state.frameReport) {
                if(!f.simStartTime || !f.simEndTime || !f.renderSubmitStartTime || !f.renderSubmitEndTime || !f.presentStartTime || !f.presentEndTime)continue;
                ++reports;
                log << "{\"event\":\"driver_frame\",\"frame\":" << f.frameID << ",\"simulation_start_us\":" << f.simStartTime
                    << ",\"simulation_end_us\":" << f.simEndTime << ",\"submit_start_us\":" << f.renderSubmitStartTime
                    << ",\"submit_end_us\":" << f.renderSubmitEndTime << ",\"present_start_us\":" << f.presentStartTime << ",\"present_end_us\":" << f.presentEndTime << "}\n";
            }
            log << "{\"event\":\"driver_state\",\"mode\":" << (mode_change_attempted?1:0) << ",\"mode_source\":\"requested\",\"low_latency_available\":" << state.lowLatencyAvailable
                << ",\"reports_available\":" << state.latencyReportAvailable << ",\"complete_reports\":" << reports << "}\n" << std::flush;
        }
#endif
        // Quiesce routes before the final state sample or vendor shutdown.
        const bool routes_clean = !presentation || presentation->stop();
        const auto stats = presentation ? presentation->snapshot() : mhw::reflex::PresentationStats{};
        const bool sdk_clean = close_sdk();
        log << "{\"event\":\"sdk_shutdown\",\"passed\":" << sdk_clean << ",\"after_presentation_stop\":true}\n";
        log << "{\"event\":\"finished\",\"startup_passed\":" << success << ",\"hooks_restored\":" << (hooks_clean && routes_clean)
            << ",\"sdk_shutdown_passed\":" << sdk_clean << ",\"qualified_factories\":" << qualified_factories
            << ",\"qualified_devices\":" << qualified_devices << ",\"pending_calls\":" << active.load() << ",\"swapchain_observed\":" << swap_observed
            << ",\"swapchain_proxy_installed\":false,\"game_reflex_active\":"
#ifdef MHW_REFLEX_FRAME_PROBE
            << (mode_change_attempted && !mode_restored)
#else
            << false
#endif
            << ",\"presents\":" << stats.presents
            << ",\"native_presents\":" << stats.native_presents << ",\"queues\":" << stats.queues << ",\"swapchains\":" << stats.swapchains
            << ",\"resizes\":" << stats.resizes << ",\"faults\":" << stats.faults << ",\"presentation_pending\":" << stats.pending_calls
            << ",\"pins_released\":" << sdk_clean
#ifdef MHW_REFLEX_TEMPORAL
            << ",\"temporal_drained\":" << temporal_clean
#endif
#ifdef MHW_REFLEX_FRAME_PROBE
            << ",\"frame_hooks_restored\":" << frames_clean << ",\"frame_calls\":" << frame_stats.completed
            << ",\"sleep_calls\":" << frame_stats.sleeps << ",\"markers\":" << frame_stats.markers
            << ",\"frame_faults\":" << frame_stats.faults
            << ",\"mode_restored_off\":" << mode_restored
#endif
            << "}\n" << std::flush;
#ifdef MHW_REFLEX_FRAME_PROBE
        return success && hooks_clean && routes_clean && frames_clean && sdk_clean && swap_observed && frame_stats.completed &&
            !frame_stats.faults && !frame_stats.frame_active && frame_stats.tokens==frame_stats.completed &&
            frame_stats.sleeps==frame_stats.completed && frame_stats.markers==frame_stats.completed*6 && !stats.faults && log ? 0 : 2;
#else
        return success && hooks_clean && routes_clean && sdk_clean && swap_observed && stats.presents && !stats.faults && log ? 0 : 2;
#endif
    } catch (const std::exception& e) {
#ifdef MHW_FG_GAME
        // Once Streamline has created the host chain under SDK pacing, the game
        // renders into Streamline's buffers: unhooking or shutting it down here
        // destroys them under the game. Keep presentation routed; FG Off only.
        if (presentation && presentation->snapshot().swapchains) {
            mhw::frame_generation::game::arm(false);
            try { log << "{\"event\":\"worker_failed\",\"reason\":" << quoted(e.what())
                << ",\"sdk_retained\":true,\"presentation_routed\":true,\"fg_off_requested\":true}\n" << std::flush; } catch (...) {}
            return 3;
        }
#endif
        const bool clean = clean_hooks();
        std::lock_guard lock(sdk_mutex);
        const bool sdk_clean=close_sdk(); // cleanup emits records before this JSON object
        try { log << "{\"event\":\"worker_failed\",\"reason\":" << quoted(e.what()) << ",\"hooks_restored\":" << clean
            << ",\"sdk_shutdown_passed\":" << sdk_clean << "}\n" << std::flush; } catch (...) {}
        return 3;
    }
}
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        module = instance; DisableThreadLibraryCalls(instance);
        const auto thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
