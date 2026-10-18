#pragma once
#include <algorithm>
#include "../game_memory/layout.hpp"
#include <array>
#include <cstdint>
#include <ostream>

namespace mhw::scaled_temporal {
// CPU-only receipts at completed queue preparation. No ownership, admission,
// packet mutation or GPU work. Capture once per rejection shape, not per frame.
class OutputCommands {
public:
    static constexpr unsigned capacity = 16, packet_capacity = 32;
    using Gate = std::array<std::uint64_t, 8>;
    // branch, key, packet, queue index (UINT32_MAX if not at a packet),
    // raw opcode word, raw word read, prior draws for key, total overlay draws.
    enum Branch : unsigned {
        none,
        flags_read,
        candidate_capacity,
        ambiguous_key,
        packet_read,
        multiple_draws,
        draw_capacity,
        unsupported_opcode,
        output_order,
        geometry,
        input_bindings,
        duplicate_packet
    };
    struct Packet {
        std::uint64_t index{}, key{}, address{};
        unsigned raw{};
        bool raw_read{}, prefix_read{};
        std::array<std::uint32_t, 18> prefix{}; // bounded 72-byte native packet prefix
    };
    struct Sample {
        std::array<std::uint64_t, 6> signature{};
        std::array<std::uint64_t, 8> at{};
        Gate gate{};
        std::array<std::array<std::uint64_t, 7>, 16> filters{};
        unsigned filter_count{};
        std::uint64_t occurrences{1}, queue{};
        unsigned total{}, scanned{}, matched{}, unreadable{}, count{}, error{};
        bool stable{};
        std::array<unsigned, 32> opcodes{};
        std::array<Packet, packet_capacity> packets{};
        Packet focus{};
        bool has_focus{};
    };

private:
    std::array<Sample, capacity> samples_{};
    unsigned count_{};
    std::uint64_t attempts_{}, overflow_{};
    template <class Reader> static Packet packet(Reader read, unsigned index, unsigned key, std::uint64_t address) {
        Packet p;
        p.index = index;
        p.key = key;
        p.address = address;
        p.raw_read = address && read(address, &p.raw, sizeof(p.raw));
        if (!p.raw_read) p.raw = 0;
        if (p.raw_read) {
            p.prefix_read = read(address, p.prefix.data(), sizeof(p.prefix));
            if (!p.prefix_read) p.prefix = {};
        }
        return p;
    }

public:
    unsigned size() const { return count_; }
    const Sample &sample(unsigned i) const { return samples_[i]; }
    template <class Graph, class Reader, class Stamp> void observe(const Graph &g, Reader read, Stamp stamp) {
        // Also capture an unsupported intermediate color chain, without assuming
        // anything about its shader or admitting it for full-resolution rendering.
        if (!g.command_gate[0] && !(g.reason == 6 && g.detail == 4)) return;
        ++attempts_;
        const std::array<std::uint64_t, 6> signature{
            g.reason,          g.detail,          g.qualification,
            g.command_gate[0], g.command_gate[1], g.command_gate[5] ? g.command_gate[4] & 31 : 32};
        for (unsigned i = 0; i < count_; ++i)
            if (samples_[i].signature == signature) {
                ++samples_[i].occurrences;
                return;
            }
        if (count_ == capacity) {
            ++overflow_;
            return;
        }
        auto &s = samples_[count_++];
        s.signature = signature;
        s.at = g.at;
        s.gate = g.command_gate;
        s.filters = g.filters;
        s.filter_count = g.count;
        const auto context = g.at[3];
        if (context > UINT64_MAX - 0x2b84 || !read(context + layout::context::packet_count, &s.total, 4) || !s.total ||
            s.total >= 65535 || !read(context + layout::context::packets, &s.queue, 8) || !s.queue ||
            s.queue > UINT64_MAX - 16ULL * s.total) {
            s.error = 1;
            return;
        }
        struct Command {
            unsigned key, padding;
            std::uint64_t packet;
        };
        static_assert(sizeof(Command) == 16);
        std::array<Command, 256> block{};
        for (unsigned base = 0; base < s.total; base += 256) {
            const auto n = (std::min)(256U, s.total - base);
            if (!read(s.queue + 16ULL * base, block.data(), n * sizeof(Command))) {
                s.error = 2;
                break;
            }
            for (unsigned j = 0; j < n; ++j) {
                const auto &c = block[j];
                ++s.scanned;
                bool match = false;
                for (unsigned k = 0; k < s.filter_count; ++k) {
                    const auto &f = s.filters[k];
                    if (f[1] == 15 && f[2] > 100 && f[2] < 0x7fffff1 && c.key == ((f[1] << 27) | f[2])) match = true;
                }
                if (!match) continue;
                ++s.matched;
                unsigned raw{};
                const bool ok = c.packet && read(c.packet, &raw, 4);
                if (ok)
                    ++s.opcodes[raw & 31];
                else
                    ++s.unreadable;
                if (s.count < packet_capacity) s.packets[s.count++] = packet(read, base + j, c.key, c.packet);
                if (g.command_gate[0] && ((g.command_gate[3] != UINT32_MAX && base + j == g.command_gate[3]) ||
                                          (g.command_gate[3] == UINT32_MAX && c.packet == g.command_gate[2]))) {
                    s.focus = packet(read, base + j, c.key, c.packet);
                    s.has_focus = true;
                }
            }
        }
        unsigned total{};
        std::uint64_t queue{};
        s.stable = !s.error && s.scanned == s.total && read(context + layout::context::packet_count, &total, 4) &&
                   total == s.total && read(context + layout::context::packets, &queue, 8) && queue == s.queue &&
                   stamp();
    }
    void write(std::ostream &o) const {
        const auto a = [&](const auto &v) {
            o << '[';
            bool comma = false;
            for (auto x : v) {
                if (comma) o << ',';
                o << x;
                comma = true;
            }
            o << ']';
        };
        const auto p = [&](const Packet &v) {
            o << "{\"index\":" << v.index << ",\"key\":" << v.key << ",\"address\":" << v.address
              << ",\"raw\":" << v.raw << ",\"raw_read\":" << v.raw_read << ",\"prefix_read\":" << v.prefix_read
              << ",\"prefix\":";
            a(v.prefix);
            o << '}';
        };
        o << "{\"schema\":1,\"policy\":\"prepared-command-rejections-v1\",\"diagnostic_only\":true,\"capacity\":"
          << capacity << ",\"packet_capacity\":" << packet_capacity << ",\"attempts\":" << attempts_
          << ",\"overflow\":" << overflow_ << ",\"samples\":[";
        for (unsigned i = 0; i < count_; ++i) {
            if (i) o << ',';
            const auto &s = samples_[i];
            o << "{\"signature\":";
            a(s.signature);
            o << ",\"at\":";
            a(s.at);
            o << ",\"gate\":";
            a(s.gate);
            o << ",\"occurrences\":" << s.occurrences << ",\"queue\":" << s.queue << ",\"total\":" << s.total
              << ",\"scanned\":" << s.scanned << ",\"matched\":" << s.matched << ",\"unreadable\":" << s.unreadable
              << ",\"error\":" << s.error << ",\"stable\":" << s.stable << ",\"opcodes\":";
            a(s.opcodes);
            o << ",\"filters\":[";
            for (unsigned k = 0; k < s.filter_count; ++k) {
                if (k) o << ',';
                a(s.filters[k]);
            }
            o << "],\"packets\":[";
            for (unsigned k = 0; k < s.count; ++k) {
                if (k) o << ',';
                p(s.packets[k]);
            }
            o << "],\"focus\":";
            if (s.has_focus)
                p(s.focus);
            else
                o << "null";
            o << '}';
        }
        o << "]}";
    }
};
} // namespace mhw::scaled_temporal
