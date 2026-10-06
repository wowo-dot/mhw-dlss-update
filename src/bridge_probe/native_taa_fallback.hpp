#pragma once
#include <cstdint>
#include <string_view>
namespace mhw::bridge_probe {
struct NativeTaaFallbackStats {
  bool installed{};
  unsigned install_status{}; // 0 idle, 1 checking, 2 code rejected, 3 hook rejected, 4 installed
  std::uint64_t suppressed{}, with_handle{}, unreadable{}, other_callers{};
};
// Called only after the bridge's exact host/disk/API/creation checks, before
// issuing its diagnostic token. Verifies the complete producer and TAA caller.
bool install_native_taa_fallback(std::uintptr_t host_base, bool depth_probe=false) noexcept;
bool native_code_span_verified(std::uintptr_t base,unsigned rva,unsigned length,std::string_view digest) noexcept;
NativeTaaFallbackStats native_taa_fallback_stats() noexcept;
#ifdef MHW_OWNED_FALLBACK_TEST
bool install_owned_taa_fallback(std::uintptr_t target, std::uintptr_t caller, bool depth_probe=false) noexcept;
bool remove_owned_taa_fallback() noexcept;
#endif
} // namespace mhw::bridge_probe
