#pragma once

#include "renderer_snapshot.hpp"
#include <cstdint>

namespace mhw::bridge_probe {

enum class DepthSelectionAction : std::uint8_t { unchanged, primary, reject_work };
enum class DepthSelectionReason : std::uint8_t {
  other_call, secondary_present, selected, invalid_request, unreadable,
  wrong_owner, wrong_layout, missing_primary, changed
};
struct DepthSelectionRequest {
  std::uintptr_t host_base{}, return_address{}, supplied_backing{};
  std::uintptr_t owner{}, view_depth_owner{}, raw_depth_owner{};
  std::uint16_t requested_state{};
  bool host_contract_verified{};
};
struct DepthSelectionResult {
  DepthSelectionAction action{DepthSelectionAction::reject_work};
  DepthSelectionReason reason{DepthSelectionReason::invalid_request};
  std::uintptr_t backing{}, native{}, tracker{};
  unsigned reads{};
};

// Only selects the argument. Does not write a texture, update engine state,
// invoke a helper or authorize GPU work. The caller must drop rejected optional
// work at its producer/consumer boundary; forwarding a null argument is unsafe.
DepthSelectionResult select_depth_backing(const DepthSelectionRequest &,
                                          ScalarReader, void *) noexcept;

} // namespace mhw::bridge_probe
