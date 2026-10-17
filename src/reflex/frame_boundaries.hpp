#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace mhw::reflex::boundaries {
inline constexpr unsigned capacity = 262144, duration_ms = 15000;
enum Kind : unsigned { update, advance, publish, render, native_present, execute, present,
    keyboard, mouse, devices, jobs, submit, device_poll };
inline constexpr unsigned kinds = 7;
struct Frame {
    std::array<std::uintptr_t, 7> gpr{};
    std::uint64_t flags{};
    std::array<std::byte, 96> xmm{};
    std::uintptr_t caller{};
    std::array<std::uintptr_t, 4> stack{};
};
static_assert(offsetof(Frame, caller) == 0xa0 && sizeof(Frame) == 0xc8);
bool install(std::uintptr_t image, std::string& reason);
void stop() noexcept;
bool finish() noexcept;
void describe(std::ostream& log);
void write(std::ostream& log);
}
