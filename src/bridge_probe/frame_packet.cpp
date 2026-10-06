#include "frame_packet.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace mhw::bridge_probe {
namespace {
struct Reader {
  ScalarReader read; void *opaque;
  unsigned count{}; bool ok{true};
  template<class T> T get(std::uintptr_t at, std::uintptr_t offset = 0) noexcept {
    T value{};
    if (!ok) return value;
    if (!at || at > (std::numeric_limits<std::uintptr_t>::max)() - offset || ++count > 128) { ok = false; return value; }
    ok = read && read(opaque, at + offset, std::as_writable_bytes(std::span(&value, 1))) == ScalarReadResult::success;
    return value;
  }
  std::uintptr_t ptr(std::uintptr_t at, std::uintptr_t offset = 0) noexcept { return get<std::uintptr_t>(at, offset); }
};
bool finite(std::uint32_t bits) noexcept { return std::isfinite(std::bit_cast<float>(bits)); }
}
ProducerFrame capture_producer_frame(std::uintptr_t base, std::uintptr_t args, ScalarReader read, void *opaque) noexcept {
  Reader r{read, opaque}; ProducerFrame p{};
  p.checkpoint = 1;
  p.renderer = r.ptr(base, 0x51c4480);
  p.frame = r.get<std::uint64_t>(p.renderer, 0x1f210);
  p.epoch = r.get<std::uint64_t>(p.renderer, 0x1f208);
  p.bank = r.get<unsigned>(p.renderer, 0x1f200);
  p.async_mode = r.get<unsigned char>(p.renderer, 0x93);
  std::array<std::uintptr_t, 8> slots{};
  for (unsigned i = 0; i < slots.size(); ++i) slots[i] = r.ptr(args, 8 * i);
  p.view = r.ptr(slots[6]); p.recording_context = r.ptr(slots[2]);
  p.current_index = r.ptr(slots[5]); p.next_index = r.ptr(slots[4]);
  p.sharpen = r.get<unsigned char>(slots[1], 391);
  p.reset_input = r.get<unsigned char>(slots[7]);
  const auto layer_reset = r.get<std::uint32_t>(slots[1], 384);
  p.reset_value_bits = p.reset_input ? 0 : layer_reset;
  for (unsigned i = 0; i < 4; ++i) {
    p.rect[i] = r.get<std::int32_t>(slots[3], 4 * i);
    p.rect_bits[i] = std::bit_cast<std::uint32_t>(static_cast<float>(p.rect[i]));
  }
  p.checkpoint = 2;
  p.jitter_mode = r.get<unsigned>(p.renderer, 0x7b424);
  p.table = r.ptr(base, 0x51c4368);
  std::uintptr_t sample{};
  if (p.jitter_mode == 1 || p.jitter_mode == 2) {
    const auto offset = p.jitter_mode == 1 ? 0xe740 + 8 * (p.frame % 18) : 0xe6a0 + 8 * (p.frame % 20);
    if (!p.table || p.table > (std::numeric_limits<std::uintptr_t>::max)() - offset) r.ok = false;
    else sample = p.table + offset;
  } else if (p.jitter_mode == 0 && base <= (std::numeric_limits<std::uintptr_t>::max)() - 0x503c3e8)
    sample = base + 0x503c3e8;
  else r.ok = false;
  for (unsigned i = 0; i < 2; ++i) p.jitter_bits[i] = r.get<std::uint32_t>(sample, 4 * i);
  const auto parameter_offset = p.jitter_mode == 1 ? 0x4be7404U : p.jitter_mode == 2 ? 0x4be7408U : 0x4be740cU;
  p.jitter_parameter = r.get<unsigned>(base, parameter_offset);
  p.checkpoint = 3;
  bool stable = true;
  for (unsigned i = 0; i < slots.size(); ++i) stable = (r.ptr(args, 8 * i) == slots[i]) && stable;
  stable = (r.ptr(slots[6]) == p.view) && stable;
  stable = (r.ptr(slots[2]) == p.recording_context) && stable;
  stable = (r.ptr(slots[5]) == p.current_index && r.ptr(slots[4]) == p.next_index) && stable;
  stable = (r.get<unsigned char>(slots[1], 391) == p.sharpen && r.get<unsigned char>(slots[7]) == p.reset_input) && stable;
  stable = (r.get<std::uint32_t>(slots[1], 384) == layer_reset) && stable;
  for (unsigned i = 0; i < 4; ++i) stable = (r.get<std::int32_t>(slots[3], 4 * i) == p.rect[i]) && stable;
  for (unsigned i = 0; i < 2; ++i) stable = (r.get<std::uint32_t>(sample, 4 * i) == p.jitter_bits[i]) && stable;
  stable = (r.get<unsigned>(p.renderer, 0x7b424) == p.jitter_mode && r.ptr(base, 0x51c4368) == p.table) && stable;
  p.frame_after = r.get<std::uint64_t>(p.renderer, 0x1f210);
  stable = (r.get<std::uint64_t>(p.renderer, 0x1f208) == p.epoch &&
      r.get<unsigned>(p.renderer, 0x1f200) == p.bank &&
      r.get<unsigned char>(p.renderer, 0x93) == p.async_mode) && stable;
  stable = (r.ptr(base, 0x51c4480) == p.renderer) && stable;
  p.selected_index = p.sharpen ? p.next_index : p.current_index;
  p.valid = r.ok && stable && p.frame == p.frame_after && p.sharpen <= 1 && p.reset_input <= 1 &&
      p.current_index == p.frame % 3 && p.next_index == (p.frame + 1) % 3 && p.view &&
      p.renderer <= (std::numeric_limits<std::uintptr_t>::max)() - 0x710 &&
      p.recording_context == p.renderer + 0x710 && p.bank <= 1 && p.async_mode <= 1 &&
      p.rect[0] == 0 && p.rect[1] == 0 && p.rect[2] == 2560 && p.rect[3] == 1440 &&
      finite(p.reset_value_bits) && finite(p.jitter_bits[0]) && finite(p.jitter_bits[1]);
  p.reads = r.count; if (p.valid) p.checkpoint = 100;
  return p;
}
PublicationReceipt capture_publication(std::uintptr_t base, const ProducerFrame &p,
    std::uintptr_t returned_entry, ScalarReader read, void *opaque) noexcept {
  Reader r{read, opaque}; PublicationReceipt q{};
  q.entry = returned_entry; q.recording_context = p.recording_context; q.checkpoint = 1;
  if (!p.valid || !returned_entry || !q.recording_context) return q;
  q.renderer = r.ptr(base, 0x51c4480);
  q.frame = r.get<std::uint64_t>(q.renderer, 0x1f210);
  q.epoch = r.get<std::uint64_t>(q.renderer, 0x1f208);
  q.bank = r.get<unsigned>(q.renderer, 0x1f200);
  q.async_mode = r.get<unsigned char>(q.renderer, 0x93);
  q.entry_base = r.ptr(q.recording_context, 0x2b78);
  q.count = r.get<unsigned>(q.recording_context, 0x2b80);
  q.capacity = r.get<unsigned>(q.recording_context, 0x2b84);
  q.checkpoint = 2;
  if (!r.ok || !q.entry_base || (q.entry_base & 15U) || !q.count ||
      q.count > q.capacity || q.capacity > 0x100000U ||
      q.entry_base > (std::numeric_limits<std::uintptr_t>::max)() - 16ULL * q.count ||
      q.entry != q.entry_base + 16ULL * (q.count - 1)) { q.reads = r.count; return q; }
  q.index = q.count - 1;
  q.key = r.get<unsigned>(q.entry); q.descriptor = r.ptr(q.entry, 8);
  const auto opcode = r.get<unsigned>(q.descriptor), subtype = r.get<unsigned>(q.descriptor, 16);
  q.payload = r.ptr(q.descriptor, 32); q.checkpoint = 3;
  q.wrapper = r.ptr(q.payload);
  for (unsigned i = 0; i < 4; ++i) q.owners[i] = r.ptr(q.payload, 8 + 8 * i);
  q.reset_value_bits = r.get<unsigned>(q.payload, 40);
  for (unsigned i = 0; i < 4; ++i) q.rect_bits[i] = r.get<unsigned>(q.payload, 44 + 4 * i);
  bool stable = r.get<unsigned>(q.payload, 60) == 0;
  stable = (r.ptr(q.payload) == q.wrapper) && stable;
  for (unsigned i = 0; i < 4; ++i) stable = (r.ptr(q.payload, 8 + 8 * i) == q.owners[i]) && stable;
  stable = (r.get<unsigned>(q.payload, 40) == q.reset_value_bits) && stable;
  for (unsigned i = 0; i < 4; ++i) stable = (r.get<unsigned>(q.payload, 44 + 4 * i) == q.rect_bits[i]) && stable;
  stable = (r.get<unsigned>(q.payload, 60) == 0 && r.ptr(q.descriptor, 32) == q.payload &&
      r.get<unsigned>(q.descriptor) == opcode && r.get<unsigned>(q.descriptor, 16) == subtype &&
      r.ptr(q.entry, 8) == q.descriptor && r.get<unsigned>(q.entry) == q.key) && stable;
  stable = (r.ptr(q.recording_context, 0x2b78) == q.entry_base &&
      r.get<unsigned>(q.recording_context, 0x2b80) == q.count &&
      r.get<unsigned>(q.recording_context, 0x2b84) == q.capacity) && stable;
  q.frame_after = r.get<std::uint64_t>(q.renderer, 0x1f210);
  stable = (r.get<std::uint64_t>(q.renderer, 0x1f208) == q.epoch &&
      r.get<unsigned>(q.renderer, 0x1f200) == q.bank &&
      r.get<unsigned char>(q.renderer, 0x93) == q.async_mode && r.ptr(base, 0x51c4480) == q.renderer) && stable;
  q.valid = r.ok && stable && q.renderer == p.renderer && q.frame == q.frame_after &&
      q.epoch == p.epoch && q.bank == p.bank && q.async_mode == p.async_mode &&
      opcode == 23 && subtype == 3 && q.payload && q.wrapper && finite(q.reset_value_bits) &&
      q.rect_bits == p.rect_bits && q.reset_value_bits == p.reset_value_bits;
  q.reads = r.count; if (q.valid) q.checkpoint = 100;
  return q;
}
StageFrame capture_stage_frame(std::uintptr_t base, std::uintptr_t payload, std::uintptr_t descriptor,
    std::uintptr_t context, ScalarReader read, void *opaque) noexcept {
  Reader r{read, opaque}; StageFrame s{};
  s.payload = payload; s.descriptor = descriptor; s.context = context;
  s.renderer = r.ptr(base, 0x51c4480); s.frame = r.get<std::uint64_t>(s.renderer, 0x1f210);
  s.renderer_epoch = r.get<std::uint64_t>(s.renderer, 0x1f208);
  s.bank = r.get<unsigned>(s.renderer, 0x1f200); s.async_mode = r.get<unsigned char>(s.renderer, 0x93);
  s.reset_value_bits = r.get<std::uint32_t>(payload, 40);
  for (unsigned i = 0; i < 4; ++i) s.rect_bits[i] = r.get<std::uint32_t>(payload, 44 + 4 * i);
  bool stable = r.get<unsigned>(payload, 60) == 0;
  std::uintptr_t epoch_slot{}, descriptor_slot{};
  if (context) {
    epoch_slot = r.ptr(context, 16); descriptor_slot = r.ptr(context);
    s.epoch = r.ptr(epoch_slot);
    stable = (descriptor && r.ptr(descriptor_slot) == descriptor) && stable;
  }
  stable = (r.get<std::uint32_t>(payload, 40) == s.reset_value_bits) && stable;
  for (unsigned i = 0; i < 4; ++i) stable = (r.get<std::uint32_t>(payload, 44 + 4 * i) == s.rect_bits[i]) && stable;
  if (context) stable = (r.ptr(context, 16) == epoch_slot && r.ptr(context) == descriptor_slot &&
      r.ptr(epoch_slot) == s.epoch && r.ptr(descriptor_slot) == descriptor) && stable;
  s.frame_after = r.get<std::uint64_t>(s.renderer, 0x1f210);
  stable = (r.get<std::uint64_t>(s.renderer, 0x1f208) == s.renderer_epoch &&
      r.get<unsigned>(s.renderer, 0x1f200) == s.bank &&
      r.get<unsigned char>(s.renderer, 0x93) == s.async_mode) && stable;
  stable = (r.ptr(base, 0x51c4480) == s.renderer) && stable;
  s.valid = r.ok && stable && s.frame == s.frame_after && s.bank <= 1 && s.async_mode <= 1 && finite(s.reset_value_bits);
  s.reads = r.count; return s;
}
bool frame_anchor_matches(std::uintptr_t base, const ProducerFrame &p, ScalarReader read, void *opaque) noexcept {
  Reader r{read, opaque};
  return p.valid && r.ptr(base, 0x51c4480) == p.renderer &&
      r.get<std::uint64_t>(p.renderer, 0x1f210) == p.frame &&
      r.get<std::uint64_t>(p.renderer, 0x1f208) == p.epoch &&
      r.get<unsigned>(p.renderer, 0x1f200) == p.bank &&
      r.get<unsigned char>(p.renderer, 0x93) == p.async_mode && r.ok;
}
bool FramePacketGate::queue(const ProducerFrame &p, const DepthWorkIdentity &work) noexcept {
  if (packet_.queued || packet_.rejection) return false;
  packet_.producer = p;
  if (!p.valid || p.view != work.view) { reject(1); return false; }
  for (unsigned i = 0; i < 4; ++i) packet_.resources[i] = work.textures[i].native;
  source_work_ = work;
  packet_.queued = true; return true;
}
bool FramePacketGate::publish(const PublicationReceipt &q, const DepthWorkCapture &work) noexcept {
  if (packet_.publication_attempted) { reject(4); return false; }
  packet_.publication_attempted = true; packet_.publication = q;
  if (!packet_.queued || packet_.rejection || packet_.stage_count || !q.valid || !work.valid ||
      work.identity != source_work_ || q.wrapper != source_work_.wrapper ||
      q.renderer != packet_.producer.renderer || q.recording_context != packet_.producer.recording_context ||
      q.epoch != packet_.producer.epoch || q.bank != packet_.producer.bank || q.async_mode != packet_.producer.async_mode ||
      !q.descriptor || !q.payload || q.rect_bits != packet_.producer.rect_bits ||
      q.reset_value_bits != packet_.producer.reset_value_bits) { reject(4); return false; }
  for (unsigned i = 0; i < 4; ++i)
    if (q.owners[i] != source_work_.textures[i].owner) { reject(4); return false; }
  packet_.publication_bound = true; return true;
}
bool FramePacketGate::matches(const StageFrame &s) const noexcept {
  const auto &p = packet_.producer;
  return packet_.queued && packet_.publication_bound && !packet_.rejection && s.valid && s.renderer == p.renderer &&
      s.payload == packet_.publication.payload && s.rect_bits == p.rect_bits && s.reset_value_bits == p.reset_value_bits;
}
bool FramePacketGate::prepare(unsigned ordinal, const StageFrame &s) noexcept {
  if (ordinal < 4 && packet_.stage_count == ordinal) { packet_.stages[ordinal] = s; ++packet_.stage_count; }
  else { reject(2); return false; }
  const auto &first = packet_.stages[0];
  if (!matches(s) || !s.context || s.descriptor != packet_.publication.descriptor || !s.payload || s.context != first.context ||
      s.descriptor != first.descriptor || s.payload != first.payload || s.epoch != first.epoch) { reject(10 + ordinal); return false; }
  return true;
}
bool FramePacketGate::consume(const StageFrame &s, std::uintptr_t command) noexcept {
  if (packet_.stage_count != 4 || packet_.consumer_bound) { reject(3); return false; }
  packet_.stages[4] = s; ++packet_.stage_count;
  if (!matches(s) || s.payload != packet_.stages[0].payload || !command) { reject(20); return false; }
  packet_.command = command; packet_.consumer_bound = true; return true;
}
bool FramePacketGate::evaluate(std::uintptr_t command, const StageFrame &s, const EvaluateContractSnapshot &input) noexcept {
  if (!packet_.consumer_bound || packet_.evaluate_bound || command != packet_.command || !matches(s) ||
      s.payload != packet_.stages[0].payload || !input.summary.complete || input.summary.fault_latched) { reject(30); return false; }
  constexpr unsigned keys[]{0, 1, 4, 3};
  for (unsigned i = 0; i < 4; ++i) {
    const auto &r = input.resources[keys[i]];
    if (!r.query_attempted || !r.callback_completed || !r.present || r.getter_result != 1 ||
        r.value != packet_.resources[i] || !r.value || r.value_is_null) { reject(31); return false; }
  }
  const auto &reset = input.signed_values[static_cast<unsigned>(EvaluateSignedKey::reset)];
  const auto expected = std::bit_cast<float>(packet_.producer.reset_value_bits) <= 0.f ? 1 : 0;
  if (!reset.query_attempted || !reset.callback_completed || !reset.present || reset.getter_result != 1 ||
      reset.value != expected) { reject(32); return false; }
  packet_.evaluate_frame = s.frame; packet_.evaluate_bound = true; return true;
}
} // namespace mhw::bridge_probe
