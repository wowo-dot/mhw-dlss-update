#pragma once
#include "replay.hpp"
#include <d3d12.h>
#include <memory>

namespace mhw::temporal_gpu_inputs {
// Worker-only preparation/conversion on the borrowed game device. input() returns
// private COPY_DEST textures in color/depth/motion order. The caller records one
// native-list copy into each and proves producer completion before convert().
// No game descriptors, root state, device debug settings, or queues are changed.
class Live {
 public:
    Live(ID3D12Device *, unsigned width, unsigned height);
    ~Live();
    Live(const Live &) = delete;
    Live &operator=(const Live &) = delete;
    ID3D12Resource *input(unsigned) const noexcept;
    std::uint64_t allocation_bytes() const noexcept;
    Result convert();
    // Available only after conversion, completion, retirement and readback.
    // Outputs remain COPY_SOURCE and are exclusively owned by this object.
    ID3D12Resource *converted(unsigned) const noexcept;
    ID3D12Fence *completed_fence() const noexcept;
 private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
