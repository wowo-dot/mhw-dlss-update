#include "temporal_snapshot.hpp"
#include <limits>

namespace mhw::bridge_probe {
namespace {
struct Reader {
  TemporalSnapshot &s;
  ScalarReader read;
  void *context;
  template <typename T>
  bool get(std::uintptr_t base, std::uintptr_t offset, T &value,
           TemporalField field) noexcept {
    if (!read || !base || s.reads >= kTemporalReadBudget ||
        base > std::numeric_limits<std::uintptr_t>::max() - offset) {
      s.read_result = ScalarReadResult::unreadable;
    } else {
      ++s.reads;
      s.read_result = read(context, base + offset,
                           std::as_writable_bytes(std::span<T>(&value, 1U)));
    }
    if (s.read_result == ScalarReadResult::success)
      return true;
    s.status = TemporalStatus::read_failed;
    s.failed_field = field;
    return false;
  }
};
} // namespace

TemporalSnapshot capture_temporal_snapshot(std::uintptr_t host,
                                           ScalarReader reader,
                                           void *context) noexcept {
  TemporalSnapshot s{};
  Reader read{s, reader, context};
  constexpr std::array fields{
      TemporalField::execute_cache, TemporalField::dlss_branch_cache,
      TemporalField::dense_velocity_cache, TemporalField::dense_output_cache};
  for (std::size_t i = 0; i < kTemporalCacheRvas.size(); ++i)
    if (!read.get(host, kTemporalCacheRvas[i], s.cache_ids[i], fields[i]))
      return s;
  std::uintptr_t manager{}, manager_after{};
  if (!read.get(host, 0x51CC238U, manager, TemporalField::manager))
    return s;
  s.manager_present = manager != 0U;
  if (manager) {
    if (!read.get(manager, 56U, s.mode_56, TemporalField::mode_56) ||
        !read.get(manager, 57U, s.mode_57, TemporalField::mode_57) ||
        !read.get(manager, 60U, s.queue_count, TemporalField::queue_count))
      return s;
  }
  if (!read.get(host, 0x51CC238U, manager_after,
                TemporalField::manager_recheck))
    return s;
  s.pointer_stable = manager_after == manager;
  if (!s.pointer_stable)
    s.status = TemporalStatus::pointers_changed;
  else if (s.mode_56 > 1U || s.mode_57 > 1U)
    s.status = TemporalStatus::invalid_values;
  else
    s.status = TemporalStatus::complete;
  return s;
}

std::uint32_t temporal_non_sentinel_mask(const TemporalSnapshot &s) noexcept {
  if (s.status != TemporalStatus::complete)
    return 0U;
  std::uint32_t mask{};
  for (std::size_t i = 0; i < s.cache_ids.size(); ++i)
    if (s.cache_ids[i] != kTemporalSentinel)
      mask |= 1U << i;
  return mask;
}
std::uint32_t
temporal_initialization_changes(const TemporalSnapshot &before,
                                const TemporalSnapshot &after) noexcept {
  if (before.status != TemporalStatus::complete ||
      after.status != TemporalStatus::complete)
    return 0U;
  return temporal_non_sentinel_mask(after) &
         ~temporal_non_sentinel_mask(before);
}
bool same_temporal_values(const TemporalSnapshot &a,
                          const TemporalSnapshot &b) noexcept {
  // The manager's queue count is volatile and includes other render work.
  return a.status == b.status && a.failed_field == b.failed_field &&
         a.read_result == b.read_result && a.cache_ids == b.cache_ids &&
         a.manager_present == b.manager_present &&
         a.pointer_stable == b.pointer_stable && a.mode_56 == b.mode_56 &&
         a.mode_57 == b.mode_57;
}
std::string_view temporal_status_name(TemporalStatus s) noexcept {
  switch (s) {
  case TemporalStatus::complete:
    return "complete";
  case TemporalStatus::read_failed:
    return "read-failed";
  case TemporalStatus::pointers_changed:
    return "pointers-changed";
  case TemporalStatus::invalid_values:
    return "invalid-values";
  }
  return "unknown";
}
std::string_view temporal_field_name(TemporalField field) noexcept {
  switch (field) {
  case TemporalField::none:
    return "none";
  case TemporalField::execute_cache:
    return "execute-cache";
  case TemporalField::dlss_branch_cache:
    return "dlss-branch-cache";
  case TemporalField::dense_velocity_cache:
    return "dense-velocity-cache";
  case TemporalField::dense_output_cache:
    return "dense-output-cache";
  case TemporalField::manager:
    return "manager";
  case TemporalField::mode_56:
    return "mode-56";
  case TemporalField::mode_57:
    return "mode-57";
  case TemporalField::queue_count:
    return "queue-count";
  case TemporalField::manager_recheck:
    return "manager-recheck";
  }
  return "unknown";
}
} // namespace mhw::bridge_probe
