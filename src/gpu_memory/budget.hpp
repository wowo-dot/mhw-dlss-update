#pragma once
#include <dxgi1_4.h>
#include <atomic>
#include <cstdint>

// The mod's video memory check (v0.24.0, docs/research/large-resolutions.md).
// Its images grow with the screen (about 1.2 GB at 4K DLAA with frame
// generation). Allocations past the OS budget still succeed, but Windows then
// pages the game's own resources to system memory; the mod fails closed
// instead: DLSS (or frame generation) stays off and the game renders as it
// would without the mod.
namespace mhw::gpu_memory {
// The process's share of the adapter's local (dedicated) video memory as the
// OS budgets it (IDXGIAdapter3::QueryVideoMemoryInfo).
struct Local {
    std::uint64_t budget{}, usage{};
};
constexpr std::uint64_t room(const Local &m) noexcept {
    return m.usage < m.budget ? m.budget - m.usage : 0;
}
constexpr bool fits(const Local &m, std::uint64_t needed) noexcept {
    return needed <= room(m);
}

// One texture, rounded up to the 64 KiB placement alignment. Layout padding
// can add a little more; the DLSS images are also measured exactly when
// created (dlss_sdk::InlineFrame's byte limit).
constexpr std::uint64_t texture_bytes(std::uint64_t width, std::uint64_t height, unsigned pixel_bytes) noexcept {
    return (width * height * pixel_bytes + 65535) / 65536 * 65536;
}
// The continuous runtime's images (continuous_dlaa/runtime.cpp): the copies
// of the game's colour, depth and motion at the scene size (R11G11B10, R32F,
// R32_UINT); the DLSS inputs converted at the render size (RGBA32F, R32F,
// RG32F); the RGBA32F output and its R11G11B10 handoff at the scene size;
// and the spatial fallback's R11G11B10 image. A live preset change keeps the
// copies (copies=false).
constexpr std::uint64_t dlss_bytes(unsigned scene_width, unsigned scene_height, unsigned render_width,
                                   unsigned render_height, bool copies = true) noexcept {
    const auto scene = [&](unsigned b) { return texture_bytes(scene_width, scene_height, b); };
    const auto render = [&](unsigned b) { return texture_bytes(render_width, render_height, b); };
    return (copies ? 3 * scene(4) : 0) + render(16) + render(4) + render(8) + scene(16) + scene(4) + scene(4);
}

// The game's adapter, pinned once for the process (the reference is kept
// until exit; DXGI calls on it are free-threaded). Later pins are ignored.
inline std::atomic<IDXGIAdapter3 *> pinned{};
inline bool pin(IDXGIAdapter3 *adapter) noexcept {
    if (!adapter) return false;
    IDXGIAdapter3 *expected{};
    adapter->AddRef();
    if (!pinned.compare_exchange_strong(expected, adapter, std::memory_order_acq_rel)) adapter->Release();
    return true;
}
// False without a pinned adapter or when the query fails.
inline bool local(Local &out) noexcept {
    auto *adapter = pinned.load(std::memory_order_acquire);
    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    if (!adapter || FAILED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) return false;
    out = {info.Budget, info.CurrentUsage};
    return true;
}

// The last check, for the logs.
struct Check {
    bool done{}, read{}, passed{};
    std::uint64_t budget{}, usage{}, needed{};
};
// Reads the budget and checks `needed` against it; fails closed when the
// budget cannot be read.
inline Check check(std::uint64_t needed) noexcept {
    Check c{true};
    c.needed = needed;
    Local m;
    if ((c.read = local(m))) {
        c.budget = m.budget;
        c.usage = m.usage;
        c.passed = fits(m, needed);
    }
    return c;
}
} // namespace mhw::gpu_memory
