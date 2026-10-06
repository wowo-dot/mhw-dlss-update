#include "depth_selection.hpp"
#include <limits>

namespace mhw::bridge_probe {
DepthSelectionResult select_depth_backing(const DepthSelectionRequest &r,
                                          ScalarReader read, void *context) noexcept {
  DepthSelectionResult out;
  if (!r.host_contract_verified || !r.host_base ||
      r.host_base > std::numeric_limits<std::uintptr_t>::max() - 0x351c870U)
    return out;
  if (r.return_address != r.host_base + 0x229e89eU) {
    out.action = DepthSelectionAction::unchanged;
    out.reason = DepthSelectionReason::other_call;
    out.backing = r.supplied_backing;
    return out;
  }
  // Never replace an existing secondary resource with a different object.
  if (r.supplied_backing) {
    out.action = DepthSelectionAction::unchanged;
    out.reason = DepthSelectionReason::secondary_present;
    out.backing = r.supplied_backing;
    return out;
  }
  if (!read || r.requested_state != 4) return out;
  if (!r.owner || r.owner != r.view_depth_owner || !r.raw_depth_owner || r.owner == r.raw_depth_owner) {
    out.reason = DepthSelectionReason::wrong_owner;
    return out;
  }
  bool failed{};
  const auto get = [&]<typename T>(std::uintptr_t owner, std::uintptr_t offset, T &value) {
    if (failed) return;
    if (!owner || owner > std::numeric_limits<std::uintptr_t>::max() - offset) { failed = true; return; }
    ++out.reads;
    failed = read(context, owner + offset, std::as_writable_bytes(std::span(&value, 1))) != ScalarReadResult::success;
  };
  std::uintptr_t type{}, primary{}, secondary{}, type_after{}, primary_after{}, secondary_after{};
  std::uintptr_t native_after{}, tracker_after{};
  std::uint32_t kind{}, flags{}, dimensions{}, format{}, gpu_format{};
  get(r.owner, 0, type); get(r.owner, 88, kind); get(r.owner, 96, primary);
  get(r.owner, 112, flags); get(r.owner, 136, dimensions);
  get(r.owner, 144, format); get(r.owner, 148, gpu_format); get(r.owner, 240, secondary);
  if (failed) { out.reason = DepthSelectionReason::unreadable; return out; }
  if (type != r.host_base + 0x351c870U || kind != 5 || flags != 0x600000U ||
      dimensions != (2560U | (1440U << 16U)) || format != 16 || gpu_format != 0xffffffffU || secondary) {
    out.reason = DepthSelectionReason::wrong_layout; return out;
  }
  if (!primary) { out.reason = DepthSelectionReason::missing_primary; return out; }
  get(primary, 32, out.native); get(primary, 112, out.tracker);
  get(r.owner, 0, type_after); get(r.owner, 96, primary_after); get(r.owner, 240, secondary_after);
  get(primary, 32, native_after); get(primary, 112, tracker_after);
  if (failed) { out.reason = DepthSelectionReason::unreadable; return out; }
  if (!out.native || !out.tracker) { out.reason = DepthSelectionReason::missing_primary; return out; }
  if (type_after != type || primary_after != primary || secondary_after != secondary ||
      native_after != out.native || tracker_after != out.tracker) {
    out.reason = DepthSelectionReason::changed; return out;
  }
  out.action = DepthSelectionAction::primary;
  out.reason = DepthSelectionReason::selected;
  out.backing = primary;
  return out;
}
} // namespace mhw::bridge_probe
