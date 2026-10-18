#pragma once
#include "recorder.hpp"
#include <span>
namespace mhw::projection_binding {
// Image allocation and page-protection check implemented by identity.cpp.
bool executable(std::uintptr_t at, std::uintptr_t base) noexcept;

template <class T> bool read(std::uintptr_t at, T &out) noexcept {
    return game_memory::read_bounded_scalar(nullptr, at, std::as_writable_bytes(std::span(&out, 1))) ==
           game_memory::ScalarReadResult::success;
}
} // namespace mhw::projection_binding
