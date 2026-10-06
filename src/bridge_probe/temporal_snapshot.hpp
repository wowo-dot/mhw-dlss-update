#pragma once

#include "renderer_snapshot.hpp"
#include <array>

namespace mhw::bridge_probe {

inline constexpr std::array<std::uintptr_t, 4> kTemporalCacheRvas{
    0x4BE60D0U, 0x4BE6110U, 0x4BE6174U, 0x4BE6178U};
inline constexpr std::uint32_t kTemporalSentinel = 0xFFFFFFFFU;
inline constexpr std::uint32_t kTemporalReadBudget = 9U;
inline constexpr std::uint32_t kTemporalBeforeCreateCapacity = 8U;
inline constexpr std::uint32_t kTemporalAfterCreateCapacity = 64U;

enum class TemporalStatus : std::uint8_t {
  complete,
  read_failed,
  pointers_changed,
  invalid_values
};
enum class TemporalField : std::uint8_t {
  none,
  execute_cache,
  dlss_branch_cache,
  dense_velocity_cache,
  dense_output_cache,
  manager,
  mode_56,
  mode_57,
  queue_count,
  manager_recheck
};
struct TemporalSnapshot {
  TemporalStatus status{TemporalStatus::read_failed};
  TemporalField failed_field{TemporalField::none};
  ScalarReadResult read_result{ScalarReadResult::success};
  std::uint32_t reads{};
  std::array<std::uint32_t, 4> cache_ids{kTemporalSentinel, kTemporalSentinel,
                                         kTemporalSentinel, kTemporalSentinel};
  bool manager_present{};
  bool pointer_stable{};
  std::uint8_t mode_56{};
  std::uint8_t mode_57{};
  std::uint32_t queue_count{};
};

// Hash/byte-gated caller only. Cached IDs are historical observations, not
// execution counters. An unchanged sentinel can also mean lookup failure.
TemporalSnapshot capture_temporal_snapshot(std::uintptr_t host,
                                           ScalarReader reader,
                                           void *context) noexcept;
std::uint32_t temporal_non_sentinel_mask(const TemporalSnapshot &) noexcept;
std::uint32_t
temporal_initialization_changes(const TemporalSnapshot &before,
                                const TemporalSnapshot &after) noexcept;
bool same_temporal_values(const TemporalSnapshot &,
                          const TemporalSnapshot &) noexcept;
std::string_view temporal_status_name(TemporalStatus) noexcept;
std::string_view temporal_field_name(TemporalField) noexcept;

} // namespace mhw::bridge_probe
