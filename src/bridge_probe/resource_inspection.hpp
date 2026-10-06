#pragma once

#include "contract_snapshot.hpp"
#include <d3d12.h>

namespace mhw::bridge_probe {

enum class ResourceInspectionStop : std::uint8_t {
  none, invalid_contract, command_device, command_type, command_identity,
  resource_interface, resource_device, device_mismatch, foreign_fault
};
struct InspectedResource {
  EvaluateResourceKey key{};
  HRESULT interface_result{E_PENDING};
  HRESULT device_result{E_PENDING};
  bool attempted{};
  bool interface_verified{};
  bool description_available{};
  bool same_device{};
  D3D12_RESOURCE_DESC description{};
};
struct ResourceInspection {
  std::array<InspectedResource, kEvaluateResourceKeyCount> resources{};
  ResourceInspectionStop stop{ResourceInspectionStop::none};
  HRESULT command_device_result{E_PENDING};
  HRESULT command_identity_result{E_PENDING};
  D3D12_COMMAND_LIST_TYPE command_type{D3D12_COMMAND_LIST_TYPE_COPY};
  LUID adapter_luid{};
  unsigned calls{}, references_acquired{}, releases_attempted{}, releases_completed{};
  bool contract_valid{}, command_identity_verified{}, adapter_available{};
  bool complete{}, fault_latched{};
};

// Call only within the first admitted Evaluate, while its borrowed command
// list and NGX resource pointers are alive. The implementation queries owned
// COM interfaces, copies descriptions, and releases every acquired reference
// before returning. No borrowed COM pointer is retained in this result.
// GetDesc does not expose current resource states or image content.
ResourceInspection inspect_evaluate_resources(ID3D12GraphicsCommandList *,
                                               const EvaluateContractSnapshot &) noexcept;
std::string_view resource_inspection_stop_name(ResourceInspectionStop) noexcept;

} // namespace mhw::bridge_probe
