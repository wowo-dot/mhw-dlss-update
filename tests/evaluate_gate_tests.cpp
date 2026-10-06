#include "evaluate_gate.hpp"
#include <iostream>

namespace {
namespace bridge = mhw::bridge_probe;
int fail(const char *message) {
  std::cerr << message << '\n';
  return 1;
}
bridge::CreateContractSnapshot accepted_create() {
  bridge::CreateContractSnapshot snapshot{};
  const std::uint32_t values[]{2560, 1440, 2560, 1440, 0, 0, 1, 1};
  for (std::size_t index = 0; index < 8; ++index) {
    const bool present = index != 4 && index != 5;
    snapshot.unsigned_values[index] = {
        static_cast<bridge::CreateUnsignedKey>(index),
        present ? 1U : 0xBAD00010U,
        values[index],
        true,
        true,
        present};
  }
  for (std::size_t index = 0; index < 3; ++index)
    snapshot.signed_values[index] = {
        static_cast<bridge::CreateSignedKey>(index),
        1U,
        index == 2 ? 1 : 0,
        true,
        true,
        true};
  snapshot.summary = {11, bridge::CaptureStopReason::none, true, false};
  return snapshot;
}
} // namespace

int main() {
  using bridge::LifecycleState;
  using bridge::Mode;
  using bridge::ProviderId;
  constexpr auto token = bridge::diagnostic_token(ProviderId::dlss);
  static_assert(token == 0x000000010000B12DULL);
  static_assert((bridge::diagnostic_token(ProviderId::isp) >> 32U) == 9U);
  static_assert(bridge::diagnostic_token(ProviderId::count) == 0U);
  static_assert(
      bridge::provider_mode(Mode::capture_one_evaluate, ProviderId::dlss) ==
      Mode::capture_one_evaluate);
  static_assert(bridge::provider_mode(Mode::capture_one_evaluate,
                                      ProviderId::isp) == Mode::capture_create);
  static_assert(bridge::provider_mode(Mode::capture_create, ProviderId::isp) ==
                Mode::capture_create);
  static_assert(bridge::provider_mode(Mode::disabled, ProviderId::dlss) ==
                Mode::disabled);
  static_assert(bridge::provider_mode(Mode::capture_one_evaluate,
                                      ProviderId::count) == Mode::disabled);
  static_assert(bridge::copied_handle_address_matches(0x2000, 0x1000, 0));
  static_assert(bridge::copied_handle_address_matches(0x2000, 0x1000, 0x2000));
  static_assert(!bridge::copied_handle_address_matches(0x1000, 0x1000, 0));
  static_assert(!bridge::copied_handle_address_matches(0x3000, 0x1000, 0x2000));
  static_assert(!bridge::copied_handle_address_matches(0, 0x1000, 0));
  for (const auto state : {LifecycleState::disabled, LifecycleState::populated,
                           LifecycleState::evaluate_token_issued,
                           LifecycleState::evaluate_captured,
                           LifecycleState::released, LifecycleState::inert}) {
    for (const bool outstanding : {false, true}) {
      const bool expected =
          state == LifecycleState::populated ||
          (state == LifecycleState::evaluate_token_issued && outstanding);
      if (bridge::sizing_lifecycle_ready(Mode::capture_one_evaluate, state,
                                         outstanding) != expected ||
          bridge::sizing_lifecycle_ready(Mode::capture_create, state,
                                         outstanding) !=
              (state == LifecycleState::populated))
        return fail("sizing must stop after Evaluate and require outstanding "
                    "ownership before it");
    }
  }
  const bridge::CoreCreateRecord good{1, 0, 0x3000, 0x4000, token};
  if (!bridge::core_create_record_matches(good, token, 0x4000))
    return fail("valid copied core record rejected");
  for (int changed = 0; changed < 9; ++changed) {
    auto record = good;
    auto expected_token = token;
    std::uint64_t expected_command = 0x4000;
    if (changed == 0)
      record.operation = 2;
    if (changed == 1)
      record.reserved = 1;
    if (changed == 2)
      record.parameter_clone = 0;
    if (changed == 3)
      record.creation_command_list = 0x5000;
    if (changed == 4)
      record.token = bridge::diagnostic_token(ProviderId::isp);
    if (changed == 5)
      ++record.token;
    if (changed == 6)
      expected_token = 0;
    if (changed == 7)
      expected_token = bridge::diagnostic_token(ProviderId::isp);
    if (changed == 8)
      expected_command = 0;
    if (bridge::core_create_record_matches(record, expected_token,
                                           expected_command))
      return fail("mismatched copied handle record accepted");
  }
  if (!bridge::create_contract_allows_evaluate(accepted_create()))
    return fail("accepted Create with absent ResourceWidth/Height must allow "
                "diagnostic Evaluate");
  for (std::size_t index = 0; index < 8; ++index) {
    if (index == 4 || index == 5)
      continue;
    auto snapshot = accepted_create();
    snapshot.unsigned_values[index].present = false;
    if (bridge::create_contract_allows_evaluate(snapshot))
      return fail("missing required Create field admitted");
  }
  for (std::size_t index = 0; index < 3; ++index) {
    auto snapshot = accepted_create();
    snapshot.signed_values[index].getter_result = 0xBAD00010U;
    if (bridge::create_contract_allows_evaluate(snapshot))
      return fail("failed signed Create getter admitted");
  }
  for (int changed = 0; changed < 12; ++changed) {
    auto snapshot = accepted_create();
    if (changed == 0)
      snapshot.summary.complete = false;
    if (changed == 1)
      snapshot.summary.fault_latched = true;
    if (changed == 2)
      snapshot.summary.queries_attempted = 10;
    if (changed == 3)
      snapshot.unsigned_values[0].value = 0;
    if (changed == 4)
      snapshot.unsigned_values[1].value = 16385;
    if (changed == 5)
      snapshot.unsigned_values[2].value = 5120;
    if (changed == 6)
      snapshot.unsigned_values[3].value = 720;
    if (changed == 7)
      snapshot.unsigned_values[6].value = 0;
    if (changed == 8)
      snapshot.unsigned_values[7].value = 2;
    if (changed == 9)
      snapshot.signed_values[0].value = 3;
    if (changed == 10)
      snapshot.signed_values[1].value = 1;
    if (changed == 11)
      snapshot.signed_values[2].value = 0;
    if (bridge::create_contract_allows_evaluate(snapshot))
      return fail("unsupported Create contract admitted");
  }
  std::cout
      << "Copied handle, provider mode and Evaluate admission checks passed\n";
  return 0;
}
