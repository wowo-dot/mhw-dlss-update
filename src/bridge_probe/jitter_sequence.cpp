#include "jitter_sequence.hpp"
#include <bit>
#include <cmath>
#include <limits>

namespace mhw::bridge_probe {
namespace {
struct Reader {
  ScalarReader read; void *opaque; unsigned count{}; bool ok{true};
  template<class T> T get(std::uintptr_t at, std::uintptr_t offset = 0) noexcept {
    T v{};
    if (!ok) return v;
    if (!at || at > (std::numeric_limits<std::uintptr_t>::max)() - offset || ++count > 40) { ok = false; return v; }
    ok = read && read(opaque, at + offset, std::as_writable_bytes(std::span(&v, 1))) == ScalarReadResult::success;
    return v;
  }
};
}
JitterGates capture_jitter_gates(std::uintptr_t base, std::uintptr_t args,
    const ProducerFrame &p, ScalarReader read, void *opaque) noexcept {
  JitterGates s{}; s.attempted = true; Reader r{read, opaque};
  if (!p.valid) { s.rejection = 1; return s; }
  s.layer = r.get<std::uintptr_t>(args, 8);
  s.wrapper_slot = r.get<std::uintptr_t>(args);
  s.wrapper = r.get<std::uintptr_t>(s.wrapper_slot);
  s.vtable = r.get<std::uintptr_t>(s.layer);
  s.vtable_rva = s.vtable >= base ? s.vtable - base : 0;
  s.layer_type = r.get<unsigned>(s.layer, 0xc);
  s.layer_flags = r.get<unsigned>(s.layer, 0x14);
  s.blend_bits = r.get<unsigned>(s.layer, 0x180);
  s.enabled = r.get<unsigned char>(s.layer, 0x184);
  s.use_dlss = r.get<unsigned char>(p.renderer, 0xa5);
  s.view_flags = r.get<unsigned>(p.view, 0x58);
  const auto view = r.get<std::uintptr_t>(p.renderer, 0x1f450);
  for (unsigned i = 0; i < 2; ++i) s.reset_vector_bits[i] = r.get<unsigned>(base, 0x503c3e8 + 4 * i);
  bool stable = r.get<std::uintptr_t>(args, 8) == s.layer && r.get<std::uintptr_t>(args) == s.wrapper_slot &&
      r.get<std::uintptr_t>(s.wrapper_slot) == s.wrapper && r.get<std::uintptr_t>(s.layer) == s.vtable &&
      r.get<unsigned>(s.layer, 0xc) == s.layer_type && r.get<unsigned>(s.layer, 0x14) == s.layer_flags &&
      r.get<unsigned>(s.layer, 0x180) == s.blend_bits && r.get<unsigned char>(s.layer, 0x184) == s.enabled &&
      r.get<unsigned char>(p.renderer, 0xa5) == s.use_dlss && r.get<unsigned>(p.view, 0x58) == s.view_flags &&
      r.get<std::uintptr_t>(p.renderer, 0x1f450) == view && view == p.view;
  for (unsigned i = 0; i < 2; ++i) stable = (r.get<unsigned>(base, 0x503c3e8 + 4 * i) == s.reset_vector_bits[i]) && stable;
  s.reads = r.count;
  const auto blend = std::bit_cast<float>(s.blend_bits);
  if (!r.ok) s.rejection = 2;
  else if (!stable) s.rejection = 3;
  else if (s.vtable_rva != 0x351bda8) s.rejection = 4;
  else if (s.enabled > 1 || s.use_dlss > 1 || !std::isfinite(blend) ||
      !std::isfinite(std::bit_cast<float>(s.reset_vector_bits[0])) || !std::isfinite(std::bit_cast<float>(s.reset_vector_bits[1]))) s.rejection = 5;
  s.valid = s.rejection == 0;
  s.predicate = s.valid && !(s.view_flags & 0x840000) && blend != 0 && s.layer_type == 8 &&
      (s.layer_flags & 0x12) == 0x12 && s.enabled != 0;
  return s;
}
void JitterSequence::poll(std::uint64_t tick) noexcept {
  if (!state_.stop && started_ && (tick < state_.started || tick - state_.started >= kJitterWindowMs)) state_.stop = 2;
}
void JitterSequence::close() noexcept { if (!state_.stop) state_.stop = 3; }
bool JitterSequence::reserve(std::uint64_t tick, bool with_handle, JitterSample &out) noexcept {
  poll(tick);
  if (state_.stop || pending_ || (!started_ && !with_handle)) return false;
  if (state_.armed && tick < state_.armed) { state_.stop = 2; return false; }
  if (!started_) { started_ = true; state_.started = tick; }
  const unsigned phase = state_.armed ? 1U : 0U;
  const auto count = phase ? state_.scene : state_.initial;
  if (count >= kJitterSamplesPerPhase || (count && (tick < last_tick_[phase] || tick - last_tick_[phase] < kJitterIntervalMs))) return false;
  pending_ = true; pending_phase_ = phase; pending_tick_ = tick;
  out = {}; out.index = state_.count; out.phase = phase; out.tick = tick;
  return true;
}
bool JitterSequence::arm_scene(std::uint64_t tick) noexcept {
  poll(tick);
  if (state_.stop || pending_ || state_.armed || state_.initial != kJitterSamplesPerPhase ||
      tick < last_tick_[0]) return false;
  state_.armed = tick;
  return true;
}
void JitterSequence::commit(JitterSample s) noexcept {
  if (!pending_ || s.index != state_.count || s.phase != pending_phase_ || s.tick != pending_tick_ || state_.count >= kJitterSequenceCapacity) return;
  const auto phase = s.phase;
  s.duplicate = s.frame.valid && have_frame_[phase] && s.frame.frame <= last_frame_[phase];
  s.valid = s.frame.valid && s.frame.projection.valid && s.gates.valid && !s.duplicate;
  if (s.frame.valid && !s.duplicate) { last_frame_[phase] = s.frame.frame; have_frame_[phase] = true; }
  last_tick_[phase] = s.tick;
  samples_[state_.count++] = s;
  if (phase) ++state_.scene; else ++state_.initial;
  pending_ = false;
  if (state_.count == kJitterSequenceCapacity && !state_.stop) state_.stop = 1;
}
bool JitterSequence::sample(unsigned index, JitterSample &out) const noexcept {
  if (index >= state_.count) return false;
  out = samples_[index]; return true;
}
} // namespace mhw::bridge_probe
