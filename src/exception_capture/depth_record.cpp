#include "depth_record.hpp"

#include <limits>

namespace mhw::exception_capture {
namespace {
class Capture {
public:
  Capture(Read read, void *context, Address base)
      : read_(read), context_(context), base_(base) {}

  template <typename T = Address>
  std::optional<T> get(const std::string &name, Address owner, Address offset) {
    T value{};
    std::optional<T> result;
    if (!out.read_failed && !out.budget_exhausted) {
      if (out.reads >= 192U) {
        out.budget_exhausted = true;
      } else if (!owner || owner > std::numeric_limits<Address>::max() - offset) {
        out.read_failed = true;
      } else {
        ++out.reads;
        if (read_(context_, owner + offset,
                  std::as_writable_bytes(std::span(&value, 1)))) {
          result = value;
        } else {
          out.read_failed = true;
        }
      }
    }
    out.fields.push_back({name, result ? std::optional<Address>(*result)
                                      : std::nullopt});
    return result;
  }

  void flag(const std::string &name, bool value) {
    out.fields.push_back({name, value ? 1U : 0U});
  }

  void texture(const std::string &name, Address owner) {
    if (!owner || out.read_failed)
      return;
    auto vt = get(name + ".vtable", owner, 0);
    const bool known = vt && *vt == base_ + 0x351C870U;
    flag(name + ".known_texture_class", known);
    if (!known)
      return;
    get<std::uint32_t>(name + ".kind", owner, 88);
    get<std::uint32_t>(name + ".flags", owner, 112);
    get<std::uint32_t>(name + ".packed_dimensions", owner, 136);
    get<std::uint32_t>(name + ".format_id", owner, 144);
    get<std::uint32_t>(name + ".gpu_format_id", owner, 148);
    for (const auto offset : {96U, 240U}) {
      auto prefix = name + (offset == 96U ? ".primary" : ".secondary");
      auto backing = get(prefix, owner, offset);
      if (backing && *backing) {
        get(prefix + ".native", *backing, 32);
        get(prefix + ".tracker", *backing, 112);
      }
      const auto again = get(prefix + ".recheck", owner, offset);
      flag(prefix + ".stable", backing && again && backing == again);
    }
    const auto again = get(name + ".vtable_recheck", owner, 0);
    flag(name + ".owner_stable", vt && again && vt == again);
  }

  Snapshot out;

private:
  Read read_;
  void *context_;
  Address base_;
};
} // namespace

Snapshot capture_record(Read read, void *context, Address base, Address payload,
                        Address descriptor, bool include_main_view) {
  Capture c(read, context, base);
  c.out.fields.reserve(128);
  const auto opcode = c.get<std::uint32_t>("descriptor.opcode", descriptor, 0);
  const auto subtype = c.get<std::uint32_t>("descriptor.subtype", descriptor, 16);
  const auto target = c.get("descriptor.payload", descriptor, 32);
  c.out.descriptor_verified = opcode == 23U && subtype == 3U &&
                              target == payload && payload != 0U;
  if (!c.out.descriptor_verified)
    return c.out;

  c.get("payload.wrapper", payload, 0);
  Address depth{};
  for (const auto &[name, offset] :
       {std::pair{"color", 8U}, {"motion", 16U}, {"output", 24U}, {"depth", 32U}}) {
    const std::string key = std::string("payload.") + name;
    auto owner = c.get(key, payload, offset);
    if (owner && *owner) {
      c.texture(name, *owner);
      if (offset == 32U)
        depth = *owner;
    }
  }
  // Scalars retained as raw integer bits; no unverified jitter interpretation.
  for (const auto offset : {40U, 44U, 48U, 52U, 56U, 60U})
    c.get<std::uint32_t>("payload.scalar_" + std::to_string(offset), payload, offset);

  if (include_main_view && !c.out.read_failed) {
    auto renderer = c.get("renderer", base, 0x51C4480);
    if (renderer && *renderer) {
      c.get("renderer.frame", *renderer, 127504);
      auto view = c.get("main_view", *renderer, 128080);
      if (view && *view) {
        c.get<std::uint32_t>("main_view.flags", *view, 88);
        auto container = c.get("main_view.container", *view, 1576);
        if (container && *container) {
          auto vt = c.get("container.vtable", *container, 0);
          if (vt == base + 0x35884E0U) {
            for (const auto offset : {144U, 160U}) {
              auto key = std::string("container.member_") + std::to_string(offset);
              auto owner = c.get(key, *container, offset);
              if (owner && *owner) {
                c.flag(key + ".equals_payload_depth", depth && *owner == depth);
                c.texture(key, *owner);
              }
              auto again = c.get(key + ".recheck", *container, offset);
              c.flag(key + ".slot_stable", owner && again && owner == again);
            }
          }
        }
        auto again = c.get("main_view.container_recheck", *view, 1576);
        c.flag("main_view.container_stable", container && again && container == again);
      }
      auto again = c.get("main_view.recheck", *renderer, 128080);
      c.flag("main_view.stable", view && again && view == again);
    }
  }
  const auto again = c.get("descriptor.payload_recheck", descriptor, 32);
  c.flag("descriptor.stable", again && again == target);
  return c.out;
}
} // namespace mhw::exception_capture
