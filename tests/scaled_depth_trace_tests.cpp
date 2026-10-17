#include "../src/continuous_dlaa/scaled_depth_trace.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>
using mhw::continuous_dlaa::ScaledDepthTrace;
void check(bool ok, const char *s) {
    if (!ok) {
        std::cerr << s << '\n';
        std::exit(1);
    }
}
int main() {
    ScaledDepthTrace t;
    for (unsigned i = 0; i < 10000; ++i) t.observe({i + 1, 9, 9, i, 0, 0, {}});
    t.observe({10001, 10, 10, 3, 1, 0, {123}});
    t.observe({10002, 10, 11, 4, 4, 6, {456}}); // Same frame, wrong epoch.
    t.observe({10003, 10, 10, 5, 4, 6, {789}});
    t.missing({10004, 10, 10, 8, 5, 100, 200, 300}, {});
    const auto first = t.samples[0];
    check(first.matching == 2 && first.saved == 2 && first.events[0].depth_sequence == 3 &&
              first.events[1].values[0] == 789,
          "exact frame/epoch correlation after ring wrap");
    check(first.ring_overwritten > 0, "lost earlier history explicitly identified");
    for (unsigned i = 0; i < 70; ++i) t.observe({10005 + i, 12, 12, i, 4, 0, {}});
    t.missing({11000, 12, 12, 9, 69, 101, 201, 301}, {});
    check(t.samples[1].matching == 70 && t.samples[1].saved == 32 && t.samples[1].events[30].depth_sequence == 30 &&
              t.samples[1].events[31].depth_sequence == 69,
          "bounded first/latest frame events");
    for (unsigned i = 0; i < 1000; ++i) t.missing({12000 + i, 100 + i, 100 + i, 9, 0, 102, 202, 302}, {});
    check(t.samples[0].ordinal == 1 && t.samples[0].events[0].values[0] == 123 && t.samples[4].ordinal == 1002,
          "first failures survive later overwrites; latest remains available");
    check(t.samples[4].saved == 0 && t.samples[4].matching == 0,
          "missing writer evidence is not borrowed from another frame");
    t.observe({1, 0, 0, 0, 0, 1, {}});
    check(t.uncorrelated == 1 && t.counts[0][1] == 1, "unstamped rejection is explicit");
    std::ostringstream s;
    t.write(s);
    check(s.str().size() < 70000, "bounded serialized trace");
    std::cout
        << "Scaled depth trace: frame/epoch isolation, overwritten history, bounded first/latest evidence passed\n";
}
