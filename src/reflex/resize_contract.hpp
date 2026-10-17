#pragma once
#include <cstdint>
#include <span>

namespace mhw::reflex {
// ResizeBuffers retains the chain's creation queue. It is equivalent to the
// ResizeBuffers1 reassignment only for this single-node, unchanged-queue case.
constexpr bool simple_resize_equivalent(unsigned count, unsigned nodes, std::uintptr_t creation_queue,
                                        std::span<const unsigned> masks,
                                        std::span<const std::uintptr_t> queues) noexcept {
    if (!count || count > 16 || nodes != 1 || !creation_queue || masks.size() != count || queues.size() != count)
        return false;
    for (unsigned i = 0; i < count; ++i)
        if (masks[i] != 1 || queues[i] != creation_queue) return false;
    return true;
}
} // namespace mhw::reflex
