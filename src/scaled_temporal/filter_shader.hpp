#pragma once

namespace mhw::scaled_temporal {
// The render extent inside the output (scene) extent that the stream route's
// private passes are built for (render_resolution::plan).
struct FilterGeometry {
    unsigned render_width{}, render_height{}, output_width{}, output_height{};
};
// One render scale in [0.5, 1] on both axes, truncated per axis, within the
// D3D12 texture size limit.
bool valid_filter_geometry(FilterGeometry) noexcept;
} // namespace mhw::scaled_temporal
