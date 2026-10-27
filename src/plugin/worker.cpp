// The worker thread (started by al_plugin_init, plugin.cpp): reads mhw_dlss.ini
// and opens the log, accepts the game's device, arms DLSS, Reflex and frame
// generation once the game qualifies, then runs the session loop until the
// game exits: the menu's live changes, the game's aspect changes, the frame
// generation switch, and a heartbeat in the log every 20 s.
#include "plugin.hpp"
#include "../game_memory/layout.hpp"
#include "../settings/capabilities.hpp"
#include "../settings/status.hpp"
#include "../settings/version.hpp"
#include "../common/log_path.hpp"
#include "../continuous_dlaa/texture_bias.hpp"
#include "../game_memory/readable_region.hpp"
#include "../ngx_startup/startup.hpp"
#include "../frame_generation/crash_log.hpp"
#include "../frame_generation/display_refresh.hpp"
#include "../perf/frame_stats.hpp"
#include "../perf/chaos.hpp"
#include "../perf/gpu_timing.hpp"
#include "../perf/hook_timing.hpp"
#include "../perf/sampler.hpp"
#include "../perf/toggles.hpp"
#include "../dlss_sdk/options.hpp"
#include "../projection_binding/memory.hpp"
#include "../render_resolution/runtime.hpp"
#include "../sharpen/sharpen.hpp"
#include "../reflex/temporal_coexistence.hpp"
#include <iterator>
#include <sstream>
#include <string_view>

namespace mhw::plugin {
namespace {
namespace pb = mhw::projection_binding;
const char *stop_reason = "not_armed"; // Why the session loop ended.
std::filesystem::path stop_file, arm_file;

// The log of this launch: reflex-frames-<pid>-<creation>.jsonl.
struct Session {
    std::string name;
    std::uint64_t creation{};
    bool crash_log{};
    unsigned perf_requests{};
};

// The worker's exit code when this copy must not run, or -1.
int claim_process() noexcept {
    HMODULE pin{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(&worker), &pin)) {
        startup_failure = "module-pin";
        signal_startup();
        return 1;
    }
    { // One active copy per process: a second one (e.g. loaded by AL and also
        // force-loaded by MO2) stays inert. The handle lives as long as the process.
        const auto name = L"Local\\mhw_dlss.instance." + std::to_wstring(GetCurrentProcessId());
        const HANDLE instance = CreateMutexW(nullptr, FALSE, name.c_str());
        if (!instance || GetLastError() == ERROR_ALREADY_EXISTS) {
            startup_failure = "another mhw_dlss copy is already active in this process";
            signal_startup();
            return 0;
        }
    }
    return -1;
}

// mhw_dlss.ini beside the DLL: the folders, the starting modes and switches.
void read_configuration(const std::filesystem::path &dll, const std::filesystem::path &host) {
    // Plugin layout: mhw_dlss.dll, mhw_dlss.ini, the NVIDIA runtime and
    // logs in one folder, injected by AL (MO2 force-load in development).
    // With the ini beside the DLL in a folder of its own (not the game
    // folder itself), that folder is the one place inside the game
    // folder the mod may read its runtime from and log to.
    const auto base = dll.parent_path().lexically_normal();
    std::error_code missing;
    const auto game_folder = host.parent_path().lexically_normal();
    if (std::filesystem::is_regular_file(base / L"mhw_dlss.ini", missing) &&
        _wcsicmp(base.c_str(), game_folder.c_str()))
        home = base.wstring();
    config_folder = base;
    std::ifstream in(base / L"mhw_dlss.ini", std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    config = mhw::settings::parse_config(text, base);
    output = config.log_dir;
    std::filesystem::create_directories(output);
    runtime = config.runtime_dir;
    // DLSS-G loads unless frame_generation_plugin = off; the frame
    // generation setting only turns it on or off (live, in the menu).
    fg_plugin = config.generation_plugin;
    fg_multiplier = config.multiplier;
    mhw::frame_generation::game::enable(config.frame_generation);
    fg_mode = config.dynamic           ? mhw::frame_generation::game::Mode::dynamic
              : config.generation_auto ? mhw::frame_generation::game::Mode::automatic
                                       : mhw::frame_generation::game::Mode::fixed;
    fg_target = config.dynamic_target;
    mhw::settings::status::generation(fg_plugin ? mhw::settings::status::Generation::starting
                                                : mhw::settings::status::Generation::off);
    mhw::continuous_dlaa::reconstruction_mode(config.dlss_mode);
    mhw::continuous_dlaa::start_off(!config.dlss);
    mhw::settings::status::dlss_mode(mhw::settings::dlss_mode_name(config.dlss_mode));
    // profile = on: the stack sampler too (perf::sample_profile; started
    // with the frame windows).
    mhw::perf::toggles.store(mhw::perf::all_toggles | (config.profile ? mhw::perf::sample_profile : 0U));
    // 0.31.1 diagnostics, before the game's NGX start-up and the arm.
    mhw::perf::gpu_timing::enabled = config.gpu_timing;
    mhw::perf::chaos::enabled = config.chaos;
    mhw::dlss_sdk::auto_exposure = config.auto_exposure;
    mhw::dlss_sdk::render_preset = mhw::settings::ngx_preset(config.dlss_cnn, config.dlss_preset);
    mhw::dlss_sdk::jitter_ab = config.jitter_ab;
    mhw::sharpen::percent(config.sharpness); // 0.32.0: the menu changes it live
    reflex_requested.store(config.reflex == mhw::settings::ReflexMode::off     ? 0U
                           : config.reflex == mhw::settings::ReflexMode::boost ? 2U
                                                                               : 1U);
    mhw::settings::status::reflex_mode(mhw::settings::reflex_name(config.reflex));
}

// Nothing the mod writes or reads lies in the game folder, except its own
// plugin folder.
void check_folders(const std::filesystem::path &host) {
    // The ini's folders are lexical (config.hpp), so the game folder is too
    // (v0.28.5): under MO2 its canonical form can be overwrite's folder.
    const auto game_root = host.parent_path().lexically_normal().wstring();
    const auto under = [](const std::wstring &text, const std::wstring &root) {
        return text.size() >= root.size() &&
               CompareStringOrdinal(text.data(), static_cast<int>(root.size()), root.data(),
                                    static_cast<int>(root.size()), TRUE) == CSTR_EQUAL &&
               (text.size() == root.size() || text[root.size()] == L'\\' || text[root.size()] == L'/');
    };
    for (const auto &path : {output, runtime}) {
        const auto text = path.wstring();
        // The plugin folder may sit inside the game folder (AL, MO2 virtual
        // paths); nothing else there is used.
        if (!home.empty() && under(path.lexically_normal().wstring(), home)) continue;
        require(!under(text, game_root), "configuration-in-game-directory");
    }
}

// The game's identity, then this launch's log: start and config.
Session open_log(const std::filesystem::path &dll) {
    Session session;
    image = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::string reason;
    require(pb::host_identity(image, reason, !exe_hash_verified), "retail-host-identity");
    FILETIME ct{}, et{}, kt{}, ut{};
    require(GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut) != FALSE, "process-identity");
    session.creation = (static_cast<std::uint64_t>(ct.dwHighDateTime) << 32) | ct.dwLowDateTime;
    session.name =
        "reflex-frames-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(session.creation) + ".jsonl";
    const auto &name = session.name;
    arm_file = output / (name + ".arm");
    require(!std::filesystem::exists(arm_file), "preserve-arm-file");
    stop_file = output / (name + ".stop");
    require(!std::filesystem::exists(stop_file), "preserve-stop-file");
    HANDLE file = CreateFileW((output / name).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "preserve-existing-log");
    CloseHandle(file);
    log.open(output / name, std::ios::binary | std::ios::app);
    require(bool(log), "log");
    log << std::boolalpha;
    session.crash_log = mhw::frame_generation::crash_log::install(output / (name + ".crash.jsonl"), fg_buffers_json);
    log << "{\"event\":\"start\",\"schema\":1,\"version\":\"" MHW_VERSION
           "\",\"experiment\":\"frame-generation-game\",\"target_mode\":1,\"fg_plugin_loaded\":"
        << fg_plugin << ",\"fg_multiplier\":" << fg_multiplier
        << ",\"fg_enabled\":false,\"pid\":" << GetCurrentProcessId() << ",\"creation\":" << session.creation
        << ",\"host_sha256\":" << quoted(pb::host_sha)
        << ",\"host_version\":\"0.0.0.0\",\"dll_sha256\":" << quoted(pb::file_sha256(dll.wstring()))
        << ",\"sdk\":\"2.12.0\",\"graphics_api\":\"D3D12\",\"mode\":0,\"sleep\":false,\"markers\":false"
        << ",\"upscaling\":true,\"temporal_version\":" << quoted(mhw::reflex::temporal::version()) << "}\n"
        << std::flush;
    log << "{\"event\":\"alpha_config\",\"dlss_mode\":\""
        << mhw::settings::dlss_setting_value(config.dlss, config.dlss_mode)
        << "\",\"frame_generation\":" << config.frame_generation << ",\"multiplier\":" << config.multiplier
        << ",\"dynamic\":" << config.dynamic
        << ",\"dynamic_target\":" << quoted(mhw::settings::dynamic_target_value(config.dynamic_target))
        << ",\"frame_generation_auto\":" << config.generation_auto
        << ",\"frame_generation_plugin\":" << quoted(config.generation_plugin ? "auto" : "off")
        << ",\"profile\":" << config.profile << ",\"perf_ab\":" << config.perf_ab
        << ",\"hook_timing\":" << config.hook_timing << ",\"texture_bias\":" << config.texture_bias
        << ",\"texture_bias_ab\":" << config.texture_bias_ab << ",\"gpu_timing\":" << config.gpu_timing
        << ",\"auto_exposure\":" << config.auto_exposure << ",\"chaos\":" << config.chaos
        << ",\"dlss_model\":" << quoted(mhw::settings::dlss_model_value(config.dlss_cnn))
        << ",\"dlss_preset\":" << quoted(mhw::settings::dlss_preset_value(config.dlss_preset))
        << ",\"sharpness\":" << config.sharpness << ",\"jitter_ab\":" << config.jitter_ab
        << ",\"waitable_ab\":" << config.waitable_ab
        << ",\"reflex\":" << quoted(mhw::settings::reflex_value(config.reflex))
        << ",\"runtime_dir\":" << quoted(mhw::log_path(config.runtime_dir))
        << ",\"log_dir\":" << quoted(mhw::log_path(config.log_dir)) << ",\"warnings\":[";
    for (unsigned i = 0; i < config.warnings.size(); ++i) log << (i ? "," : "") << quoted(config.warnings[i]);
    log << "]}\n" << std::flush;
    return session;
}

// Ready for the device: al_plugin_init returns, and the NGX stage is prepared.
void accept_device() {
    // Refuse a late load; public API hooks are installed before first use.
    std::uintptr_t backend{};
    require(pb::read(image + layout::image::d3d_render, backend), "backend-slot");
    if (backend) {
        std::uintptr_t device{};
        require(pb::read(backend + layout::d3d_render::device, device) && !device, "main-device-already-created");
    }
    // The device arrives through AL's DEVICE_CREATED (on_phase).
    log << "{\"event\":\"prepared\",\"loader\":true,\"sdk_initialized\":false}\n" << std::flush;
    accepting.store(true);
    signal_startup();
    // The device callback runs on the game's thread while this worker admits
    // the later NGX initializer.
    mhw::reflex::temporal::prepare(module, output, image, runtime);
    {
        std::lock_guard lock(sdk_mutex);
        mhw::reflex::temporal::receipt(log, "prepared", device_pin.Get());
    }
    {
        std::lock_guard lock(sdk_mutex);
        log << "{\"event\":\"ready\",\"startup_completed\":" << completed.load() << "}\n" << std::flush;
    }
}

// The device attached and the game's swap chain exists, routed.
bool wait_for_swap_chain() {
    const auto began = GetTickCount64();
    while (!completed.load() && GetTickCount64() - began < 180000) Sleep(20);
    const bool attach_stopped = stop_device_attach();
    const bool swap_observed = success && attach_stopped && describe_swap() && !swapchain_chains_missing.load();
    if (!swap_observed)
        mhw::settings::status::worker_failed(failure.empty() ? std::string_view("presentation")
                                                             : std::string_view(failure));
    return swap_observed;
}

// The texture mip bias (texture_bias = on): installed as soon as the game has
// built its sampler banks, from the frames' start on, so that the materials
// loading after it take the bias. Tried until the banks exist (at most 2000
// tries), then logged once.
bool texture_bias_settled{};
unsigned texture_bias_tries{};
void try_texture_bias() {
    namespace tb = mhw::continuous_dlaa::texture_bias;
    if (texture_bias_settled || !config.texture_bias) return;
    const auto result = tb::install(image);
    if (result == tb::Install::retry && ++texture_bias_tries < 2000) return;
    texture_bias_settled = true;
    log << "{\"event\":\"texture_bias_install\",\"installed\":" << (result == tb::Install::installed)
        << ",\"tries\":" << texture_bias_tries + 1 << ",\"refusal\":" << quoted(tb::refusal()) << "}\n"
        << std::flush;
}
// Each session loop: the materials' bank follows what DLSS draws (with
// texture_bias_ab, the game's own every other 10 s).
void follow_texture_bias(bool drawing) {
    namespace tb = mhw::continuous_dlaa::texture_bias;
    if (!tb::installed()) return;
    auto scale = drawing ? mhw::continuous_dlaa::texture_scale() : 0.F;
    if (config.texture_bias_ab && (GetTickCount64() / 10000) % 2) scale = 0.F;
    tb::follow(scale, log);
}

// Reflex Off is confirmed and the diagnostics start; the time waiting for
// the game began.
std::uint64_t start_frames(const Session &session) {
    require(driver_mode("startup_off", false), "driver-startup-Off");
    log << "{\"event\":\"frames_ready\",\"mode\":0,\"target_mode\":1,\"sleep_calls\":0,\"markers\":0,\"armed\":false}\n"
        << std::flush;
    log << "{\"event\":\"fg_crash_log\",\"installed\":" << session.crash_log << "}\n";
    log_fg_buffers(log, "frames_ready");
    const auto wait_start = GetTickCount64();
    log << "{\"event\":\"perf_toggles\",\"request\":0,\"mask\":" << mhw::perf::toggles.load()
        << ",\"previous\":0,\"accepted\":true}\n"
        << std::flush;
    mhw::perf::host_frames.take();
    if (config.profile) mhw::perf::sampler::start();
    // Hot game-memory regions stay validated off the render thread.
    mhw::game_memory::start_region_refresher();
    if (config.hook_timing) mhw::perf::hook_timing::enable();
    try_texture_bias();
    return wait_start;
}

// Arm once the swap chain has been unchanged for 3 s (startup creates it and
// resizes it several times) and the game qualifies; until then the game
// renders natively. The first and the latest reason the game was not ready
// yet are logged when it arms.
bool wait_until_qualified(std::uint64_t wait_start) {
    bool arm_now = false;
    auto stats = presentation->snapshot();
    auto stable_since = GetTickCount64();
    unsigned attempts{};
    std::string not_ready, first_reason;
    bool waiting_logged = false;
    while (stats.routing && !stats.faults) {
        Sleep(250);
        const auto now_stats = presentation->snapshot();
        if (now_stats.swapchains != stats.swapchains || now_stats.resizes != stats.resizes)
            stable_since = GetTickCount64();
        stats = now_stats;
        log_lifecycle(log);
        try_texture_bias();
        if (GetTickCount64() - stable_since < 3000) continue;
        not_ready.clear();
        mhw::perf::chaos::delay("ready");
        if (mhw::reflex::temporal::ready(not_ready)) {
            arm_now = true;
            break;
        }
        if (!attempts++) first_reason = not_ready;
        // A game that never qualifies would otherwise leave no reason
        // in the log until the session ends (game-63).
        if (!waiting_logged && GetTickCount64() - wait_start >= 15000) {
            waiting_logged = true;
            log << "{\"event\":\"alpha_waiting\",\"attempts\":" << attempts
                << ",\"waited_ms\":" << GetTickCount64() - wait_start << ",\"reason\":" << quoted(not_ready) << "}\n"
                << std::flush;
        }
    }
    log << "{\"event\":\"alpha_arm\",\"ready\":" << arm_now << ",\"attempts\":" << attempts
        << ",\"waited_ms\":" << GetTickCount64() - wait_start << ",\"first_reason\":" << quoted(first_reason)
        << ",\"last_reason\":" << quoted(not_ready) << ",\"routing\":" << stats.routing
        << ",\"faults\":" << stats.faults << ",\"fault_site\":" << stats.fault_site
        << ",\"fault_hr\":" << stats.fault_hr << "}\n"
        << std::flush;
    if (!arm_now)
        mhw::settings::status::worker_failed(not_ready.empty() ? std::string_view("presentation")
                                                               : std::string_view(not_ready));
    return arm_now;
}

// DLSS: the NGX stage's temporal gates qualify before the three Reflex frame
// gates are patched; its own observer continues to own submission.
void arm_dlss() {
    mhw::reflex::temporal::arm(device_pin.Get());
    mhw::reflex::temporal::receipt(log, "armed", device_pin.Get());
    unsigned render_w{}, render_h{}, scene_w{}, scene_h{}, window_w{}, window_h{};
    mhw::reflex::temporal::render_extent(render_w, render_h);
    mhw::reflex::temporal::scene_extent(scene_w, scene_h, window_w, window_h);
    mhw::settings::status::dlss_sizes(render_w, render_h, scene_w, scene_h);
    mhw::settings::status::dlss_mode(mhw::settings::dlss_mode_name(mhw::continuous_dlaa::reconstruction_mode()));
    mhw::settings::status::dlss(mhw::continuous_dlaa::dlss_off() ? mhw::settings::status::Stage::player_off
                                                                 : mhw::settings::status::Stage::running);
    // NGX startup and the DLSS install succeeded (capabilities.hpp).
    mhw::settings::status::dlss_available(true);
    { // 0.32.0: the DLSS file NGX runs (the shipped one, or NVIDIA's under the NVIDIA App's override).
        const auto provider = mhw::ngx_startup::active_provider();
        log << "{\"event\":\"dlss_file\",\"version\":" << quoted(provider.version)
            << ",\"source\":" << quoted(std::string(provider.source)) << "}\n"
            << std::flush;
        mhw::settings::status::dlss_file(provider.version, provider.source);
        mhw::settings::status::dlss_model(mhw::settings::status::model_name(mhw::continuous_dlaa::preset_in_use()));
    }
}

// Reflex in the player's mode (Off only without DLSS-G), then the frame
// boundary hooks.
void arm_reflex() {
    auto arm_mode = static_cast<sl::ReflexMode>(reflex_requested.load());
    if (arm_mode == sl::ReflexMode::eOff && fg_loaded) arm_mode = capture_mode;
    {
        std::lock_guard lock(sdk_mutex);
        mode_change_attempted = true;
        const auto result = sdk.options(arm_mode);
        mode_options("activate_on", arm_mode, result);
        api(result, "Reflex-On");
        reflex_applied.store(static_cast<unsigned>(arm_mode));
        reflex_requested.store(static_cast<unsigned>(arm_mode));
        // Owned runs report Off immediately and after frame 8, On
        // by frame 32. Keep this observation, require the mode at
        // the first five-second checkpoint and again after drain.
        require(driver_mode("requested_on", arm_mode != sl::ReflexMode::eOff, false), "driver-On-query");
    }
    live_frames = new mhw::reflex::LiveFrames(sdk);
    presentation->observe(live_frames->observer());
    live_frames->install(image, reinterpret_cast<std::uintptr_t>(device_pin.Get()), log);
    live_frames->describe(log, static_cast<unsigned>(arm_mode));
    log << "{\"event\":\"frames_armed\",\"mode\":" << static_cast<unsigned>(arm_mode)
        << ",\"sleep_calls_enabled\":true,\"markers_enabled\":true}\n"
        << std::flush;
    mhw::settings::status::reflex(mhw::settings::status::Stage::running);
}

// Frame generation on the game's chain, for the scene rectangle.
void arm_frame_generation() {
    // With MHW's aspect-ratio option the scene is letterboxed or
    // pillarboxed in the back buffer, centred (checked against the
    // screen write's iRegion and viewport in the FG status).
    unsigned scene_width{}, scene_height{}, window_width{}, window_height{};
    const bool scene_known =
        mhw::reflex::temporal::scene_extent(scene_width, scene_height, window_width, window_height);
    if (!fg_plugin) {
        // frame_generation_plugin = off: DLSS and Reflex only; the DLSS-G plugin is not loaded.
        log << "{\"event\":\"fg_unsupported\",\"reason\":\"plugin_off\"}\n" << std::flush;
    } else if (const auto chain = presentation->snapshot();
               !scene_known || !scene_width || !scene_height || scene_width > chain.width ||
               scene_height > chain.height || window_width != chain.width || window_height != chain.height) {
        log << "{\"event\":\"fg_unsupported\",\"reason\":\"scene_rect\",\"scene_known\":" << scene_known
            << ",\"scene\":[" << scene_width << ',' << scene_height << "],\"window\":[" << window_width << ','
            << window_height << "],\"chain\":[" << chain.width << ',' << chain.height << "]}\n"
            << std::flush;
        mhw::settings::status::generation(mhw::settings::status::Generation::unavailable, "scene_rect");
    } else {
        const mhw::frame_generation::game::SceneRect scene{
            (chain.width - scene_width) / 2, (chain.height - scene_height) / 2, scene_width, scene_height};
        mhw::frame_generation::game::back_buffer(chain.format);
        mhw::frame_generation::game::configure(sdk, chain.width, chain.height, scene);
        mhw::frame_generation::game::multiplier(fg_multiplier);
        mhw::frame_generation::game::mode(fg_mode, fg_target);
        // Armed whenever DLSS runs with DLSS-G loaded (fg_ready); v0.27.4
        // loads and unloads DLSS-G with the swap chain (sync_generation).
        fg_ready = true;
        game_chain_ready = game_chain_qualified();
        if (game_chain_ready) presentation->waitable_slot(game_waitable_slot());
        presentation->waitable_ab(config.waitable_ab); // 0.32.0 diagnostic
        log << "{\"event\":\"fg_switch_ready\",\"game_chain\":" << game_chain_ready
            << ",\"identity\":" << presentation->snapshot().identity << "}\n"
            << std::flush;
        if (!mhw::continuous_dlaa::dlss_off() && fg_loaded) mhw::frame_generation::game::arm(true);
        // DLSS-G's state is read on the present thread (Streamline
        // requires it there) and logged in fg_status ("state").
        log << "{\"event\":\"fg_armed\",\"width\":" << chain.width << ",\"height\":" << chain.height << ",\"scene\":["
            << scene.x << ',' << scene.y << ',' << scene.width << ',' << scene.height << ']'
            << ",\"display_hz\":" << mhw::frame_generation::display_refresh_hz() << "}\n"
            << std::flush;
        mhw::settings::status::generation(mhw::settings::status::Generation::armed);
    }
}

// The session loop, every 100 ms until the game exits or a fault ends it
// (stop_reason).
void run_session() {
    const auto started = GetTickCount64();
    auto last = started;
    // v0.24.7: Reflex is confirmed running at the first heartbeat
    // with frames. The mod can arm before the game's first frame,
    // and a first launch after a driver install can compile
    // shaders for longer than a heartbeat (game-105: no frame in
    // the first 20 s, the check failed and DLSS stayed off).
    constexpr std::uint64_t reflex_check_frames = 60;
    bool reflex_confirmed = false;
    // After a live mode change: the sleep count at which the driver is
    // asked whether it took the change (it applies at the next sleep).
    std::uint64_t live_check_sleeps{};
    DlssRetry dlss_retry;
    SceneRetry scene_retry;
    GenerationSwitch generation_switch;
    unsigned learned_multiplier{};
    std::uint64_t chain_retry_at{}; // no real chain: the game's resize asked every 2 s
    // perf_ab (diagnostic, FG off): DLSS paused and running in
    // turn, 30 s each, logged with the time to the plugin log.
    std::uint64_t perf_ab_next = config.perf_ab && !fg_loaded ? GetTickCount64() + 30000 : 0;
    bool perf_ab_paused = false;
    stop_reason = "duration";
    // 20 s, so a session closed soon after an event still logs it.
    constexpr std::uint64_t heartbeat_ms = 20000;
    for (;;) {
        const auto frames = live_frames->snapshot();
        const auto stats = presentation->snapshot();
        log_lifecycle(log);
        if (frames.closed && !frames.faults) {
            // The game destroyed its window (its exit); logged at once, the process ends soon.
            log << "{\"event\":\"game_exit\",\"completed\":" << frames.completed << "}\n" << std::flush;
            // v0.24.7: a last summary, so a session shorter than
            // one heartbeat still records its counters. Built in
            // memory and written at once: the process may end.
            {
                std::ostringstream summary;
                summary << std::boolalpha; // as the log's own lines
                mhw::reflex::temporal::sample();
                summary << "{\"event\":\"fg_status\",\"display_hz\":" << mhw::frame_generation::display_refresh_hz()
                        << ",\"session\":";
                mhw::frame_generation::game::write(summary);
                summary << "}\n";
                summary << "{\"event\":\"dlss_summary\",\"runtime\":";
                mhw::reflex::temporal::summary(summary);
                summary << "}\n";
                log << summary.str() << std::flush;
            }
            stop_reason = "game_exit";
            break;
        }
        if (!frames.running || frames.faults) {
            // Logged at once: the process may end before fg_stopped.
            log << "{\"event\":\"reflex_fault\",\"running\":" << frames.running << ",\"completed\":" << frames.completed
                << ',';
            write_reflex_fault(log, frames);
            log << "}\n" << std::flush;
            stop_reason = "reflex_frames";
            break;
        }
        if (!stats.routing) {
            stop_reason = "presentation";
            break;
        }
        // v0.28.6 (tifa-6): a fault that kept routing (the identity
        // chain, SDK pacing) turns frame generation off for the
        // session and is logged; DLSS, Reflex and the menu go on.
        if (stats.faults != presentation_faults) presentation_fault(stats);
        // No real chain behind the identity chain (both switch attempts and
        // the last resort failed): its calls are answered meanwhile, and
        // each game resize tries the last resort again.
        if (stats.identity && !stats.real_chain && GetTickCount64() >= chain_retry_at) {
            game_resize_request();
            chain_retry_at = GetTickCount64() + 2000;
        }
        if (!frames.running || frames.faults || !stats.routing) break;
        if (reflex_requested.load() != reflex_applied.load()) {
            std::lock_guard lock(sdk_mutex);
            if (apply_reflex_mode(fg_loaded)) live_check_sleeps = live_frames->snapshot().sleeps + 2;
        } else if (live_check_sleeps && frames.sleeps >= live_check_sleeps) {
            // As at the first heartbeat: low latency on for On and Boost, off for Off.
            live_check_sleeps = 0;
            if (!driver_mode("live", reflex_applied.load() != 0)) reflex_unconfirmed("live");
        }
        mhw::render_resolution::write_captures(log);
        if (perf_ab_next && GetTickCount64() >= perf_ab_next) {
            perf_ab_paused = !perf_ab_paused;
            perf_ab_next += 30000;
            const bool applied = mhw::continuous_dlaa::diagnostic_pause(perf_ab_paused);
            loader_log(AL_LOG_INFO, std::string("perf: dlss = ") + (perf_ab_paused ? "paused" : "running") +
                                        (applied ? " (applied now)" : " (refused)"));
            log << "{\"event\":\"perf_ab\",\"paused\":" << perf_ab_paused << ",\"applied\":" << applied << "}\n"
                << std::flush;
            if (!applied) perf_ab_next = 0;
        }
        sync_generation(generation_switch);
        { // DLSS-G's limits once its state was read (capabilities.hpp).
            const auto session = mhw::frame_generation::game::status();
            if (session.maximum && session.maximum + 1 != learned_multiplier) {
                learned_multiplier = session.maximum + 1;
                const auto dynamic = session.dynamic_support == 2   ? mhw::settings::capabilities::Support::yes
                                     : session.dynamic_support == 1 ? mhw::settings::capabilities::Support::no
                                                                    : mhw::settings::capabilities::Support::unknown;
                mhw::settings::status::generation_limits(learned_multiplier, dynamic);
                log << "{\"event\":\"fg_limits\",\"source\":\"session\",\"max_multiplier\":" << learned_multiplier
                    << ",\"dynamic\":" << session.dynamic_support << "}\n"
                    << std::flush;
            }
        }
        // The game's own aspect switch (render_resolution yielded). While the
        // player has DLSS off it waits for the way back (change_dlss_mode).
        if (mhw::render_resolution::scene_changed() && GetTickCount64() >= scene_retry.at &&
            !mhw::continuous_dlaa::dlss_off())
            change_dlss_scene(scene_retry);
        { // A live preset change or Off from the menu, or its retry.
            mhw::continuous_dlaa::ModeRequest wanted{};
            if (mhw::continuous_dlaa::requested_mode(wanted)) {
                dlss_retry = {};
                change_dlss_mode(wanted, dlss_retry);
            } else if (dlss_retry.at && GetTickCount64() >= dlss_retry.at) {
                dlss_retry.at = 0;
                change_dlss_mode(dlss_retry.request, dlss_retry);
            }
        }
        try_texture_bias();
        follow_texture_bias(true);
        if (!mhw::reflex::temporal::healthy()) {
            stop_reason = "temporal_unhealthy";
            mhw::reflex::temporal::fault_snapshot();
            break;
        }
        const auto now = GetTickCount64();
        if (now - last >= heartbeat_ms) {
            mhw::reflex::temporal::sample();
            log << "{\"event\":\"fg_status\",\"display_hz\":" << mhw::frame_generation::display_refresh_hz()
                << ",\"session\":";
            mhw::frame_generation::game::write(log);
            log << "}\n" << std::flush;
            log << "{\"event\":\"dlss_summary\",\"runtime\":";
            mhw::reflex::temporal::summary(log);
            log << "}\n" << std::flush;
            mhw::perf::hook_timing::write(log, frames.completed);
            mhw::perf::gpu_timing::write(log);
            log_fg_buffers(log, "status");
            log_frames(log, "armed");
            log_latency(log);
            mhw::perf::sampler::write(log);
            if (!reflex_confirmed && frames.completed >= reflex_check_frames) {
                if (!driver_mode("running_on", reflex_applied.load() != 0)) reflex_unconfirmed("running_on");
                reflex_confirmed = true;
            }
            LARGE_INTEGER clock_qpc{}, clock_frequency{};
            QueryPerformanceCounter(&clock_qpc);
            QueryPerformanceFrequency(&clock_frequency);
            const auto clock_tick = GetTickCount64();
            log << "{\"event\":\"frame_heartbeat\",\"completed\":" << frames.completed
                << ",\"tokens\":" << frames.tokens << ",\"sleeps\":" << frames.sleeps
                << ",\"markers\":" << frames.markers << ",\"faults\":" << frames.faults << ",\"owner\":"
                << frames.owner
                // 0.32.0: NVIDIA's PC Latency pings: received, sent, refused.
                << ",\"latency_pings\":{\"subscribed\":" << latency_ping_subscribed
                << ",\"message\":" << latency_ping_message.load() << ",\"seen\":" << latency_pings_seen.load()
                << ",\"sent\":" << frames.pings << ",\"refused\":" << frames.ping_failures << '}'
                << ",\"presents\":" << stats.presents << ",\"native_presents\":"
                << stats.native_presents
                // 0.32.0 diagnostic (waitable_ab): per phase, presents timed and their microseconds.
                << ",\"waitable_ab\":{\"phase\":" << stats.ab_phase << ",\"flips\":" << stats.ab_flips << ",\"wait\":["
                << stats.ab_frames[0] << ',' << stats.ab_us[0] << "],\"free\":[" << stats.ab_frames[1] << ','
                << stats.ab_us[1] << "],\"present\":[[" << stats.ab_present_calls[0] << ',' << stats.ab_present_us[0]
                << "],[" << stats.ab_present_calls[1] << ',' << stats.ab_present_us[1]
                << "]]}"
                // The phase is (tick_ms / 10000) % 2: this pair lines PresentMon's
                // QPC times (--qpc_time) up with it.
                << ",\"clock\":{\"qpc\":" << clock_qpc.QuadPart << ",\"tick_ms\":" << clock_tick
                << ",\"frequency\":" << clock_frequency.QuadPart << '}' << ",\"queues\":" << stats.queues
                << ",\"late_queues\":" << stats.late_queues << ",\"presentation_faults\":" << stats.faults;
            // The first callers of late queue creations, by module (tifa-6: whose they were).
            log << ",\"late_queue_callers\":[";
            for (unsigned i = 0, n = 0; i < stats.late_queue_callers.size(); ++i)
                if (stats.late_queue_callers[i])
                    log << (n++ ? "," : "") << quoted(code_site(stats.late_queue_callers[i]));
            log << ']';
            log << "}\n" << std::flush;
            last = now;
        }
        Sleep(100);
    }
    // The session ended (the game's exit or a fault): the game's own textures.
    follow_texture_bias(false);
}

// After the session loop. Under SDK pacing the game renders into Streamline's
// buffers and the physical chain belongs to its pacer; neither can be retired
// under MHW. Stop therefore means FG Off; DLSS, Reflex and routing stay live
// until exit.
void end_session() {
    { // The loop ended on a fault: frame generation is off from here on.
        // The game's exit is not one; the status keeps its last state.
        const std::string_view why = stop_reason;
        if (why == "reflex_frames" || why == "presentation")
            mhw::settings::status::reflex(mhw::settings::status::Stage::off, why);
        if (why != "game_exit") mhw::settings::status::worker_failed(why);
    }
    mhw::frame_generation::game::arm(false);
    const auto fg_off_began = GetTickCount64();
    while (mhw::frame_generation::game::frame_generation_on() && GetTickCount64() - fg_off_began < 3000) Sleep(10);
    // A DLSS fault: the game's own rendering returns, at its own render
    // scale and anti-aliasing, instead of the reduced scale DLSS asked for.
    if (stop_reason == std::string_view("temporal_unhealthy")) {
        mhw::continuous_dlaa::stop();
        const auto until = GetTickCount64() + 3000;
        while (GetTickCount64() < until && !mhw::render_resolution::write_captures(log)) Sleep(50);
    }
    const auto frames = live_frames->snapshot();
    if (stop_reason == std::string_view("duration") && std::filesystem::exists(stop_file)) stop_reason = "stop_request";
    log_lifecycle(log);
    log << "{\"event\":\"fg_stopped\",\"reason\":\"" << stop_reason
        << "\",\"off\":" << !mhw::frame_generation::game::frame_generation_on()
        << ",\"reflex_frames\":" << frames.completed << ",\"reflex_faults\":" << frames.faults << ',';
    write_reflex_fault(log, frames);
    log << ",\"session\":";
    mhw::frame_generation::game::write(log);
    log << "}\n";
}

// Keep recording host lifecycle calls (resizes after FG ended) with a
// presentation heartbeat on change, until exit or 30 minutes.
void observe_until_exit(Session &session) {
    log << "{\"event\":\"fg_runtime_retained\",\"sdk_alive\":true,\"presentation_routed\":"
        << (presentation && presentation->snapshot().routing) << ",\"temporal_active\":true}\n"
        << std::flush;
    const auto observed_from = GetTickCount64();
    std::uint64_t last_resizes = ~0ULL, last_swapchains = ~0ULL;
    auto window_start = observed_from;
    log_frames(log, "stop");
    while (presentation && GetTickCount64() - observed_from < 1800000ULL) {
        log_lifecycle(log);
        poll_perf(log, output, session.name, session.creation, session.perf_requests);
        if (GetTickCount64() - window_start >= 5000) {
            log_frames(log, "fg_off");
            log_latency(log);
            mhw::perf::sampler::write(log);
            window_start = GetTickCount64();
        }
        mhw::render_resolution::write_captures(log);
        const auto stats = presentation->snapshot();
        if (stats.resizes != last_resizes || stats.swapchains != last_swapchains) {
            log << "{\"event\":\"presentation_state\",\"swapchains\":" << stats.swapchains
                << ",\"resizes\":" << stats.resizes << ",\"width\":" << stats.width << ",\"height\":" << stats.height
                << ",\"buffers\":" << stats.buffers << ",\"faults\":" << stats.faults
                << ",\"routing\":" << stats.routing << ",\"presents\":" << stats.presents
                << ",\"queues\":" << stats.queues << ",\"fault_site\":" << stats.fault_site
                << ",\"fault_hr\":" << stats.fault_hr << ",\"identity\":" << stats.identity
                << ",\"identity_refused\":" << stats.identity_refused
                << ",\"frame_generation_loaded\":" << stats.frame_generation_loaded
                << ",\"switches\":" << stats.switches << ",\"switch_failures\":" << stats.switch_failures
                << ",\"switch_refusals\":" << stats.switch_refusals << ",\"switch_reason\":" << stats.switch_reason
                << ",\"switch_waits\":" << stats.switch_waits
                << ",\"fault_why\":" << quoted(std::string(stats.fault_why.data()))
                << ",\"fault_thread\":" << stats.fault_thread << ",\"late_queues\":" << stats.late_queues
                << ",\"queue_overflows\":" << stats.queue_overflows << ",\"queue_failures\":" << stats.queue_failures
                << ",\"unroutable\":" << stats.unroutable_calls << ",\"last_resort\":" << stats.last_resort << "}\n"
                << std::flush;
            last_resizes = stats.resizes;
            last_swapchains = stats.swapchains;
        }
        Sleep(100);
    }
}

// The worker failed: why, in the log (or beside mhw_dlss.ini before it opened).
int fail(const std::exception &e) noexcept {
    try {
        startup_failure = e.what();
    } catch (...) {}
    mhw::settings::status::worker_failed(e.what());
    signal_startup();
    // Once Streamline has created the host chain under SDK pacing, the game
    // renders into Streamline's buffers: unhooking or shutting it down here
    // destroys them under the game. Keep presentation routed; FG Off only.
    if (presentation && presentation->snapshot().swapchains) {
        mhw::frame_generation::game::arm(false);
        try {
            log << "{\"event\":\"worker_failed\",\"reason\":" << quoted(e.what())
                << ",\"sdk_retained\":true,\"presentation_routed\":" << presentation->snapshot().routing
                << ",\"fg_off_requested\":true}\n"
                << std::flush;
            mhw::perf::chaos::write(log);
        } catch (...) {}
        return 3;
    }
    const bool clean = stop_device_attach();
    std::lock_guard lock(sdk_mutex);
    const bool sdk_clean = close_sdk(); // cleanup emits records before this JSON object
    try {
        log << "{\"event\":\"worker_failed\",\"reason\":" << quoted(e.what()) << ",\"hooks_restored\":" << clean
            << ",\"sdk_shutdown_passed\":" << sdk_clean << "}\n"
            << std::flush;
    } catch (...) {}
    // Failed before the log opened (configuration, paths, identity): leave
    // one line next to mhw_dlss.ini so the silent fail-closed is explainable.
    if (!log.is_open() && !config_folder.empty()) try {
            std::ofstream early(config_folder / L"startup-error.jsonl", std::ios::binary | std::ios::app);
            early << "{\"event\":\"worker_failed\",\"stage\":\"startup\",\"version\":\"" << mhw::settings::version
                  << "\",\"pid\":" << GetCurrentProcessId() << ",\"reason\":" << quoted(e.what()) << "}\n"
                  << std::flush;
        } catch (...) {}
    return 3;
}
} // namespace

DWORD WINAPI worker(void *) noexcept {
    if (const int refused = claim_process(); refused >= 0) return static_cast<DWORD>(refused);
    try {
        wchar_t dll_name[32768]{}, host_name[32768]{};
        const auto dll_length = GetModuleFileNameW(module, dll_name, static_cast<DWORD>(std::size(dll_name)));
        const auto host_length = GetModuleFileNameW(nullptr, host_name, static_cast<DWORD>(std::size(host_name)));
        require(dll_length && dll_length < std::size(dll_name), "module-path");
        require(host_length && host_length < std::size(host_name), "host-path");
        const std::filesystem::path dll(dll_name), host(host_name);
        read_configuration(dll, host);
        check_folders(host);
        auto session = open_log(dll);
        accept_device();
        if (wait_for_swap_chain()) {
            const auto wait_start = start_frames(session);
            if (wait_until_qualified(wait_start)) {
                mhw::perf::chaos::delay("arm");
                arm_dlss();
                mhw::perf::chaos::write(log);
                arm_reflex();
                arm_frame_generation();
                run_session();
            }
        }
        if (live_frames) end_session();
        observe_until_exit(session);
        return 0;
    } catch (const std::exception &e) {
        return static_cast<DWORD>(fail(e));
    }
}
} // namespace mhw::plugin
