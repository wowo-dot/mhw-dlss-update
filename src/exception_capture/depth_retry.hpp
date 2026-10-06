#pragma once
#include "depth_tracker.hpp"
#include <array>
#include <string_view>

namespace mhw::exception_capture {
enum class DepthRetryMode { once, three_frames };
struct RetryObservation {
  const TrackerSnapshot *tracker{};
  std::uint64_t tick{};
  unsigned thread{};
};
// Default remains one attempt. The explicit three-frame experiment also binds
// depth identity, thread, consecutive renderer counters, tracker branch and time.
// Live code/context and capture-channel validation remain mandatory at each event.
class DepthRetry {
public:
  explicit DepthRetry(DepthRetryMode mode = DepthRetryMode::once) : mode_(mode) {}
  std::optional<Address> take(const Snapshot &snapshot, Address base,
                              bool first_chance, bool contract_verified, RetryObservation observation = {});
  unsigned attempts() const { return attempts_; }
  unsigned limit() const { return mode_ == DepthRetryMode::three_frames ? 3U : 1U; }
  std::string_view reason() const { return reason_; }
  static constexpr std::uint64_t window_ms = 500;
  bool within_window(std::uint64_t now) const {
    return mode_ == DepthRetryMode::once || (attempts_ && now >= last_tick_ && now - start_ <= window_ms);
  }
private:
  DepthRetryMode mode_;
  unsigned attempts_{};
  std::uint64_t start_{}, last_tick_{}, frame_{};
  unsigned thread_{};
  std::array<Address, 12> identity_{};
  std::string_view reason_{"not-attempted"};
};
} // namespace mhw::exception_capture
