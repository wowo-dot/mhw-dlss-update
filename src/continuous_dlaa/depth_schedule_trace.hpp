#pragma once
#include "native_state_trace.hpp"
#include "../temporal_bindings/depth_routes.hpp"
#include <d3d12.h>
#include <memory>
namespace mhw::continuous_dlaa {
// Observation only. No pointer from these records is used to authorize GPU work.
struct DepthSchedulePositions {
    using Position = std::array<unsigned, 2>; // matches, first index
    Position writer{0, UINT_MAX};
    std::array<std::array<Position, 16>, 3> spans{};
    unsigned scanned{};
    bool complete{}, stable{};
    bool operator==(const DepthSchedulePositions &) const = default;
    void capture(const temporal_bindings::Reader &r, const NativeStateTrace &p, std::uintptr_t selected) noexcept {
        for (auto &role : spans)
            for (auto &v : role) v = {0, UINT_MAX};
        if (!p.readable || !p.scan_complete || !p.stable || p.matches != 1 || !selected) return;
        std::array<std::array<std::uint64_t, 2>, 64> chunk{};
        const auto match = [](Position &v, unsigned index) {
            if (!v[0]) v[1] = index;
            ++v[0];
        };
        for (unsigned begin = 0; begin < p.packets;) {
            const auto count = std::min(64U, p.packets - begin);
            if (!r.read ||
                !r.read(r.user, p.array + 16ULL * begin, std::as_writable_bytes(std::span(chunk).first(count))))
                return;
            for (unsigned i = 0; i < count; ++i) {
                const auto packet = chunk[i][1];
                if (packet == selected) match(writer, begin + i);
                for (unsigned role = 0; role < 3; ++role)
                    for (unsigned j = 0; j < p.trackers[role].count; ++j)
                        if (packet == p.trackers[role].spans[j][1]) match(spans[role][j], begin + i);
            }
            begin += count;
            scanned = begin;
        }
        complete = scanned == p.packets;
        std::uintptr_t array{};
        unsigned packets{};
        std::uint64_t epoch{};
        stable = NativeStateTrace::field(r, p.renderer, 0x7af18, array) &&
                 NativeStateTrace::field(r, p.renderer, 0x7af28, packets) &&
                 NativeStateTrace::field(r, p.renderer, 0x1f208, epoch) && array == p.array && packets == p.packets &&
                 epoch == p.epoch;
    }
    void write(std::ostream &o) const {
        o << "{\"writer\":[" << writer[0] << ',' << writer[1] << "],\"scanned\":" << scanned
          << ",\"complete\":" << complete << ",\"stable\":" << stable << ",\"span_packets\":[";
        for (unsigned i = 0; i < 3; ++i) {
            o << (i ? ",[" : "[");
            for (unsigned j = 0; j < 16; ++j) o << (j ? ",[" : "[") << spans[i][j][0] << ',' << spans[i][j][1] << ']';
            o << ']';
        }
        o << "]}";
    }
};
class DepthScheduleTrace {
    struct Sample {
        std::uint64_t ordinal{}, tick{}, frame{}, epoch{}, sequence{}, depth_sequence{}, generation{};
        std::uintptr_t list{}, packet{}, writer{};
        bool missing{}, owner_stable{}, writer_stable{}, plan_equal{}, positions_equal{};
        std::array<temporal_bindings::Owner, 3> owners{};
        temporal_bindings::depth_routes::CopyPacket expected{}, actual{};
        unsigned barrier_count{}, relevant{}, saved{};
        std::array<std::array<unsigned, 5>, 16>
            barriers{}; // type,flags,subresource,before,after; depth or global alias only
        NativeStateTrace plan;
        DepthSchedulePositions positions;
    };
    std::array<Sample, 4> samples_{};
    NativeStateTrace again_;
    DepthSchedulePositions positions_again_;
    std::uint64_t checks_{}, missing_{}, captured_{}, last_missing_tick_{};
    static void packet(std::ostream &o, const temporal_bindings::depth_routes::CopyPacket &p) {
        o << '[' << p.opcode << ',' << p.kind << ',' << p.source << ',' << p.destination << ',' << p.nested;
        for (auto v : p.rectangle) o << ',' << v;
        o << ']';
    }

public:
    // Runtime mutex held. One baseline, first two misses, and latest miss at
    // most once per second. Larger snapshots live here, never on a worker stack.
    void observe(const temporal_bindings::Reader &r, const temporal_bindings::Snapshot &source,
                 const temporal_bindings::SceneDepth &depth, std::uintptr_t writer,
                 const temporal_bindings::depth_routes::CopyPacket &expected, std::uint64_t depth_sequence,
                 std::uint64_t sequence, std::uintptr_t list, std::uint64_t generation, std::uintptr_t temporal_packet,
                 std::uint64_t tick, bool missing, std::span<const D3D12_RESOURCE_BARRIER> barriers) noexcept {
        ++checks_;
        if (missing) ++missing_;
        unsigned slot{};
        if (!missing) {
            if (samples_[0].ordinal) return;
        } else if (!samples_[1].ordinal)
            slot = 1;
        else if (!samples_[2].ordinal)
            slot = 2;
        else {
            if (tick < last_missing_tick_ || tick - last_missing_tick_ < 1000) return;
            slot = 3;
        }
        if (missing) last_missing_tick_ = tick;
        auto &s = samples_[slot];
        std::destroy_at(&s);
        std::construct_at(&s);
        s.ordinal = ++captured_;
        s.tick = tick;
        s.frame = source.frame;
        s.epoch = source.epoch;
        s.sequence = sequence;
        s.depth_sequence = depth_sequence;
        s.list = list;
        s.generation = generation;
        s.packet = temporal_packet;
        s.writer = writer;
        s.expected = expected;
        s.missing = missing;
        s.owners = {depth.texture, source.inputs[0], source.inputs[4]};
        s.barrier_count = static_cast<unsigned>(barriers.size());
        for (const auto &b : barriers) {
            std::array<unsigned, 5> v{static_cast<unsigned>(b.Type), static_cast<unsigned>(b.Flags), 0, 0, 0};
            bool keep = false;
            if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION &&
                reinterpret_cast<std::uintptr_t>(b.Transition.pResource) == depth.texture.resource) {
                keep = true;
                v[2] = b.Transition.Subresource;
                v[3] = b.Transition.StateBefore;
                v[4] = b.Transition.StateAfter;
            }
            if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING)
                keep = !b.Aliasing.pResourceBefore || !b.Aliasing.pResourceAfter ||
                       reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore) == depth.texture.resource ||
                       reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter) == depth.texture.resource;
            if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV)
                keep = !b.UAV.pResource || reinterpret_cast<std::uintptr_t>(b.UAV.pResource) == depth.texture.resource;
            if (keep) {
                ++s.relevant;
                if (s.saved < s.barriers.size()) s.barriers[s.saved++] = v;
            }
        }
        temporal_bindings::Owner owner_before{}, owner_after{};
        const bool first_owner =
            temporal_bindings::read_owner(r, depth.texture.object, owner_before) && owner_before == depth.texture;
        const bool first_writer = temporal_bindings::depth_routes::packet(r, writer, s.actual) && s.actual == expected;
        // The shared reader's three roles are explicitly remapped for this
        // diagnostic. They are depth, color and temporal output, not its usual roles.
        auto mapped = source;
        mapped.inputs[0] = s.owners[0];
        mapped.inputs[4] = s.owners[1];
        mapped.inputs[5] = s.owners[2];
        s.plan.capture(r, mapped, temporal_packet);
        s.positions.capture(r, s.plan, writer);
        std::destroy_at(&again_);
        std::construct_at(&again_);
        again_.capture(r, mapped, temporal_packet);
        std::destroy_at(&positions_again_);
        std::construct_at(&positions_again_);
        positions_again_.capture(r, again_, writer);
        s.plan_equal = s.plan == again_;
        s.positions_equal = s.positions == positions_again_;
        s.owner_stable = first_owner && temporal_bindings::read_owner(r, depth.texture.object, owner_after) &&
                         owner_after == owner_before;
        temporal_bindings::depth_routes::CopyPacket packet_after;
        s.writer_stable = first_writer && temporal_bindings::depth_routes::packet(r, writer, packet_after) &&
                          packet_after == s.actual;
    }
    void write(std::ostream &o) const {
        o << "{\"schema\":1,\"policy\":\"depth-scheduled-consumer-observation-v1\",\"diagnostic_only\":true,\"roles\":[\"scene_depth\",\"color\",\"temporal_output\"],\"capacity\":4,\"interval_ms\":1000,\"checks\":"
          << checks_ << ",\"missing\":" << missing_ << ",\"captured\":" << captured_ << ",\"samples\":[";
        bool comma = false;
        for (unsigned slot = 0; slot < samples_.size(); ++slot) {
            const auto &s = samples_[slot];
            if (!s.ordinal) continue;
            o << (comma ? "," : "") << "{\"slot\":" << slot << ",\"ordinal\":" << s.ordinal << ",\"at\":[" << s.tick
              << ',' << s.frame << ',' << s.epoch << ',' << s.sequence << ',' << s.depth_sequence << ',' << s.list
              << ',' << s.generation << ',' << s.packet << ',' << s.writer << "],\"missing\":" << s.missing;
            comma = true;
            o << ",\"owner_stable\":" << s.owner_stable << ",\"writer_stable\":" << s.writer_stable
              << ",\"plan_equal\":" << s.plan_equal << ",\"positions_equal\":" << s.positions_equal << ",\"owners\":[";
            for (unsigned i = 0; i < 3; ++i) {
                const auto &v = s.owners[i];
                o << (i ? ",[" : "[") << v.object << ',' << v.backing << ',' << v.resource << ',' << v.width << ','
                  << v.height << ',' << v.gpu_format << ']';
            }
            o << "],\"expected_writer\":";
            packet(o, s.expected);
            o << ",\"actual_writer\":";
            packet(o, s.actual);
            o << ",\"barrier_count\":" << s.barrier_count << ",\"relevant\":" << s.relevant << ",\"barriers\":[";
            for (unsigned i = 0; i < s.saved; ++i) {
                o << (i ? ",[" : "[");
                for (unsigned j = 0; j < 5; ++j) o << (j ? "," : "") << s.barriers[i][j];
                o << ']';
            }
            o << "],\"plan\":";
            s.plan.write(o);
            o << ",\"positions\":";
            s.positions.write(o);
            o << '}';
        }
        o << "]}";
    }
};
} // namespace mhw::continuous_dlaa
