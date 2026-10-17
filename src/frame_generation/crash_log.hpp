#pragma once
#include <windows.h>
#include <cstdint>
#include <filesystem>

// Diagnostic only (FG target). A first vectored exception handler that records
// a fatal exception raised inside the host executable: code, faulting RVA,
// integer registers, an unwound call stack as module+RVA, and an optional
// caller-supplied context line. An unhandled-exception filter records the same
// for a crash in any module, then calls the previous filter. Neither handles
// the exception; the original search continues unchanged. At most four records
// are written.
namespace mhw::frame_generation::crash_log {
using ContextWriter = int (*)(char *buffer, int size) noexcept;
bool install(const std::filesystem::path &file, ContextWriter context = nullptr) noexcept;
// A code address as module basename + offset ("name.dll+0x1f00", no path), or
// the raw address ("0x...") when it is in no module. Allocation-free (the
// handler uses it); returns what snprintf returns.
int code_site(char *out, int size, std::uintptr_t address) noexcept;
} // namespace mhw::frame_generation::crash_log
