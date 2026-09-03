#include <windows.h>

#include <bcrypt.h>
#include <dxgi1_2.h>
#include <intrin.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <MinHook.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#pragma intrinsic(_ReturnAddress)
#endif

namespace {

using NgxResult = std::uint32_t;
using NgxParameter = void;
using PopulateParametersFn = NgxResult(__cdecl *)(NgxParameter *);
using GetParametersFn = NgxResult(__cdecl *)(NgxParameter **);
using GetProcAddressForCallerFn = FARPROC(WINAPI *)(HMODULE, LPCSTR, void *);

constexpr std::size_t kHashBufferSize = 1U << 20U;
constexpr std::uint64_t kMaximumHashedFileSize = 128ULL << 20U;
constexpr DWORD kTraceDurationMilliseconds = 30'000U;
constexpr DWORD kTracePollMilliseconds = 25U;
constexpr DWORD kCensusIntervalMilliseconds = 250U;
constexpr DWORD kQuiesceTimeoutMilliseconds = 5'000U;
constexpr std::size_t kEventCapacity = 4U;
constexpr std::size_t kSlotCount = 6U;
constexpr std::uint32_t kMaximumSnapshotEntries = 2'048U;
constexpr std::size_t kMaximumCensusModules = 16U;
constexpr std::uint32_t kResultSuccess = 0x00000001U;
constexpr std::uint32_t kResultFail = 0xBAD00000U;
constexpr std::uint32_t kResultPlatformError = 0xBAD00002U;
constexpr std::uint32_t kExpectedHostTimestamp = 0x66D13CB6U;
constexpr std::uint64_t kExpectedHostPreferredImageBase = 0x140000000ULL;
constexpr std::uint32_t kExpectedHostImageSize = 0x55D3000U;
constexpr std::uint64_t kExpectedNvidiaPreferredImageBase = 0x180000000ULL;

constexpr std::array<unsigned char, 32> kExpectedHostHash{
    0xC2, 0xEB, 0xBB, 0xD2, 0xC4, 0x9F, 0x21, 0x6D,
    0x48, 0x4E, 0x31, 0xA5, 0x21, 0x9B, 0xED, 0x41,
    0x9E, 0xB1, 0xE5, 0xE7, 0xD2, 0x06, 0xD0, 0x2C,
    0xBA, 0x04, 0x0A, 0x3A, 0xB7, 0x9D, 0x90, 0xEA};
constexpr std::array<unsigned char, 32> kExpectedKernelBaseHash{
    0x9D, 0x26, 0x85, 0x24, 0x59, 0x74, 0x53, 0x22,
    0xB7, 0x04, 0xE2, 0xA4, 0x9E, 0x44, 0x89, 0x22,
    0x83, 0x9C, 0x58, 0x4D, 0x06, 0xE2, 0x08, 0xCF,
    0x9A, 0xBC, 0xD0, 0xD8, 0x57, 0xC4, 0xB7, 0xCD};
constexpr std::array<unsigned char, 32> kExpectedKernel32Hash{
    0x0A, 0xB6, 0x1F, 0x2E, 0x0D, 0x41, 0x2A, 0x58,
    0x52, 0x33, 0xF1, 0xB3, 0x08, 0xC1, 0x20, 0xCB,
    0xA7, 0x4F, 0x80, 0x0C, 0x10, 0x30, 0xBF, 0xFE,
    0x0A, 0xAB, 0xD2, 0x0D, 0xF8, 0xC6, 0xD9, 0x07};

constexpr std::array<unsigned char, 32> kLocalFeatureHash{
    0xD4, 0xCC, 0x11, 0xBC, 0xEC, 0x69, 0x3D, 0xFE,
    0xFD, 0x6E, 0xDD, 0xBA, 0x6D, 0xD2, 0x9B, 0xDF,
    0xC1, 0x6B, 0x40, 0x43, 0xE7, 0xB8, 0x31, 0x50,
    0xF0, 0x09, 0x7D, 0x14, 0xB7, 0x65, 0x12, 0xD8};
constexpr std::array<unsigned char, 32> kCacheFeatureHash{
    0xDB, 0x20, 0x38, 0x81, 0xEA, 0x50, 0x28, 0xCB,
    0x58, 0x4A, 0xA6, 0xC5, 0x8D, 0x45, 0x73, 0x61,
    0xDD, 0x0E, 0x6A, 0x28, 0x17, 0x67, 0x98, 0x78,
    0x09, 0x8B, 0x08, 0xB9, 0x40, 0x54, 0x9A, 0x03};
constexpr std::array<unsigned char, 32> kLegacyCoreHash{
    0x3F, 0x59, 0x85, 0x82, 0xD4, 0xC2, 0x35, 0x30,
    0xEF, 0x2F, 0x8B, 0x31, 0x2A, 0xCA, 0x25, 0x61,
    0x75, 0x3E, 0x1C, 0x7B, 0x54, 0x0A, 0x78, 0x22,
    0xDE, 0xFD, 0xC6, 0xDC, 0x23, 0xAC, 0x8D, 0x12};
constexpr std::array<unsigned char, 32> kModernCoreHash{
    0xB3, 0xC8, 0x26, 0x38, 0xC6, 0x0F, 0x29, 0xAE,
    0xFF, 0x68, 0x05, 0x2F, 0xFE, 0x71, 0x47, 0x24,
    0x6A, 0x73, 0xC8, 0x0C, 0xBE, 0xA5, 0x88, 0x3B,
    0x35, 0xA9, 0xB0, 0xD1, 0xED, 0x57, 0xDD, 0xE7};

constexpr std::array<unsigned char, 21> kKernelBaseResolverPrologue{
    0x40, 0x53, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x48, 0x33, 0xFF,
    0x4C, 0x89, 0x74, 0x24, 0x70, 0x4C, 0x89, 0x7C, 0x24, 0x40};
constexpr std::array<unsigned char, 20> kLocalPopulatePrologue{
    0x40, 0x55, 0x53, 0x41, 0x56, 0x48, 0x8D, 0xAC, 0x24, 0x50,
    0xFD, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0xB0, 0x03, 0x00, 0x00};
constexpr std::array<unsigned char, 20> kCachePopulatePrologue{
    0x40, 0x55, 0x53, 0x41, 0x56, 0x48, 0x8D, 0xAC, 0x24, 0x40,
    0xFD, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0xC0, 0x03, 0x00, 0x00};
constexpr std::array<unsigned char, 21> kModernCapabilityPrologue{
    0x40, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05, 0x4B, 0x51,
    0x07, 0x00, 0x48, 0x8B, 0xF9, 0x48, 0x85, 0xC0, 0x75, 0x3B};
constexpr std::array<unsigned char, 21> kModernParametersPrologue{
    0x40, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05, 0xEB, 0x4E,
    0x07, 0x00, 0x48, 0x8B, 0xF9, 0x48, 0x85, 0xC0, 0x75, 0x46};

enum class Slot : std::uint32_t {
  local_populate,
  cache_populate,
  legacy_capability,
  legacy_parameters,
  modern_capability,
  modern_parameters,
};

constexpr std::array<Slot, kSlotCount> kAllSlots{
    Slot::local_populate,     Slot::cache_populate,
    Slot::legacy_capability,  Slot::legacy_parameters,
    Slot::modern_capability,  Slot::modern_parameters};

enum class IdentityState : std::uint32_t {
  empty,
  pending,
  verified,
  rejected,
};

enum class EventState : std::uint32_t {
  empty,
  writing,
  ready,
  logged,
};

struct PeIdentity {
  std::uint16_t machine{};
  std::uint16_t optional_magic{};
  std::uint32_t timestamp{};
  std::uint64_t image_base{};
  std::uint32_t image_size{};
};

struct TraceEventPayload {
  std::uint64_t sequence{};
  ULONGLONG tick{};
  ULONGLONG duration_ticks{};
  DWORD thread_id{};
  std::uintptr_t caller{};
  std::uintptr_t argument{};
  NgxResult result{};
  bool argument_nonnull{};
};

struct TraceEvent {
  std::atomic<EventState> state{EventState::empty};
  TraceEventPayload payload{};
};

struct SlotRuntime {
  std::atomic<std::uintptr_t> target{};
  std::atomic<std::uintptr_t> module{};
  std::atomic<std::uintptr_t> resolver_caller{};
  std::atomic<std::uint64_t> resolver_tick{};
  std::atomic<IdentityState> identity{IdentityState::empty};
  std::atomic<std::uint64_t> resolves{};
  std::atomic<std::uint64_t> calls{};
  std::atomic<std::uint64_t> success{};
  std::atomic<std::uint64_t> bad00000{};
  std::atomic<std::uint64_t> bad00002{};
  std::atomic<std::uint64_t> other{};
  std::atomic<std::uint64_t> recursion{};
  std::atomic<std::uint64_t> conflicts{};
  std::atomic<std::uint64_t> dropped{};
  std::atomic<std::uint32_t> reserved_events{};
  std::array<TraceEvent, kEventCapacity> events{};
};

static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<EventState>::is_always_lock_free);

std::array<SlotRuntime, kSlotCount> g_slots{};
std::atomic<std::uint64_t> g_event_sequence{};
std::atomic<std::uint64_t> g_resolver_calls{};
std::atomic<std::uint64_t> g_name_rejects{};
std::atomic<std::uint64_t> g_target_rejects{};
std::atomic<std::uint64_t> g_target_nulls{};
std::atomic<std::uint64_t> g_resolver_before_ready{};
std::atomic<std::uint64_t> g_active_resolver_detours{};
std::atomic<std::uint64_t> g_active_wrapper_calls{};
std::atomic<bool> g_trace_enabled{};
std::atomic<int> g_trace_state{};
std::atomic<std::uintptr_t> g_host_base{};
std::atomic<std::uint32_t> g_host_image_size{};
std::atomic<std::uintptr_t> g_original_resolver{};
HMODULE g_self{};
ULONGLONG g_attach_tick{};
thread_local std::uint32_t g_active_slots{};
std::ofstream g_log;

constexpr std::size_t slot_index(const Slot slot) noexcept {
  return static_cast<std::size_t>(slot);
}

SlotRuntime &slot_runtime(const Slot slot) noexcept {
  return g_slots[slot_index(slot)];
}

const char *slot_name(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return "local-populate";
  case Slot::cache_populate:
    return "cache-populate";
  case Slot::legacy_capability:
    return "legacy-core-capability";
  case Slot::legacy_parameters:
    return "legacy-core-parameters";
  case Slot::modern_capability:
    return "modern-core-capability";
  case Slot::modern_parameters:
    return "modern-core-parameters";
  }
  return "unknown";
}

const char *identity_name(const IdentityState state) noexcept {
  switch (state) {
  case IdentityState::empty:
    return "empty";
  case IdentityState::pending:
    return "pending";
  case IdentityState::verified:
    return "verified";
  case IdentityState::rejected:
    return "rejected";
  }
  return "unknown";
}

std::filesystem::path module_path(const HMODULE module) {
  wchar_t path[32'768]{};
  const DWORD length =
      GetModuleFileNameW(module, path, static_cast<DWORD>(_countof(path)));
  if (length == 0U || length >= _countof(path)) {
    return {};
  }
  return std::filesystem::path(path, path + length);
}

std::filesystem::path trace_log_path() {
  wchar_t local_app_data[32'768]{};
  const DWORD length = GetEnvironmentVariableW(
      L"LOCALAPPDATA", local_app_data,
      static_cast<DWORD>(_countof(local_app_data)));
  if (length != 0U && length < _countof(local_app_data)) {
    return std::filesystem::path(local_app_data) / L"MhwDlssUnlock" /
           (L"unlock-" + std::to_wstring(GetCurrentProcessId()) + L".log");
  }
  return std::filesystem::temp_directory_path() /
         (L"mhw-dlss-unlock-" + std::to_wstring(GetCurrentProcessId()) +
          L".log");
}

void open_log() noexcept {
  try {
    const std::filesystem::path path = trace_log_path();
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

std::string hexadecimal(const std::uint64_t value) {
  char buffer[19]{};
  _snprintf_s(buffer, _TRUNCATE, "0x%llX",
              static_cast<unsigned long long>(value));
  return buffer;
}

std::string sha256_text(const std::array<unsigned char, 32> &hash) {
  constexpr char digits[] = "0123456789ABCDEF";
  std::string result(hash.size() * 2U, '0');
  for (std::size_t index = 0U; index < hash.size(); ++index) {
    const unsigned char value = hash[index];
    result[index * 2U] = digits[value >> 4U];
    result[index * 2U + 1U] = digits[value & 0x0FU];
  }
  return result;
}

bool sha256_handle(const HANDLE file,
                   std::array<unsigned char, 32> *const output) noexcept {
  if (file == nullptr || file == INVALID_HANDLE_VALUE || output == nullptr) {
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
  std::array<unsigned char, 32> digest{};
  std::uint64_t total_bytes{};
  bool success = false;
  NTSTATUS status{};

  if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart < 0 ||
      static_cast<std::uint64_t>(file_size.QuadPart) >
          kMaximumHashedFileSize ||
      !SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) {
    goto cleanup;
  }
  status =
      BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (status != 0) {
    goto cleanup;
  }
  status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                             reinterpret_cast<PUCHAR>(&object_size),
                             sizeof(object_size), &result_size, 0);
  if (status != 0) {
    goto cleanup;
  }
  status = BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                             reinterpret_cast<PUCHAR>(&hash_size),
                             sizeof(hash_size), &result_size, 0);
  if (status != 0 || hash_size != digest.size()) {
    goto cleanup;
  }
  hash_object =
      static_cast<unsigned char *>(HeapAlloc(GetProcessHeap(), 0, object_size));
  if (hash_object == nullptr) {
    goto cleanup;
  }
  status = BCryptCreateHash(algorithm, &hash, hash_object, object_size, nullptr,
                            0, 0);
  if (status != 0) {
    goto cleanup;
  }
  buffer = static_cast<unsigned char *>(
      HeapAlloc(GetProcessHeap(), 0, kHashBufferSize));
  if (buffer == nullptr) {
    goto cleanup;
  }
  for (;;) {
    DWORD bytes_read{};
    if (!ReadFile(file, buffer, static_cast<DWORD>(kHashBufferSize), &bytes_read,
                  nullptr)) {
      goto cleanup;
    }
    if (bytes_read == 0U) {
      break;
    }
    total_bytes += bytes_read;
    status = BCryptHashData(hash, buffer, bytes_read, 0);
    if (status != 0) {
      goto cleanup;
    }
  }
  if (total_bytes != static_cast<std::uint64_t>(file_size.QuadPart)) {
    goto cleanup;
  }
  status = BCryptFinishHash(hash, digest.data(),
                            static_cast<ULONG>(digest.size()), 0);
  if (status != 0) {
    goto cleanup;
  }
  *output = digest;
  success = true;

cleanup:
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

bool sha256_file(const std::filesystem::path &path,
                 std::array<unsigned char, 32> *const output) noexcept {
  const HANDLE file =
      CreateFileW(path.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  const bool result = sha256_handle(file, output);
  CloseHandle(file);
  return result;
}

bool hash_matches(const std::filesystem::path &path,
                  const std::array<unsigned char, 32> &expected) noexcept {
  std::array<unsigned char, 32> actual{};
  return sha256_file(path, &actual) && actual == expected;
}

bool read_mapped_pe_identity(const HMODULE module,
                             PeIdentity *const output) noexcept {
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
    output->timestamp = nt->FileHeader.TimeDateStamp;
    output->image_base = nt->OptionalHeader.ImageBase;
    output->image_size = nt->OptionalHeader.SizeOfImage;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool read_disk_pe_identity(const std::filesystem::path &path,
                           PeIdentity *const output) noexcept {
  if (output == nullptr) {
    return false;
  }
  const HANDLE file =
      CreateFileW(path.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  IMAGE_DOS_HEADER dos{};
  IMAGE_NT_HEADERS64 nt{};
  DWORD bytes_read{};
  bool result = false;
  if (ReadFile(file, &dos, static_cast<DWORD>(sizeof(dos)), &bytes_read,
               nullptr) &&
      bytes_read == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
      dos.e_lfanew > 0 &&
      static_cast<std::uint32_t>(dos.e_lfanew) <= 0x10'0000U) {
    LARGE_INTEGER position{};
    position.QuadPart = dos.e_lfanew;
    if (SetFilePointerEx(file, position, nullptr, FILE_BEGIN) &&
        ReadFile(file, &nt, static_cast<DWORD>(sizeof(nt)), &bytes_read,
                 nullptr) &&
        bytes_read == sizeof(nt) && nt.Signature == IMAGE_NT_SIGNATURE &&
        nt.FileHeader.SizeOfOptionalHeader >= sizeof(IMAGE_OPTIONAL_HEADER64)) {
      output->machine = nt.FileHeader.Machine;
      output->optional_magic = nt.OptionalHeader.Magic;
      output->timestamp = nt.FileHeader.TimeDateStamp;
      output->image_base = nt.OptionalHeader.ImageBase;
      output->image_size = nt.OptionalHeader.SizeOfImage;
      result = true;
    }
  }
  CloseHandle(file);
  return result;
}

std::array<std::uint16_t, 4>
file_version(const std::filesystem::path &path) noexcept {
  std::array<std::uint16_t, 4> result{};
  try {
    DWORD ignored{};
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0U || size > (16U << 20U)) {
      return result;
    }
    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0U, size, data.data())) {
      return result;
    }
    void *raw{};
    UINT raw_size{};
    if (!VerQueryValueW(data.data(), L"\\", &raw, &raw_size) ||
        raw == nullptr || raw_size < sizeof(VS_FIXEDFILEINFO)) {
      return result;
    }
    const auto *info = static_cast<const VS_FIXEDFILEINFO *>(raw);
    if (info->dwSignature != VS_FFI_SIGNATURE) {
      return result;
    }
    result = {
        static_cast<std::uint16_t>(HIWORD(info->dwFileVersionMS)),
        static_cast<std::uint16_t>(LOWORD(info->dwFileVersionMS)),
        static_cast<std::uint16_t>(HIWORD(info->dwFileVersionLS)),
        static_cast<std::uint16_t>(LOWORD(info->dwFileVersionLS))};
  } catch (...) {
  }
  return result;
}

wchar_t ascii_lower(const wchar_t value) noexcept {
  if (value >= L'A' && value <= L'Z') {
    return static_cast<wchar_t>(value + (L'a' - L'A'));
  }
  return value;
}

bool equal_path_text(const std::wstring_view left,
                     const std::wstring_view right) noexcept {
  return left.size() == right.size() &&
         (left.empty() ||
          CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
                               right.data(), static_cast<int>(right.size()),
                               TRUE) == CSTR_EQUAL);
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

std::uint32_t expected_timestamp(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return 0x5DE9C0EBU;
  case Slot::cache_populate:
    return 0x64405395U;
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return 0x60E65AC5U;
  case Slot::modern_capability:
  case Slot::modern_parameters:
    return 0x6A0CB15EU;
  }
  return 0U;
}

std::uint32_t expected_image_size(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return 0x00583000U;
  case Slot::cache_populate:
    return 0x00B57000U;
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return 0x00078000U;
  case Slot::modern_capability:
  case Slot::modern_parameters:
    return 0x00173000U;
  }
  return 0U;
}

std::uintptr_t expected_rva(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return 0x180E0U;
  case Slot::cache_populate:
    return 0x1DFA0U;
  case Slot::legacy_capability:
    return 0x0A53BU;
  case Slot::legacy_parameters:
    return 0x0A582U;
  case Slot::modern_capability:
    return 0x669F0U;
  case Slot::modern_parameters:
    return 0x66C50U;
  }
  return 0U;
}

const std::array<unsigned char, 32> &expected_hash(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return kLocalFeatureHash;
  case Slot::cache_populate:
    return kCacheFeatureHash;
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return kLegacyCoreHash;
  case Slot::modern_capability:
  case Slot::modern_parameters:
    return kModernCoreHash;
  }
  return kLocalFeatureHash;
}

std::array<std::uint16_t, 4> expected_version(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return {1U, 1U, 13U, 0U};
  case Slot::cache_populate:
    return {1U, 2U, 110U, 0U};
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return {30U, 0U, 14U, 9516U};
  case Slot::modern_capability:
  case Slot::modern_parameters:
    return {32U, 0U, 16U, 1047U};
  }
  return {};
}

bool executable_image_address(const HMODULE module,
                              const void *const address) noexcept {
  if (module == nullptr || address == nullptr) {
    return false;
  }
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(address, &information, sizeof(information)) !=
          sizeof(information) ||
      information.State != MEM_COMMIT || information.Type != MEM_IMAGE ||
      information.AllocationBase != module ||
      (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
    return false;
  }
  const DWORD protection = information.Protect & 0xFFU;
  return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
         protection == PAGE_EXECUTE_READWRITE ||
         protection == PAGE_EXECUTE_WRITECOPY;
}

template <std::size_t Size>
bool exact_bytes(const void *const address,
                 const std::array<unsigned char, Size> &expected) noexcept {
  if (address == nullptr) {
    return false;
  }
  __try {
    return std::memcmp(address, expected.data(), expected.size()) == 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool legacy_stub_matches(const HMODULE module, const void *const address,
                         const Slot slot) noexcept {
  if (slot != Slot::legacy_capability && slot != Slot::legacy_parameters) {
    return false;
  }
  const auto *bytes = static_cast<const unsigned char *>(address);
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  __try {
    for (std::size_t index = 0U; index < 20U; ++index) {
      if (bytes[index] != 0x90U) {
        return false;
      }
    }
    constexpr std::array<unsigned char, 9> first_tail{
        0x48, 0x8B, 0x00, 0x48, 0x83, 0xF8, 0x00, 0x74, 0xED};
    constexpr std::array<unsigned char, 9> second_tail{
        0x48, 0x8B, 0x00, 0x48, 0x83, 0xF8, 0x00, 0x75, 0x0B};
    constexpr std::array<unsigned char, 13> ending{
        0x48, 0xB8, 0x02, 0x00, 0xD0, 0xBA, 0x00,
        0x00, 0x00, 0x00, 0xC3, 0x50, 0xC3};
    if (bytes[20] != 0x48U || bytes[21] != 0xB8U ||
        std::memcmp(bytes + 30U, first_tail.data(), first_tail.size()) != 0 ||
        bytes[39] != 0x48U || bytes[40] != 0xB8U ||
        std::memcmp(bytes + 49U, second_tail.data(), second_tail.size()) != 0 ||
        std::memcmp(bytes + 58U, ending.data(), ending.size()) != 0) {
      return false;
    }
    std::uint64_t ready_slot{};
    std::uint64_t target_slot{};
    std::memcpy(&ready_slot, bytes + 22U, sizeof(ready_slot));
    std::memcpy(&target_slot, bytes + 41U, sizeof(target_slot));
    const std::uintptr_t expected_target_slot =
        base + (slot == Slot::legacy_capability ? 0x6E3C8U : 0x6E3D0U);
    return ready_slot == base + 0x6E4B8U &&
           target_slot == expected_target_slot;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool slot_prologue_matches(const Slot slot, const HMODULE module,
                           const void *const address) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return exact_bytes(address, kLocalPopulatePrologue);
  case Slot::cache_populate:
    return exact_bytes(address, kCachePopulatePrologue);
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return legacy_stub_matches(module, address, slot);
  case Slot::modern_capability:
    return exact_bytes(address, kModernCapabilityPrologue);
  case Slot::modern_parameters:
    return exact_bytes(address, kModernParametersPrologue);
  }
  return false;
}

bool mapped_slot_matches(const Slot slot, const HMODULE module,
                         const FARPROC target) noexcept {
  if (module == nullptr || target == nullptr ||
      !executable_image_address(module, reinterpret_cast<const void *>(target))) {
    return false;
  }
  PeIdentity identity{};
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  return read_mapped_pe_identity(module, &identity) &&
         identity.machine == IMAGE_FILE_MACHINE_AMD64 &&
         identity.optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
         identity.timestamp == expected_timestamp(slot) &&
         identity.image_size == expected_image_size(slot) &&
         identity.image_base == base &&
         reinterpret_cast<std::uintptr_t>(target) == base + expected_rva(slot) &&
         slot_prologue_matches(slot, module,
                               reinterpret_cast<const void *>(target));
}

bool caller_is_host_executable(const void *const caller) noexcept {
  const auto host = g_host_base.load(std::memory_order_acquire);
  const auto image_size = g_host_image_size.load(std::memory_order_acquire);
  const auto address = reinterpret_cast<std::uintptr_t>(caller);
  if (host == 0U || image_size == 0U || address < host ||
      address >= host + image_size ||
      !executable_image_address(reinterpret_cast<HMODULE>(host), caller)) {
    return false;
  }

  const auto *base = reinterpret_cast<const unsigned char *>(host);
  __try {
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::uint32_t>(dos->e_lfanew) > 0x10'0000U) {
      return false;
    }
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
        base + static_cast<std::size_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.NumberOfSections == 0U ||
        nt->FileHeader.NumberOfSections > 96U) {
      return false;
    }
    const std::uintptr_t rva = address - host;
    const IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
    for (std::uint16_t index = 0U; index < nt->FileHeader.NumberOfSections;
         ++index) {
      const std::uint32_t section_size = section[index].Misc.VirtualSize;
      const std::uintptr_t begin = section[index].VirtualAddress;
      const std::uintptr_t end = begin + section_size;
      if (section_size != 0U && rva >= begin && rva < end) {
        return (section[index].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0U;
      }
    }
    return false;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool caller_is_module_rva(const void *const caller,
                          const std::uint32_t timestamp,
                          const std::uint32_t image_size,
                          const std::uintptr_t expected_caller_rva) noexcept {
  if (caller == nullptr) {
    return false;
  }
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(caller, &information, sizeof(information)) !=
          sizeof(information) ||
      information.Type != MEM_IMAGE || information.AllocationBase == nullptr) {
    return false;
  }
  const auto module = static_cast<HMODULE>(information.AllocationBase);
  PeIdentity identity{};
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  return read_mapped_pe_identity(module, &identity) &&
         identity.machine == IMAGE_FILE_MACHINE_AMD64 &&
         identity.optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
         identity.timestamp == timestamp && identity.image_size == image_size &&
         identity.image_base == base &&
         reinterpret_cast<std::uintptr_t>(caller) ==
              base + expected_caller_rva &&
         executable_image_address(module, caller);
}

bool expected_resolver_caller(const Slot slot, const void *const caller) noexcept {
  switch (slot) {
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return caller_is_host_executable(caller);
  case Slot::modern_capability:
    return caller_is_module_rva(caller, 0x60E65AC5U, 0x00078000U,
                                0x09A12U);
  case Slot::modern_parameters:
    return caller_is_module_rva(caller, 0x60E65AC5U, 0x00078000U,
                                0x09A2AU);
  case Slot::local_populate:
  case Slot::cache_populate:
    return caller_is_module_rva(caller, 0x6A0CB15EU, 0x00173000U,
                                0x43333U) ||
           caller_is_module_rva(caller, 0x6A0CB15EU, 0x00173000U,
                                0x43351U);
  }
  return false;
}

bool procedure_name_equals(const LPCSTR name,
                           const char *const expected) noexcept {
  if (name == nullptr || expected == nullptr ||
      reinterpret_cast<std::uintptr_t>(name) <= 0xFFFFU) {
    return false;
  }
  __try {
    for (std::size_t index = 0U;; ++index) {
      if (name[index] != expected[index]) {
        return false;
      }
      if (expected[index] == '\0') {
        return true;
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

void publish_event(const Slot slot, const void *const caller,
                   const void *const argument, const NgxResult result,
                   const ULONGLONG duration_ticks) noexcept {
  SlotRuntime &runtime = slot_runtime(slot);
  runtime.calls.fetch_add(1U, std::memory_order_relaxed);
  if (result == kResultSuccess) {
    runtime.success.fetch_add(1U, std::memory_order_relaxed);
  } else if (result == kResultFail) {
    runtime.bad00000.fetch_add(1U, std::memory_order_relaxed);
  } else if (result == kResultPlatformError) {
    runtime.bad00002.fetch_add(1U, std::memory_order_relaxed);
  } else {
    runtime.other.fetch_add(1U, std::memory_order_relaxed);
  }

  const std::uint32_t reserved =
      runtime.reserved_events.fetch_add(1U, std::memory_order_relaxed);
  if (reserved >= runtime.events.size()) {
    runtime.dropped.fetch_add(1U, std::memory_order_relaxed);
    return;
  }
  TraceEvent &event = runtime.events[reserved];
  EventState expected = EventState::empty;
  if (!event.state.compare_exchange_strong(
          expected, EventState::writing, std::memory_order_acq_rel,
          std::memory_order_relaxed)) {
    runtime.dropped.fetch_add(1U, std::memory_order_relaxed);
    return;
  }
  event.payload.sequence =
      g_event_sequence.fetch_add(1U, std::memory_order_relaxed) + 1U;
  event.payload.tick = GetTickCount64();
  event.payload.duration_ticks = duration_ticks;
  event.payload.thread_id = GetCurrentThreadId();
  event.payload.caller = reinterpret_cast<std::uintptr_t>(caller);
  event.payload.argument = reinterpret_cast<std::uintptr_t>(argument);
  event.payload.result = result;
  event.payload.argument_nonnull = argument != nullptr;
  event.state.store(EventState::ready, std::memory_order_release);
}

template <Slot TargetSlot>
NgxResult invoke_populate(NgxParameter *const parameters, const void *const caller,
                          const DWORD incoming_last_error) noexcept {
  SlotRuntime &runtime = slot_runtime(TargetSlot);
  const auto original = reinterpret_cast<PopulateParametersFn>(
      runtime.target.load(std::memory_order_acquire));
  if (original == nullptr) {
    SetLastError(incoming_last_error);
    return kResultFail;
  }

  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  const bool observe =
      g_trace_enabled.load(std::memory_order_seq_cst) &&
      runtime.identity.load(std::memory_order_acquire) !=
          IdentityState::rejected;
  if (!observe) {
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return original(parameters);
  }
  constexpr std::uint32_t mask = 1U << slot_index(TargetSlot);
  const bool recursive = (g_active_slots & mask) != 0U;
  if (recursive) {
    runtime.recursion.fetch_add(1U, std::memory_order_relaxed);
  }
  const std::uint32_t previous_mask = g_active_slots;
  g_active_slots |= mask;
  const ULONGLONG start = GetTickCount64();
  SetLastError(incoming_last_error);
  const NgxResult result = original(parameters);
  const DWORD outgoing_last_error = GetLastError();
  const ULONGLONG duration = GetTickCount64() - start;
  g_active_slots = previous_mask;
  if (!recursive) {
    publish_event(TargetSlot, caller, parameters, result, duration);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(outgoing_last_error);
  return result;
}

template <Slot TargetSlot>
NgxResult invoke_get_parameters(NgxParameter **const parameters,
                                const void *const caller,
                                const DWORD incoming_last_error) noexcept {
  SlotRuntime &runtime = slot_runtime(TargetSlot);
  const auto original = reinterpret_cast<GetParametersFn>(
      runtime.target.load(std::memory_order_acquire));
  if (original == nullptr) {
    SetLastError(incoming_last_error);
    return kResultFail;
  }

  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  const bool observe =
      g_trace_enabled.load(std::memory_order_seq_cst) &&
      runtime.identity.load(std::memory_order_acquire) !=
          IdentityState::rejected;
  if (!observe) {
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return original(parameters);
  }
  constexpr std::uint32_t mask = 1U << slot_index(TargetSlot);
  const bool recursive = (g_active_slots & mask) != 0U;
  if (recursive) {
    runtime.recursion.fetch_add(1U, std::memory_order_relaxed);
  }
  const std::uint32_t previous_mask = g_active_slots;
  g_active_slots |= mask;
  const ULONGLONG start = GetTickCount64();
  SetLastError(incoming_last_error);
  const NgxResult result = original(parameters);
  const DWORD outgoing_last_error = GetLastError();
  const ULONGLONG duration = GetTickCount64() - start;
  g_active_slots = previous_mask;
  if (!recursive) {
    publish_event(TargetSlot, caller, parameters, result, duration);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(outgoing_last_error);
  return result;
}

} // namespace

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
local_populate_wrapper(NgxParameter *parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_populate<Slot::local_populate>(parameters, caller,
                                               incoming_last_error);
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
cache_populate_wrapper(NgxParameter *parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_populate<Slot::cache_populate>(parameters, caller,
                                               incoming_last_error);
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
legacy_capability_wrapper(NgxParameter **parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_get_parameters<Slot::legacy_capability>(
      parameters, caller, incoming_last_error);
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
legacy_parameters_wrapper(NgxParameter **parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_get_parameters<Slot::legacy_parameters>(
      parameters, caller, incoming_last_error);
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
modern_capability_wrapper(NgxParameter **parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_get_parameters<Slot::modern_capability>(
      parameters, caller, incoming_last_error);
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
modern_parameters_wrapper(NgxParameter **parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_get_parameters<Slot::modern_parameters>(
      parameters, caller, incoming_last_error);
}

namespace {

FARPROC wrapper_for_slot(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return reinterpret_cast<FARPROC>(&local_populate_wrapper);
  case Slot::cache_populate:
    return reinterpret_cast<FARPROC>(&cache_populate_wrapper);
  case Slot::legacy_capability:
    return reinterpret_cast<FARPROC>(&legacy_capability_wrapper);
  case Slot::legacy_parameters:
    return reinterpret_cast<FARPROC>(&legacy_parameters_wrapper);
  case Slot::modern_capability:
    return reinterpret_cast<FARPROC>(&modern_capability_wrapper);
  case Slot::modern_parameters:
    return reinterpret_cast<FARPROC>(&modern_parameters_wrapper);
  }
  return nullptr;
}

bool find_matching_slot(const LPCSTR name, const HMODULE module,
                        const FARPROC target, const void *const caller,
                        Slot *const output) noexcept {
  if (output == nullptr) {
    return false;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_PopulateParameters")) {
    for (const Slot slot : {Slot::local_populate, Slot::cache_populate}) {
      if (mapped_slot_matches(slot, module, target) &&
          expected_resolver_caller(slot, caller)) {
        *output = slot;
        return true;
      }
    }
    return false;
  }
  if (procedure_name_equals(name,
                            "NVSDK_NGX_D3D12_GetCapabilityParameters")) {
    for (const Slot slot : {Slot::legacy_capability,
                            Slot::modern_capability}) {
      if (mapped_slot_matches(slot, module, target) &&
          expected_resolver_caller(slot, caller)) {
        *output = slot;
        return true;
      }
    }
    return false;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_GetParameters")) {
    for (const Slot slot : {Slot::legacy_parameters,
                            Slot::modern_parameters}) {
      if (mapped_slot_matches(slot, module, target) &&
          expected_resolver_caller(slot, caller)) {
        *output = slot;
        return true;
      }
    }
    return false;
  }
  return false;
}

bool is_target_name(const LPCSTR name) noexcept {
  return procedure_name_equals(name, "NVSDK_NGX_PopulateParameters") ||
         procedure_name_equals(
             name, "NVSDK_NGX_D3D12_GetCapabilityParameters") ||
         procedure_name_equals(name, "NVSDK_NGX_D3D12_GetParameters");
}

FARPROC bind_slot(const Slot slot, const HMODULE module, const FARPROC target,
                   const void *const caller) noexcept {
  SlotRuntime &runtime = slot_runtime(slot);
  runtime.resolves.fetch_add(1U, std::memory_order_relaxed);
  if (runtime.identity.load(std::memory_order_acquire) ==
      IdentityState::rejected) {
    return target;
  }
  const std::uintptr_t target_value =
      reinterpret_cast<std::uintptr_t>(target);
  std::uintptr_t expected = 0U;
  if (runtime.target.compare_exchange_strong(
          expected, target_value, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    runtime.module.store(reinterpret_cast<std::uintptr_t>(module),
                         std::memory_order_release);
    runtime.resolver_caller.store(reinterpret_cast<std::uintptr_t>(caller),
                                  std::memory_order_release);
    runtime.resolver_tick.store(GetTickCount64(), std::memory_order_release);
    runtime.identity.store(IdentityState::pending, std::memory_order_release);
    return wrapper_for_slot(slot);
  }
  if (expected == target_value) {
    return wrapper_for_slot(slot);
  }
  runtime.conflicts.fetch_add(1U, std::memory_order_relaxed);
  return target;
}

extern "C" __declspec(noinline) FARPROC WINAPI
get_proc_address_for_caller_detour(const HMODULE module, const LPCSTR name,
                                   void *const caller) {
  const DWORD incoming_last_error = GetLastError();
  g_active_resolver_detours.fetch_add(1U, std::memory_order_seq_cst);
  const bool observe = g_trace_enabled.load(std::memory_order_seq_cst);
  const auto original = reinterpret_cast<GetProcAddressForCallerFn>(
      g_original_resolver.load(std::memory_order_acquire));
  if (original == nullptr) {
    g_active_resolver_detours.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return nullptr;
  }
  SetLastError(incoming_last_error);
  const FARPROC target = original(module, name, caller);
  const DWORD outgoing_last_error = GetLastError();
  FARPROC returned = target;
  if (!observe) {
    if (g_trace_state.load(std::memory_order_acquire) == 0) {
      g_resolver_before_ready.fetch_add(1U, std::memory_order_relaxed);
    }
  } else {
    g_resolver_calls.fetch_add(1U, std::memory_order_relaxed);
    const bool target_name = is_target_name(name);
    if (!target_name) {
      g_name_rejects.fetch_add(1U, std::memory_order_relaxed);
    } else if (target == nullptr) {
      g_target_nulls.fetch_add(1U, std::memory_order_relaxed);
    } else {
      Slot slot{};
      if (find_matching_slot(name, module, target, caller, &slot)) {
        returned = bind_slot(slot, module, target, caller);
      } else {
        g_target_rejects.fetch_add(1U, std::memory_order_relaxed);
      }
    }
  }

  g_active_resolver_detours.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(outgoing_last_error);
  return returned;
}

bool mapped_identity_matches(const HMODULE module,
                             const std::uint32_t timestamp,
                             const std::uint32_t image_size) noexcept {
  PeIdentity identity{};
  return read_mapped_pe_identity(module, &identity) &&
         identity.machine == IMAGE_FILE_MACHINE_AMD64 &&
         identity.optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
         identity.timestamp == timestamp && identity.image_size == image_size &&
         identity.image_base == reinterpret_cast<std::uintptr_t>(module);
}

bool kernel32_stub_targets_resolver(const FARPROC kernel32_get_proc_address,
                                    const FARPROC resolver) noexcept {
  if (kernel32_get_proc_address == nullptr || resolver == nullptr) {
    return false;
  }
  const auto *bytes =
      reinterpret_cast<const unsigned char *>(kernel32_get_proc_address);
  __try {
    constexpr std::array<unsigned char, 7> prefix{
        0x4C, 0x8B, 0x04, 0x24, 0x48, 0xFF, 0x25};
    if (std::memcmp(bytes, prefix.data(), prefix.size()) != 0) {
      return false;
    }
    std::int32_t displacement{};
    std::memcpy(&displacement, bytes + prefix.size(), sizeof(displacement));
    const auto next = reinterpret_cast<std::uintptr_t>(bytes + 11U);
    const auto slot_address = static_cast<std::uintptr_t>(
        static_cast<std::intptr_t>(next) + displacement);
    const auto *slot = reinterpret_cast<const void *const *>(slot_address);
    return *slot == reinterpret_cast<const void *>(resolver);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool resolve_and_verify_system_target(void **const output) {
  if (output == nullptr) {
    return false;
  }
  *output = nullptr;
  const HMODULE kernel_base = GetModuleHandleW(L"KernelBase.dll");
  const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (kernel_base == nullptr || kernel32 == nullptr) {
    write_log("trace.abort reason=system-resolver-modules-missing");
    return false;
  }
  const std::filesystem::path kernel_base_path = module_path(kernel_base);
  const std::filesystem::path kernel32_path = module_path(kernel32);
  if (kernel_base_path.empty() || kernel32_path.empty() ||
      !hash_matches(kernel_base_path, kExpectedKernelBaseHash) ||
      !hash_matches(kernel32_path, kExpectedKernel32Hash)) {
    write_log("trace.abort reason=system-resolver-hash-mismatch");
    return false;
  }
  if (!mapped_identity_matches(kernel_base, 0xEF4C9CABU, 0x003FF000U) ||
      !mapped_identity_matches(kernel32, 0xFB84BF00U, 0x000C9000U)) {
    write_log("trace.abort reason=system-resolver-pe-mismatch");
    return false;
  }

  const FARPROC resolver =
      GetProcAddress(kernel_base, "GetProcAddressForCaller");
  const FARPROC kernel32_get_proc_address =
      GetProcAddress(kernel32, "GetProcAddress");
  const auto kernel_base_address =
      reinterpret_cast<std::uintptr_t>(kernel_base);
  const auto kernel32_address = reinterpret_cast<std::uintptr_t>(kernel32);
  if (resolver == nullptr || kernel32_get_proc_address == nullptr ||
      reinterpret_cast<std::uintptr_t>(resolver) !=
          kernel_base_address + 0x0B30B0U ||
      reinterpret_cast<std::uintptr_t>(kernel32_get_proc_address) !=
          kernel32_address + 0x032060U ||
      !executable_image_address(kernel_base,
                                reinterpret_cast<const void *>(resolver)) ||
      !exact_bytes(reinterpret_cast<const void *>(resolver),
                   kKernelBaseResolverPrologue) ||
      !kernel32_stub_targets_resolver(kernel32_get_proc_address, resolver)) {
    write_log("trace.abort reason=system-resolver-control-flow-mismatch");
    return false;
  }
  *output = reinterpret_cast<void *>(resolver);
  write_log("trace.system-boundary verified=true kernelbase-sha256=" +
            sha256_text(kExpectedKernelBaseHash) + " kernel32-sha256=" +
            sha256_text(kExpectedKernel32Hash));
  return true;
}

bool verify_nvidia_adapter_inventory() {
  IDXGIFactory1 *factory{};
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || factory == nullptr) {
    write_log("gpu.enumeration result=factory-failed");
    return false;
  }
  std::uint32_t hardware_count{};
  std::uint32_t nvidia_count{};
  std::uint32_t rtx5090_count{};
  bool complete = true;
  for (UINT index = 0U;; ++index) {
    IDXGIAdapter1 *adapter{};
    const HRESULT status = factory->EnumAdapters1(index, &adapter);
    if (status == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(status) || adapter == nullptr) {
      complete = false;
      if (adapter != nullptr) {
        adapter->Release();
      }
      break;
    }
    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(adapter->GetDesc1(&description))) {
      complete = false;
      adapter->Release();
      break;
    }
    const bool software_or_remote =
        (description.Flags &
         (DXGI_ADAPTER_FLAG_SOFTWARE | DXGI_ADAPTER_FLAG_REMOTE)) != 0U;
    const std::wstring_view name(description.Description);
    write_log("gpu.adapter index=" + std::to_string(index) + " vendor=" +
              hexadecimal(description.VendorId) + " software-or-remote=" +
              (software_or_remote ? "true" : "false") + " name=" +
              utf8(name));
    if (!software_or_remote) {
      ++hardware_count;
      if (description.VendorId == 0x10DEU) {
        ++nvidia_count;
      }
      if (description.VendorId == 0x10DEU &&
          name == L"NVIDIA GeForce RTX 5090") {
        ++rtx5090_count;
      }
    }
    adapter->Release();
  }
  factory->Release();
  write_log("gpu.inventory hardware=" + std::to_string(hardware_count) +
            " nvidia-hardware=" + std::to_string(nvidia_count) +
            " allowlisted-rtx5090=" + std::to_string(rtx5090_count) +
            " complete=" + (complete ? "true" : "false"));
  return complete && hardware_count == 1U && nvidia_count == 1U &&
         rtx5090_count == 1U;
}

bool relevant_ngx_module_path(const std::filesystem::path &path) {
  const std::wstring filename = path.filename().wstring();
  const std::wstring full = path.wstring();
  return equal_path_text(filename, L"nvngx.dll") ||
         equal_path_text(filename, L"_nvngx.dll") ||
         equal_path_text(filename, L"nvngx_dlss.dll") ||
         contains_ascii_case_insensitive(full, L"\\NVIDIA\\NGX\\models\\");
}

bool ngx_module_already_loaded() noexcept {
  const HANDLE snapshot = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (snapshot == INVALID_HANDLE_VALUE) {
    return true;
  }
  MODULEENTRY32W entry{};
  entry.dwSize = static_cast<DWORD>(sizeof(entry));
  BOOL has_entry = Module32FirstW(snapshot, &entry);
  if (has_entry == FALSE) {
    CloseHandle(snapshot);
    return true;
  }
  std::uint32_t entries{};
  bool found = false;
  bool enumeration_failed = false;
  try {
    while (has_entry != FALSE && entries < kMaximumSnapshotEntries) {
      ++entries;
      if (relevant_ngx_module_path(std::filesystem::path(entry.szExePath))) {
        found = true;
        break;
      }
      SetLastError(ERROR_SUCCESS);
      has_entry = Module32NextW(snapshot, &entry);
      if (has_entry == FALSE && GetLastError() != ERROR_NO_MORE_FILES) {
        enumeration_failed = true;
      }
    }
  } catch (...) {
    found = true;
  }
  CloseHandle(snapshot);
  return found || enumeration_failed || has_entry != FALSE;
}

bool install_resolver_hook(void *const target) {
  const MH_STATUS initialize = MH_Initialize();
  if (initialize != MH_OK) {
    write_log("trace.abort reason=minhook-initialize status=" +
              std::to_string(static_cast<int>(initialize)));
    return false;
  }
  void *trampoline{};
  const MH_STATUS create = MH_CreateHook(
      target, reinterpret_cast<void *>(&get_proc_address_for_caller_detour),
      &trampoline);
  if (create != MH_OK) {
    write_log("trace.abort reason=resolver-hook-create status=" +
              std::to_string(static_cast<int>(create)));
    MH_Uninitialize();
    return false;
  }
  const MH_STATUS queue = MH_QueueEnableHook(target);
  if (queue != MH_OK) {
    write_log("trace.abort reason=resolver-hook-queue status=" +
              std::to_string(static_cast<int>(queue)));
    MH_RemoveHook(target);
    MH_Uninitialize();
    return false;
  }
  if (ngx_module_already_loaded()) {
    write_log("trace.abort reason=ngx-module-present-before-resolver-enable");
    MH_RemoveHook(target);
    MH_Uninitialize();
    return false;
  }
  g_original_resolver.store(reinterpret_cast<std::uintptr_t>(trampoline),
                            std::memory_order_release);
  g_trace_enabled.store(true, std::memory_order_seq_cst);
  const MH_STATUS apply = MH_ApplyQueued();
  if (apply != MH_OK) {
    g_trace_enabled.store(false, std::memory_order_seq_cst);
    const MH_STATUS disable = MH_DisableHook(target);
    const MH_STATUS remove = MH_RemoveHook(target);
    const MH_STATUS uninitialize = MH_Uninitialize();
    write_log("trace.abort reason=resolver-hook-apply status=" +
              std::to_string(static_cast<int>(apply)) + " cleanup-disable=" +
              std::to_string(static_cast<int>(disable)) + " cleanup-remove=" +
              std::to_string(static_cast<int>(remove)) +
              " cleanup-uninitialize=" +
              std::to_string(static_cast<int>(uninitialize)));
    return false;
  }
  write_log("trace.resolver-ready target=KernelBase!GetProcAddressForCaller "
            "pre-enable-ngx-present=false lifetime=process mutations=0");
  return true;
}

std::string version_text(const std::array<std::uint16_t, 4> &version) {
  return std::to_string(version[0]) + "." + std::to_string(version[1]) + "." +
         std::to_string(version[2]) + "." + std::to_string(version[3]);
}

bool expected_slot_path(const Slot slot, const std::filesystem::path &path,
                        const std::filesystem::path &host_directory,
                        const std::filesystem::path &cache_path) {
  const std::wstring full = path.wstring();
  switch (slot) {
  case Slot::local_populate:
    return equal_path_text(
        full, (host_directory / L"nvngx_dlss.dll").wstring());
  case Slot::cache_populate:
    return !cache_path.empty() &&
           equal_path_text(full, cache_path.wstring());
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return equal_path_text(path.filename().wstring(), L"nvngx.dll") &&
           contains_ascii_case_insensitive(
               full, L"\\System32\\DriverStore\\FileRepository\\");
  case Slot::modern_capability:
  case Slot::modern_parameters:
    return equal_path_text(path.filename().wstring(), L"_nvngx.dll") &&
           contains_ascii_case_insensitive(
               full, L"\\System32\\DriverStore\\FileRepository\\");
  }
  return false;
}

bool verify_slot_identity(const Slot slot,
                          const std::filesystem::path &host_directory,
                          const std::filesystem::path &cache_path) {
  SlotRuntime &runtime = slot_runtime(slot);
  if (runtime.identity.load(std::memory_order_acquire) !=
      IdentityState::pending) {
    return true;
  }
  const auto target_value = runtime.target.load(std::memory_order_acquire);
  const auto module_value = runtime.module.load(std::memory_order_acquire);
  const auto target = reinterpret_cast<FARPROC>(target_value);
  const auto module = reinterpret_cast<HMODULE>(module_value);
  std::string rejection;
  std::filesystem::path path;
  std::array<unsigned char, 32> hash{};
  PeIdentity disk{};
  if (target == nullptr || module == nullptr) {
    rejection = "binding-incomplete";
  } else if (!mapped_slot_matches(slot, module, target)) {
    rejection = "mapped-identity-changed";
  } else {
    path = module_path(module);
    if (path.empty()) {
      rejection = "path-unavailable";
    } else if (!expected_slot_path(slot, path, host_directory, cache_path)) {
      rejection = "path-mismatch";
    } else if (!sha256_file(path, &hash)) {
      rejection = "hash-read-failed";
    } else if (hash != expected_hash(slot)) {
      rejection = "hash-mismatch";
    } else if (!read_disk_pe_identity(path, &disk) ||
               disk.machine != IMAGE_FILE_MACHINE_AMD64 ||
               disk.optional_magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
               disk.timestamp != expected_timestamp(slot) ||
               disk.image_base != kExpectedNvidiaPreferredImageBase ||
               disk.image_size != expected_image_size(slot)) {
      rejection = "disk-pe-mismatch";
    } else if (file_version(path) != expected_version(slot)) {
      rejection = "file-version-mismatch";
    }
  }

  if (rejection.empty()) {
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(target), &pinned) ||
        pinned != module) {
      rejection = "module-pin-failed";
    }
  }
  if (!rejection.empty()) {
    runtime.identity.store(IdentityState::rejected, std::memory_order_release);
    write_log("trace.identity slot=" + std::string(slot_name(slot)) +
              " state=rejected reason=" + rejection + " path=" +
              (path.empty() ? "unavailable" : utf8(path.wstring())) +
              " mutations=0");
    return false;
  }

  runtime.identity.store(IdentityState::verified, std::memory_order_release);
  write_log("trace.identity slot=" + std::string(slot_name(slot)) +
            " state=verified path=" + utf8(path.wstring()) + " sha256=" +
            sha256_text(hash) + " version=" +
            version_text(expected_version(slot)) + " target=" +
            hexadecimal(target_value) + " mutations=0");
  return true;
}

std::string address_origin(const std::uintptr_t address) {
  if (address == 0U) {
    return "unavailable";
  }
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(reinterpret_cast<const void *>(address), &information,
                   sizeof(information)) != sizeof(information) ||
      information.Type != MEM_IMAGE || information.AllocationBase == nullptr) {
    return "unknown@" + hexadecimal(address);
  }
  const auto module = static_cast<HMODULE>(information.AllocationBase);
  const std::filesystem::path path = module_path(module);
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  return (path.empty() ? "unknown" : utf8(path.filename().wstring())) + "+" +
         hexadecimal(address - base);
}

void drain_slot_events(const Slot slot) {
  SlotRuntime &runtime = slot_runtime(slot);
  for (TraceEvent &event : runtime.events) {
    if (event.state.load(std::memory_order_acquire) != EventState::ready) {
      continue;
    }
    const TraceEventPayload payload = event.payload;
    write_log("trace.call sequence=" + std::to_string(payload.sequence) +
              " slot=" + slot_name(slot) + " identity=" +
              identity_name(runtime.identity.load(std::memory_order_acquire)) +
              " hit-tick=" + std::to_string(payload.tick) + " hit-tid=" +
              std::to_string(payload.thread_id) + " caller=" +
              address_origin(payload.caller) + " caller-address=" +
              hexadecimal(payload.caller) + " argument-nonnull=" +
              (payload.argument_nonnull ? "true" : "false") +
              " argument=" + hexadecimal(payload.argument) + " result=" +
              hexadecimal(payload.result) + " duration-ms=" +
              std::to_string(payload.duration_ticks) + " mutated=false");
    event.state.store(EventState::logged, std::memory_order_release);
  }
}

struct SeenModule {
  std::uintptr_t base{};
  std::wstring path;
};

struct CensusState {
  std::vector<SeenModule> seen;
  bool failure_logged{};
  bool truncated_logged{};
};

bool census_seen(const CensusState &state, const std::uintptr_t base,
                 const std::wstring_view path) noexcept {
  for (const SeenModule &seen : state.seen) {
    if (seen.base == base && equal_path_text(seen.path, path)) {
      return true;
    }
  }
  return false;
}

void census_modules(CensusState &state) {
  const HANDLE snapshot = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (snapshot == INVALID_HANDLE_VALUE) {
    if (!state.failure_logged) {
      write_log("trace.census result=snapshot-failed error=" +
                std::to_string(GetLastError()));
      state.failure_logged = true;
    }
    return;
  }

  MODULEENTRY32W entry{};
  entry.dwSize = static_cast<DWORD>(sizeof(entry));
  BOOL has_entry = Module32FirstW(snapshot, &entry);
  std::uint32_t entries{};
  while (has_entry != FALSE && entries < kMaximumSnapshotEntries) {
    ++entries;
    const std::filesystem::path snapshot_path(entry.szExePath);
    if (relevant_ngx_module_path(snapshot_path)) {
      const auto base = reinterpret_cast<std::uintptr_t>(entry.hModule);
      const std::wstring full = snapshot_path.wstring();
      if (!census_seen(state, base, full) &&
          state.seen.size() < kMaximumCensusModules) {
        state.seen.push_back({base, full});
        PeIdentity mapped{};
        std::array<unsigned char, 32> hash{};
        const bool has_pe = read_mapped_pe_identity(entry.hModule, &mapped);
        const bool has_hash = sha256_file(snapshot_path, &hash);
        write_log("trace.module path=" + utf8(full) + " base=" +
                  hexadecimal(base) + " timestamp=" +
                  (has_pe ? hexadecimal(mapped.timestamp) : "unavailable") +
                  " size-of-image=" +
                  (has_pe ? hexadecimal(mapped.image_size) : "unavailable") +
                  " sha256=" +
                  (has_hash ? sha256_text(hash) : "unavailable") +
                  " version=" + version_text(file_version(snapshot_path)));
      } else if (state.seen.size() >= kMaximumCensusModules &&
                 !state.truncated_logged) {
        write_log("trace.census truncated=true reason=module-limit");
        state.truncated_logged = true;
      }
    }
    has_entry = Module32NextW(snapshot, &entry);
  }
  if (has_entry != FALSE && !state.truncated_logged) {
    write_log("trace.census truncated=true reason=snapshot-entry-limit");
    state.truncated_logged = true;
  }
  CloseHandle(snapshot);
}

bool prepare_host_boundary(std::filesystem::path *const host_path) {
  if (host_path == nullptr) {
    return false;
  }
  const HMODULE host = GetModuleHandleW(nullptr);
  const std::filesystem::path path = module_path(host);
  PeIdentity mapped{};
  if (host == nullptr || path.empty() ||
      !equal_path_text(path.filename().wstring(), L"MonsterHunterWorld.exe") ||
      !read_mapped_pe_identity(host, &mapped) ||
      mapped.machine != IMAGE_FILE_MACHINE_AMD64 ||
      mapped.optional_magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      mapped.timestamp != kExpectedHostTimestamp ||
      mapped.image_base != reinterpret_cast<std::uintptr_t>(host) ||
      mapped.image_size != kExpectedHostImageSize) {
    write_log("trace.abort reason=host-mapped-identity-mismatch path=" +
              (path.empty() ? "unavailable" : utf8(path.wstring())));
    return false;
  }

  g_host_base.store(reinterpret_cast<std::uintptr_t>(host),
                    std::memory_order_release);
  g_host_image_size.store(mapped.image_size, std::memory_order_release);
  *host_path = path;
  write_log("trace.host-boundary mapped=true path=" + utf8(path.wstring()) +
            " actual-base=" +
            hexadecimal(reinterpret_cast<std::uintptr_t>(host)) +
            " mapped-image-base=" + hexadecimal(mapped.image_base) +
            " timestamp=" + hexadecimal(mapped.timestamp) +
            " size-of-image=" + hexadecimal(mapped.image_size));
  return true;
}

bool verify_host_disk_identity(const std::filesystem::path &host_path) {
  PeIdentity disk{};
  std::array<unsigned char, 32> hash{};
  if (!read_disk_pe_identity(host_path, &disk) ||
      disk.machine != IMAGE_FILE_MACHINE_AMD64 ||
      disk.optional_magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      disk.timestamp != kExpectedHostTimestamp ||
      disk.image_base != kExpectedHostPreferredImageBase ||
      disk.image_size != kExpectedHostImageSize ||
      !sha256_file(host_path, &hash) || hash != kExpectedHostHash) {
    write_log("trace.abort reason=host-disk-identity-mismatch path=" +
              utf8(host_path.wstring()));
    return false;
  }
  write_log("trace.host-identity verified=true sha256=" + sha256_text(hash) +
            " mutations=0");
  return true;
}

void verify_pending_slot_identities(
    const std::filesystem::path &host_directory,
    const std::filesystem::path &cache_path) noexcept {
  for (const Slot slot : kAllSlots) {
    if (slot_runtime(slot).identity.load(std::memory_order_acquire) !=
        IdentityState::pending) {
      continue;
    }
    try {
      verify_slot_identity(slot, host_directory, cache_path);
    } catch (...) {
      slot_runtime(slot).identity.store(IdentityState::rejected,
                                        std::memory_order_release);
      write_log("trace.identity slot=" + std::string(slot_name(slot)) +
                " state=rejected reason=unexpected-verification-exception "
                "mutations=0");
    }
  }
}

void drain_all_slot_events() {
  for (const Slot slot : kAllSlots) {
    drain_slot_events(slot);
  }
}

void log_slot_summary(const Slot slot) {
  const SlotRuntime &runtime = g_slots[slot_index(slot)];
  const auto target = runtime.target.load(std::memory_order_acquire);
  const auto module = runtime.module.load(std::memory_order_acquire);
  const auto caller = runtime.resolver_caller.load(std::memory_order_acquire);
  write_log(
      "trace.summary slot=" + std::string(slot_name(slot)) + " identity=" +
      identity_name(runtime.identity.load(std::memory_order_acquire)) +
      " module=" + hexadecimal(module) + " target=" + hexadecimal(target) +
      " resolves=" +
      std::to_string(runtime.resolves.load(std::memory_order_relaxed)) +
      " first-resolve-tick=" +
      std::to_string(runtime.resolver_tick.load(std::memory_order_relaxed)) +
      " resolver-caller=" + address_origin(caller) + " calls=" +
      std::to_string(runtime.calls.load(std::memory_order_relaxed)) +
      " success=" +
      std::to_string(runtime.success.load(std::memory_order_relaxed)) +
      " bad00000=" +
      std::to_string(runtime.bad00000.load(std::memory_order_relaxed)) +
      " bad00002=" +
      std::to_string(runtime.bad00002.load(std::memory_order_relaxed)) +
      " other=" +
      std::to_string(runtime.other.load(std::memory_order_relaxed)) +
      " recursion=" +
      std::to_string(runtime.recursion.load(std::memory_order_relaxed)) +
      " conflicts=" +
      std::to_string(runtime.conflicts.load(std::memory_order_relaxed)) +
      " dropped-events=" +
      std::to_string(runtime.dropped.load(std::memory_order_relaxed)) +
      " mutated=false");
}

bool close_trace_phase() noexcept {
  g_trace_enabled.store(false, std::memory_order_seq_cst);
  const ULONGLONG start = GetTickCount64();
  while (g_active_resolver_detours.load(std::memory_order_seq_cst) != 0U ||
         g_active_wrapper_calls.load(std::memory_order_seq_cst) != 0U) {
    if (GetTickCount64() - start >= kQuiesceTimeoutMilliseconds) {
      return false;
    }
    Sleep(1U);
  }
  return true;
}

void run_bounded_trace(const std::filesystem::path &host_directory,
                       const std::filesystem::path &cache_path) {
  CensusState census{};
  census.seen.reserve(kMaximumCensusModules);
  const ULONGLONG start = GetTickCount64();
  ULONGLONG next_census = start;
  write_log("trace.window start=true duration-ms=" +
            std::to_string(kTraceDurationMilliseconds));

  for (;;) {
    verify_pending_slot_identities(host_directory, cache_path);
    drain_all_slot_events();
    const ULONGLONG now = GetTickCount64();
    if (now >= next_census) {
      try {
        census_modules(census);
      } catch (...) {
        if (!census.failure_logged) {
          write_log("trace.census result=unexpected-exception");
          census.failure_logged = true;
        }
      }
      next_census = now + kCensusIntervalMilliseconds;
    }
    const ULONGLONG elapsed = now - start;
    if (elapsed >= kTraceDurationMilliseconds) {
      break;
    }
    const ULONGLONG remaining = kTraceDurationMilliseconds - elapsed;
    Sleep(remaining < kTracePollMilliseconds
              ? static_cast<DWORD>(remaining)
              : kTracePollMilliseconds);
  }

  const bool quiesced = close_trace_phase();
  write_log("trace.window closed=true bindings-enabled=false quiesced=" +
            std::string(quiesced ? "true" : "false") +
            " hook-retained=true");
  verify_pending_slot_identities(host_directory, cache_path);
  drain_all_slot_events();
  try {
    census_modules(census);
  } catch (...) {
    if (!census.failure_logged) {
      write_log("trace.census result=unexpected-final-exception");
    }
  }
  for (const Slot slot : kAllSlots) {
    log_slot_summary(slot);
  }

  const std::uint64_t before_ready =
      g_resolver_before_ready.load(std::memory_order_relaxed);
  write_log(
      "trace.complete duration-ms=" +
      std::to_string(GetTickCount64() - start) + " census-modules=" +
      std::to_string(census.seen.size()) + " resolver-calls=" +
      std::to_string(g_resolver_calls.load(std::memory_order_relaxed)) +
      " resolver-nontarget-names=" +
      std::to_string(g_name_rejects.load(std::memory_order_relaxed)) +
      " target-null-results=" +
      std::to_string(g_target_nulls.load(std::memory_order_relaxed)) +
      " target-identity-or-caller-rejects=" +
      std::to_string(g_target_rejects.load(std::memory_order_relaxed)) +
      " resolver-calls-before-ready=" + std::to_string(before_ready) +
      " quiesced=" + (quiesced ? "true" : "false") +
      " coverage=" +
      (before_ready == 0U && quiesced
           ? "best-effort-complete-after-enable"
           : "incomplete-after-enable") +
      " startup-worker-gap-observable=false mutations=0");
}

DWORD WINAPI trace_worker(void *) noexcept {
  try {
    HMODULE pinned_self{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(
                                &get_proc_address_for_caller_detour),
                            &pinned_self) ||
        pinned_self != g_self) {
      g_trace_state.store(-1, std::memory_order_release);
      return 0U;
    }

    open_log();
    write_log(
        "trace.start version=0.4.0 mode=ngx-resolver-boundary-observer "
        "expected-loader=mo2-force-load mutations=0 attach-to-worker-ms=" +
        std::to_string(GetTickCount64() - g_attach_tick));

    std::filesystem::path host_path;
    if (!prepare_host_boundary(&host_path)) {
      g_trace_state.store(-1, std::memory_order_release);
      return 0U;
    }

    void *resolver{};
    if (!resolve_and_verify_system_target(&resolver) ||
        !install_resolver_hook(resolver)) {
      g_trace_state.store(-1, std::memory_order_release);
      return 0U;
    }
    g_trace_state.store(1, std::memory_order_release);

    if (!verify_host_disk_identity(host_path) ||
        !verify_nvidia_adapter_inventory()) {
      const bool quiesced = close_trace_phase();
      g_trace_state.store(-1, std::memory_order_release);
      write_log("trace.inert reason=environment-verification-failed "
                "future-resolutions-pass-through=true "
                "previously-issued-wrappers-transparent=true quiesced=" +
                std::string(quiesced ? "true" : "false") + " mutations=0");
      return 0U;
    }

    const std::filesystem::path cache_path = cache_feature_path();
    write_log("trace.active host-verified=true gpu-allowlist=RTX_5090 "
              "hook-lifetime=process mutations=0");
    run_bounded_trace(host_path.parent_path(), cache_path);
    g_trace_state.store(2, std::memory_order_release);
  } catch (...) {
    const bool quiesced = close_trace_phase();
    g_trace_state.store(-1, std::memory_order_release);
    write_log("trace.failure reason=unexpected-exception "
              "future-resolutions-pass-through=true "
              "previously-issued-wrappers-transparent=true quiesced=" +
              std::string(quiesced ? "true" : "false") + " mutations=0");
  }
  return 0U;
}

} // namespace

extern "C" __declspec(dllexport) int __cdecl MhwDlssUnlockGetState() noexcept {
  return g_trace_state.load(std::memory_order_acquire);
}

BOOL APIENTRY DllMain(const HMODULE module, const DWORD reason, LPVOID) {
  if (reason != DLL_PROCESS_ATTACH) {
    return TRUE;
  }
  g_self = module;
  g_attach_tick = GetTickCount64();
  DisableThreadLibraryCalls(module);
  const HANDLE thread = CreateThread(nullptr, 0, &trace_worker, nullptr, 0,
                                     nullptr);
  if (thread != nullptr) {
    CloseHandle(thread);
  } else {
    g_trace_state.store(-1, std::memory_order_release);
  }
  return TRUE;
}
