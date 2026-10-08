#include "sampler.hpp"
#include "frame_stats.hpp"
#include "toggles.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace mhw::perf::sampler {
namespace {
constexpr unsigned module_capacity = 512, name_capacity = 512, frame_capacity = 64, thread_capacity = 1024;
constexpr unsigned cell_capacity = 1U << 15, rotation = 2, unknown_module = 0xFFF;

struct Module {
    std::uintptr_t base{}, end{};
    const RUNTIME_FUNCTION* table{};
    unsigned count{}, id{};
};
struct Thread {
    DWORD id{};
    HANDLE handle{};
    std::uintptr_t teb{};
};
struct Cell {
    std::uint64_t key{};
    std::uint32_t count{};
};
// Tables in a cell key: what ran, our innermost and outermost frame, and the
// outermost frame of ours together with the module that ran.
enum Table : unsigned { leaf, inner, outer, outer_leaf_module };

// Sampler thread only, apart from the names and cells (table_lock).
std::array<Module, module_capacity> modules;
unsigned module_count{};
std::array<std::array<char, 48>, name_capacity> names{};
unsigned name_count{};
unsigned self_id = unknown_module;
std::array<Thread, thread_capacity> threads;
unsigned thread_count{}, next_thread{};
std::array<Cell, cell_capacity> cells;
std::array<std::uint64_t, 2> samples{};
std::uint64_t failed{}, truncated{}, overflow{}, window_start{};
SRWLOCK table_lock = SRWLOCK_INIT;
std::atomic<bool> started{};
DWORD sampler_thread{};

using QueryThread = LONG(NTAPI*)(HANDLE, int, void*, ULONG, ULONG*);
QueryThread query_thread{};
struct BasicInformation {
    LONG exit_status;
    void* teb;
    void* client[2];
    ULONG_PTR affinity;
    LONG priority, base_priority;
};

unsigned name_id(const wchar_t* name) noexcept {
    std::array<char, 48> narrow{};
    for (unsigned i = 0; name[i] && i + 1 < narrow.size(); ++i) narrow[i] = name[i] < 128 ? static_cast<char>(name[i]) : '?';
    for (unsigned i = 0; i < name_count; ++i)
        if (names[i] == narrow) return i;
    if (name_count == name_capacity) return unknown_module;
    AcquireSRWLockExclusive(&table_lock);
    names[name_count] = narrow;
    const auto id = name_count++;
    ReleaseSRWLockExclusive(&table_lock);
    return id;
}
bool exception_table(std::uintptr_t base, const RUNTIME_FUNCTION*& table, unsigned& count) noexcept {
    __try {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + static_cast<std::uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
        const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        table = dir.VirtualAddress ? reinterpret_cast<const RUNTIME_FUNCTION*>(base + dir.VirtualAddress) : nullptr;
        count = table ? dir.Size / sizeof(RUNTIME_FUNCTION) : 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void refresh_modules() noexcept {
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    unsigned n{};
    HMODULE self{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&refresh_modules), &self);
    for (BOOL ok = Module32FirstW(snapshot, &entry); ok && n < module_capacity; ok = Module32NextW(snapshot, &entry)) {
        Module m;
        m.base = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
        m.end = m.base + entry.modBaseSize;
        if (!exception_table(m.base, m.table, m.count)) continue;
        m.id = name_id(entry.szModule);
        if (entry.hModule == self) self_id = m.id;
        modules[n++] = m;
    }
    CloseHandle(snapshot);
    std::sort(modules.begin(), modules.begin() + n, [](const Module& a, const Module& b) { return a.base < b.base; });
    module_count = n;
}
void refresh_threads() noexcept {
    const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;
    std::array<DWORD, thread_capacity> live{};
    unsigned n{};
    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    const auto pid = GetCurrentProcessId();
    for (BOOL ok = Thread32First(snapshot, &entry); ok && n < thread_capacity; ok = Thread32Next(snapshot, &entry))
        if (entry.th32OwnerProcessID == pid && entry.th32ThreadID != sampler_thread) live[n++] = entry.th32ThreadID;
    CloseHandle(snapshot);
    // Keep handles of threads that still exist; open the new ones.
    unsigned kept{};
    for (unsigned i = 0; i < thread_count; ++i) {
        if (std::find(live.begin(), live.begin() + n, threads[i].id) != live.begin() + n) threads[kept++] = threads[i];
        else CloseHandle(threads[i].handle);
    }
    thread_count = kept;
    for (unsigned i = 0; i < n && thread_count < thread_capacity; ++i) {
        bool known = false;
        for (unsigned j = 0; j < kept; ++j) known = known || threads[j].id == live[i];
        if (known) continue;
        const auto handle = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, live[i]);
        if (!handle) continue;
        BasicInformation info{};
        if (!query_thread || query_thread(handle, 0, &info, sizeof(info), nullptr) < 0 || !info.teb) {
            CloseHandle(handle);
            continue;
        }
        threads[thread_count++] = {live[i], handle, reinterpret_cast<std::uintptr_t>(info.teb)};
    }
}
const Module* find(std::uintptr_t pc) noexcept {
    const auto end = modules.begin() + module_count;
    auto it = std::upper_bound(modules.begin(), end, pc, [](std::uintptr_t v, const Module& m) { return v < m.base; });
    if (it == modules.begin()) return nullptr;
    --it;
    return pc < it->end ? &*it : nullptr;
}
const RUNTIME_FUNCTION* lookup(const Module& m, std::uintptr_t pc) noexcept {
    if (!m.table) return nullptr;
    const auto rva = static_cast<DWORD>(pc - m.base);
    unsigned lo = 0, hi = m.count;
    while (lo < hi) {
        const auto mid = (lo + hi) / 2;
        const auto& f = m.table[mid];
        if (rva < f.BeginAddress) hi = mid;
        else if (rva >= f.EndAddress) lo = mid + 1;
        else return &f;
    }
    return nullptr;
}
// Runs while the thread is suspended: reads its stack inside the TEB bounds,
// no allocation, no locks.
unsigned walk(CONTEXT& c, std::uintptr_t teb, std::array<std::uintptr_t, frame_capacity>& pcs, bool& cut) noexcept {
    unsigned n{};
    __try {
        const auto stack_base = *reinterpret_cast<const std::uintptr_t*>(teb + 8);
        const auto stack_limit = *reinterpret_cast<const std::uintptr_t*>(teb + 16);
        while (n < frame_capacity) {
            const auto pc = static_cast<std::uintptr_t>(c.Rip), sp = static_cast<std::uintptr_t>(c.Rsp);
            if (!pc || sp < stack_limit || sp + 8 > stack_base) break;
            pcs[n++] = pc;
            const auto* m = find(pc);
            if (!m) break;
            // A return address may follow a call that ends its function.
            const auto* f = lookup(*m, n == 1 ? pc : pc - 1);
            if (!f) {
                c.Rip = *reinterpret_cast<const DWORD64*>(sp);
                c.Rsp += 8;
            } else {
                void* data{};
                DWORD64 frame{};
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, m->base, pc, const_cast<PRUNTIME_FUNCTION>(f), &c, &data, &frame, nullptr);
            }
            if (c.Rsp <= sp) break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        cut = true;
    }
    return n;
}
void count(Table table, unsigned kind, unsigned module, std::uint32_t rva, unsigned extra) noexcept {
    const auto key = std::uint64_t(table) << 60 | std::uint64_t(kind) << 59 | std::uint64_t(module & 0xFFF) << 44 |
                     std::uint64_t(extra & 0xFFF) << 32 | rva;
    auto slot = static_cast<unsigned>((key * 0x9E3779B97F4A7C15ULL) >> 49) & (cell_capacity - 1);
    for (unsigned probe = 0; probe < 64; ++probe, slot = (slot + 1) & (cell_capacity - 1)) {
        auto& cell = cells[slot];
        if (cell.count && cell.key != key) continue;
        cell.key = key;
        ++cell.count;
        return;
    }
    ++overflow;
}
// Function start of a frame, for grouping.
void frame_id(std::uintptr_t pc, bool leaf_frame, unsigned& module, std::uint32_t& rva) noexcept {
    module = unknown_module;
    rva = 0;
    const auto* m = find(pc);
    if (!m) return;
    module = m->id;
    const auto* f = lookup(*m, leaf_frame ? pc : pc - 1);
    rva = f ? f->BeginAddress : static_cast<std::uint32_t>(pc - m->base);
}
void sample(Thread& t, unsigned kind) noexcept {
    std::array<std::uintptr_t, frame_capacity> pcs;
    CONTEXT c{};
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (SuspendThread(t.handle) == static_cast<DWORD>(-1)) {
        ++failed;
        return;
    }
    bool cut = false;
    unsigned n{};
    if (GetThreadContext(t.handle, &c)) n = walk(c, t.teb, pcs, cut);
    ResumeThread(t.handle);
    if (!n) {
        ++failed;
        return;
    }
    unsigned leaf_module{}, inner_module = unknown_module, outer_module = unknown_module;
    std::uint32_t leaf_rva{}, inner_rva{}, outer_rva{};
    frame_id(pcs[0], true, leaf_module, leaf_rva);
    for (unsigned i = 0; i < n; ++i) {
        unsigned module{};
        std::uint32_t rva{};
        frame_id(pcs[i], i == 0, module, rva);
        if (module != self_id) continue;
        if (inner_module == unknown_module) inner_module = module, inner_rva = rva;
        outer_module = module;
        outer_rva = rva;
    }
    AcquireSRWLockExclusive(&table_lock);
    ++samples[kind];
    if (cut || n == frame_capacity) ++truncated;
    count(leaf, kind, leaf_module, leaf_rva, 0);
    count(inner, kind, inner_module, inner_rva, 0);
    count(outer, kind, outer_module, outer_rva, 0);
    count(outer_leaf_module, kind, outer_module, outer_rva, leaf_module);
    ReleaseSRWLockExclusive(&table_lock);
}
DWORD WINAPI run(void*) noexcept {
    sampler_thread = GetCurrentThreadId();
    query_thread = reinterpret_cast<QueryThread>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
    const auto timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    std::uint64_t refreshed{};
    for (;;) {
        if (!on(sample_profile)) {
            Sleep(50);
            continue;
        }
        const auto now = GetTickCount64();
        if (now - refreshed >= 2000) {
            refresh_modules();
            refresh_threads();
            refreshed = now;
        }
        const auto present = present_thread.load(std::memory_order_relaxed);
        for (unsigned i = 0; i < thread_count; ++i)
            if (threads[i].id == present) sample(threads[i], 0);
        for (unsigned k = 0; k < rotation && thread_count; ++k) {
            auto& t = threads[next_thread++ % thread_count];
            if (t.id != present) sample(t, 1);
        }
        if (timer) {
            LARGE_INTEGER due{};
            due.QuadPart = -10000;  // 1 ms
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, 20);
        } else {
            Sleep(1);
        }
    }
}
}  // namespace

void start() noexcept {
    if (started.exchange(true)) return;
    window_start = GetTickCount64();
    if (const auto thread = CreateThread(nullptr, 0, run, nullptr, 0, nullptr)) CloseHandle(thread);
}

void write(std::ostream& out) {
    std::vector<Cell> taken;
    std::vector<std::array<char, 48>> known;
    std::array<std::uint64_t, 2> counted{};
    std::uint64_t failures{}, cuts{}, lost{};
    AcquireSRWLockExclusive(&table_lock);
    for (auto& cell : cells)
        if (cell.count) {
            taken.push_back(cell);
            cell = {};
        }
    known.assign(names.begin(), names.begin() + name_count);
    counted = samples;
    samples = {};
    failures = failed;
    cuts = truncated;
    lost = overflow;
    failed = truncated = overflow = 0;
    ReleaseSRWLockExclusive(&table_lock);
    const auto now = GetTickCount64();
    const auto elapsed = now - window_start;
    window_start = now;
    if (taken.empty()) return;
    std::sort(taken.begin(), taken.end(), [](const Cell& a, const Cell& b) { return a.count > b.count; });
    // Frames without an unwind entry are counted by instruction address, so a
    // hot leaf function can spread over many cells; the report regroups them.
    if (taken.size() > 2000) taken.resize(2000);
    out << "{\"event\":\"profile\",\"perf\":" << toggles.load() << ",\"ms\":" << elapsed << ",\"samples\":[" << counted[0] << ','
        << counted[1] << "],\"failed\":" << failures << ",\"truncated\":" << cuts << ",\"overflow\":" << lost << ",\"self\":" << self_id
        << ",\"modules\":[";
    for (unsigned i = 0; i < known.size(); ++i) out << (i ? "," : "") << '"' << known[i].data() << '"';
    out << "],\"cells\":[";
    for (unsigned i = 0; i < taken.size(); ++i) {
        const auto k = taken[i].key;
        out << (i ? "," : "") << '[' << (k >> 60) << ',' << (k >> 59 & 1) << ',' << (k >> 44 & 0xFFF) << ',' << (k & 0xFFFFFFFF) << ','
            << (k >> 32 & 0xFFF) << ',' << taken[i].count << ']';
    }
    out << "]}\n" << std::flush;
}
}  // namespace mhw::perf::sampler
