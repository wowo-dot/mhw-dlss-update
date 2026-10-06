#include "depth_retry.hpp"

#include <map>
#include <limits>
#include <string_view>

namespace mhw::exception_capture {
std::optional<Address> DepthRetry::take(const Snapshot &s, Address base,
                                       bool first_chance, bool contract_verified, RetryObservation observation) {
  reason_ = "correction-budget";
  if (attempts_ >= limit()) return {};
  reason_ = "work-item-or-contract";
  if (!first_chance || !contract_verified || !s.descriptor_verified ||
      s.read_failed || s.budget_exhausted) return {};
  std::map<std::string_view, Address> fields;
  for (const auto &f : s.fields) {
    if (!f.value || !fields.emplace(f.name, *f.value).second) return {};
  }
  auto get = [&](const std::string &key) -> std::optional<Address> {
    const auto it = fields.find(key);
    return it == fields.end() ? std::nullopt : std::optional<Address>(it->second);
  };
  auto nonzero = [&](const std::string &key) { const auto v = get(key); return v && *v; };
  auto same = [&](const std::string &a, const std::string &b) {
    const auto v = get(a); return v && v == get(b);
  };
  if (get("descriptor.opcode") != 23U || get("descriptor.subtype") != 3U ||
      !nonzero("descriptor.payload") || !nonzero("payload.wrapper") ||
      !same("descriptor.payload", "descriptor.payload_recheck") ||
      get("descriptor.stable") != 1U || !nonzero("renderer") || !nonzero("main_view") ||
      !nonzero("main_view.container") || get("main_view.stable") != 1U ||
      get("main_view.container_stable") != 1U ||
      !same("main_view", "main_view.recheck") ||
      !same("main_view.container", "main_view.container_recheck") ||
      get("container.vtable") != base + 0x35884E0U ||
      !same("payload.depth", "container.member_160") ||
      get("container.member_160.equals_payload_depth") != 1U ||
      get("container.member_144.equals_payload_depth") != 0U ||
      get("container.member_144") == get("payload.depth")) return {};

  struct Texture { const char *name; Address kind, format, flags; bool secondary; };
  constexpr Texture textures[] = {
    {"color", 5, 34, 0x200002, false}, {"motion", 0, 5, 2, false},
    {"output", 0, 34, 0x600002, false}, {"depth", 5, 16, 0x600000, false},
    {"container.member_160", 5, 16, 0x600000, false},
    {"container.member_144", 6, 17, 0x500000, true},
  };
  for (const auto &t : textures) {
    const std::string name(t.name);
    const std::string owner = name.starts_with("container.") ? name : "payload." + name;
    const auto backing = name + (t.secondary ? ".secondary" : ".primary");
    const auto empty = name + (t.secondary ? ".primary" : ".secondary");
    if (!nonzero(owner) || get(name + ".known_texture_class") != 1U ||
        get(name + ".vtable") != base + 0x351C870U ||
        !same(name + ".vtable", name + ".vtable_recheck") ||
        get(name + ".owner_stable") != 1U || get(name + ".kind") != t.kind ||
        get(name + ".format_id") != t.format || get(name + ".flags") != t.flags ||
        // The raw depth object's captured packed word also has bit 31 set.
        // Match that exact word; do not interpret the extra bit as height.
        get(name + ".packed_dimensions") != ((2560U | (1440U << 16U)) | (t.secondary ? 0x80000000U : 0U)) ||
        get(name + ".gpu_format_id") != 0xFFFFFFFFU ||
        !nonzero(backing) || !nonzero(backing + ".native") || !nonzero(backing + ".tracker") ||
        !same(backing, backing + ".recheck") || get(backing + ".stable") != 1U ||
        get(empty) != 0U || get(empty + ".recheck") != 0U || get(empty + ".stable") != 1U)
      return {};
    if (name.starts_with("container.") &&
        (!same(name, name + ".recheck") || get(name + ".slot_stable") != 1U)) return {};
  }
  for (const auto *suffix : {".primary", ".primary.native", ".primary.tracker"})
    if (!same(std::string("depth") + suffix, std::string("container.member_160") + suffix)) return {};
  if (mode_ == DepthRetryMode::three_frames) {
    reason_ = "tracker-branch";
    const auto *t = observation.tracker;
    if (!t || !t->complete || !t->same_epoch ||
        !t->data.descriptor_verified || t->data.read_failed || t->data.budget_exhausted) return {};
    std::map<std::string_view, Address> tracker;
    for (const auto &f : t->data.fields)
      if (!f.value || !tracker.emplace(f.name, *f.value).second) return {};
    auto tracked = [&](std::string_view key) -> std::optional<Address> {
      const auto it = tracker.find(key); return it == tracker.end() ? std::nullopt : std::optional<Address>(it->second);
    };
    // Exact observed same-epoch branches: 0x800 appends a state record (24944);
    // 0x4 only updates the active operation and returns (7148). These are engine
    // states, not D3D12_RESOURCE_STATES. Other read masks remain unadmitted.
    const auto active_state = tracked("active.state");
    if (!active_state || (*active_state != 0x800U && *active_state != 4U) ||
        t->compatible_read != (*active_state == 4U)) return {};
    if (tracked("anchors.match_work_item") != 1U || tracked("stable") != 1U ||
        tracked("complete") != 1U || tracked("same_epoch") != 1U ||
        tracked("recheck.active_state") != active_state || tracked("tracker.state") != active_state ||
        tracked("recheck.state") != active_state ||
        tracked("owner.primary") != get("depth.primary") ||
        tracked("backing.native") != get("depth.primary.native") ||
        tracked("backing.tracker") != get("depth.primary.tracker")) return {};
    reason_ = "frame-sequence";
    const auto frame = get("renderer.frame");
    if (!frame || (attempts_ && (frame_ == std::numeric_limits<Address>::max() || *frame != frame_ + 1))) return {};
    reason_ = "time-or-thread";
    if (!observation.tick || !observation.thread || (attempts_ &&
        (observation.thread != thread_ || observation.tick < last_tick_ ||
         observation.tick - start_ > window_ms))) return {};
    constexpr const char *keys[] = {"renderer", "main_view", "main_view.container", "payload.wrapper",
        "payload.depth", "depth.primary", "depth.primary.native", "depth.primary.tracker",
        "container.member_144", "container.member_144.secondary", "container.member_144.secondary.native",
        "container.member_144.secondary.tracker"};
    std::array<Address, std::size(keys)> identity{};
    for (std::size_t i = 0; i < identity.size(); ++i) {
      const auto value = get(keys[i]); if (!value || !*value) return {};
      identity[i] = *value;
    }
    reason_ = "resource-identity";
    if (attempts_ && identity != identity_) return {};
    if (!attempts_) { start_ = observation.tick; thread_ = observation.thread; identity_ = identity; }
    last_tick_ = observation.tick; frame_ = *frame;
  }
  ++attempts_;
  reason_ = "admitted";
  return get("depth.primary");
}
} // namespace mhw::exception_capture
