#include "depth_tracker.hpp"
#include <limits>
#include <set>

namespace mhw::exception_capture {
namespace {
std::optional<Address> field(const Snapshot &s, const char *key) {
  for (const auto &f : s.fields) if (f.name == key) return f.value;
  return {};
}
struct Capture {
  Read read;
  void *context;
  TrackerSnapshot out;
  template <typename T = Address> std::optional<T> get(const char *name, Address owner, Address offset) {
    T value{};
    std::optional<T> result;
    auto &s = out.data;
    if (!s.read_failed && !s.budget_exhausted) {
      if (s.reads == 64) s.budget_exhausted = true;
      else if (!owner || owner > std::numeric_limits<Address>::max() - offset) s.read_failed = true;
      else {
        ++s.reads;
        if (read(context, owner + offset, std::as_writable_bytes(std::span(&value, 1)))) result = value;
        else s.read_failed = true;
      }
    }
    s.fields.push_back({name, result ? std::optional<Address>(*result) : std::nullopt});
    return result;
  }
  void flag(const char *name, bool value) { out.data.fields.push_back({name, value ? 1U : 0U}); }
};
} // namespace
TrackerSnapshot capture_depth_tracker(Read read, void *context, const Snapshot &work, Address preparation) {
  Capture c{read, context, {}};
  c.out.data.fields.reserve(48);
  std::set<std::string> keys;
  for (const auto &f : work.fields) if (!keys.insert(f.name).second) return c.out;
  const auto owner = field(work, "payload.depth"), backing = field(work, "depth.primary");
  const auto tracker = field(work, "depth.primary.tracker"), native = field(work, "depth.primary.native");
  c.out.data.descriptor_verified = read && preparation && work.descriptor_verified && !work.read_failed &&
      !work.budget_exhausted && owner && *owner && backing && *backing && tracker && *tracker && native && *native;
  if (!c.out.data.descriptor_verified) return c.out;
  const auto live_backing = c.get("owner.primary", *owner, 96);
  const auto live_tracker = c.get("backing.tracker", *backing, 112);
  const auto live_native = c.get("backing.native", *backing, 32);
  const bool anchors = live_backing == backing && live_tracker == tracker && live_native == native;
  c.flag("anchors.match_work_item", anchors);
  if (!anchors) return c.out;
  const auto epoch_slot = c.get("context.epoch_slot", preparation, 16);
  const auto operation_slot = c.get("context.operation_slot", preparation, 8);
  const auto descriptor_slot = c.get("context.descriptor_slot", preparation, 0);
  const auto epoch = epoch_slot ? c.get("context.epoch", *epoch_slot, 0) : std::nullopt;
  if (operation_slot) c.get<std::uint32_t>("context.operation", *operation_slot, 0);
  if (descriptor_slot) c.get("context.descriptor", *descriptor_slot, 0);
  c.get("tracker.first_record", *tracker, 0);
  const auto active = c.get("tracker.active_record", *tracker, 8);
  const auto tracker_epoch = c.get("tracker.epoch", *tracker, 16);
  c.get<std::uint16_t>("tracker.before", *tracker, 24);
  const auto state = c.get<std::uint16_t>("tracker.state", *tracker, 26);
  c.get<std::uint32_t>("tracker.read_operation", *tracker, 28);
  c.get("tracker.read_epoch", *tracker, 32);
  c.get<std::uint8_t>("tracker.flag", *tracker, 40);
  c.out.same_epoch = epoch && tracker_epoch && epoch == tracker_epoch;
  std::optional<std::uint16_t> active_state;
  if (c.out.same_epoch && active && *active) {
    c.get("active.descriptor", *active, 0);
    c.get<std::uint32_t>("active.operation", *active, 8);
    c.get("active.next", *active, 16);
    c.get("active.previous", *active, 24);
    c.get<std::uint16_t>("active.before", *active, 32);
    active_state = c.get<std::uint16_t>("active.state", *active, 34);
    c.get<std::uint8_t>("active.flag", *active, 36);
  }
  const auto current = c.out.same_epoch ? active_state : state;
  c.out.compatible_read = current && ((*current & 4U) != 0U);
  const bool stable = c.get("recheck.backing", *owner, 96) == backing &&
      c.get("recheck.tracker", *backing, 112) == tracker &&
      c.get("recheck.native", *backing, 32) == native &&
      c.get("recheck.active", *tracker, 8) == active &&
      c.get("recheck.epoch", *tracker, 16) == tracker_epoch &&
      c.get<std::uint16_t>("recheck.state", *tracker, 26) == state &&
      epoch_slot && c.get("recheck.context_epoch", *epoch_slot, 0) == epoch;
  bool active_stable = !c.out.same_epoch;
  if (c.out.same_epoch && active && *active)
    active_stable = c.get<std::uint16_t>("recheck.active_state", *active, 34) == active_state;
  c.flag("stable", stable && active_stable);
  c.flag("same_epoch", c.out.same_epoch);
  c.out.complete = stable && active_stable && current && !c.out.data.read_failed && !c.out.data.budget_exhausted;
  c.flag("complete", c.out.complete);
  c.flag("compatible_read", c.out.complete && c.out.compatible_read);
  return c.out;
}
} // namespace mhw::exception_capture
