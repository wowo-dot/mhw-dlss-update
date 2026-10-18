#pragma once
// Astera Loader ("AL", plugin API 1.4) hook chains, shared by the modules that
// detour DXGI/D3D12 methods ("al.d3d12") and game functions ("al.hooks").
//
// al.d3d12: under AL the mod never patches DXGI/D3D12 methods itself. Each
// detour is registered at PRE_MAIN with the storage where AL writes the
// continuation (the next plugin's detour or the original method), and AL owns
// the one patch per method. Owners keep their detour bodies; the stored
// continuation replaces a trampoline. A gate in front of each registered
// detour passes calls straight on while its owner is not installed, as an
// absent hook would. The DLL starts only under AL (al_plugin_init), with both
// interfaces; tests use a stand-in loader (tests/fake_astera.hpp).
//
// al.hooks: game functions other plugins may hook too (Main:Update) join AL's
// one chain per function instead of a patch of the mod's own.
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace mhw::astera {
// COM families and slots as in al_plugin.h (QueryInterface is slot 0).
enum Family : std::uint32_t { factory = 1, swapchain = 2, device = 3, queue = 4, command_list = 5 };
constexpr std::uint32_t method(Family family, std::uint32_t slot) noexcept {
    return (std::uint32_t(family) << 16) | slot;
}
struct Hook {
    std::uint32_t method;
    void *detour;
    void **next;
};
// The gate in front of a registered detour: while its owner's `live` (an
// atomic flag, or a pointer that is null until installed) is unset, calls go
// straight to `next[index]`, the continuation AL stored, as if the detour were
// absent. Register &Gate<...>::call as the Hook's detour.
template <auto Detour, auto &live, auto &next, unsigned index> struct Gate;
template <class R, class... A, R(WINAPI *Detour)(A...), auto &live, auto &next, unsigned index>
struct Gate<Detour, live, next, index> {
    static R WINAPI call(A... a) {
        if (live.load(std::memory_order_acquire)) return Detour(a...);
        return reinterpret_cast<R(WINAPI *)(A...)>(next[index])(a...);
    }
};

// The plugin entry calls this once, from al_plugin_init, before any owner
// registers or installs. `begin`/`end` mark the calling thread's DXGI/D3D12
// calls as internal (they go straight to the original methods).
void activate(void (*begin)(void *), void (*end)(void *), void *context) noexcept;
bool active() noexcept;
void begin_internal() noexcept;
void end_internal() noexcept;
// Inside an Internal scope on this thread: the mod's own detours pass such
// calls on, as AL's chain does.
bool internal_call() noexcept;
// Calls the SDK proxies make into DXGI/D3D12 (creation, upgrade) reach the
// original methods once, past every plugin's detour.
struct Internal {
    Internal() noexcept { begin_internal(); }
    ~Internal() { end_internal(); }
    Internal(const Internal &) = delete;
    Internal &operator=(const Internal &) = delete;
};

// A game function in AL's chain: `pattern` (al.scanner syntax, the function's
// original bytes) must match at `address`; AL checks it against the original
// bytes even after another plugin's patch, and writes *next before the detour
// can run. Lower `order` runs first, further from the original function.
struct GameHook {
    std::int32_t order;
    const char *name;
    const char *pattern;
    void *address;
    void *detour;
    void **next;
};
// A pattern of exactly N plain bytes ("40 53 ..."), as bytes; false for a
// wildcard, a bad digit or another length. Code gates check a function's
// original head with it while AL's chain owns the function's entry.
template <unsigned N> constexpr bool pattern_bytes(const char *p, std::array<std::uint8_t, N> &out) noexcept {
    const auto nibble = [](char c) noexcept {
        return c >= '0' && c <= '9'   ? c - '0'
               : c >= 'A' && c <= 'F' ? c - 'A' + 10
               : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                      : -1;
    };
    unsigned n{};
    while (*p) {
        if (*p == ' ') {
            ++p;
            continue;
        }
        const int hi = nibble(p[0]);
        if (hi < 0) return false;
        const int lo = nibble(p[1]);
        if (lo < 0 || n == N) return false;
        out[n++] = static_cast<std::uint8_t>(hi * 16 + lo);
        p += 2;
    }
    return n == N;
}
template <unsigned N> constexpr bool valid_pattern(const char *p) noexcept {
    std::array<std::uint8_t, N> bytes{};
    return pattern_bytes<N>(p, bytes);
}
enum class GameHookState : std::uint32_t { pending = 0, active = 1, disabled = 2, failed = 3 };
// The plugin entry hands over AL's "al.hooks" 1.0 table (an al_hooks_t) and
// its plugin handle, once, from al_plugin_init.
void provide_game_hooks(const void *table, void *self) noexcept;
bool game_hooks_available() noexcept;
// The hook's handle, or 0 when AL refused it (AL's plugin log says why).
std::uint32_t game_hook(const GameHook &hook) noexcept;
// Detours that read their caller ([rsp] at entry: the asm stubs' caller,
// _ReturnAddress()) must run first in their chain: AL's entry stub jumps
// straight to the first detour, and each later one is called by the one
// before. The mod's own game hooks use this order (v0.30.0).
inline constexpr std::int32_t first_in_chain = -1000000;
// One of the mod's own game-function hooks, in its al.hooks chain.
// hook_owned() registers it, live at once, so `next` must be the very pointer
// the detour continues with: it is written before the detour can run, and
// stays valid after unhook_owned(). False without al.hooks or when AL
// refuses it (AL's plugin log says why).
struct OwnedHook {
    const char *name{};    // for AL's log, e.g. "mhw_dlss Render:Update"
    const char *pattern{}; // the target's first 16 original bytes (al.hooks checks them)
    void *target{};
    std::uint32_t handle{}; // 0 until registered, and when refused
    bool live{};
};
bool hook_owned(OwnedHook &hook, void *detour, void **next) noexcept;
// Takes it out of its chain again (set_enabled). True when it is out, or was
// never in.
bool unhook_owned(OwnedHook &hook) noexcept;
// Takes the detour out of its chain or puts it back; calls already inside it
// finish normally. False for an unknown handle or without al.hooks.
bool set_game_hook_enabled(std::uint32_t handle, bool enabled) noexcept;
GameHookState game_hook_state(std::uint32_t handle) noexcept;
} // namespace mhw::astera
