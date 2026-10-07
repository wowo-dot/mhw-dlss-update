#include "frame_generation/camera_constants.hpp"
#include "frame_generation/input_ring.hpp"
#include "frame_generation/screen_transform.hpp"
#include "frame_generation/scene_rect.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace fg = mhw::frame_generation;
using F16 = std::array<float, 16>;

namespace {
void require(bool ok, const char *what) {
    if (!ok) throw std::runtime_error(what);
}
bool near(double a, double b, double tolerance) { return std::abs(a - b) <= tolerance * (1 + std::abs(b)); }

// DirectX-style perspective P followed by MHW's reversed-Z remap D (z' = w - z,
// row vectors): fNoJitterProj = P*D, written analytically in double because
// forming P*D in float cancels f/(f-n) - 1 and loses ~1e-3 of the far plane.
F16 projection(double fov, double aspect, double n, double f, bool left_handed = true) {
    const double ys = 1 / std::tan(fov / 2), xs = ys / aspect, s = left_handed ? 1 : -1;
    const std::array<double, 16> m{xs, 0, 0, 0, 0, ys, 0, 0, 0, 0, -s * n / (f - n), s, 0, 0, n * f / (f - n), 0};
    F16 r{};
    for (unsigned i = 0; i < 16; ++i) r[i] = static_cast<float>(m[i]);
    return r;
}
// World-to-view for a camera at `eye` rotated by `yaw` about +Y (row vectors).
F16 view(float yaw, std::array<double, 3> eye) {
    const double c = std::cos(yaw), s = std::sin(yaw);
    // Camera axes in world space: right, up, forward (columns of R).
    const std::array<std::array<double, 3>, 3> axes{{{c, 0, -s}, {0, 1, 0}, {s, 0, c}}};
    F16 v{};
    for (unsigned r = 0; r < 3; ++r)
        for (unsigned col = 0; col < 3; ++col) v[r * 4 + col] = static_cast<float>(axes[col][r]);
    for (unsigned col = 0; col < 3; ++col) {
        double t = 0;
        for (unsigned k = 0; k < 3; ++k) t -= eye[k] * axes[col][k];
        v[12 + col] = static_cast<float>(t);
    }
    v[15] = 1;
    return v;
}
std::array<double, 4> transform(std::array<double, 4> p, const std::array<float, 16> &m) {
    std::array<double, 4> r{};
    for (unsigned c = 0; c < 4; ++c)
        for (unsigned k = 0; k < 4; ++k) r[c] += p[k] * m[k * 4 + c];
    return r;
}

void camera_tests() {
    const float fov = 1.0472F, aspect = 16.F / 9.F;
    fg::CameraFrame in;
    in.projection = projection(fov, aspect, 0.25F, 5000.F);
    in.view = view(0.3F, {120.0, 15.0, -40.0});
    in.render_width = 1280;
    in.render_height = 720;
    in.jitter_x = .2F;
    in.jitter_y = -.1F;
    fg::CameraConstants out;
    require(fg::derive_camera_constants(in, out) == fg::CameraFailure::none, "derive");
    require(near(out.near_plane, .25, 1e-4) && near(out.far_plane, 5000, 1e-3) && !out.far_infinite, "near/far");
    require(near(out.fov, fov, 1e-5) && near(out.aspect, aspect, 1e-5) && out.left_handed, "fov/aspect");
    require(out.reset && out.mvec_scale[0] == 1.F / 1280 && out.mvec_scale[1] == 1.F / 720, "first frame/mvec");
    require(near(out.position[0], 120, 1e-5) && near(out.position[1], 15, 1e-5) && near(out.position[2], -40, 1e-5),
            "position");
    require(near(out.forward[0], std::sin(.3), 1e-6) && near(out.forward[2], std::cos(.3), 1e-6) && out.up[1] == 1,
            "basis");
    for (unsigned i = 0; i < 16; ++i)
        require(near(out.clip_to_prev_clip[i], i % 5 == 0 ? 1 : 0, 1e-6), "first frame identity reprojection");

    // Reprojection at large world coordinates: a world point's current clip
    // position times clipToPrevClip must land on its previous clip position.
    const std::array<double, 3> eye{51234.5, 812.25, -73321.75}, previous_eye{51230.0, 812.0, -73325.5};
    in.view = view(.31F, eye);
    in.previous_view = view(.29F, previous_eye);
    in.previous_projection = in.projection;
    in.has_previous = true;
    in.reset = false;
    require(fg::derive_camera_constants(in, out) == fg::CameraFailure::none && !out.reset, "derive with history");
    const std::array<double, 4> world{eye[0] + 30, eye[1] + 2, eye[2] + 40, 1};
    const auto current = transform(transform(world, in.view), in.projection);
    const auto expected = transform(transform(world, in.previous_view), in.previous_projection);
    const auto mapped = transform(current, out.clip_to_prev_clip);
    for (unsigned k = 0; k < 3; ++k)
        require(near(mapped[k] / mapped[3], expected[k] / expected[3], 2e-3), "clip to previous clip");
    const auto back = transform(mapped, out.prev_clip_to_clip);
    for (unsigned k = 0; k < 3; ++k) require(near(back[k] / back[3], current[k] / current[3], 2e-3), "inverse");

    // Infinite reversed-Z projection: clip_z = near, clip_w = z_v.
    fg::CameraFrame infinite = in;
    infinite.has_previous = false;
    const float ys = 1.F / std::tan(fov / 2);
    infinite.projection = {ys / aspect, 0, 0, 0, 0, ys, 0, 0, 0, 0, 0, 1, 0, 0, .5F, 0};
    require(fg::derive_camera_constants(infinite, out) == fg::CameraFailure::none && out.far_infinite &&
                near(out.near_plane, .5, 1e-6),
            "infinite far");

    // Right-handed projection looks down -Z.
    fg::CameraFrame right = in;
    right.has_previous = false;
    right.projection = projection(fov, aspect, .25F, 5000.F, false);
    require(fg::derive_camera_constants(right, out) == fg::CameraFailure::none && !out.left_handed &&
                near(out.forward[2], -std::cos(.31), 1e-5),
            "right handed");

    // Fail closed.
    auto bad = in;
    bad.view[0] *= 1.1F;
    require(fg::derive_camera_constants(bad, out) == fg::CameraFailure::view, "non-orthonormal view");
    bad = in;
    bad.projection[5] = NAN;
    require(fg::derive_camera_constants(bad, out) == fg::CameraFailure::projection, "nan projection");
    bad = in;
    bad.projection[11] = 0;
    bad.projection[15] = 1;
    require(fg::derive_camera_constants(bad, out) == fg::CameraFailure::projection, "orthographic");
    bad = in;
    bad.jitter_x = 3;
    require(fg::derive_camera_constants(bad, out) == fg::CameraFailure::input, "jitter range");
    bad = in;
    bad.previous_view[5] = 2;
    require(fg::derive_camera_constants(bad, out) == fg::CameraFailure::previous, "bad previous");
    bad.reset = true;  // A reset frame does not need a usable previous camera.
    require(fg::derive_camera_constants(bad, out) == fg::CameraFailure::none && out.reset, "reset ignores previous");
}

void ring_tests() {
    using Ring = fg::InputRing<3>;
    Ring ring;
    const auto a = ring.acquire(10, 20, 0);
    require(a != Ring::none && ring.acquire(10, 20, 0) == Ring::none, "duplicate frame");
    require(ring.recorded(a, 100, 1) && !ring.recorded(a, 100, 1), "record once");
    require(ring.take(11, 7) == Ring::none, "unsubmitted slot is not presentable");
    ring.submitted(100, 1, 5);
    require(ring.take(10, 7) == Ring::none, "same-frame present cannot use it");
    require(ring.take(11, 7) == a && ring.slot(a).token == 7, "take");
    ring.presented(a, 42);
    require(ring.slot(a).state == Ring::State::consuming, "consuming");

    // Fill the other two slots; the third acquire starves until the fence.
    const auto b = ring.acquire(11, 21, 0), c = ring.acquire(12, 22, 0);
    require(b != Ring::none && c != Ring::none && ring.acquire(13, 23, 41) == Ring::none, "starved");
    require(ring.counters().starved == 1, "starved counter");
    ring.recorded(b, 101, 1);
    ring.recorded(c, 102, 1);
    ring.submitted(101, 1, 6);
    ring.submitted(102, 1, 6);
    // Present of native 13: newest older ready is 12; 11 missed its Present.
    require(ring.take(13, 8) == c && ring.slot(b).state == Ring::State::free && ring.counters().stale == 1, "stale");
    ring.untag(c);
    require(ring.slot(c).state == Ring::State::free, "untag");
    const auto d = ring.acquire(14, 24, 42);  // Fence 42 completed: slot a reclaimed.
    require(d != Ring::none && ring.counters().reclaimed == 1, "reclaim");
    ring.recorded(d, 103, 2);
    ring.reset(103, 2);
    require(ring.slot(d).state == Ring::State::free && ring.counters().discarded == 1, "premature reset discards");
    const auto e = ring.acquire(15, 25, 42);
    ring.recorded(e, 104, 3);
    ring.submitted(104, 3, 9);
    require(ring.take(16, 9) == e, "take e");
    ring.presented(e, 0);
    require(ring.slot(e).state == Ring::State::free, "zero fence completes immediately");
    const auto f = ring.acquire(16, 26, 0);
    ring.abandon(f);
    require(ring.slot(f).state == Ring::State::free && ring.counters().abandoned == 1, "abandon");
    require(ring.take(20, 10) == Ring::none && ring.counters().no_ready >= 1, "no ready");
}

void screen_tests() {
    namespace sc = fg::screen;
    // Neutral brightness (g = 1, the captured default) is saturate + sRGB.
    require(sc::channel(0.0f, 1.0f) == 0.0f && near(sc::channel(1.0f, 1.0f), 1.0, 1e-6), "endpoints");
    require(near(sc::channel(0.5f, 1.0f), 0.735356, 1e-5), "sRGB mid grey");
    require(near(sc::channel(0.002f, 1.0f), 0.02584, 1e-5), "sRGB linear toe");
    require(sc::channel(-1.0f, 1.0f) == 0.0f && near(sc::channel(7.0f, 1.0f), 1.0, 1e-6), "saturate before the curve");
    // Brightness curve: k = min(0.05 g + 0.95, 1), c' = 1 - (1 - k c)^g.
    require(near(sc::brightness(0.5f, 2.0f), 0.75, 1e-6), "g = 2");
    require(near(sc::brightness(0.5f, 0.5f), 1.0 - std::sqrt(1.0 - 0.975 * 0.5), 1e-6), "g = 0.5 uses k < 1");
    require(sc::brightness(1.0f, 0.5f) < 1.0f, "g < 1 never reaches white");
    float last = -1.0f;
    for (int i = 0; i <= 64; ++i) {
        const float v = sc::channel(i / 64.0f, 1.4f);
        require(v >= last && v >= 0.0f && v <= 1.0f, "monotone in range");
        last = v;
    }
    // Implausible parameters fall back to the neutral curve.
    require(!sc::gamma_plausible(0.0f) && !sc::gamma_plausible(std::nanf("")) && !sc::gamma_plausible(100.0f) &&
                sc::gamma_plausible(1.0f), "gamma window");
    // Every Brightness step measured in game (min 0.1 .. max 3.0) is accepted.
    for (const float g : {0.1f, 0.259212f, 0.519212f, 0.999211f, 1.0f, 1.84f, 2.44f, 3.0f})
        require(sc::gamma_plausible(g), "game brightness steps");
    for (int i = 0; i <= 64; ++i) {
        const float v = sc::channel(i / 64.0f, 0.1f);
        require(v >= 0.0f && v <= 1.0f, "minimum brightness stays in range");
    }
    require(sc::channel(0.5f, std::nanf("")) == sc::channel(0.5f, 1.0f), "fallback");
}
// Back-buffer subrect in Streamline's {top, left, width, height} order.
void subrect_tests() {
    namespace g = fg::game;
    constexpr g::SceneRect pillar16{1280, 0, 2560, 1440}, pillar21{853, 0, 3413, 1440}, letter21{0, 180, 2560, 1080};
    static_assert(g::extent_fields(pillar16) == std::array<unsigned, 4>{0, 1280, 2560, 1440});
    static_assert(g::extent_fields(letter21) == std::array<unsigned, 4>{180, 0, 2560, 1080});
    require(g::inside(pillar16, 5120, 1440) && g::inside(pillar21, 5120, 1440) && g::inside(letter21, 2560, 1440), "scene rectangles inside");
    require(!g::inside({1281, 0, 3840, 1440}, 5120, 1440) && !g::inside({0, 1280, 2560, 1440}, 5120, 1440) && !g::inside({0, 0, 0, 1440}, 5120, 1440),
            "rectangles outside or empty");
}
}  // namespace

int main() {
    try {
        camera_tests();
        ring_tests();
        screen_tests();
        subrect_tests();
    } catch (const std::exception &e) {
        std::fprintf(stderr, "failed: %s\n", e.what());
        return 1;
    }
    return 0;
}
