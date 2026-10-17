#pragma once
#include "inputs.hpp"

namespace mhw::temporal_bindings {
// Branch operands of the pinned DX12 opcode-10 executor (RVA 0x259df30).
// A child draw packet exists on direct-copy paths too, but its state-list
// header is only meaningful when native execution actually calls that child.
enum class CopyExecutionPath : unsigned { unknown, whole_copy, region_copy, skipped, child_draw };
struct CopyExecution {
    unsigned kind{};
    std::uintptr_t destination{}, source{}, destination_primary{}, source_primary{}, destination_backing{},
        source_backing{};
    std::uintptr_t child{}, viewport{};
    std::array<unsigned, 4> rectangle{};
    unsigned destination_width{}, destination_height{}, source_width{}, source_height{};
    unsigned destination_kind{}, destination_format{}, source_format{}, child_opcode{};
    CopyExecutionPath path{};
    bool operator==(const CopyExecution &) const = default;
};
inline bool read_copy_execution(const Reader &r, std::uintptr_t packet, CopyExecution &s) noexcept {
    s = {};
    const auto field = [&]<class T>(std::uintptr_t p, unsigned offset, T &v) {
        return p && p <= UINTPTR_MAX - offset - sizeof(T) && r.get(p + offset, v);
    };
    unsigned raw{};
    if (!field(packet, 0, raw) || (raw & 31) != 10 || !field(packet, 0x10, s.kind) ||
        !field(packet, 0x18, s.destination) || !field(packet, 0x20, s.source) ||
        !field(s.source, 0xf0, s.source_primary) || !field(s.destination, 0xf0, s.destination_primary))
        return false;
    s.source_backing = s.source_primary;
    s.destination_backing = s.destination_primary;
    if ((!s.source_backing && !field(s.source, 0x60, s.source_backing)) ||
        (!s.destination_backing && !field(s.destination, 0x60, s.destination_backing)) ||
        !field(packet, 0x40, s.child) || !field(s.child, 0x10, s.viewport) || !field(s.viewport, 0x4c, s.rectangle))
        return false;
    if (s.kind == 8) {
        if (!field(s.source_backing, 0xb0, s.source_width) || !field(s.source_backing, 0xb4, s.source_height) ||
            !field(s.destination_backing, 0xb0, s.destination_width) ||
            !field(s.destination_backing, 0xb4, s.destination_height))
            return false;
        const bool whole = s.source_width == s.destination_width && s.source_height == s.destination_height &&
                           s.rectangle == std::array<unsigned, 4>{0, 0, s.source_width, s.source_height};
        // The native choice between CopyResource and CopyTextureRegion on this
        // branch does not affect whether the child executes its barriers.
        s.path = whole ? CopyExecutionPath::whole_copy : CopyExecutionPath::child_draw;
    } else {
        if (!field(s.destination, 0x58, s.destination_kind)) return false;
        if (s.destination_kind == 6)
            s.path = CopyExecutionPath::skipped;
        else {
            if (!field(s.source_backing, 0xc4, s.source_format) ||
                !field(s.destination_backing, 0xc4, s.destination_format))
                return false;
            s.path = s.source_format == s.destination_format ? CopyExecutionPath::region_copy
                                                             : CopyExecutionPath::child_draw;
        }
    }
    if (s.path == CopyExecutionPath::child_draw && (!field(s.child, 0, s.child_opcode) || (s.child_opcode & 31) != 0))
        return false;
    return true;
}
} // namespace mhw::temporal_bindings
