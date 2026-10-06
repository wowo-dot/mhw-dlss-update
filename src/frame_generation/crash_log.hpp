#pragma once
#include <windows.h>
#include <filesystem>

// Diagnostic only (FG target). A first vectored exception handler that records
// a fatal exception raised inside the host executable: code, faulting RVA,
// integer registers, an unwound call stack as module+RVA, and an optional
// caller-supplied context line. It never handles the exception; the original
// search continues unchanged. At most four records are written.
namespace mhw::frame_generation::crash_log {
using ContextWriter = int (*)(char *buffer, int size) noexcept;
bool install(const std::filesystem::path &file, ContextWriter context = nullptr) noexcept;
}
