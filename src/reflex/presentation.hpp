#pragma once
#include "streamline.hpp"
#include "present_result.hpp"
#include "../astera/astera_hooks.hpp"
#include "../frame_generation/presentation_buffers.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <array>
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace mhw::reflex {
struct PresentationStats {
    std::uint64_t queues{}, swapchains{}, presents{}, native_presents{}, test_presents{}, resizes{}, faults{},
        sdk_paced_presents{};
    // The last fault: FaultSite, its HRESULT (0 for a describe failure), the
    // thread and why (the exception's what() or the failed check; v0.28.6).
    std::uint32_t fault_site{}, fault_hr{}, fault_thread{};
    std::array<char, 48> fault_why{};
    // v0.28.6 (tifa-6, queue_admission): queue creations left native because
    // the game's chain existed (the first four callers kept) or the table was
    // full, and proxies Streamline refused (then made natively, last HRESULT).
    std::uint64_t late_queues{}, queue_overflows{}, queue_failures{};
    std::array<std::uintptr_t, 4> late_queue_callers{};
    std::uint32_t queue_failure_hr{};
    // Identity-chain calls answered without a real chain (identity_route).
    std::uint64_t unroutable_calls{};
    std::uint32_t width{}, height{}, format{}, buffers{};
    std::uintptr_t native_device{}, native_swapchain{}, proxy_swapchain{};
    unsigned installed_hooks{}, pending_calls{};
    bool routing{}, restored{};
    // v0.27.4 (fg-live-toggle.md): the game holds the identity chain; real
    // chains made again by switch_chain() and how the last request ended
    // (0 none, 1 with DLSS-G, 2 without, 3 refused: not possible here, 4 refused:
    // exclusive fullscreen, 5 refused: the game's waitable not as expected,
    // 6 failed and made again as before, 7 failed: no real chain, 8 failed and
    // made natively past Streamline: last_resort, v0.28.6).
    bool identity{}, frame_generation_loaded{}, last_resort{};
    bool real_chain{}; // a real chain behind the identity chain (none after reason 7)
    std::uint64_t identity_refused{}, switches{}, switch_failures{}, switch_refusals{}, waitable_failures{},
        switch_waits{};
    unsigned switch_reason{}, switch_pending{};
    // The first failed attempt's HRESULT, and the last one's when the switch
    // fell back further (the chain as before, then the last resort).
    std::uint32_t switch_last_hr{}, switch_final_hr{};
    // 0.32.0 diagnostic (waitable_ab): the phase (0 the chain's waitable, 1 an
    // always-set event), flips, and per phase the game's presents timed and
    // their summed intervals in microseconds, while DLSS-G paces the chain.
    unsigned ab_phase{};
    std::uint64_t ab_flips{};
    std::array<std::uint64_t, 2> ab_frames{}, ab_us{};
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
    void *context{};
    bool (*begin)(void *, std::uintptr_t native_swap, unsigned flags) noexcept {};
    void (*end)(void *, std::uintptr_t native_swap, unsigned flags, HRESULT native_result) noexcept {};
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
    bool described{}, windowed{}; // chain description after the call
    std::uint32_t width{}, height{}, format{}, buffers{}, flags{};
};
// Called on the calling thread before a lifecycle call is forwarded, outside
// the route mutex. Returns true if it turned an active feature Off.
struct LifecycleListener {
    void *context{};
    bool (*before)(void *, unsigned method) noexcept {};
    // Optional (v0.24.8): after a successful call on the host's chain (or a
    // creation), with the chain's back buffer format (DXGI_FORMAT).
    void (*after)(void *, std::uint32_t format) noexcept {};
};
// One process-lived instance. Native interfaces stay in the host, including
// every returned queue/swap-chain reference. Mandatory public COM calls use
// owned SDK proxies; a TLS guard sends their native calls to the trampolines.
// Stop/drain/verify hooks BEFORE SDK shutdown; release pins AFTER shutdown.
// No frame tokens, sleep, markers, resource tags or rendering work here.
class Presentation final {
public:
    // Device, factory and swap-chain detours; v0.27.4 adds the identity
    // chain's routed methods (identity_chain.hpp).
    static constexpr unsigned hook_capacity = 26;
    static constexpr unsigned lifecycle_capacity = 64;
    struct State;
    explicit Presentation(Streamline &sdk);
    Presentation(const Presentation &) = delete;
    Presentation &operator=(const Presentation &) = delete;
    // SDK-paced foundation routes host calls only on the chain-creation
    // thread. Its worker calls stay native even on the same physical chain.
    // Game integration must establish API-thread ownership separately.
    void install(ID3D12Device *native_device, IDXGIFactory2 *native_factory,
                 PresentationContract contract = PresentationContract::synchronous_native);
    void observe(PresentationObserver observer);
    // At most one listener, set once while routing.
    void lifecycle(LifecycleListener listener);
    // Copies lifecycle events recorded since the last call (oldest first) and
    // the number lost to the bounded buffer. Never waits for the route mutex.
    unsigned drain_lifecycle(std::array<LifecycleEvent, lifecycle_capacity> &out, std::uint64_t &dropped) noexcept;
    PresentationStats snapshot() const;
    // v0.27.4: the game's next ResizeBuffers on the identity chain makes the
    // real chain again with (`frame_generation`) or without DLSS-G
    // (Streamline's DLSS-G guide 18.0). The caller has DLSS-G Off and asks the
    // game for that resize. False without an identity chain or without DLSS-G
    // requested at slInit. Any thread.
    bool request_switch(bool frame_generation) noexcept;
    // Worker: where the game keeps the latency waitable it got from the
    // identity chain (cD3DRender+0x10F0, checked by the caller's code gate);
    // a switch replaces it there.
    void waitable_slot(std::uintptr_t address) noexcept;
    // 0.32.0 diagnostic (ini waitable_ab, fg pacing study): while DLSS-G paces
    // the chain, the game's per-frame wait alternates every 10 s between the
    // chain's latency waitable and an always-set event (Reflex's sleep alone
    // then paces the game). Any thread.
    void waitable_ab(bool on) noexcept;
    // Ends routing: without AL the host's later calls on the identity chain
    // reach its own methods, which cannot present with a game's flags
    // (tifa-6), so only for a host done with its chain (the fixtures). Under
    // AL false while the identity chain exists. The plugin never stops while
    // the game runs (frame generation Off only, routing until exit).
    bool stop() noexcept;
    // Requires successful stop and SDK shutdown. False retains all pins.
    bool release_after_shutdown() noexcept;
    const std::array<PresentationHook, hook_capacity> &hooks() const noexcept;

private:
    // Retain dispatch/trampoline storage for stale entry pointers. Deliberately
    // no destructor that could call the SDK under process-detach loader lock.
    State *state_;
};
// The routed host chain's GetBuffer labels, last host index and Present-entry
// index (the crash log writes them), copied under a separate lock that never
// waits for the route mutex. False before construction.
bool scene_presentation_buffers(frame_generation::PresentationBuffers &) noexcept;
// AL: the presentation detours, each with the storage AL fills with its
// continuation; registered at PRE_MAIN with claim_presentation. Factory and
// device chains exist from DEVICE_CREATED, swap-chain chains once the game's
// first swap chain is created (checked by presentation_astera_swapchain_chains).
void presentation_astera_hooks(std::vector<astera::Hook> &);
bool presentation_astera_swapchain_chains() noexcept;
} // namespace mhw::reflex
