#include "unlock_policy.hpp"

#include <algorithm>

namespace mhw::unlock {
namespace {

constexpr std::array<unsigned char, 26> kCacheBucketMapperPrologue{
    0xFF, 0xC9, 0x83, 0xF9, 0x08, 0x77, 0x49, 0x48, 0x8D,
    0x15, 0x22, 0xE9, 0xFC, 0xFF, 0x8B, 0x8C, 0x8A, 0x24,
    0x17, 0x03, 0x00, 0x48, 0x03, 0xCA, 0xFF, 0xE1};

constexpr std::array<unsigned char, 25> kCacheDescriptionParserPrologue{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC,
    0x30, 0x33, 0xFF, 0x48, 0x8B, 0xD9, 0x48, 0x83, 0x79,
    0x18, 0x08, 0x72, 0x03, 0x48, 0x8B, 0x09};

constexpr std::array<unsigned char, kCachePopulateDecisionSequenceSize>
    kCachePopulateDecisionSequence{
        0x48, 0x8D, 0x4C, 0x24, 0x30, 0xE8, 0xEC, 0x32, 0x01, 0x00,
        0x83, 0xF8, 0x01, 0x74, 0x07, 0xE8, 0x22, 0x43, 0x01, 0x00,
        0xEB, 0x09, 0x8B, 0x4C, 0x24, 0x30, 0xE8, 0xC7, 0x36, 0x01,
        0x00, 0x85, 0xC0, 0x0F, 0x84, 0xCC, 0x02, 0x00, 0x00};

constexpr std::wstring_view kAllowlistedRtx5090 =
    L"NVIDIA GeForce RTX 5090";
constexpr std::size_t kMaximumAdapterDescriptionLength = 127U;

wchar_t ascii_upper(const wchar_t value) noexcept {
  if (value >= L'a' && value <= L'z') {
    return static_cast<wchar_t>(value - (L'a' - L'A'));
  }
  return value;
}

bool ascii_case_insensitive_equal(const std::wstring_view left,
                                  const std::wstring_view right) noexcept {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t index = 0; index < left.size(); ++index) {
    if (ascii_upper(left[index]) != ascii_upper(right[index])) {
      return false;
    }
  }
  return true;
}

std::span<const unsigned char>
expected_classifier_prologue(const Classifier classifier) noexcept {
  switch (classifier) {
  case Classifier::cache_bucket_mapper:
    return kCacheBucketMapperPrologue;
  case Classifier::cache_description_parser:
    return kCacheDescriptionParserPrologue;
  }
  return {};
}

} // namespace

bool matches_expected_cache_feature_identity(
    const CacheFeatureIdentity &identity) noexcept {
  return identity.sha256 == kExpectedCacheFeatureHash &&
         identity.pe_timestamp == kExpectedCachePeTimestamp &&
         identity.preferred_image_base == kExpectedCachePreferredImageBase &&
         identity.image_size == kExpectedCacheImageSize &&
         identity.file_version == kExpectedCacheFileVersion &&
         identity.application_id == kExpectedCacheApplicationId &&
         identity.gpu_architecture == kExpectedCacheGpuArchitecture &&
         identity.snippet_version == kExpectedCacheSnippetVersion &&
         identity.api_version == kExpectedCacheApiVersion;
}

bool matches_expected_cache_disk_pe_identity(
    const PeHeaderIdentity &identity) noexcept {
  return identity.machine == kExpectedCacheMachine &&
         identity.optional_magic == kExpectedCacheOptionalMagic &&
         identity.timestamp == kExpectedCachePeTimestamp &&
         identity.image_base == kExpectedCachePreferredImageBase &&
         identity.image_size == kExpectedCacheImageSize;
}

bool matches_expected_cache_mapped_pe_identity(
    const PeHeaderIdentity &identity,
    const std::uintptr_t loaded_module_base) noexcept {
  return loaded_module_base != 0U &&
         identity.machine == kExpectedCacheMachine &&
         identity.optional_magic == kExpectedCacheOptionalMagic &&
         identity.timestamp == kExpectedCachePeTimestamp &&
         identity.image_base == loaded_module_base &&
         identity.image_size == kExpectedCacheImageSize;
}

bool is_allowlisted_geforce_rtx50_description(
    const std::wstring_view description) noexcept {
  return ascii_case_insensitive_equal(description, kAllowlistedRtx5090);
}

bool msvc_wstring_description_is_allowlisted_geforce_rtx50(
    const void *const object) noexcept {
  if (object == nullptr) {
    return false;
  }

  const auto *bytes = static_cast<const unsigned char *>(object);
  const std::size_t size =
      *reinterpret_cast<const std::size_t *>(bytes + 0x10U);
  const std::size_t capacity =
      *reinterpret_cast<const std::size_t *>(bytes + 0x18U);
  if (size == 0U || size > kMaximumAdapterDescriptionLength ||
      capacity < size || capacity > kMaximumAdapterDescriptionLength) {
    return false;
  }

  const wchar_t *data{};
  if (capacity < 8U) {
    data = reinterpret_cast<const wchar_t *>(bytes);
  } else {
    data = *reinterpret_cast<const wchar_t *const *>(bytes);
  }
  return data != nullptr && data[size] == L'\0' &&
         is_allowlisted_geforce_rtx50_description(
             std::wstring_view(data, size));
}

bool is_allowlisted_geforce_rtx50_adapter(
    const std::uint32_t vendor_id, const bool software_or_remote,
    const std::wstring_view description) noexcept {
  return vendor_id == kNvidiaVendorId && !software_or_remote &&
         is_allowlisted_geforce_rtx50_description(description);
}

bool is_verified_adapter_inventory(
    const bool enumeration_complete,
    const std::uint32_t hardware_adapter_count,
    const std::uint32_t nvidia_hardware_count,
    const std::uint32_t allowlisted_adapter_count) noexcept {
  return enumeration_complete && hardware_adapter_count == 1U &&
         nvidia_hardware_count == 1U && allowlisted_adapter_count == 1U;
}

std::uint32_t map_unknown_gpu_type(
    const std::uint32_t original_result,
    const bool verified_geforce_rtx50,
    const bool verified_callsite) noexcept {
  if (original_result != 0U || !verified_geforce_rtx50 ||
      !verified_callsite) {
    return original_result;
  }
  return kFallbackGpuTypeRtx2080;
}

std::size_t classifier_prologue_size(const Classifier classifier) noexcept {
  return expected_classifier_prologue(classifier).size();
}

bool matches_classifier_prologue(
    const Classifier classifier,
    const std::span<const unsigned char> bytes) noexcept {
  const std::span<const unsigned char> expected =
      expected_classifier_prologue(classifier);
  return !expected.empty() && bytes.size() >= expected.size() &&
         std::equal(expected.begin(), expected.end(), bytes.begin());
}

bool matches_cache_populate_decision_sequence(
    const std::span<const unsigned char> bytes) noexcept {
  return bytes.size() >= kCachePopulateDecisionSequence.size() &&
         std::equal(kCachePopulateDecisionSequence.begin(),
                    kCachePopulateDecisionSequence.end(), bytes.begin());
}

bool is_expected_classifier_return_rva(
    const Classifier classifier, const std::uintptr_t return_rva) noexcept {
  switch (classifier) {
  case Classifier::cache_bucket_mapper:
    return return_rva == kCacheBucketMapperPopulateReturnRva ||
           return_rva == kCacheBucketMapperOptimalReturnRva;
  case Classifier::cache_description_parser:
    return return_rva == kCacheDescriptionOptimalReturnRva ||
           return_rva == kCacheDescriptionLegacyReturnRva;
  }
  return false;
}

bool is_expected_classifier_return_address(
    const Classifier classifier, const std::uintptr_t module_base,
    const std::uint32_t image_size,
    const std::uintptr_t return_address) noexcept {
  if (module_base == 0U || image_size == 0U ||
      return_address < module_base) {
    return false;
  }
  const std::uintptr_t return_rva = return_address - module_base;
  return return_rva < image_size &&
         is_expected_classifier_return_rva(classifier, return_rva);
}

} // namespace mhw::unlock
