#include "recorder.hpp"
#include "renderer_snapshot.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <span>
extern "C" { void *mhw_writer_originals[1]{}; void mhw_writer_setter_entry(); }
namespace mhw::jitter_writer {
namespace {
SRWLOCK admission = SRWLOCK_INIT;
Layout layout{};
std::uintptr_t target{};
std::array<Call,capacity> calls{};
std::atomic<Stop> status{Stop::idle};
std::atomic<unsigned> reserved{}, pending{};
std::atomic<std::uint64_t> visits{}, named{}, other_type{}, dropped{}, read_failures{}, ordering{};
std::uint64_t started{};
bool created{}, enabled{}, used{};
struct ErrorGuard { DWORD saved{GetLastError()}; ~ErrorGuard() { SetLastError(saved); } };
template<class T> bool read(std::uintptr_t at, T &out) noexcept {
  return bridge_probe::read_bounded_scalar(nullptr, at, std::as_writable_bytes(std::span(&out,1))) == bridge_probe::ScalarReadResult::success;
}
void end(Stop why) noexcept { auto expected=Stop::recording; status.compare_exchange_strong(expected,why,std::memory_order_acq_rel); }
bool executable(std::uintptr_t at, std::uintptr_t base) noexcept {
  MEMORY_BASIC_INFORMATION m{};
  if (VirtualQuery(reinterpret_cast<void *>(at), &m, sizeof(m)) != sizeof(m)) return false;
  const auto p = m.Protect & 255;
  return m.State == MEM_COMMIT && m.Type == MEM_IMAGE && reinterpret_cast<std::uintptr_t>(m.AllocationBase) == base &&
      !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && (p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY);
}
struct Hash {
  BCRYPT_ALG_HANDLE algorithm{};
  BCRYPT_HASH_HANDLE handle{};
  Hash() {
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return;
    if (BCryptCreateHash(algorithm, &handle, nullptr, 0, nullptr, 0, 0) != 0) handle = nullptr;
  }
  ~Hash() { if (handle) BCryptDestroyHash(handle); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
  bool add(const void *p, ULONG size) { return handle && BCryptHashData(handle, static_cast<PUCHAR>(const_cast<void *>(p)), size, 0) == 0; }
  std::string finish() {
    std::array<UCHAR, 32> bytes{};
    if (!handle || BCryptFinishHash(handle, bytes.data(), static_cast<ULONG>(bytes.size()), 0) != 0) return {};
    constexpr char digits[] = "0123456789abcdef";
    std::string text; text.reserve(64);
    for (auto b : bytes) { text += digits[b >> 4]; text += digits[b & 15]; }
    return text;
  }
};
bool code_hash(std::uintptr_t base, unsigned rva, unsigned length, const char *expected) {
  if (!executable(base + rva, base) || !executable(base + rva + length - 1, base)) return false;
  Hash hash;
  for (unsigned n = 0; n < length;) {
    std::uint64_t data{}; const auto count = (std::min)(8U, length - n);
    if (bridge_probe::read_bounded_scalar(nullptr, base + rva + n,
        {reinterpret_cast<std::byte *>(&data), count}) != bridge_probe::ScalarReadResult::success || !hash.add(&data, count)) return false;
    n += count;
  }
  return hash.finish() == expected;
}
}
std::string file_sha256(const std::wstring &path) {
  std::ifstream file(std::filesystem::path(path), std::ios::binary); if (!file) return {};
  Hash hash; std::array<char, 65536> buffer{};
  while (file) { file.read(buffer.data(), buffer.size()); const auto n = file.gcount();
    if (n > 0 && !hash.add(buffer.data(), static_cast<ULONG>(n))) return {}; }
  return file.eof() ? hash.finish() : std::string{};
}
bool host_identity(std::uintptr_t base, std::string &reason) {
  wchar_t path[32768]{};
  const auto n = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
  if (!n || n >= std::size(path) || file_sha256(path) != host_sha) { reason = "host-sha256"; return false; }
  std::uint16_t magic{}, machine{}; std::uint32_t pe{}, sig{}, stamp{}, size{};
  if (!read(base, magic) || magic != 0x5a4d || !read(base + 0x3c, pe) || pe > 0x1000 ||
      !read(base + pe, sig) || sig != 0x4550 || !read(base + pe + 4, machine) || machine != 0x8664 ||
      !read(base + pe + 8, stamp) || stamp != 0x66d13cb6 || !read(base + pe + 24 + 56, size) || size != 0x55d3000) {
    reason = "mapped-pe"; return false;
  }
  return true;
}
namespace {
Snapshot snapshot(std::uintptr_t descriptor) noexcept {
  Snapshot s{};
  if (descriptor > UINTPTR_MAX - 0x40 || !read(descriptor,s.name) ||
      !read(descriptor+0x10,s.flags) || !read(descriptor+0x18,s.owner) ||
      !read(descriptor+0x20,s.target) || !read(descriptor+0x40,s.index)) { s.failure=1; return s; }
  if (!s.owner || s.owner > UINTPTR_MAX-0x184 || !read(s.owner,s.type)) { s.failure=2; return s; }
  if (s.name!=layout.name || s.type!=layout.layer_vtable || s.target!=s.owner+0x184 || s.flags!=3 || s.index!=0) {
    s.failure=3; return s;
  }
  std::uint8_t value{};
  if (!read(s.target,value)) s.failure=4;
  s.value=value; return s;
}
TrackSnapshot track_snapshot(std::uintptr_t descriptor, std::uintptr_t caller, const Snapshot &destination) noexcept {
  TrackSnapshot s{};
  if (!layout.track_caller || caller!=layout.track_caller) return s;
  s.applicable=true;
  if (destination.failure || descriptor<0x10 || descriptor-0x10>UINTPTR_MAX-0x70) { s.failure=2; return s; }
  s.record=descriptor-0x10;
  if (!read(s.record,s.header) || !read(s.record+8,s.owner) ||
      !read(s.record+0x68,s.kind) || !read(s.record+0x6c,s.index)) { s.failure=1; return s; }
  if (s.kind!=11 || s.owner!=destination.owner || !s.header || s.header>UINTPTR_MAX-0x30) { s.failure=2; return s; }
  std::uint16_t count{};
  if (!read(s.header+2,count) || !read(s.header+0x20,s.keys) || !read(s.header+0x28,s.values)) { s.failure=1; return s; }
  s.count=count;
  const auto key_offset=static_cast<std::uintptr_t>(s.index)*4;
  if (!s.count || s.index>=s.count || !s.keys || !s.values ||
      key_offset>UINTPTR_MAX-4 || s.keys>UINTPTR_MAX-key_offset-4 ||
      s.values>UINTPTR_MAX-static_cast<std::uintptr_t>(s.index)-1) { s.failure=2; return s; }
  std::uint8_t value{};
  if (!read(s.keys+key_offset,s.key_word) || !read(s.values+s.index,value)) { s.failure=1; return s; }
  s.raw_value=value; return s;
}
bool install(std::uintptr_t at, Layout l) noexcept {
  if (created) return false;
  const auto init=MH_Initialize();
  if (init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) return false;
  target=at; layout=l;
  if (MH_CreateHook(reinterpret_cast<void *>(target),reinterpret_cast<void *>(&mhw_writer_setter_entry),&mhw_writer_originals[0])!=MH_OK) return false;
  created=true;
  if (MH_EnableHook(reinterpret_cast<void *>(target))!=MH_OK) { disable(); return false; }
  enabled=true; return true;
}
}
bool install_verified(std::uintptr_t base, std::string &reason) {
  if (!base || !host_identity(base,reason)) return false;
  if (!GetModuleHandleW(L"d3d12.dll")) { reason="d3d12-not-loaded"; return false; }
  struct Span { unsigned rva,size; const char *hash; };
  constexpr Span spans[]={
    {0x218f3f0,0x6a,"f480524524f9f979b95e7e5f93a342ae959aa9b564b0934b3ca9b482737dbc24"},
    {0x2391830,0x6f2,"e977a8a458ad4edc53984366fdc3254e166081a8a0e8733cd2feb32a5a3b0f10"},
    {0x23931e0,0x7c,"88aa2c037b51855a2e516f683ec68c1a00565edc5f78741e8db71783e0b7e488"},
    {0x221c9e0,0xe44,"aaa1e764a90e7454a5f9b0deece165a16983a7ae8eabe7a8febdf00479892f0b"},
    {0x221f39d,0x48a,"e44ecd07baf5f8a2ce1f8c2932f0cf69678e066056aebffc010172d0a1326f51"}
  };
  for (const auto &s:spans) if (!code_hash(base,s.rva,s.size,s.hash)) { reason="live-code-span-"+std::to_string(s.rva); return false; }
  std::uintptr_t registration{}, update{};
  std::uint64_t name0{}; std::uint32_t name8{}; std::uint16_t suffix{};
  if (!read(base+0x351bda8+0x18,registration) || registration!=base+0x2391830 ||
      !read(base+0x351bda8+0x48,update) || update!=base+0x23931e0 ||
      !read(base+0x3344478,name0) || name0!=0x4572657474694a6dULL ||
      !read(base+0x3344480,name8) || name8!=0x6c62616e || !read(base+0x3344484,suffix) || suffix!=0x65) {
    reason="property-type-or-name"; return false;
  }
  if (!install(base+0x218f3f0,{base+0x3344478,base+0x351bda8,base+0x221d5ed})) { reason="hook-install"; return false; }
  return true;
}
bool arm(std::uint64_t tick) noexcept {
  AcquireSRWLockExclusive(&admission);
  const bool ok=enabled && !used && !pending.load(std::memory_order_acquire);
  if (ok) {
    used=true; started=tick?tick:GetTickCount64();
    status.store(Stop::recording,std::memory_order_release);
  }
  ReleaseSRWLockExclusive(&admission); return ok;
}
void poll(std::uint64_t tick) noexcept {
  if (status.load(std::memory_order_acquire)==Stop::recording && (tick?tick:GetTickCount64())-started>=window_ms) end(Stop::deadline);
}
void stop() noexcept { end(Stop::requested); }
State state() noexcept {
  AcquireSRWLockExclusive(&admission);
  State s{status.load(std::memory_order_acquire),(std::min)(reserved.load(std::memory_order_acquire),capacity),
    pending.load(std::memory_order_acquire),started,visits.load(),named.load(),other_type.load(),dropped.load(),read_failures.load()};
  ReleaseSRWLockExclusive(&admission); return s;
}
bool copy_call(unsigned index, Call &out) noexcept {
  AcquireSRWLockExclusive(&admission);
  const bool ok=status.load(std::memory_order_acquire)!=Stop::recording && !pending.load(std::memory_order_acquire) &&
    index<(std::min)(reserved.load(std::memory_order_acquire),capacity);
  if (ok) out=calls[index];
  ReleaseSRWLockExclusive(&admission); return ok && out.complete;
}
bool disable() noexcept {
  stop();
  if (!created) return true;
  const auto result=MH_DisableHook(reinterpret_cast<void *>(target));
  const bool ok=result==MH_OK || result==MH_ERROR_DISABLED;
  if (ok) enabled=false;
  return ok;
}
#ifdef MHW_WRITER_OWNED_FIXTURE
bool install_owned(std::uintptr_t at, Layout l) noexcept { return install(at,l); }
bool remove_owned() noexcept {
  if (pending.load() || !disable()) return false;
  return !created || MH_RemoveHook(reinterpret_cast<void *>(target))==MH_OK;
}
#endif
}
extern "C" std::uint64_t mhw_writer_before(unsigned, std::uintptr_t descriptor, std::uintptr_t input, std::uintptr_t caller) noexcept {
  using namespace mhw::jitter_writer;
  ErrorGuard error;
  if (status.load(std::memory_order_acquire)!=Stop::recording) return 0;
  if (!TryAcquireSRWLockShared(&admission)) { dropped.fetch_add(1); return 0; }
  auto unlock=[] { ReleaseSRWLockShared(&admission); };
  if (status.load(std::memory_order_acquire)!=Stop::recording) { unlock(); return 0; }
  const auto tick=GetTickCount64();
  if (tick-started>=window_ms) { end(Stop::deadline); unlock(); return 0; }
  if (visits.fetch_add(1)>=visit_limit) { end(Stop::visits); unlock(); return 0; }
  std::uintptr_t name{}, owner{}, type{};
  if (descriptor>UINTPTR_MAX-0x40 || !read(descriptor,name)) { read_failures.fetch_add(1); end(Stop::invalid); unlock(); return 0; }
  if (name!=layout.name) { unlock(); return 0; }
  named.fetch_add(1);
  if (!read(descriptor+0x18,owner) || !owner || !read(owner,type)) { read_failures.fetch_add(1); end(Stop::invalid); unlock(); return 0; }
  if (type!=layout.layer_vtable) { other_type.fetch_add(1); unlock(); return 0; }
  const auto index=reserved.fetch_add(1);
  if (index>=capacity) { dropped.fetch_add(1); end(Stop::capacity); unlock(); return 0; }
  pending.fetch_add(1,std::memory_order_acq_rel);
  auto &r=calls[index];
  r.token=index+1; r.tick=tick; r.entry_order=ordering.fetch_add(1)+1;
  r.descriptor=descriptor; r.caller=caller; r.input=static_cast<unsigned>(input&255); r.thread=GetCurrentThreadId();
  r.before=snapshot(descriptor);
  r.track_before=track_snapshot(descriptor,caller,r.before);
  if (r.before.failure || r.track_before.failure) end(Stop::invalid);
  unlock(); return r.token;
}
extern "C" void mhw_writer_after(std::uint64_t token) noexcept {
  using namespace mhw::jitter_writer;
  ErrorGuard error;
  if (!token || token>capacity) return;
  auto &r=calls[static_cast<unsigned>(token-1)];
  r.after=snapshot(r.descriptor);
  r.track_after=track_snapshot(r.descriptor,r.caller,r.after);
  if (r.after.failure || r.track_after.failure || r.before.owner!=r.after.owner || r.before.target!=r.after.target) end(Stop::invalid);
  r.exit_tick=GetTickCount64(); r.exit_order=ordering.fetch_add(1)+1; r.complete=true;
  pending.fetch_sub(1,std::memory_order_acq_rel);
}
