#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace mhw::dlss_sdk {
struct HistoryFrame {
    std::uint64_t frame{},epoch{};
    float jitter_x{},jitter_y{}; // input pixels, +X right, +Y down
};
using HistoryPlan=std::array<HistoryFrame,3>;
inline constexpr unsigned max_frames=9;
inline bool valid_frame_count(std::size_t count) noexcept {return count==3 || count==max_frames;}
// The bounded diagnostic deliberately fixes time; these are not measured frame
// intervals. Production integration must supply actual frame time and resets.
constexpr float diagnostic_frame_ms=16.666666F;
inline bool valid_history(std::span<const HistoryFrame> plan) noexcept {
    if(!valid_frame_count(plan.size()))return false;
    for(unsigned i=0;i<plan.size();++i){const auto &p=plan[i];
        if(!std::isfinite(p.jitter_x)||!std::isfinite(p.jitter_y)||
           std::abs(p.jitter_x)>.5F||std::abs(p.jitter_y)>.5F)return false;
        if(i && (plan[i-1].frame==UINT64_MAX||plan[i-1].epoch==UINT64_MAX||
            p.frame!=plan[i-1].frame+1||p.epoch!=plan[i-1].epoch+1))return false;
    }
    return true;
}
}
