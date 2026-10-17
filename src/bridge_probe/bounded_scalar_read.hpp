#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace mhw::bridge_probe {

enum class ScalarReadResult : std::uint8_t { success, unreadable, fault };
using ScalarReader = ScalarReadResult (*)(void *, std::uintptr_t, std::span<std::byte>) noexcept;
// Windows runtime adapter: only 1..8 bytes, inside one readable committed
// region.
ScalarReadResult read_bounded_scalar(void *, std::uintptr_t, std::span<std::byte>) noexcept;

} // namespace mhw::bridge_probe
