#pragma once
#include <d3d12.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include "../dlss_sdk/initialization.hpp"

namespace mhw::ngx_startup {
inline constexpr unsigned entry_rva = 0x273f230, caller_rva = 0x23842dc;
inline constexpr std::uint64_t native_app = 0x05f8a85f;
// v0.29.0: both startup hooks' first 16 original bytes (clean image), for
// Astera Loader's al.hooks and for the code gates while AL's chain owns the
// entries: NVSDK_NGX_D3D12_Init and the game's DLSS 1 builder (0x23842a0).
inline constexpr unsigned head_size = 16, game_dlss_rva = 0x23842a0;
inline constexpr char entry_head[] = "48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41";
inline constexpr char game_dlss_head[] = "40 53 48 83 EC 20 48 8B D9 E8 82 53 16 00 84 C0";
struct Config {
    void *target{};
    std::uintptr_t caller{}, client_slots{};
    std::filesystem::path runtime, data;
    std::uintptr_t backend_slot{}; // production renderer slot; zero only in the isolated fixture
    // Frame-generation builds: Streamline's own NGX initialization may load the
    // provider first. Accepted only from this exact runtime path and hash.
    bool shared_provider{};
};
// One startup transaction. Pinned caller retains the configuration and stream.
// Only disable the hook; retain the trampoline and SDK/device until process exit.
bool install(const Config &, std::ostream &);
bool done() noexcept;
bool disable() noexcept;
// Each startup hook's removal (the native init, then the game's DLSS builder;
// both are tried); 0,0 = removed or never made (v0.28.6), 1 and the hook's AL
// state when set_enabled failed. (Up to v0.28.6, with MinHook: its status and
// GetLastError().)
std::array<int, 4> disable_result() noexcept;
// A startup hook target's code gate. AL's chain may already own the entry, so
// the original head (`head`, entry_head or game_dlss_head) with the live rest.
bool verify_entry(std::uintptr_t base, unsigned rva, unsigned bytes, const char *sha, const char *head) noexcept;
bool settled() noexcept;
bool supported() noexcept;
unsigned calls() noexcept;
bool write_graphics_snapshot() noexcept;
const dlss_sdk::BorrowedInitialization *borrowed_initialization() noexcept;
bool run(HMODULE, const char *version, bool shared_provider = false) noexcept;
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
void use_paths(const std::filesystem::path &output, const std::filesystem::path &runtime) noexcept;
} // namespace mhw::ngx_startup
