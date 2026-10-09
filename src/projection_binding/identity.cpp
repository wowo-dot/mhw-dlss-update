#include "memory.hpp"
#include "renderer_snapshot.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <span>
namespace mhw::projection_binding {
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
std::string bytes_sha256(const void *data,unsigned size) { Hash h; return h.add(data,size)?h.finish():std::string{}; }
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
