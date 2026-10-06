#include "native_taa_fallback.hpp"
#include "native_depth_probe.hpp"
#include "renderer_snapshot.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

namespace mhw::bridge_probe {
namespace {
using Producer = std::uintptr_t(__fastcall *)(std::uintptr_t);
Producer original{};
std::uintptr_t expected_caller{}, hooked_target{};
std::atomic<bool> installed{};
bool depth_probe_enabled{};
std::atomic<unsigned> install_status{};
std::atomic<std::uint64_t> suppressed{}, with_handle{}, unreadable{}, other_callers{};

bool read_address(std::uintptr_t at, std::uintptr_t &value) noexcept {
  return read_bounded_scalar(nullptr, at, std::as_writable_bytes(std::span(&value, 1))) == ScalarReadResult::success;
}
bool executable_image(std::uintptr_t at, std::uintptr_t base) noexcept {
  MEMORY_BASIC_INFORMATION memory{};
  if (!at || VirtualQuery(reinterpret_cast<void *>(at), &memory, sizeof(memory)) != sizeof(memory)) return false;
  const auto access = memory.Protect & 0xff;
  return memory.Type == MEM_IMAGE && memory.State == MEM_COMMIT &&
      reinterpret_cast<std::uintptr_t>(memory.AllocationBase) == base &&
      !(memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
      (access == PAGE_EXECUTE_READ || access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY);
}
bool code_digest(std::uintptr_t at, unsigned length, std::string_view expected) {
  std::vector<UCHAR> bytes(length);
  for (unsigned i = 0; i < length;) {
    const auto count = (std::min)(8U, length - i);
    if (read_bounded_scalar(nullptr, at + i, {reinterpret_cast<std::byte *>(bytes.data() + i), count}) != ScalarReadResult::success)
      return false;
    i += count;
  }
  BCRYPT_ALG_HANDLE algorithm{};
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return false;
  std::array<UCHAR, 32> hash{};
  const auto status = BCryptHash(algorithm, nullptr, 0, bytes.data(), length, hash.data(), static_cast<ULONG>(hash.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (status != 0 || expected.size() != 64) return false;
  constexpr char digits[] = "0123456789abcdef";
  for (unsigned i = 0; i < hash.size(); ++i)
    if (expected[2 * i] != digits[hash[i] >> 4] || expected[2 * i + 1] != digits[hash[i] & 15]) return false;
  return true;
}
}

// The exact caller ignores the return value and has already recorded its TAA
// dispatch. Skipping this optional producer prevents dense-velocity/DLSS work
// from being queued. No fake Evaluate, output copy or state helper is called.
extern "C" __declspec(dllexport) __declspec(noinline) std::uintptr_t __fastcall
native_taa_fallback_producer(std::uintptr_t args) noexcept {
  const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
  if (caller != expected_caller) {
    other_callers.fetch_add(1, std::memory_order_relaxed);
    return original(args);
  }
  const auto last_error = GetLastError();
  std::uintptr_t wrapper_slot{}, wrapper{};
  if (!read_address(args, wrapper_slot) || !read_address(wrapper_slot, wrapper))
    unreadable.fetch_add(1, std::memory_order_relaxed);
  else if (wrapper)
    with_handle.fetch_add(1, std::memory_order_relaxed);
  if (depth_probe_enabled) observe_native_jitter_sequence(args, wrapper != 0);
  if(wrapper && depth_probe_enabled && admit_native_depth_producer(args)) {
    SetLastError(last_error);
    const auto result = original(args);
    const auto producer_error = GetLastError();
    publish_native_depth_producer(result);
    SetLastError(producer_error);
    return result;
  }
  suppressed.fetch_add(1, std::memory_order_relaxed);
  SetLastError(last_error);
  return 0;
}

namespace {
bool install(std::uintptr_t target, std::uintptr_t caller) noexcept {
  if (installed.load(std::memory_order_acquire)) return target == hooked_target && caller == expected_caller;
  const auto init = MH_Initialize();
  if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) return false;
  void *trampoline{};
  if (MH_CreateHook(reinterpret_cast<void *>(target), reinterpret_cast<void *>(&native_taa_fallback_producer), &trampoline) != MH_OK)
    return false;
  if (!trampoline) { MH_RemoveHook(reinterpret_cast<void *>(target)); return false; }
  original = reinterpret_cast<Producer>(trampoline);
  expected_caller = caller; hooked_target = target;
  if (MH_EnableHook(reinterpret_cast<void *>(target)) != MH_OK) {
    // Keep a valid trampoline even on an uncertain enable failure. The bridge
    // refuses Create, and a reached wrapper can still forward other callers.
    return false;
  }
  installed.store(true, std::memory_order_release);
  return true;
}
}

bool native_code_span_verified(std::uintptr_t base,unsigned rva,unsigned length,std::string_view digest) noexcept {
  if(!base || !length || rva > (std::numeric_limits<unsigned>::max)()-length ||
      base>(std::numeric_limits<std::uintptr_t>::max)()-rva-length) return false;
  try { return executable_image(base+rva,base) && executable_image(base+rva+length-1,base) && code_digest(base+rva,length,digest); }
  catch(...) { return false; }
}
bool install_native_taa_fallback(std::uintptr_t base,bool depth_probe) noexcept {
  if (installed.load(std::memory_order_acquire)) return hooked_target == base + 0x23913f0U && depth_probe_enabled==depth_probe;
  install_status.store(2, std::memory_order_release);
  if (!base || base > std::numeric_limits<std::uintptr_t>::max() - 0x23931cdU) return false;
  try {
    if (!executable_image(base + 0x23913f0U, base) || !executable_image(base + 0x23916d0U, base) ||
        !executable_image(base + 0x2392150U, base) || !executable_image(base + 0x23931ccU, base)) return false;
    if (!code_digest(base + 0x23913f0U, 0x2e1U, "0fa009dc0ff13af4f39d5305a9a3392e5274b637230d7a531006a13dcfacb060") ||
        !code_digest(base + 0x2392150U, 0x107dU, "d2c6c0f6a5ed39c998bcf62afed19132e6dd0d7cfbb19e18b80c15531af2bf91")) return false;
    install_status.store(3, std::memory_order_release);
    if(depth_probe && !install_native_depth_probe(base)) return false;
    depth_probe_enabled=depth_probe;
    const bool ready = install(base + 0x23913f0U, base + 0x2392f8eU);
    if (ready) install_status.store(4, std::memory_order_release);
    return ready;
  } catch (...) { return false; }
}
NativeTaaFallbackStats native_taa_fallback_stats() noexcept {
  return {installed.load(std::memory_order_acquire), install_status.load(std::memory_order_acquire), suppressed.load(std::memory_order_relaxed),
    with_handle.load(std::memory_order_relaxed), unreadable.load(std::memory_order_relaxed), other_callers.load(std::memory_order_relaxed)};
}
#ifdef MHW_OWNED_FALLBACK_TEST
bool install_owned_taa_fallback(std::uintptr_t target, std::uintptr_t caller,bool depth_probe) noexcept {
  const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
  depth_probe_enabled=depth_probe;
  return executable_image(target, base) && executable_image(caller, base) && install(target, caller);
}
bool remove_owned_taa_fallback() noexcept {
  if (MH_DisableHook(reinterpret_cast<void *>(hooked_target)) != MH_OK ||
      MH_RemoveHook(reinterpret_cast<void *>(hooked_target)) != MH_OK) return false;
  installed.store(false, std::memory_order_release); return true;
}
#endif
} // namespace mhw::bridge_probe
