#pragma once
#include "native_taa.hpp"
#include <algorithm>
#include <cstring>
#include <ostream>
#include <string_view>

namespace mhw::continuous_dlaa {
// First failed invocation only, under the existing runtime lock. No allocation,
// file I/O or game calls here. Metadata does not grant resource ownership.
struct ShaderRejection {
    bool present{}, shape_matches{}, tested_copy_ok{}, bytecode_matches{}, previously_verified{};
    bool captured_from_tested_bytes{}, file_saved{};
    unsigned bytes{};
    std::uint64_t sequence{}, generation{};
    std::uintptr_t list{};
    temporal_bindings::Packet packet;
    temporal_bindings::Snapshot source;
    temporal_bindings::ShaderRecipe recipe;
    std::array<std::byte, temporal_bindings::shader_limit> bytecode{};
    void capture(const temporal_bindings::Snapshot &s, const temporal_bindings::ShaderRecipe &p, bool shape,
                 bool copied, bool matched, bool verified, std::span<const std::byte> tested,
                 const temporal_bindings::Reader &reader) noexcept {
        if (present) return;
        present = true;
        source = s;
        recipe = p;
        shape_matches = shape;
        tested_copy_ok = copied;
        bytecode_matches = matched;
        previously_verified = verified;
        if (copied && tested.size() == p.size && p.size <= bytecode.size()) {
            std::memcpy(bytecode.data(), tested.data(), p.size);
            bytes = p.size;
            captured_from_tested_bytes = true;
        } else if (p.size >= 52 && p.size <= bytecode.size() && p.bytecode && reader.read &&
                   reader.read(reader.user, p.bytecode, std::span(bytecode).first(p.size)))
            bytes = p.size;
    }
    void write(std::ostream &out, std::string_view hash = {}) const {
        out << std::boolalpha << "{\"schema\":1,\"present\":" << present;
        if (!present) {
            out << '}';
            return;
        }
        out << ",\"shape_matches\":" << shape_matches << ",\"tested_copy_ok\":" << tested_copy_ok
            << ",\"bytecode_matches\":" << bytecode_matches << ",\"previously_verified\":" << previously_verified
            << ",\"captured_from_tested_bytes\":" << captured_from_tested_bytes << ",\"bytecode_bytes\":" << bytes
            << ",\"bytecode_sha256\":\"" << hash << "\",\"file_saved\":" << file_saved << ",\"sequence\":" << sequence
            << ",\"generation\":" << generation << ",\"list\":" << list << ",\"packet\":[" << packet.object << ','
            << packet.state << ',' << packet.opcode << ',' << packet.x << ',' << packet.y << ',' << packet.z << ','
            << packet.mode << ',' << packet.indirect << ']' << ",\"frame\":" << source.frame
            << ",\"epoch\":" << source.epoch << ",\"layer\":" << source.layer << ",\"context\":" << source.context
            << ",\"view\":" << source.view << ",\"state\":" << source.state << ",\"technique\":" << source.technique
            << ",\"taa_bytes\":" << source.taa_bytes << ",\"bank\":" << source.bank << ",\"async\":" << source.async
            << ",\"rectangle\":[";
        for (unsigned i = 0; i < source.rectangle.size(); ++i) out << (i ? "," : "") << source.rectangle[i];
        out << "],\"inputs\":[";
        for (unsigned i = 0; i < source.inputs.size(); ++i) {
            const auto &v = source.inputs[i];
            out << (i ? "," : "") << "{\"id\":" << v.id << ",\"object\":" << v.object << ",\"resource\":" << v.resource
                << ",\"width\":" << v.width << ",\"height\":" << v.height << ",\"format\":" << v.format
                << ",\"gpu_format\":" << v.gpu_format << '}';
        }
        out << "],\"recipe\":{\"state\":" << recipe.state << ",\"handle\":" << recipe.handle
            << ",\"set\":" << recipe.set << ",\"entry\":" << recipe.entry << ",\"descriptor\":" << recipe.descriptor
            << ",\"bytecode\":" << recipe.bytecode << ",\"size\":" << recipe.size
            << ",\"technique\":" << recipe.technique << ",\"owner_count\":" << recipe.owner_count
            << ",\"binding_count\":" << recipe.binding_count << ",\"binding_start\":" << recipe.binding_start
            << ",\"owners\":[";
        for (unsigned i = 0; i < std::min<unsigned>(recipe.owner_count, static_cast<unsigned>(recipe.owners.size()));
             ++i) {
            const auto &v = recipe.owners[i];
            out << (i ? "," : "") << '[' << v.id << ',' << v.mode << ',' << v.object << ']';
        }
        out << "],\"bindings\":[";
        for (unsigned i = 0;
             i < std::min<unsigned>(recipe.binding_count, static_cast<unsigned>(recipe.bindings.size())); ++i) {
            const auto &v = recipe.bindings[i];
            out << (i ? "," : "") << '[' << v.word << ',' << v.view << ',' << v.cpu_descriptor << ']';
        }
        out << "]}}";
    }
};
static_assert(sizeof(ShaderRejection) < 280000);
} // namespace mhw::continuous_dlaa
