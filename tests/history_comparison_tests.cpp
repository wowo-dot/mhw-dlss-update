#include "../src/continuous_dlaa/history.hpp"
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
using namespace mhw::continuous_dlaa;
void check(bool ok) {
    if (!ok) throw std::runtime_error("distance-only history comparison");
}
std::array<float, 16> view(double x, double y, double z, double yaw = 0) {
    const auto c = std::cos(yaw), s = std::sin(yaw);
    return {float(c),
            0,
            float(s),
            0,
            0,
            1,
            0,
            0,
            float(-s),
            0,
            float(c),
            0,
            float(-x * c + z * s),
            float(-y),
            float(-x * s - z * c),
            1};
}
HistoryStamp stamp() {
    HistoryStamp s{100, 100, 1000000, 1, 2, 3, {2560, 1440}, {}};
    s.view_matrix = view(0, 0, 0);
    return s;
}
HistoryStamp next(HistoryStamp s) {
    ++s.frame;
    ++s.epoch;
    s.microseconds += 33333;
    return s;
}
HistoryDecision compared(const HistoryStamp &a, const HistoryStamp &b) {
    const auto control = history_decision(a, b), test = history_decision(a, b, HistoryPolicy::distance_only_comparison);
    check(test.valid == control.valid);
    if (!test.valid) return test;
    const bool remove = control.reasons == reason_bit(ResetReason::camera_displacement);
    check(test.control_reasons == control.reasons && test.displacement_only_suppressed == remove);
    check(test.reasons == (remove ? 0 : control.reasons) && test.reset == bool(test.reasons));
    check(test.camera_distance == control.camera_distance && test.previous_camera == control.previous_camera &&
          test.next_camera == control.next_camera && test.milliseconds == control.milliseconds &&
          test.projection_mask == control.projection_mask && test.view_jump_mask == control.view_jump_mask);
    return test;
}
bool read_stamp(HistoryStamp &s) {
    if (!(std::cin >> s.frame)) return false;
    check(
        bool(std::cin >> s.epoch >> s.microseconds >> s.camera >> s.view >> s.depth >> s.viewport[0] >> s.viewport[1]));
    for (auto &v : s.view_matrix) {
        unsigned bits{};
        check(bool(std::cin >> bits));
        v = std::bit_cast<float>(bits);
    }
    for (auto &v : s.projection) check(bool(std::cin >> v));
    return true;
}
int main(int argc, char **argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--replay") {
        HistoryStamp a, b;
        while (read_stamp(a)) {
            check(read_stamp(b));
            const auto d = compared(a, b);
            std::cout << d.valid << ' ' << d.reset << ' ' << d.reasons << ' ' << d.control_reasons << ' '
                      << d.displacement_only_suppressed << ' ' << std::setprecision(17) << d.camera_distance << ' '
                      << d.milliseconds << '\n';
        }
        return 0;
    }
    const auto a = stamp();
    auto b = next(a);
    check(compared({}, a).reasons == 1 && compared(a, b).reasons == 0);
    for (auto distance : {0., 19.99, 20., 20.01, 30., 100., 10000.}) {
        b = next(a);
        b.view_matrix = view(distance, 0, 0);
        const auto d = compared(a, b);
        check(!d.reset && d.displacement_only_suppressed == (distance > 20.));
    }
    // A translation-only teleport is deliberately retained by this diagnostic.
    // It must never be advertised as a finished camera-cut policy.
    for (unsigned reason = 1; reason < 10; ++reason) {
        b = next(a);
        b.view_matrix = view(100, 0, 0);
        switch (static_cast<ResetReason>(reason)) {
        case ResetReason::frame_gap: ++b.frame; break;
        case ResetReason::epoch_gap: ++b.epoch; break;
        case ResetReason::camera: ++b.camera; break;
        case ResetReason::view: ++b.view; break;
        case ResetReason::depth: ++b.depth; break;
        case ResetReason::viewport: ++b.viewport[0]; break;
        case ResetReason::projection: ++b.projection[0]; break;
        case ResetReason::interval: b.microseconds = a.microseconds + 250001; break;
        case ResetReason::view_other: b.view_matrix = view(100, 0, 0, 1); break;
        default: throw std::runtime_error("reason");
        }
        const auto d = compared(a, b);
        check(d.reset && !d.displacement_only_suppressed && d.reasons == ((1U << reason) | 1024U));
    }
    b = next(a);
    b.view_matrix[0] = 2;
    check(!compared(a, b).valid && !compared(b, next(b)).valid);
    for (unsigned i = 0; i < 16; ++i) {
        b = next(a);
        b.view_matrix[i] = std::numeric_limits<float>::quiet_NaN();
        check(!compared(a, b).valid);
    }
    b = a;
    check(!compared(a, b).valid);
    b = next(a);
    b.microseconds = a.microseconds;
    check(!compared(a, b).valid);
    // At each frame rate the same smoothly orbiting camera is valid, including
    // samples that exceed the old per-frame distance heuristic.
    for (unsigned fps : {15U, 30U, 60U, 120U, 240U}) {
        auto before = a;
        before.view_matrix = view(500, 0, 0);
        unsigned suppressed{};
        for (unsigned i = 1; i <= fps * 3; ++i) {
            const double angle = 1.5 * i / fps;
            auto after = next(before);
            after.microseconds = before.microseconds + 1000000 / fps;
            after.view_matrix = view(500 * std::cos(angle), 0, 500 * std::sin(angle), angle);
            const auto d = compared(before, after);
            check(d.valid && !d.reset);
            suppressed += d.displacement_only_suppressed;
            before = after;
        }
        check((suppressed > 0) == (fps <= 30));
    }
    HistoryTelemetry telemetry{HistoryPolicy::distance_only_comparison};
    auto before = a;
    telemetry.record({}, before, compared({}, before));
    for (unsigned i = 1; i <= 120; ++i) {
        auto after = next(before);
        after.view_matrix = view(i * 30., 0, 0);
        if (i % 40 == 0) ++after.frame;
        telemetry.record(before, after, compared(before, after));
        before = after;
    }
    check(telemetry.evaluations == 121 && telemetry.resets == 4 && telemetry.retained == 117 &&
          telemetry.displacement_only_resets_suppressed == 117);
    check(telemetry.counts[10] == 3 && telemetry.counts[1] == 3);
    std::ostringstream out;
    telemetry.write(out);
    check(out.str().size() < 28000);
    static_assert(sizeof(HistoryTelemetry) < 8192);
    if (argc == 2 && std::string_view(argv[1]) == "--json")
        std::cout << out.str() << '\n';
    else
        std::cout
            << "PASS history comparison: isolated displacement, all retained cut reasons, invalid matrices, orbit frame rates and bounded telemetry\n";
}
