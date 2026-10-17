#include "memory.hpp"
#include "bounded_scalar_read.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>
namespace mhw::projection_binding {
bool executable(std::uintptr_t at, std::uintptr_t base) noexcept {
  MEMORY_BASIC_INFORMATION m{};
  if (VirtualQuery(reinterpret_cast<void *>(at), &m, sizeof(m)) != sizeof(m)) return false;
  const auto p = m.Protect & 255;
  return m.State == MEM_COMMIT && m.Type == MEM_IMAGE && reinterpret_cast<std::uintptr_t>(m.AllocationBase) == base &&
      !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && (p == PAGE_EXECUTE_READ || p == PAGE_EXECUTE_READWRITE || p == PAGE_EXECUTE_WRITECOPY);
}
// One SHA-256 provider for the process (v0.23.6): opening one for every hash
// showed in the per-frame profile (perf4). An algorithm handle may be shared by
// threads; each Hash has its own hash object. It is never closed.
BCRYPT_ALG_HANDLE sha256_provider() noexcept {
  static const BCRYPT_ALG_HANDLE provider = [] {
    BCRYPT_ALG_HANDLE h{};
    return BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 ? h : nullptr;
  }();
  return provider;
}
struct Hash {
  BCRYPT_HASH_HANDLE handle{};
  Hash() {
    const auto algorithm = sha256_provider();
    if (!algorithm || BCryptCreateHash(algorithm, &handle, nullptr, 0, nullptr, 0, 0) != 0) handle = nullptr;
  }
  ~Hash() { if (handle) BCryptDestroyHash(handle); }
  Hash(const Hash &) = delete;
  Hash &operator=(const Hash &) = delete;
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
bool verify_code(std::uintptr_t base, unsigned rva, unsigned length, const char *expected) {
  return verify_code_with_head(base, rva, length, expected, nullptr, 0);
}
bool verify_code_with_head(std::uintptr_t base, unsigned rva, unsigned length, const char *expected,
    const std::uint8_t *head, unsigned head_size) {
  if (head_size > length || (head_size && !head)) return false;
  if (!executable(base + rva, base) || !executable(base + rva + length - 1, base)) return false;
  Hash hash;
  if (head_size && !hash.add(head, head_size)) return false;
  for (unsigned n = head_size; n < length;) {
    std::uint64_t data{}; const auto count = (std::min)(8U, length - n);
    if (bridge_probe::read_bounded_scalar(nullptr, base + rva + n,
        {reinterpret_cast<std::byte *>(&data), count}) != bridge_probe::ScalarReadResult::success || !hash.add(&data, count)) return false;
    n += count;
  }
  return hash.finish() == expected;
}
std::string file_sha256(const std::wstring &path) {
  std::ifstream file(std::filesystem::path(path), std::ios::binary); if (!file) return {};
  Hash hash; std::array<char, 65536> buffer{};
  while (file) { file.read(buffer.data(), buffer.size()); const auto n = file.gcount();
    if (n > 0 && !hash.add(buffer.data(), static_cast<ULONG>(n))) return {}; }
  return file.eof() ? hash.finish() : std::string{};
}
FileDigest file_digest(const std::wstring &path) {
  FileDigest d;
  const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) { d.stage = "open"; d.error = GetLastError(); return d; }
  LARGE_INTEGER size{};
  if (!GetFileSizeEx(file, &size)) { d.stage = "size"; d.error = GetLastError(); CloseHandle(file); return d; }
  d.size = static_cast<std::uint64_t>(size.QuadPart);
  Hash hash; std::vector<char> buffer(65536);
  while (true) {
    DWORD n{};
    if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &n, nullptr)) { d.stage = "read"; d.error = GetLastError(); break; }
    if (!n) break;
    if (!hash.add(buffer.data(), n)) { d.stage = "hash"; break; }
    d.read += n;
  }
  CloseHandle(file);
  if (*d.stage) return d;
  if (d.read != d.size) { d.stage = "short"; return d; }
  d.sha = hash.finish();
  if (d.sha.empty()) d.stage = "hash";
  return d;
}
// Memo (v0.23.6): the stream route identifies overlay and haze shaders by their
// SHA-256 on every plan build, so the same few bytecodes were hashed every
// frame (perf4). Inputs from 256 bytes to 256 KB are kept with their hash (32,
// in turn); a lookup compares the whole input, so a kept hash is returned only
// for identical bytes.
namespace {
struct Memo {
  std::vector<std::byte> bytes;
  std::string sha;
};
std::array<Memo, 32> memos;
unsigned memo_next{};
SRWLOCK memo_lock = SRWLOCK_INIT;
}  // namespace
std::string bytes_sha256(const void *data,unsigned size) {
  const bool memo = data && size >= 256 && size <= 262144;
  if (memo) {
    std::string kept;
    AcquireSRWLockShared(&memo_lock);
    for (const auto &m : memos)
      if (m.bytes.size() == size && std::memcmp(m.bytes.data(), data, size) == 0) { kept = m.sha; break; }
    ReleaseSRWLockShared(&memo_lock);
    if (!kept.empty()) return kept;
  }
  Hash h;
  auto sha = h.add(data, size) ? h.finish() : std::string{};
  if (memo && sha.size() == 64) {
    try {
      const auto *first = static_cast<const std::byte *>(data);
      std::vector<std::byte> copy(first, first + size);
      AcquireSRWLockExclusive(&memo_lock);
      memos[memo_next] = {std::move(copy), sha};
      memo_next = (memo_next + 1) % static_cast<unsigned>(memos.size());
      ReleaseSRWLockExclusive(&memo_lock);
    } catch (...) {
    }
  }
  return sha;
}
bool host_identity(std::uintptr_t base, std::string &reason, bool hash_file) {
  wchar_t path[32768]{};
  const auto n = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
  if (!n || n >= std::size(path) || (hash_file && file_sha256(path) != host_sha)) { reason = "host-sha256"; return false; }
  std::uint16_t magic{}, machine{}; std::uint32_t pe{}, sig{}, stamp{}, size{};
  if (!read(base, magic) || magic != 0x5a4d || !read(base + 0x3c, pe) || pe > 0x1000 ||
      !read(base + pe, sig) || sig != 0x4550 || !read(base + pe + 4, machine) || machine != 0x8664 ||
      !read(base + pe + 8, stamp) || stamp != 0x66d13cb6 || !read(base + pe + 24 + 56, size) || size != 0x55d3000) {
    reason = "mapped-pe"; return false;
  }
  return true;
}
}
