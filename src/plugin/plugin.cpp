// Astera Loader plugin entry (plugin API 1.4: "al.d3d12" 1.0, "al.hooks" 1.0).
// AL loads AsteraLoader\plugins\mhw_dlss\mhw_dlss.dll, calls al_plugin_info,
// then al_plugin_init on its PRE_MAIN thread outside the loader lock. Init
// claims the presentation role, registers every DXGI/D3D12 detour (AL owns the
// patches), hands al.hooks to the game-function hooks, starts the worker and
// waits until it accepts the device. No vendor call runs under DllMain.
#include "plugin.hpp"
#include "../astera/astera_hooks.hpp"
#include "../alpha/menu.hpp"
#include "../alpha/status.hpp"
#include "../alpha/version.hpp"
#include "../projection_binding/recorder.hpp"
#include "../reflex/reflex_mode.hpp"
#include "../scaled_temporal/stream_route.hpp"
#include "../temporal_present/observer.hpp"
#include <cstddef>
#include <string_view>
#include <vector>

namespace mhw::plugin {
namespace {
const al_api_t* loader_api{};
al_plugin_t* loader_self{};
const al_d3d12_t* loader_d3d12{};
void loader_begin(void* self) { loader_d3d12->begin_internal(static_cast<al_plugin_t*>(self)); }
void loader_end(void* self) { loader_d3d12->end_internal(static_cast<al_plugin_t*>(self)); }
void loader_status(al_status_t status, const std::string& reason) noexcept {
    if (loader_api) loader_api->set_status(loader_self, status, reason.empty() ? nullptr : reason.c_str());
    namespace st = mhw::alpha::status;
    st::loader(status == AL_STATUS_DISABLED ? st::Loader::disabled : status == AL_STATUS_DEGRADED ? st::Loader::degraded : st::Loader::active, reason);
}
// UNAVAILABLE: AL will not call the detours (DirectX 11, a family failed).
bool loader_graphics_running(std::string& why) noexcept {
    al_d3d12_status_t s{}; s.size = sizeof(s); loader_d3d12->status(loader_self, &s);
    if (s.state != AL_D3D12_UNAVAILABLE) return true;
    why = s.reason ? s.reason : "al.d3d12 unavailable"; return false;
}
void AL_CALL on_phase(al_plugin_t*, al_phase_t phase, const void* data) {
    try {
        if (phase == AL_PHASE_DEVICE_CREATED) {
            const auto* d = static_cast<const al_phase_device_t*>(data);
            std::string why;
            if (!d || d->size < sizeof(*d) || !d->device) { loader_status(AL_STATUS_DEGRADED, "device phase without a device"); return; }
            if (!loader_graphics_running(why)) { loader_status(AL_STATUS_DEGRADED, "graphics hooks unavailable: " + why); return; }
            {
                Call call;
                if (accepting.load()) {
                    std::lock_guard lock(sdk_mutex);
                    if (accepting.load()) {
                        try {
                            attach_device(static_cast<IUnknown*>(d->adapter), static_cast<D3D_FEATURE_LEVEL>(d->minimum_feature_level),
                                static_cast<IUnknown*>(d->device));
                        } catch (const std::exception& e) { failed(e); }
                    }
                }
            }
            if (!success) loader_status(AL_STATUS_DEGRADED, failure.empty() ? "the game's device was not attached" : failure);
        } else if (phase == AL_PHASE_SWAPCHAIN_CREATED) {
            std::string why;
            if (!mhw::reflex::presentation_astera_swapchain_chains() || !loader_graphics_running(why)) {
                swapchain_chains_missing.store(true);
                loader_status(AL_STATUS_DEGRADED, "swap-chain hooks unavailable; DLSS and frame generation stay off");
            }
        }
    } catch (...) {}
}
const char* const supported_exes[]{mhw::projection_binding::host_sha, nullptr};
const al_plugin_info_t plugin_info{
    sizeof(al_plugin_info_t), AL_API_VERSION, "mhw_dlss", "MHW DLSS / Frame Generation", mhw::alpha::version,
    AL_PHASE_BIT(AL_PHASE_DEVICE_CREATED) | AL_PHASE_BIT(AL_PHASE_SWAPCHAIN_CREATED),
    0, nullptr, nullptr, supported_exes, on_phase, nullptr};
al_result_t start(const al_api_t* api, const al_init_t* init) {
    const auto refuse = [&](const std::string& why) { loader_status(AL_STATUS_DISABLED, why); loader_log(AL_LOG_ERROR, why); return AL_ERROR; };
    loader_d3d12 = static_cast<const al_d3d12_t*>(api->get_interface(init->self, AL_D3D12_INTERFACE, AL_D3D12_VERSION));
    if (!loader_d3d12 || loader_d3d12->size < sizeof(al_d3d12_t)) return refuse("this Astera Loader has no al.d3d12 1.0");
    // Every game-function hook of the mod is in AL's al.hooks chains.
    const auto* game_hooks = static_cast<const al_hooks_t*>(api->get_interface(init->self, AL_HOOKS_INTERFACE, AL_HOOKS_VERSION));
    if (!game_hooks || game_hooks->size < sizeof(al_hooks_t)) return refuse("this Astera Loader has no al.hooks 1.0");
    const auto* game = static_cast<const al_game_t*>(api->get_interface(init->self, AL_GAME_INTERFACE, AL_GAME_VERSION));
    if (game && game->size >= sizeof(al_game_t)) {
        if (game->graphics == AL_GRAPHICS_DX11) return refuse("the game is set to DirectX 11; this mod needs DirectX 12");
        exe_hash_verified = game->exe_sha256 && std::string_view(game->exe_sha256) == mhw::projection_binding::host_sha;
    }
    if (loader_d3d12->claim_presentation(init->self) != AL_OK) return refuse("another plugin holds the presentation role (see the log)");
    mhw::astera::activate(loader_begin, loader_end, init->self);
    mhw::astera::provide_game_hooks(game_hooks, init->self);
    std::vector<mhw::astera::Hook> hooks;
    mhw::reflex::presentation_astera_hooks(hooks);
    mhw::temporal_present::observer::astera_hooks(hooks);
    mhw::continuous_dlaa::astera_hooks(hooks);
    mhw::scaled_temporal::stream_route::astera_hooks(hooks);
    std::vector<al_d3d12_hook_t> table;
    for (const auto& h : hooks) table.push_back({h.method, 0, h.detour, h.next});
    if (loader_d3d12->hook(init->self, table.data(), static_cast<std::uint32_t>(table.size())) != AL_OK)
        return refuse("graphics hook registration refused (see the log)");
    loader_log(AL_LOG_INFO, std::to_string(table.size()) + " graphics detours registered, presentation role held");
    startup_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!startup_ready) return refuse("startup event");
    const auto thread = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
    if (!thread) return refuse("worker thread");
    CloseHandle(thread);
    // The worker reads its configuration and opens its log; device creation
    // cannot start before this returns (PRE_MAIN).
    if (WaitForSingleObject(startup_ready, 30000) != WAIT_OBJECT_0) return refuse("startup timed out");
    if (!accepting.load()) return refuse(startup_failure.empty() ? "startup failed (see the mod's log)" : startup_failure);
    loader_status(AL_STATUS_ACTIVE, {});
    // CMFM's Mods menu, when installed (optional; the ini is read by now).
    mhw::alpha::menu::add_pages(api, init->self, alpha_config, alpha_base, alpha_config.generation_plugin);
    return AL_OK;
}
}  // namespace

void loader_log(al_log_level_t level, const std::string& text) noexcept { if (loader_api) loader_api->log(loader_self, level, text.c_str()); }
}  // namespace mhw::plugin

extern "C" __declspec(dllexport) const al_plugin_info_t* AL_CALL al_plugin_info(void) { return &mhw::plugin::plugin_info; }
extern "C" __declspec(dllexport) al_result_t AL_CALL al_plugin_init(const al_api_t* api, const al_init_t* init) {
    namespace plugin = mhw::plugin;
    if (!api || !init || !init->self || api->size < offsetof(al_api_t, publish_interface)) return AL_ERROR;
    plugin::loader_api = api; plugin::loader_self = init->self;
    try { return plugin::start(api, init); } catch (...) { plugin::loader_status(AL_STATUS_DISABLED, "start-up exception"); return AL_ERROR; }
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        mhw::plugin::module = instance; DisableThreadLibraryCalls(instance);
        // An Astera Loader plugin: al_plugin_init starts the worker.
    }
    return TRUE;
}
