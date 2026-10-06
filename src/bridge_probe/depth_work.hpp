#pragma once
#include "depth_selection.hpp"
#include <array>

namespace mhw::bridge_probe {
struct DepthTextureIdentity {
  std::uintptr_t owner{}, backing{}, native{}, tracker{};
  bool operator==(const DepthTextureIdentity &) const = default;
};
struct DepthWorkIdentity {
  std::uintptr_t wrapper{}, view{}, container{}, raw_depth{};
  // Payload order: color, dense motion, output, depth.
  std::array<DepthTextureIdentity, 4> textures{};
  bool operator==(const DepthWorkIdentity &) const = default;
};
struct DepthWorkCapture {
  DepthWorkIdentity identity{};
  unsigned reads{};
  bool valid{};
  unsigned checkpoint{}; // 1 descriptor, 2 producer index, 10/20 view, 30..33
                         // texture, 40 selector, 50/60/70 recheck, 100 complete
};
DepthWorkCapture capture_producer_depth_work(std::uintptr_t base,
                                             std::uintptr_t args, ScalarReader,
                                             void *) noexcept;
DepthWorkCapture capture_prepared_depth_work(std::uintptr_t base,
                                             std::uintptr_t payload,
                                             std::uintptr_t descriptor,
                                             ScalarReader, void *) noexcept;
DepthWorkCapture capture_consumer_depth_work(std::uintptr_t base,
                                             std::uintptr_t payload,
                                             ScalarReader, void *) noexcept;
bool depth_tracking_context_verified(
    std::uintptr_t context, std::uintptr_t descriptor,
    const DepthTextureIdentity &, ScalarReader, void *,
    unsigned *observed_state = nullptr) noexcept;

enum class DepthWorkPhase : unsigned {
  idle,
  queued,
  preparing,
  prepared,
  consuming,
  complete,
  rejected
};
// Serialized by the runtime. One item per process; no pointer is dereferenced
// by this policy. Metadata is retired on completion/rejection, never reused.
class DepthWorkGate {
public:
  bool queue(const DepthWorkCapture &) noexcept;
  std::uintptr_t prepare(unsigned ordinal, const DepthWorkCapture &,
                         std::uintptr_t payload, std::uintptr_t descriptor,
                         std::uintptr_t context, unsigned thread,
                         std::uintptr_t supplied, unsigned request,
                         bool depth_context_valid) noexcept;
  bool consume(const DepthWorkCapture &, std::uintptr_t payload) noexcept;
  void finish() noexcept;
  void reject() noexcept;
  DepthWorkPhase phase() const noexcept { return phase_; }

private:
  DepthWorkPhase phase_{DepthWorkPhase::idle};
  DepthWorkIdentity work_{};
  std::uintptr_t payload_{}, descriptor_{}, context_{};
  unsigned thread_{}, next_{};
};
} // namespace mhw::bridge_probe
