// The vendored MinHook's process-only thread list (v0.28.6): Windows PSS lists
// this process's other threads, and hooks are made, enabled and removed with it.
#include "../src/hooking/thread_snapshot.hpp"
#include <MinHook.h>
#include <windows.h>
#include <cstdio>

namespace {
int failures = 0;
void check(bool ok, const char *what) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}
volatile int sink = 0;
// Long enough for MinHook's 5-byte jump without relying on padding.
__declspec(noinline) int target(int x) {
    int total = x;
    for (int i = 0; i < 4; ++i) total = total * 3 + sink;
    return total;
}
int (*original)(int) = nullptr;
__declspec(noinline) int detour(int x) { return original(x) + 1000; }
DWORD WINAPI wait_for(void *event) {
    WaitForSingleObject(static_cast<HANDLE>(event), INFINITE);
    return 0;
}
}  // namespace

int main() {
    const HANDLE go = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE threads[3]{};
    DWORD ids[3]{};
    for (unsigned i = 0; i < 3; ++i) threads[i] = CreateThread(nullptr, 0, wait_for, go, 0, &ids[i]);

    LPDWORD list = nullptr;
    UINT count = 0;
    check(al_enumerate_process_threads(GetProcessHeap(), &list, &count) != FALSE, "PSS lists the process's threads");
    unsigned found = 0;
    bool self = false;
    for (UINT i = 0; i < count; ++i) {
        for (const auto id : ids) found += list[i] == id;
        self = self || list[i] == GetCurrentThreadId();
    }
    check(found == 3, "the three waiting threads are listed");
    check(!self, "the calling thread is not listed");
    if (list) HeapFree(GetProcessHeap(), 0, list);

    const auto before = al::hooks::thread_snapshot_counts();
    const int plain = target(2);
    check(MH_Initialize() == MH_OK, "MinHook initializes");
    check(MH_CreateHook(reinterpret_cast<void *>(&target), reinterpret_cast<void *>(&detour),
                        reinterpret_cast<void **>(&original)) == MH_OK, "hook made");
    check(MH_EnableHook(reinterpret_cast<void *>(&target)) == MH_OK, "hook enabled");
    check(target(2) == plain + 1000, "the detour runs");
    check(MH_DisableHook(reinterpret_cast<void *>(&target)) == MH_OK, "hook removed");
    check(target(2) == plain, "the original runs again");
    check(MH_Uninitialize() == MH_OK, "MinHook shuts down");
    const auto after = al::hooks::thread_snapshot_counts();
    check(after.succeeded - before.succeeded >= 2, "enable and disable froze threads through PSS");

    SetEvent(go);
    WaitForMultipleObjects(3, threads, TRUE, 5000);
    for (const auto t : threads) CloseHandle(t);
    CloseHandle(go);
    if (failures) return 1;
    std::puts("minhook_threads_tests: ok");
    return 0;
}
