// The game's device: at AL's DEVICE_CREATED (plugin.cpp) Streamline is
// initialized and attached, the card's capabilities read, Reflex checked and
// the presentation layer installed. Also the native swap chain's first
// description, and the SDK's shutdown when the worker fails before the game
// renders through it.
#include "plugin.hpp"
#include "../game_memory/layout.hpp"
#include "../settings/capabilities.hpp"
#include "../settings/menu.hpp"
#include "../settings/status.hpp"
#include "../projection_binding/memory.hpp"
#include "../reflex/driver_status.hpp"
#include "../reflex/native_factory.hpp"
#include "../reflex/temporal_coexistence.hpp"
#include <dxgi1_6.h>
#include <array>

namespace mhw::plugin {
namespace {
namespace pb = mhw::projection_binding;
bool initialized{}, device_checked{};
unsigned owner{}, qualified_devices{};
std::array<mhw::reflex::PresentationHook, mhw::reflex::Presentation::hook_capacity> presentation_heads{};
static_assert(sizeof(mhw::reflex::PresentationHook) == 48, "versioned retained-head metadata");
bool frames_written{};
bool temporal_cleanup_written{};
bool finish_temporal() noexcept {
    const bool clean = mhw::reflex::temporal::finish();
    try {
        if (!temporal_cleanup_written) {
            mhw::reflex::temporal::receipt(log, "drained", device_pin.Get());
            temporal_cleanup_written = true;
        }
    } catch (...) {
        return false;
    }
    return clean;
}
bool query_swap(std::uintptr_t address, IDXGISwapChain **owned, DXGI_SWAP_CHAIN_DESC *desc) noexcept {
    __try {
        return SUCCEEDED(reinterpret_cast<IUnknown *>(address)->QueryInterface(__uuidof(IDXGISwapChain),
                                                                               reinterpret_cast<void **>(owned))) &&
               SUCCEEDED((*owned)->GetDesc(desc));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
} // namespace

bool close_sdk() noexcept {
    const bool temporal_clean = finish_temporal();
    if (live_frames) {
        const bool restored = live_frames->stop();
        if (!frames_written) {
            try {
                live_frames->write(log);
                frames_written = true;
            } catch (...) {
                return false;
            }
        }
        if (!restored) return false;
    }
    if (presentation) {
        const bool restored = presentation->stop();
        presentation_heads = presentation->hooks();
        try {
            for (unsigned i = 0; i < presentation_heads.size(); ++i) {
                const auto &head = presentation_heads[i];
                if (!head.created) continue;
                // AL owns these patches: no original bytes here, nothing to restore
                // (0.28.6 logged presentation_hook_restoration, restored false).
                log << "{\"event\":\"presentation_hook\",\"kind\":" << i << ",\"target\":" << head.target
                    << ",\"enabled\":" << head.enabled << ",\"chain\":\"al.d3d12\"}\n";
            }
            log << "{\"event\":\"presentation_cleanup\",\"restored\":" << restored << ",\"heads_rva\":"
                << reinterpret_cast<std::uintptr_t>(&presentation_heads) - reinterpret_cast<std::uintptr_t>(module)
                << ",\"head_record_bytes\":" << sizeof(mhw::reflex::PresentationHook) << "}\n"
                << std::flush;
        } catch (...) {
            return false;
        }
        if (!restored) return false;
    }
    if (mode_change_attempted && !mode_restored) {
        // Every callback is stopped and drained before changing mode. Apply Off
        // even when the On call or its readback failed partway through setup.
        const auto result = sdk.options(sl::ReflexMode::eOff);
        try {
            mode_options("restore_off", sl::ReflexMode::eOff, result);
            // Frame and presentation callbacks have drained. Keep the device
            // pinned while the accepted Off request becomes observable.
            const auto wait = mhw::reflex::wait_driver_off(device_pin.Get());
            log << "{\"event\":\"driver_sleep_status\",\"phase\":\"restored_off\",\"device\":"
                << reinterpret_cast<std::uintptr_t>(device_pin.Get()) << ",\"result\":" << wait.status.result
                << ",\"low_latency\":" << wait.status.low_latency << ",\"query_id\":" << 0xaef96ca1U
                << ",\"structure_version\":" << (136U | (1U << 16)) << ",\"queries\":" << wait.queries
                << ",\"elapsed_ms\":" << wait.elapsed_ms << ",\"timeout_ms\":" << mhw::reflex::driver_off_timeout_ms
                << ",\"initial_low_latency\":" << wait.initial_low_latency << "}\n"
                << std::flush;
            const bool off = wait.status.result == 0 && !wait.status.low_latency;
            mode_restored = result == sl::Result::eOk && off;
        } catch (...) {
            return false;
        }
        if (!mode_restored) return false;
    }
    // Remove Reflex callbacks even on temporal failure, but retain SDK/device
    // ownership if temporal GPU work or hook removal is not proven complete.
    if (!temporal_clean) return false;
    const auto result = sdk.shutdown();
    if (result != sl::Result::eOk) return false;
    if (presentation && !presentation->release_after_shutdown()) return false;
    initialized = false;
    device_pin.Reset();
    return true;
}
void failed(const std::exception &error) noexcept {
    try {
        failure = error.what();
        accepting.store(false);
        // Worker drains creation callbacks before stopping routes or SDK.
        log << "{\"event\":\"failed\",\"reason\":" << quoted(failure) << "}\n" << std::flush;
    } catch (...) {
        accepting.store(false);
    }
    completed.store(true);
}
// The game's main device exists (AL's DEVICE_CREATED, from the game's own
// import call): Streamline, Reflex and presentation.
void attach_device(IUnknown *adapter, D3D_FEATURE_LEVEL level, IUnknown *created_device) {
    require(!initialized && !device_checked && !qualified_devices, "duplicate-startup-device");
    ++qualified_devices;
    require(created_device != nullptr, "native-device-creation");
    require(SUCCEEDED(created_device->QueryInterface(IID_PPV_ARGS(&device_pin))), "native-device-interface");
    require(device_pin->GetNodeCount() == 1 && SUCCEEDED(device_pin->GetDeviceRemovedReason()), "native-device-health");
    owner = GetCurrentThreadId();
    // v0.27.5: the card first (capabilities.hpp), so a failure below still shows it.
    namespace caps_ns = mhw::settings::capabilities;
    caps_ns::Capabilities caps;
    caps.known = true;
    if (Microsoft::WRL::ComPtr<IDXGIAdapter1> described;
        adapter && SUCCEEDED(adapter->QueryInterface(IID_PPV_ARGS(&described)))) {
        DXGI_ADAPTER_DESC1 d{};
        if (SUCCEEDED(described->GetDesc1(&d))) {
            char name[256]{};
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, static_cast<int>(sizeof(name)), nullptr, nullptr);
            caps.card.name = name;
            caps.card.vendor = d.VendorId;
            caps.card.device = d.DeviceId;
            caps.card.memory_mb = static_cast<std::uint64_t>(d.DedicatedVideoMemory) >> 20;
            // DLSS is NVIDIA's (the menu locks it with that reason).
            if (caps.card.vendor != caps_ns::nvidia) caps.dlss = caps_ns::Support::no;
        }
    }
    mhw::settings::status::card_capabilities(caps);
    // FG target only: load DLSS-G with Reflex/PCL; the default stays Reflex-only.
    sdk.initialize(runtime, output / L"sdk-logs",
                   fg_plugin ? mhw::reflex::StreamlineFeatures::reflex_and_frame_generation
                             : mhw::reflex::StreamlineFeatures::reflex_only);
    initialized = true;
    log << "{\"event\":\"sdk_initialized\",\"before_factory\":false,\"after_native_device_creation\":true,"
           "\"before_native_queues_and_swapchain\":true,\"caller_rva\":0" // the D3D12CreateDevice detour's caller up to v0.28.6
        << ",\"thread\":" << owner << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get())
        << ",\"sleep\":false,\"markers\":false}\n"
        << std::flush;
    auto luid = device_pin->GetAdapterLuid();
    sl::AdapterInfo info{};
    info.deviceLUID = reinterpret_cast<std::uint8_t *>(&luid);
    info.deviceLUIDSizeInBytes = sizeof(luid);
    { // The driver as Streamline detects it.
        sl::FeatureRequirements reflex_requirements{};
        if (sdk.requirements(sl::kFeatureReflex, reflex_requirements) == sl::Result::eOk) {
            caps.card.driver_major = reflex_requirements.driverVersionDetected.major;
            caps.card.driver_minor = reflex_requirements.driverVersionDetected.minor;
        }
        caps.reflex =
            sdk.supported(sl::kFeatureReflex, info) == sl::Result::eOk ? caps_ns::Support::yes : caps_ns::Support::no;
        if (!fg_plugin) {
            caps.frame_generation = caps_ns::Support::no;
            caps.missing = caps_ns::Missing::plugin_off;
        }
        mhw::settings::status::card_capabilities(caps);
    }
    api(sdk.supported(sl::kFeatureReflex, info), "Reflex-support");
    api(sdk.supported(sl::kFeaturePCL, info), "PCL-support");
    if (fg_plugin) {
        const auto fg_support = sdk.supported(sl::kFeatureDLSS_G, info);
        sl::FeatureRequirements fg_requirements{};
        const auto fg_requirements_result = sdk.requirements(sl::kFeatureDLSS_G, fg_requirements);
        log << "{\"event\":\"fg_support\",\"supported\":" << static_cast<int>(fg_support)
            << ",\"requirements\":" << static_cast<int>(fg_requirements_result)
            << ",\"flags\":" << static_cast<unsigned>(fg_requirements.flags)
            << ",\"max_viewports\":" << fg_requirements.maxNumViewports
            << ",\"required_tags\":" << fg_requirements.numRequiredTags << "}\n"
            << std::flush;
        { // Why not, in the SDK's own terms (sl_result.h).
            const auto result = fg_support != sl::Result::eOk ? fg_support : fg_requirements_result;
            caps.frame_generation_result = static_cast<unsigned>(result);
            caps.frame_generation = result == sl::Result::eOk ? caps_ns::Support::yes : caps_ns::Support::no;
            caps.missing = result == sl::Result::eOk                     ? caps_ns::Missing::none
                           : result == sl::Result::eErrorDriverOutOfDate ? caps_ns::Missing::driver
                           : result == sl::Result::eErrorOSOutOfDate     ? caps_ns::Missing::os
                           : result == sl::Result::eErrorOSDisabledHWS   ? caps_ns::Missing::scheduling
                           : result == sl::Result::eErrorAdapterNotSupported ||
                                   result == sl::Result::eErrorNoSupportedAdapterFound ||
                                   result == sl::Result::eErrorFeatureNotSupported
                               ? caps_ns::Missing::card
                               : caps_ns::Missing::other;
            if (fg_requirements_result == sl::Result::eOk) {
                caps.driver_required_major = fg_requirements.driverVersionRequired.major;
                caps.driver_required_minor = fg_requirements.driverVersionRequired.minor;
                if (fg_requirements.driverVersionDetected.major) {
                    caps.card.driver_major = fg_requirements.driverVersionDetected.major;
                    caps.card.driver_minor = fg_requirements.driverVersionDetected.minor;
                }
            }
            mhw::settings::status::card_capabilities(caps);
        }
        // v0.27.0: DLSS-G loads at every start, so frame generation turns on
        // and off live. A card or system without it runs DLSS and Reflex alone.
        if (fg_support != sl::Result::eOk || fg_requirements_result != sl::Result::eOk) {
            const auto unloaded = sdk.unload_frame_generation();
            log << "{\"event\":\"fg_unloaded\",\"supported\":" << static_cast<int>(fg_support)
                << ",\"requirements\":" << static_cast<int>(fg_requirements_result)
                << ",\"result\":" << static_cast<int>(unloaded) << "}\n"
                << std::flush;
            fg_plugin = false;
            mhw::settings::status::generation(mhw::settings::status::Generation::unavailable, "DLSS-G-support");
        }
    }
    api(sdk.attach(device_pin.Get()), "attach-native-device");
    latency_ping_message.store(sdk.latency_ping_message());
    if (fg_plugin && sdk.frame_generation()) {
        // DLSS-G's own limits (guide 14.1, 14.2), while it is loaded; if it
        // cannot answer before a swap chain exists the session's first state
        // read completes them.
        sl::DLSSGState limits{};
        const auto read = sdk.frame_generation_state(sl::ViewportHandle{0}, limits);
        if (read == sl::Result::eOk && limits.numFramesToGenerateMax) {
            caps.max_multiplier = limits.numFramesToGenerateMax + 1;
            caps.dynamic =
                limits.bIsDynamicMFGSupported == sl::Boolean::eTrue ? caps_ns::Support::yes : caps_ns::Support::no;
        }
        log << "{\"event\":\"fg_limits\",\"result\":" << static_cast<int>(read)
            << ",\"max_generated\":" << limits.numFramesToGenerateMax
            << ",\"dynamic\":" << (limits.bIsDynamicMFGSupported == sl::Boolean::eTrue) << "}\n"
            << std::flush;
        // v0.27.4: frame generation off (or DLSS off) at start: the game's
        // chain is made without DLSS-G (full speed); a switch loads it later.
        if (!config.frame_generation || !config.dlss) {
            const auto unloaded = sdk.unload_frame_generation();
            log << "{\"event\":\"fg_unloaded\",\"reason\":\"off_at_start\",\"result\":" << static_cast<int>(unloaded)
                << "}\n"
                << std::flush;
        }
    }
    // Loaded (also a plugin that could not be unloaded): Streamline owns the
    // swap chain. Usable: requested and supported (loaded now or later).
    fg_loaded = sdk.frame_generation();
    fg_plugin = fg_plugin && sdk.frame_generation_requested();
    // 0.32.0: before DLSS installs (arm_dlss), so its converter keeps the
    // frame sets frame generation reads without copying them.
    mhw::continuous_dlaa::frame_generation_inputs(fg_plugin);
    fg_loaded_state.store(fg_loaded);
    log << "{\"event\":\"fg_loaded\",\"loaded\":" << fg_loaded << ",\"usable\":" << fg_plugin << "}\n" << std::flush;
    if (fg_loaded) api(sdk.frame_generation_off(sl::ViewportHandle{0}, true), "DLSS-G-initially-Off");
    api(sdk.options(sl::ReflexMode::eOff), "Reflex-Off");
    sl::ReflexState state{};
    api(sdk.state(state), "Reflex-state");
    if (!state.lowLatencyAvailable) caps.reflex = caps_ns::Support::no;
    mhw::settings::status::card_capabilities(caps);
    log << "{\"event\":\"card_capabilities\",\"name\":" << quoted(caps.card.name) << ",\"vendor\":" << caps.card.vendor
        << ",\"device\":" << caps.card.device << ",\"memory_mb\":" << caps.card.memory_mb
        << ",\"driver\":" << quoted(caps_ns::version_text(caps.card.driver_major, caps.card.driver_minor))
        << ",\"reflex\":" << static_cast<unsigned>(caps.reflex)
        << ",\"frame_generation\":" << static_cast<unsigned>(caps.frame_generation)
        << ",\"missing\":" << static_cast<unsigned>(caps.missing) << ",\"result\":" << caps.frame_generation_result
        << ",\"driver_required\":"
        << quoted(caps_ns::version_text(caps.driver_required_major, caps.driver_required_minor))
        << ",\"max_multiplier\":" << caps.max_multiplier << ",\"dynamic\":" << static_cast<unsigned>(caps.dynamic)
        << "}\n"
        << std::flush;
    // v0.28.0: the menu's choices follow the card.
    mhw::settings::menu::card_ready();
    device_checked = true;
    log << "{\"event\":\"device_checked\",\"caller_rva\":0,\"loader\":true"
        << ",\"thread\":" << owner << ",\"device\":" << reinterpret_cast<std::uintptr_t>(device_pin.Get())
        << ",\"luid_low\":" << luid.LowPart << ",\"luid_high\":" << luid.HighPart
        << ",\"feature_level\":" << static_cast<unsigned>(level)
        << ",\"low_latency_available\":" << state.lowLatencyAvailable
        << ",\"mode\":0,\"rendering_modified\":false,\"sleep_calls\":0,\"markers\":0}\n"
        << std::flush;
    require(state.lowLatencyAvailable, "Reflex-unavailable");
    auto factory_pin = mhw::reflex::adapter_factory(adapter);
    // The factory is the adapter's parent. (Up to v0.28.6 the D3D12CreateDevice
    // detour also matched it against the renderer receiving the device.)
    log << "{\"event\":\"factory_context\",\"source\":\"loader-adapter-parent\",\"output_owner_matches\":false,"
           "\"factory\":"
        << reinterpret_cast<std::uintptr_t>(factory_pin.Get()) << ",\"backend\":0"
        << ",\"global_backend_published\":false}\n"
        << std::flush;
    presentation = new mhw::reflex::Presentation(sdk);
    // Streamline owns pacing, generated presentation and the host-visible buffers.
    presentation->install(device_pin.Get(), factory_pin.Get(),
                          fg_loaded ? mhw::reflex::PresentationContract::sdk_paced
                                    : mhw::reflex::PresentationContract::synchronous_native);
    // DLSS-G Off before any host window manipulation (guide 12.0).
    presentation->lifecycle(
        {presentation,
         [](void *, unsigned method) noexcept { return mhw::frame_generation::game::lifecycle_guard(method); },
         // The HUD-less image follows the back buffer: SDR or HDR10 (v0.24.8).
         [](void *, std::uint32_t format) noexcept { mhw::frame_generation::game::back_buffer(format); }});
    success = true;
    accepting.store(false);
    completed.store(true);
    log << "{\"event\":\"presentation_installed\",\"factory\":" << reinterpret_cast<std::uintptr_t>(factory_pin.Get())
        << ",\"device_native\":true,\"returned_interfaces_native\":true,\"creation_routes\":4,\"mode\":0}\n"
        << std::flush;
}
// No device is attached after this: admission ends and a DEVICE_CREATED
// callback already inside finishes. False if one is still running.
bool stop_device_attach() noexcept {
    accepting.store(false);
    const auto began = GetTickCount64();
    while (active.load() && GetTickCount64() - began < 30000) Sleep(10);
    return !active.load();
}
// The game's native swap chain, once it exists: its description (the
// presentation layer routes it).
bool describe_swap() {
    const auto began = GetTickCount64();
    while (GetTickCount64() - began < 60000) {
        std::uintptr_t backend{}, swap{}, table{}, qi{}, release{}, desc_method{}, again{};
        if (pb::read(image + layout::image::d3d_render, backend) && backend &&
            pb::read(backend + layout::d3d_render::swap_chain, swap) && swap && pb::read(swap, table) && table &&
            pb::read(table, qi) && pb::read(table + 16, release) && pb::read(table + 96, desc_method)) {
            bool native = true;
            for (const auto method : {qi, release, desc_method}) {
                MEMORY_BASIC_INFORMATION memory{};
                native = VirtualQuery(reinterpret_cast<void *>(method), &memory, sizeof(memory)) == sizeof(memory) &&
                         memory.Type == MEM_IMAGE && memory.AllocationBase == GetModuleHandleW(L"dxgi.dll") && native;
            }
            Microsoft::WRL::ComPtr<IDXGISwapChain> pin;
            DXGI_SWAP_CHAIN_DESC desc{};
            if (native && query_swap(swap, &pin, &desc) && pb::read(backend + layout::d3d_render::swap_chain, again) &&
                again == swap) {
                log << "{\"event\":\"swapchain_snapshot\",\"native_interface\":true,\"swapchain\":" << swap
                    << ",\"width\":" << desc.BufferDesc.Width << ",\"height\":" << desc.BufferDesc.Height
                    << ",\"format\":" << static_cast<unsigned>(desc.BufferDesc.Format)
                    << ",\"buffers\":" << desc.BufferCount << ",\"windowed\":" << (desc.Windowed != FALSE) << "}\n"
                    << std::flush;
                return desc.BufferDesc.Width && desc.BufferDesc.Height && desc.BufferDesc.Format != DXGI_FORMAT_UNKNOWN;
            }
        }
        Sleep(50);
    }
    return false;
}
} // namespace mhw::plugin
