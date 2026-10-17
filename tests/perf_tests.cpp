// v0.12 overhead work: frame-interval statistics and the cached region check.
#include "../src/perf/frame_stats.hpp"
#include "../src/perf/toggles.hpp"
#include "../src/bridge_probe/readable_region.hpp"
#include "../src/bridge_probe/bounded_scalar_read.hpp"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <span>
#include <sstream>
#include <string>
#include "../src/perf/hook_timing.hpp"
#include "../src/perf/in_flight.hpp"
#include "../src/scaled_temporal/watch_filter.hpp"
namespace mhw::projection_binding { std::string bytes_sha256(const void *, unsigned); }

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
// Sixteen one-page regions (alternating protection) inside one 64 KB block
// share one cache slot with sixteen different answers. Eight threads read
// them concurrently; a torn seqlock read would return a wrong region end.
void shared_cache_tests() {
    namespace bp = mhw::bridge_probe;
    mhw::perf::toggles.store(mhw::perf::cached_reads);
    auto* block = static_cast<unsigned char*>(VirtualAlloc(nullptr, 65536, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    check(block != nullptr, "block");
    if (!block) return;
    for (unsigned i = 1; i < 16; i += 2) {
        DWORD old{};
        VirtualProtect(block + i * 4096, 4096, PAGE_READONLY, &old);
    }
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    std::atomic<unsigned> wrong{}, done{};
    std::array<HANDLE, 8> threads{};
    struct Job { std::uintptr_t base; std::atomic<unsigned>* wrong; std::atomic<unsigned>* done; unsigned seed; };
    std::array<Job, 8> jobs{};
    for (unsigned t = 0; t < threads.size(); ++t) {
        jobs[t] = {base, &wrong, &done, t * 7919U + 1U};
        threads[t] = CreateThread(nullptr, 0, [](void* p) -> DWORD {
            auto& j = *static_cast<Job*>(p);
            unsigned x = j.seed;
            for (unsigned n = 0; n < 200000; ++n) {
                x = x * 1103515245U + 12345U;
                const unsigned page = (x >> 16) & 15U, offset = (x >> 4) & 4095U;
                if (bp::readable_end(j.base + page * 4096 + offset) != j.base + (page + 1) * 4096) ++*j.wrong;
            }
            ++*j.done;
            return 0;
        }, &jobs[t], 0, nullptr);
    }
    WaitForMultipleObjects(static_cast<DWORD>(threads.size()), threads.data(), TRUE, 60000);
    for (auto h : threads) CloseHandle(h);
    check(done.load() == threads.size(), "all reader threads finished");
    check(wrong.load() == 0, "no torn or misattributed cache entries");
    VirtualFree(block, 0, MEM_RELEASE);
    mhw::perf::toggles.store(0);
}
// v0.23.4: the second level answers every 64 KB block of one region with one
// region query, also when the first level has expired.
void region_level_tests() {
    namespace bp = mhw::bridge_probe;
    mhw::perf::toggles.store(mhw::perf::cached_reads);
    Sleep(150);  // earlier answers (possibly for this address range) expire
    constexpr std::size_t size = std::size_t{4} << 20;  // 64 blocks
    auto* block = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    check(block != nullptr, "4 MB region");
    if (!block) return;
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    // From the top down, so the first query starts in the region's last block.
    const auto touch = [&] {
        bool ok = true;
        for (std::size_t b = size / 65536; b-- > 0;) ok = ok && bp::readable_end(base + b * 65536 + 100) == base + size;
        return ok;
    };
    auto before = bp::read_cache_stats();
    check(touch(), "every block reports the region end");
    auto after = bp::read_cache_stats();
    check(after.queries - before.queries == 2, "one region: its run and its allocation base");
    check(after.level1_misses - before.level1_misses == 64 && after.region_hits - before.region_hits == 63,
          "each block missed the first level once; the region answered all but the first");
    before = after;
    check(touch(), "first level hits");
    after = bp::read_cache_stats();
    check(after.level1_misses == before.level1_misses && after.queries == before.queries, "no misses within the window");
    Sleep(150);
    before = bp::read_cache_stats();
    check(touch(), "after the window");
    after = bp::read_cache_stats();
    check(after.queries - before.queries == 1 && after.refreshes - before.refreshes == 1,
          "expired blocks refresh through one query from the region's base");
    // A region made inaccessible is seen once the window has passed.
    DWORD old{};
    check(VirtualProtect(block, size, PAGE_NOACCESS, &old) != FALSE, "protect");
    Sleep(150);
    check(bp::readable_end(base + 100) == 0 && bp::readable_end(base + size - 100) == 0, "expired region is queried again");
    VirtualFree(block, 0, MEM_RELEASE);
    mhw::perf::toggles.store(0);
}
// v0.23.6: the refresher renews a region readers use, so a new block of it is
// answered past the first window without a read-path query; a region made
// inaccessible is dropped and its next read queries again. Timing checks are
// skipped (not failed) when the machine stalls past their margins.
void refresher_tests() {
    namespace bp = mhw::bridge_probe;
    mhw::perf::toggles.store(mhw::perf::cached_reads);
    Sleep(150);
    constexpr std::size_t size = std::size_t{1} << 20;
    auto* block = static_cast<unsigned char*>(VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    check(block != nullptr, "1 MB region");
    if (!block) return;
    const auto base = reinterpret_cast<std::uintptr_t>(block);
    const auto tick = [] { return static_cast<std::uint32_t>(GetTickCount64()); };
    const auto first = tick();
    check(bp::readable_end(base + 100) == base + size, "first read");
    Sleep(60);
    auto before = bp::read_cache_stats();
    const auto refreshed_at = tick();
    check(bp::refresh_regions(refreshed_at) >= 1, "the refresher renews the region in use");
    auto after = bp::read_cache_stats();
    check(after.background_queries > before.background_queries && after.queries == before.queries,
          "the refresher's queries are its own");
    Sleep(60);
    before = bp::read_cache_stats();
    const auto read_at = tick();
    check(bp::readable_end(base + 5 * 65536 + 8) == base + size, "the renewed region answers");
    after = bp::read_cache_stats();
    if (read_at - first >= 100 && read_at - refreshed_at < 90)
        check(after.queries == before.queries && after.region_hits == before.region_hits + 1,
              "past the first window, no read-path query");
    else
        std::printf("skipped: refresher window check (timing)\n");
    DWORD old{};
    check(VirtualProtect(block, size, PAGE_NOACCESS, &old) != FALSE, "protect");
    Sleep(50);
    bp::refresh_regions(tick());
    Sleep(110);
    check(bp::readable_end(base + 100) == 0 && bp::readable_end(base + 5 * 65536 + 8) == 0,
          "a dropped region is queried again");
    VirtualFree(block, 0, MEM_RELEASE);
    mhw::perf::toggles.store(0);
}
// v0.23.6: SHA-256 with one provider and the memo. Repeated input, one byte
// changed (the memo must not answer for other bytes), and back.
void sha_memo_tests() {
    std::array<unsigned char, 1024> data{};
    for (unsigned i = 0; i < data.size(); ++i) data[i] = static_cast<unsigned char>(i);
    const auto hash = [&] { return mhw::projection_binding::bytes_sha256(data.data(), static_cast<unsigned>(data.size())); };
    const std::string expected = "785b0751fc2c53dc14a4ce3d800e69ef9ce1009eb327ccf458afe09c242c26c9";
    check(hash() == expected, "SHA-256 of 0..255 four times");
    check(hash() == expected, "the memo returns the same hash for the same bytes");
    data[700] ^= 1;
    check(hash() == "de9209846e2e263052a811d6b682a8654e4dbb1a2bfce5ed95d992bd8fa48bee", "one byte changed: its own hash");
    data[700] ^= 1;
    check(hash() == expected, "back to the first bytes");
    check(mhw::projection_binding::bytes_sha256("abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "short input (not memoized)");
}
// v0.23.7: hook timing. 6,400 calls on one thread time 100 of them; the
// estimate is 6,400 calls over the window's frames, and the forwarded part
// (a spin inside forward_begin/end) is counted apart from the hook's own.
void hook_timing_tests() {
    namespace ht = mhw::perf::hook_timing;
    ht::enable();
    const auto before = ht::totals[ht::draw].timed.load();
    for (unsigned i = 0; i < 6400; ++i) {
        auto s = ht::begin(ht::draw);
        ht::forward_begin(s);
        for (volatile unsigned k = 0; k < 50; ++k) {}
        ht::forward_end(s);
        ht::end(s);
    }
    check(ht::totals[ht::draw].timed.load() - before == 100, "every 64th call is timed");
    check(ht::totals[ht::draw].forwarded.load() > 0, "forwarded time counted");
    std::ostringstream out;
    ht::write(out, 64);
    const auto text = out.str();
    check(text.find("\"event\":\"hook_timing\"") != std::string::npos && text.find("\"draw\":{\"calls_per_frame\":100") != std::string::npos,
          "6,400 calls over 64 frames: 100 a frame");
    // v0.24.2: every category is named, the game-function hooks included.
    bool named = true;
    for (const auto *n : ht::names) named = named && n && *n;
    check(named && text.find("\"route_indexed\":{") != std::string::npos && text.find("\"native_frame\":{") != std::string::npos,
          "every category has a name in the event");
    ht::enabled.store(false);
}
// v0.24.3: the stream route's watch filters. Every added address is found (no
// false negative, the filters' contract); at a realistic load (one plan's ~40
// packets in a plan filter, 32 plans' ~1,300 in the union) unrelated addresses
// are mostly rejected; a filled filter admits everything, a cleared one
// nothing; a union assigned from a local build matches its adds.
void watch_filter_tests() {
    using mhw::scaled_temporal::PlanFilter;
    using mhw::scaled_temporal::UnionFilter;
    static PlanFilter one;
    static UnionFilter all, rebuilt;
    std::uint64_t state = 0x243F6A8885A308D3ULL;
    const auto next = [&] { state = state * 6364136223846793005ULL + 1442695040888963407ULL; return (state >> 8) & ~std::uint64_t(15); };
    std::array<std::uintptr_t, 1300> added{};
    std::array<std::uint64_t, UnionFilter::words> local{};
    for (unsigned i = 0; i < added.size(); ++i) {
        added[i] = static_cast<std::uintptr_t>(next());
        if (i < 40) one.add(added[i]);
        all.add(added[i]);
        UnionFilter::set(local, added[i]);
    }
    rebuilt.assign(local);
    bool found = true;
    for (unsigned i = 0; i < added.size(); ++i)
        found = found && all.may_contain(added[i]) && rebuilt.may_contain(added[i]) && (i >= 40 || one.may_contain(added[i]));
    check(found, "every added address is found");
    unsigned one_hits{}, all_hits{};
    for (unsigned i = 0; i < 100000; ++i) {
        const auto a = static_cast<std::uintptr_t>(next());
        one_hits += one.may_contain(a);
        all_hits += all.may_contain(a);
    }
    check(one_hits < 1000, "one plan: under 1% of unrelated addresses pass");
    check(all_hits < 5000, "32 plans' union: under 5% of unrelated addresses pass");
    // Neighbouring packets (one engine arena) spread over the bits.
    static PlanFilter run;
    for (unsigned i = 0; i < 40; ++i) run.add(0x7ff600000000ULL + 0x50 * i);
    unsigned run_hits{};
    for (unsigned i = 40; i < 10040; ++i) run_hits += run.may_contain(0x7ff600000000ULL + 0x50 * i);
    check(run_hits < 300, "neighbouring packets are mostly rejected");
    one.fill();
    check(one.may_contain(0x1230) && one.may_contain(static_cast<std::uintptr_t>(next())), "a filled filter admits everything");
    one.clear();
    check(!one.may_contain(added[0]), "a cleared filter admits nothing");
}
// v0.23.10: the in-flight count without locked instructions. Eight threads
// add and subtract; a held count is seen from another thread; and the drain
// contract under a race: after the flag is cleared and zero() observed, no
// thread is still inside a section it entered on seeing the flag set.
void in_flight_tests() {
    static mhw::perf::InFlightCount count;
    check(count.zero() && count.load() == 0, "starts at zero");
    std::array<HANDLE, 8> threads{};
    for (auto& t : threads)
        t = CreateThread(nullptr, 0, [](void*) -> DWORD {
            for (unsigned i = 0; i < 100000; ++i) { count.add(); count.sub(); }
            return 0;
        }, nullptr, 0, nullptr);
    WaitForMultipleObjects(static_cast<DWORD>(threads.size()), threads.data(), TRUE, 60000);
    for (auto h : threads) CloseHandle(h);
    check(count.zero() && count.load() == 0, "balanced adds and subtracts end at zero");
    count.add();
    check(!count.zero() && count.load() == 1, "a held count is seen");
    HANDLE other = CreateThread(nullptr, 0, [](void*) -> DWORD { return count.zero() ? 1U : 0U; }, nullptr, 0, nullptr);
    WaitForSingleObject(other, 10000);
    DWORD seen_zero{};
    GetExitCodeThread(other, &seen_zero);
    CloseHandle(other);
    check(seen_zero == 0, "another thread sees the held count");
    count.sub();
    check(count.zero(), "released");

    struct Race {
        mhw::perf::InFlightCount count;
        std::atomic<bool> flag{true}, stop{};
        std::atomic<unsigned> inside{}, violations{}, admitted{};
    };
    static Race race;
    std::array<HANDLE, 8> workers{};
    for (auto& w : workers)
        w = CreateThread(nullptr, 0, [](void*) -> DWORD {
            while (!race.stop.load()) {
                race.count.add();
                if (race.flag.load(std::memory_order_relaxed)) {
                    race.inside.fetch_add(1);
                    ++race.admitted;
                    for (volatile unsigned k = 0; k < 20; ++k) {}
                    race.inside.fetch_sub(1);
                }
                race.count.sub();
            }
            return 0;
        }, nullptr, 0, nullptr);
    for (unsigned round = 0; round < 200; ++round) {
        race.flag.store(true);
        Sleep(1);
        race.flag.store(false);
        const auto end = GetTickCount64() + 2000;
        bool drained = race.count.zero();
        while (!drained && GetTickCount64() < end) drained = race.count.idle();
        if (!drained || race.inside.load() != 0) ++race.violations;
    }
    race.stop.store(true);
    WaitForMultipleObjects(static_cast<DWORD>(workers.size()), workers.data(), TRUE, 60000);
    for (auto h : workers) CloseHandle(h);
    check(race.admitted.load() > 0, "the race admitted callbacks");
    check(race.violations.load() == 0, "after the drain no admitted callback is still inside");
}
}  // namespace

int main() {
    in_flight_tests();
    watch_filter_tests();
    hook_timing_tests();
    frame_stats_tests();
    region_tests();
    shared_cache_tests();
    region_level_tests();
    refresher_tests();
    sha_memo_tests();
    if (failures) return 1;
    std::printf("perf tests passed\n");
    return 0;
}
