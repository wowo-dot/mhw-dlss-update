#pragma once
#include <cstdint>

namespace mhw::bridge_probe {
// End of the committed, readable, non-guard region holding address, or 0.
// With perf::cached_reads each thread keeps one answer per 64 KB block of
// address space (256 blocks, direct-mapped) for 100 ms of GetTickCount64
// instead of one VirtualQuery per field; every read stays SEH-guarded, so a
// region released within that window fails the read instead of the process.
std::uintptr_t readable_end(std::uintptr_t address) noexcept;
}  // namespace mhw::bridge_probe
