#include "recorder.hpp"
#include <windows.h>
#include <intrin.h>
#include <bit>
#include <cmath>
namespace mhw::projection_binding {
// Same bounded mutation contract as the accepted scoped-camera experiment:
// aligned committed RW data only, compare/exchange, no protection changes.
bool jitter_exchange(std::uintptr_t at,std::uint64_t expected,std::uint64_t value) noexcept {
  if(!at || (at&7))return false;
  MEMORY_BASIC_INFORMATION m{};
  if(VirtualQuery(reinterpret_cast<void *>(at),&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
     (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || (m.Protect&255)!=PAGE_READWRITE ||
     at<reinterpret_cast<std::uintptr_t>(m.BaseAddress) || m.RegionSize<8 ||
     at-reinterpret_cast<std::uintptr_t>(m.BaseAddress)>m.RegionSize-8)return false;
  __try {return static_cast<std::uint64_t>(_InterlockedCompareExchange64(reinterpret_cast<volatile long long *>(at),
      std::bit_cast<long long>(value),std::bit_cast<long long>(expected)))==expected;}
  __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool jitter_finite(std::uint64_t bits) noexcept {
  const auto x=std::bit_cast<float>(static_cast<unsigned>(bits)),y=std::bit_cast<float>(static_cast<unsigned>(bits>>32));
  return std::isfinite(x) && std::isfinite(y) && std::abs(x)<=0.5F && std::abs(y)<=0.5F && (x!=0 || y!=0);
}
}
