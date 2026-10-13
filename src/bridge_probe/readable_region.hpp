#pragma once
#include <cstdint>

namespace mhw::bridge_probe {
// End of the committed, readable, non-guard region holding address, or 0.
// With perf::cached_reads one shared answer per 64 KB block of address space
// (16,384 lock-free entries, direct-mapped) is reused for 100 ms of
// GetTickCount64 instead of one VirtualQuery per field; every read stays
// SEH-guarded, so a region released within that window fails the read
// instead of the process.
std::uintptr_t readable_end(std::uintptr_t address) noexcept;
// A second level keeps whole regions (64, by the time of their own query): a
// block that misses the first level is answered from it when its region was
// queried within the window, so a region costs one VirtualQuery (two when it
// reaches back to its allocation base) per window, not one per block.
struct ReadCacheStats {
  std::uint64_t level1_misses{}, region_hits{}, queries{};
};
ReadCacheStats read_cache_stats() noexcept;
}  // namespace mhw::bridge_probe
