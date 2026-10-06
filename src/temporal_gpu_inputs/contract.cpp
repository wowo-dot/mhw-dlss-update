#include "contract.hpp"
#include <DirectXPackedVector.h>
#include <bit>
#include <cmath>

namespace mhw::temporal_gpu_inputs {
std::array<float, 4> color_reference(std::uint32_t word) noexcept {
    const DirectX::PackedVector::XMFLOAT3PK packed(word);
    DirectX::XMFLOAT3 value;
    DirectX::XMStoreFloat3(&value, DirectX::PackedVector::XMLoadFloat3PK(&packed));
    return {value.x, value.y, value.z, 1.0f};
}
std::array<float, 2> motion_reference(std::uint32_t word, unsigned width, unsigned height) noexcept {
    using DirectX::PackedVector::XMConvertHalfToFloat;
    return {-XMConvertHalfToFloat(static_cast<std::uint16_t>(word >> 16)) * (static_cast<float>(width) * .5f),
        XMConvertHalfToFloat(static_cast<std::uint16_t>(word & 0xfffeU)) * (static_cast<float>(height) * .5f)};
}
bool validate(const Frame &f, std::string &reason) noexcept {
    const auto count = std::uint64_t(f.width) * f.height;
    if (!f.width || !f.height || f.width > 8192 || f.height > 8192 || count * 4 > input_byte_limit) {
        reason = "input extent exceeds capture contract"; return false;
    }
    if (f.color.size() != count || f.depth.size() != count || f.motion.size() != count) {
        reason = "input plane size mismatch"; return false;
    }
    for (std::size_t i = 0; i < count; ++i) {
        const auto c = f.color[i], m = f.motion[i];
        const float d = std::bit_cast<float>(f.depth[i]);
        if (((c >> 6) & 31) == 31 || ((c >> 17) & 31) == 31 || ((c >> 27) & 31) == 31 ||
            ((m >> 26) & 31) == 31 || ((m >> 10) & 31) == 31 || !std::isfinite(d) || d < 0 || d > 1) {
            reason = "nonfinite color/motion or invalid depth"; return false;
        }
    }
    reason.clear(); return true;
}
}
