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
#include <atomic>
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
// One shared entry per 64 KB block of address space, direct-mapped over 1 GB
// (16,384 entries, 384 KB) and read by every thread without a lock. A
// per-thread cache of 256 blocks thrashed on the render queue walk in the
// training grounds (game-34: 42% of the Present thread in
// NtQueryVirtualMemory). Each entry is a seqlock: a writer makes the version
// odd, stores, and makes it even; a reader that sees an odd or changed version
// treats the entry as a miss. A writer that loses the race skips caching.
struct SharedRegion {
  std::atomic<std::uint32_t> version{}, tick{};
  std::atomic<std::uintptr_t> base{}, end{};
};
constexpr unsigned cache_slots = 1U << 14, cache_ms = 100U;
std::array<SharedRegion, cache_slots> regions{};

std::uintptr_t cached_end(SharedRegion &slot, std::uintptr_t address,
                          std::uint32_t now) noexcept {
  const auto before = slot.version.load(std::memory_order_acquire);
  if (before & 1U)
    return 0U;
  const auto base = slot.base.load(std::memory_order_relaxed);
  const auto end = slot.end.load(std::memory_order_relaxed);
  const auto tick = slot.tick.load(std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_acquire);
  if (slot.version.load(std::memory_order_relaxed) != before)
    return 0U;
  return address >= base && address < end && now - tick < cache_ms ? end : 0U;
}
void remember(SharedRegion &slot, std::uintptr_t base, std::uintptr_t end,
              std::uint32_t now) noexcept {
  auto version = slot.version.load(std::memory_order_relaxed);
  if ((version & 1U) ||
      !slot.version.compare_exchange_strong(version, version + 1U,
                                            std::memory_order_acquire))
    return;
  slot.base.store(base, std::memory_order_relaxed);
  slot.end.store(end, std::memory_order_relaxed);
  slot.tick.store(now, std::memory_order_relaxed);
  slot.version.store(version + 2U, std::memory_order_release);
}
} // namespace

std::uintptr_t readable_end(const std::uintptr_t address) noexcept {
  const bool cached = perf::on(perf::cached_reads);
  const auto now = cached ? static_cast<std::uint32_t>(GetTickCount64()) : 0U;
  auto &slot = regions[(address >> 16) & (cache_slots - 1U)];
  if (cached)
    if (const auto end = cached_end(slot, address, now))
      return end;
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
    remember(slot, base, end, now);
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
