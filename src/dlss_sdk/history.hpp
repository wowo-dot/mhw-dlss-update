#pragma once
#include <cstdint>

namespace mhw::dlss_sdk {
struct HistoryFrame {
    std::uint64_t frame{},epoch{};
    float jitter_x{},jitter_y{}; // input pixels, +X right, +Y down
};
}
