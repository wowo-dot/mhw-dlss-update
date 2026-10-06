#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace mhw::bridge_probe {

enum class ScalarReadResult : std::uint8_t { success, unreadable, fault };
using ScalarReader = ScalarReadResult (*)(void *, std::uintptr_t,
                                          std::span<std::byte>) noexcept;
// Windows runtime adapter: only 1..8 bytes, inside one readable committed
// region.
ScalarReadResult read_bounded_scalar(void *, std::uintptr_t,
                                     std::span<std::byte>) noexcept;

inline constexpr std::uint32_t kRendererSnapshotReadBudget = 24U;
inline constexpr std::uint32_t kRendererSnapshotCapacity = 64U;
inline constexpr std::uint64_t kRendererSnapshotIntervalMs = 2'000U;
inline constexpr std::uint64_t kRendererSnapshotInitialDelayMs = 250U;

enum class RendererReadField : std::uint8_t {
  none,
  singleton,
  backend,
  use_dlss,
  sizing_flags,
  optimal_scale,
  present_width,
  present_height,
  content_scale,
  frame_index,
  main_view,
  view_flags,
  backend_ready,
  ngx_context,
  singleton_recheck,
  backend_recheck,
  view_recheck
};
enum class RendererSnapshotStatus : std::uint8_t {
  renderer_absent,
  read_failed,
  invalid_values,
  pointers_changed,
  gates_blocked,
  gates_ready
};
enum RendererBlock : std::uint32_t {
  use_dlss_off = 1U,
  sizing_disallows_dlss = 2U,
  backend_absent = 4U,
  backend_not_ready = 8U,
  ngx_context_absent = 16U
};

// This is a delayed, asynchronous observation, not a coherent frame capture.
// No borrowed addresses escape the capture; presence and scalar values only.
struct RendererSnapshot {
  RendererSnapshotStatus status{RendererSnapshotStatus::renderer_absent};
  RendererReadField failed_field{RendererReadField::none};
  ScalarReadResult read_result{ScalarReadResult::success};
  std::uint32_t reads{};
  bool renderer_present{};
  bool backend_present{};
  bool ngx_context_present{};
  bool main_view_present{};
  bool pointers_stable{};
  std::uint8_t use_dlss{};
  std::uint8_t backend_ready{};
  std::uint32_t sizing_flags{};
  std::uint32_t present_width{};
  std::uint32_t present_height{};
  std::uint32_t main_view_flags{};
  std::uint64_t frame_index{};
  float optimal_scale{};
  float content_scale{};
  std::uint32_t block_mask{};
};

// Offsets are for the hash-pinned host in the renderer-gate research contract.
// Caller must verify that contract before supplying the singleton address.
RendererSnapshot capture_renderer_snapshot(std::uintptr_t singleton,
                                           ScalarReader reader,
                                           void *context) noexcept;
std::string_view renderer_status_name(RendererSnapshotStatus) noexcept;
std::string_view renderer_field_name(RendererReadField) noexcept;
// Exclude the advancing frame index from change-based event publication.
bool same_renderer_gate_values(const RendererSnapshot &,
                               const RendererSnapshot &) noexcept;

} // namespace mhw::bridge_probe
