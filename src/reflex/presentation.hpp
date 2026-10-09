#pragma once
#include "streamline.hpp"
#include "present_result.hpp"
#include "../astera/astera_hooks.hpp"
#if defined(MHW_FG_SCENE_PROBE) || defined(MHW_FG_GAME)
#include "../frame_generation/presentation_buffers.hpp"
#endif
#include <d3d12.h>
#include <dxgi1_6.h>
#include <array>
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace mhw::reflex {
struct PresentationStats {
    std::uint64_t queues{}, swapchains{}, presents{}, native_presents{}, test_presents{}, resizes{}, faults{}, sdk_paced_presents{};
    std::uint32_t width{}, height{}, format{}, buffers{};
    std::uintptr_t native_device{}, native_swapchain{}, proxy_swapchain{};
    unsigned installed_hooks{}, pending_calls{};
    bool routing{}, restored{};
};
struct PresentationHook {
    std::uintptr_t target{};
    std::array<std::uint64_t, 4> original{};
    bool created{}, enabled{}, restored{};
};
// Optional frame observer surrounds the host's one routed Present transaction,
// outside the proxy's recursive native call. Storage must outlive stop/drain.
// A false begin ticket means no matching end callback. Never changes HRESULT.
struct PresentationObserver {
    void* context{};
    bool (*begin)(void*, std::uintptr_t native_swap, unsigned flags) noexcept{};
    void (*end)(void*, std::uintptr_t native_swap, unsigned flags, HRESULT native_result) noexcept{};
};
// Swap-chain lifecycle call made by the host (not by the SDK's own recursion).
// Methods: 1..3 create (desc, hwnd, core window), 6 SetFullscreenState,
// 8 ResizeBuffers, 11 ResizeBuffers1, 12 ResizeTarget. CPU metadata only.
struct LifecycleEvent {
    unsigned method{}, thread{};
    bool host_thread{}, current_chain{}, routed{}, guard_off{};
    std::uint32_t result{};
    std::uint64_t sequence{}, began_qpc{}, ended_qpc{};
    std::array<std::uint64_t, 6> args{};
    bool described{}, windowed{};  // chain description after the call
    std::uint32_t width{}, height{}, format{}, buffers{}, flags{};
};
// Called on the calling thread before a lifecycle call is forwarded, outside
// the route mutex. Returns true if it turned an active feature Off.
struct LifecycleListener {
    void* context{};
    bool (*before)(void*, unsigned method) noexcept{};
};
// One process-lived instance. Native interfaces stay in the host, including
// every returned queue/swap-chain reference. Mandatory public COM calls use
// owned SDK proxies; a TLS guard sends their native calls to the trampolines.
// Stop/drain/verify hooks BEFORE SDK shutdown; release pins AFTER shutdown.
// No frame tokens, sleep, markers, resource tags or rendering work here.
class Presentation final {
public:
    // The FG scene variant also hooks ResizeTarget (13); others install 12.
    static constexpr unsigned hook_capacity = 13;
    static constexpr unsigned lifecycle_capacity = 64;
    struct State;
    explicit Presentation(Streamline& sdk);
    Presentation(const Presentation&) = delete;
    Presentation& operator=(const Presentation&) = delete;
    // SDK-paced foundation routes host calls only on the chain-creation
    // thread. Its worker calls stay native even on the same physical chain.
    // Game integration must establish API-thread ownership separately.
    void install(ID3D12Device* native_device, IDXGIFactory2* native_factory,
                 PresentationContract contract = PresentationContract::synchronous_native);
    void observe(PresentationObserver observer);
    // At most one listener, set once while routing.
    void lifecycle(LifecycleListener listener);
    // Copies lifecycle events recorded since the last call (oldest first) and
    // the number lost to the bounded buffer. Never waits for the route mutex.
    unsigned drain_lifecycle(std::array<LifecycleEvent, lifecycle_capacity>& out, std::uint64_t& dropped) noexcept;
    // Opt-in CPU metadata only, configured before install. No added hooks,
    // resource references, rendering calls or FG plugin initialization.
    void ownership(std::uintptr_t image, unsigned image_bytes);
    void ownership_running();
    void write_ownership(std::ostream&) const;
    PresentationStats snapshot() const;
    bool stop() noexcept;
    // Requires successful stop and SDK shutdown. False retains all pins.
    bool release_after_shutdown() noexcept;
    const std::array<PresentationHook, hook_capacity>& hooks() const noexcept;
private:
    // Retain dispatch/trampoline storage for stale entry pointers. Deliberately
    // no destructor that could call the SDK under process-detach loader lock.
    State* state_;
};
#if defined(MHW_FG_SCENE_PROBE) || defined(MHW_FG_GAME)
// Defined only in the scene-diagnostic library variant (mhw_fg_reflex_presentation). Copies the routed host chain's
// GetBuffer labels, last host index and Present-entry index under a separate
// lock that never waits for the route mutex. False before construction.
bool scene_presentation_buffers(frame_generation::PresentationBuffers&) noexcept;
#endif
// AL: the presentation detours, each with the storage AL fills with its
// continuation; registered at PRE_MAIN with claim_presentation. Factory and
// device chains exist from DEVICE_CREATED, swap-chain chains once the game's
// first swap chain is created (checked by presentation_astera_swapchain_chains).
void presentation_astera_hooks(std::vector<astera::Hook>&);
bool presentation_astera_swapchain_chains() noexcept;
}
