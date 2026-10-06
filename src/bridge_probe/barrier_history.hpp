#pragma once
#include "contract_snapshot.hpp"
#include <d3d12.h>
#include <array>
#include <span>

namespace mhw::bridge_probe {
inline constexpr unsigned kBarrierCallCapacity = 2048;
inline constexpr unsigned kBarrierDescriptionCapacity = 16384;
inline constexpr unsigned kBarrierBatchCapacity = 1024;
inline constexpr unsigned kBarrierHistoryPerResource = 4;
enum class BarrierCallKind : unsigned { barrier, reset, close, bundle, execute, signal, wait, fence_completed, fence_cpu_signal };
struct BarrierCall {
  std::uintptr_t command{}, caller{};
  std::uint64_t tick{};
  unsigned thread{}, sequence{}, first{}, count{};
  BarrierCallKind kind{};
  HRESULT result{E_PENDING};
  std::uintptr_t queue{}, fence{};
  std::uint64_t fence_value{}, entry_order{}, return_order{};
};
struct BarrierMatch {
  std::uintptr_t caller{};
  std::uint64_t tick{};
  unsigned sequence{}, thread{}, index{}, batch_size{}, type{}, flags{}, subresource{}, before{}, after{};
  std::uintptr_t command{};
  std::uint64_t entry_order{}, return_order{};
};
inline constexpr unsigned kSubmittedListCapacity = 4096, kSubmissionBatchCapacity = 128;
struct DepthTimeline {
  unsigned matches{}, retained{}, controls{}, controls_retained{}, queues{}, queues_retained{}, list_count{};
  bool truncated{}, malformed{};
  std::array<BarrierMatch, 16> recent{};
  std::array<BarrierCall, 64> control_events{};
  std::array<BarrierCall, 64> queue_events{};
  std::array<std::uintptr_t, 256> submitted_lists{};
};
struct ResourceBarrierHistory {
  EvaluateResourceKey key{};
  unsigned transitions{}, uav{}, aliasing{}, matches{}, retained{}, last_declared_state{};
  bool present{}, ambiguous{}, continuity_mismatch{}, declaration_available{}, expected_state_matches{};
  std::array<BarrierMatch, kBarrierHistoryPerResource> recent{};
};
struct BarrierCapture {
  std::uintptr_t command{};
  unsigned calls{}, descriptions{}, matching_calls{}, resets{}, epoch_sequence{}, evaluate_thread{};
  bool installed{}, armed{}, finished{}, methods_match{}, faulted{}, overflow{}, contention{}, expired{}, in_flight{};
  bool contract_valid{}, coverage_complete{}, closed{}, bundle_seen{}, reset_failed{};
  std::array<ResourceBarrierHistory, kEvaluateResourceKeyCount> resources{};
  DepthTimeline depth_timeline{};
};
// Pure replay of immutable, completed CPU recording observations. It describes
// declared transitions only: no GPU completion, initial state or lifetime proof.
BarrierCapture summarize_barriers(std::span<const BarrierCall>,
                                 std::span<const D3D12_RESOURCE_BARRIER>,
                                 std::uintptr_t command, unsigned thread,
                                 const EvaluateContractSnapshot &, BarrierCapture status) noexcept;
// Retain cross-list depth matches and the observed queue/lifecycle timeline.
// No inference of GPU order or pointer lifetime is made from CPU event order.
DepthTimeline depth_timeline(std::span<const BarrierCall>, std::span<const D3D12_RESOURCE_BARRIER>,
                             std::span<const std::uintptr_t> submitted_lists, std::uintptr_t depth,
                             std::uintptr_t evaluate_command) noexcept;
} // namespace mhw::bridge_probe
