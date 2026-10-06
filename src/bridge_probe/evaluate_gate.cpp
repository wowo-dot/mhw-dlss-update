#include "evaluate_gate.hpp"
#include "optimal_settings.hpp"

namespace mhw::bridge_probe {

bool sizing_lifecycle_ready(Mode mode, LifecycleState state,
                            bool token_outstanding) noexcept {
  if (reusable_fallback_mode(mode)) {
    return (create_lifecycle_ready(mode, state) && !token_outstanding) ||
           ((state == LifecycleState::evaluate_token_issued ||
             (mode==Mode::native_depth_probe && state==LifecycleState::evaluate_captured)) && token_outstanding);
  }
  return state == LifecycleState::populated ||
         (evaluate_limit(mode) && token_outstanding &&
          (state == LifecycleState::evaluate_token_issued ||
           (mode == Mode::capture_three_evaluates && state == LifecycleState::evaluate_captured)));
}

bool core_create_record_matches(
    const CoreCreateRecord &record, std::uint64_t expected_token,
    std::uint64_t expected_creation_command_list, Mode mode) noexcept {
  const bool token_valid = reusable_fallback_mode(mode)
      ? is_fallback_generation_token(expected_token)
      : expected_token == diagnostic_token(ProviderId::dlss);
  return token_valid &&
         expected_creation_command_list != 0U && record.operation == 1U &&
         record.reserved == 0U && record.parameter_clone != 0U &&
         record.creation_command_list == expected_creation_command_list &&
         record.token == expected_token;
}

bool create_contract_allows_evaluate(
    const CreateContractSnapshot &snapshot) noexcept {
  if (!snapshot.summary.complete || snapshot.summary.fault_latched ||
      snapshot.summary.stop_reason != CaptureStopReason::none ||
      snapshot.summary.queries_attempted != kCreateQueryCount)
    return false;
  for (const auto key :
       {CreateUnsignedKey::width, CreateUnsignedKey::height,
        CreateUnsignedKey::out_width, CreateUnsignedKey::out_height,
        CreateUnsignedKey::creation_node_mask,
        CreateUnsignedKey::visibility_node_mask}) {
    const auto &value = snapshot.unsigned_values[static_cast<std::size_t>(key)];
    if (value.key != key || !value.query_attempted ||
        !value.callback_completed || !value.present ||
        value.getter_result != 1U)
      return false;
  }
  for (std::size_t index = 0; index < snapshot.signed_values.size(); ++index) {
    const auto &value = snapshot.signed_values[index];
    if (value.key != static_cast<CreateSignedKey>(index) ||
        !value.query_attempted || !value.callback_completed || !value.present ||
        value.getter_result != 1U)
      return false;
  }
  const auto width = snapshot.unsigned_values[0].value;
  const auto height = snapshot.unsigned_values[1].value;
  return width > 0U && width <= kOptimalSettingsMaxDimension && height > 0U &&
         height <= kOptimalSettingsMaxDimension &&
         snapshot.unsigned_values[2].value == width &&
         snapshot.unsigned_values[3].value == height &&
         snapshot.unsigned_values[6].value == 1U &&
         snapshot.unsigned_values[7].value == 1U &&
         snapshot.signed_values[0].value >= 0 &&
         snapshot.signed_values[0].value <= 2 &&
         snapshot.signed_values[1].value == 0 &&
         snapshot.signed_values[2].value == 1;
}

} // namespace mhw::bridge_probe
