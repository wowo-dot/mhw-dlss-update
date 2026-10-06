#pragma once
#include "depth_work.hpp"

namespace mhw::bridge_probe {
struct ProjectionAnchor {
  std::uintptr_t renderer{}, context{}, view{}, table{};
  std::uint64_t frame{}, epoch{};
  unsigned bank{}, async_mode{};
};
enum class ProjectionReject : unsigned {
  none, anchor, read, source_changed, camera_type, camera_epoch,
  buffer_unavailable, layout, changed_or_nonfinite
};
struct ProjectionSnapshot {
  bool attempted{}, valid{}, matrix_relation{};
  ProjectionReject rejection{};
  unsigned reads{}, checkpoint{}, epoch{}, buffer_id{}, projection_id{}, unjittered_id{}, words{};
  std::uintptr_t manager{}, metadata_table{}, metadata{}, tagged_constants{}, constants{};
  std::uintptr_t vtable{}, vtable_rva{};
  std::array<std::uint32_t, 2> jitter_bits{}, table20_bits{}, viewport_bits{};
  std::array<std::uint32_t, 16> projection_bits{}, unjittered_bits{};
};
// Observes the CPU projection buffer only. It neither qualifies GPU execution
// nor grants admission to the rendering experiment.
ProjectionSnapshot capture_projection_snapshot(std::uintptr_t base, const ProjectionAnchor &,
    ScalarReader, void *) noexcept;
bool projection_relation_matches(const ProjectionSnapshot &) noexcept;
} // namespace mhw::bridge_probe
