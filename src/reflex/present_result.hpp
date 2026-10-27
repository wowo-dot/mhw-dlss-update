#pragma once
#include <windows.h>
#include <cstdint>

namespace mhw::reflex {
enum class PresentationContract { synchronous_native, sdk_paced };
struct PresentDecision {
    HRESULT result;
    bool fault;
    bool forward_native;
};
// A lost device. Any failed Present reaches the game's sRender::checkResult,
// which aborts (MtDebug::abort); for a lost device that is the game's own
// behaviour without the mod, so that result is passed on unchanged.
constexpr bool device_lost(HRESULT hr) noexcept {
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == DXGI_ERROR_DEVICE_HUNG;
}
// The FG pacer may present another swap chain, another thread, or multiple
// frames. Absence of a TLS-observed native call is never permission to present
// the host frame again. The SDK owns the paced transaction: a failure is a
// fault (frame generation then turns off) and the game gets S_OK for the lost
// frame (v0.28.6), except a lost device, which it gets as before.
constexpr PresentDecision presentation_result(PresentationContract contract, unsigned observed_native_calls,
                                              HRESULT sdk_result, HRESULT native_result) noexcept {
    if (contract == PresentationContract::sdk_paced)
        return {FAILED(sdk_result) && !device_lost(sdk_result) ? S_OK : sdk_result, FAILED(sdk_result), false};
    return {native_result, observed_native_calls != 1 || FAILED(sdk_result), observed_native_calls == 0};
}
// A paced Present that keeps failing (frame generation could not be
// unloaded): after this many in a row the game gets the SDK's HRESULT again,
// so a frozen picture ends in the game's own abort and crash report.
inline constexpr unsigned paced_failure_limit = 300;
constexpr HRESULT paced_present_result(HRESULT decided, HRESULT sdk_result, unsigned consecutive_failures) noexcept {
    return consecutive_failures > paced_failure_limit ? sdk_result : decided;
}
// Whether a fault leaves the game's calls routed. With the identity chain
// always (v0.28.6, tifa-6): the chain the game holds is never presented, and
// its own Present returned DXGI_ERROR_INVALID_CALL, so the game aborted.
// Under SDK pacing too: the game renders into the SDK's back buffers and
// indexes them by the SDK's GetCurrentBackBufferIndex, so forwarding to the
// physical chain would give it that chain's indices (0..5 with six buffers
// for six-times frame generation), past the game's own back-buffer table
// (game-124: a garbage resource in its next back-buffer barrier, and D3D12
// crashed). Frame generation is turned off instead. Otherwise (synchronous
// contract, the game holding the real chain) its calls fall back to it.
constexpr bool fault_keeps_routing(PresentationContract contract, bool identity) noexcept {
    return identity || contract == PresentationContract::sdk_paced;
}
// Where a fault happened (PresentationStats::fault_site): a routed call, a
// queue proxy, a chain creation, or a switch that left no real chain.
enum class FaultSite : unsigned { none, present, present1, resize, resize1, queue, create, switch_chain };
// A device queue creation (v0.28.6, tifa-6): proxied through the SDK only
// before the game's swap chain exists, with a free slot. Afterwards nothing
// needs one (Streamline's own queues, an overlay's), and each proxy kept its
// queue: ten more filled the 16 slots, the 17th faulted. Full: native.
enum class QueueAdmission { native, late, overflow, proxy };
constexpr QueueAdmission queue_admission(bool accepting, bool chain_made, bool free_slot) noexcept {
    if (!accepting) return QueueAdmission::native;
    if (chain_made) return QueueAdmission::late;
    return free_slot ? QueueAdmission::proxy : QueueAdmission::overflow;
}
// A call on a swap chain while the game holds the identity chain: the mod's
// own calls (bypass) and other chains stay native; the identity chain's go to
// the real chain, through Streamline's proxy or natively after a last resort
// (presentation.cpp), and without a real chain are answered without the
// identity chain: never to the identity chain's own Present (v0.28.6).
enum class IdentityRoute { forward, real, unroutable };
constexpr IdentityRoute identity_route(bool bypass, bool identity_self, bool real_chain) noexcept {
    if (bypass || !identity_self) return IdentityRoute::forward;
    return real_chain ? IdentityRoute::real : IdentityRoute::unroutable;
}
// fg_queue's deep step at a present (0.32.0 diagnostic, fg-game-wait.md): the
// fence value of frame `index` may become `last_value`, the true value read at
// the last present for frame `last_index`, only when this present is the next
// frame's and the value grew. Then the next present waits for frame index - 1,
// never further back: the game's per-frame slots are three deep.
constexpr bool queue_follows(std::uint64_t last_index, std::uint64_t last_value, std::uint64_t index,
                             std::uint64_t value) noexcept {
    return last_index != 0 && index == last_index + 1 && value > last_value;
}
} // namespace mhw::reflex
