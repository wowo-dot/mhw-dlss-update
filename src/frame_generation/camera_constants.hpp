#pragma once
#include <array>
#include <cmath>
#include <cstdint>

// Frame-generation camera constants derived from MHW's captured matrices.
// Pure math with no SDK types; the Streamline adapter copies the fields.
//
// Conventions (docs/research/view-projection-contract.md): row vectors,
// row-major storage, view = world-to-view, projection = fNoJitterProj = P*D,
// where D maps NDC depth z to 1-z (reversed Z) and is its own inverse.
namespace mhw::frame_generation {

using Matrix = std::array<double, 16>;

struct CameraFrame {
    std::array<float, 16> view{}, projection{};  // current fView, fNoJitterProj
    std::array<float, 16> previous_view{}, previous_projection{};
    bool has_previous{};
    float jitter_x{}, jitter_y{};  // render pixels, +X right, +Y down (NGX convention)
    unsigned render_width{}, render_height{};
    bool reset{};
};

struct CameraConstants {
    std::array<float, 16> view_to_clip{}, clip_to_view{}, clip_to_prev_clip{}, prev_clip_to_clip{};
    std::array<float, 2> jitter{}, mvec_scale{}, pinhole{};
    std::array<float, 3> position{}, up{}, right{}, forward{};
    float near_plane{}, far_plane{}, fov{}, aspect{};
    bool far_infinite{}, left_handed{}, reset{};
};

enum class CameraFailure : unsigned { none, input, view, projection, previous, inverse, range };

namespace camera_detail {

inline Matrix widen(const std::array<float, 16> &m) {
    Matrix r{};
    for (unsigned i = 0; i < 16; ++i) r[i] = m[i];
    return r;
}

inline std::array<float, 16> narrow(const Matrix &m) {
    std::array<float, 16> r{};
    for (unsigned i = 0; i < 16; ++i) r[i] = static_cast<float>(m[i]);
    return r;
}

inline Matrix multiply(const Matrix &a, const Matrix &b) {
    Matrix r{};
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned col = 0; col < 4; ++col) {
            double sum = 0;
            for (unsigned k = 0; k < 4; ++k) sum += a[row * 4 + k] * b[k * 4 + col];
            r[row * 4 + col] = sum;
        }
    return r;
}

// Gauss-Jordan with partial pivoting; false for singular or non-finite input.
inline bool invert(const Matrix &m, Matrix &out) {
    std::array<double, 32> a{};
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c) {
            a[r * 8 + c] = m[r * 4 + c];
            a[r * 8 + 4 + c] = r == c ? 1 : 0;
        }
    for (unsigned col = 0; col < 4; ++col) {
        unsigned pivot = col;
        for (unsigned r = col + 1; r < 4; ++r)
            if (std::abs(a[r * 8 + col]) > std::abs(a[pivot * 8 + col])) pivot = r;
        const double p = a[pivot * 8 + col];
        if (!std::isfinite(p) || std::abs(p) < 1e-12) return false;
        if (pivot != col)
            for (unsigned c = 0; c < 8; ++c) std::swap(a[pivot * 8 + c], a[col * 8 + c]);
        for (unsigned c = 0; c < 8; ++c) a[col * 8 + c] /= p;
        for (unsigned r = 0; r < 4; ++r) {
            if (r == col) continue;
            const double f = a[r * 8 + col];
            for (unsigned c = 0; c < 8; ++c) a[r * 8 + c] -= f * a[col * 8 + c];
        }
    }
    for (unsigned r = 0; r < 4; ++r)
        for (unsigned c = 0; c < 4; ++c) out[r * 4 + c] = a[r * 8 + 4 + c];
    for (double v : out)
        if (!std::isfinite(v)) return false;
    return true;
}

inline bool finite(const std::array<float, 16> &m) {
    for (float v : m)
        if (!std::isfinite(v)) return false;
    return true;
}

// Affine row-vector view: rotation R in the upper 3x3 (rows orthonormal,
// det +1), translation t in row 3. Camera centre C satisfies C*R + t = 0.
struct View {
    std::array<std::array<double, 3>, 3> rotation{};
    std::array<double, 3> centre{};
};

inline bool decode_view(const std::array<float, 16> &m, View &v) {
    if (!finite(m) || m[3] != 0 || m[7] != 0 || m[11] != 0 || m[15] != 1) return false;
    for (unsigned r = 0; r < 3; ++r)
        for (unsigned c = 0; c < 3; ++c) v.rotation[r][c] = m[r * 4 + c];
    const auto &R = v.rotation;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j) {
            double dot = 0;
            for (unsigned k = 0; k < 3; ++k) dot += R[i][k] * R[j][k];
            if (std::abs(dot - (i == j ? 1. : 0.)) > 1e-4) return false;
        }
    const double det = R[0][0] * (R[1][1] * R[2][2] - R[1][2] * R[2][1]) -
                       R[0][1] * (R[1][0] * R[2][2] - R[1][2] * R[2][0]) +
                       R[0][2] * (R[1][0] * R[2][1] - R[1][1] * R[2][0]);
    if (std::abs(det - 1.) > 1e-4) return false;
    // R is orthonormal, so C = -t * R^T.
    for (unsigned j = 0; j < 3; ++j) {
        double sum = 0;
        for (unsigned k = 0; k < 3; ++k) sum += -double(m[12 + k]) * R[j][k];
        v.centre[j] = sum;
    }
    for (double c : v.centre)
        if (!std::isfinite(c)) return false;
    return true;
}

// View-space depth z_v that a perspective projection maps to NDC depth d:
// clip_z = z_v*M22 + M32, clip_w = z_v*M23 + M33.
inline double depth_at(const Matrix &m, double d) {
    const double denominator = m[10] - d * m[11];
    return denominator == 0 ? INFINITY : (d * m[15] - m[14]) / denominator;
}

}  // namespace camera_detail

// Fails closed: on any inconsistency the caller sends no FG constants.
inline CameraFailure derive_camera_constants(const CameraFrame &in, CameraConstants &out) {
    using namespace camera_detail;
    out = {};
    if (!in.render_width || !in.render_height || !std::isfinite(in.jitter_x) || !std::isfinite(in.jitter_y) ||
        std::abs(in.jitter_x) > 1.F || std::abs(in.jitter_y) > 1.F)
        return CameraFailure::input;
    View view, previous;
    if (!decode_view(in.view, view)) return CameraFailure::view;
    if (!finite(in.projection)) return CameraFailure::projection;
    const Matrix projection = widen(in.projection);
    // Perspective, not orthographic: w = +-z_v, no constant w term.
    const double w_axis = projection[11];
    if (std::abs(std::abs(w_axis) - 1.) > 1e-5 || std::abs(projection[15]) > 1e-6 || projection[3] != 0 ||
        projection[7] != 0 || !(projection[0] > 0) || !(projection[5] > 0))
        return CameraFailure::projection;
    Matrix clip_to_view;
    if (!invert(projection, clip_to_view)) return CameraFailure::inverse;

    // Reversed Z: NDC depth 1 is the near plane, 0 the far plane.
    const double near_v = depth_at(projection, 1.), far_v = depth_at(projection, 0.);
    const double near_plane = std::abs(near_v), far_plane = std::abs(far_v);
    if (!std::isfinite(near_plane) || !(near_plane > 0) || (std::isfinite(far_plane) && !(far_plane > near_plane)))
        return CameraFailure::range;
    out.near_plane = static_cast<float>(near_plane);
    out.far_infinite = !std::isfinite(far_plane) || far_plane > 1e9;
    out.far_plane = out.far_infinite ? 1e9F : static_cast<float>(far_plane);
    out.fov = static_cast<float>(2. * std::atan(1. / projection[5]));
    out.aspect = static_cast<float>(projection[5] / projection[0]);
    out.pinhole = {static_cast<float>(projection[8]), static_cast<float>(projection[9])};
    out.left_handed = w_axis > 0;

    // Camera basis: columns of the view rotation are the camera axes in world
    // space. A right-handed view looks down -Z.
    const auto &R = view.rotation;
    for (unsigned k = 0; k < 3; ++k) {
        out.right[k] = static_cast<float>(R[k][0]);
        out.up[k] = static_cast<float>(R[k][1]);
        out.forward[k] = static_cast<float>(out.left_handed ? R[k][2] : -R[k][2]);
        out.position[k] = static_cast<float>(view.centre[k]);
    }

    Matrix clip_to_prev = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    if (in.has_previous && !in.reset) {
        if (!decode_view(in.previous_view, previous) || !finite(in.previous_projection))
            return CameraFailure::previous;
        // Camera-centred view-to-previous-view: [R^T Rp, 0; (C - Cp) Rp, 1].
        // Subtracting centres in double keeps large world coordinates exact.
        Matrix view_to_prev{};
        const auto &P = previous.rotation;
        for (unsigned r = 0; r < 3; ++r)
            for (unsigned c = 0; c < 3; ++c) {
                double sum = 0;
                for (unsigned k = 0; k < 3; ++k) sum += R[k][r] * P[k][c];
                view_to_prev[r * 4 + c] = sum;
            }
        for (unsigned c = 0; c < 3; ++c) {
            double sum = 0;
            for (unsigned k = 0; k < 3; ++k) sum += (view.centre[k] - previous.centre[k]) * P[k][c];
            view_to_prev[12 + c] = sum;
        }
        view_to_prev[15] = 1;
        clip_to_prev = multiply(multiply(clip_to_view, view_to_prev), widen(in.previous_projection));
    }
    Matrix prev_to_clip;
    if (!invert(clip_to_prev, prev_to_clip)) return CameraFailure::inverse;

    out.view_to_clip = in.projection;
    out.clip_to_view = narrow(clip_to_view);
    out.clip_to_prev_clip = narrow(clip_to_prev);
    out.prev_clip_to_clip = narrow(prev_to_clip);
    out.jitter = {in.jitter_x, in.jitter_y};
    out.mvec_scale = {1.F / static_cast<float>(in.render_width), 1.F / static_cast<float>(in.render_height)};
    out.reset = in.reset || !in.has_previous;
    return CameraFailure::none;
}

}  // namespace mhw::frame_generation
