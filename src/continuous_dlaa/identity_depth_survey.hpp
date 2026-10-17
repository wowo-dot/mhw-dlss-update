#pragma once
#include "native_state_trace.hpp"
#include "../temporal_bindings/copy_execution.hpp"
#include "../temporal_bindings/depth_routes.hpp"
#include <array>
#include <cstdint>
#include <ostream>
namespace mhw::continuous_dlaa {
// Observation only (v0.19.2). At render scale 1.0 the scene-depth writer is a
// plain native copy (the opcode-10 whole-copy path), so DepthConsumerAdmission
// (which qualifies the scaled nested draw) does not apply, and the depth lease
// at the copy fails whenever the game records that copy on another command
// list than the TAA dispatch (game-70, game-74). A copy at the TAA dispatch
// needs the scene depth's D3D state there, proven from the native schedule as
// the scaled admission does. This survey records that schedule for the TAA's
// chosen depth copy: the copy path, the depth tracker's spans (packet role,
// last use, native before/after states) and every depth state node between the
// writer and the TAA dispatch. Nothing it reads authorizes GPU work.
// Native states (converter 0x259ba50): 8 PIXEL_SHADER_RESOURCE, 12 both shader
// reads, 0x20 COPY_SOURCE, 0x100 RENDER_TARGET, 0x200 DEPTH_WRITE, 0x400 UAV,
// other values from 0x101 COPY_DEST.
class IdentityDepthSurvey {
    using Reader = temporal_bindings::Reader;
    template <class T> static bool field(const Reader &r, std::uintptr_t p, unsigned o, T &v) noexcept {
        return NativeStateTrace::field(r, p, o, v);
    }
    // Packet roles: 1 the writer, 2 the TAA dispatch, 3 the writer's child draw, 0 other.
    struct Span {
        unsigned role{}, last_use{}, before{}, after{}, first{};
    };
    struct Node {
        unsigned index{}, role{}, before{}, after{};
        bool nested{};
    };
    struct Sample {
        std::uint64_t ordinal{}, tick{}, frame{}, sequence{}, depth_sequence{}, generation{};
        std::uintptr_t list{};
        bool missing{};
        // 0 complete; 1 entry, 2 copy execution, 3 tracker, 4 schedule, 5 interval nodes
        unsigned reason{}, path{}, kind{}, writer_index{UINT_MAX}, consumer_index{UINT_MAX}, writer_matches{},
            consumer_matches{}, packets{};
        unsigned initial{}, final{}, span_count{}, node_count{};
        bool epoch_matches{}, complete{};
        std::array<Span, 16> spans{};
        std::array<Node, 24> nodes{};
    };
    std::array<Sample, 8> first_{}, missing_{};
    std::uint64_t observed_{}, missing_count_{}, surveyed_{};
    static unsigned role(std::uintptr_t packet, std::uintptr_t writer, std::uintptr_t consumer,
                         std::uintptr_t child) noexcept {
        return packet == writer ? 1U : packet == consumer ? 2U : child && packet == child ? 3U : 0U;
    }
    static bool node_list(const Reader &r, std::uintptr_t packet, unsigned index, bool nested,
                          const temporal_bindings::Owner &owner, std::uintptr_t writer, std::uintptr_t consumer,
                          Sample &s) noexcept {
        std::uintptr_t n{};
        if (!field(r, packet, 8, n)) return false;
        for (unsigned visited = 0; n && visited < 128; ++visited) {
            std::uintptr_t next{}, backing{}, resource{};
            std::uint16_t before{}, after{};
            if (!field(r, n, 0, next) || !field(r, n, 8, backing) || !field(r, backing, 0x20, resource) ||
                !field(r, n, 0x18, before) || !field(r, n, 0x1a, after))
                return false;
            if ((resource == owner.resource || backing == owner.backing) && s.node_count < s.nodes.size())
                s.nodes[s.node_count++] = {index,
                                           packet == writer     ? 1U
                                           : packet == consumer ? 2U
                                                                : 0U,
                                           before, after, nested};
            n = next;
        }
        return !n;
    }
    static void survey(const Reader &r, const temporal_bindings::Snapshot &source,
                       const temporal_bindings::Owner &owner, std::uintptr_t writer,
                       const temporal_bindings::depth_routes::CopyPacket &expected, std::uintptr_t consumer,
                       Sample &s) noexcept {
        s.reason = 1;
        if (!writer || !consumer || writer == consumer || source.context < 0x710 || !owner.object || !owner.backing ||
            !owner.resource || expected.destination != owner.object)
            return;
        s.reason = 2;
        temporal_bindings::CopyExecution copy;
        if (!temporal_bindings::read_copy_execution(r, writer, copy)) return;
        s.path = static_cast<unsigned>(copy.path);
        s.kind = copy.kind;
        s.reason = 3;
        NativeStateTrace::Tracker t;
        if (!NativeStateTrace::tracker(r, owner.backing, t)) return;
        s.initial = t.initial;
        s.final = t.final;
        auto node = t.first;
        while (node && s.span_count < s.spans.size()) {
            std::uintptr_t packet{}, next{};
            std::uint32_t last{};
            std::uint16_t before{}, after{};
            unsigned char first{};
            if (!field(r, node, 0, packet) || !field(r, node, 8, last) || !field(r, node, 0x10, next) ||
                !field(r, node, 0x20, before) || !field(r, node, 0x22, after) || !field(r, node, 0x24, first))
                return;
            s.spans[s.span_count++] = {role(packet, writer, consumer, copy.child), last, before, after, first};
            node = next;
        }
        s.complete = !node;
        s.reason = 4;
        const auto renderer = source.context - 0x710;
        std::uint64_t epoch{};
        std::uintptr_t array{};
        unsigned count{};
        if (!field(r, renderer, 0x1f208, epoch) || !field(r, renderer, 0x7af18, array) ||
            !field(r, renderer, 0x7af28, count) || !array || !count || count > 32768 ||
            array > UINTPTR_MAX - 16ULL * count)
            return;
        s.packets = count;
        s.epoch_matches = t.epoch == epoch && t.used_epoch == epoch;
        std::array<std::array<std::uint64_t, 2>, 64> block{};
        for (unsigned begin = 0; begin < count;) {
            const auto n = (std::min)(64U, count - begin);
            if (!r.read || !r.read(r.user, array + 16ULL * begin, std::as_writable_bytes(std::span(block).first(n))))
                return;
            for (unsigned i = 0; i < n; ++i) {
                if (block[i][1] == writer) {
                    ++s.writer_matches;
                    s.writer_index = begin + i;
                }
                if (block[i][1] == consumer) {
                    ++s.consumer_matches;
                    s.consumer_index = begin + i;
                }
            }
            begin += n;
        }
        if (s.writer_matches != 1 || s.consumer_matches != 1 || s.writer_index >= s.consumer_index ||
            s.consumer_index - s.writer_index > 256)
            return;
        s.reason = 5;
        for (unsigned index = s.writer_index; index <= s.consumer_index; ++index) {
            std::uintptr_t packet{};
            unsigned raw{};
            if (!field(r, array, index * 16 + 8, packet) || !packet || !field(r, packet, 0, raw)) return;
            if (!node_list(r, packet, index, false, owner, writer, consumer, s)) return;
            if ((raw & 31) == 10) {
                temporal_bindings::CopyExecution c;
                if (!temporal_bindings::read_copy_execution(r, packet, c)) return;
                if (c.path == temporal_bindings::CopyExecutionPath::child_draw &&
                    !node_list(r, c.child, index, true, owner, writer, consumer, s))
                    return;
            }
        }
        s.reason = 0;
    }
    static void emit(std::ostream &o, const Sample &s) {
        o << "{\"ordinal\":" << s.ordinal << ",\"at\":[" << s.tick << ',' << s.frame << ',' << s.sequence << ','
          << s.depth_sequence << ',' << s.list << ',' << s.generation << "],\"missing\":" << s.missing
          << ",\"reason\":" << s.reason << ",\"path\":" << s.path << ",\"kind\":" << s.kind << ",\"indices\":["
          << s.writer_index << ',' << s.consumer_index << ',' << s.writer_matches << ',' << s.consumer_matches << ','
          << s.packets << "],\"tracker\":[" << s.initial << ',' << s.final << ',' << s.epoch_matches << ','
          << s.complete << "],\"spans\":[";
        for (unsigned i = 0; i < s.span_count; ++i) {
            const auto &a = s.spans[i];
            o << (i ? "," : "") << '[' << a.role << ',' << a.last_use << ',' << a.before << ',' << a.after << ','
              << a.first << ']';
        }
        o << "],\"nodes\":[";
        for (unsigned i = 0; i < s.node_count; ++i) {
            const auto &n = s.nodes[i];
            o << (i ? "," : "") << '[' << n.index << ',' << n.role << ',' << n.before << ',' << n.after << ','
              << n.nested << ']';
        }
        o << "]}";
    }

public:
    // At each identity evaluation; bounded to the first 8 and 8 missing frames.
    void observe(const Reader &r, const temporal_bindings::Snapshot &source, const temporal_bindings::Owner &owner,
                 std::uintptr_t writer, const temporal_bindings::depth_routes::CopyPacket &expected,
                 std::uintptr_t consumer, std::uint64_t sequence, std::uint64_t depth_sequence, std::uintptr_t list,
                 std::uint64_t generation, std::uint64_t tick, bool missing) noexcept {
        ++observed_;
        if (missing) ++missing_count_;
        Sample *slot = observed_ <= first_.size() ? &first_[static_cast<unsigned>(observed_ - 1)]
                       : missing && missing_count_ <= missing_.size()
                           ? &missing_[static_cast<unsigned>(missing_count_ - 1)]
                           : nullptr;
        if (!slot) return;
        ++surveyed_;
        *slot = {};
        auto &s = *slot;
        s.ordinal = observed_;
        s.tick = tick;
        s.frame = source.frame;
        s.sequence = sequence;
        s.depth_sequence = depth_sequence;
        s.list = list;
        s.generation = generation;
        s.missing = missing;
        survey(r, source, owner, writer, expected, consumer, s);
    }
    void write(std::ostream &o) const {
        o << "{\"schema\":1,\"policy\":\"identity-depth-schedule-survey-v1\",\"diagnostic_only\":true,\"observed\":"
          << observed_ << ",\"missing\":" << missing_count_ << ",\"surveyed\":" << surveyed_ << ",\"first\":[";
        for (unsigned i = 0; i < first_.size() && i < observed_; ++i) {
            if (i) o << ',';
            emit(o, first_[i]);
        }
        o << "],\"missing_samples\":[";
        for (unsigned i = 0; i < missing_.size() && i < missing_count_; ++i) {
            if (i) o << ',';
            emit(o, missing_[i]);
        }
        o << "]}";
    }
};
} // namespace mhw::continuous_dlaa
