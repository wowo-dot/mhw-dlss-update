#pragma once
// CMFM ("cmfm.d3d12", plugin API 1.3) hook chains, shared by the modules that
// detour DXGI/D3D12 methods. Under CMFM the mod never patches those methods
// itself: each detour is registered at PRE_MAIN with the storage where CMFM
// writes the continuation (the next plugin's detour or the original method),
// and CMFM owns the one patch per method. Owners keep their detour bodies; the
// stored continuation replaces the MinHook trampoline. A gate in front of each
// registered detour passes calls straight on while its owner is not installed,
// as an absent hook did. Without CMFM (research builds), active() is false and
// every owner keeps its MinHook path.
#include <cstdint>
#include <vector>

namespace mhw::cmfm {
// COM families and slots as in cmfm_plugin.h (QueryInterface is slot 0).
enum Family : std::uint32_t {factory=1,swapchain=2,device=3,queue=4,command_list=5};
constexpr std::uint32_t method(Family family,std::uint32_t slot) noexcept {return (std::uint32_t(family)<<16)|slot;}
struct Hook {std::uint32_t method;void *detour;void **next;};

// The plugin entry calls this once, from cmfm_plugin_init, before any owner
// registers or installs. `begin`/`end` mark the calling thread's DXGI/D3D12
// calls as internal (they go straight to the original methods).
void activate(void (*begin)(void *),void (*end)(void *),void *context) noexcept;
bool active() noexcept;
void begin_internal() noexcept;
void end_internal() noexcept;
// Calls the SDK proxies make into DXGI/D3D12 (creation, upgrade) reach the
// original methods once, past every plugin's detour. No-op without CMFM.
struct Internal {
    Internal() noexcept {begin_internal();}
    ~Internal() {end_internal();}
    Internal(const Internal &)=delete;Internal &operator=(const Internal &)=delete;
};
}
