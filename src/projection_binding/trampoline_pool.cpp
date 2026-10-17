#include "recorder.hpp"
#include <windows.h>
#include <MinHook.h>
extern "C" {
#include <buffer.h>
}
namespace mhw::projection_binding {
namespace {
PoolState pool;
constexpr std::uintptr_t radius=0x40000000,block_size=4096;
}
const PoolState &pool_state() noexcept {return pool;}
bool reserve_pool(const std::array<std::uintptr_t,10> &targets,std::string &reason) {
  // Worker-only, before any hook creation. MinHook's private buffer API is
  // deliberately tied to the repository's pinned v1.3.4 implementation.
  if(pool.attempted) {reason="pool-already-attempted";return false;}
  pool.attempted=true;pool.origin=targets[0];
  for(const auto target:targets) if(target<radius || target>UINTPTR_MAX-radius) {reason="pool-origin-range";return false;}
  PROCESS_MITIGATION_DYNAMIC_CODE_POLICY policy{};
  pool.policy_available=GetProcessMitigationPolicy(GetCurrentProcess(),ProcessDynamicCodePolicy,&policy,sizeof(policy))!=FALSE;
  pool.policy_flags=policy.Flags;
  if(!pool.policy_available || policy.ProhibitDynamicCode) {pool.win32=GetLastError();reason="pool-dynamic-code-policy";return false;}
  pool.status=MH_Initialize();
  if(pool.status!=MH_OK) {reason="pool-initialize";return false;}
  SetLastError(ERROR_SUCCESS);
  pool.slot=reinterpret_cast<std::uintptr_t>(AllocateBuffer(reinterpret_cast<void *>(pool.origin)));
  pool.win32=GetLastError();
  if(!pool.slot) {pool.status=MH_ERROR_MEMORY_ALLOC;reason="pool-allocation";return false;}
  pool.page=pool.slot&~std::uintptr_t{block_size-1};
  MEMORY_BASIC_INFORMATION m{};
  bool valid=VirtualQuery(reinterpret_cast<void *>(pool.slot),&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
    m.Type==MEM_PRIVATE && m.Protect==PAGE_EXECUTE_READWRITE && reinterpret_cast<std::uintptr_t>(m.AllocationBase)==pool.page &&
    reinterpret_cast<std::uintptr_t>(m.BaseAddress)==pool.page && m.RegionSize>=block_size;
  for(const auto target:targets)valid=valid && pool.page>=target-radius && pool.page<target+radius-(block_size-1);
  if(!valid) {reason="pool-reachability";return false;}
  // Holding one unused 64-byte slot pins a freshly allocated 4 KiB block. Its
  // other 62 slots are available to later MH_CreateHook calls. No target code
  // is read or patched here; the reservation lives until the pinned DLL exits.
  pool.ready=true;return true;
}
}
