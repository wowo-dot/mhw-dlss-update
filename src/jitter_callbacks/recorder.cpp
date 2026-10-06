#include "recorder.hpp"
#include "renderer_snapshot.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <limits>
#include <span>
#include <vector>

extern "C" {
void *mhw_jitter_originals[3]{};
void mhw_jitter_update_entry();
void mhw_jitter_reset_entry();
void mhw_jitter_setter_entry();
}
namespace mhw::jitter {
namespace {
SRWLOCK admission = SRWLOCK_INIT;
Layout layout{};
std::array<std::uintptr_t, 3> targets{};
std::array<Call, capacity> calls{};
std::atomic<unsigned> reserved{}, pending{};
std::atomic<Stop> status{Stop::idle};
std::atomic<std::uint64_t> dropped{};
std::atomic<std::uint64_t> order{};
unsigned phase{}, last_phase{};
std::uint64_t started{};
bool hooked{}, hooks_created{};
thread_local std::uint64_t parent_token{};
thread_local std::uintptr_t parent_layer{};
struct ErrorGuard { DWORD saved{GetLastError()}; ~ErrorGuard() { SetLastError(saved); } };

template<class T> bool read(std::uintptr_t at, T &value) noexcept {
  return bridge_probe::read_bounded_scalar(nullptr, at, std::as_writable_bytes(std::span(&value, 1))) ==
      bridge_probe::ScalarReadResult::success;
}
Snapshot snapshot(unsigned kind, std::uintptr_t object, std::uintptr_t input, std::uintptr_t layer) noexcept {
  Snapshot s{}; s.layer = layer; s.has_layer = layer != 0; s.has_input = kind == 2;
  auto get = [&]<class T>(std::uintptr_t at, T &out) {
    if (s.failure) return false;
    ++s.reads;
    if (s.reads > 40 || !read(at, out)) { s.failure = 1; return false; }
    return true;
  };
  get(layout.renderer_slot, s.renderer); get(layout.camera_slot, s.camera); get(layout.manager_slot, s.manager);
  if (!s.renderer || !s.camera || !s.manager) { s.failure = 2; return s; }
  get(s.camera, s.camera_type);
  if (s.camera_type != layout.camera_vtables[0] && s.camera_type != layout.camera_vtables[1]) { s.failure = 3; return s; }
  if (kind == 2 && object != s.camera) { s.failure = 4; return s; }
  if (layer) {
    get(layer, s.layer_type);
    if (s.layer_type != layout.layer_vtable) { s.failure = 5; return s; }
    get(layer + 0xc, s.layer_state); get(layer + 0x14, s.layer_flags);
    get(layer + 0x40, s.layer_mask); get(layer + 0x180, s.blend_bits); get(layer + 0x184, s.taa_bytes);
  }
  get(s.manager + 0x4a48, s.manager_mask);
  get(s.renderer + 0x1f210, s.frame); get(s.renderer + 0x1f208, s.epoch);
  get(s.renderer + 0x1f450, s.view);
  if (!s.view) { s.failure = 6; return s; }
  get(s.view + 0x58, s.view_flags); get(s.renderer + 0x1f560, s.reset_mode);
  get(s.camera + 0x38, s.camera_jitter); get(s.camera + 0x40, s.camera_epoch);
  if (kind == 2) get(input, s.setter_input);
  get(s.renderer + 0x1f210, s.frame_after); get(s.renderer + 0x1f208, s.epoch_after);
  std::uintptr_t r{}, c{}, m{}, v{}, vt{};
  get(layout.renderer_slot, r); get(layout.camera_slot, c); get(layout.manager_slot, m);
  get(s.renderer + 0x1f450, v); get(s.camera, vt);
  std::uintptr_t layer_type = s.layer_type;
  if (layer) get(layer, layer_type);
  s.stable = !s.failure && r == s.renderer && c == s.camera && m == s.manager && v == s.view &&
      vt == s.camera_type && layer_type == s.layer_type && s.frame == s.frame_after && s.epoch == s.epoch_after;
  return s;
}
void end(Stop why) noexcept {
  auto expected = Stop::recording;
  status.compare_exchange_strong(expected, why, std::memory_order_acq_rel);
}
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
bool install(std::array<std::uintptr_t, 3> t, Layout l) noexcept {
  if (hooks_created || hooked) return false;
  const auto init = MH_Initialize();
  if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) return false;
  void *entries[] = {reinterpret_cast<void *>(&mhw_jitter_update_entry), reinterpret_cast<void *>(&mhw_jitter_reset_entry),
                    reinterpret_cast<void *>(&mhw_jitter_setter_entry)};
  targets = t; layout = l;
  unsigned created = 0;
  for (; created < 3; ++created) {
    if (MH_CreateHook(reinterpret_cast<void *>(targets[created]), entries[created], &mhw_jitter_originals[created]) != MH_OK) break;
  }
  if (created != 3) {
    while (created) MH_RemoveHook(reinterpret_cast<void *>(targets[--created]));
    return false;
  }
  hooks_created = true;
  // One suspension pass for all three hooks. Do not touch hooks owned by another component.
  for (auto t0 : targets) if (MH_QueueEnableHook(reinterpret_cast<void *>(t0)) != MH_OK) { disable(); return false; }
  if (MH_ApplyQueued() != MH_OK) { disable(); return false; }
  hooked = true; return true;
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
bool install_verified(std::uintptr_t base, std::string &reason) {
  if (!base || !host_identity(base, reason)) return false;
  if (!GetModuleHandleW(L"d3d12.dll")) { reason = "d3d12-not-loaded"; return false; }
  struct Span { unsigned rva, size; const char *hash; };
  constexpr Span spans[] = {
    {0x23931e0,0x7c,"88aa2c037b51855a2e516f683ec68c1a00565edc5f78741e8db71783e0b7e488"},
    {0x2393260,0xf7,"a0f23cc4334b1e0e03b98c99894004f23cc0e6ef82d469ace598789081fc9e46"},
    {0x228f390,0x18,"5123862eb70e10acd2c695bc76f53b3b62c3d2ef6d34a8b5f3ff6c8a88644414"},
    {0x2228f50,0xdc,"9d4d90a9dfcda59f81fd72ecff4ada29797519d8d0aa6ae2617a60733d6f10cd"},
    {0x2228f20,0x2b,"275b6e02eb427ed6ec3fa86c14c8472208e9569b2ca03c5256df6d206cdd8e90"},
    {0x227db60,0x34,"d9a8d5c90857455446e900ceae4ce7a176b875ea02d2bdb8c4ff34b03536e5e6"},
    {0x229a080,0x2d3,"b925de3da9241cb0df94cf2ec85a2e9c5a9c96e4152c132ad3f31f57bc13afa7"}
  };
  for (const auto &s : spans) if (!code_hash(base, s.rva, s.size, s.hash)) { reason = "live-code-span-" + std::to_string(s.rva); return false; }
  std::uintptr_t a{}, b{};
  if (!read(base + 0x351bda8 + 0x48, a) || a != base + 0x23931e0 ||
      !read(base + 0x351bda8 + 0x50, b) || b != base + 0x2393260) { reason = "taa-vtable"; return false; }
  Layout l{base + 0x51c4480, base + 0x51c4400, base + 0x51238c8, base + 0x351bda8,
           {base + 0x350d188, base + 0x33f82a8}};
  if (!install({a,b,base + 0x228f390}, l)) { reason = "hook-install"; return false; }
  return true;
}
bool arm(unsigned p, std::uint64_t tick) noexcept {
  AcquireSRWLockExclusive(&admission);
  const bool ok = hooked && p >= 1 && p <= 3 && p == last_phase + 1 && !pending.load(std::memory_order_acquire) &&
      status.load(std::memory_order_acquire) != Stop::recording;
  if (ok) {
    phase = last_phase = p; started = tick ? tick : GetTickCount64(); calls = {};
    reserved.store(0); dropped.store(0); order.store(0); status.store(Stop::recording, std::memory_order_release);
  }
  ReleaseSRWLockExclusive(&admission); return ok;
}
void poll(std::uint64_t tick) noexcept {
  const auto now = tick ? tick : GetTickCount64();
  if (status.load(std::memory_order_acquire) == Stop::recording && now - started >= window_ms) end(Stop::deadline);
}
void stop() noexcept { end(Stop::requested); }
State state() noexcept {
  AcquireSRWLockExclusive(&admission);
  const State result{phase,(std::min)(reserved.load(std::memory_order_acquire),capacity),pending.load(std::memory_order_acquire),
          started,dropped.load(std::memory_order_relaxed),status.load(std::memory_order_acquire)};
  ReleaseSRWLockExclusive(&admission); return result;
}
bool copy_call(unsigned index, Call &out) noexcept {
  AcquireSRWLockExclusive(&admission);
  const bool ok = status.load(std::memory_order_acquire) != Stop::recording && !pending.load(std::memory_order_acquire) &&
      index < (std::min)(reserved.load(std::memory_order_acquire), capacity);
  if (ok) out = calls[index];
  ReleaseSRWLockExclusive(&admission); return ok && out.complete;
}
bool disable() noexcept {
  stop(); if (!hooks_created) return true;
  bool ok = true;
  for (auto t : targets) if (MH_QueueDisableHook(reinterpret_cast<void *>(t)) != MH_OK) ok = false;
  if (MH_ApplyQueued() != MH_OK) ok = false;
  if (ok) hooked = false;
  return ok;
}
std::string event_name(unsigned pid, std::uint64_t creation, unsigned p) {
  return "Local\\MhwJitterCallbacks-v1-" + std::to_string(pid) + "-" + std::to_string(creation) + "-" + std::to_string(p);
}
#ifdef MHW_JITTER_OWNED_FIXTURE
bool install_owned(std::array<std::uintptr_t,3> t, Layout l) noexcept { return install(t,l); }
bool remove_owned() noexcept {
  if (!disable() || pending.load()) return false;
  for (auto t : targets) if (MH_RemoveHook(reinterpret_cast<void *>(t)) != MH_OK) return false;
  hooks_created = false; return true;
}
#endif
}

extern "C" std::uint64_t mhw_jitter_before(unsigned kind, std::uintptr_t object, std::uintptr_t input, std::uintptr_t caller) noexcept {
  using namespace mhw::jitter;
  ErrorGuard error;
  if (status.load(std::memory_order_acquire) != Stop::recording) return 0;
  if (!TryAcquireSRWLockShared(&admission)) { dropped.fetch_add(1, std::memory_order_relaxed); return 0; }
  if (status.load(std::memory_order_acquire) != Stop::recording) { ReleaseSRWLockShared(&admission); return 0; }
  const auto now = GetTickCount64();
  if (now - started >= window_ms) { end(Stop::deadline); ReleaseSRWLockShared(&admission); return 0; }
  const auto index = reserved.fetch_add(1, std::memory_order_relaxed);
  if (index >= capacity) { dropped.fetch_add(1, std::memory_order_relaxed); end(Stop::capacity); ReleaseSRWLockShared(&admission); return 0; }
  pending.fetch_add(1, std::memory_order_acq_rel);
  auto &r = calls[index];
  r.token = (static_cast<std::uint64_t>(phase) << 32) | (index + 1);
  r.parent = parent_token; r.previous_layer = parent_layer;
  r.kind = kind; r.thread = GetCurrentThreadId(); r.tick = now;
  r.entry_order = order.fetch_add(1, std::memory_order_relaxed) + 1;
  r.argument = object; r.input = input; r.caller = caller;
  const auto layer = kind < 2 ? object : parent_layer;
  r.before = snapshot(kind, object, input, layer);
  parent_token = r.token; parent_layer = layer;
  if (r.before.failure) end(Stop::invalid_sample);
  ReleaseSRWLockShared(&admission);
  return r.token;
}
extern "C" void mhw_jitter_after(std::uint64_t token) noexcept {
  using namespace mhw::jitter;
  ErrorGuard error;
  if (!token) return;
  const auto index = static_cast<unsigned>(token) - 1;
  if (index >= capacity) return;
  auto &r = calls[index];
  r.after = snapshot(r.kind, r.argument, r.input, r.before.layer);
  if (r.after.failure || parent_token != token) end(Stop::invalid_sample);
  r.exit_tick = GetTickCount64();
  r.exit_order = order.fetch_add(1, std::memory_order_relaxed) + 1;
  parent_token = r.parent; parent_layer = r.previous_layer;
  r.complete = true;
  pending.fetch_sub(1, std::memory_order_release);
}
