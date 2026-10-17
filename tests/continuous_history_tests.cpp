#include "../src/continuous_dlaa/history.hpp"
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace mhw::continuous_dlaa;
void require(bool ok) {
    if (!ok) throw std::runtime_error("continuous camera history policy");
}
using V = std::array<double, 3>;
std::array<float, 16> pose(V center, double yaw = 0, double pitch = 0) {
    const double cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const double y[3][3]{{cy, 0, sy}, {0, 1, 0}, {-sy, 0, cy}}, p[3][3]{{1, 0, 0}, {0, cp, -sp}, {0, sp, cp}};
    std::array<float, 16> m{};
    m[15] = 1;
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j) {
            double v{};
            for (unsigned k = 0; k < 3; ++k) v += y[i][k] * p[k][j];
            m[4 * i + j] = static_cast<float>(v);
        }
    for (unsigned j = 0; j < 3; ++j) {
        double t{};
        for (unsigned i = 0; i < 3; ++i) t -= center[i] * m[4 * i + j];
        m[12 + j] = static_cast<float>(t);
    }
    return m;
}
HistoryStamp initial() {
    HistoryStamp a{100, 200, 1000000, 1, 2, 3, {2560, 1440}, {}};
    a.view_matrix = pose({0, 0, 0});
    return a;
}
HistoryStamp advance(HistoryStamp a) {
    ++a.frame;
    ++a.epoch;
    a.microseconds += 17000;
    return a;
}
int main(int argc, char **argv) {
    // Feed only typed matrix words from archived local examples; no game assets.
    if (argc == 2 && std::string(argv[1]) == "--replay") {
        for (;;) {
            auto a = initial(), b = advance(a);
            unsigned bits{};
            if (!(std::cin >> bits)) break;
            a.view_matrix[0] = std::bit_cast<float>(bits);
            for (unsigned i = 1; i < 32; ++i) {
                require(bool(std::cin >> bits));
                (i < 16 ? a.view_matrix[i] : b.view_matrix[i - 16]) = std::bit_cast<float>(bits);
            }
            const auto d = history_decision(a, b);
            std::cout << std::setprecision(17) << d.valid << ' ' << d.reset << ' ' << d.reasons << ' '
                      << d.camera_distance << ' ' << d.view_jump_mask << '\n';
        }
        return 0;
    }
    const auto a = initial(), b = advance(a);
    const auto first = history_decision({}, a);
    require(first.valid && first.reset && first.reasons == 1);
    const auto normal = history_decision(a, b);
    require(normal.valid && !normal.reset && normal.milliseconds == 17);
    for (unsigned i = 1; i < reset_reason_count; ++i) {
        auto test = b;
        switch (static_cast<ResetReason>(i)) {
        case ResetReason::frame_gap: ++test.frame; break;
        case ResetReason::epoch_gap: ++test.epoch; break;
        case ResetReason::camera: ++test.camera; break;
        case ResetReason::view: ++test.view; break;
        case ResetReason::depth: ++test.depth; break;
        case ResetReason::viewport: ++test.viewport[1]; break;
        case ResetReason::projection: ++test.projection[10]; break;
        case ResetReason::interval: test.microseconds = a.microseconds + 250001; break;
        case ResetReason::view_other: test.view_matrix = pose({0, 0, 0}, 1); break;
        case ResetReason::camera_displacement: test.view_matrix = pose({0, 0, 20.00001}); break;
        default: throw std::runtime_error("reason fixture");
        }
        const auto d = history_decision(a, test);
        require(d.valid && d.reasons == (1U << i));
    }
    // FOV and clip-plane animation keeps history (game-38: a reset on every
    // windrake frame stopped FG); a jump over 10% or a structural change cuts.
    {
        auto p = a;
        const std::array<float, 16> lens{1.35F, 0, 0, 0, 0, 2.4F, 0, 0, 0, 0, 1e-5F, -1, 0, 0, 16, 0};
        for (unsigned i = 0; i < 16; ++i) p.projection[i] = std::bit_cast<unsigned>(lens[i]);
        const auto with = [&](unsigned i, float v) {
            auto s = advance(p);
            s.projection[i] = std::bit_cast<unsigned>(v);
            return history_decision(p, s);
        };
        require(!history_decision(p, advance(p)).reset);
        for (const float scale : {1.0001F, 1.005F, .95F, 1.09F, .91F}) {
            const auto d = with(5, 2.4F * scale);
            require(d.valid && !d.reset && d.projection_mask == 32);
        }
        require(!with(14, 15.9F).reset && !with(10, 1.05e-5F).reset);
        for (const float v : {2.4F * 1.12F, 2.4F * .88F, -2.4F, 0.F, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()})
            require(with(5, v).reasons == reason_bit(ResetReason::projection));
        require(with(8, .01F).reasons == reason_bit(ResetReason::projection)); // an off-centre term appears
        HistoryTelemetry kept;
        kept.record(p, advance(p), with(0, 1.36F));
        require(kept.retained == 1 && kept.projection_changes_retained == 1 && !kept.resets);
    }
    auto test = b;
    test.view_matrix = pose({20, 0, 0});
    test.microseconds = a.microseconds + 250000;
    require(!history_decision(a, test).reset);
    test.view_matrix = pose({20.00001, 0, 0});
    require(history_decision(a, test).reset);
    test = b;
    test.view_matrix = pose({15, 15, 0});
    require(history_decision(a, test).reasons == reason_bit(ResetReason::camera_displacement));
    test = b;
    test.view_matrix = pose({21, 0, 0}, 1);
    const auto both = history_decision(a, test);
    require(both.reasons == (reason_bit(ResetReason::view_other) | reason_bit(ResetReason::camera_displacement)));
    test = b;
    test.frame = a.frame;
    require(!history_decision(a, test).valid);
    test = b;
    test.microseconds = a.microseconds;
    require(!history_decision(a, test).valid);
    test = b;
    test.microseconds += 2000000;
    require(history_decision(a, test).milliseconds == 1000);
    // Finite alone is insufficient: singular, scaled, reflected, sheared and
    // projective transforms cannot silently retain history.
    for (unsigned kind = 0; kind < 7; ++kind) {
        test = b;
        if (kind == 0) test.view_matrix = {};
        if (kind == 1) test.view_matrix[0] = 2;
        if (kind == 2) test.view_matrix[0] = -1;
        if (kind == 3) test.view_matrix[1] = .1F;
        if (kind == 4) test.view_matrix[3] = .001F;
        if (kind == 5) test.view_matrix[15] = 2;
        if (kind == 6) test.view_matrix[12] = std::numeric_limits<float>::infinity();
        require(!history_decision(a, test).valid && !history_decision(test, b).valid);
    }
    for (unsigned i = 0; i < 16; ++i) {
        test = b;
        test.view_matrix[i] = std::numeric_limits<float>::quiet_NaN();
        require(!history_decision(a, test).valid);
    }
    // Rotation at a fixed, distant world position must not manufacture a cut.
    auto stationary = a;
    stationary.view_matrix = pose({10000, 0, 0});
    test = b;
    test.view_matrix = pose({10000, 0, 0}, .01);
    const auto rotate = history_decision(stationary, test);
    require(rotate.valid && !rotate.reset && rotate.camera_distance < .01 && (rotate.view_jump_mask & 0x7000));
    // Ordinary orbit motion and a real translation cut at the same position.
    test.view_matrix = pose({10002, 0, 0}, .01);
    require(!history_decision(stationary, test).reset);
    test.view_matrix = pose({10030, 0, 0}, .01);
    require(history_decision(stationary, test).reasons == reason_bit(ResetReason::camera_displacement));
    std::mt19937 rng(1182);
    std::uniform_real_distribution<double> coordinate(-100000, 100000), angle(-3.14, 3.14);
    for (unsigned n = 0; n < 100000; ++n) {
        const V c{coordinate(rng), coordinate(rng), coordinate(rng)};
        const auto yaw = angle(rng), pitch = angle(rng);
        auto before = a, after = b;
        before.view_matrix = pose(c, yaw, pitch);
        after.view_matrix = pose(c, yaw + .001, pitch - .001);
        V recovered{};
        require(camera_position(before.view_matrix, recovered));
        for (unsigned j = 0; j < 3; ++j) require(std::abs(recovered[j] - c[j]) < .03);
        const auto d = history_decision(before, after);
        require(d.valid && !d.reset && d.camera_distance < .05);
        auto moved = c;
        moved[0] += 30;
        after.view_matrix = pose(moved, yaw + .001, pitch - .001);
        require(history_decision(before, after).reasons == reason_bit(ResetReason::camera_displacement));
    }
    HistoryTelemetry telemetry;
    telemetry.record({}, a, first);
    auto previous = a;
    V center{};
    double yaw{};
    for (unsigned n = 0; n < 10000; ++n) {
        auto next = advance(previous);
        if (n == 0) {
            next.frame += 2;
            next.epoch += 2;
            ++next.camera;
            ++next.view;
            ++next.depth;
            ++next.viewport[0];
            ++next.projection[5];
            next.microseconds += 500000;
            center[0] += 25;
            yaw = 1;
        }
        if (n % 100 == 99) center[0] += 25;
        next.view_matrix = pose(center, yaw);
        telemetry.record(previous, next, history_decision(previous, next));
        previous = next;
    }
    auto next = advance(previous);
    next.view_matrix = pose({10000, 0, 0});
    telemetry.record(previous, next, history_decision(previous, next));
    previous = next;
    next = advance(previous);
    next.view_matrix = pose({10000, 0, 0}, .01);
    telemetry.record(previous, next, history_decision(previous, next));
    require(telemetry.evaluations == 10003 && telemetry.resets == 103 && telemetry.retained == 9900 &&
            telemetry.counts[0] == 1 && telemetry.counts[10] == 102 && telemetry.longest_retained_streak == 99);
    require(telemetry.translation_resets_avoided == 1 && telemetry.first_avoided.next.frame == 10104);
    const auto count = telemetry.evaluations;
    telemetry.record({}, a, {});
    require(telemetry.evaluations == count);
    std::ostringstream out;
    telemetry.write(out);
    require(out.str().size() < 24000);
    static_assert(sizeof(HistoryTelemetry) < 8192);
    if (argc > 1) std::cout << out.str() << '\n';
}
