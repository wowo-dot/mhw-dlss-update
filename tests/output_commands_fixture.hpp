#pragma once
#include <cstring>
#include <vector>
// No game process. Simulated queues exercise bounded capture, unreadable
// packets, duplicate draws, truncation and a changing completed-queue stamp.
void command_cases() {
    using Commands = mhw::scaled_temporal::OutputCommands;
    Trace::Graph g;
    g.at = {100, 7, 7, 0x1000, 20, 30, 0, 1};
    g.count = 1;
    g.filters[0] = {40, 15, 4100, 50, 60, 0, 80};
    g.reason = 17;
    g.command_gate = {Commands::multiple_draws, 0x78001004, 0x6000, 39, 0, 1, 1, 1};
    std::vector<std::byte> memory(0x10000);
    const auto put = [&](std::size_t at, const auto &v) { std::memcpy(memory.data() + at, &v, sizeof(v)); };
    put(0x1000 + 0x2b78, std::uint64_t{0x4000});
    put(0x1000 + 0x2b80, 40U);
    for (unsigned i = 0; i < 40; ++i) {
        put(0x4000 + 16 * i, 0x78001004U);
        put(0x4008 + 16 * i, std::uint64_t{0x5000 + 80 * i});
        put(0x5000 + 80 * i, i == 39 ? 13U : i ? 0U : 11U);
    }
    g.command_gate[2] = 0x5000 + 80 * 39;
    g.command_gate[4] = 13;
    g.command_gate[0] = Commands::unsupported_opcode;
    unsigned reads{};
    const auto read = [&](std::uint64_t at, void *out, std::size_t bytes) {
        ++reads;
        if (at > memory.size() || bytes > memory.size() - at) return false;
        std::memcpy(out, memory.data() + at, bytes);
        return true;
    };
    Trace t;
    t.commands.observe(g, read, [] { return true; });
    t.graph(g, false);
    emit("commands-truncated", t);
    const auto &s = t.commands.sample(0);
    check(s.stable && s.matched == 40 && s.count == 32 && s.has_focus && s.focus.index == 39,
          "late rejected packet survives bounded truncation");
    check(s.opcodes[0] == 38 && s.opcodes[11] == 1 && s.opcodes[13] == 1,
          "complete histogram includes packets after rejection");
    const auto old_reads = reads;
    g.at[1]++;
    t.commands.observe(g, read, [] { return true; });
    check(reads == old_reads && t.commands.sample(0).occurrences == 2, "repeat rejection performs no queue reads");
    emit("commands-repeat", t);
    Trace changed;
    changed.commands.observe(g, read, [] { return false; });
    check(!changed.commands.sample(0).stable, "changing stamp is not stable evidence");
    emit("commands-changing", changed);
    put(0x4008, std::uint64_t{0xfffffff0});
    Trace unreadable;
    unreadable.commands.observe(g, read, [] { return true; });
    check(unreadable.commands.sample(0).unreadable == 1 && !unreadable.commands.sample(0).packets[0].raw_read,
          "unreadable opcode is not opcode zero");
    emit("commands-unreadable", unreadable);
    // Header readable, optional prefix unreadable: still record the actual opcode.
    put(0x4008, std::uint64_t{0xfffc});
    put(0xfffc, 7U);
    Trace short_packet;
    short_packet.commands.observe(g, read, [] { return true; });
    check(short_packet.commands.sample(0).packets[0].raw_read &&
              !short_packet.commands.sample(0).packets[0].prefix_read,
          "prefix read failure preserves opcode");
    emit("commands-short-prefix", short_packet);
    for (unsigned i = 1; i < 23; ++i) {
        if (i == 11) continue;
        g.command_gate[4] = i;
        t.commands.observe(g, read, [] { return true; });
    }
    check(t.commands.size() == 16, "distinct rejection capacity bounded");
    emit("commands-capacity", t);
    Trace invalid;
    put(0x1000 + 0x2b80, 65535U);
    invalid.commands.observe(g, read, [] { return true; });
    check(invalid.commands.sample(0).error == 1 && !invalid.commands.sample(0).scanned,
          "oversized queue not traversed");
    emit("commands-invalid", invalid);
    Trace missing;
    put(0x1000 + 0x2b80, 40U);
    put(0x1000 + 0x2b78, std::uint64_t{0xfff0});
    missing.commands.observe(g, read, [] { return true; });
    check(missing.commands.sample(0).error == 2, "unreadable command block retained");
    emit("commands-block", missing);
    Trace gallery;
    put(0x1000 + 0x2b78, std::uint64_t{0x4000});
    g.reason = 6;
    g.detail = 4;
    g.command_gate = {};
    gallery.commands.observe(g, read, [] { return true; });
    check(gallery.commands.size() == 1, "unknown color chain observed without admission");
    emit("commands-gallery", gallery);
    // Whole-plan dependency rejections have no selected packet or stage prefix.
    for (unsigned code : {12U, 13U}) {
        Trace dependency;
        g.reason = 17;
        g.detail = 0;
        g.qualification = code;
        g.command_gate = {code == 12 ? Commands::input_bindings : Commands::output_order, 0, 0, UINT32_MAX, 0, 0, 0, 2};
        dependency.commands.observe(g, read, [] { return true; });
        dependency.graph(g, false);
        emit(code == 12 ? "commands-dependency-discovery" : "commands-dependency-order", dependency);
    }
}
