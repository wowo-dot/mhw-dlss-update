#include "crash_log.hpp"
#include <atomic>
#include <cstdio>

namespace mhw::frame_generation::crash_log {
namespace {
HANDLE file = INVALID_HANDLE_VALUE;
ContextWriter context_writer{};
std::uintptr_t host_begin{}, host_end{};
std::atomic<unsigned> records{};
std::atomic_flag writing = ATOMIC_FLAG_INIT;

bool fatal(DWORD code) {
    return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
           code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_STACK_OVERFLOW ||
           code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED || code == 0xC0000409;
}

// module basename + RVA, or the raw address when it is in no module.
int describe(char *out, int size, DWORD64 address) {
    HMODULE module{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(address), &module))
        return std::snprintf(out, static_cast<size_t>(size), "\"0x%llx\"", static_cast<unsigned long long>(address));
    wchar_t path[MAX_PATH]{};
    const auto length = GetModuleFileNameW(module, path, MAX_PATH);
    const wchar_t *name = path;
    for (DWORD i = 0; i < length; ++i)
        if (path[i] == L'\\' || path[i] == L'/') name = path + i + 1;
    return std::snprintf(out, static_cast<size_t>(size), "\"%ls+0x%llx\"", name,
                         static_cast<unsigned long long>(address - reinterpret_cast<DWORD64>(module)));
}

// Unwinds a copy of the faulting context; a bad frame ends the walk instead of
// faulting inside the handler.
int walk_stack(CONTEXT walk, char *out, int size) {
    int n = 0;
    __try {
        for (unsigned frame = 0; frame < 24 && walk.Rip && n < size - 300; ++frame) {
            if (frame) out[n++] = ',';
            n += describe(out + n, size - n, walk.Rip);
            DWORD64 image{};
            const auto *function = RtlLookupFunctionEntry(walk.Rip, &image, nullptr);
            if (function) {
                void *handler_data{};
                DWORD64 establisher{};
                RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, walk.Rip, const_cast<PRUNTIME_FUNCTION>(function), &walk,
                                 &handler_data, &establisher, nullptr);
            } else {  // Leaf function: the return address is at RSP.
                walk.Rip = *reinterpret_cast<const DWORD64 *>(walk.Rsp);
                walk.Rsp += 8;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return n;
}

LONG CALLBACK handler(EXCEPTION_POINTERS *info) {
    const auto *record = info->ExceptionRecord;
    const auto rip = reinterpret_cast<std::uintptr_t>(record->ExceptionAddress);
    // Only fatal-class exceptions raised in host code; our own guarded reads
    // fault inside this DLL and are handled by their __except blocks.
    if (!fatal(record->ExceptionCode) || rip < host_begin || rip >= host_end || file == INVALID_HANDLE_VALUE)
        return EXCEPTION_CONTINUE_SEARCH;
    if (records.load() >= 4 || writing.test_and_set()) return EXCEPTION_CONTINUE_SEARCH;
    static char line[16384];
    const auto *c = info->ContextRecord;
    int n = std::snprintf(line, sizeof line,
        "{\"event\":\"host_exception\",\"code\":%lu,\"rva\":%llu,\"thread\":%lu,\"access\":[%llu,%llu],"
        "\"rax\":%llu,\"rbx\":%llu,\"rcx\":%llu,\"rdx\":%llu,\"rsi\":%llu,\"rdi\":%llu,\"rbp\":%llu,\"rsp\":%llu,"
        "\"r8\":%llu,\"r9\":%llu,\"r10\":%llu,\"r11\":%llu,\"r12\":%llu,\"r13\":%llu,\"r14\":%llu,\"r15\":%llu,\"stack\":[",
        record->ExceptionCode, static_cast<unsigned long long>(rip - host_begin), GetCurrentThreadId(),
        record->NumberParameters > 0 ? static_cast<unsigned long long>(record->ExceptionInformation[0]) : 0ULL,
        record->NumberParameters > 1 ? static_cast<unsigned long long>(record->ExceptionInformation[1]) : 0ULL,
        c->Rax, c->Rbx, c->Rcx, c->Rdx, c->Rsi, c->Rdi, c->Rbp, c->Rsp, c->R8, c->R9, c->R10, c->R11, c->R12, c->R13,
        c->R14, c->R15);
    n += walk_stack(*c, line + n, static_cast<int>(sizeof line) - n);
    n += std::snprintf(line + n, sizeof line - static_cast<size_t>(n), "],\"context\":");
    const int extra = context_writer ? context_writer(line + n, static_cast<int>(sizeof line) - n - 8) : 0;
    n += extra > 0 ? extra : std::snprintf(line + n, sizeof line - static_cast<size_t>(n), "null");
    n += std::snprintf(line + n, sizeof line - static_cast<size_t>(n), "}\n");
    DWORD written{};
    WriteFile(file, line, static_cast<DWORD>(n), &written, nullptr);
    FlushFileBuffers(file);
    records.fetch_add(1);
    writing.clear();
    return EXCEPTION_CONTINUE_SEARCH;
}
}  // namespace

bool install(const std::filesystem::path &path, ContextWriter context) noexcept {
    const auto host = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(host);
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(host + static_cast<std::uintptr_t>(dos->e_lfanew));
    host_begin = host;
    host_end = host + nt->OptionalHeader.SizeOfImage;
    context_writer = context;
    file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    return file != INVALID_HANDLE_VALUE && AddVectoredExceptionHandler(1, handler) != nullptr;
}
}  // namespace mhw::frame_generation::crash_log
