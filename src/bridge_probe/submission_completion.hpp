#pragma once
#include "barrier_history.hpp"

namespace mhw::bridge_probe {
enum class CompletionStatus : unsigned { pending, observed, rejected };
enum class CompletionIssue : unsigned {
  none, malformed, recording, queue_overlap, writer, device_removed
};
struct SubmissionCompletion {
  CompletionStatus status{};
  CompletionIssue issue{};
  unsigned reset{}, close{}, submission{}, signal{}, completed{};
  std::uintptr_t command{}, queue{}, fence{};
  std::uint64_t marker{}, value{}, completed_order{};
};
// Pure bounded replay. Labels only: never dereferences or retains COM objects,
// never authorizes rendering, and cannot exclude unobserved fence writers.
SubmissionCompletion review_submission_completion(
    std::span<const BarrierCall>, std::span<const std::uintptr_t> lists,
    std::uintptr_t command, std::uint64_t marker) noexcept;
} // namespace mhw::bridge_probe
