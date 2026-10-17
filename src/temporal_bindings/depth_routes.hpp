#pragma once
#include "inputs.hpp"
#include <d3d12.h>
#include <iosfwd>
namespace mhw::temporal_bindings::depth_routes {
struct Stamp {
    std::uintptr_t context{}, view{}, container{};
    std::uint64_t frame{}, epoch{};
    unsigned bank{}, async{};
    bool operator==(const Stamp &) const = default;
};
struct CopyPacket {
    unsigned opcode{}, kind{};
    std::uintptr_t source{}, destination{}, nested{};
    std::array<unsigned, 6> rectangle{};
    bool operator==(const CopyPacket &) const = default;
};
bool stamp(const Reader &, const Config &, Stamp &);
bool packet(const Reader &, std::uintptr_t, CopyPacket &);
bool draw_states(std::span<const D3D12_RESOURCE_BARRIER>, std::uintptr_t, std::uintptr_t) noexcept;
} // namespace mhw::temporal_bindings::depth_routes
