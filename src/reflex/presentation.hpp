#pragma once
#include "streamline.hpp"
#include "present_result.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <array>
#include <cstdint>
#include <iosfwd>

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
// One process-lived instance. Native interfaces stay in the host, including
// every returned queue/swap-chain reference. Mandatory public COM calls use
// owned SDK proxies; a TLS guard sends their native calls to the trampolines.
// Stop/drain/verify hooks BEFORE SDK shutdown; release pins AFTER shutdown.
// No frame tokens, sleep, markers, resource tags or rendering work here.
class Presentation final {
public:
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
    // Opt-in CPU metadata only, configured before install. No added hooks,
    // resource references, rendering calls or FG plugin initialization.
    void ownership(std::uintptr_t image, unsigned image_bytes);
    void ownership_running();
    void write_ownership(std::ostream&) const;
    PresentationStats snapshot() const;
    bool stop() noexcept;
    // Requires successful stop and SDK shutdown. False retains all pins.
    bool release_after_shutdown() noexcept;
    const std::array<PresentationHook, 12>& hooks() const noexcept;
private:
    // Retain dispatch/trampoline storage for stale entry pointers. Deliberately
    // no destructor that could call the SDK under process-detach loader lock.
    State* state_;
};
}
