// projection_binding::file_digest (v0.28.5): the provider check's reader
// agrees with file_sha256 and says why a hash is missing.
#include "../src/projection_binding/memory.hpp"
#include <windows.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace pb = mhw::projection_binding;
namespace {
int failures = 0;
void check(bool ok, const char *what) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); ++failures; }
}
}  // namespace

int main() {
  const auto folder = std::filesystem::temp_directory_path() / ("file_digest_tests-" + std::to_string(GetCurrentProcessId()));
  std::filesystem::create_directories(folder);
  const auto file = folder / L"data.bin";
  {
    // Larger than one 64 KB read, not a multiple of it.
    std::ofstream out(file, std::ios::binary);
    for (unsigned i = 0; i < 200000; ++i) out.put(static_cast<char>(i * 31 + 7));
  }
  const auto expected = pb::file_sha256(file.wstring());
  check(expected.size() == 64, "file_sha256 hashes the file");

  const auto win32 = pb::file_digest(file.wstring());
  check(win32.sha == expected && !*win32.stage && win32.size == 200000 && win32.read == 200000, "file_digest matches file_sha256");

  const auto missing = pb::file_digest((folder / L"missing.bin").wstring());
  check(missing.sha.empty() && std::string(missing.stage) == "open" && missing.error == ERROR_FILE_NOT_FOUND, "a missing file stops at open");

  // Held without read sharing by someone else: the read stops at open.
  const HANDLE held = CreateFileW(file.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  check(held != INVALID_HANDLE_VALUE, "hold the file");
  const auto locked = pb::file_digest(file.wstring());
  check(locked.sha.empty() && std::string(locked.stage) == "open" && locked.error == ERROR_SHARING_VIOLATION, "a held file stops at open");
  CloseHandle(held);

  const auto empty = folder / L"empty.bin";
  { std::ofstream out(empty, std::ios::binary); }
  const auto none = pb::file_digest(empty.wstring());
  check(none.sha == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" && none.size == 0, "an empty file hashes");

  std::error_code ignored;
  std::filesystem::remove_all(folder, ignored);
  if (failures) return 1;
  std::puts("file_digest_tests: ok");
  return 0;
}
