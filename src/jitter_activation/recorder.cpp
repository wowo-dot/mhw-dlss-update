#include "recorder.hpp"
#include "renderer_snapshot.hpp"
#include <windows.h>
#include <intrin.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <span>

namespace mhw::jitter_activation {
namespace {
using Original=bool(__fastcall *)(std::uintptr_t,std::uintptr_t);
Original original{};
std::uintptr_t base{},target{};
bool created{},enabled{},used{};
InstallState install_report{};
SRWLOCK admission=SRWLOCK_INIT;
std::array<Call,capacity> calls{};
std::atomic<Stop> status{Stop::idle};
std::atomic<unsigned> reserved{},pending{},visits{},dropped{},injections{},restorations{};
std::uint64_t started{};
template<class T> bool read(std::uintptr_t at,T &out) noexcept {
  return bridge_probe::read_bounded_scalar(nullptr,at,std::as_writable_bytes(std::span(&out,1)))==bridge_probe::ScalarReadResult::success;
}
void end(Stop why) noexcept { auto s=Stop::recording; status.compare_exchange_strong(s,why); }
// A guarded compare/exchange never overwrites an unexpected concurrent value.
bool exchange(std::uintptr_t at,std::uint64_t expected,std::uint64_t value) noexcept {
  if (!at || (at&7)) return false;
  MEMORY_BASIC_INFORMATION m{};
  if (VirtualQuery(reinterpret_cast<void *>(at),&m,sizeof(m))!=sizeof(m) || m.State!=MEM_COMMIT ||
      (m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) || (m.Protect&255)!=PAGE_READWRITE ||
      at<reinterpret_cast<std::uintptr_t>(m.BaseAddress) || m.RegionSize<8 ||
      at-reinterpret_cast<std::uintptr_t>(m.BaseAddress)>m.RegionSize-8) return false;
  __try {
    return static_cast<std::uint64_t>(_InterlockedCompareExchange64(reinterpret_cast<volatile long long *>(at),
        std::bit_cast<long long>(value),std::bit_cast<long long>(expected)))==expected;
  } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool pair_finite(std::uint64_t bits) noexcept {
  const auto x=std::bit_cast<float>(static_cast<unsigned>(bits)),y=std::bit_cast<float>(static_cast<unsigned>(bits>>32));
  return std::isfinite(x) && std::isfinite(y) && std::abs(x)<=0.5F && std::abs(y)<=0.5F && (x!=0 || y!=0);
}
bool byte(std::uintptr_t at,unsigned &out) noexcept {
  unsigned char value{}; const bool ok=read(at,value); out=value; return ok;
}
bool provider(ProviderState &s,std::uintptr_t renderer) noexcept {
  s.valid=byte(renderer+0xa5,s.use_dlss) && read(renderer+0xb4,s.sizing_flags) &&
      read(renderer+0x78,s.backend) && s.use_dlss<=1 && s.backend &&
      s.backend<=UINTPTR_MAX-0x42708 && read(s.backend+0x42700,s.wrapper);
  return s.valid;
}
bool excluded(const ProviderState &s) noexcept {
  // mUseDLSS is a renderer switch, not the menu setting or a live handle.
  return s.valid && !(s.sizing_flags&1U) && s.wrapper==0;
}
bool anchors(Call &r,std::uintptr_t context) noexcept {
  auto &a=r.anchor;
  std::uintptr_t camera{};
  r.admission_checkpoint=1;
  if (!read(base+0x51c4480,a.renderer) || !read(base+0x51c4368,a.table) || !read(base+0x51c4400,camera) ||
      !a.renderer || !a.table || camera!=r.camera || !camera || a.renderer>UINTPTR_MAX-0x1f570) return false;
  a.context=context;
  r.admission_checkpoint=2;
  if (context!=a.renderer+0x710 || !read(camera,r.camera_type) ||
      (r.camera_type!=base+0x350d188 && r.camera_type!=base+0x33f82a8) || camera>UINTPTR_MAX-0x76 ||
      r.projection_object!=camera+0x50) return false;
  r.admission_checkpoint=3;
  if (!byte(r.projection_object+0x20,r.projection_active) || !r.projection_active ||
      !byte(r.projection_object+0x25,r.display_index) || r.display_index) return false;
  r.admission_checkpoint=4;
  if (!read(camera+0x38,r.original_jitter) || r.original_jitter!=0 || !read(camera+0x40,r.camera_stamp) ||
      !read(a.renderer+0x1f210,a.frame) || !read(a.renderer+0x1f208,a.epoch) || r.camera_stamp!=static_cast<unsigned>(a.epoch)) return false;
  r.admission_checkpoint=5;
  if (!read(a.renderer+0x1f450,a.view) || !a.view || a.view>UINTPTR_MAX-0x5c ||
      !read(a.view+0x58,r.view_flags) || (r.view_flags&0x840000)) return false;
  r.admission_checkpoint=6;
  if (!provider(r.provider_before,a.renderer) || !excluded(r.provider_before)) return false;
  r.admission_checkpoint=7;
  if (!read(a.renderer+0x1f200,a.bank) || a.bank>1 || !byte(a.renderer+0x93,a.async_mode) || a.async_mode>1) return false;
  r.admission_checkpoint=8;
  if (a.table>UINTPTR_MAX-0xe740 ||
      !read(a.table+0xe6a0+8*(a.frame%20),r.candidate) || !pair_finite(r.candidate)) return false;
  r.admission_checkpoint=100; return true;
}
unsigned before(std::uintptr_t projection_object,std::uintptr_t context,std::uintptr_t caller) noexcept {
  if (status.load()!=Stop::recording) return 0;
  if (GetTickCount64()-started>=window_ms) { end(Stop::deadline); return 0; }
  if (visits.fetch_add(1)>=visit_limit) { end(Stop::visits); return 0; }
  std::uintptr_t current{},renderer{};
  if (!read(base+0x51c4400,current) || !read(base+0x51c4480,renderer)) { end(Stop::invalid); return 0; }
  if (!current || current>UINTPTR_MAX-0x50 || projection_object!=current+0x50 || !renderer || renderer>UINTPTR_MAX-0x710 || context!=renderer+0x710) return 0;
#ifndef MHW_ACTIVATION_OWNED_FIXTURE
  if (caller!=base+0x228e834) return 0;
#endif
  if (!TryAcquireSRWLockExclusive(&admission)) { dropped.fetch_add(1); end(Stop::invalid); return 0; }
  if (status.load()!=Stop::recording) { ReleaseSRWLockExclusive(&admission); return 0; }
  const auto index=reserved.load();
  if (index>=capacity) { end(Stop::complete); ReleaseSRWLockExclusive(&admission); return 0; }
  Call candidate{}; candidate.stage=index; candidate.camera=current; candidate.projection_object=projection_object; candidate.caller=caller;
  candidate.tick=GetTickCount64(); candidate.thread=GetCurrentThreadId();
  const bool valid=anchors(candidate,context);
  if (valid && index && candidate.anchor.frame==calls[index-1].anchor.frame && candidate.anchor.epoch==calls[index-1].anchor.epoch) {
    ReleaseSRWLockExclusive(&admission); return 0;
  }
  pending.store(1); calls[index]=candidate; reserved.store(index+1);
  auto &r=calls[index];
  if (!valid) { r.rejection=1; end(Stop::invalid); return index+1; }
  if (index && (r.camera!=calls[0].camera || r.camera_type!=calls[0].camera_type || r.anchor.renderer!=calls[0].anchor.renderer ||
      r.anchor.context!=calls[0].anchor.context || r.anchor.view!=calls[0].anchor.view || r.anchor.table!=calls[0].anchor.table ||
      r.thread!=calls[0].thread || r.anchor.frame<=calls[index-1].anchor.frame || r.anchor.epoch<=calls[index-1].anchor.epoch)) {
    r.rejection=2; end(Stop::invalid); return index+1;
  }
  if (index==1) {
    if (!exchange(r.camera+0x38,r.original_jitter,r.candidate)) { r.rejection=3; end(Stop::invalid); }
    else { r.injected=true; injections.fetch_add(1); }
  }
  return index+1;
}
void after(unsigned token,bool result) noexcept {
  auto &r=calls[token-1]; r.returned=true; r.result=result;
  if (!r.rejection) {
    r.projection=bridge_probe::capture_projection_snapshot(base,r.anchor,bridge_probe::read_bounded_scalar,nullptr);
    const auto observed=static_cast<std::uint64_t>(r.projection.jitter_bits[0]) | (static_cast<std::uint64_t>(r.projection.jitter_bits[1])<<32);
    const auto sample=static_cast<std::uint64_t>(r.projection.table20_bits[0]) | (static_cast<std::uint64_t>(r.projection.table20_bits[1])<<32);
    if (!result || !r.projection.valid || !r.projection.matrix_relation || r.projection.manager!=r.camera ||
        r.projection.vtable!=r.camera_type || sample!=r.candidate || observed!=(r.injected?r.candidate:r.original_jitter)) r.rejection=4;
    if (r.stage && r.projection.viewport_bits!=calls[0].projection.viewport_bits) r.rejection=5;
    if (!provider(r.provider_after,r.anchor.renderer) || !excluded(r.provider_after) || r.provider_after!=r.provider_before) r.rejection=9;
    if (r.rejection) end(Stop::invalid);
  }
}
void finish(unsigned token) noexcept {
  auto &r=calls[token-1];
  if (r.injected) {
    r.restored=exchange(r.camera+0x38,r.candidate,r.original_jitter);
    if (r.restored) restorations.fetch_add(1);
    else { r.rejection=6; end(Stop::invalid); }
  } else r.restored=true;
  if (!read(r.camera+0x38,r.restored_jitter) || r.restored_jitter!=r.original_jitter) { r.rejection=7; end(Stop::invalid); }
  if (!r.returned) { r.rejection=8; end(Stop::invalid); }
  r.exit_tick=GetTickCount64(); r.complete=true;
  if (r.stage==2 && !r.rejection) end(Stop::complete);
  pending.store(0,std::memory_order_release);
  ReleaseSRWLockExclusive(&admission);
}
// RCX is a borrowed embedded projection object (camera+0x50 for slot zero),
// RDX is the context. The complete, hashed target returns bool in AL.
// SEH finally restores our camera mutation on normal return and exception unwind.
bool __fastcall entry(std::uintptr_t camera,std::uintptr_t context) {
  const auto incoming=GetLastError();
  const auto token=before(camera,context,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
  bool result=false,returned=false; DWORD outgoing=incoming;
  SetLastError(incoming);
  __try {
    result=original(camera,context); outgoing=GetLastError(); returned=true;
    if (token) after(token,result);
  } __finally {
    const auto error=returned?outgoing:GetLastError();
    if (token) finish(token);
    SetLastError(error);
  }
  return result;
}
bool install(std::uintptr_t at,std::uintptr_t image) noexcept {
  if (created) return false;
  SetLastError(ERROR_SUCCESS);
  const auto init=MH_Initialize();
  install_report={1,init,GetLastError(),false,false};
  if (init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) return false;
  base=image; target=at;
  void *trampoline{};
  SetLastError(ERROR_SUCCESS);
  const auto create=MH_CreateHook(reinterpret_cast<void *>(at),reinterpret_cast<void *>(&entry),&trampoline);
  install_report={2,create,GetLastError(),create==MH_OK,false};
  if (create!=MH_OK) return false;
  created=true; original=reinterpret_cast<Original>(trampoline);
  SetLastError(ERROR_SUCCESS);
  const auto enable=MH_EnableHook(reinterpret_cast<void *>(at));
  install_report={3,enable,GetLastError(),true,enable==MH_OK};
  if (enable!=MH_OK) { disable(); return false; }
  enabled=true; return true;
}
}
InstallState install_state() noexcept { return install_report; }
const char *install_status_name() noexcept { return MH_StatusToString(static_cast<MH_STATUS>(install_report.status)); }
// Exact-code gate implementation is separate from hook-time operations.
bool verify_code(std::uintptr_t base,unsigned rva,unsigned size,const char *hash);
bool install_verified(std::uintptr_t image,std::string &reason) {
  if (!host_identity(image,reason) || !GetModuleHandleW(L"d3d12.dll")) { if (reason.empty()) reason="d3d12-not-loaded"; return false; }
  struct Span { unsigned rva,size; const char *hash; };
  constexpr Span spans[]={
    {0x228f3b0,2000,"df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373"},
    {0x23d0e70,5321,"872aa5439077ddac98d887f7acd1666e801e9c7db3e43bc2b433f1841b3da509"},
    {0x23ccc50,203,"1419b53c5c02532d53ea1c5125134f6b9749ca44fde0be91574e18f2ce808127"},
    {0x28e280,1030,"e23d74268bd6aa3b214f7e0c0cacf977d2c68fe719ffabf1ef24c2236a592713"},
    {0x1fad4c0,211,"428cc3b640b19bc38892e5ada6299dae077a7563e49e2b6d87d8eb887019bade"},
    {0x227ff00,1116,"b7915ccf1b16b06f871d59535512639eec505d4eb1eab6a5732093e52d012353"},
    {0x227db60,52,"d9a8d5c90857455446e900ceae4ce7a176b875ea02d2bdb8c4ff34b03536e5e6"},
    {0x228e770,528,"33ae7df95058ac2e1ce8d920ff89c62d664068a975931bb10afb95ea88f00b97"},
    {0x2392a18,62,"f4b949a977efcfb2d7cc6a61f5d21392f62115527011a81a7153763c07feaa3d"}
  };
  for (const auto &s:spans) if (!verify_code(image,s.rva,s.size,s.hash)) { reason="live-code-"+std::to_string(s.rva); return false; }
  for (const auto vt:{0x350d188U,0x33f82a8U}) {
    std::uintptr_t p{}; if (!read(image+vt+0x48,p) || p!=image+0x228e770) { reason="camera-vtable"; return false; }
  }
  if (!install(image+0x228f3b0,image)) { reason="hook-install"; return false; }
  return true;
}
bool arm(std::uint64_t tick) noexcept {
  if (!enabled || used || pending.load()) return false;
  used=true; started=tick?tick:GetTickCount64(); status.store(Stop::recording); return true;
}
void stop() noexcept { end(Stop::requested); }
void poll(std::uint64_t tick) noexcept { if (status.load()==Stop::recording && (tick?tick:GetTickCount64())-started>=window_ms) end(Stop::deadline); }
State state() noexcept { return {status.load(),reserved.load(),pending.load(std::memory_order_acquire),visits.load(),dropped.load(),injections.load(),restorations.load(),started}; }
bool settled() noexcept {
  // Synchronize with the admission window before pending becomes visible.
  // Once stopped, no new recorder can pass its in-lock status check.
  if (status.load()==Stop::recording || !TryAcquireSRWLockExclusive(&admission)) return false;
  const bool done=pending.load(std::memory_order_acquire)==0;
  ReleaseSRWLockExclusive(&admission); return done;
}
bool copy_call(unsigned index,Call &out) noexcept {
  if (!settled() || index>=reserved.load() || index>=capacity) return false;
  out=calls[index]; return out.complete;
}
bool disable() noexcept {
  stop(); if (!created) return true;
  const auto result=MH_DisableHook(reinterpret_cast<void *>(target));
  if (result!=MH_OK && result!=MH_ERROR_DISABLED) return false;
  enabled=false; return true;
}
#ifdef MHW_ACTIVATION_OWNED_FIXTURE
bool install_owned(std::uintptr_t at,std::uintptr_t image) noexcept { return install(at,image); }
bool remove_owned() noexcept { return !pending.load() && disable() && (!created || MH_RemoveHook(reinterpret_cast<void *>(target))==MH_OK); }
#endif
}
