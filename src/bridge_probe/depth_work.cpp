#include "depth_work.hpp"
#include <limits>

namespace mhw::bridge_probe {
namespace {
struct Reader {
  ScalarReader read;
  void *context;
  unsigned count{};
  bool ok{true};
  template <class T>
  T get(std::uintptr_t at, std::uintptr_t offset = 0) noexcept {
    T v{};
    if (!ok)
      return v;
    if (!at || at > (std::numeric_limits<std::uintptr_t>::max)() - offset ||
        ++count > 160) {
      ok = false;
      return v;
    }
    ok = read &&
         read(context, at + offset, std::as_writable_bytes(std::span(&v, 1))) ==
             ScalarReadResult::success;
    return v;
  }
  std::uintptr_t ptr(std::uintptr_t at, std::uintptr_t off = 0) noexcept {
    return get<std::uintptr_t>(at, off);
  }
};
bool texture(Reader &r, std::uintptr_t base, DepthTextureIdentity &t,
             unsigned kind, unsigned flags, unsigned format) noexcept {
  const auto vt = r.ptr(t.owner);
  const auto actual_kind = r.get<unsigned>(t.owner, 88);
  t.backing = r.ptr(t.owner, 96);
  const auto actual_flags = r.get<unsigned>(t.owner, 112),
             dimensions = r.get<unsigned>(t.owner, 136);
  const auto actual_format = r.get<unsigned>(t.owner, 144),
             gpu_format = r.get<unsigned>(t.owner, 148);
  const auto secondary = r.ptr(t.owner, 240);
  if (!r.ok || vt != base + 0x351c870U || actual_kind != kind || !t.backing ||
      actual_flags != flags || dimensions != (2560U | (1440U << 16)) ||
      actual_format != format || gpu_format != 0xffffffffU || secondary)
    return false;
  t.native = r.ptr(t.backing, 32);
  t.tracker = r.ptr(t.backing, 112);
  const auto vt_after = r.ptr(t.owner), backing_after = r.ptr(t.owner, 96),
             secondary_after = r.ptr(t.owner, 240);
  const auto native_after = r.ptr(t.backing, 32),
             tracker_after = r.ptr(t.backing, 112);
  return r.ok && t.native && t.tracker && vt == vt_after &&
         backing_after == t.backing && !secondary_after &&
         native_after == t.native && tracker_after == t.tracker;
}
bool validate(Reader &r, std::uintptr_t base, DepthWorkIdentity &w,
              unsigned &checkpoint) noexcept {
  checkpoint = 10;
  if (!base ||
      base > (std::numeric_limits<std::uintptr_t>::max)() - 0x51c4480U ||
      !w.wrapper)
    return false;
  const auto renderer = r.ptr(base, 0x51c4480U);
  checkpoint = 20;
  w.view = r.ptr(renderer, 128080);
  w.container = r.ptr(w.view, 1576);
  const auto container_type = r.ptr(w.container);
  w.raw_depth = r.ptr(w.container, 144);
  const auto view_depth = r.ptr(w.container, 160);
  if (!r.ok || container_type != base + 0x35884e0U || !w.raw_depth ||
      view_depth != w.textures[3].owner || w.raw_depth == view_depth)
    return false;
  constexpr unsigned kinds[]{5, 0, 0, 5},
      flags[]{0x200002, 2, 0x600002, 0x600000}, formats[]{34, 5, 34, 16};
  for (unsigned i = 0; i < 4; ++i) {
    checkpoint = 30 + i;
    if (!texture(r, base, w.textures[i], kinds[i], flags[i], formats[i]))
      return false;
  }
  checkpoint = 40;
  DepthSelectionRequest request{
      base,       base + 0x229e89eU, 0, w.textures[3].owner,
      view_depth, w.raw_depth,       4, true};
  const auto selected = select_depth_backing(request, r.read, r.context);
  r.count += selected.reads;
  if (selected.action != DepthSelectionAction::primary ||
      selected.backing != w.textures[3].backing ||
      selected.native != w.textures[3].native ||
      selected.tracker != w.textures[3].tracker)
    return false;
  const auto raw_after = r.ptr(w.container, 144),
             depth_after = r.ptr(w.container, 160);
  checkpoint = 50;
  const auto container_after = r.ptr(w.view, 1576),
             view_after = r.ptr(renderer, 128080),
             renderer_after = r.ptr(base, 0x51c4480U);
  return r.ok && raw_after == w.raw_depth && depth_after == view_depth &&
         container_after == w.container && view_after == w.view &&
         renderer_after == renderer;
}
DepthWorkCapture payload_work(std::uintptr_t base, std::uintptr_t payload,
                              std::uintptr_t descriptor, ScalarReader read,
                              void *context) noexcept {
  Reader r{read, context};
  DepthWorkCapture c;
  if (descriptor) {
    const auto opcode = r.get<unsigned>(descriptor),
               subtype = r.get<unsigned>(descriptor, 16);
    const auto target = r.ptr(descriptor, 32);
    if (!r.ok || opcode != 23 || subtype != 3 || target != payload)
      return {{}, r.count, false, 1};
  }
  c.identity.wrapper = r.ptr(payload);
  for (unsigned i = 0; i < 4; ++i)
    c.identity.textures[i].owner = r.ptr(payload, 8 + 8 * i);
  c.valid = validate(r, base, c.identity, c.checkpoint);
  if (c.valid) {
    c.checkpoint = 60;
    c.valid = r.ptr(payload) == c.identity.wrapper;
    for (unsigned i = 0; i < 4; ++i)
      c.valid = (r.ptr(payload, 8 + 8 * i) == c.identity.textures[i].owner) &&
                c.valid;
    if (descriptor)
      c.valid = (r.ptr(descriptor, 32) == payload) && c.valid;
  }
  c.valid = c.valid && r.ok;
  if (c.valid)
    c.checkpoint = 100;
  c.reads = r.count;
  return c;
}
} // namespace
DepthWorkCapture capture_producer_depth_work(std::uintptr_t base,
                                             std::uintptr_t args,
                                             ScalarReader read,
                                             void *context) noexcept {
  Reader r{read, context};
  DepthWorkCapture c;
  const auto wrapper_slot = r.ptr(args), layer = r.ptr(args, 8);
  c.identity.wrapper = r.ptr(wrapper_slot);
  c.identity.textures[0].owner = r.ptr(layer, 336);
  c.identity.textures[1].owner = r.ptr(layer, 424);
  const auto sharpen = r.get<unsigned char>(layer, 391);
  const auto index_slot = r.ptr(args, sharpen ? 32 : 40),
             index = r.ptr(index_slot);
  if (!r.ok || sharpen > 1 || index > 2)
    return {{}, r.count, false, 2};
  c.identity.textures[2].owner = r.ptr(layer, 360 + 8 * index);
  const auto view_slot = r.ptr(args, 48), producer_view = r.ptr(view_slot),
             container = r.ptr(producer_view, 1576);
  c.identity.textures[3].owner = r.ptr(container, 160);
  c.valid = validate(r, base, c.identity, c.checkpoint);
  if (c.valid)
    c.checkpoint = 70;
  c.valid =
      c.valid && r.ptr(wrapper_slot) == c.identity.wrapper &&
      c.identity.view == producer_view && r.ptr(view_slot) == producer_view &&
      r.ptr(args, 8) == layer && r.ptr(index_slot) == index &&
      r.get<unsigned char>(layer, 391) == sharpen &&
      r.ptr(layer, 336) == c.identity.textures[0].owner &&
      r.ptr(layer, 424) == c.identity.textures[1].owner &&
      r.ptr(layer, 360 + 8 * index) == c.identity.textures[2].owner && r.ok;
  c.reads = r.count;
  if (c.valid)
    c.checkpoint = 100;
  return c;
}
DepthWorkCapture capture_prepared_depth_work(std::uintptr_t base,
                                             std::uintptr_t payload,
                                             std::uintptr_t descriptor,
                                             ScalarReader read,
                                             void *context) noexcept {
  if (!descriptor)
    return {};
  return payload_work(base, payload, descriptor, read, context);
}
DepthWorkCapture capture_consumer_depth_work(std::uintptr_t base,
                                             std::uintptr_t payload,
                                             ScalarReader read,
                                             void *context) noexcept {
  return payload_work(base, payload, 0, read, context);
}
bool depth_tracking_context_verified(std::uintptr_t context,
                                     std::uintptr_t descriptor,
                                     const DepthTextureIdentity &depth,
                                     ScalarReader read, void *opaque,
                                     unsigned *observed_state) noexcept {
  Reader r{read, opaque};
  const auto descriptor_slot = r.ptr(context), epoch_slot = r.ptr(context, 16);
  if (r.ptr(descriptor_slot) != descriptor)
    return false;
  const auto epoch = r.ptr(epoch_slot),
             tracker_epoch = r.ptr(depth.tracker, 16),
             active = r.ptr(depth.tracker, 8);
  const auto state = r.get<std::uint16_t>(depth.tracker, 26),
             active_state = r.get<std::uint16_t>(active, 34);
  if (observed_state)
    *observed_state = state;
  if (!r.ok || !active || epoch != tracker_epoch || state != active_state ||
      (state != 4 && state != 8 && state != 0x800))
    return false;
  return r.ptr(depth.owner, 96) == depth.backing &&
         r.ptr(depth.backing, 32) == depth.native &&
         r.ptr(depth.backing, 112) == depth.tracker &&
         r.ptr(depth.tracker, 8) == active &&
         r.ptr(depth.tracker, 16) == tracker_epoch &&
         r.get<std::uint16_t>(depth.tracker, 26) == state &&
         r.get<std::uint16_t>(active, 34) == active_state &&
         r.ptr(epoch_slot) == epoch && r.ptr(descriptor_slot) == descriptor &&
         r.ok;
}
bool DepthWorkGate::queue(const DepthWorkCapture &c) noexcept {
  if (phase_ != DepthWorkPhase::idle)
    return false;
  if (!c.valid || !c.identity.wrapper) {
    reject();
    return false;
  }
  work_ = c.identity;
  phase_ = DepthWorkPhase::queued;
  return true;
}
std::uintptr_t DepthWorkGate::prepare(unsigned ordinal,
                                      const DepthWorkCapture &c,
                                      std::uintptr_t payload,
                                      std::uintptr_t descriptor,
                                      std::uintptr_t context, unsigned thread,
                                      std::uintptr_t supplied, unsigned request,
                                      bool depth_context_valid) noexcept {
  constexpr unsigned resource[4]{0, 1, 3, 2};
  if (ordinal >= 4 ||
      (phase_ != DepthWorkPhase::queued &&
       phase_ != DepthWorkPhase::preparing) ||
      ordinal != next_ || !c.valid || c.identity != work_ || !payload ||
      !descriptor || !context || !thread) {
    reject();
    return 0;
  }
  if (!ordinal) {
    payload_ = payload;
    descriptor_ = descriptor;
    context_ = context;
    thread_ = thread;
  }
  if (payload != payload_ || descriptor != descriptor_ || context != context_ ||
      thread != thread_ || request != (ordinal == 3 ? 0x400U : 4U) ||
      (ordinal == 2 ? supplied != 0 || !depth_context_valid
                    : supplied != work_.textures[resource[ordinal]].backing)) {
    reject();
    return 0;
  }
  const auto backing = work_.textures[resource[ordinal]].backing;
  ++next_;
  phase_ = next_ == 4 ? DepthWorkPhase::prepared : DepthWorkPhase::preparing;
  return backing;
}
bool DepthWorkGate::consume(const DepthWorkCapture &c,
                            std::uintptr_t payload) noexcept {
  if (phase_ != DepthWorkPhase::prepared || !c.valid || c.identity != work_ ||
      payload != payload_) {
    reject();
    return false;
  }
  phase_ = DepthWorkPhase::consuming;
  return true;
}
void DepthWorkGate::finish() noexcept {
  if (phase_ != DepthWorkPhase::consuming) {
    reject();
    return;
  }
  work_ = {};
  payload_ = descriptor_ = context_ = 0;
  phase_ = DepthWorkPhase::complete;
}
void DepthWorkGate::reject() noexcept {
  work_ = {};
  payload_ = descriptor_ = context_ = 0;
  phase_ = DepthWorkPhase::rejected;
}
} // namespace mhw::bridge_probe
