#pragma once
#include <cstdint>

namespace mhw::game_memory {
// End of the committed, readable, non-guard region holding address, or 0.
// With perf::cached_reads one shared answer per 64 KB block of address space
// (16,384 lock-free entries, direct-mapped) is reused for 100 ms of
// GetTickCount64 instead of one VirtualQuery per field; every read stays
// SEH-guarded, so a region released within that window fails the read
// instead of the process.
std::uintptr_t readable_end(std::uintptr_t address) noexcept;
// Under Wine (Proton), memory the host mapped itself is invisible to
// VirtualQuery, which calls it free: vkd3d-proton's mapped GPU heaps, where
// the game's per-frame constants live (devilzavacado-1, 0.30.1-diag.1: the
// constants inside a 532 GB "free" run, so the projection capture failed and
// DLSS stopped at the first frame). There, and only there, a free answer is
// settled by reading one byte of the page: a page that reads is one readable
// page; a fault leaves it unreadable. Windows' answers stay final.
// probe_page is that guarded read.
bool probe_page(std::uintptr_t address) noexcept;
// A second level keeps whole regions (256, by the time of their own query): a
// block that misses the first level is answered from it when its region was
// queried within the window, so a region costs one VirtualQuery per window
// (two when it is first found and reaches back to its allocation base), not
// one per block. An expired region is refreshed by one query from its base.
// host_pages and host_faults count Wine's free answers that read and that
// faulted.
struct ReadCacheStats {
    std::uint64_t level1_misses{}, region_hits{}, refreshes{}, queries{}, background_queries{};
    std::uint64_t host_pages{}, host_faults{};
};
ReadCacheStats read_cache_stats() noexcept;
// The refresher (v0.23.6): renews, from its base, every region a reader took
// within the last second once its answer is 40 ms old, so hot regions stay
// within the window and readers rarely query (a region that changed or is no
// longer readable is dropped and queried again by its next reader). Returns
// the regions renewed. start_region_refresher() runs it every 20 ms on its own
// thread (idempotent); the 100 ms contract is unchanged.
unsigned refresh_regions(std::uint32_t now_tick) noexcept;
void start_region_refresher() noexcept;
} // namespace mhw::game_memory
