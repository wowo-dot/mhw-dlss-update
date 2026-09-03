#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace mhw::unlock {

inline constexpr std::uint32_t kNvidiaVendorId = 0x10DEU;
inline constexpr std::uint32_t kFallbackGpuTypeRtx2080 = 7U;

inline constexpr std::uintptr_t kCacheBucketMapperRva = 0x316D0U;
inline constexpr std::uintptr_t kCacheDescriptionParserRva = 0x31750U;
inline constexpr std::uintptr_t kCachePopulateDecisionSequenceRva = 0x1DFEAU;
inline constexpr std::size_t kCachePopulateDecisionSequenceSize = 39U;

inline constexpr std::uintptr_t kCacheBucketMapperPopulateReturnRva = 0x1E009U;
inline constexpr std::uintptr_t kCacheBucketMapperOptimalReturnRva = 0x22F0DU;
inline constexpr std::uintptr_t kCacheDescriptionOptimalReturnRva = 0x0F8E8U;
inline constexpr std::uintptr_t kCacheDescriptionLegacyReturnRva = 0x32457U;

inline constexpr std::array<unsigned char, 32> kExpectedCacheFeatureHash{
    0xDB, 0x20, 0x38, 0x81, 0xEA, 0x50, 0x28, 0xCB,
    0x58, 0x4A, 0xA6, 0xC5, 0x8D, 0x45, 0x73, 0x61,
    0xDD, 0x0E, 0x6A, 0x28, 0x17, 0x67, 0x98, 0x78,
    0x09, 0x8B, 0x08, 0xB9, 0x40, 0x54, 0x9A, 0x03};
inline constexpr std::uint32_t kExpectedCachePeTimestamp = 0x64405395U;
inline constexpr std::uint16_t kExpectedCacheMachine = 0x8664U;
inline constexpr std::uint16_t kExpectedCacheOptionalMagic = 0x020BU;
inline constexpr std::uint64_t kExpectedCachePreferredImageBase = 0x180000000ULL;
inline constexpr std::uint32_t kExpectedCacheImageSize = 0x00B57000U;
inline constexpr std::array<std::uint16_t, 4> kExpectedCacheFileVersion{
    1U, 2U, 110U, 0U};
inline constexpr std::uint32_t kExpectedCacheApplicationId = 0x0B9D2F5CU;
inline constexpr std::uint32_t kExpectedCacheGpuArchitecture = 0x160U;
inline constexpr std::uint32_t kExpectedCacheSnippetVersion = 0x1026EU;
inline constexpr std::uint32_t kExpectedCacheApiVersion = 0x12U;

enum class Classifier : std::uint8_t {
  cache_bucket_mapper,
  cache_description_parser,
};

struct PeHeaderIdentity {
  std::uint16_t machine{};
  std::uint16_t optional_magic{};
  std::uint32_t timestamp{};
  std::uint64_t image_base{};
  std::uint32_t image_size{};
};

struct CacheFeatureIdentity {
  std::array<unsigned char, 32> sha256{};
  std::uint32_t pe_timestamp{};
  std::uint64_t preferred_image_base{};
  std::uint32_t image_size{};
  std::array<std::uint16_t, 4> file_version{};
  std::uint32_t application_id{};
  std::uint32_t gpu_architecture{};
  std::uint32_t snippet_version{};
  std::uint32_t api_version{};
};

bool matches_expected_cache_feature_identity(
    const CacheFeatureIdentity &identity) noexcept;

bool matches_expected_cache_disk_pe_identity(
    const PeHeaderIdentity &identity) noexcept;

bool matches_expected_cache_mapped_pe_identity(
    const PeHeaderIdentity &identity,
    std::uintptr_t loaded_module_base) noexcept;

bool is_allowlisted_geforce_rtx50_description(
    std::wstring_view description) noexcept;

// Reads the exact 32-byte MSVC wide-string layout used by the verified cache
// build. The caller must ensure `object` and its selected data buffer are
// readable; the runtime boundary must wrap this helper in SEH.
bool msvc_wstring_description_is_allowlisted_geforce_rtx50(
    const void *object) noexcept;

bool is_allowlisted_geforce_rtx50_adapter(
    std::uint32_t vendor_id, bool software_or_remote,
    std::wstring_view description) noexcept;

// The bucket mapper has no adapter argument. Requiring exactly one hardware
// adapter makes the process-level identity unambiguous for this experiment.
bool is_verified_adapter_inventory(bool enumeration_complete,
                                   std::uint32_t hardware_adapter_count,
                                   std::uint32_t nvidia_hardware_count,
                                   std::uint32_t allowlisted_adapter_count) noexcept;

std::uint32_t map_unknown_gpu_type(std::uint32_t original_result,
                                   bool verified_geforce_rtx50,
                                   bool verified_callsite) noexcept;

std::size_t classifier_prologue_size(Classifier classifier) noexcept;

bool matches_classifier_prologue(Classifier classifier,
                                 std::span<const unsigned char> bytes) noexcept;

bool matches_cache_populate_decision_sequence(
    std::span<const unsigned char> bytes) noexcept;

bool is_expected_classifier_return_rva(Classifier classifier,
                                       std::uintptr_t return_rva) noexcept;

bool is_expected_classifier_return_address(
    Classifier classifier, std::uintptr_t module_base,
    std::uint32_t image_size, std::uintptr_t return_address) noexcept;

} // namespace mhw::unlock
