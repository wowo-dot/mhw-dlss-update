#include "optimal_settings.hpp"

namespace mhw::bridge_probe {
namespace {

template <typename Capture, typename Key>
bool valid_input(const Capture &capture, const Key key) noexcept {
  return capture.key == key && capture.query_attempted &&
         capture.callback_completed && capture.present &&
         capture.getter_result == 1U;
}

bool valid_inputs(const OptimalSettingsSnapshot &inputs) noexcept {
  return inputs.summary.complete && !inputs.summary.fault_latched &&
         inputs.summary.stop_reason == CaptureStopReason::none &&
         inputs.summary.queries_attempted == 4U &&
         valid_input(inputs.unsigned_values[0], CreateUnsignedKey::width) &&
         valid_input(inputs.unsigned_values[1], CreateUnsignedKey::height) &&
         valid_input(inputs.signed_values[0],
                     CreateSignedKey::perf_quality_value) &&
         valid_input(inputs.signed_values[1], CreateSignedKey::rtx_value) &&
         inputs.unsigned_values[0].value > 0U &&
         inputs.unsigned_values[0].value <= kOptimalSettingsMaxDimension &&
         inputs.unsigned_values[1].value > 0U &&
         inputs.unsigned_values[1].value <= kOptimalSettingsMaxDimension &&
         inputs.signed_values[0].value >= 0 &&
         inputs.signed_values[0].value <= 2 &&
         inputs.signed_values[1].value == 0;
}

template <typename Value, typename Setter, typename Getter>
bool write_and_verify(OptimalSettingsPublication &publication,
                      const SetterCallbacks &setters,
                      const GetterCallbacks &getters, const Setter setter,
                      const Getter getter, const std::string_view key,
                      const Value value) noexcept {
  ++publication.writes_attempted;
  if (!setter(setters.context, key, value)) {
    publication.decision = OptimalSettingsDecision::setter_fault;
    return false;
  }
  ++publication.writes_completed;
  ++publication.readbacks_attempted;
  const auto reply = getter(getters.context, key);
  if (!reply.callback_completed) {
    publication.decision = OptimalSettingsDecision::readback_fault;
    return false;
  }
  if (!reply.present || reply.getter_result != 1U || reply.value != value) {
    publication.decision = OptimalSettingsDecision::readback_mismatch;
    return false;
  }
  ++publication.readbacks_verified;
  return true;
}

} // namespace

std::string_view
decision_name(const OptimalSettingsDecision decision) noexcept {
  switch (decision) {
  case OptimalSettingsDecision::admission_rejected:
    return "admission-rejected";
  case OptimalSettingsDecision::invalid_input:
    return "invalid-input";
  case OptimalSettingsDecision::missing_accessor:
    return "missing-accessor";
  case OptimalSettingsDecision::setter_fault:
    return "setter-fault";
  case OptimalSettingsDecision::readback_fault:
    return "readback-fault";
  case OptimalSettingsDecision::readback_mismatch:
    return "readback-mismatch";
  case OptimalSettingsDecision::success:
    return "success";
  }
  return "unknown";
}

OptimalSettingsPublication
publish_diagnostic_optimal_settings(const OptimalSettingsSnapshot &inputs,
                                    const GetterCallbacks &getters,
                                    const SetterCallbacks &setters) noexcept {
  OptimalSettingsPublication publication{};
  if (!valid_inputs(inputs)) {
    return publication;
  }
  if (getters.get_unsigned == nullptr || getters.get_signed == nullptr ||
      getters.get_float == nullptr || setters.set_unsigned == nullptr ||
      setters.set_signed == nullptr || setters.set_float == nullptr) {
    publication.decision = OptimalSettingsDecision::missing_accessor;
    return publication;
  }
  publication.out_width = inputs.unsigned_values[0].value;
  publication.out_height = inputs.unsigned_values[1].value;
  // The captured MHW consumer turns mode 1 with scale 1 into DLSS-only flags.
  // This is diagnostic routing metadata, not a claim of DLAA support.
  publication.dlss_mode = 1;
  publication.sharpness = 0.0F;
  if (write_and_verify(publication, setters, getters, setters.set_unsigned,
                       getters.get_unsigned, "OutWidth",
                       publication.out_width) &&
      write_and_verify(publication, setters, getters, setters.set_unsigned,
                       getters.get_unsigned, "OutHeight",
                       publication.out_height) &&
      write_and_verify(publication, setters, getters, setters.set_float,
                       getters.get_float, "Sharpness", publication.sharpness) &&
      write_and_verify(publication, setters, getters, setters.set_signed,
                       getters.get_signed, "DLSSMode", publication.dlss_mode)) {
    publication.decision = OptimalSettingsDecision::success;
  }
  return publication;
}

} // namespace mhw::bridge_probe
