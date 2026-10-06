#pragma once
#include <cstdint>
#include <cstddef>
namespace mhw::aa_control::deferred {
// Empty native parameter packet. Preparation owns +8; with count=0 both the
// preparer and executor skip parameter writes. Tail fields are our receipt.
struct alignas(16) Marker {
    std::uint32_t opcode{11},reserved{};
    std::uintptr_t barriers{};
    std::uint32_t count{},padding{};
    std::uint64_t cookie{0x41414D41524B4552ULL},generation{},tail{};
};
static_assert(offsetof(Marker,count)==0x10 && offsetof(Marker,cookie)==0x18);
inline bool valid(const Marker &p,unsigned slot) noexcept {
    return p.opcode==11&&!p.reserved&&!p.barriers&&!p.count&&!p.padding&&
        p.cookie==0x41414D41524B4552ULL&&p.generation==slot+1&&!p.tail;
}
inline std::uint32_t key(unsigned phase,unsigned offset) noexcept {
    return (phase<<27)|(((phase>>5)+(offset>>5))&0x7ffffffU);
}
inline bool admit(unsigned count,unsigned limit,unsigned capacity,unsigned phase,unsigned offset) noexcept {
    // +2b84 is the current front/back queue boundary; +2b88 is total front
    // capacity. Keep ample room for the remainder of the native phase.
    return capacity<=1048576&&limit<=capacity&&count<limit&&limit-count>=128&&
        key(phase,offset)==((15U<<27)|2U);
}
}
