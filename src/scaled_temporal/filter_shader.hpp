#pragma once
#include <cstddef>
#include <span>
#include <vector>

namespace mhw::scaled_temporal {
// Builds a private variant of the observed motion-reconstruction PS. This does
// not change a native shader, PSO, resource, viewport, or command packet.
// Only the observed shader and top-left, same-aspect 1x/2x geometry are admitted.
// Native CBScreen/blur constants, UVs and t1..t4 must remain in render space;
// t0 and the output viewport must cover the complete output allocation.
struct FilterGeometry {
    unsigned render_width{},render_height{},output_width{},output_height{};
};
bool valid_filter_geometry(FilterGeometry) noexcept;
std::vector<std::byte> make_filter_shader(std::span<const std::byte> native,FilterGeometry);
}
