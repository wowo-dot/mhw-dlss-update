#include "resource_inspection.hpp"

namespace mhw::bridge_probe {
namespace {
int fault_filter(DWORD code) noexcept {
  return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR
             ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}
void fault(ResourceInspection &s) noexcept {
  s.fault_latched = true;
  s.stop = ResourceInspectionStop::foreign_fault;
}
HRESULT query(IUnknown *object, REFIID iid, void **out, ResourceInspection &s) noexcept {
  ++s.calls;
  __try {
    const auto hr = object->QueryInterface(iid, out);
    if (*out) ++s.references_acquired;
    return hr;
  } __except (fault_filter(GetExceptionCode())) { fault(s); return E_FAIL; }
}
HRESULT device(ID3D12DeviceChild *object, REFIID iid, void **out, ResourceInspection &s) noexcept {
  ++s.calls;
  __try {
    const auto hr = object->GetDevice(iid, out);
    if (*out) ++s.references_acquired;
    return hr;
  } __except (fault_filter(GetExceptionCode())) { fault(s); return E_FAIL; }
}
void release(IUnknown *object, ResourceInspection &s) noexcept {
  if (!object) return;
  ++s.calls;
  ++s.releases_attempted;
  __try { object->Release(); ++s.releases_completed; }
  __except (fault_filter(GetExceptionCode())) { fault(s); }
}
template <typename T> struct Owned {
  T *value{};
  ResourceInspection &s;
  explicit Owned(ResourceInspection &state) : s(state) {}
  ~Owned() { release(value, s); }
  Owned(const Owned &) = delete;
  Owned &operator=(const Owned &) = delete;
  void **out() { return reinterpret_cast<void **>(&value); }
};
bool command_metadata(ID3D12GraphicsCommandList *list, ID3D12Device *owner,
                       ResourceInspection &s) noexcept {
  __try {
    ++s.calls;
    s.command_type = list->GetType();
    ++s.calls;
    s.adapter_luid = owner->GetAdapterLuid();
    s.adapter_available = true;
    return true;
  } __except (fault_filter(GetExceptionCode())) { fault(s); return false; }
}
bool description(ID3D12Resource *object, D3D12_RESOURCE_DESC &out,
                   ResourceInspection &s) noexcept {
  ++s.calls;
  __try { out = object->GetDesc(); return true; }
  __except (fault_filter(GetExceptionCode())) { fault(s); return false; }
}
bool valid_contract(const EvaluateContractSnapshot &s) noexcept {
  if (!s.summary.complete || s.summary.fault_latched ||
      s.summary.stop_reason != CaptureStopReason::none ||
      s.summary.queries_attempted != kEvaluateQueryCount) return false;
  for (std::size_t i = 0; i < s.resources.size(); ++i) {
    const auto &r = s.resources[i];
    if (r.key != static_cast<EvaluateResourceKey>(i) || !r.query_attempted ||
        !r.callback_completed || !r.present || r.getter_result != 1U ||
        r.value_is_null != (r.value == 0U) ||
        (r.key != EvaluateResourceKey::previous_output && r.value == 0U)) return false;
  }
  return true;
}
void inspect(ID3D12GraphicsCommandList *list, const EvaluateContractSnapshot &input,
             ResourceInspection &s) noexcept {
  s.contract_valid = list && valid_contract(input);
  if (!s.contract_valid) { s.stop = ResourceInspectionStop::invalid_contract; return; }
  Owned<ID3D12Device> command_device(s);
  s.command_device_result = device(list, __uuidof(ID3D12Device), command_device.out(), s);
  if (s.fault_latched) return;
  if (FAILED(s.command_device_result) || !command_device.value) {
    s.stop = ResourceInspectionStop::command_device; return;
  }
  if (!command_metadata(list, command_device.value, s)) return;
  if (s.command_type != D3D12_COMMAND_LIST_TYPE_DIRECT && s.command_type != D3D12_COMMAND_LIST_TYPE_COMPUTE) {
    s.stop = ResourceInspectionStop::command_type; return;
  }
  Owned<IUnknown> command_identity(s);
  s.command_identity_result = query(command_device.value, __uuidof(IUnknown), command_identity.out(), s);
  if (s.fault_latched) return;
  if (FAILED(s.command_identity_result) || !command_identity.value) {
    s.stop = ResourceInspectionStop::command_identity; return;
  }
  s.command_identity_verified = true;
  for (std::size_t i = 0; i < input.resources.size(); ++i) {
    // Release failures at the end of the previous iteration also latch off.
    if (s.fault_latched) return;
    if (!input.resources[i].value) continue;
    auto &r = s.resources[i];
    r.attempted = true;
    Owned<ID3D12Resource> resource(s);
    r.interface_result = query(reinterpret_cast<IUnknown *>(input.resources[i].value),
                               __uuidof(ID3D12Resource), resource.out(), s);
    if (s.fault_latched) return;
    r.interface_verified = SUCCEEDED(r.interface_result) && resource.value;
    if (!r.interface_verified) { s.stop = ResourceInspectionStop::resource_interface; return; }
    r.description_available = description(resource.value, r.description, s);
    if (!r.description_available) return;
    Owned<IUnknown> resource_device(s);
    // IUnknown supplies canonical COM identity, not an adapter-only comparison.
    r.device_result = device(resource.value, __uuidof(IUnknown), resource_device.out(), s);
    if (s.fault_latched) return;
    if (FAILED(r.device_result) || !resource_device.value) {
      s.stop = ResourceInspectionStop::resource_device; return;
    }
    r.same_device = resource_device.value == command_identity.value;
    if (!r.same_device) { s.stop = ResourceInspectionStop::device_mismatch; return; }
  }
}
} // namespace

ResourceInspection inspect_evaluate_resources(ID3D12GraphicsCommandList *list,
                                               const EvaluateContractSnapshot &input) noexcept {
  ResourceInspection s;
  for (std::size_t i = 0; i < s.resources.size(); ++i)
    s.resources[i].key = static_cast<EvaluateResourceKey>(i);
  inspect(list, input, s); // All owned references are released before completion.
  s.complete = s.contract_valid && s.stop == ResourceInspectionStop::none &&
               !s.fault_latched && s.references_acquired == s.releases_completed;
  return s;
}
std::string_view resource_inspection_stop_name(ResourceInspectionStop reason) noexcept {
  switch (reason) {
  case ResourceInspectionStop::none: return "none";
  case ResourceInspectionStop::invalid_contract: return "invalid-contract";
  case ResourceInspectionStop::command_device: return "command-device";
  case ResourceInspectionStop::command_type: return "command-type";
  case ResourceInspectionStop::command_identity: return "command-identity";
  case ResourceInspectionStop::resource_interface: return "resource-interface";
  case ResourceInspectionStop::resource_device: return "resource-device";
  case ResourceInspectionStop::device_mismatch: return "device-mismatch";
  case ResourceInspectionStop::foreign_fault: return "foreign-fault";
  }
  return "unknown";
}
} // namespace mhw::bridge_probe
