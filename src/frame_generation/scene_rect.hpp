#pragma once
#include <array>

namespace mhw::frame_generation::game {
// Where the game's scene sits in the back buffer: the whole output, or a
// letterbox/pillarbox with MHW's aspect-ratio option (pixels; x/y = left/top).
struct SceneRect {
    unsigned x{}, y{}, width{}, height{};
};
// Streamline's sl::Extent field order is {top, left, width, height}. v0.11.1
// passed {x, y, ...}: every letterboxed or pillarboxed rectangle then fell
// outside the back buffer, Streamline reset the subrect to the whole window,
// warped the scene across it and ignored the scene-sized HUD-less image
// ("extent does not match color buffer size", game-19 to game-27).
constexpr std::array<unsigned, 4> extent_fields(SceneRect r) noexcept { return {r.y, r.x, r.width, r.height}; }
// The rectangle lies inside a width x height back buffer.
constexpr bool inside(SceneRect r, unsigned width, unsigned height) noexcept {
    return r.width && r.height && r.x <= width && r.y <= height && r.width <= width - r.x && r.height <= height - r.y;
}
}  // namespace mhw::frame_generation::game
