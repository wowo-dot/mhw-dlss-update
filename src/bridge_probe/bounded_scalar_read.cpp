#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "renderer_snapshot.hpp"
#include <cstring>
#include <windows.h>

namespace mhw::bridge_probe {

ScalarReadResult
read_bounded_scalar(void *, const std::uintptr_t address,
                    const std::span<std::byte> output) noexcept {
  if (address == 0U || output.empty() || output.size() > 8U ||
      output.data() == nullptr)
    return ScalarReadResult::unreadable;
  MEMORY_BASIC_INFORMATION memory{};
  if (VirtualQuery(reinterpret_cast<const void *>(address), &memory,
                   sizeof(memory)) != sizeof(memory) ||
      memory.State != MEM_COMMIT ||
      (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U)
    return ScalarReadResult::unreadable;
  const DWORD access = memory.Protect & 0xFFU;
  if (access != PAGE_READONLY && access != PAGE_READWRITE &&
      access != PAGE_WRITECOPY && access != PAGE_EXECUTE_READ &&
      access != PAGE_EXECUTE_READWRITE && access != PAGE_EXECUTE_WRITECOPY)
    return ScalarReadResult::unreadable;
  const auto base = reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
  if (address < base || memory.RegionSize < output.size() ||
      address - base > memory.RegionSize - output.size())
    return ScalarReadResult::unreadable;
  __try {
    std::memcpy(output.data(), reinterpret_cast<const void *>(address),
                output.size());
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return ScalarReadResult::fault;
  }
  return ScalarReadResult::success;
}

} // namespace mhw::bridge_probe
