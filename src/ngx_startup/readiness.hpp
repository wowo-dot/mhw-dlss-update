#pragma once
#include <cstdint>
namespace mhw::ngx_startup {
enum class Slots { unreadable,empty,populated };
enum class Admission { wait,ready,missed,expired };
inline constexpr std::uint64_t readiness_timeout_ms=300000;
constexpr Admission admit(unsigned code_mask,Slots slots,std::uint64_t elapsed) noexcept {
    if(slots==Slots::populated)return Admission::missed;
    if(elapsed>=readiness_timeout_ms)return Admission::expired;
    return code_mask==15&&slots==Slots::empty?Admission::ready:Admission::wait;
}
}
