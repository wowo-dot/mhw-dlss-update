#pragma once
#include <d3d12.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include "../dlss_sdk/live.hpp"

namespace mhw::ngx_startup {
inline constexpr unsigned entry_rva=0x273f230, caller_rva=0x23842dc;
inline constexpr std::uint64_t native_app=0x05f8a85f;
struct Config {
    void *target{};
    std::uintptr_t caller{},client_slots{};
    std::filesystem::path runtime,data;
    std::uintptr_t backend_slot{}; // production renderer slot; zero only in the isolated fixture
    bool hook_pool_reserved{}; // only the same DLL's successful startup reservation
    // Frame-generation builds: Streamline's own NGX initialization may load the
    // provider first. Accepted only from this exact runtime path and hash.
    bool shared_provider{};
};
// One startup transaction. Pinned caller retains the configuration and stream.
// Only disable the hook; retain the trampoline and SDK/device until process exit.
bool install(const Config &,std::ostream &);
bool done() noexcept;
bool disable() noexcept;
// MinHook's status and GetLastError() for each startup hook's removal (the
// native init, then the game's DLSS builder); 0 = removed or never made (v0.28.6).
std::array<int,4> disable_result() noexcept;
bool settled() noexcept;
bool supported() noexcept;
unsigned calls() noexcept;
bool write_graphics_snapshot() noexcept;
const dlss_sdk::BorrowedInitialization *borrowed_initialization() noexcept;
bool run(HMODULE,const char *version,bool hook_pool_reserved=false,bool shared_provider=false) noexcept;
// Alpha: the game's own DLSS 1 is declined (docs/research/game-dlss1.md):
// right after the game's NGX init, which this transaction observes, the game
// builds its DLSS 1 context (0x23842a0); it is freed and the game gets none,
// as on cards DLSS 1 does not support (RTX 50, where the mod was tested).
// Call before run().
void decline_game_dlss() noexcept;
// What became of it: 0 not requested, 1 hooked and not called yet, 2 the game
// made none (DLSS 1 unsupported), 3 declined, 4 kept (unexpected wrapper
// state), 5 not hooked (the game's code differs).
unsigned game_dlss() noexcept;
// Alpha: run() logs to this folder and takes the NGX runtime from this folder
// (mhw_dlss.ini), instead of the research sidecars <dll>.output-dir and
// <output>/sdk-runtime-dir.txt. Both must be existing absolute directories.
void use_paths(const std::filesystem::path &output,const std::filesystem::path &runtime) noexcept;
}
