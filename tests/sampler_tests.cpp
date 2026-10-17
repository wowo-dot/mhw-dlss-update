// v0.12.2 stack sampler: a busy thread with a known call chain stands in for
// the host Present thread; its samples must name the leaf function, the
// innermost and the outermost function of this module.
#include "../src/perf/sampler.hpp"
#include "../src/perf/frame_stats.hpp"
#include "../src/perf/toggles.hpp"
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::atomic<bool> stop{};
volatile std::uint64_t sink{};

// A stack frame gives the function an unwind entry, like most game and driver
// code. Frames without one are recorded by instruction address instead.
__declspec(noinline) void busy_leaf() {
    volatile std::uint64_t local[16];
    for (unsigned i = 0; i < 4096; ++i) {
        local[i & 15] = sink;
        sink = sink * 6364136223846793005ULL + local[(i + 3) & 15];
    }
}
__declspec(noinline) void busy_outer() {
    while (!stop.load()) busy_leaf();
}
DWORD WINAPI busy_thread(void *) {
    busy_outer();
    return 0;
}
DWORD WINAPI idle_thread(void *) {
    while (!stop.load()) Sleep(5);
    return 0;
}
std::uint32_t rva(const void *function) {
    HMODULE self{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       static_cast<LPCWSTR>(function), &self);
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(function) -
                                      reinterpret_cast<std::uintptr_t>(self));
}
// cells: [table, kind, module, rva, extra, count]
std::vector<std::array<std::uint64_t, 6>> cells(const std::string &text) {
    std::vector<std::array<std::uint64_t, 6>> out;
    auto at = text.find("\"cells\":[");
    if (at == std::string::npos) return out;
    at += 9;
    while (at < text.size() && text[at] == '[') {
        std::array<std::uint64_t, 6> c{};
        ++at;
        for (unsigned i = 0; i < 6; ++i) {
            std::size_t used{};
            c[i] = std::stoull(text.substr(at, 24), &used);
            at += used + 1; // number and ',' or ']'
        }
        out.push_back(c);
        if (at < text.size() && text[at] == ',') ++at;
    }
    return out;
}
std::uint64_t field(const std::string &text, const char *name) {
    const auto at = text.find(name);
    return at == std::string::npos ? 0 : std::stoull(text.substr(at + std::char_traits<char>::length(name), 24));
}
} // namespace

int main() {
    int failures{};
    const auto check = [&](bool ok, const char *what) {
        if (!ok) {
            std::printf("FAIL: %s\n", what);
            ++failures;
        }
    };
    DWORD busy_id{}, idle_id{};
    const auto busy = CreateThread(nullptr, 0, busy_thread, nullptr, 0, &busy_id);
    const auto idle = CreateThread(nullptr, 0, idle_thread, nullptr, 0, &idle_id);
    mhw::perf::present_thread.store(busy_id);
    mhw::perf::sampler::start();
    std::ostringstream empty;
    mhw::perf::sampler::write(empty);
    check(empty.str().empty(), "no event while the switch is off");
    mhw::perf::toggles.store(mhw::perf::sample_profile);
    Sleep(1500);
    mhw::perf::toggles.store(0);
    Sleep(100);
    std::ostringstream out;
    mhw::perf::sampler::write(out);
    const auto text = out.str();
    stop.store(true);
    WaitForSingleObject(busy, 2000);
    WaitForSingleObject(idle, 2000);
    const auto self = field(text, "\"self\":");
    const auto present = field(text, "\"samples\":[");
    check(present >= 300, "present-thread samples at about 1 kHz");
    check(field(text, "\"failed\":") == 0, "no failed samples");
    std::array<std::uint64_t, 3> top_rva{}, top_count{};
    for (const auto &c : cells(text)) {
        if (c[1] != 0 || c[0] > 2 || c[2] != self) continue; // present thread, own module
        if (c[5] > top_count[c[0]]) top_count[c[0]] = c[5], top_rva[c[0]] = c[3];
    }
    check(top_rva[0] == rva(reinterpret_cast<const void *>(&busy_leaf)), "leaf table: busy_leaf");
    check(top_rva[1] == rva(reinterpret_cast<const void *>(&busy_leaf)), "innermost own frame: busy_leaf");
    check(top_rva[2] == rva(reinterpret_cast<const void *>(&busy_thread)), "outermost own frame: the thread procedure");
    check(top_count[0] * 10 >= present * 9, "the busy thread is almost always in busy_leaf");
    if (failures) {
        std::printf("%s\n", text.substr(0, 600).c_str());
        return 1;
    }
    std::printf("sampler tests passed (%llu present samples)\n", static_cast<unsigned long long>(present));
    return 0;
}
