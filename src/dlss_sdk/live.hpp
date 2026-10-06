#pragma once
#include "history.hpp"
#include <d3d12.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <vector>

namespace mhw::dlss_sdk {
// Supplied only by an owner retaining successful initialization on this device.
// Validation returns its pinned provider, or null; it never initializes NGX.
struct BorrowedInitialization {
    HMODULE (*validate)(ID3D12Device *,const std::filesystem::path &) noexcept{};
};
struct GpuFrame {
    std::array<ID3D12Resource *,3> inputs{}; // private converted color/depth/motion, COPY_SOURCE
    ID3D12Fence *producer_fence{};
    std::uint64_t producer_value{};
    bool producer_retired{};
    unsigned width{},height{};
};
struct LiveResult {
    std::array<std::vector<float>,max_frames> output;
    bool complete{},feature_released{},parameters_destroyed{},shutdown_deferred{};
};
// Three or nine evaluations, serialized on the calling worker. A validated
// history plan resets only the first frame; without it every frame resets.
// Input ownership/state is supplied only by completed Live::convert receipts.
// All GPU work uses private lists. Inputs return to COPY_SOURCE. Borrowed-device
// initialization is retained until process exit; no Shutdown1 on the game device.
LiveResult evaluate_bounded(ID3D12Device *,std::span<const GpuFrame>,
    const std::filesystem::path &runtime,const std::filesystem::path &data,
    bool borrowed_device,std::ostream &events,const BorrowedInitialization *existing=nullptr,
    std::span<const HistoryFrame> history={});
}
