#include "unlock_policy.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <limits>
#include <span>

namespace {

int fail(const char *message) {
  std::cerr << message << '\n';
  return 1;
}

} // namespace

int main() {
  using mhw::unlock::CacheFeatureIdentity;
  using mhw::unlock::Classifier;
  using mhw::unlock::PeHeaderIdentity;
  using mhw::unlock::classifier_prologue_size;
  using mhw::unlock::is_allowlisted_geforce_rtx50_adapter;
  using mhw::unlock::is_allowlisted_geforce_rtx50_description;
  using mhw::unlock::is_expected_classifier_return_address;
  using mhw::unlock::is_expected_classifier_return_rva;
  using mhw::unlock::is_verified_adapter_inventory;
  using mhw::unlock::map_unknown_gpu_type;
  using mhw::unlock::matches_cache_populate_decision_sequence;
  using mhw::unlock::matches_classifier_prologue;
  using mhw::unlock::matches_expected_cache_disk_pe_identity;
  using mhw::unlock::matches_expected_cache_feature_identity;
  using mhw::unlock::matches_expected_cache_mapped_pe_identity;
  using mhw::unlock::msvc_wstring_description_is_allowlisted_geforce_rtx50;

  // Independent regression fixture copied from the externally verified digest.
  // Do not initialize this from kExpectedCacheFeatureHash: this comparison is
  // specifically intended to catch transcription errors in the policy constant.
  constexpr std::array<unsigned char, 32> observed_cache_sha256{
      0xDB, 0x20, 0x38, 0x81, 0xEA, 0x50, 0x28, 0xCB,
      0x58, 0x4A, 0xA6, 0xC5, 0x8D, 0x45, 0x73, 0x61,
      0xDD, 0x0E, 0x6A, 0x28, 0x17, 0x67, 0x98, 0x78,
      0x09, 0x8B, 0x08, 0xB9, 0x40, 0x54, 0x9A, 0x03};
  if (mhw::unlock::kExpectedCacheFeatureHash != observed_cache_sha256) {
    return fail("cache feature hash constant was mistranscribed");
  }

  // Independent raw-header fixture from the signed cache file. A disk image
  // retains its preferred base; a mapped image contains its ASLR load base.
  constexpr PeHeaderIdentity observed_disk_pe{
      0x8664U, 0x020BU, 0x64405395U, 0x180000000ULL, 0x00B57000U};
  if (!matches_expected_cache_disk_pe_identity(observed_disk_pe)) {
    return fail("exact on-disk cache PE identity was rejected");
  }
  PeHeaderIdentity changed_pe = observed_disk_pe;
  ++changed_pe.machine;
  if (matches_expected_cache_disk_pe_identity(changed_pe)) {
    return fail("disk PE identity accepted a wrong machine");
  }
  changed_pe = observed_disk_pe;
  ++changed_pe.optional_magic;
  if (matches_expected_cache_disk_pe_identity(changed_pe)) {
    return fail("disk PE identity accepted a wrong optional-header magic");
  }
  changed_pe = observed_disk_pe;
  ++changed_pe.timestamp;
  if (matches_expected_cache_disk_pe_identity(changed_pe)) {
    return fail("disk PE identity accepted a wrong timestamp");
  }
  changed_pe = observed_disk_pe;
  ++changed_pe.image_base;
  if (matches_expected_cache_disk_pe_identity(changed_pe)) {
    return fail("disk PE identity accepted a wrong preferred base");
  }
  changed_pe = observed_disk_pe;
  ++changed_pe.image_size;
  if (matches_expected_cache_disk_pe_identity(changed_pe)) {
    return fail("disk PE identity accepted a wrong image size");
  }

  constexpr std::uintptr_t observed_load_base = 0x00007FFE61FB0000ULL;
  PeHeaderIdentity observed_mapped_pe = observed_disk_pe;
  observed_mapped_pe.image_base = observed_load_base;
  if (!matches_expected_cache_mapped_pe_identity(observed_mapped_pe,
                                                 observed_load_base) ||
      matches_expected_cache_mapped_pe_identity(observed_mapped_pe, 0U) ||
      matches_expected_cache_mapped_pe_identity(
          observed_mapped_pe, observed_load_base + 0x10000U)) {
    return fail("ASLR mapped-PE identity policy is incorrect");
  }
  observed_mapped_pe.image_base =
      mhw::unlock::kExpectedCachePreferredImageBase;
  if (matches_expected_cache_mapped_pe_identity(observed_mapped_pe,
                                                observed_load_base)) {
    return fail("mapped PE incorrectly accepted the static preferred base");
  }

  CacheFeatureIdentity identity{};
  identity.sha256 = mhw::unlock::kExpectedCacheFeatureHash;
  identity.pe_timestamp = mhw::unlock::kExpectedCachePeTimestamp;
  identity.preferred_image_base =
      mhw::unlock::kExpectedCachePreferredImageBase;
  identity.image_size = mhw::unlock::kExpectedCacheImageSize;
  identity.file_version = mhw::unlock::kExpectedCacheFileVersion;
  identity.application_id = mhw::unlock::kExpectedCacheApplicationId;
  identity.gpu_architecture = mhw::unlock::kExpectedCacheGpuArchitecture;
  identity.snippet_version = mhw::unlock::kExpectedCacheSnippetVersion;
  identity.api_version = mhw::unlock::kExpectedCacheApiVersion;
  if (!matches_expected_cache_feature_identity(identity)) {
    return fail("exact cache feature identity was rejected");
  }

  CacheFeatureIdentity changed = identity;
  changed.sha256[0] ^= 0x01U;
  if (matches_expected_cache_feature_identity(changed)) {
    return fail("cache identity accepted a wrong hash");
  }
  changed = identity;
  ++changed.pe_timestamp;
  if (matches_expected_cache_feature_identity(changed)) {
    return fail("cache identity accepted a wrong PE timestamp");
  }
  changed = identity;
  ++changed.preferred_image_base;
  if (matches_expected_cache_feature_identity(changed)) {
    return fail("cache identity accepted a wrong preferred image base");
  }
  changed = identity;
  ++changed.image_size;
  if (matches_expected_cache_feature_identity(changed)) {
    return fail("cache identity accepted a wrong image size");
  }
  changed = identity;
  ++changed.file_version[2];
  if (matches_expected_cache_feature_identity(changed)) {
    return fail("cache identity accepted a wrong file version");
  }
  changed = identity;
  ++changed.application_id;
  if (matches_expected_cache_feature_identity(changed)) {
    return fail("cache identity accepted a wrong application ID");
  }
  changed = identity;
  ++changed.gpu_architecture;
  if (matches_expected_cache_feature_identity(changed)) {
    return fail("cache identity accepted a wrong GPU architecture");
  }
  changed = identity;
  ++changed.snippet_version;
  if (matches_expected_cache_feature_identity(changed)) {
    return fail("cache identity accepted a wrong snippet version");
  }
  changed = identity;
  ++changed.api_version;
  if (matches_expected_cache_feature_identity(changed)) {
    return fail("cache identity accepted a wrong API version");
  }

  if (!is_allowlisted_geforce_rtx50_description(
          L"NVIDIA GeForce RTX 5090") ||
      !is_allowlisted_geforce_rtx50_description(
          L"nvidia geforce rtx 5090")) {
    return fail("explicit RTX 5090 description was rejected");
  }
  constexpr std::array<std::wstring_view, 10> rejected_descriptions{
      L"Fake NVIDIA GeForce RTX 5090",
      L"NVIDIA GeForce RTX 5090 extra",
      L"NVIDIA GeForce RTX 50900",
      L"NVIDIA GeForce RTX 50",
      L"NVIDIA GeForce RTX 5080",
      L"NVIDIA GeForce RTX 5090 Laptop GPU",
      L"NVIDIA GeForce RTX 4090",
      L"NVIDIA Quadro RTX 5000",
      L"Graphics Device",
      L""};
  for (const std::wstring_view description : rejected_descriptions) {
    if (is_allowlisted_geforce_rtx50_description(description)) {
      return fail("non-allowlisted adapter description was accepted");
    }
  }

  if (!is_allowlisted_geforce_rtx50_adapter(
          mhw::unlock::kNvidiaVendorId, false,
          L"NVIDIA GeForce RTX 5090") ||
      is_allowlisted_geforce_rtx50_adapter(
          0x1002U, false, L"NVIDIA GeForce RTX 5090") ||
      is_allowlisted_geforce_rtx50_adapter(
          mhw::unlock::kNvidiaVendorId, true,
          L"NVIDIA GeForce RTX 5090") ||
      is_allowlisted_geforce_rtx50_adapter(
          mhw::unlock::kNvidiaVendorId, false,
          L"NVIDIA GeForce RTX 5080")) {
    return fail("adapter identity policy is incorrect");
  }

  if (!is_verified_adapter_inventory(true, 1U, 1U, 1U) ||
      is_verified_adapter_inventory(false, 1U, 1U, 1U) ||
      is_verified_adapter_inventory(true, 2U, 1U, 1U) ||
      is_verified_adapter_inventory(true, 1U, 0U, 1U) ||
      is_verified_adapter_inventory(true, 1U, 1U, 0U) ||
      is_verified_adapter_inventory(true, 1U, 2U, 1U) ||
      is_verified_adapter_inventory(true, 1U, 1U, 2U)) {
    return fail("strict adapter inventory policy is incorrect");
  }

  struct MsvcWideStringFixture {
    union {
      wchar_t inline_buffer[8];
      const wchar_t *heap_buffer;
    } storage{};
    std::size_t size{};
    std::size_t capacity{};
  };
  static_assert(sizeof(MsvcWideStringFixture) == 32U);

  constexpr wchar_t heap_name[] = L"NVIDIA GeForce RTX 5090";
  constexpr std::size_t heap_name_length = std::size(heap_name) - 1U;
  MsvcWideStringFixture heap_description{};
  heap_description.storage.heap_buffer = heap_name;
  heap_description.size = heap_name_length;
  heap_description.capacity = heap_name_length;
  if (!msvc_wstring_description_is_allowlisted_geforce_rtx50(
          &heap_description)) {
    return fail("valid heap-backed MSVC description was rejected");
  }

  constexpr wchar_t lowercase_heap_name[] = L"nvidia geforce rtx 5090";
  heap_description.storage.heap_buffer = lowercase_heap_name;
  if (!msvc_wstring_description_is_allowlisted_geforce_rtx50(
          &heap_description)) {
    return fail("case-normalized heap-backed MSVC description was rejected");
  }

  heap_description.storage.heap_buffer = heap_name;
  heap_description.size = 128U;
  heap_description.capacity = 128U;
  if (msvc_wstring_description_is_allowlisted_geforce_rtx50(
          &heap_description)) {
    return fail("oversized MSVC description was accepted");
  }
  heap_description.size = heap_name_length;
  heap_description.capacity = heap_name_length - 1U;
  if (msvc_wstring_description_is_allowlisted_geforce_rtx50(
          &heap_description)) {
    return fail("MSVC description with capacity below size was accepted");
  }
  heap_description.capacity = 128U;
  if (msvc_wstring_description_is_allowlisted_geforce_rtx50(
          &heap_description)) {
    return fail("MSVC description with excessive capacity was accepted");
  }

  std::array<wchar_t, std::size(heap_name)> missing_terminator{};
  std::copy_n(heap_name, heap_name_length, missing_terminator.begin());
  missing_terminator[heap_name_length] = L'X';
  heap_description.storage.heap_buffer = missing_terminator.data();
  heap_description.size = heap_name_length;
  heap_description.capacity = heap_name_length;
  if (msvc_wstring_description_is_allowlisted_geforce_rtx50(
          &heap_description)) {
    return fail("unterminated MSVC description was accepted");
  }

  MsvcWideStringFixture inline_description{};
  constexpr wchar_t short_name[] = L"RTX";
  std::wmemcpy(inline_description.storage.inline_buffer, short_name,
               std::size(short_name));
  inline_description.size = std::size(short_name) - 1U;
  inline_description.capacity = 7U;
  if (msvc_wstring_description_is_allowlisted_geforce_rtx50(
          &inline_description) ||
      msvc_wstring_description_is_allowlisted_geforce_rtx50(nullptr)) {
    return fail("invalid inline or null MSVC description was accepted");
  }

  if (map_unknown_gpu_type(0U, true, true) != 7U ||
      map_unknown_gpu_type(0U, false, true) != 0U ||
      map_unknown_gpu_type(0U, true, false) != 0U ||
      map_unknown_gpu_type(0U, false, false) != 0U) {
    return fail("unknown GPU result guard policy is incorrect");
  }
  constexpr std::array<std::uint32_t, 9> known_bucket_results{
      1U, 4U, 7U, 10U, 11U, 12U, 15U, 18U, 21U};
  for (const std::uint32_t result : known_bucket_results) {
    if (map_unknown_gpu_type(result, true, true) != result) {
      return fail("known bucket result was modified");
    }
  }

  constexpr std::array<unsigned char, 26> bucket_mapper{
      0xFF, 0xC9, 0x83, 0xF9, 0x08, 0x77, 0x49, 0x48, 0x8D,
      0x15, 0x22, 0xE9, 0xFC, 0xFF, 0x8B, 0x8C, 0x8A, 0x24,
      0x17, 0x03, 0x00, 0x48, 0x03, 0xCA, 0xFF, 0xE1};
  constexpr std::array<unsigned char, 25> description_parser{
      0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC,
      0x30, 0x33, 0xFF, 0x48, 0x8B, 0xD9, 0x48, 0x83, 0x79,
      0x18, 0x08, 0x72, 0x03, 0x48, 0x8B, 0x09};
  if (classifier_prologue_size(Classifier::cache_bucket_mapper) !=
          bucket_mapper.size() ||
      classifier_prologue_size(Classifier::cache_description_parser) !=
          description_parser.size() ||
      !matches_classifier_prologue(Classifier::cache_bucket_mapper,
                                   bucket_mapper) ||
      !matches_classifier_prologue(Classifier::cache_description_parser,
                                   description_parser) ||
      matches_classifier_prologue(Classifier::cache_bucket_mapper,
                                  description_parser) ||
      matches_classifier_prologue(Classifier::cache_description_parser,
                                  bucket_mapper)) {
    return fail("cache classifier prologue validation is incorrect");
  }
  if (matches_classifier_prologue(
          Classifier::cache_bucket_mapper,
          std::span<const unsigned char>(bucket_mapper.data(),
                                         bucket_mapper.size() - 1U)) ||
      matches_classifier_prologue(
          Classifier::cache_description_parser,
          std::span<const unsigned char>(description_parser.data(),
                                         description_parser.size() - 1U))) {
    return fail("short classifier prologue was accepted");
  }
  for (std::size_t index = 0U; index < bucket_mapper.size(); ++index) {
    auto corrupted = bucket_mapper;
    corrupted[index] ^= 0x01U;
    if (matches_classifier_prologue(Classifier::cache_bucket_mapper,
                                    corrupted)) {
      return fail("corrupted bucket-mapper prologue was accepted");
    }
  }
  for (std::size_t index = 0U; index < description_parser.size(); ++index) {
    auto corrupted = description_parser;
    corrupted[index] ^= 0x01U;
    if (matches_classifier_prologue(Classifier::cache_description_parser,
                                    corrupted)) {
      return fail("corrupted description-parser prologue was accepted");
    }
  }

  constexpr std::array<unsigned char,
                       mhw::unlock::kCachePopulateDecisionSequenceSize>
      populate_decision{
          0x48, 0x8D, 0x4C, 0x24, 0x30, 0xE8, 0xEC, 0x32, 0x01, 0x00,
          0x83, 0xF8, 0x01, 0x74, 0x07, 0xE8, 0x22, 0x43, 0x01, 0x00,
          0xEB, 0x09, 0x8B, 0x4C, 0x24, 0x30, 0xE8, 0xC7, 0x36, 0x01,
          0x00, 0x85, 0xC0, 0x0F, 0x84, 0xCC, 0x02, 0x00, 0x00};
  if (!matches_cache_populate_decision_sequence(populate_decision) ||
      matches_cache_populate_decision_sequence(
          std::span<const unsigned char>(populate_decision.data(),
                                         populate_decision.size() - 1U))) {
    return fail("PopulateParameters decision sequence length check failed");
  }
  for (std::size_t index = 0U; index < populate_decision.size(); ++index) {
    auto corrupted = populate_decision;
    corrupted[index] ^= 0x01U;
    if (matches_cache_populate_decision_sequence(corrupted)) {
      return fail("corrupted PopulateParameters decision sequence was accepted");
    }
  }

  if (!is_expected_classifier_return_rva(
          Classifier::cache_bucket_mapper,
          mhw::unlock::kCacheBucketMapperPopulateReturnRva) ||
      !is_expected_classifier_return_rva(
          Classifier::cache_bucket_mapper,
          mhw::unlock::kCacheBucketMapperOptimalReturnRva) ||
      is_expected_classifier_return_rva(
          Classifier::cache_bucket_mapper,
          mhw::unlock::kCacheBucketMapperPopulateReturnRva + 1U) ||
      is_expected_classifier_return_rva(
          Classifier::cache_bucket_mapper,
          mhw::unlock::kCacheBucketMapperOptimalReturnRva - 1U) ||
      !is_expected_classifier_return_rva(
          Classifier::cache_description_parser,
          mhw::unlock::kCacheDescriptionOptimalReturnRva) ||
      !is_expected_classifier_return_rva(
          Classifier::cache_description_parser,
          mhw::unlock::kCacheDescriptionLegacyReturnRva) ||
      is_expected_classifier_return_rva(
          Classifier::cache_description_parser,
          mhw::unlock::kCacheDescriptionLegacyReturnRva - 1U)) {
    return fail("classifier return-RVA policy is incorrect");
  }

  constexpr std::uintptr_t module_base = 0x00007FFE00000000ULL;
  if (!is_expected_classifier_return_address(
          Classifier::cache_bucket_mapper, module_base,
          mhw::unlock::kExpectedCacheImageSize,
          module_base + mhw::unlock::kCacheBucketMapperPopulateReturnRva) ||
      !is_expected_classifier_return_address(
          Classifier::cache_description_parser, module_base,
          mhw::unlock::kExpectedCacheImageSize,
          module_base + mhw::unlock::kCacheDescriptionOptimalReturnRva) ||
      !is_expected_classifier_return_address(
          Classifier::cache_bucket_mapper, module_base,
          mhw::unlock::kExpectedCacheImageSize,
          module_base + mhw::unlock::kCacheBucketMapperOptimalReturnRva) ||
      is_expected_classifier_return_address(
          Classifier::cache_bucket_mapper, module_base,
          mhw::unlock::kExpectedCacheImageSize, module_base - 1U) ||
      is_expected_classifier_return_address(
          Classifier::cache_bucket_mapper, module_base,
          mhw::unlock::kExpectedCacheImageSize,
          module_base + mhw::unlock::kExpectedCacheImageSize) ||
      is_expected_classifier_return_address(
          Classifier::cache_bucket_mapper, 0U,
          mhw::unlock::kExpectedCacheImageSize,
          mhw::unlock::kCacheBucketMapperPopulateReturnRva) ||
      is_expected_classifier_return_address(
          Classifier::cache_bucket_mapper, module_base, 0U,
          module_base + mhw::unlock::kCacheBucketMapperPopulateReturnRva) ||
      is_expected_classifier_return_address(
          Classifier::cache_bucket_mapper,
          std::numeric_limits<std::uintptr_t>::max() - 8U,
          mhw::unlock::kExpectedCacheImageSize, 4U)) {
    return fail("ASLR-safe classifier return-address policy is incorrect");
  }

  std::cout << "unlock policy tests passed\n";
  return 0;
}
