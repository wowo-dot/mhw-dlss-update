#pragma once
#include <windows.h>

namespace mhw::reflex {
enum class PresentationContract { synchronous_native, sdk_paced };
struct PresentDecision { HRESULT result; bool fault; bool forward_native; };
// The FG pacer may present another swap chain, another thread, or multiple
// frames. Absence of a TLS-observed native call is never permission to present
// the host frame again. The SDK owns the paced transaction and its HRESULT.
constexpr PresentDecision presentation_result(PresentationContract contract, unsigned observed_native_calls,
                                               HRESULT sdk_result, HRESULT native_result) noexcept {
    if (contract == PresentationContract::sdk_paced) return {sdk_result, FAILED(sdk_result), false};
    return {native_result, observed_native_calls != 1 || FAILED(sdk_result), observed_native_calls == 0};
}
// A fault on a routed chain (a failed Present, a resize that cannot be
// described). Under SDK pacing the game renders into the SDK's back buffers
// and indexes them by the SDK's GetCurrentBackBufferIndex, so routing stays:
// forwarding to the physical chain would give it that chain's indices (0..5
// with six buffers for six-times frame generation), past the game's own
// back-buffer table (game-124: a garbage resource in its next back-buffer
// barrier, and D3D12 crashed). Frame generation is turned off instead. The
// synchronous contract renders into the physical buffers, so it falls back.
constexpr bool fault_keeps_routing(PresentationContract contract) noexcept {
    return contract == PresentationContract::sdk_paced;
}
// Where a routed fault happened (PresentationStats::fault_site).
enum class FaultSite : unsigned { none, present, present1, resize, resize1 };
}
