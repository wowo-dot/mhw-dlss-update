#pragma once
#include "frame_packet.hpp"

namespace mhw::bridge_probe {
inline constexpr unsigned kJitterSamplesPerPhase = 16, kJitterSequenceCapacity = 32;
inline constexpr std::uint64_t kJitterIntervalMs = 100, kJitterWindowMs = 120000;
struct JitterGates {
  bool attempted{}, valid{}, predicate{};
  unsigned reads{}, rejection{}, layer_type{}, layer_flags{}, view_flags{}, blend_bits{};
  unsigned char enabled{}, use_dlss{};
  std::uintptr_t layer{}, vtable{}, vtable_rva{}, wrapper_slot{}, wrapper{};
  std::array<unsigned, 2> reset_vector_bits{};
  bool operator==(const JitterGates &) const = default;
};
JitterGates capture_jitter_gates(std::uintptr_t base, std::uintptr_t args,
    const ProducerFrame &, ScalarReader, void *) noexcept;
struct JitterSample {
  unsigned index{}, phase{}; // 0: initial burst, 1: explicitly triggered scene burst
  std::uint64_t tick{};
  bool valid{}, duplicate{};
  unsigned thread{};
  JitterGates gates{};
  ProducerFrame frame{};
};
struct JitterSequenceState {
  unsigned count{}, initial{}, scene{}, stop{}; // 1: capacity, 2: deadline/clock, 3: trace closed
  std::uint64_t started{}, armed{};
};
// Caller serializes reservations/commits. Every reservation consumes a slot,
// including invalid or duplicate-frame observations; budgets never replenish.
class JitterSequence {
public:
  bool reserve(std::uint64_t tick, bool with_handle, JitterSample &) noexcept;
  bool arm_scene(std::uint64_t tick) noexcept;
  void commit(JitterSample) noexcept;
  void poll(std::uint64_t tick) noexcept;
  void close() noexcept;
  const JitterSequenceState &state() const noexcept { return state_; }
  bool sample(unsigned index, JitterSample &) const noexcept;
private:
  JitterSequenceState state_{};
  std::array<JitterSample, kJitterSequenceCapacity> samples_{};
  std::array<std::uint64_t, 2> last_tick_{}, last_frame_{};
  std::array<bool, 2> have_frame_{};
  bool started_{}, pending_{};
  unsigned pending_phase_{};
  std::uint64_t pending_tick_{};
};
} // namespace mhw::bridge_probe
