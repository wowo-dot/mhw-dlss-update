#pragma once
// Astera Loader ("AL", plugin API 1.4) hook chains, shared by the modules that
// detour DXGI/D3D12 methods ("al.d3d12") and game functions ("al.hooks").
//
// al.d3d12: under AL the mod never patches DXGI/D3D12 methods itself. Each
// detour is registered at PRE_MAIN with the storage where AL writes the
// continuation (the next plugin's detour or the original method), and AL owns
// the one patch per method. Owners keep their detour bodies; the stored
// continuation replaces the MinHook trampoline. A gate in front of each
// registered detour passes calls straight on while its owner is not installed,
// as an absent hook did. Without AL (research builds), active() is false and
// every owner keeps its MinHook path.
//
// al.hooks: game functions other plugins may hook too (Main:Update) join AL's
// one chain per function instead of a patch of the mod's own.
#include <cstdint>
#include <vector>

namespace mhw::astera {
// COM families and slots as in al_plugin.h (QueryInterface is slot 0).
enum Family : std::uint32_t {factory=1,swapchain=2,device=3,queue=4,command_list=5};
constexpr std::uint32_t method(Family family,std::uint32_t slot) noexcept {return (std::uint32_t(family)<<16)|slot;}
struct Hook {std::uint32_t method;void *detour;void **next;};

// The plugin entry calls this once, from al_plugin_init, before any owner
// registers or installs. `begin`/`end` mark the calling thread's DXGI/D3D12
// calls as internal (they go straight to the original methods).
void activate(void (*begin)(void *),void (*end)(void *),void *context) noexcept;
bool active() noexcept;
void begin_internal() noexcept;
void end_internal() noexcept;
// Inside an Internal scope on this thread, with or without AL: the mod's own
// MinHook detours pass such calls on, as AL's chain does.
bool internal_call() noexcept;
// Calls the SDK proxies make into DXGI/D3D12 (creation, upgrade) reach the
// original methods once, past every plugin's detour. No-op without AL.
struct Internal {
    Internal() noexcept {begin_internal();}
    ~Internal() {end_internal();}
    Internal(const Internal &)=delete;Internal &operator=(const Internal &)=delete;
};

// A game function in AL's chain: `pattern` (al.scanner syntax, the function's
// original bytes) must match at `address`; AL checks it against the original
// bytes even after another plugin's patch, and writes *next before the detour
// can run. Lower `order` runs first, further from the original function.
struct GameHook {std::int32_t order;const char *name;const char *pattern;void *address;void *detour;void **next;};
enum class GameHookState : std::uint32_t {pending=0,active=1,disabled=2,failed=3};
// The plugin entry hands over AL's "al.hooks" 1.0 table (an al_hooks_t) and
// its plugin handle, once, from al_plugin_init.
void provide_game_hooks(const void *table,void *self) noexcept;
bool game_hooks_available() noexcept;
// The hook's handle, or 0 when AL refused it (AL's plugin log says why).
std::uint32_t game_hook(const GameHook &hook) noexcept;
// Takes the detour out of its chain or puts it back; calls already inside it
// finish normally. False for an unknown handle or without al.hooks.
bool set_game_hook_enabled(std::uint32_t handle,bool enabled) noexcept;
GameHookState game_hook_state(std::uint32_t handle) noexcept;
}
