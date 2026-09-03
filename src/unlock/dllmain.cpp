#include <windows.h>

#include <bcrypt.h>
#include <dxgi1_2.h>
#include <intrin.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <MinHook.h>

#include "unlock_policy.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#pragma intrinsic(_ReturnAddress)
#endif

namespace {

using mhw::unlock::Classifier;
using mhw::unlock::PeHeaderIdentity;

constexpr std::size_t kHashBufferSize = 1U << 20U;
constexpr std::uint64_t kMaximumDiagnosticFileSize = 64ULL << 20U;
constexpr DWORD kDiagnosticDurationMilliseconds = 30'000U;
constexpr DWORD kDiagnosticPollMilliseconds = 250U;
constexpr DWORD kVerificationWaitMilliseconds = 5'000U;
constexpr std::size_t kMaximumCensusModules = 16U;
constexpr std::uint32_t kMaximumSnapshotEntries = 2'048U;
constexpr DWORD kMaximumVersionResourceSize = 16U << 20U;
constexpr std::uintptr_t kDescriptionClassifierRva =
    mhw::unlock::kCacheDescriptionParserRva;
constexpr std::uintptr_t kBucketMapperRva =
    mhw::unlock::kCacheBucketMapperRva;
constexpr std::uintptr_t kPopulateDecisionRva =
    mhw::unlock::kCachePopulateDecisionSequenceRva;
constexpr std::uint32_t kExpectedPeTimestamp =
    mhw::unlock::kExpectedCachePeTimestamp;
constexpr std::uint64_t kExpectedPreferredImageBase =
    mhw::unlock::kExpectedCachePreferredImageBase;
constexpr std::uint32_t kExpectedPeImageSize =
    mhw::unlock::kExpectedCacheImageSize;

constexpr std::array<unsigned char, 32> kExpectedHostHash{
    0xC2, 0xEB, 0xBB, 0xD2, 0xC4, 0x9F, 0x21, 0x6D,
    0x48, 0x4E, 0x31, 0xA5, 0x21, 0x9B, 0xED, 0x41,
    0x9E, 0xB1, 0xE5, 0xE7, 0xD2, 0x06, 0xD0, 0x2C,
    0xBA, 0x04, 0x0A, 0x3A, 0xB7, 0x9D, 0x90, 0xEA};

constexpr auto kExpectedFeatureHash =
    mhw::unlock::kExpectedCacheFeatureHash;

using DescriptionClassifierFn = std::uint32_t(__fastcall *)(const void *);
using BucketMapperFn = std::uint32_t(__fastcall *)(std::uint32_t);
using IdentityGetterFn = std::uint32_t(__cdecl *)();

enum class HookState : std::uint8_t {
  inactive,
  installing,
  verifying,
  active,
};

enum class HitPublishState : std::uint32_t {
  empty,
  writing,
  ready,
};

struct FirstHitPayload {
  ULONGLONG tick{};
  DWORD thread_id{};
  std::uint32_t call_ordinal{};
  HookState entry_state{HookState::inactive};
  std::uint32_t original_result{};
  std::uint32_t returned_result{};
  std::uint32_t input_value{};
  bool override_active{};
  bool subject_matches{};
  bool caller_matches{};
  bool argument_nonnull{};
};

struct HookTelemetry {
  std::atomic<std::uint32_t> calls{};
  std::atomic<HitPublishState> publish_state{HitPublishState::empty};
  FirstHitPayload first{};
};

static_assert(std::atomic<std::uint32_t>::is_always_lock_free);
static_assert(std::atomic<HitPublishState>::is_always_lock_free);

std::atomic<HookState> g_hook_state{HookState::inactive};
std::atomic<bool> g_verified_adapter{};
std::atomic<std::uintptr_t> g_feature_base{};
std::atomic<std::uint32_t> g_verification_timeouts{};
DescriptionClassifierFn g_original_description_classifier{};
BucketMapperFn g_original_bucket_mapper{};
std::ofstream g_log;
HMODULE g_self{};
HookTelemetry g_description_telemetry{};
HookTelemetry g_bucket_telemetry{};

std::filesystem::path module_path(const HMODULE module) {
  wchar_t path[32'768]{};
  const DWORD length =
      GetModuleFileNameW(module, path, static_cast<DWORD>(_countof(path)));
  if (length == 0 || length >= _countof(path)) {
    return {};
  }
  return std::filesystem::path(path, path + length);
}

std::filesystem::path log_path() {
  wchar_t local_app_data[32'768]{};
  const DWORD length = GetEnvironmentVariableW(
      L"LOCALAPPDATA", local_app_data, static_cast<DWORD>(_countof(local_app_data)));
  if (length != 0 && length < _countof(local_app_data)) {
    return std::filesystem::path(local_app_data) / L"MhwDlssUnlock" /
           (L"unlock-" + std::to_wstring(GetCurrentProcessId()) + L".log");
  }
  return std::filesystem::temp_directory_path() /
         (L"mhw-dlss-unlock-" + std::to_wstring(GetCurrentProcessId()) + L".log");
}

std::filesystem::path cache_feature_path() noexcept {
  PWSTR program_data{};
  if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT,
                                  nullptr, &program_data)) ||
      program_data == nullptr) {
    return {};
  }

  std::filesystem::path result;
  try {
    result = std::filesystem::path(program_data) / L"NVIDIA" / L"NGX" /
             L"models" / L"dlss" / L"versions" / L"66158" / L"files" /
             L"160_B9D2F5C.bin";
  } catch (...) {
    result.clear();
  }
  CoTaskMemFree(program_data);
  return result;
}

void open_log() noexcept {
  try {
    const std::filesystem::path path = log_path();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    g_log.open(path, std::ios::out | std::ios::trunc);
  } catch (...) {
  }
}

void write_log(const std::string_view message) {
  if (!g_log.is_open()) {
    return;
  }
  g_log << GetTickCount64() << "\ttid=" << GetCurrentThreadId() << '\t'
        << message << '\n';
  g_log.flush();
}

std::string utf8(const std::wstring_view value) {
  if (value.empty()) {
    return {};
  }
  const int needed = WideCharToMultiByte(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0,
      nullptr, nullptr);
  if (needed <= 0) {
    return "<conversion-failed>";
  }
  std::string result(static_cast<std::size_t>(needed), '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, value.data(),
                          static_cast<int>(value.size()), result.data(), needed,
                          nullptr, nullptr) != needed) {
    return "<conversion-failed>";
  }
  return result;
}

struct Sha256Failure {
  const char *stage{"none"};
  NTSTATUS status{};
  DWORD error{ERROR_SUCCESS};
};

void set_sha256_failure(Sha256Failure *const failure, const char *const stage,
                        const NTSTATUS status = 0,
                        const DWORD error = ERROR_SUCCESS) noexcept {
  if (failure != nullptr) {
    failure->stage = stage;
    failure->status = status;
    failure->error = error;
  }
}

bool sha256_handle(
    const HANDLE file, std::array<unsigned char, 32> *const output,
    const std::uint64_t maximum_size = UINT64_MAX,
    Sha256Failure *const failure = nullptr) noexcept {
  if (failure != nullptr) {
    *failure = {};
  }
  if (file == nullptr || file == INVALID_HANDLE_VALUE || output == nullptr) {
    set_sha256_failure(failure, "invalid-argument", 0,
                       ERROR_INVALID_PARAMETER);
    return false;
  }

  BCRYPT_ALG_HANDLE algorithm{};
  BCRYPT_HASH_HANDLE hash{};
  unsigned char *hash_object{};
  unsigned char *buffer{};
  DWORD object_size{};
  DWORD result_size{};
  DWORD hash_size{};
  LARGE_INTEGER file_size{};
  LARGE_INTEGER zero{};
  LARGE_INTEGER original_position{};
  std::array<unsigned char, 32> digest{};
  std::uint64_t total_bytes{};
  bool have_original_position = false;
  bool success = false;
  NTSTATUS status{};

  if (!GetFileSizeEx(file, &file_size)) {
    set_sha256_failure(failure, "get-file-size", 0, GetLastError());
    goto cleanup;
  }
  if (file_size.QuadPart < 0 ||
      static_cast<std::uint64_t>(file_size.QuadPart) > maximum_size) {
    set_sha256_failure(failure, "file-size-limit", 0, ERROR_FILE_TOO_LARGE);
    goto cleanup;
  }
  if (!SetFilePointerEx(file, zero, &original_position, FILE_CURRENT)) {
    set_sha256_failure(failure, "get-file-position", 0, GetLastError());
    goto cleanup;
  }
  have_original_position = true;
  if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) {
    set_sha256_failure(failure, "seek-file-begin", 0, GetLastError());
    goto cleanup;
  }

  status =
      BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (status != 0) {
    set_sha256_failure(failure, "bcrypt-open-algorithm", status);
    goto cleanup;
  }
  status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                             reinterpret_cast<PUCHAR>(&object_size),
                             sizeof(object_size), &result_size, 0);
  if (status != 0) {
    set_sha256_failure(failure, "bcrypt-get-object-length", status);
    goto cleanup;
  }
  status = BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                             reinterpret_cast<PUCHAR>(&hash_size),
                             sizeof(hash_size), &result_size, 0);
  if (status != 0) {
    set_sha256_failure(failure, "bcrypt-get-hash-length", status);
    goto cleanup;
  }
  if (hash_size != digest.size()) {
    set_sha256_failure(failure, "bcrypt-hash-length-mismatch", 0,
                       ERROR_INVALID_DATA);
    goto cleanup;
  }

  hash_object =
      static_cast<unsigned char *>(HeapAlloc(GetProcessHeap(), 0, object_size));
  if (hash_object == nullptr) {
    set_sha256_failure(failure, "allocate-hash-object", 0,
                       ERROR_NOT_ENOUGH_MEMORY);
    goto cleanup;
  }
  status = BCryptCreateHash(algorithm, &hash, hash_object, object_size, nullptr,
                            0, 0);
  if (status != 0) {
    set_sha256_failure(failure, "bcrypt-create-hash", status);
    goto cleanup;
  }

  buffer = static_cast<unsigned char *>(
      HeapAlloc(GetProcessHeap(), 0, kHashBufferSize));
  if (buffer == nullptr) {
    set_sha256_failure(failure, "allocate-read-buffer", 0,
                       ERROR_NOT_ENOUGH_MEMORY);
    goto cleanup;
  }
  for (;;) {
    DWORD bytes_read{};
    if (!ReadFile(file, buffer, static_cast<DWORD>(kHashBufferSize), &bytes_read,
                  nullptr)) {
      set_sha256_failure(failure, "read-file", 0, GetLastError());
      goto cleanup;
    }
    if (bytes_read == 0) {
      break;
    }
    total_bytes += bytes_read;
    status = BCryptHashData(hash, buffer, bytes_read, 0);
    if (status != 0) {
      set_sha256_failure(failure, "bcrypt-hash-data", status);
      goto cleanup;
    }
  }
  if (total_bytes != static_cast<std::uint64_t>(file_size.QuadPart)) {
    set_sha256_failure(failure, "unexpected-end-of-file", 0,
                       ERROR_HANDLE_EOF);
    goto cleanup;
  }

  status = BCryptFinishHash(hash, digest.data(),
                            static_cast<ULONG>(digest.size()), 0);
  if (status != 0) {
    set_sha256_failure(failure, "bcrypt-finish-hash", status);
    goto cleanup;
  }
  *output = digest;
  success = true;

cleanup:
  if (have_original_position &&
      !SetFilePointerEx(file, original_position, nullptr, FILE_BEGIN) &&
      success) {
    set_sha256_failure(failure, "restore-file-position", 0, GetLastError());
    success = false;
  }
  if (hash != nullptr) {
    BCryptDestroyHash(hash);
  }
  if (algorithm != nullptr) {
    BCryptCloseAlgorithmProvider(algorithm, 0);
  }
  if (buffer != nullptr) {
    HeapFree(GetProcessHeap(), 0, buffer);
  }
  if (hash_object != nullptr) {
    HeapFree(GetProcessHeap(), 0, hash_object);
  }
  return success;
}

bool sha256_file(
    const std::filesystem::path &path,
    std::array<unsigned char, 32> *const output,
    const std::uint64_t maximum_size = UINT64_MAX,
    Sha256Failure *const failure = nullptr) noexcept {
  if (failure != nullptr) {
    *failure = {};
  }
  const HANDLE file =
      CreateFileW(path.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    set_sha256_failure(failure, "open-file", 0, GetLastError());
    return false;
  }
  const bool success = sha256_handle(file, output, maximum_size, failure);
  CloseHandle(file);
  return success;
}

bool hash_matches(const std::filesystem::path &path,
                  const std::array<unsigned char, 32> &expected) noexcept {
  std::array<unsigned char, 32> actual{};
  return sha256_file(path, &actual) && actual == expected;
}

bool same_file(const std::filesystem::path &left,
               const std::filesystem::path &right) noexcept {
  std::error_code error;
  const bool equivalent = std::filesystem::equivalent(left, right, error);
  return !error && equivalent;
}

std::string sha256_text(const std::array<unsigned char, 32> &hash) {
  constexpr char digits[] = "0123456789ABCDEF";
  std::string result(hash.size() * 2U, '0');
  for (std::size_t index = 0; index < hash.size(); ++index) {
    const unsigned char value = hash[index];
    result[index * 2U] = digits[value >> 4U];
    result[index * 2U + 1U] = digits[value & 0x0FU];
  }
  return result;
}

std::string hexadecimal(const std::uint64_t value) {
  char buffer[19]{};
  _snprintf_s(buffer, _TRUNCATE, "0x%llX",
              static_cast<unsigned long long>(value));
  return buffer;
}

std::string file_version(const std::filesystem::path &path) noexcept {
  try {
    DWORD ignored{};
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0U || size > kMaximumVersionResourceSize) {
      return "unavailable";
    }

    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0U, size, data.data())) {
      return "unavailable";
    }

    void *raw_information{};
    UINT information_size{};
    if (!VerQueryValueW(data.data(), L"\\", &raw_information,
                        &information_size) ||
        raw_information == nullptr ||
        information_size < sizeof(VS_FIXEDFILEINFO)) {
      return "unavailable";
    }
    const auto *information =
        static_cast<const VS_FIXEDFILEINFO *>(raw_information);
    if (information->dwSignature != VS_FFI_SIGNATURE) {
      return "unavailable";
    }

    char version[48]{};
    _snprintf_s(
        version, _TRUNCATE, "%u.%u.%u.%u",
        static_cast<unsigned int>(HIWORD(information->dwFileVersionMS)),
        static_cast<unsigned int>(LOWORD(information->dwFileVersionMS)),
        static_cast<unsigned int>(HIWORD(information->dwFileVersionLS)),
        static_cast<unsigned int>(LOWORD(information->dwFileVersionLS)));
    return version;
  } catch (...) {
    return "unavailable";
  }
}

bool read_file_version(const std::filesystem::path &path,
                       std::array<std::uint16_t, 4> *const output) noexcept {
  if (output == nullptr) {
    return false;
  }
  try {
    DWORD ignored{};
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0U || size > kMaximumVersionResourceSize) {
      return false;
    }
    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0U, size, data.data())) {
      return false;
    }
    void *raw_information{};
    UINT information_size{};
    if (!VerQueryValueW(data.data(), L"\\", &raw_information,
                        &information_size) ||
        raw_information == nullptr ||
        information_size < sizeof(VS_FIXEDFILEINFO)) {
      return false;
    }
    const auto *information =
        static_cast<const VS_FIXEDFILEINFO *>(raw_information);
    if (information->dwSignature != VS_FFI_SIGNATURE) {
      return false;
    }
    *output = {
        static_cast<std::uint16_t>(HIWORD(information->dwFileVersionMS)),
        static_cast<std::uint16_t>(LOWORD(information->dwFileVersionMS)),
        static_cast<std::uint16_t>(HIWORD(information->dwFileVersionLS)),
        static_cast<std::uint16_t>(LOWORD(information->dwFileVersionLS))};
    return true;
  } catch (...) {
    return false;
  }
}

wchar_t ascii_lower(const wchar_t value) noexcept {
  if (value >= L'A' && value <= L'Z') {
    return static_cast<wchar_t>(value + (L'a' - L'A'));
  }
  return value;
}

bool contains_ascii_case_insensitive(const std::wstring_view text,
                                     const std::wstring_view needle) noexcept {
  if (needle.empty()) {
    return true;
  }
  if (needle.size() > text.size()) {
    return false;
  }
  for (std::size_t offset = 0U; offset <= text.size() - needle.size();
       ++offset) {
    bool matches = true;
    for (std::size_t index = 0U; index < needle.size(); ++index) {
      if (ascii_lower(text[offset + index]) != ascii_lower(needle[index])) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return true;
    }
  }
  return false;
}

bool equal_path_text(const std::wstring_view left,
                     const std::wstring_view right) noexcept {
  if (left.size() != right.size()) {
    return false;
  }
  if (left.empty()) {
    return true;
  }
  return CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
                              right.data(), static_cast<int>(right.size()),
                              TRUE) == CSTR_EQUAL;
}

bool is_relevant_feature_module(const HMODULE module,
                                const std::filesystem::path &path) {
  if (module == g_self) {
    return false;
  }
  const std::wstring filename = path.filename().wstring();
  if (contains_ascii_case_insensitive(filename, L"mhw_dlss_")) {
    return false;
  }
  const std::wstring path_text = path.wstring();
  return contains_ascii_case_insensitive(filename, L"nvngx") ||
         contains_ascii_case_insensitive(filename, L"dlss") ||
         contains_ascii_case_insensitive(filename, L"_B9D2F5C.bin") ||
         contains_ascii_case_insensitive(path_text, L"\\NVIDIA\\NGX\\") ||
         contains_ascii_case_insensitive(path_text, L"\\NGX\\models\\");
}

std::string module_origin(const std::filesystem::path &path,
                          const std::filesystem::path &host_directory) {
  const std::wstring path_text = path.wstring();
  if (contains_ascii_case_insensitive(path_text, L"\\NVIDIA\\NGX\\models\\")) {
    return "ngx-cache";
  }
  if (!host_directory.empty() &&
      equal_path_text(path.parent_path().wstring(), host_directory.wstring())) {
    return "game-local";
  }
  return "other";
}

// Keep SEH in a POD-only leaf. MSVC cannot mix object unwinding and __try in
// the same function when /EHsc is enabled.
bool read_mapped_pe_identity(const HMODULE module,
                             PeHeaderIdentity *const output) noexcept {
  if (module == nullptr || output == nullptr) {
    return false;
  }
  const auto *base = reinterpret_cast<const unsigned char *>(module);
  __try {
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::uint32_t>(dos->e_lfanew) > 0x10'0000U) {
      return false;
    }
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
        base + static_cast<std::size_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) {
      return false;
    }
    output->machine = nt->FileHeader.Machine;
    output->optional_magic = nt->OptionalHeader.Magic;
    output->image_base = nt->OptionalHeader.ImageBase;
    output->image_size = nt->OptionalHeader.SizeOfImage;
    output->timestamp = nt->FileHeader.TimeDateStamp;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

struct DiskPeFailure {
  const char *stage{"none"};
  DWORD error{ERROR_SUCCESS};
};

void set_disk_pe_failure(DiskPeFailure *const failure,
                         const char *const stage,
                         const DWORD error) noexcept {
  if (failure != nullptr) {
    failure->stage = stage;
    failure->error = error;
  }
}

bool read_disk_pe_identity(const HANDLE file, PeHeaderIdentity *const output,
                           DiskPeFailure *const failure) noexcept {
  if (failure != nullptr) {
    *failure = {};
  }
  if (file == nullptr || file == INVALID_HANDLE_VALUE || output == nullptr) {
    set_disk_pe_failure(failure, "invalid-argument", ERROR_INVALID_PARAMETER);
    return false;
  }

  constexpr std::uint32_t maximum_nt_header_offset = 0x10'0000U;
  LARGE_INTEGER file_size{};
  LARGE_INTEGER zero{};
  LARGE_INTEGER original_position{};
  LARGE_INTEGER nt_position{};
  IMAGE_DOS_HEADER dos{};
  IMAGE_NT_HEADERS64 nt{};
  PeHeaderIdentity identity{};
  DWORD bytes_read{};
  bool have_original_position = false;
  bool success = false;

  if (!GetFileSizeEx(file, &file_size)) {
    set_disk_pe_failure(failure, "get-file-size", GetLastError());
    goto cleanup;
  }
  if (file_size.QuadPart < static_cast<LONGLONG>(sizeof(dos)) ||
      static_cast<std::uint64_t>(file_size.QuadPart) >
          kMaximumDiagnosticFileSize) {
    set_disk_pe_failure(failure, "file-size-out-of-range", ERROR_BAD_EXE_FORMAT);
    goto cleanup;
  }
  if (!SetFilePointerEx(file, zero, &original_position, FILE_CURRENT)) {
    set_disk_pe_failure(failure, "get-file-position", GetLastError());
    goto cleanup;
  }
  have_original_position = true;
  if (!SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) {
    set_disk_pe_failure(failure, "seek-dos-header", GetLastError());
    goto cleanup;
  }
  if (!ReadFile(file, &dos, static_cast<DWORD>(sizeof(dos)), &bytes_read,
                nullptr)) {
    set_disk_pe_failure(failure, "read-dos-header", GetLastError());
    goto cleanup;
  }
  if (bytes_read != sizeof(dos)) {
    set_disk_pe_failure(failure, "short-dos-header", ERROR_HANDLE_EOF);
    goto cleanup;
  }
  if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0 ||
      static_cast<std::uint32_t>(dos.e_lfanew) > maximum_nt_header_offset) {
    set_disk_pe_failure(failure, "invalid-dos-header", ERROR_BAD_EXE_FORMAT);
    goto cleanup;
  }

  nt_position.QuadPart = dos.e_lfanew;
  if (nt_position.QuadPart >
      file_size.QuadPart - static_cast<LONGLONG>(sizeof(nt))) {
    set_disk_pe_failure(failure, "nt-header-out-of-range",
                        ERROR_BAD_EXE_FORMAT);
    goto cleanup;
  }
  if (!SetFilePointerEx(file, nt_position, nullptr, FILE_BEGIN)) {
    set_disk_pe_failure(failure, "seek-nt-header", GetLastError());
    goto cleanup;
  }
  bytes_read = 0U;
  if (!ReadFile(file, &nt, static_cast<DWORD>(sizeof(nt)), &bytes_read,
                nullptr)) {
    set_disk_pe_failure(failure, "read-nt-header", GetLastError());
    goto cleanup;
  }
  if (bytes_read != sizeof(nt)) {
    set_disk_pe_failure(failure, "short-nt-header", ERROR_HANDLE_EOF);
    goto cleanup;
  }
  if (nt.Signature != IMAGE_NT_SIGNATURE ||
      nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) {
    set_disk_pe_failure(failure, "invalid-nt-header", ERROR_BAD_EXE_FORMAT);
    goto cleanup;
  }

  identity.machine = nt.FileHeader.Machine;
  identity.optional_magic = nt.OptionalHeader.Magic;
  identity.timestamp = nt.FileHeader.TimeDateStamp;
  identity.image_base = nt.OptionalHeader.ImageBase;
  identity.image_size = nt.OptionalHeader.SizeOfImage;
  *output = identity;
  success = true;

cleanup:
  if (have_original_position &&
      !SetFilePointerEx(file, original_position, nullptr, FILE_BEGIN) &&
      success) {
    set_disk_pe_failure(failure, "restore-file-position", GetLastError());
    success = false;
  }
  return success;
}

class ScopedSnapshot final {
public:
  explicit ScopedSnapshot(const HANDLE handle) noexcept : handle_(handle) {}
  ~ScopedSnapshot() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
    }
  }
  ScopedSnapshot(const ScopedSnapshot &) = delete;
  ScopedSnapshot &operator=(const ScopedSnapshot &) = delete;
  [[nodiscard]] HANDLE get() const noexcept { return handle_; }

private:
  HANDLE handle_{INVALID_HANDLE_VALUE};
};

class ScopedFileHandle final {
public:
  explicit ScopedFileHandle(const HANDLE handle) noexcept : handle_(handle) {}
  ~ScopedFileHandle() {
    if (handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
    }
  }
  ScopedFileHandle(const ScopedFileHandle &) = delete;
  ScopedFileHandle &operator=(const ScopedFileHandle &) = delete;
  [[nodiscard]] HANDLE get() const noexcept { return handle_; }
  void reset() noexcept {
    if (handle_ != INVALID_HANDLE_VALUE) {
      CloseHandle(handle_);
      handle_ = INVALID_HANDLE_VALUE;
    }
  }

private:
  HANDLE handle_{INVALID_HANDLE_VALUE};
};

class ScopedModuleReference final {
public:
  explicit ScopedModuleReference(const HMODULE module) noexcept
      : module_(module) {}
  ~ScopedModuleReference() {
    if (module_ != nullptr) {
      FreeLibrary(module_);
    }
  }
  ScopedModuleReference(const ScopedModuleReference &) = delete;
  ScopedModuleReference &operator=(const ScopedModuleReference &) = delete;
  [[nodiscard]] HMODULE get() const noexcept { return module_; }
  void reset() noexcept {
    if (module_ != nullptr) {
      FreeLibrary(module_);
      module_ = nullptr;
    }
  }

private:
  HMODULE module_{};
};

struct SeenModule {
  std::uintptr_t base{};
  std::wstring path;
};

struct DiagnosticState {
  std::filesystem::path host_directory;
  std::vector<SeenModule> seen_modules;
  bool snapshot_failure_logged{};
  bool snapshot_truncation_logged{};
  bool module_limit_logged{};
  bool unexpected_failure_logged{};
};

bool module_was_seen(const DiagnosticState &state, const std::uintptr_t base,
                     const std::wstring_view path) noexcept {
  for (const SeenModule &seen : state.seen_modules) {
    if (seen.base == base && equal_path_text(seen.path, path)) {
      return true;
    }
  }
  return false;
}

void log_new_module(DiagnosticState &state, const HMODULE module,
                    const std::filesystem::path &snapshot_path) {
  HMODULE held{};
  if (!GetModuleHandleExW(
          GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
          reinterpret_cast<LPCWSTR>(module), &held) ||
      held == nullptr) {
    return;
  }
  ScopedModuleReference reference(held);
  if (held != module) {
    return;
  }

  std::filesystem::path path = module_path(held);
  if (path.empty()) {
    path = snapshot_path;
  }
  if (!is_relevant_feature_module(held, path)) {
    return;
  }

  const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(held);
  const std::wstring path_text = path.wstring();
  if (module_was_seen(state, base, path_text)) {
    return;
  }
  if (state.seen_modules.size() >= kMaximumCensusModules) {
    if (!state.module_limit_logged) {
      write_log("diagnostic.census truncated=true reason=relevant-module-limit");
      state.module_limit_logged = true;
    }
    return;
  }

  PeHeaderIdentity identity{};
  const bool has_pe_identity = read_mapped_pe_identity(held, &identity);
  const bool has_populate =
      GetProcAddress(held, "NVSDK_NGX_PopulateParameters") != nullptr;
  const bool has_d3d12_init =
      GetProcAddress(held, "NVSDK_NGX_D3D12_Init") != nullptr;
  const bool has_application_id =
      GetProcAddress(held, "NVSDK_NGX_GetApplicationId") != nullptr;
  const bool has_snippet_version =
      GetProcAddress(held, "NVSDK_NGX_GetSnippetVersion") != nullptr;

  state.seen_modules.push_back(SeenModule{base, path_text});
  reference.reset();

  std::array<unsigned char, 32> hash{};
  const bool has_hash =
      sha256_file(path, &hash, kMaximumDiagnosticFileSize);
  const std::string version = file_version(path);
  write_log(
      "diagnostic.module origin=" + module_origin(path, state.host_directory) +
      " path=" + utf8(path_text) + " base=" +
      hexadecimal(static_cast<std::uint64_t>(base)) +
      " mapped-image-base=" +
      (has_pe_identity ? hexadecimal(identity.image_base) : "unavailable") +
      " size-of-image=" +
      (has_pe_identity ? hexadecimal(identity.image_size) : "unavailable") +
      " pe-timestamp=" +
      (has_pe_identity ? hexadecimal(identity.timestamp) : "unavailable") +
      " sha256=" + (has_hash ? sha256_text(hash) : "unavailable") +
      " file-version=" + version + " export-populate=" +
      (has_populate ? "true" : "false") + " export-d3d12-init=" +
      (has_d3d12_init ? "true" : "false") +
      " export-application-id=" +
      (has_application_id ? "true" : "false") +
      " export-snippet-version=" +
      (has_snippet_version ? "true" : "false"));
}

void census_modules(DiagnosticState &state) {
  HANDLE snapshot = INVALID_HANDLE_VALUE;
  DWORD snapshot_error = ERROR_SUCCESS;
  for (std::uint32_t attempt = 0U; attempt < 3U; ++attempt) {
    snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snapshot != INVALID_HANDLE_VALUE) {
      break;
    }
    snapshot_error = GetLastError();
    if (snapshot_error != ERROR_BAD_LENGTH) {
      break;
    }
  }
  if (snapshot == INVALID_HANDLE_VALUE) {
    if (!state.snapshot_failure_logged) {
      write_log("diagnostic.census result=snapshot-failed error=" +
                std::to_string(snapshot_error));
      state.snapshot_failure_logged = true;
    }
    return;
  }
  ScopedSnapshot retained_snapshot(snapshot);

  MODULEENTRY32W entry{};
  entry.dwSize = static_cast<DWORD>(sizeof(entry));
  BOOL has_entry = Module32FirstW(retained_snapshot.get(), &entry);
  std::uint32_t entries = 0U;
  while (has_entry != FALSE && entries < kMaximumSnapshotEntries) {
    ++entries;
    const std::filesystem::path snapshot_path(entry.szExePath);
    if (is_relevant_feature_module(entry.hModule, snapshot_path)) {
      log_new_module(state, entry.hModule, snapshot_path);
    }
    has_entry = Module32NextW(retained_snapshot.get(), &entry);
  }
  if (has_entry != FALSE && !state.snapshot_truncation_logged) {
    write_log("diagnostic.census truncated=true reason=snapshot-entry-limit");
    state.snapshot_truncation_logged = true;
  }
}

void census_modules_safely(DiagnosticState &state) {
  try {
    census_modules(state);
  } catch (...) {
    if (!state.unexpected_failure_logged) {
      write_log("diagnostic.census result=unexpected-failure");
      state.unexpected_failure_logged = true;
    }
  }
}

HMODULE find_loaded_module(const std::filesystem::path &expected_path) noexcept {
  HANDLE snapshot = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (snapshot == INVALID_HANDLE_VALUE) {
    return nullptr;
  }
  ScopedSnapshot retained_snapshot(snapshot);

  MODULEENTRY32W entry{};
  entry.dwSize = static_cast<DWORD>(sizeof(entry));
  BOOL has_entry = Module32FirstW(retained_snapshot.get(), &entry);
  std::uint32_t entries = 0U;
  while (has_entry != FALSE && entries < kMaximumSnapshotEntries) {
    ++entries;
    const std::filesystem::path candidate(entry.szExePath);
    if (same_file(candidate, expected_path)) {
      return entry.hModule;
    }
    has_entry = Module32NextW(retained_snapshot.get(), &entry);
  }
  return nullptr;
}

bool expected_host_candidate(const std::filesystem::path &path) noexcept {
  try {
    if (!equal_path_text(path.filename().wstring(), L"MonsterHunterWorld.exe")) {
      return false;
    }
    PeHeaderIdentity identity{};
    return read_mapped_pe_identity(GetModuleHandleW(nullptr), &identity);
  } catch (...) {
    return false;
  }
}

bool executable_memory(const void *address) noexcept;

bool populate_decision_matches(const void *const address) noexcept {
  if (address == nullptr || !executable_memory(address)) {
    return false;
  }
  __try {
    return mhw::unlock::matches_cache_populate_decision_sequence(
        std::span<const unsigned char>(
            static_cast<const unsigned char *>(address),
            mhw::unlock::kCachePopulateDecisionSequenceSize));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool relative_call_targets(const HMODULE module, const std::uintptr_t call_rva,
                           const std::uintptr_t target_rva) noexcept {
  if (module == nullptr) {
    return false;
  }
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  const auto *instruction = reinterpret_cast<const unsigned char *>(base + call_rva);
  if (!executable_memory(instruction)) {
    return false;
  }
  __try {
    if (instruction[0] != 0xE8U) {
      return false;
    }
    std::int32_t displacement{};
    std::memcpy(&displacement, instruction + 1U, sizeof(displacement));
    const auto destination = static_cast<std::uintptr_t>(
        static_cast<std::intptr_t>(base + call_rva + 5U) + displacement);
    return destination == base + target_rva;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool read_identity_getter(const HMODULE module, const char *const name,
                          std::uint32_t *const output) noexcept {
  if (module == nullptr || name == nullptr || output == nullptr) {
    return false;
  }
  const auto getter = reinterpret_cast<IdentityGetterFn>(GetProcAddress(module, name));
  if (getter == nullptr) {
    return false;
  }
  __try {
    *output = getter();
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool read_cache_feature_identity(
    const std::filesystem::path &path, const HMODULE module,
    const std::array<unsigned char, 32> &verified_sha256,
    const PeHeaderIdentity &verified_disk_pe,
    mhw::unlock::CacheFeatureIdentity *const output) noexcept {
  if (module == nullptr || output == nullptr) {
    return false;
  }
  mhw::unlock::CacheFeatureIdentity identity{};
  identity.sha256 = verified_sha256;
  if (!read_file_version(path, &identity.file_version) ||
      !read_identity_getter(module, "NVSDK_NGX_GetApplicationId",
                            &identity.application_id) ||
      !read_identity_getter(module, "NVSDK_NGX_GetGPUArchitecture",
                            &identity.gpu_architecture) ||
      !read_identity_getter(module, "NVSDK_NGX_GetSnippetVersion",
                            &identity.snippet_version) ||
      !read_identity_getter(module, "NVSDK_NGX_GetAPIVersion",
                            &identity.api_version) ||
      GetProcAddress(module, "NVSDK_NGX_PopulateParameters") == nullptr ||
      GetProcAddress(module, "NVSDK_NGX_D3D12_Init") == nullptr) {
    return false;
  }
  identity.pe_timestamp = verified_disk_pe.timestamp;
  identity.preferred_image_base = verified_disk_pe.image_base;
  identity.image_size = verified_disk_pe.image_size;
  *output = identity;
  return true;
}

struct FeatureControlFlowChecks {
  bool populate_decision{};
  bool description_optimal_call{};
  bool description_legacy_call{};
  bool bucket_populate_call{};
  bool bucket_optimal_call{};
};

FeatureControlFlowChecks
check_feature_control_flow(const HMODULE module) noexcept {
  FeatureControlFlowChecks checks{};
  if (module == nullptr) {
    return checks;
  }
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  checks.populate_decision = populate_decision_matches(
      reinterpret_cast<const void *>(base + kPopulateDecisionRva));
  checks.description_optimal_call =
      relative_call_targets(module, 0x0F8E3U, kDescriptionClassifierRva);
  checks.description_legacy_call =
      relative_call_targets(module, 0x32452U, kDescriptionClassifierRva);
  checks.bucket_populate_call =
      relative_call_targets(module, 0x1E004U, kBucketMapperRva);
  checks.bucket_optimal_call =
      relative_call_targets(module, 0x22F08U, kBucketMapperRva);
  return checks;
}

bool all_feature_control_flow_checks_pass(
    const FeatureControlFlowChecks &checks) noexcept {
  return checks.populate_decision && checks.description_optimal_call &&
         checks.description_legacy_call && checks.bucket_populate_call &&
         checks.bucket_optimal_call;
}

bool executable_memory(const void *address) noexcept {
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(address, &information, sizeof(information)) !=
          sizeof(information) ||
      information.State != MEM_COMMIT ||
      (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
    return false;
  }
  const DWORD protection = information.Protect & 0xFFU;
  return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
         protection == PAGE_EXECUTE_READWRITE ||
         protection == PAGE_EXECUTE_WRITECOPY;
}

bool prologue_matches(const void *address, const Classifier classifier) noexcept {
  if (!executable_memory(address)) {
    return false;
  }
  const std::size_t size =
      mhw::unlock::classifier_prologue_size(classifier);
  if (size == 0U || size > 64U) {
    return false;
  }
  __try {
    return mhw::unlock::matches_classifier_prologue(
        classifier,
        std::span<const unsigned char>(
            static_cast<const unsigned char *>(address), size));
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool verify_nvidia_adapter_inventory() {
  IDXGIFactory1 *factory{};
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || factory == nullptr) {
    write_log("gpu.enumeration result=factory-failed");
    return false;
  }

  std::uint32_t hardware_count = 0;
  std::uint32_t nvidia_hardware_count = 0;
  std::uint32_t allowlisted_count = 0;
  bool enumeration_complete = true;
  for (UINT index = 0;; ++index) {
    IDXGIAdapter1 *adapter{};
    const HRESULT result = factory->EnumAdapters1(index, &adapter);
    if (result == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(result) || adapter == nullptr) {
      write_log("gpu.enumeration result=adapter-failed index=" +
                std::to_string(index));
      if (adapter != nullptr) {
        adapter->Release();
      }
      enumeration_complete = false;
      break;
    }

    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(adapter->GetDesc1(&description))) {
      write_log("gpu.enumeration result=description-failed index=" +
                std::to_string(index));
      adapter->Release();
      enumeration_complete = false;
      break;
    } else {
      const bool software_or_remote =
          (description.Flags &
           (DXGI_ADAPTER_FLAG_SOFTWARE | DXGI_ADAPTER_FLAG_REMOTE)) != 0;
      const std::wstring_view name(description.Description);
      write_log("gpu.adapter index=" + std::to_string(index) + " vendor=0x" +
                [&description] {
                  char value[9]{};
                  _snprintf_s(value, _TRUNCATE, "%08X", description.VendorId);
                  return std::string(value);
                }() +
                " software-or-remote=" +
                (software_or_remote ? "true" : "false") + " name=" +
                utf8(name));
      if (!software_or_remote) {
        ++hardware_count;
        if (description.VendorId == mhw::unlock::kNvidiaVendorId) {
          ++nvidia_hardware_count;
        }
        if (mhw::unlock::is_allowlisted_geforce_rtx50_adapter(
                description.VendorId, software_or_remote, name)) {
          ++allowlisted_count;
        }
      }
    }
    adapter->Release();
  }
  factory->Release();
  write_log("gpu.inventory hardware=" + std::to_string(hardware_count) +
            " nvidia-hardware=" +
            std::to_string(nvidia_hardware_count) + " allowlisted-rtx5090=" +
            std::to_string(allowlisted_count) + " complete=" +
            (enumeration_complete ? "true" : "false"));
  return mhw::unlock::is_verified_adapter_inventory(
      enumeration_complete, hardware_count, nvidia_hardware_count,
      allowlisted_count);
}

bool opaque_description_is_geforce_rtx50(const void *object) noexcept {
  __try {
    return mhw::unlock::
        msvc_wstring_description_is_allowlisted_geforce_rtx50(object);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

const char *hook_state_name(const HookState state) noexcept {
  switch (state) {
  case HookState::inactive:
    return "inactive";
  case HookState::installing:
    return "installing";
  case HookState::verifying:
    return "verifying";
  case HookState::active:
    return "active";
  }
  return "unknown";
}

void publish_hit(HookTelemetry &telemetry, const HookState entry_state,
                 const std::uint32_t original_result,
                 const std::uint32_t returned_result,
                 const std::uint32_t input_value,
                 const bool override_active, const bool subject_matches,
                 const bool caller_matches,
                 const bool argument_nonnull) noexcept {
  const std::uint32_t ordinal =
      telemetry.calls.fetch_add(1U, std::memory_order_relaxed) + 1U;
  HitPublishState expected = HitPublishState::empty;
  if (!telemetry.publish_state.compare_exchange_strong(
          expected, HitPublishState::writing, std::memory_order_acq_rel,
          std::memory_order_relaxed)) {
    return;
  }

  telemetry.first.tick = GetTickCount64();
  telemetry.first.thread_id = GetCurrentThreadId();
  telemetry.first.call_ordinal = ordinal;
  telemetry.first.entry_state = entry_state;
  telemetry.first.original_result = original_result;
  telemetry.first.returned_result = returned_result;
  telemetry.first.input_value = input_value;
  telemetry.first.override_active = override_active;
  telemetry.first.subject_matches = subject_matches;
  telemetry.first.caller_matches = caller_matches;
  telemetry.first.argument_nonnull = argument_nonnull;
  telemetry.publish_state.store(HitPublishState::ready,
                                std::memory_order_release);
}

void drain_first_hit(const char *const name, const HookTelemetry &telemetry,
                     bool &already_logged) {
  if (already_logged ||
      telemetry.publish_state.load(std::memory_order_acquire) !=
          HitPublishState::ready) {
    return;
  }
  const FirstHitPayload first = telemetry.first;
  write_log("diagnostic.first-hit hook=" + std::string(name) +
            " hit-tick=" + std::to_string(first.tick) + " hit-tid=" +
            std::to_string(first.thread_id) + " ordinal=" +
            std::to_string(first.call_ordinal) + " entry-state=" +
            hook_state_name(first.entry_state) + " override-active=" +
            (first.override_active ? "true" : "false") +
            " argument-nonnull=" +
            (first.argument_nonnull ? "true" : "false") +
            " caller-match=" +
            (first.caller_matches ? "true" : "false") + " input=" +
            std::to_string(first.input_value) +
            " subject-match=" +
            (first.subject_matches ? "true" : "false") + " original=" +
            std::to_string(first.original_result) + " returned=" +
            std::to_string(first.returned_result));
  already_logged = true;
}

bool override_is_active() noexcept {
  HookState state = g_hook_state.load(std::memory_order_acquire);
  if (state != HookState::installing && state != HookState::verifying) {
    return state == HookState::active;
  }

  // The forced-load worker installs inert hooks before the slower host hash
  // and adapter checks finish. A concurrent NGX call waits briefly for that
  // exact-build transaction instead of caching a premature zero.
  const ULONGLONG deadline =
      GetTickCount64() + kVerificationWaitMilliseconds;
  do {
    SwitchToThread();
    state = g_hook_state.load(std::memory_order_acquire);
  } while ((state == HookState::installing || state == HookState::verifying) &&
           GetTickCount64() < deadline);
  if (state == HookState::installing || state == HookState::verifying) {
    g_verification_timeouts.fetch_add(1U, std::memory_order_relaxed);
  }
  return state == HookState::active;
}

bool expected_caller(const Classifier classifier,
                     const void *const return_address) noexcept {
  return mhw::unlock::is_expected_classifier_return_address(
      classifier, g_feature_base.load(std::memory_order_acquire),
      kExpectedPeImageSize,
      reinterpret_cast<std::uintptr_t>(return_address));
}

__declspec(noinline) std::uint32_t __fastcall
description_classifier_detour(const void *description) {
  const void *const return_address = _ReturnAddress();
  const DWORD entry_last_error = GetLastError();
  const HookState entry_state = g_hook_state.load(std::memory_order_acquire);
  std::uint32_t original = 0U;
  DWORD forwarded_last_error = entry_last_error;
  if (g_original_description_classifier != nullptr) {
    original = g_original_description_classifier(description);
    forwarded_last_error = GetLastError();
  }

  const bool active = override_is_active();
  const bool caller_matches =
      expected_caller(Classifier::cache_description_parser, return_address);
  const bool subject_matches =
      g_verified_adapter.load(std::memory_order_acquire) &&
      opaque_description_is_geforce_rtx50(description);
  const std::uint32_t returned = active
                                     ? mhw::unlock::map_unknown_gpu_type(
                                           original, subject_matches,
                                           caller_matches)
                                     : original;
  publish_hit(g_description_telemetry, entry_state, original, returned, 0U,
              active, subject_matches, caller_matches,
              description != nullptr);
  SetLastError(forwarded_last_error);
  return returned;
}

__declspec(noinline) std::uint32_t __fastcall
bucket_mapper_detour(const std::uint32_t bucket) {
  const void *const return_address = _ReturnAddress();
  const DWORD entry_last_error = GetLastError();
  const HookState entry_state = g_hook_state.load(std::memory_order_acquire);
  std::uint32_t original = 0U;
  DWORD forwarded_last_error = entry_last_error;
  if (g_original_bucket_mapper != nullptr) {
    original = g_original_bucket_mapper(bucket);
    forwarded_last_error = GetLastError();
  }

  const bool active = override_is_active();
  const bool caller_matches =
      expected_caller(Classifier::cache_bucket_mapper, return_address);
  const bool subject_matches =
      g_verified_adapter.load(std::memory_order_acquire);
  const std::uint32_t returned = active
                                     ? mhw::unlock::map_unknown_gpu_type(
                                           original, subject_matches,
                                           caller_matches)
                                     : original;
  publish_hit(g_bucket_telemetry, entry_state, original, returned, bucket,
              active, subject_matches, caller_matches, true);
  SetLastError(forwarded_last_error);
  return returned;
}

bool install_hooks(const HMODULE feature) {
  const auto base = reinterpret_cast<std::uintptr_t>(feature);
  auto *description_target =
      reinterpret_cast<void *>(base + kDescriptionClassifierRva);
  auto *bucket_target = reinterpret_cast<void *>(base + kBucketMapperRva);

  if (!prologue_matches(description_target,
                        Classifier::cache_description_parser) ||
      !prologue_matches(bucket_target, Classifier::cache_bucket_mapper)) {
    write_log("unlock.abort reason=classifier-prologue-mismatch");
    return false;
  }

  const MH_STATUS initialize = MH_Initialize();
  if (initialize != MH_OK) {
    write_log("unlock.abort reason=minhook-initialize status=" +
              std::to_string(static_cast<int>(initialize)));
    return false;
  }

  const MH_STATUS description_create = MH_CreateHook(
      description_target,
      reinterpret_cast<void *>(&description_classifier_detour),
      reinterpret_cast<void **>(&g_original_description_classifier));
  if (description_create != MH_OK) {
    write_log("unlock.abort reason=description-hook-create status=" +
              std::to_string(static_cast<int>(description_create)));
    MH_Uninitialize();
    return false;
  }

  const MH_STATUS bucket_create = MH_CreateHook(
      bucket_target, reinterpret_cast<void *>(&bucket_mapper_detour),
      reinterpret_cast<void **>(&g_original_bucket_mapper));
  if (bucket_create != MH_OK) {
    write_log("unlock.abort reason=bucket-hook-create status=" +
              std::to_string(static_cast<int>(bucket_create)));
    MH_RemoveHook(description_target);
    MH_Uninitialize();
    return false;
  }

  const MH_STATUS description_queue = MH_QueueEnableHook(description_target);
  const MH_STATUS bucket_queue = MH_QueueEnableHook(bucket_target);
  if (description_queue != MH_OK || bucket_queue != MH_OK) {
    write_log("unlock.abort reason=hook-queue status-description=" +
              std::to_string(static_cast<int>(description_queue)) +
              " status-bucket=" +
              std::to_string(static_cast<int>(bucket_queue)));
    MH_RemoveHook(bucket_target);
    MH_RemoveHook(description_target);
    MH_Uninitialize();
    return false;
  }

  // MinHook resumes suspended threads before MH_ApplyQueued returns. Mark the
  // transaction as installing so a just-resumed detour waits for the outcome
  // instead of caching either a premature override or the original zero.
  g_hook_state.store(HookState::installing, std::memory_order_release);
  const MH_STATUS apply = MH_ApplyQueued();
  if (apply != MH_OK) {
    g_hook_state.store(HookState::inactive, std::memory_order_release);
    const MH_STATUS description_disable = MH_DisableHook(description_target);
    const MH_STATUS bucket_disable = MH_DisableHook(bucket_target);
    const MH_STATUS description_remove = MH_RemoveHook(description_target);
    const MH_STATUS bucket_remove = MH_RemoveHook(bucket_target);
    const MH_STATUS uninitialize = MH_Uninitialize();
    write_log("unlock.abort reason=hook-apply status=" +
              std::to_string(static_cast<int>(apply)) +
              " cleanup-description-disable=" +
              std::to_string(static_cast<int>(description_disable)) +
              " cleanup-bucket-disable=" +
              std::to_string(static_cast<int>(bucket_disable)) +
              " cleanup-description-remove=" +
              std::to_string(static_cast<int>(description_remove)) +
              " cleanup-bucket-remove=" +
              std::to_string(static_cast<int>(bucket_remove)) +
              " cleanup-uninitialize=" +
              std::to_string(static_cast<int>(uninitialize)));
    return false;
  }
  g_hook_state.store(HookState::verifying, std::memory_order_release);
  write_log("unlock.hooks-ready state=verifying targets=cache-description,cache-bucket");
  return true;
}

void run_bounded_diagnostics(DiagnosticState &state,
                             const ULONGLONG start_tick) {
  bool description_first_logged = false;
  bool bucket_first_logged = false;
  for (;;) {
    drain_first_hit("cache-description", g_description_telemetry,
                    description_first_logged);
    drain_first_hit("cache-bucket", g_bucket_telemetry,
                    bucket_first_logged);
    census_modules_safely(state);

    const ULONGLONG elapsed = GetTickCount64() - start_tick;
    if (elapsed >= kDiagnosticDurationMilliseconds) {
      break;
    }
    const ULONGLONG remaining = kDiagnosticDurationMilliseconds - elapsed;
    const DWORD delay =
        remaining < kDiagnosticPollMilliseconds
            ? static_cast<DWORD>(remaining)
            : kDiagnosticPollMilliseconds;
    Sleep(delay);
  }

  drain_first_hit("cache-description", g_description_telemetry,
                  description_first_logged);
  drain_first_hit("cache-bucket", g_bucket_telemetry, bucket_first_logged);
  write_log(
      "diagnostic.complete duration-ms=" +
      std::to_string(GetTickCount64() - start_tick) + " census-modules=" +
      std::to_string(state.seen_modules.size()) + " description-calls=" +
      std::to_string(
          g_description_telemetry.calls.load(std::memory_order_relaxed)) +
      " description-first-recorded=" +
      (description_first_logged ? "true" : "false") + " bucket-calls=" +
      std::to_string(
          g_bucket_telemetry.calls.load(std::memory_order_relaxed)) +
      " bucket-first-recorded=" +
      (bucket_first_logged ? "true" : "false") +
      " verification-timeouts=" +
      std::to_string(
          g_verification_timeouts.load(std::memory_order_relaxed)));
}

DWORD WINAPI worker(void *) {
  try {
    HMODULE pinned_self{};
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&description_classifier_detour),
            &pinned_self) ||
        pinned_self != g_self) {
      return 0;
    }
    open_log();
    write_log("unlock.start version=0.3.3 mode=ngx-cache-decision-bridge "
              "expected-loader=mo2-force-load");

    const std::filesystem::path host_path = module_path(nullptr);
    if (host_path.empty() || !expected_host_candidate(host_path)) {
      write_log("unlock.abort reason=host-precheck-mismatch");
      return 0;
    }

    DiagnosticState diagnostics{};
    diagnostics.host_directory = host_path.parent_path();
    diagnostics.seen_modules.reserve(kMaximumCensusModules);
    const ULONGLONG diagnostic_start_tick = GetTickCount64();
    write_log("diagnostic.start duration-ms=" +
              std::to_string(kDiagnosticDurationMilliseconds));

    const std::filesystem::path feature_path = cache_feature_path();
    if (feature_path.empty()) {
      write_log("unlock.abort reason=programdata-path-unavailable");
      return 0;
    }
    if (find_loaded_module(feature_path) != nullptr) {
      write_log("unlock.abort reason=feature-already-loaded-timing-ambiguous");
      return 0;
    }

    HANDLE guarded_file =
        CreateFileW(feature_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                    nullptr);
    if (guarded_file == INVALID_HANDLE_VALUE) {
      write_log("unlock.abort reason=feature-guard-open-failed error=" +
                std::to_string(GetLastError()));
      return 0;
    }
    ScopedFileHandle feature_guard(guarded_file);
    std::array<unsigned char, 32> feature_hash{};
    Sha256Failure feature_hash_failure{};
    if (!sha256_handle(feature_guard.get(), &feature_hash,
                       kMaximumDiagnosticFileSize, &feature_hash_failure)) {
      write_log(
          "unlock.abort reason=feature-hash-read-failed stage=" +
          std::string(feature_hash_failure.stage) + " status=" +
          std::to_string(static_cast<long>(feature_hash_failure.status)) +
          " error=" + std::to_string(feature_hash_failure.error));
      return 0;
    }
    if (feature_hash != kExpectedFeatureHash) {
      write_log("unlock.abort reason=feature-hash-mismatch expected=" +
                sha256_text(kExpectedFeatureHash) +
                " actual=" + sha256_text(feature_hash));
      return 0;
    }

    PeHeaderIdentity disk_pe{};
    DiskPeFailure disk_pe_failure{};
    if (!read_disk_pe_identity(feature_guard.get(), &disk_pe,
                               &disk_pe_failure)) {
      write_log("unlock.abort reason=feature-disk-pe-read-failed stage=" +
                std::string(disk_pe_failure.stage) + " error=" +
                std::to_string(disk_pe_failure.error));
      return 0;
    }
    if (!mhw::unlock::matches_expected_cache_disk_pe_identity(disk_pe)) {
      write_log(
          "unlock.abort reason=feature-disk-pe-identity-mismatch machine=" +
          hexadecimal(disk_pe.machine) + " optional-magic=" +
          hexadecimal(disk_pe.optional_magic) + " timestamp=" +
          hexadecimal(disk_pe.timestamp) + " preferred-image-base=" +
          hexadecimal(disk_pe.image_base) + " size-of-image=" +
          hexadecimal(disk_pe.image_size));
      return 0;
    }

    if (find_loaded_module(feature_path) != nullptr) {
      write_log("unlock.abort reason=feature-already-loaded-timing-ambiguous");
      return 0;
    }
    HMODULE feature = LoadLibraryExW(feature_path.c_str(), nullptr,
                                     LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                         LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (feature == nullptr) {
      write_log("unlock.abort reason=feature-preload-failed error=" +
                std::to_string(GetLastError()));
      return 0;
    }
    const std::filesystem::path loaded_path = module_path(feature);
    if (loaded_path.empty()) {
      write_log("unlock.abort reason=loaded-feature-path-unavailable");
      FreeLibrary(feature);
      return 0;
    }
    if (!same_file(loaded_path, feature_path)) {
      write_log("unlock.abort reason=loaded-feature-path-mismatch expected=" +
                utf8(feature_path.wstring()) +
                " actual=" + utf8(loaded_path.wstring()));
      FreeLibrary(feature);
      return 0;
    }

    PeHeaderIdentity mapped_pe{};
    if (!read_mapped_pe_identity(feature, &mapped_pe)) {
      write_log("unlock.abort reason=loaded-feature-mapped-pe-read-failed");
      FreeLibrary(feature);
      return 0;
    }
    if (!mhw::unlock::matches_expected_cache_mapped_pe_identity(
            mapped_pe, reinterpret_cast<std::uintptr_t>(feature))) {
      write_log(
          "unlock.abort reason=loaded-feature-mapped-pe-identity-mismatch "
          "machine=" +
          hexadecimal(mapped_pe.machine) + " optional-magic=" +
          hexadecimal(mapped_pe.optional_magic) + " timestamp=" +
          hexadecimal(mapped_pe.timestamp) + " mapped-image-base=" +
          hexadecimal(mapped_pe.image_base) + " actual-module-base=" +
          hexadecimal(reinterpret_cast<std::uintptr_t>(feature)) +
          " size-of-image=" + hexadecimal(mapped_pe.image_size));
      FreeLibrary(feature);
      return 0;
    }

    const FeatureControlFlowChecks control_flow =
        check_feature_control_flow(feature);
    if (!all_feature_control_flow_checks_pass(control_flow)) {
      write_log(
          "unlock.abort reason=loaded-feature-control-flow-mismatch "
          "populate-decision=" +
          std::string(control_flow.populate_decision ? "true" : "false") +
          " description-optimal-call=" +
          (control_flow.description_optimal_call ? "true" : "false") +
          " description-legacy-call=" +
          (control_flow.description_legacy_call ? "true" : "false") +
          " bucket-populate-call=" +
          (control_flow.bucket_populate_call ? "true" : "false") +
          " bucket-optimal-call=" +
          (control_flow.bucket_optimal_call ? "true" : "false"));
      FreeLibrary(feature);
      return 0;
    }

    HMODULE pinned_feature{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(feature),
                            &pinned_feature) ||
        pinned_feature != feature) {
      write_log("unlock.abort reason=feature-pin-failed");
      return 0;
    }
    g_feature_base.store(reinterpret_cast<std::uintptr_t>(feature),
                         std::memory_order_release);
    const ULONGLONG verification_start_tick = GetTickCount64();
    if (!install_hooks(feature)) {
      return 0;
    }
    write_log("feature.preload result=loaded-by-unlock target=ngx-cache-1.2.110");

    mhw::unlock::CacheFeatureIdentity identity{};
    if (!read_cache_feature_identity(loaded_path, feature, feature_hash,
                                     disk_pe, &identity)) {
      g_hook_state.store(HookState::inactive, std::memory_order_release);
      write_log("unlock.abort reason=loaded-feature-runtime-identity-read-failed "
                "action=retain-inert");
      return 0;
    }
    if (!mhw::unlock::matches_expected_cache_feature_identity(identity)) {
      g_hook_state.store(HookState::inactive, std::memory_order_release);
      write_log(
          "unlock.abort reason=loaded-feature-runtime-identity-mismatch "
          "timestamp=" +
          hexadecimal(identity.pe_timestamp) + " preferred-image-base=" +
          hexadecimal(identity.preferred_image_base) + " size-of-image=" +
          hexadecimal(identity.image_size) + " application-id=" +
          hexadecimal(identity.application_id) + " gpu-architecture=" +
          hexadecimal(identity.gpu_architecture) + " snippet-version=" +
          hexadecimal(identity.snippet_version) + " api-version=" +
          hexadecimal(identity.api_version) + " action=retain-inert");
      return 0;
    }
    if (!hash_matches(host_path, kExpectedHostHash)) {
      g_hook_state.store(HookState::inactive, std::memory_order_release);
      write_log("unlock.abort reason=host-hash-mismatch action=retain-inert");
      return 0;
    }
    if (!verify_nvidia_adapter_inventory()) {
      g_hook_state.store(HookState::inactive, std::memory_order_release);
      write_log("unlock.abort reason=nvidia-adapter-inventory-ambiguous "
                "action=retain-inert");
      return 0;
    }
    g_verified_adapter.store(true, std::memory_order_release);
    g_hook_state.store(HookState::active, std::memory_order_release);
    feature_guard.reset();
    write_log("unlock.ready target=ngx-cache-1.2.110 "
              "fallback-gpu-type=7 profile=RTX_2080 allowlist=RTX_5090 "
              "verification-ms=" +
              std::to_string(GetTickCount64() - verification_start_tick));
    write_log("unlock.active lifetime=process");
    try {
      run_bounded_diagnostics(diagnostics, diagnostic_start_tick);
    } catch (...) {
      write_log("diagnostic.failure reason=unexpected-exception");
    }
  } catch (...) {
    g_hook_state.store(HookState::inactive, std::memory_order_release);
    write_log("unlock.failure reason=unexpected-exception action=retain-inert");
  }
  return 0;
}

} // namespace

BOOL APIENTRY DllMain(const HMODULE module, const DWORD reason, LPVOID) {
  if (reason != DLL_PROCESS_ATTACH) {
    return TRUE;
  }
  g_self = module;
  DisableThreadLibraryCalls(module);

  HANDLE thread = CreateThread(nullptr, 0, &worker, nullptr, 0, nullptr);
  if (thread != nullptr) {
    CloseHandle(thread);
  }
  return TRUE;
}
