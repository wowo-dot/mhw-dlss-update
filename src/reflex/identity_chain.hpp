#pragma once
#include <dxgi1_6.h>
#include <array>

// The identity chain (docs/research/fg-live-toggle.md, v0.27.4): the swap
// chain the game holds is a composition swap chain (no window, never
// presented); every call the game or an overlay makes on it goes to the real
// chain on the game's window, which the mod can then release and make again
// (with or without DLSS-G). Astera Loader hooks the methods of the object the
// game receives, so the identity chain must be the same DXGI class as the real
// one, and each routed slot its own function (AL refuses aliased slots and
// al.d3d12 then turns unavailable for every plugin). Pure helpers
// (tests/identity_chain_tests.cpp).
namespace mhw::reflex {

// IDXGISwapChain4 vtable slots routed for the identity chain: what the game,
// Astera Loader and overlays ask a swap chain. Rare methods that DXGI may
// implement as shared stubs (core window, mono, restrict output, background,
// rotation, source size, matrix transform) are left native.
inline constexpr std::array<unsigned, 22> identity_routed_slots{
    8,   // Present
    9,   // GetBuffer
    10,  // SetFullscreenState
    11,  // GetFullscreenState
    12,  // GetDesc
    13,  // ResizeBuffers
    14,  // ResizeTarget
    15,  // GetContainingOutput
    16,  // GetFrameStatistics
    17,  // GetLastPresentCount
    18,  // GetDesc1
    19,  // GetFullscreenDesc
    20,  // GetHwnd
    22,  // Present1
    31,  // SetMaximumFrameLatency
    32,  // GetMaximumFrameLatency
    33,  // GetFrameLatencyWaitableObject
    36,  // GetCurrentBackBufferIndex
    37,  // CheckColorSpaceSupport
    38,  // SetColorSpace1
    39,  // ResizeBuffers1
    40,  // SetHDRMetaData
};

// The identity chain's description for the game's chain `game` in `format`:
// the game's size and format (Astera Loader describes the chain before its
// hooks route), two buffers, and what composition chains require (stretch
// scaling, no window flags).
inline DXGI_SWAP_CHAIN_DESC1 identity_description(const DXGI_SWAP_CHAIN_DESC1 &game, DXGI_FORMAT format) noexcept {
    DXGI_SWAP_CHAIN_DESC1 d{};
    d.Width = game.Width ? game.Width : 64;
    d.Height = game.Height ? game.Height : 64;
    d.Format = format;
    d.SampleDesc.Count = 1;
    d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.BufferCount = 2;
    d.Scaling = DXGI_SCALING_STRETCH;
    d.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    d.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    return d;
}

// Whether `a` and `b` reach the same function in every routed slot, and those
// functions are distinct (the condition above).
inline bool identity_same_class(IUnknown *a, IUnknown *b) noexcept {
    if (!a || !b) return false;
    auto **ta = *reinterpret_cast<void ***>(a);
    auto **tb = *reinterpret_cast<void ***>(b);
    for (unsigned i = 0; i < identity_routed_slots.size(); ++i) {
        const auto slot = identity_routed_slots[i];
        if (!ta[slot] || ta[slot] != tb[slot]) return false;
        for (unsigned j = 0; j < i; ++j)
            if (ta[identity_routed_slots[j]] == ta[slot]) return false;
    }
    return true;
}

}  // namespace mhw::reflex
