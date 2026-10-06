#pragma once
#include "depth_work.hpp"
#include "frame_packet.hpp"
#include "jitter_sequence.hpp"
namespace mhw::bridge_probe {
struct NativeDepthProbeStats {
  unsigned install_status{}, phase{};
  unsigned rejection_stage{}, capture_checkpoint{}, capture_reads{},
      tracker_state{};
  std::uint64_t producer_admitted{}, preparation_calls{}, depth_corrected{},
      preparation_rejected{}, consumer_admitted{}, consumer_rejected{},
      consumer_completed{};
};
bool install_native_depth_probe(std::uintptr_t host_base) noexcept;
bool admit_native_depth_producer(std::uintptr_t args) noexcept;
void publish_native_depth_producer(std::uintptr_t returned_entry) noexcept;
bool native_depth_evaluate_permitted() noexcept;
NativeDepthProbeStats native_depth_probe_stats() noexcept;
FramePacket native_depth_frame_packet() noexcept;
void observe_native_jitter_sequence(std::uintptr_t args, bool with_wrapper, std::uint64_t tick = 0) noexcept;
bool native_jitter_sample(unsigned index, JitterSample &) noexcept;
JitterSequenceState native_jitter_sequence_state(std::uint64_t tick = 0) noexcept;
void close_native_jitter_sequence() noexcept;
bool arm_native_jitter_scene(std::uint64_t tick = 0) noexcept;
bool bind_native_depth_evaluate(std::uintptr_t command, const EvaluateContractSnapshot &) noexcept;
#ifdef MHW_OWNED_DEPTH_TEST
bool install_owned_depth_probe(std::uintptr_t base, std::uintptr_t helper,
                               std::uintptr_t consumer,
                               const std::array<std::uintptr_t, 4> &callers,
                               std::uintptr_t consumer_caller) noexcept;
bool remove_owned_depth_probe() noexcept;
#endif
} // namespace mhw::bridge_probe
