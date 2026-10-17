#include "../src/scaled_temporal/output_diagnostics.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
using Trace = mhw::scaled_temporal::OutputDiagnostics;
void check(bool v, const char *why) {
    if (!v) {
        std::cerr << why << '\n';
        std::exit(1);
    }
}
std::string serialize(const Trace &t) {
    std::ostringstream s;
    s << std::boolalpha;
    t.write(s);
    return s.str();
}
void emit(const char *name, const Trace &t) {
    std::cout << "{\"case\":\"" << name << "\",\"trace\":" << serialize(t) << "}\n";
}
#include "output_commands_fixture.hpp"
int main() {
    command_cases();
    Trace t;
    emit("empty", t);
    Trace::Graph g;
    g.at = {100, 7, 7, 10, 20, 30, 0, 1};
    g.count = 1;
    g.filters[0] = {40, 15, 100, 50, 60, 70, 80};
    g.link_reads = 15;
    g.links = {50, 90, 100, 70, 80, 80, 110, 110};
    t.graph(g, true);
    for (unsigned i = 0; i < 10; ++i) {
        g.reason = 6;
        g.detail = 4;
        g.link_reads = 7;
        g.links[5] = 81;
        t.graph(g, false);
    }
    auto text = serialize(t);
    check(text.find("\"graph_rejected\":10") != text.npos, "all failures counted");
    check(text.find("\"ordinal\":10,") != text.npos && text.find("\"ordinal\":9,") == text.npos,
          "bounded graph first/latest");
    emit("graph", t);
    Trace geometry;
    auto rejected = g;
    rejected.reason = 17;
    rejected.detail = 0;
    rejected.qualification = (5U << 16) | 3U;
    rejected.vertex_gate = {1000, 2000, 10, 4, 0, 1, 8, 0, 0, 2};
    geometry.graph(rejected, false);
    emit("vertex-gate", geometry);
    Trace d;
    std::array<Trace::Draw, 4> draws{};
    draws[1] = {101, 12, 2, 30, 40, 50, 102, 1, 1, 7, 8};
    const std::array<std::uint64_t, 4> packets{101, 102, 103};
    const std::array<std::array<std::uint64_t, 2>, 2> early{{{12, 1}, {12, 2}}};
    d.submit(500, 102, early);
    d.order(7, 7, 103, 2, 11, 1, packets, draws);
    emit("prior-submission", d);
    // Same pointer with a different generation must not acquire the old receipt.
    auto later = draws[1];
    later[1] = 12;
    later[2] = 3;
    later[6] = 101;
    later[7] = 0;
    d.draw(7, 7, 0, later);
    emit("wrong-generation", d);
    const std::array<std::array<std::uint64_t, 2>, 3> batch{{{11, 1}, {12, 3}, {13, 4}}};
    d.submit(500, 105, batch);
    later[1] = 13;
    later[2] = 4;
    later[6] = 103;
    later[7] = 1;
    d.draw(7, 7, 2, later);
    emit("late-callback", d);
    const auto unchanged = serialize(d);
    later[6] = 999;
    d.draw(7, 7, 2, later);
    d.draw(8, 7, 0, later);
    check(serialize(d) == unchanged, "unrelated packet/frame cannot update evidence");
    // History overwrite never fabricates a receipt for a generation it no longer stores.
    for (unsigned i = 0; i < 140; ++i) {
        const std::array<std::array<std::uint64_t, 2>, 1> b{{{20, 100 + i}}};
        d.submit(600, 200 + i, b);
    }
    for (unsigned i = 0; i < 10; ++i) d.order(8 + i, 8 + i, 500 + i, 2, 11, 1, packets, draws);
    emit("wrapped", d);
    check(serialize(d).size() < 16000, "bounded output");
}
