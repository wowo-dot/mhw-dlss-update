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
// Second level (v0.23.4): whole regions. A block that misses the first level
// (expired, never seen, or another block in its slot) looks here first, so
// Windows is asked once per region per cache window instead of once per 64 KB
// block per window. game-88's profile: the render-unit walk (about a thousand
// nodes a frame) refreshed each of its blocks every 100 ms, and
// NtQueryVirtualMemory took 10% of the Present thread. An entry keeps the time
// of its own query and the first level copies it, so no answer outlives the
// window. game-89 (v0.23.4) still made about 2,000 queries a second: about a
// hundred hot regions, each expiring every 100 ms and queried twice. An
// expired region is now refreshed with one query from its base, and 256 are
// kept.
struct Region {
  std::uintptr_t base{}, end{};
  std::uint32_t tick{};
};
constexpr unsigned region_count = 256U;
std::array<Region, region_count> region_list{};
unsigned region_next{};
SRWLOCK region_lock = SRWLOCK_INIT;
std::atomic<std::uint64_t> level1_misses{}, region_hits{}, refreshes{},
    queries{};

// The region holding address: `known` within the window, `expired` (out is
// the expired entry, to refresh) or `none`.
enum class Known { none, expired, known };
Known known_region(std::uintptr_t address, std::uint32_t now,
                   Region &out) noexcept {
  Known found = Known::none;
  AcquireSRWLockShared(&region_lock);
  for (const auto &r : region_list)
    if (address >= r.base && address < r.end) {
      if (now - r.tick < cache_ms) {
        out = r;
        found = Known::known;
        break;
      }
      if (found == Known::none) {
        out = r;
        found = Known::expired;
      }
    }
  ReleaseSRWLockShared(&region_lock);
  return found;
}
void add_region(const Region &r) noexcept {
  AcquireSRWLockExclusive(&region_lock);
  // An entry overlapping the new region is replaced, else the next in turn.
  unsigned at = region_count;
  for (unsigned i = 0; i < region_count && at == region_count; ++i)
    if (region_list[i].base < r.end && r.base < region_list[i].end)
      at = i;
  if (at == region_count) {
    at = region_next;
    region_next = (region_next + 1U) % region_count;
  }
  region_list[at] = r;
  ReleaseSRWLockExclusive(&region_lock);
}
// The run of pages with one state and protection from the page holding
// address. With `whole`, when the run reaches back to the allocation base (a
// query from there ends at the same place with the same state and
// protection), the region starts at the allocation base.
bool query(std::uintptr_t address, bool whole, Region &out) noexcept {
  MEMORY_BASIC_INFORMATION memory{};
  queries.fetch_add(1, std::memory_order_relaxed);
  if (VirtualQuery(reinterpret_cast<const void *>(address), &memory,
                   sizeof(memory)) != sizeof(memory) ||
      !readable(memory))
    return false;
  const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
  if (address < base || memory.RegionSize > UINTPTR_MAX - base ||
      address - base >= memory.RegionSize)
    return false;
  out.base = base;
  out.end = base + memory.RegionSize;
  const auto allocation =
      reinterpret_cast<std::uintptr_t>(memory.AllocationBase);
  if (whole && allocation != 0U && allocation < base) {
    MEMORY_BASIC_INFORMATION first{};
    queries.fetch_add(1, std::memory_order_relaxed);
    if (VirtualQuery(memory.AllocationBase, &first, sizeof(first)) ==
            sizeof(first) &&
        readable(first) &&
        reinterpret_cast<std::uintptr_t>(first.BaseAddress) == allocation &&
        first.RegionSize <= UINTPTR_MAX - allocation &&
        allocation + first.RegionSize == out.end &&
        first.State == memory.State && first.Protect == memory.Protect)
      out.base = allocation;
  }
  return true;
}
} // namespace

std::uintptr_t readable_end(const std::uintptr_t address) noexcept {
  Region r;
  if (!perf::on(perf::cached_reads))
    return query(address, false, r) ? r.end : 0U;
  const auto now = static_cast<std::uint32_t>(GetTickCount64());
  auto &slot = regions[(address >> 16) & (cache_slots - 1U)];
  if (const auto end = cached_end(slot, address, now))
    return end;
  level1_misses.fetch_add(1, std::memory_order_relaxed);
  const auto known = known_region(address, now, r);
  Region fresh;
  if (known == Known::known) {
    region_hits.fetch_add(1, std::memory_order_relaxed);
  } else if (known == Known::expired && query(r.base, false, fresh) &&
             fresh.base == r.base && fresh.end == r.end) {
    // The same run from the same base: one query refreshes the region.
    refreshes.fetch_add(1, std::memory_order_relaxed);
    r.tick = now;
    add_region(r);
  } else {
    if (!query(address, true, r))
      return 0U;
    r.tick = now;
    add_region(r);
  }
  remember(slot, r.base, r.end, r.tick);
  return r.end;
}

ReadCacheStats read_cache_stats() noexcept {
  return {level1_misses.load(std::memory_order_relaxed),
          region_hits.load(std::memory_order_relaxed),
          refreshes.load(std::memory_order_relaxed),
          queries.load(std::memory_order_relaxed)};
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
