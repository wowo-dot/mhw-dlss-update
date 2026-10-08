// v0.12 overhead work: frame-interval statistics and the cached region check.
#include "../src/perf/frame_stats.hpp"
#include "../src/perf/toggles.hpp"
#include "../src/bridge_probe/readable_region.hpp"
#include "../src/bridge_probe/renderer_snapshot.hpp"
#include <windows.h>
#include <cstdio>
#include <span>

namespace {
int failures{};
void check(bool ok, const char* what) {
    if (!ok) { std::printf("FAIL: %s\n", what); ++failures; }
}

void frame_stats_tests() {
    mhw::perf::FrameStats s;
    // 98 frames at 10 ms, one at 30 ms, one at 120 ms (overflow bucket).
    for (unsigned i = 0; i < 98; ++i) s.interval(10000);
    s.interval(30000);
    s.interval(120000);
    auto w = s.take();
    check(w.frames == 100 && w.total_us == 98 * 10000 + 30000 + 120000, "window totals");
    check(w.p50_us == 10250 && w.p95_us == 10250, "median and p95 are the 10 ms bucket's upper edge");
    check(w.p99_us == 30250, "p99 is the 30 ms frame");
    check(w.max_us == 120000 && w.over_50ms == 1, "maximum and long frames");
    w = s.take();
    check(w.frames == 0 && w.p50_us == 0 && w.max_us == 0, "take() starts a new window");
    // A single frame in the overflow bucket reports the window maximum.
    s.interval(250000);
    w = s.take();
    check(w.p50_us == 250000 && w.p99_us == 250000, "overflow percentile is the maximum");
    // present(): the first call only sets the reference; 10 MHz QPC.
    s.present(1000000, 10000000);
    s.present(1000000 + 166667, 10000000);
    s.present(1000000 + 166667, 10000000);  // no time passed: ignored
    w = s.take();
    check(w.frames == 1 && w.total_us == 16666, "QPC interval in microseconds");
    s.restart();
    s.present(5, 10000000);
    check(s.take().frames == 0, "restart() drops the reference point");
}

void region_tests() {
    namespace bp = mhw::bridge_probe;
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const auto page = info.dwPageSize;
    auto* block = static_cast<unsigned char*>(VirtualAlloc(nullptr, page * 3, MEM_RESERVE, PAGE_NOACCESS));
    check(block != nullptr, "reserve");
    if (!block) return;
    // Commit the first two pages read/write; the third stays reserved.
    check(VirtualAlloc(block, page * 2, MEM_COMMIT, PAGE_READWRITE) != nullptr, "commit");
    block[8] = 42;
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    std::uint64_t value{};
    const auto read = [&](std::uintptr_t at) {
        return bp::read_bounded_scalar(nullptr, at, std::as_writable_bytes(std::span(&value, 1)));
    };
    for (const unsigned mask : {0U, unsigned(mhw::perf::cached_reads)}) {
        mhw::perf::toggles.store(mask);
        check(bp::readable_end(base + 8) == base + page * 2, "region end");
        check(bp::readable_end(base + page * 2) == 0, "reserved page is not readable");
        check(read(base + 8) == bp::ScalarReadResult::success && value == 42, "read inside the region");
        check(read(base + page * 2 - 4) == bp::ScalarReadResult::unreadable, "read across the region end");
        check(read(0) == bp::ScalarReadResult::unreadable, "null");
    }
    // With the cache on, a region made inaccessible within the cache window is
    // still reported readable, and the guarded read fails instead of crashing.
    mhw::perf::toggles.store(mhw::perf::cached_reads);
    check(bp::readable_end(base + 8) == base + page * 2, "cached region");
    DWORD old{};
    check(VirtualProtect(block, page * 2, PAGE_NOACCESS, &old) != FALSE, "protect");
    const auto cached = bp::readable_end(base + 8);
    const auto result = read(base + 8);
    check(cached == base + page * 2 ? result == bp::ScalarReadResult::fault : result == bp::ScalarReadResult::unreadable,
          "stale cache entry: guarded read fails");
    Sleep(150);  // past the cache window
    check(bp::readable_end(base + 8) == 0 && read(base + 8) == bp::ScalarReadResult::unreadable, "cache entry expires");
    // Without the cache the protection change is seen at once.
    mhw::perf::toggles.store(0);
    check(bp::readable_end(base + 8) == 0, "uncached check");
    VirtualFree(block, 0, MEM_RELEASE);
}
}  // namespace

int main() {
    frame_stats_tests();
    region_tests();
    if (failures) return 1;
    std::printf("perf tests passed\n");
    return 0;
}
