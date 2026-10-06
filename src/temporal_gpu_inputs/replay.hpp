#pragma once
#include "contract.hpp"
#include <vector>

namespace mhw::temporal_gpu_inputs {
struct Adapter {
    bool warp{};
    std::uint32_t luid_low{};
    std::int32_t luid_high{};
};
struct Result {
    std::vector<float> color, depth, motion;
    std::uint32_t adapter_low{};
    std::int32_t adapter_high{};
    unsigned vendor{}, device{}, debug_warnings{}, debug_errors{};
    bool debug_layer{}, gpu_validation{}, completed{}, allocator_retired{};
    std::uint64_t fence_value{}, allocation_bytes{};
    std::uintptr_t queue_address{},list_address{},allocator_address{},fence_address{},heap_address{};
    std::array<std::uintptr_t,3> output_addresses{},readback_addresses{};
};
// Standalone owned device/list/heap/resources. Never opens or modifies a game process.
// On uncertain submission failure, retain the context until process termination.
Result replay(const Frame &, const Adapter &);
}
