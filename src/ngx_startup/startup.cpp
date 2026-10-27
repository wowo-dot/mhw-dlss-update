#include "startup.hpp"
#include "readiness.hpp"
#include "../game_memory/layout.hpp"
#include "../common/require.hpp"
#include "../common/log_path.hpp"
#include "../common/platform.hpp"
#include "../common/dlss_file_identity.hpp"
#include "../astera/astera_hooks.hpp"
#include "../projection_binding/memory.hpp"
#include "../perf/chaos.hpp"
#include <nvsdk_ngx.h>
#include <dxgi.h>
#include <psapi.h>
#include <winver.h>
#include <wrl/client.h>
#include <intrin.h>
#include <algorithm>
#include <atomic>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace mhw::ngx_startup {
namespace {
using Fn = unsigned(__cdecl *)(std::uint64_t, const wchar_t *, ID3D12Device *, unsigned);
using Microsoft::WRL::ComPtr;
Config cfg;
Fn original{};
std::ostream *out{};
std::atomic_uint count{}, pending{}, messages{}, native_result{};
std::atomic_bool finished{}, allowed{}, stopping{};
bool installed{}, initialized{};
HMODULE pinned_provider{};
ComPtr<ID3D12Device> device; // retained by the pinned diagnostic until process exit
// The shipped nvngx_dlss.dll: DLSS SDK 310.9.16 (0.32.0; 310.7.0 before).
constexpr char provider_sha[] = "d00b5b0ab29c3ca101b70c3a5ecb3a01b7ef33766a3383a28a18623083e6123e";
// 0.32.0: the DLSS Super Resolution file NGX runs (dlss_files.hpp), set with
// pinned_provider: its path and identity, for the log and Status.
std::wstring provider_path;
DlssFile provider_file;
// The game's own DLSS 1 (decline_game_dlss()). 0x23842a0 calls the game's NGX
// init (hook() below), checks SuperSampling and ImageSignalProcessing and
// builds a 72-byte wrapper (0x2383760: NGX parameters, a critical section at
// +16, no features at +56/+64) that cD3DRender::initForD3D12 stores at
// backend +0x42700. Its release (0x23837a0) also shuts NGX down, which this
// mod's own NGX must survive: the wrapper is freed as there, without the
// shutdown, the state the game's own refusals leave (NGX initialized, no
// wrapper; 0x51c6c28 = 2 or 3, the RTX 50 case).
// 0.31.1 (devilzavacado-2): once this startup has stopped, refused or done,
// the builder is skipped, the way it returns early when the game does not run
// D3D12: no wrapper and no NGX init. The game's own init would load the game
// folder's old DLSS files, and under Proton they hit a breakpoint and end the
// game. For the same reason the game's init is skipped when this mod's own
// NGX init failed (hook(), skipped): the builder takes that as NGX's refusal.
// All of the game's NGX calls go through the wrapper (clean image), so
// without one it makes none.
constexpr unsigned generic_free_rva = 0x2175260;
using GameDlss = void *(*)(void *);
using GenericFree = void (*)(void *, std::size_t);
GameDlss game_dlss_original{};
GenericFree generic_free{};
void *game_dlss_target{};
bool decline_requested{};
std::atomic_uint game_dlss_state{};
unsigned init_result{};
bool skipped{};
// v0.30.0: both hooks join al.hooks chains (AL patches each entry once, with
// its own thread handling; removal is set_enabled). Both detours run first in
// their chains (astera::first_in_chain): hook() checks _ReturnAddress() against
// the game's DLSS 1 builder. The removal is logged in detail (disable_result),
// so these two keep their own code rather than astera::OwnedHook.
static_assert(astera::valid_pattern<head_size>(entry_head) && astera::valid_pattern<head_size>(game_dlss_head),
              "each startup hook's pattern is its function's first 16 bytes");
std::uint32_t entry_handle{}, game_dlss_handle{};
std::uint32_t chain(const char *name, const char *pattern, void *target, void *detour, void **next) noexcept {
    return astera::game_hook({astera::first_in_chain, name, pattern, target, detour, next});
}
// This worker runs alongside the other plugins' PRE_MAIN work, which can take
// seconds: stracker_compat loading ten Stracker plugins took 3.9 s (0.30.0's
// long session), and both hooks were registered by then. Such a hook is
// pending until PRE_MAIN ends and active before the game's own code starts,
// so before the game can call it (settled_game_hook_state). Waits as long as
// for the code (readiness.hpp).
std::uint64_t pending_ms{};
bool chain_active(std::uint32_t handle) noexcept {
    const auto began = GetTickCount64();
    const auto state = astera::settled_game_hook_state(handle, readiness_timeout_ms);
    pending_ms += GetTickCount64() - began;
    return state == astera::GameHookState::active;
}
// Takes one hook out; results as disable_result() describes.
bool remove(bool &hooked, std::uint32_t handle, int *result) noexcept {
    if (!hooked) return true;
    const bool ok = astera::set_game_hook_enabled(handle, false);
    result[0] = ok ? 0 : 1;
    result[1] = ok ? 0 : static_cast<int>(astera::game_hook_state(handle));
    if (ok) hooked = false;
    return ok;
}
bool free_wrapper(void *wrapper) noexcept {
    const auto at = reinterpret_cast<std::uintptr_t>(wrapper);
    std::uint64_t features[2]{};
    if (!projection_binding::read(at + 56, features[0]) || !projection_binding::read(at + 64, features[1]) ||
        features[0] || features[1])
        return false;
    __try {
        DeleteCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(at + 16));
        generic_free(wrapper, 72);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void *game_dlss_hook(void *game_device) {
    if (stopping.load()) {
        game_dlss_state = 6;
        return nullptr;
    }
    pending.fetch_add(1);
    void *wrapper{};
    __try {
        wrapper = game_dlss_original(game_device);
        if (!wrapper)
            game_dlss_state = 2;
        else if (free_wrapper(wrapper)) {
            wrapper = nullptr;
            game_dlss_state = 3;
        } else
            game_dlss_state = 4;
    } __finally {
        pending.fetch_sub(1);
    }
    return wrapper;
}
void NVSDK_CONV logger(const char *, NVSDK_NGX_Logging_Level, NVSDK_NGX_Feature) {
    ++messages;
}
bool zero_slots() noexcept {
    for (unsigned i = 0; i < 7; ++i) {
        std::uint64_t value{};
        if (!projection_binding::read(cfg.client_slots + i * 8, value) || value) return false;
    }
    return true;
}
HRESULT pin_swap(std::uintptr_t address, IDXGISwapChain **result) noexcept {
    __try {
        return reinterpret_cast<IUnknown *>(address)->QueryInterface(IID_PPV_ARGS(result));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return E_FAIL;
    }
}
HRESULT swap_desc(IDXGISwapChain *swap, DXGI_SWAP_CHAIN_DESC *desc) noexcept {
    __try {
        return swap->GetDesc(desc);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return E_FAIL;
    }
}
void graphics(ID3D12Device *expected) {
    require(cfg.backend_slot && expected, "startup-no-graphics");
    std::uintptr_t backend{}, native_device{}, swap_address{}, table{}, qi{}, desc_method{};
    require(projection_binding::read(cfg.backend_slot, backend) && backend && backend < UINTPTR_MAX - 0x42720 &&
                projection_binding::read(backend + layout::d3d_render::device, native_device) &&
                native_device == reinterpret_cast<std::uintptr_t>(expected) &&
                projection_binding::read(backend + layout::d3d_render::swap_chain, swap_address) && swap_address &&
                projection_binding::read(swap_address, table) && table && table < UINTPTR_MAX - 13 * 8 &&
                projection_binding::read(table, qi) && projection_binding::read(table + 12 * 8, desc_method),
            "startup-graphics-pointers");
    const auto dxgi = GetModuleHandleW(L"dxgi.dll");
    require(dxgi != nullptr, "startup-dxgi-module");
    for (const auto address : {qi, desc_method}) {
        HMODULE owner{};
        MEMORY_BASIC_INFORMATION memory{};
        const bool queried = VirtualQuery(reinterpret_cast<void *>(address), &memory, sizeof(memory)) == sizeof(memory);
        const auto protection = memory.Protect & 255;
        require(
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(address), &owner) &&
                owner == dxgi && queried && memory.State == MEM_COMMIT && memory.Type == MEM_IMAGE &&
                !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
                (protection == PAGE_EXECUTE_READ || protection == PAGE_EXECUTE_READWRITE ||
                 protection == PAGE_EXECUTE_WRITECOPY),
            "startup-swap-method");
    }
    ComPtr<IDXGISwapChain> swap;
    require(SUCCEEDED(pin_swap(swap_address, &swap)) && reinterpret_cast<std::uintptr_t>(swap.Get()) == swap_address,
            "startup-swap-identity");
    DXGI_SWAP_CHAIN_DESC desc{};
    require(SUCCEEDED(swap_desc(swap.Get(), &desc)) && desc.BufferDesc.Width && desc.BufferDesc.Height &&
                desc.BufferDesc.Format != DXGI_FORMAT_UNKNOWN,
            "startup-swap-desc");
    std::uintptr_t stable{};
    require(projection_binding::read(backend + layout::d3d_render::swap_chain, stable) && stable == swap_address,
            "startup-swap-changed");
    *out << "{\"event\":\"graphics\",\"api\":\"D3D12\",\"backend\":" << backend << ",\"device\":" << native_device
         << ",\"swapchain\":" << swap_address << ",\"width\":" << desc.BufferDesc.Width
         << ",\"height\":" << desc.BufferDesc.Height << ",\"swapchain_format\":" << unsigned(desc.BufferDesc.Format)
         << "}\n"
         << std::flush;
}
// Whether `path` is this mod's own nvngx_dlss.dll with the pinned hash.
bool shipped_provider(const wchar_t *path) {
    std::error_code error;
    return std::filesystem::equivalent(path, cfg.runtime / L"nvngx_dlss.dll", error) && !error &&
           projection_binding::file_sha256(path) == provider_sha;
}
// Streamline may have loaded the provider from this exact runtime folder
// first. Accept only that path, when it is DLSS this mod runs (dlss_files.hpp).
bool shared_provider_verified() noexcept {
    try {
        const auto provider = cfg.runtime / L"nvngx_dlss.dll";
        const auto module = GetModuleHandleW(provider.c_str());
        if (!module) return false;
        wchar_t loaded[32768]{};
        const auto length = GetModuleFileNameW(module, loaded, 32768);
        std::error_code error;
        return length && length < 32768 && std::filesystem::equivalent(loaded, provider, error) && !error &&
               dlss_accepted(identify_dlss(loaded, shipped_provider(loaded)), dlss_super_resolution);
    } catch (...) {
        return false;
    }
}
// v0.30.1 (jgb2003-1): when NGX did not load this mod's provider, every
// loaded DLSS candidate (providers.hpp) with its path and file version, once.
std::atomic_uint foreign{};
bool providers_logged{};
std::string file_version(const wchar_t *path) {
    DWORD ignored{};
    const auto size = GetFileVersionInfoSizeW(path, &ignored);
    if (!size) return {};
    std::vector<unsigned char> data(size);
    VS_FIXEDFILEINFO *info{};
    UINT length{};
    if (!GetFileVersionInfoW(path, 0, size, data.data()) ||
        !VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&info), &length) || !info ||
        length < sizeof *info)
        return {};
    return std::to_string(HIWORD(info->dwFileVersionMS)) + '.' + std::to_string(LOWORD(info->dwFileVersionMS)) + '.' +
           std::to_string(HIWORD(info->dwFileVersionLS)) + '.' + std::to_string(LOWORD(info->dwFileVersionLS));
}
void log_providers(const char *phase) {
    if (providers_logged) return;
    providers_logged = true;
    std::vector<HMODULE> modules(512);
    DWORD needed{};
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        const auto bytes = static_cast<DWORD>(modules.size() * sizeof(HMODULE));
        if (!K32EnumProcessModules(GetCurrentProcess(), modules.data(), bytes, &needed)) needed = 0;
        if (needed <= bytes) break;
        modules.resize(needed / sizeof(HMODULE) + 64);
    }
    modules.resize(std::min<std::size_t>(needed / sizeof(HMODULE), modules.size()));
    auto strongest = Provider::none;
    *out << "{\"event\":\"dlss_providers\",\"phase\":\"" << phase << "\",\"modules\":[";
    bool first = true;
    for (const auto module : modules) {
        wchar_t path[32768]{};
        const auto length = GetModuleFileNameW(module, path, 32768);
        if (!length || length >= 32768) continue;
        const auto kind = provider_kind(path);
        if (kind == Provider::none) continue;
        std::error_code error;
        const bool mod =
            std::filesystem::equivalent(std::filesystem::path(path).parent_path(), cfg.runtime, error) && !error;
        if (!mod) strongest = stronger(strongest, kind);
        *out << (first ? "" : ",") << "{\"path\":" << std::quoted(mhw::log_path(path)) << ",\"kind\":" << unsigned(kind)
             << ",\"mod\":" << mod << ",\"version\":\"" << file_version(path) << "\"}";
        first = false;
    }
    foreign = static_cast<unsigned>(strongest);
    *out << "],\"enumerated\":" << modules.size() << ",\"foreign\":" << unsigned(strongest) << "}\n" << std::flush;
}
// 0.32.0: the DLSS Super Resolution files NGX has loaded, from wherever (this
// mod's folder; NVIDIA's store under the NVIDIA App's override; another file),
// found among the modules that could be one (providers.hpp) by their version
// resource. Each must be DLSS this mod runs (dlss_files.hpp) and is pinned;
// the newest is the provider the log and Status name. Logged once per phase.
bool loaded_providers_accepted(const char *phase) {
    std::vector<HMODULE> modules(512);
    DWORD needed{};
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        const auto bytes = static_cast<DWORD>(modules.size() * sizeof(HMODULE));
        if (!K32EnumProcessModules(GetCurrentProcess(), modules.data(), bytes, &needed)) needed = 0;
        if (needed <= bytes) break;
        modules.resize(needed / sizeof(HMODULE) + 64);
    }
    modules.resize(std::min<std::size_t>(needed / sizeof(HMODULE), modules.size()));
    bool any{}, all{true};
    HMODULE chosen{};
    std::wstring chosen_path;
    DlssFile chosen_file;
    *out << "{\"event\":\"dlss_provider\",\"phase\":\"" << phase << "\",\"files\":[";
    for (const auto module : modules) {
        wchar_t path[32768]{};
        const auto length = GetModuleFileNameW(module, path, 32768);
        if (!length || length >= 32768 || provider_kind(path) == Provider::none) continue;
        DlssFile version;
        if (!read_dlss_version(path, version) || version.internal != dlss_super_resolution) continue;
        const auto file = identify_dlss(path, shipped_provider(path));
        const bool accepted = dlss_accepted(file, dlss_super_resolution);
        HMODULE pinned{};
        if (accepted)
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                               reinterpret_cast<LPCWSTR>(module), &pinned);
        *out << (any ? "," : "") << "{\"path\":" << std::quoted(mhw::log_path(path))
             << ",\"kind\":" << unsigned(provider_kind(path)) << ",\"version\":\"" << version_text(file.version)
             << "\",\"shipped\":" << file.shipped << ",\"nvidia_signed\":" << file.nvidia_signed
             << ",\"signer\":" << std::quoted(file.signer) << ",\"accepted\":" << accepted << '}';
        any = true;
        all = all && accepted && pinned;
        if (accepted && pinned && (!chosen || file.version > chosen_file.version)) {
            chosen = pinned;
            chosen_path = path;
            chosen_file = file;
        }
    }
    const bool ok = any && all && chosen;
    *out << "],\"accepted\":" << ok << "}\n" << std::flush;
    if (ok) {
        pinned_provider = chosen;
        provider_path = chosen_path;
        provider_file = chosen_file;
    }
    return ok;
}
bool capabilities(const char *phase) {
    NVSDK_NGX_Parameter *p{};
    const auto get = NVSDK_NGX_D3D12_GetCapabilityParameters(&p);
    int values[3]{};
    NVSDK_NGX_Result results[3]{NVSDK_NGX_Result_Fail, NVSDK_NGX_Result_Fail, NVSDK_NGX_Result_Fail};
    const char *keys[]{NVSDK_NGX_Parameter_SuperSampling_Available,
                       NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver,
                       NVSDK_NGX_Parameter_SuperSampling_FeatureInitResult};
    if (get == NVSDK_NGX_Result_Success && p)
        for (unsigned i = 0; i < 3; ++i) results[i] = p->Get(keys[i], &values[i]);
    const auto destroy = p ? NVSDK_NGX_D3D12_DestroyParameters(p) : NVSDK_NGX_Result_Fail;
    // This mod's own file, if NGX loaded it (the NVIDIA App's override loads
    // NVIDIA's copy instead); the provider is whichever DLSS NGX runs.
    const bool found = GetModuleHandleW((cfg.runtime / L"nvngx_dlss.dll").c_str()) != nullptr;
    const bool identity = loaded_providers_accepted(phase);
    if (!identity) log_providers(phase);
    // Finish all queries before emitting a complete record, including failures.
    *out << "{\"event\":\"capabilities\",\"phase\":\"" << phase << "\",\"get_result\":" << unsigned(get)
         << ",\"get_values\":[" << unsigned(results[0]) << ',' << unsigned(results[1]) << ',' << unsigned(results[2])
         << "],\"values_u32\":[" << unsigned(values[0]) << ',' << unsigned(values[1]) << ',' << unsigned(values[2])
         << ']' << ",\"destroy_result\":" << unsigned(destroy) << ",\"modern_loaded\":" << found
         << ",\"provider_identity_verified\":" << identity << ",\"log_messages\":" << messages.load() << "}\n"
         << std::flush;
    return get == NVSDK_NGX_Result_Success && results[0] == 1 && results[1] == 1 && results[2] == 1 && values[0] == 1 &&
           values[1] == 0 && values[2] == 1 && destroy == 1 && identity;
}
void before(std::uint64_t app, const wchar_t *path, ID3D12Device *d, unsigned api, std::uintptr_t caller) noexcept {
    try {
        const bool slots_zero = zero_slots();
        *out << "{\"event\":\"native_enter\",\"caller\":" << caller
             << ",\"device\":" << reinterpret_cast<std::uintptr_t>(d) << ",\"api\":" << api
             << ",\"slots_zero\":" << slots_zero << ",\"tick\":" << GetTickCount64() << "}\n"
             << std::flush;
        require(caller == cfg.caller && app == native_app && api == 0x12 && d && path && slots_zero,
                "startup-entry-contract");
        require(d->GetNodeCount() == 1 && SUCCEEDED(d->GetDeviceRemovedReason()), "startup-device");
        const auto provider = cfg.runtime / L"nvngx_dlss.dll";
        if (GetModuleHandleW(provider.c_str())) {
            require(shared_provider_verified(), "modern-provider-already-loaded");
            *out << "{\"event\":\"provider_preloaded\",\"shared\":true,\"path_and_hash_verified\":true}\n"
                 << std::flush;
        }
        const auto luid = d->GetAdapterLuid();
        device = d;
        perf::chaos::delay("ngx_init"); // chaos = on (diagnostic)
        const auto search = cfg.runtime.wstring();
        const wchar_t *paths[]{search.c_str()};
        NVSDK_NGX_FeatureCommonInfo info{};
        info.PathListInfo = {paths, 1};
        info.LoggingInfo = {logger, NVSDK_NGX_LOGGING_LEVEL_ON, true};
        const auto result = NVSDK_NGX_D3D12_Init_with_ProjectID(
            "956cdd19-7d3c-4660-b664-91c20d2bc9bd", NVSDK_NGX_ENGINE_TYPE_CUSTOM, "0.1.0", cfg.data.c_str(), d, &info);
        *out << "{\"event\":\"public_init\",\"result\":" << unsigned(result)
             << ",\"api\":21,\"adapter_luid_low\":" << luid.LowPart << ",\"adapter_luid_high\":" << luid.HighPart
             << ",\"runtime_sha256\":\"" << provider_sha
             << "\",\"project_id\":\"956cdd19-7d3c-4660-b664-91c20d2bc9bd\",\"tick\":" << GetTickCount64() << "}\n"
             << std::flush;
        init_result = result;
        initialized = result == NVSDK_NGX_Result_Success;
        require(initialized, "public-init-failed");
        allowed = capabilities("before_native");
    } catch (const std::exception &e) {
        *out << "{\"event\":\"rejected\",\"reason\":" << std::quoted(e.what()) << "}\n" << std::flush;
    }
}
void after(unsigned result) noexcept {
    native_result = result;
    try {
        *out << "{\"event\":\"native_return\",\"result\":" << result << ",\"native_skipped\":" << skipped
             << ",\"arguments_unchanged\":true,\"result_unchanged\":" << !skipped << ",\"tick\":" << GetTickCount64()
             << "}\n"
             << std::flush;
        if (initialized) {
            const bool valid = capabilities("after_native");
            allowed = allowed.load() && valid;
        }
    } catch (...) {
        allowed = false;
    }
    finished = true;
}
unsigned __cdecl hook(std::uint64_t app, const wchar_t *path, ID3D12Device *d, unsigned api) {
    pending.fetch_add(1);
    const auto index = count.fetch_add(1);
    unsigned result{};
    const bool observe = index == 0 && !stopping.load();
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    // A reentrant/unexpected call never reinitializes the SDK or changes its
    // result. The observed one from the game's builder gets this mod's
    // refusal instead of the game's init when this mod's NGX did not start.
    __try {
        if (observe) before(app, path, d, api, caller);
        skipped = observe && !initialized && caller == cfg.caller;
        result = !skipped                        ? original(app, path, d, api)
                 : NVSDK_NGX_FAILED(init_result) ? init_result
                                                 : static_cast<unsigned>(NVSDK_NGX_Result_Fail);
        if (observe) after(result);
    } __finally {
        pending.fetch_sub(1);
    }
    return result;
}
} // namespace
bool install(const Config &config, std::ostream &log) {
    require(!installed && !original && config.target && config.caller && config.client_slots,
            "startup-install-contract");
    cfg = config;
    out = &log;
    log << std::boolalpha;
    require(cfg.runtime.is_absolute() && cfg.data.is_absolute(), "startup-paths");
    mhw::write_platform(log);
    // Before the init hook: the game builds the wrapper right after its init.
    // First of all (0.31.1): the builder is skipped whenever this startup
    // stops, also when one of the checks below refuses it.
    if (decline_requested) {
        // The wrapper's builder, constructor and release, and the allocator's
        // free (clean image): otherwise the game's DLSS 1 stays as it is.
        const auto base = reinterpret_cast<std::uintptr_t>(cfg.target) - entry_rva;
        const bool code =
            verify_entry(base, 0x23842a0, 344, "920c2df8717dcb2cf809d06f7d17c6e75b1d540b3e810e01b25b8a9e57d5ad2c",
                         game_dlss_head) &&
            projection_binding::verify_code(base, 0x2383760, 54,
                                            "1ff21702d0d9ef19421ba8132849d21a53e75be539b8574a1a26686f124c8eeb") &&
            projection_binding::verify_code(base, 0x23837a0, 130,
                                            "066bbfc555bcbeb999b5e7b0688877cc9ec2d07f9155a2c6af94ce98d71c9b30") &&
            projection_binding::verify_code(base, 0x2175260, 50,
                                            "d678c8b941af2d1e2bcd5e4a5926d070ac5e983003ee2b51f85b2f53388b72ce");
        game_dlss_target = reinterpret_cast<void *>(base + game_dlss_rva);
        generic_free = reinterpret_cast<GenericFree>(base + generic_free_rva);
        bool declining{};
        if (code) {
            // Live once AL accepts it, and left in for the session (disable()).
            game_dlss_handle =
                chain("mhw_dlss NGX:GameDlss1", game_dlss_head, game_dlss_target,
                      reinterpret_cast<void *>(&game_dlss_hook), reinterpret_cast<void **>(&game_dlss_original));
            declining = chain_active(game_dlss_handle) && game_dlss_original;
        }
        game_dlss_state = declining ? 1 : 5;
        log << "{\"event\":\"game_dlss\",\"declining\":" << declining << ",\"code\":" << code
            << ",\"pending_ms\":" << pending_ms << "}\n"
            << std::flush;
    }
    {
        // v0.28.5: the provider's path and why its hash is missing, if it is
        // (0.28.4 under MO2 looked in overwrite's copy of the folder; worker.cpp).
        // 0.32.0: a file a player swapped in runs too, when it is DLSS this mod
        // runs (dlss_files.hpp); the shipped one passes by its hash alone.
        const auto path = cfg.runtime / L"nvngx_dlss.dll";
        const auto digest = projection_binding::file_digest(path.wstring());
        const bool shipped = digest.sha == provider_sha;
        const auto file = digest.read ? identify_dlss(path.c_str(), shipped) : DlssFile{};
        const bool accepted = dlss_accepted(file, dlss_super_resolution);
        log << "{\"event\":\"provider_check\",\"path\":" << std::quoted(mhw::log_path(path)) << ",\"stage\":\""
            << digest.stage << "\",\"error\":" << digest.error << ",\"size\":" << digest.size
            << ",\"read\":" << digest.read << ",\"matches\":" << shipped << ",\"version\":\""
            << version_text(file.version) << "\",\"internal\":" << std::quoted(file.internal)
            << ",\"nvidia_signed\":" << file.nvidia_signed << ",\"accepted\":" << accepted << "}\n"
            << std::flush;
        // 0.40.0: a file that can't be read (missing) has its own reason (status.hpp).
        require(digest.read != 0, "dlss-file-missing");
        require(accepted, "startup-provider-sha256");
    }
    require(zero_slots(), "native-init-already-observed");
    require(!GetModuleHandleW((cfg.runtime / L"nvngx_dlss.dll").c_str()) || shared_provider_verified(),
            "modern-provider-already-loaded");
    std::filesystem::create_directories(cfg.data);
    require(astera::game_hooks_available(), "startup-al-hooks");
    // Write before enable: callback events may occur immediately after enabling.
    // hook_service: "minhook" was possible up to v0.28.6; the log tools read it.
    log << "{\"event\":\"installing\",\"tick\":" << GetTickCount64()
        << ",\"sdk_initializations_limit\":1,\"gpu_commands\":0,\"hook_service\":\"al\"}\n"
        << std::flush;
    entry_handle = chain("mhw_dlss NGX:D3D12_Init", entry_head, cfg.target, reinterpret_cast<void *>(&hook),
                         reinterpret_cast<void **>(&original));
    installed = entry_handle != 0;
    require(chain_active(entry_handle) && original, "startup-hook-chain");
    return true;
}
bool verify_entry(std::uintptr_t base, unsigned rva, unsigned bytes, const char *sha, const char *head) noexcept {
    // Another plugin's chain may own the entry already: its original head (the
    // pattern) with the live rest.
    std::array<std::uint8_t, head_size> first{};
    return astera::pattern_bytes<head_size>(head, first) &&
           projection_binding::verify_code_with_head(base, rva, bytes, sha, first.data(), head_size);
}
void decline_game_dlss() noexcept {
    decline_requested = true;
}
unsigned game_dlss() noexcept {
    return game_dlss_state.load();
}
bool done() noexcept {
    return finished.load();
}
// v0.28.6: why a removal failed (closed beta tifa-2: hook_disabled false on one
// PC only, with SharpPluginLoader installed).
// The builder's hook stays in since 0.31.1 (skipping it from here on), so its
// pair stays 0,0.
std::array<int, 4> disable_results{};
bool disable() noexcept {
    stopping = true;
    return remove(installed, entry_handle, &disable_results[0]); // a suspended entry may still forward
}
std::array<int, 4> disable_result() noexcept {
    return disable_results;
}
bool settled() noexcept {
    return pending.load() == 0;
}
bool supported() noexcept {
    return allowed.load();
}
Provider foreign_provider() noexcept {
    return static_cast<Provider>(foreign.load());
}
ActiveProvider active_provider() {
    if (!finished.load() || !pinned_provider) return {};
    return {version_text(provider_file.version), provider_file.shipped                                    ? "mod"
                                                 : provider_kind(provider_path) == Provider::nvidia_store ? "nvidia_app"
                                                                                                          : "replaced"};
}
unsigned calls() noexcept {
    return count.load();
}
namespace {
std::string graphics_failed;
} // namespace
bool write_graphics_snapshot() noexcept {
    try {
        graphics(device.Get());
        return true;
    } catch (const std::exception &e) {
        try {
            graphics_failed = e.what();
        } catch (...) {}
        return false;
    } catch (...) {
        return false;
    }
}
std::string graphics_failure() {
    return graphics_failed;
}
namespace {
bool reusable() noexcept {
    return !installed && stopping && finished && pending == 0 && count == 1 && allowed && native_result == 1 &&
           initialized && pinned_provider && device;
}
HMODULE validate_initialization(ID3D12Device *expected, const std::filesystem::path &runtime) noexcept {
    try {
        if (!reusable() || !expected || expected != device.Get()) return nullptr;
        // The same runtime folder, and the provider still the pinned module at
        // its path (a loaded image can't be rewritten, so the file it was
        // accepted as is the one running; 0.32.0: no re-hash on every rebuild).
        if (!std::filesystem::equivalent(runtime / L"nvngx_dlss.dll", cfg.runtime / L"nvngx_dlss.dll")) return nullptr;
        wchar_t path[32768]{};
        const auto length = GetModuleFileNameW(pinned_provider, path, 32768);
        std::error_code error;
        if (!length || length >= 32768 || !std::filesystem::equivalent(path, provider_path, error) || error)
            return nullptr;
        return pinned_provider;
    } catch (...) {
        return nullptr;
    }
}
const dlss_sdk::BorrowedInitialization retained_initialization{validate_initialization};
} // namespace
const dlss_sdk::BorrowedInitialization *borrowed_initialization() noexcept {
    return reusable() ? &retained_initialization : nullptr;
}
} // namespace mhw::ngx_startup
