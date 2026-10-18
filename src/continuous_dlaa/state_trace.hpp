#pragma once
#include "../temporal_bindings/inputs.hpp"
#include "../game_memory/layout.hpp"
#include "native_state_trace.hpp"
#include <d3d12.h>
#include <algorithm>
#include <memory>
#include <ostream>
namespace mhw::continuous_dlaa {
// Diagnostic mirror only. Runtime admission continues to use state_of().
// Result: match, missing transition, aliasing, split/flags, subresource,
// discontinuous chain, unexpected final state.
struct StateProof {
    unsigned result{1}, value{}, transitions{}, index{UINT_MAX};
    bool operator==(const StateProof &) const = default;
};
inline StateProof explain_state(std::span<const D3D12_RESOURCE_BARRIER> barriers, std::uintptr_t resource,
                                unsigned expected) noexcept {
    StateProof p;
    bool known = false;
    for (unsigned i = 0; i < barriers.size(); ++i) {
        const auto &b = barriers[i];
        if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING &&
            (!b.Aliasing.pResourceBefore || !b.Aliasing.pResourceAfter ||
             reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore) == resource ||
             reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter) == resource)) {
            p.result = 2;
            p.index = i;
            return p;
        }
        if (b.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
            reinterpret_cast<std::uintptr_t>(b.Transition.pResource) != resource)
            continue;
        ++p.transitions;
        p.index = i;
        if (b.Flags) {
            p.result = 3;
            return p;
        }
        if (b.Transition.Subresource != UINT_MAX) {
            p.result = 4;
            return p;
        }
        if (known && p.value != unsigned(b.Transition.StateBefore)) {
            p.result = 5;
            return p;
        }
        known = true;
        p.value = b.Transition.StateAfter;
    }
    p.result = !known ? 1 : p.value == expected ? 0 : 6;
    return p;
}
class StateTrace {
public:
    static constexpr unsigned capacity = 8, barrier_capacity = 16;
    struct Resource {
        std::uintptr_t object{}, backing{}, resource{};
        unsigned width{}, height{}, format{};
        StateProof proof;
    };
    struct Sample {
        std::uint64_t ordinal{}, tick{}, frame{}, epoch{}, sequence{}, generation{};
        std::uintptr_t list{}, packet{};
        bool temporal{}, native_complete{};
        unsigned barrier_count{}, relevant{}, saved{}, native_nodes{}, native_relevant{}, native_saved{};
        std::array<Resource, 3> resources;
        // index,type,flags,subresource,before,after,resource-role-mask,other-mask
        std::array<std::array<unsigned, 8>, barrier_capacity> barriers{};
        // linked-list index, resource-role-mask, native before/after words
        std::array<std::array<unsigned, 4>, barrier_capacity> native{};
        NativeStateTrace provenance;
    };

private:
    std::array<Sample, capacity> first_{};
    Sample last_{};
    std::uint64_t total_{}, output_{}, temporal_{};
    static unsigned mask(const Sample &s, ID3D12Resource *r) noexcept {
        if (!r) return 8;
        unsigned m{};
        for (unsigned i = 0; i < 3; ++i)
            if (s.resources[i].resource == reinterpret_cast<std::uintptr_t>(r)) m |= 1U << i;
        return m;
    }
    static void write_sample(std::ostream &o, const Sample &s) {
        o << "{\"ordinal\":" << s.ordinal << ",\"tick\":" << s.tick << ",\"frame\":" << s.frame
          << ",\"epoch\":" << s.epoch << ",\"sequence\":" << s.sequence << ",\"generation\":" << s.generation
          << ",\"list\":" << s.list << ",\"packet\":" << s.packet << ",\"temporal\":" << s.temporal
          << ",\"resources\":[";
        for (unsigned i = 0; i < 3; ++i) {
            const auto &r = s.resources[i];
            const auto &p = r.proof;
            o << (i ? "," : "") << "{\"object\":" << r.object << ",\"backing\":" << r.backing
              << ",\"resource\":" << r.resource << ",\"width\":" << r.width << ",\"height\":" << r.height
              << ",\"format\":" << r.format << ",\"proof\":[" << p.result << ',' << p.value << ',' << p.transitions
              << ',' << p.index << "]}";
        }
        o << "],\"barrier_count\":" << s.barrier_count << ",\"relevant_barriers\":" << s.relevant << ",\"barriers\":[";
        for (unsigned i = 0; i < s.saved; ++i) {
            o << (i ? ",[" : "[");
            for (unsigned j = 0; j < 8; ++j) o << (j ? "," : "") << s.barriers[i][j];
            o << ']';
        }
        o << "],\"native_complete\":" << s.native_complete << ",\"native_nodes\":" << s.native_nodes
          << ",\"native_relevant\":" << s.native_relevant << ",\"native\":[";
        for (unsigned i = 0; i < s.native_saved; ++i) {
            o << (i ? ",[" : "[");
            for (unsigned j = 0; j < 4; ++j) o << (j ? "," : "") << s.native[i][j];
            o << ']';
        }
        o << "],\"provenance\":";
        s.provenance.write(o);
        o << '}';
    }

public:
    // Caller holds runtime mutex. Only bounded copies/reads at rejected checks;
    // no D3D calls, COM references, per-frame allocations or resource writes.
    const Sample &capture(const temporal_bindings::Reader &reader, const temporal_bindings::Snapshot &source,
                          std::uint64_t sequence, ID3D12GraphicsCommandList *list, std::uint64_t generation,
                          std::uintptr_t packet, std::uint64_t tick, bool temporal,
                          std::span<const D3D12_RESOURCE_BARRIER> barriers) noexcept {
        const auto ordinal = total_ + 1;
        auto &s = ordinal <= capacity ? first_[static_cast<unsigned>(ordinal - 1)] : last_;
        // Reinitialize the retained slot in place; do not put the larger
        // provenance sample on a game worker's stack or allocate per capture.
        std::destroy_at(&s);
        std::construct_at(&s);
        s.ordinal = ++total_;
        if (temporal)
            ++temporal_;
        else
            ++output_;
        s.tick = tick;
        s.frame = source.frame;
        s.epoch = source.epoch;
        s.sequence = sequence;
        s.list = reinterpret_cast<std::uintptr_t>(list);
        s.generation = generation;
        s.packet = packet;
        s.temporal = temporal;
        s.barrier_count = static_cast<unsigned>(barriers.size());
        constexpr unsigned roles[]{0, 4, 5}, expected[]{0x40, 8, 0xc0};
        for (unsigned i = 0; i < 3; ++i) {
            const auto &r = source.inputs[roles[i]];
            s.resources[i] = {r.object,
                              r.backing,
                              r.resource,
                              r.width,
                              r.height,
                              r.gpu_format,
                              explain_state(barriers, r.resource, expected[i])};
        }
        for (unsigned i = 0; i < barriers.size(); ++i) {
            const auto &b = barriers[i];
            std::array<unsigned, 8> row{i, static_cast<unsigned>(b.Type), static_cast<unsigned>(b.Flags), 0, 0, 0, 0,
                                        0};
            if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
                row[3] = b.Transition.Subresource;
                row[4] = b.Transition.StateBefore;
                row[5] = b.Transition.StateAfter;
                row[6] = mask(s, b.Transition.pResource);
            } else if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV)
                row[6] = mask(s, b.UAV.pResource);
            else if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING) {
                row[6] = mask(s, b.Aliasing.pResourceBefore);
                row[7] = mask(s, b.Aliasing.pResourceAfter);
            }
            if (!(row[6] | row[7])) continue;
            ++s.relevant;
            if (s.saved < barrier_capacity) s.barriers[s.saved++] = row;
        }
        std::uintptr_t node{};
        if (packet && packet <= UINTPTR_MAX - 0x10 && reader.get(packet + 8, node)) {
            s.native_complete = true;
            while (node) {
                std::uintptr_t next{}, backing{}, resource{};
                std::uint16_t before{}, after{};
                if (s.native_nodes == 128 || node > UINTPTR_MAX - 0x20 || !reader.get(node, next) ||
                    !reader.get(node + 8, backing) || !backing || backing > UINTPTR_MAX - 0x28 ||
                    !reader.get(backing + layout::backing::resource, resource) ||
                    !reader.get(node + layout::state_node::before, before) ||
                    !reader.get(node + layout::state_node::after, after)) {
                    s.native_complete = false;
                    break;
                }
                const auto m = mask(s, reinterpret_cast<ID3D12Resource *>(resource));
                if (m & 7) {
                    ++s.native_relevant;
                    if (s.native_saved < barrier_capacity)
                        s.native[s.native_saved++] = {s.native_nodes, m, before, after};
                }
                ++s.native_nodes;
                node = next;
            }
        }
        s.provenance.capture(reader, source, packet);
        return s;
    }
    void write(std::ostream &o) const {
        o << "{\"schema\":2,\"policy\":\"consumer-local-state-v1\",\"first_capacity\":8,\"barrier_capacity\":16,\"total\":"
          << total_ << ",\"output_rejections\":" << output_ << ",\"temporal_rejections\":" << temporal_
          << ",\"omitted\":" << (total_ > capacity + 1 ? total_ - capacity - 1 : 0) << ",\"samples\":[";
        const auto n = static_cast<unsigned>(std::min<std::uint64_t>(total_, capacity));
        for (unsigned i = 0; i < n; ++i) {
            if (i) o << ',';
            write_sample(o, first_[i]);
        }
        if (total_ > capacity) {
            o << ',';
            write_sample(o, last_);
        }
        o << "]}";
    }
};
} // namespace mhw::continuous_dlaa
