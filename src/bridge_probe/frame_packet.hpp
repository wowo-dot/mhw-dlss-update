#pragma once
#include "depth_work.hpp"
#include "contract_snapshot.hpp"
#include "projection_snapshot.hpp"

namespace mhw::bridge_probe {
struct ProducerFrame {
  ProjectionSnapshot projection{};
  bool valid{};
  unsigned reads{}, checkpoint{}, jitter_mode{}, sharpen{}, reset_input{};
  std::uintptr_t renderer{}, table{}, view{}, recording_context{};
  std::uint64_t frame{}, frame_after{}, current_index{}, next_index{}, selected_index{};
  std::uint64_t epoch{};
  unsigned bank{}, async_mode{};
  std::array<std::int32_t, 4> rect{};
  std::array<std::uint32_t, 4> rect_bits{};
  std::array<std::uint32_t, 2> jitter_bits{};
  std::uint32_t reset_value_bits{}, jitter_parameter{};
};
struct StageFrame {
  bool valid{};
  unsigned reads{};
  std::uintptr_t renderer{}, payload{}, descriptor{}, context{};
  std::uint64_t frame{}, frame_after{}, epoch{};
  std::uint64_t renderer_epoch{};
  unsigned bank{}, async_mode{};
  std::array<std::uint32_t, 4> rect_bits{};
  std::uint32_t reset_value_bits{};
};
struct PublicationReceipt {
  bool valid{};
  unsigned reads{}, checkpoint{}, count{}, capacity{}, index{}, key{};
  std::uintptr_t renderer{}, recording_context{}, entry{}, entry_base{}, descriptor{}, payload{}, wrapper{};
  std::array<std::uintptr_t, 4> owners{};
  std::uint64_t frame{}, frame_after{}, epoch{};
  unsigned bank{}, async_mode{};
  std::array<std::uint32_t, 4> rect_bits{};
  std::uint32_t reset_value_bits{};
  bool operator==(const PublicationReceipt &) const = default;
};
PublicationReceipt capture_publication(std::uintptr_t base, const ProducerFrame &,
    std::uintptr_t returned_entry, ScalarReader, void *) noexcept;
ProducerFrame capture_producer_frame(std::uintptr_t base, std::uintptr_t args, ScalarReader, void *) noexcept;
bool frame_anchor_matches(std::uintptr_t base, const ProducerFrame &, ScalarReader, void *) noexcept;
StageFrame capture_stage_frame(std::uintptr_t base, std::uintptr_t payload,
    std::uintptr_t descriptor, std::uintptr_t context, ScalarReader, void *) noexcept;
struct FramePacket {
  unsigned schema{2}, stage_count{}, rejection{};
  bool queued{}, consumer_bound{}, evaluate_bound{};
  bool publication_attempted{}, publication_bound{};
  ProducerFrame producer{};
  PublicationReceipt publication{};
  std::array<StageFrame, 5> stages{};
  std::array<std::uintptr_t, 4> resources{}; // color, motion, output, depth
  std::uintptr_t command{};
  std::uint64_t evaluate_frame{};
};
// Runtime serializes this state with its existing one-item gate mutex.
class FramePacketGate {
public:
  bool queue(const ProducerFrame &, const DepthWorkIdentity &) noexcept;
  bool publish(const PublicationReceipt &, const DepthWorkCapture &) noexcept;
  bool prepare(unsigned ordinal, const StageFrame &) noexcept;
  bool consume(const StageFrame &, std::uintptr_t command) noexcept;
  bool evaluate(std::uintptr_t command, const StageFrame &, const EvaluateContractSnapshot &) noexcept;
  const FramePacket &packet() const noexcept { return packet_; }
  void reject(unsigned why) noexcept { if (!packet_.rejection) packet_.rejection = why; }
private:
  bool matches(const StageFrame &) const noexcept;
  FramePacket packet_{};
  DepthWorkIdentity source_work_{};
};
} // namespace mhw::bridge_probe
