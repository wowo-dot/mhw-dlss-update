#include "contract_snapshot.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

namespace {

using mhw::bridge_probe::CaptureState;
using mhw::bridge_probe::CaptureStopReason;
using mhw::bridge_probe::CreateContractSnapshot;
using mhw::bridge_probe::EvaluateContractSnapshot;
using mhw::bridge_probe::FloatGetterReply;
using mhw::bridge_probe::GetterCallbacks;
using mhw::bridge_probe::ResourceGetterReply;
using mhw::bridge_probe::SignedGetterReply;
using mhw::bridge_probe::UnsignedGetterReply;

enum class GetterKind : std::uint8_t {
  unsigned_value,
  signed_value,
  float_value,
  resource,
};

struct QueryRecord {
  GetterKind kind{};
  std::string_view key{};
};

struct MockContext {
  std::array<QueryRecord, 64> queries{};
  std::size_t query_count{};
  std::size_t fault_at{(std::numeric_limits<std::size_t>::max)()};
  bool return_missing_keys{};
};

int fail(const char *message) {
  std::cerr << message << '\n';
  return 1;
}

std::size_t record_query(MockContext &context, const GetterKind kind,
                         const std::string_view key) noexcept {
  const std::size_t index = context.query_count;
  if (index < context.queries.size()) {
    context.queries[index] = {kind, key};
  }
  ++context.query_count;
  return index;
}

bool callback_completed(const MockContext &context,
                        const std::size_t query_index) noexcept {
  return query_index != context.fault_at;
}

UnsignedGetterReply get_unsigned(void *const opaque_context,
                                 const std::string_view key) noexcept {
  auto &context = *static_cast<MockContext *>(opaque_context);
  const std::size_t query_index =
      record_query(context, GetterKind::unsigned_value, key);

  std::uint32_t value{};
  if (key == "Width") {
    value = 2560U;
  } else if (key == "Height") {
    value = 1440U;
  } else if (key == "OutWidth") {
    value = 5120U;
  } else if (key == "OutHeight") {
    value = 2880U;
  } else if (key == "ResourceWidth") {
    value = 2560U;
  } else if (key == "ResourceHeight") {
    value = 1440U;
  } else if (key == "CreationNodeMask") {
    value = 1U;
  } else if (key == "VisibilityNodeMask") {
    value = 3U;
  }

  return {0x11000000U + static_cast<std::uint32_t>(query_index), value,
          callback_completed(context, query_index), true};
}

SignedGetterReply get_signed(void *const opaque_context,
                             const std::string_view key) noexcept {
  auto &context = *static_cast<MockContext *>(opaque_context);
  const std::size_t query_index =
      record_query(context, GetterKind::signed_value, key);

  std::int32_t value{};
  if (key == "PerfQualityValue") {
    value = 2;
  } else if (key == "RTXValue") {
    value = 1;
  } else if (key == "Hint.HDR") {
    value = 1;
  } else if (key == "DepthInverted") {
    value = 1;
  } else if (key == "Reset") {
    value = 1;
  } else if (key == "DLAAMode") {
    value = 0;
  } else if (key == "Hint.RunPrePass") {
    value = 1;
  } else if (key == "Hint.RunPass") {
    value = 1;
  } else if (key == "Hint.RunPostPass") {
    value = 0;
  }

  if (context.return_missing_keys && key == "DLAAMode") {
    return {0xE0030001U, -77,
            callback_completed(context, query_index), false};
  }
  return {0x22000000U + static_cast<std::uint32_t>(query_index), value,
          callback_completed(context, query_index), true};
}

FloatGetterReply get_float(void *const opaque_context,
                           const std::string_view key) noexcept {
  auto &context = *static_cast<MockContext *>(opaque_context);
  const std::size_t query_index =
      record_query(context, GetterKind::float_value, key);

  float value{};
  if (key == "Sharpness") {
    value = 0.25F;
  } else if (key == "MV.Scale.X") {
    value = -0.5F;
  } else if (key == "MV.Scale.Y") {
    value = 0.5F;
  } else if (key == "MV.Offset.X") {
    value = 1.0F;
  } else if (key == "MV.Offset.Y") {
    value = -1.0F;
  } else if (key == "HDRClamp") {
    value = 4.0F;
  }

  if (context.return_missing_keys && key == "HDRClamp") {
    return {0xE0020001U, 99.0F,
            callback_completed(context, query_index), false};
  }
  return {0x33000000U + static_cast<std::uint32_t>(query_index), value,
          callback_completed(context, query_index), true};
}

ResourceGetterReply get_resource(void *const opaque_context,
                                 const std::string_view key) noexcept {
  auto &context = *static_cast<MockContext *>(opaque_context);
  const std::size_t query_index =
      record_query(context, GetterKind::resource, key);

  std::uintptr_t value{};
  if (key == "Color") {
    value = 0x1000U;
  } else if (key == "MotionVectors") {
    value = 0U;
  } else if (key == "PreviousOutput") {
    value = 0x3000U;
  } else if (key == "Depth") {
    value = 0x4000U;
  } else if (key == "Output") {
    value = 0x5000U;
  }

  if (context.return_missing_keys && key == "PreviousOutput") {
    return {0xE0010001U, 0xDEADU,
            callback_completed(context, query_index), false};
  }
  return {0x44000000U + static_cast<std::uint32_t>(query_index), value,
          callback_completed(context, query_index), true};
}

GetterCallbacks callbacks_for(MockContext &context) noexcept {
  return {&context, get_unsigned, get_signed, get_float, get_resource};
}

} // namespace

int main() {
  using mhw::bridge_probe::capture_optimal_settings_contract;
  MockContext optimal_context{};
  const auto optimal =
      capture_optimal_settings_contract(callbacks_for(optimal_context));
  if (!optimal.summary.complete || optimal.summary.queries_attempted != 4U ||
      optimal_context.query_count != 4U ||
      optimal.unsigned_values[0].value != 2560U ||
      optimal.unsigned_values[1].value != 1440U) {
    return fail("optimal-settings input capture failed");
  }
  const std::array<std::string_view, 4> optimal_keys{
      "Width", "Height", "PerfQualityValue", "RTXValue"};
  for (std::size_t i = 0; i < optimal_keys.size(); ++i) {
    const auto kind =
        i < 2 ? GetterKind::unsigned_value : GetterKind::signed_value;
    if (optimal_context.queries[i].key != optimal_keys[i] ||
        optimal_context.queries[i].kind != kind) {
      return fail("optimal-settings callback crossed its four-input contract");
    }
  }
  for (std::size_t fault = 0; fault < 4U; ++fault) {
    MockContext fault_context{};
    fault_context.fault_at = fault;
    const auto result =
        capture_optimal_settings_contract(callbacks_for(fault_context));
    if (result.summary.complete || !result.summary.fault_latched ||
        fault_context.query_count != fault + 1U ||
        result.summary.stop_reason != CaptureStopReason::callback_fault) {
      return fail("optimal-settings getter fault did not stop later queries");
    }
  }
  const auto no_optimal_callbacks = capture_optimal_settings_contract({});
  if (no_optimal_callbacks.summary.complete ||
      no_optimal_callbacks.summary.queries_attempted != 0U ||
      no_optimal_callbacks.summary.stop_reason !=
          CaptureStopReason::callback_missing) {
    return fail("missing optimal-settings getter was accepted");
  }
  using mhw::bridge_probe::CreateSignedKey;
  using mhw::bridge_probe::CreateUnsignedKey;
  using mhw::bridge_probe::EvaluateFloatKey;
  using mhw::bridge_probe::EvaluateResourceKey;
  using mhw::bridge_probe::EvaluateSignedKey;
  using mhw::bridge_probe::capture_create_contract;
  using mhw::bridge_probe::capture_evaluate_contract;
  using mhw::bridge_probe::kContractQueryCapacity;
  using mhw::bridge_probe::kCreateQueryCount;
  using mhw::bridge_probe::kCreateSignedKeyCount;
  using mhw::bridge_probe::kCreateUnsignedKeyCount;
  using mhw::bridge_probe::kEvaluateFloatKeyCount;
  using mhw::bridge_probe::kEvaluateQueryCount;
  using mhw::bridge_probe::kEvaluateResourceKeyCount;
  using mhw::bridge_probe::kEvaluateSignedKeyCount;
  using mhw::bridge_probe::key_name;

  static_assert(kCreateUnsignedKeyCount == 8U);
  static_assert(kCreateSignedKeyCount == 3U);
  static_assert(kEvaluateResourceKeyCount == 5U);
  static_assert(kEvaluateFloatKeyCount == 6U);
  static_assert(kEvaluateSignedKeyCount == 6U);
  static_assert(kCreateQueryCount == 11U);
  static_assert(kEvaluateQueryCount == 17U);
  static_assert(kContractQueryCapacity == 28U);
  static_assert(std::is_trivially_copyable_v<CaptureState>);
  static_assert(std::is_trivially_copyable_v<CreateContractSnapshot>);
  static_assert(std::is_trivially_copyable_v<EvaluateContractSnapshot>);
  static_assert(std::is_standard_layout_v<CreateContractSnapshot>);
  static_assert(std::is_standard_layout_v<EvaluateContractSnapshot>);

  constexpr std::array<std::string_view, kCreateUnsignedKeyCount>
      expected_create_unsigned_names{
          "Width",          "Height",         "OutWidth",
          "OutHeight",      "ResourceWidth",  "ResourceHeight",
          "CreationNodeMask", "VisibilityNodeMask",
      };
  constexpr std::array<std::string_view, kCreateSignedKeyCount>
      expected_create_signed_names{
          "PerfQualityValue", "RTXValue", "Hint.HDR"};
  constexpr std::array<std::string_view, kEvaluateResourceKeyCount>
      expected_resource_names{
          "Color", "MotionVectors", "PreviousOutput", "Depth", "Output"};
  constexpr std::array<std::string_view, kEvaluateFloatKeyCount>
      expected_float_names{"Sharpness", "MV.Scale.X",  "MV.Scale.Y",
                           "MV.Offset.X", "MV.Offset.Y", "HDRClamp"};
  constexpr std::array<std::string_view, kEvaluateSignedKeyCount>
      expected_evaluate_signed_names{
          "DepthInverted",   "Reset",        "DLAAMode",
          "Hint.RunPrePass", "Hint.RunPass", "Hint.RunPostPass"};

  for (std::size_t index = 0U; index < expected_create_unsigned_names.size();
       ++index) {
    if (key_name(static_cast<CreateUnsignedKey>(index)) !=
        expected_create_unsigned_names[index]) {
      return fail("Create unsigned key spelling/order changed");
    }
  }
  for (std::size_t index = 0U; index < expected_create_signed_names.size();
       ++index) {
    if (key_name(static_cast<CreateSignedKey>(index)) !=
        expected_create_signed_names[index]) {
      return fail("Create signed key spelling/order changed");
    }
  }
  for (std::size_t index = 0U; index < expected_resource_names.size();
       ++index) {
    if (key_name(static_cast<EvaluateResourceKey>(index)) !=
        expected_resource_names[index]) {
      return fail("Evaluate resource key spelling/order changed");
    }
  }
  for (std::size_t index = 0U; index < expected_float_names.size(); ++index) {
    if (key_name(static_cast<EvaluateFloatKey>(index)) !=
        expected_float_names[index]) {
      return fail("Evaluate float key spelling/order changed");
    }
  }
  for (std::size_t index = 0U;
       index < expected_evaluate_signed_names.size(); ++index) {
    if (key_name(static_cast<EvaluateSignedKey>(index)) !=
        expected_evaluate_signed_names[index]) {
      return fail("Evaluate signed key spelling/order changed");
    }
  }
  if (!key_name(CreateUnsignedKey::count).empty() ||
      !key_name(CreateSignedKey::count).empty() ||
      !key_name(EvaluateResourceKey::count).empty() ||
      !key_name(EvaluateFloatKey::count).empty() ||
      !key_name(EvaluateSignedKey::count).empty()) {
    return fail("sentinel key unexpectedly has a query name");
  }

  MockContext exact_context{};
  const GetterCallbacks exact_callbacks = callbacks_for(exact_context);
  CaptureState exact_state{};
  const CreateContractSnapshot create =
      capture_create_contract(exact_state, exact_callbacks);
  if (!create.summary.complete || create.summary.fault_latched ||
      create.summary.stop_reason != CaptureStopReason::none ||
      create.summary.queries_attempted != kCreateQueryCount ||
      exact_state.query_count != kCreateQueryCount) {
    return fail("complete Create contract capture failed");
  }

  constexpr std::array<std::uint32_t, kCreateUnsignedKeyCount>
      expected_create_unsigned_values{2560U, 1440U, 5120U, 2880U,
                                      2560U, 1440U, 1U,    3U};
  for (std::size_t index = 0U; index < create.unsigned_values.size(); ++index) {
    const auto &capture = create.unsigned_values[index];
    if (capture.key != static_cast<CreateUnsignedKey>(index) ||
        capture.getter_result != 0x11000000U + index ||
        capture.value != expected_create_unsigned_values[index] ||
        !capture.query_attempted || !capture.callback_completed ||
        !capture.present) {
      return fail("Create unsigned value/result was not preserved");
    }
  }
  constexpr std::array<std::int32_t, kCreateSignedKeyCount>
      expected_create_signed_values{2, 1, 1};
  for (std::size_t index = 0U; index < create.signed_values.size(); ++index) {
    const auto &capture = create.signed_values[index];
    const std::uint32_t expected_result =
        0x22000000U + static_cast<std::uint32_t>(8U + index);
    if (capture.key != static_cast<CreateSignedKey>(index) ||
        capture.getter_result != expected_result ||
        capture.value != expected_create_signed_values[index] ||
        !capture.query_attempted || !capture.callback_completed ||
        !capture.present) {
      return fail("Create signed value/result was not preserved");
    }
  }

  const EvaluateContractSnapshot evaluate =
      capture_evaluate_contract(exact_state, exact_callbacks);
  if (!evaluate.summary.complete || evaluate.summary.fault_latched ||
      evaluate.summary.stop_reason != CaptureStopReason::none ||
      evaluate.summary.queries_attempted != kEvaluateQueryCount ||
      exact_state.query_count != kContractQueryCapacity ||
      exact_context.query_count != kContractQueryCapacity) {
    return fail("complete Evaluate contract capture failed");
  }

  constexpr std::array<std::uintptr_t, kEvaluateResourceKeyCount>
      expected_resources{0x1000U, 0U, 0x3000U, 0x4000U, 0x5000U};
  for (std::size_t index = 0U; index < evaluate.resources.size(); ++index) {
    const auto &capture = evaluate.resources[index];
    const std::uint32_t expected_result =
        0x44000000U + static_cast<std::uint32_t>(11U + index);
    if (capture.key != static_cast<EvaluateResourceKey>(index) ||
        capture.getter_result != expected_result ||
        capture.value != expected_resources[index] ||
        capture.value_is_null != (expected_resources[index] == 0U) ||
        !capture.query_attempted || !capture.callback_completed ||
        !capture.present) {
      return fail("Evaluate resource value/result/nullness was not preserved");
    }
  }
  constexpr std::array<float, kEvaluateFloatKeyCount> expected_floats{
      0.25F, -0.5F, 0.5F, 1.0F, -1.0F, 4.0F};
  for (std::size_t index = 0U; index < evaluate.floats.size(); ++index) {
    const auto &capture = evaluate.floats[index];
    const std::uint32_t expected_result =
        0x33000000U + static_cast<std::uint32_t>(16U + index);
    if (capture.key != static_cast<EvaluateFloatKey>(index) ||
        capture.getter_result != expected_result ||
        capture.value != expected_floats[index] ||
        !capture.query_attempted || !capture.callback_completed ||
        !capture.present) {
      return fail("Evaluate float value/result was not preserved");
    }
  }
  constexpr std::array<std::int32_t, kEvaluateSignedKeyCount>
      expected_evaluate_signed_values{1, 1, 0, 1, 1, 0};
  for (std::size_t index = 0U; index < evaluate.signed_values.size(); ++index) {
    const auto &capture = evaluate.signed_values[index];
    const std::uint32_t expected_result =
        0x22000000U + static_cast<std::uint32_t>(22U + index);
    if (capture.key != static_cast<EvaluateSignedKey>(index) ||
        capture.getter_result != expected_result ||
        capture.value != expected_evaluate_signed_values[index] ||
        !capture.query_attempted || !capture.callback_completed ||
        !capture.present) {
      return fail("Evaluate signed value/result was not preserved");
    }
  }

  std::array<QueryRecord, kContractQueryCapacity> expected_order{};
  std::size_t expected_index{};
  for (const std::string_view name : expected_create_unsigned_names) {
    expected_order[expected_index++] = {GetterKind::unsigned_value, name};
  }
  for (const std::string_view name : expected_create_signed_names) {
    expected_order[expected_index++] = {GetterKind::signed_value, name};
  }
  for (const std::string_view name : expected_resource_names) {
    expected_order[expected_index++] = {GetterKind::resource, name};
  }
  for (const std::string_view name : expected_float_names) {
    expected_order[expected_index++] = {GetterKind::float_value, name};
  }
  for (const std::string_view name : expected_evaluate_signed_names) {
    expected_order[expected_index++] = {GetterKind::signed_value, name};
  }
  if (expected_index != kContractQueryCapacity) {
    return fail("test query-order fixture has the wrong size");
  }
  for (std::size_t index = 0U; index < expected_order.size(); ++index) {
    if (exact_context.queries[index].kind != expected_order[index].kind ||
        exact_context.queries[index].key != expected_order[index].key) {
      return fail("getter query order changed");
    }
  }

  MockContext missing_context{};
  missing_context.return_missing_keys = true;
  const GetterCallbacks missing_callbacks = callbacks_for(missing_context);
  CaptureState missing_state{};
  const EvaluateContractSnapshot missing =
      capture_evaluate_contract(missing_state, missing_callbacks);
  const auto &missing_previous = missing.resources[2];
  const auto &missing_hdr_clamp = missing.floats[5];
  const auto &missing_dlaa = missing.signed_values[2];
  if (!missing.summary.complete || missing.summary.fault_latched ||
      missing_state.fault_latched ||
      missing.summary.queries_attempted != kEvaluateQueryCount ||
      !missing_previous.callback_completed || missing_previous.present ||
      missing_previous.getter_result != 0xE0010001U ||
      missing_previous.value != 0xDEADU || missing_previous.value_is_null ||
      !missing_hdr_clamp.callback_completed || missing_hdr_clamp.present ||
      missing_hdr_clamp.getter_result != 0xE0020001U ||
      missing_hdr_clamp.value != 99.0F ||
      !missing_dlaa.callback_completed || missing_dlaa.present ||
      missing_dlaa.getter_result != 0xE0030001U ||
      missing_dlaa.value != -77) {
    return fail("missing keys were not preserved as non-faulting observations");
  }

  MockContext fault_context{};
  fault_context.fault_at = 2U;
  const GetterCallbacks fault_callbacks = callbacks_for(fault_context);
  CaptureState fault_state{};
  const CreateContractSnapshot faulted_create =
      capture_create_contract(fault_state, fault_callbacks);
  if (faulted_create.summary.complete ||
      !faulted_create.summary.fault_latched ||
      faulted_create.summary.stop_reason !=
          CaptureStopReason::callback_fault ||
      faulted_create.summary.queries_attempted != 3U ||
      fault_context.query_count != 3U || fault_state.query_count != 3U ||
      !faulted_create.unsigned_values[0].callback_completed ||
      !faulted_create.unsigned_values[1].callback_completed ||
      !faulted_create.unsigned_values[2].query_attempted ||
      faulted_create.unsigned_values[2].callback_completed ||
      !faulted_create.unsigned_values[2].present ||
      faulted_create.unsigned_values[2].getter_result != 0x11000002U ||
      faulted_create.unsigned_values[2].value != 5120U ||
      faulted_create.unsigned_values[3].query_attempted ||
      faulted_create.signed_values[0].query_attempted) {
    return fail("first callback fault was not captured and latched");
  }
  const EvaluateContractSnapshot after_fault =
      capture_evaluate_contract(fault_state, fault_callbacks);
  if (after_fault.summary.complete || !after_fault.summary.fault_latched ||
      after_fault.summary.stop_reason != CaptureStopReason::callback_fault ||
      after_fault.summary.queries_attempted != 0U ||
      fault_context.query_count != 3U ||
      after_fault.resources[0].key != EvaluateResourceKey::color ||
      after_fault.resources[0].query_attempted ||
      after_fault.floats[0].query_attempted ||
      after_fault.signed_values[0].query_attempted) {
    return fail("future queries were not suppressed after a callback fault");
  }

  MockContext missing_callback_context{};
  GetterCallbacks missing_callback = callbacks_for(missing_callback_context);
  missing_callback.get_float = nullptr;
  CaptureState missing_callback_state{};
  const EvaluateContractSnapshot callback_missing =
      capture_evaluate_contract(missing_callback_state, missing_callback);
  if (callback_missing.summary.complete ||
      callback_missing.summary.stop_reason !=
          CaptureStopReason::callback_missing ||
      callback_missing.summary.queries_attempted !=
          kEvaluateResourceKeyCount ||
      missing_callback_context.query_count != kEvaluateResourceKeyCount ||
      !callback_missing.resources.back().query_attempted ||
      callback_missing.floats.front().query_attempted ||
      callback_missing.signed_values.front().query_attempted) {
    return fail("missing callback did not latch before later getter groups");
  }

  const std::size_t calls_before_overflow = exact_context.query_count;
  const CreateContractSnapshot over_capacity =
      capture_create_contract(exact_state, exact_callbacks);
  if (over_capacity.summary.complete ||
      !over_capacity.summary.fault_latched ||
      over_capacity.summary.stop_reason !=
          CaptureStopReason::query_capacity_exhausted ||
      over_capacity.summary.queries_attempted != 0U ||
      exact_state.query_count != kContractQueryCapacity ||
      exact_context.query_count != calls_before_overflow ||
      over_capacity.unsigned_values.front().query_attempted ||
      over_capacity.unsigned_values.front().key != CreateUnsignedKey::width ||
      over_capacity.signed_values.front().key !=
          CreateSignedKey::perf_quality_value) {
    return fail("query capacity did not latch without invoking a callback");
  }

  MockContext partial_capacity_context{};
  const GetterCallbacks partial_capacity_callbacks =
      callbacks_for(partial_capacity_context);
  CaptureState partial_capacity_state{};
  partial_capacity_state.query_count = kContractQueryCapacity - 1U;
  const CreateContractSnapshot partial_capacity = capture_create_contract(
      partial_capacity_state, partial_capacity_callbacks);
  if (partial_capacity.summary.queries_attempted != 1U ||
      partial_capacity.summary.stop_reason !=
          CaptureStopReason::query_capacity_exhausted ||
      partial_capacity_context.query_count != 1U ||
      !partial_capacity.unsigned_values[0].query_attempted ||
      partial_capacity.unsigned_values[1].query_attempted ||
      partial_capacity_state.query_count != kContractQueryCapacity) {
    return fail("capacity was not enforced at the exact query boundary");
  }

  MockContext invalid_state_context{};
  const GetterCallbacks invalid_state_callbacks =
      callbacks_for(invalid_state_context);
  CaptureState invalid_state{};
  invalid_state.query_count = kContractQueryCapacity + 1U;
  const CreateContractSnapshot invalid =
      capture_create_contract(invalid_state, invalid_state_callbacks);
  if (invalid.summary.complete || !invalid.summary.fault_latched ||
      invalid.summary.stop_reason != CaptureStopReason::invalid_state ||
      invalid.summary.queries_attempted != 0U ||
      invalid_state_context.query_count != 0U) {
    return fail("corrupt capture state did not fail closed");
  }

  std::cout << "contract snapshot tests passed\n";
  return 0;
}
