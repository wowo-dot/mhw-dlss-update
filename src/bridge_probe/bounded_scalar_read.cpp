#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "renderer_snapshot.hpp"
#include "readable_region.hpp"
#include "../perf/toggles.hpp"
#include <array>
#include <cstring>
#include <windows.h>

namespace mhw::bridge_probe {
namespace {
bool readable(const MEMORY_BASIC_INFORMATION &memory) noexcept {
  if (memory.State != MEM_COMMIT ||
      (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U)
    return false;
  const DWORD access = memory.Protect & 0xFFU;
  return access == PAGE_READONLY || access == PAGE_READWRITE ||
         access == PAGE_WRITECOPY || access == PAGE_EXECUTE_READ ||
         access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY;
}
// One entry per 64 KB block of address space, direct-mapped: the render
// thread reads from many separate allocations per frame, which eight
// round-robin entries could not hold (game-33: 26% of its time was still in
// NtQueryVirtualMemory).
struct CachedRegion {
  std::uintptr_t base{}, end{};
  std::uint64_t tick{};
};
constexpr unsigned cache_slots = 256U, cache_ms = 100U;
thread_local std::array<CachedRegion, cache_slots> regions{};
} // namespace

std::uintptr_t readable_end(const std::uintptr_t address) noexcept {
  const bool cached = perf::on(perf::cached_reads);
  std::uint64_t now{};
  auto &slot = regions[(address >> 16) & (cache_slots - 1U)];
  if (cached) {
    now = GetTickCount64();
    if (address >= slot.base && address < slot.end && now - slot.tick < cache_ms)
      return slot.end;
  }
  MEMORY_BASIC_INFORMATION memory{};
  if (VirtualQuery(reinterpret_cast<const void *>(address), &memory,
                   sizeof(memory)) != sizeof(memory) ||
      !readable(memory))
    return 0U;
  const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
  if (address < base || memory.RegionSize > UINTPTR_MAX - base ||
      address - base >= memory.RegionSize)
    return 0U;
  const auto end = base + memory.RegionSize;
  if (cached)
    slot = {base, end, now};
  return end;
}

ScalarReadResult
read_bounded_scalar(void *, const std::uintptr_t address,
                    const std::span<std::byte> output) noexcept {
  if (address == 0U || output.empty() || output.size() > 8U ||
      output.data() == nullptr)
    return ScalarReadResult::unreadable;
  const auto end = readable_end(address);
  if (end == 0U || end - address < output.size())
    return ScalarReadResult::unreadable;
  __try {
    std::memcpy(output.data(), reinterpret_cast<const void *>(address),
                output.size());
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return ScalarReadResult::fault;
  }
  return ScalarReadResult::success;
}

} // namespace mhw::bridge_probe
