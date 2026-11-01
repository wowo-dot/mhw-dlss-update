#pragma once
#include <unknwn.h>
#include <cstdint>

namespace mhw::reflex {
struct DriverSleepStatus {
    std::int32_t result{-10001}; // local unavailable sentinel, not an NVAPI status
    bool low_latency{};
    // 0.32.0: the driver's own frame pacing (NV_GET_SLEEP_STATUS_PARAMS_V1):
    // its sleep interval (a frame limit; 0 none), the frame generation
    // multiplier and Dynamic control it applies, and Dynamic's frame time
    // target, plus VRR and the control panel's V-Sync.
    std::uint32_t sleep_interval_us{}, dfg_frame_time_target_us{};
    std::uint8_t fg_multiplier{}, dfg_control{};
    bool fullscreen_vrr{}, control_panel_vsync{}, use_game_sleep{};
};
// Read-only NVAPI query on the SDK-attached native device. Requires initialized
// Streamline/NVAPI and caller serialization with options/shutdown. No driver
// load, initialization, option mutation, or frame-path callback here.
DriverSleepStatus driver_sleep_status(IUnknown *native_device) noexcept;
struct DriverOffWait {
    DriverSleepStatus status;
    std::uint32_t queries{};
    std::uint64_t elapsed_ms{};
    bool initial_low_latency{};
};
inline constexpr unsigned driver_off_timeout_ms = 1000;
// Only after callbacks drain and SDK Off succeeds. Status reporting can lag;
// bounded polling observes it without another mode change or frame marker.
DriverOffWait wait_driver_off(IUnknown *native_device) noexcept;
} // namespace mhw::reflex
