#pragma once
#include <cmath>
#include <cstdint>

namespace mhw::render_resolution {
struct Extent {
    unsigned width{}, height{};
    bool operator==(const Extent &) const = default;
};
struct Plan {
    Extent display{}, render{};
    float requested{}, quantized{};
    bool valid{};
};

// The native setter selects config dimensions when its override flag is set.
// Admit that route only when it describes the same fixed display extent.
// The scale basis is the display (scene) size, or with the aspect-ratio
// option (scene letterboxed or pillarboxed) the window size.
inline bool matching_basis(Extent display, unsigned override_enabled, Extent basis, Extent window) noexcept {
    return override_enabled <= 1 &&
           (basis == display || (override_enabled == 1 && basis == window && display.width <= window.width &&
                                 display.height <= window.height));
}

// Match the pinned native setter (RVA 0x229bd70): clamp to [0.5/content,1],
// truncate basis-width times scale, align it down to 16 pixels, then divide by
// the basis width. The basis is the display (scene) width, or the config basis
// (the window width) when the aspect-ratio option sets the override flag, so a
// pillarboxed 3413-wide scene in a 5120 window quantizes against 5120. The
// render extent is the scene times that scale, truncated per dimension. This
// requires content==1 and preserves allocation/display dimensions.
// Sides up to D3D12's largest 2D texture (D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION);
// the float products stay exact (below 2^24).
inline constexpr unsigned max_side = 16384;
inline Plan plan(Extent display, unsigned basis_width, float requested) noexcept {
    Plan p{display, {}, requested};
    if (display.width < 64 || display.height < 64 || display.width > max_side || display.height > max_side ||
        basis_width < display.width || basis_width > max_side || !std::isfinite(requested) || requested < .5F ||
        requested > 1.F)
        return p;
    const auto width = static_cast<unsigned>(static_cast<float>(basis_width) * requested) & ~15U;
    p.quantized = static_cast<float>(width) / static_cast<float>(basis_width);
    p.render = {static_cast<unsigned>(static_cast<float>(display.width) * p.quantized),
                static_cast<unsigned>(static_cast<float>(display.height) * p.quantized)};
    // The native view helper clamps again. Reject plans that the two stages
    // would interpret differently; do not claim an extent that will not apply.
    p.valid = p.quantized >= .5F && p.quantized <= 1.F && p.render.width >= 32 && p.render.height >= 32;
    return p;
}
inline Plan plan(Extent display, float requested) noexcept {
    return plan(display, display.width, requested);
}

// Renderer-thread only. Restore our pending request before native policy runs,
// so a cached/no-op native update never treats our prior override as its own.
// A foreign/new native request wins over the value we previously wrote.
struct Ownership {
    bool owned{};
    float native{}, override_value{};
    bool before(float current, float &restore) noexcept {
        const bool ours = owned && current == override_value;
        restore = ours ? native : current;
        owned = false;
        return ours;
    }
    void after(float native_value, float applied_override) noexcept {
        native = native_value;
        override_value = applied_override;
        owned = true;
    }
};
} // namespace mhw::render_resolution
