#pragma once
#include <cmath>
#include <cstdint>

namespace mhw::render_resolution {
struct Extent { unsigned width{},height{}; bool operator==(const Extent &) const = default; };
struct Plan { Extent display{},render{}; float requested{},quantized{}; bool valid{}; };

// The native setter selects config dimensions when its override flag is set.
// Admit that route only when it describes the same fixed display extent.
inline bool matching_basis(Extent display,unsigned override_enabled,Extent basis) noexcept {
    return override_enabled<=1 && basis==display;
}

// Match the pinned native setter: clamp to [0.5/content,1], truncate width,
// align it down to 16 pixels, then divide by display width. This first
// experiment requires content==1 and preserves allocation/display dimensions.
inline Plan plan(Extent display,float requested) noexcept {
    Plan p{display,{},requested};
    if(display.width<64||display.height<64||display.width>8192||display.height>8192||
       !std::isfinite(requested)||requested<.5F||requested>1.F)return p;
    const auto width=static_cast<unsigned>(static_cast<float>(display.width)*requested)&~15U;
    p.quantized=static_cast<float>(width)/static_cast<float>(display.width);
    p.render={static_cast<unsigned>(static_cast<float>(display.width)*p.quantized),
              static_cast<unsigned>(static_cast<float>(display.height)*p.quantized)};
    // The native view helper clamps again. Reject plans that the two stages
    // would interpret differently; do not claim an extent that will not apply.
    p.valid=p.quantized>=.5F&&p.quantized<=1.F&&p.render.width>=32&&p.render.height>=32;
    return p;
}

// Renderer-thread only. Restore our pending request before native policy runs,
// so a cached/no-op native update never treats our prior override as its own.
// A foreign/new native request wins over the value we previously wrote.
struct Ownership {
    bool owned{};
    float native{},override_value{};
    bool before(float current,float &restore) noexcept {
        const bool ours=owned&&current==override_value;
        restore=ours?native:current;
        owned=false;
        return ours;
    }
    void after(float native_value,float applied_override) noexcept {
        native=native_value;override_value=applied_override;owned=true;
    }
};
}
