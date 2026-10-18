#pragma once
// The plugin's process-wide state and the pieces that share it (internal to
// mhw_dlss.dll). Astera Loader loads the DLL and calls al_plugin_init
// (plugin.cpp), which starts the worker (worker.cpp) and waits until it accepts
// the game's device. The device arrives at AL's DEVICE_CREATED phase
// (device.cpp: Streamline, Reflex, the presentation layer). The worker then
// arms DLSS, Reflex and frame generation once the game qualifies, runs the
// session loop until the game exits, and writes the log (log_events.cpp).
#include "../reflex/streamline.hpp"
#include "../common/json.hpp"
#include "../common/require.hpp"
#include "../reflex/presentation.hpp"
#include "../reflex/live_frames.hpp"
#include "../settings/config.hpp"
#include "../continuous_dlaa/runtime.hpp"
#include "../frame_generation/game_session.hpp"
#include "../../third_party/al/al_plugin.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>

namespace mhw::plugin {
// --- The process --------------------------------------------------------------
inline HMODULE module{};       // this DLL (DllMain)
inline std::uintptr_t image{}; // the game's exe in memory
inline mhw::settings::Config config;
// The plugin's own folder (empty when the DLL sits in the game folder itself):
// the only place inside the game folder the mod reads its runtime from and
// logs to. Lexical, as the configuration's paths are: under MO2 the module
// reports its virtual game-folder path while canonical() resolves to the
// mod's real folder.
inline std::wstring home;
// Folder holding mhw_dlss.ini: a failure before the log opens is recorded there.
inline std::filesystem::path config_folder;
inline std::filesystem::path runtime, output; // the NVIDIA DLLs; the logs
inline std::ofstream log;
// AL's al.game reported this exe's SHA-256: the worker skips re-hashing it.
inline bool exe_hash_verified{};

// --- Start-up handshake (plugin.cpp waits, the worker signals) ----------------
// al_plugin_init starts the worker and waits for this event, set once the
// worker accepts the game's device or failed before that. Init never waits
// for the NGX stage, which follows device creation.
inline HANDLE startup_ready{};
inline std::string startup_failure;
inline void signal_startup() noexcept {
    if (startup_ready) SetEvent(startup_ready);
}

// --- The game's device (device.cpp) -------------------------------------------
// AL's DEVICE_CREATED callbacks in flight (Call); the worker drains them.
inline std::atomic<unsigned> active{};
inline std::atomic<bool> accepting{}, completed{};
struct Call {
    Call() { active.fetch_add(1); }
    ~Call() { active.fetch_sub(1); }
};
inline bool success{};      // the device attached
inline std::string failure; // why it did not
// AL: the swap-chain chains were missing when the game created its swap chain.
inline std::atomic<bool> swapchain_chains_missing{};
// SDK calls from the device callback and the worker are serialized.
inline std::mutex sdk_mutex;
// These are process-lived. A failed shutdown retains the native device pin;
// no borrowed object is destroyed while the SDK could still refer to it.
// Explicit worker cleanup only. Never call vendor shutdown from CRT/DllMain
// teardown.
inline auto &device_pin = *new Microsoft::WRL::ComPtr<ID3D12Device>;
inline auto &sdk = *new mhw::reflex::Streamline;
inline mhw::reflex::Presentation *presentation{};
inline mhw::reflex::LiveFrames *live_frames{};

// --- Reflex (reflex_control.cpp) ----------------------------------------------
constexpr auto capture_mode = sl::ReflexMode::eLowLatency;
// The player's Reflex mode (reflex_mode.hpp): requested by the ini and the
// menu, applied by the worker.
inline std::atomic<unsigned> reflex_requested{static_cast<unsigned>(capture_mode)}, reflex_applied{3};
// The worker set a Reflex mode; shutdown restores Off (close_sdk).
inline bool mode_change_attempted{}, mode_restored{};

// --- Frame generation (generation.cpp) ----------------------------------------
// fg_plugin: DLSS-G requested at start, then usable (requested and supported;
// the device's attach decides). fg_loaded: loaded now (Streamline paces the
// swap chain, Reflex stays on); v0.27.4 loads and unloads it with the swap
// chain (fg-live-toggle.md), so the worker updates it after each switch.
inline bool fg_plugin = true, fg_loaded = false;
inline unsigned fg_multiplier = 2;
inline std::atomic<bool> fg_loaded_state{};
// The ini's frame generation mode and dynamic target (game_session.hpp).
inline mhw::frame_generation::game::Mode fg_mode{};
inline unsigned fg_target{};
// Frame generation was configured at arming and has not stopped since: it is
// armed whenever DLSS runs with DLSS-G loaded (its inputs are DLSS's).
inline bool fg_ready{};
// v0.28.6 (tifa-6): a presentation fault with routing kept turns frame
// generation off for the session (DLSS-G unloaded by a switch); the worker
// goes on (DLSS, Reflex, the menu). The faults already handled.
inline bool generation_fault{};
inline std::uint64_t presentation_faults{};
// The state a switch last failed to reach (refused or failed: 0 off, 1 on);
// not asked again until the wanted state changes or the game changes its
// fullscreen state (log_lifecycle).
inline int switch_unreached = -1;
// The game's chain can be made again for a switch (game_chain_qualified).
inline bool game_chain_ready{};

// --- Helpers --------------------------------------------------------------------
inline void api(sl::Result result, const char *reason) {
    if (result != sl::Result::eOk)
        throw std::runtime_error(std::string(reason) + ":" + std::to_string(static_cast<unsigned>(result)));
}
// The plugin log and status in AL (plugin.cpp).
void loader_log(al_log_level_t level, const std::string &text) noexcept;

// device.cpp
void attach_device(IUnknown *adapter, D3D_FEATURE_LEVEL level, IUnknown *created_device);
void failed(const std::exception &error) noexcept;
bool stop_device_attach() noexcept;
bool describe_swap();
bool close_sdk() noexcept;

// reflex_control.cpp
bool driver_mode(const char *phase, bool expected, bool enforce = true);
void mode_options(const char *phase, sl::ReflexMode mode, sl::Result result);
const char *reflex_mode_name(unsigned mode) noexcept;
bool apply_reflex_mode(bool loaded);
void reflex_unconfirmed(const char *phase);

// generation.cpp
bool game_chain_qualified() noexcept;
std::uintptr_t game_waitable_slot() noexcept;
bool game_resize_request() noexcept;
// A switch in progress: asked at, game requests made, the presentation's
// outcome count before it, and what was asked.
struct GenerationSwitch {
    std::uint64_t asked_at{};
    unsigned tries{};
    std::uint64_t outcomes{};
    bool want{}, active{};
};
void sync_generation(GenerationSwitch &g);
void presentation_fault(const mhw::reflex::PresentationStats &v);
bool configure_generation_scene();

// dlss_control.cpp
// A live preset change the game did not render through (paused, minimized):
// retried every 2 s, up to 15 times, unless the menu asks for another mode.
struct DlssRetry {
    std::uint64_t at{};
    mhw::continuous_dlaa::ModeRequest request{};
    unsigned count{};
};
// The live aspect change: tried every 500 ms while not ready (the game
// mid-resize, a drain), up to 40 times.
struct SceneRetry {
    std::uint64_t at{};
    unsigned count{};
    std::string reason;
};
void dlss_status();
void change_dlss_mode(const mhw::continuous_dlaa::ModeRequest &wanted, DlssRetry &retry);
void change_dlss_scene(SceneRetry &retry);

// log_events.cpp
// crash_log::code_site as a string.
std::string code_site(std::uintptr_t address);
int fg_buffers_json(char *out, int size) noexcept;
void log_fg_buffers(std::ostream &out, const char *phase);
void log_lifecycle(std::ostream &out);
void log_frames(std::ostream &out, const char *phase);
void write_reflex_fault(std::ostream &out, const mhw::reflex::LiveFrameStats &frames);
void log_latency(std::ostream &out);
void poll_perf(std::ostream &out, const std::filesystem::path &folder, const std::string &stem, std::uint64_t creation,
               unsigned &requests);

// worker.cpp
DWORD WINAPI worker(void *) noexcept;
} // namespace mhw::plugin
