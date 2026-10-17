#pragma once
#include <cstdint>

namespace mhw::reflex {
// An SDK pacer may use the same physical chain from a worker thread. Only
// the explicitly owned host thread may turn that chain into a logical SDK
// transaction. Worker calls must reach the original DXGI implementation.
constexpr bool paced_host_call(std::uintptr_t self, std::uintptr_t host, unsigned caller, unsigned owner) noexcept {
    return host != 0 && owner != 0 && self == host && caller == owner;
}
} // namespace mhw::reflex
