#include "filter_shader.hpp"
#include "../dlss_sdk/geometry.hpp"

namespace mhw::scaled_temporal {
bool valid_filter_geometry(FilterGeometry g) noexcept {
    using dlss_sdk::max_side;
    if (g.render_width < 32 || g.render_height < 32 || g.render_width > max_side || g.render_height > max_side ||
        g.output_width > max_side || g.output_height > max_side)
        return false;
    // One render scale in [0.5, 1] for both axes, truncated per axis: identity
    // (DLAA), exact 2:1, a pillarboxed odd scene (3413x1440 at 1706x720), and
    // the Quality and Balanced extents. The adaptations use the actual ratio
    // render/output, never a fixed factor.
    return dlss_sdk::single_scale(g.render_width, g.render_height, g.output_width, g.output_height);
}
} // namespace mhw::scaled_temporal
