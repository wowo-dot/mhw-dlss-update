#include "optimal_settings.hpp"

#include <array>
#include <iostream>
#include <limits>

namespace {
namespace bridge = mhw::bridge_probe;
struct Context {
  std::array<std::string_view, 4> writes{};
  std::array<std::string_view, 4> reads{};
  std::size_t write_count{};
  std::size_t read_count{};
  std::size_t fail_write{4};
  std::size_t fail_read{4};
  std::size_t mismatch_read{4};
  std::size_t missing_read{4};
  std::size_t error_read{4};
  std::uint32_t width{};
  std::uint32_t height{};
  std::int32_t mode{};
  float sharpness{1.0F};
};

bool record_write(Context &context, const std::string_view key) noexcept {
  const auto index = context.write_count++;
  if (index >= context.writes.size())
    return false;
  context.writes[index] = key;
  return index != context.fail_write;
}
bool set_unsigned(void *opaque, std::string_view key,
                  std::uint32_t value) noexcept {
  auto &context = *static_cast<Context *>(opaque);
  if (!record_write(context, key))
    return false;
  if (key == "OutWidth")
    context.width = value;
  else if (key == "OutHeight")
    context.height = value;
  else
    return false;
  return true;
}
bool set_signed(void *opaque, std::string_view key,
                std::int32_t value) noexcept {
  auto &context = *static_cast<Context *>(opaque);
  if (!record_write(context, key) || key != "DLSSMode")
    return false;
  context.mode = value;
  return true;
}
bool set_float(void *opaque, std::string_view key, float value) noexcept {
  auto &context = *static_cast<Context *>(opaque);
  if (!record_write(context, key) || key != "Sharpness")
    return false;
  context.sharpness = value;
  return true;
}
template <typename Reply, typename Value>
Reply read(Context &context, std::string_view key, Value value) noexcept {
  const auto index = context.read_count++;
  if (index >= context.reads.size())
    return {};
  context.reads[index] = key;
  if (index == context.mismatch_read)
    value += static_cast<Value>(1);
  return {index == context.error_read ? 0xBAD00005U : 1U, value,
          index != context.fail_read, index != context.missing_read};
}
bridge::UnsignedGetterReply get_unsigned(void *opaque,
                                         std::string_view key) noexcept {
  auto &context = *static_cast<Context *>(opaque);
  return read<bridge::UnsignedGetterReply>(
      context, key, key == "OutWidth" ? context.width : context.height);
}
bridge::SignedGetterReply get_signed(void *opaque,
                                     std::string_view key) noexcept {
  auto &context = *static_cast<Context *>(opaque);
  return read<bridge::SignedGetterReply>(context, key, context.mode);
}
bridge::FloatGetterReply get_float(void *opaque,
                                   std::string_view key) noexcept {
  auto &context = *static_cast<Context *>(opaque);
  return read<bridge::FloatGetterReply>(context, key, context.sharpness);
}
bridge::GetterCallbacks getters(Context &context) noexcept {
  return {&context, get_unsigned, get_signed, get_float, nullptr};
}
bridge::SetterCallbacks setters(Context &context) noexcept {
  return {&context, set_unsigned, set_signed, set_float};
}
bridge::OptimalSettingsSnapshot inputs() noexcept {
  bridge::OptimalSettingsSnapshot value{};
  value.unsigned_values = {
      {{bridge::CreateUnsignedKey::width, 1U, 2560U, true, true, true},
       {bridge::CreateUnsignedKey::height, 1U, 1440U, true, true, true}}};
  value.signed_values = {
      {{bridge::CreateSignedKey::perf_quality_value, 1U, 0, true, true, true},
       {bridge::CreateSignedKey::rtx_value, 1U, 0, true, true, true}}};
  value.summary = {4U, bridge::CaptureStopReason::none, true, false};
  return value;
}
bool rejected_without_writes(const bridge::OptimalSettingsSnapshot &input) {
  Context context{};
  const auto result = bridge::publish_diagnostic_optimal_settings(
      input, getters(context), setters(context));
  return result.decision == bridge::OptimalSettingsDecision::invalid_input &&
         context.write_count == 0 && context.read_count == 0;
}
int fail(const char *message) {
  std::cerr << message << '\n';
  return 1;
}
} // namespace

int main() {
  using bridge::OptimalSettingsDecision;
  using bridge::publish_diagnostic_optimal_settings;
  static_assert(!bridge::optimal_settings_call_within_budget(0));
  static_assert(bridge::optimal_settings_call_within_budget(1));
  static_assert(bridge::optimal_settings_call_within_budget(16));
  static_assert(!bridge::optimal_settings_call_within_budget(17));
  // Replay the 21-query shape that disabled the v0.9.0 fallback on returning
  // to Options. Only the first 16 retain event slots; all 21 must still size.
  std::size_t retained{}, unrecorded{};
  for (std::uint64_t ordinal = 1; ordinal <= 21; ++ordinal) {
    const auto policy = bridge::optimal_settings_call_policy(
        bridge::Mode::native_taa_fallback, ordinal);
    if (policy == bridge::OptimalSettingsCallPolicy::reject)
      return fail("fallback stopped when its diagnostic slots filled");
    if (policy == bridge::OptimalSettingsCallPolicy::capture) ++retained;
    else ++unrecorded;
    Context context{};
    const auto result = publish_diagnostic_optimal_settings(
        inputs(), getters(context), setters(context));
    if (result.decision != OptimalSettingsDecision::success ||
        context.width != 2560 || context.height != 1440 ||
        result.writes_completed != 4 || result.readbacks_verified != 4)
      return fail("repeated sizing lost its validated 1:1 publication");
  }
  if (retained != 16 || unrecorded != 5)
    return fail("fallback diagnostic retention is not bounded to 16");
  for (const auto mode : {bridge::Mode::capture_create,
                          bridge::Mode::capture_one_evaluate,
                          bridge::Mode::capture_three_evaluates}) {
    if (bridge::optimal_settings_call_policy(mode, 16) != bridge::OptimalSettingsCallPolicy::capture ||
        bridge::optimal_settings_call_policy(mode, 17) != bridge::OptimalSettingsCallPolicy::reject)
      return fail("legacy capture-mode operational budget changed");
  }
  if (bridge::optimal_settings_call_policy(bridge::Mode::disabled, 1) != bridge::OptimalSettingsCallPolicy::reject ||
      bridge::optimal_settings_call_policy(bridge::Mode::native_taa_fallback, 0) != bridge::OptimalSettingsCallPolicy::reject)
    return fail("disabled or overflowed sizing was admitted");
  // Retention exhaustion never licenses invalid inputs or failed accessors.
  {
    auto invalid = inputs();
    invalid.unsigned_values[0].value = 0;
    if (!rejected_without_writes(invalid))
      return fail("invalid unrecorded sizing wrote outputs");
    Context context{}; context.fail_write = 0;
    const auto result = publish_diagnostic_optimal_settings(
        inputs(), getters(context), setters(context));
    if (result.decision != OptimalSettingsDecision::setter_fault || context.read_count != 0 || context.write_count != 1)
      return fail("unrecorded sizing ignored an accessor failure");
  }
  for (const auto dimensions : std::array<std::array<std::uint32_t, 2>, 4>{
           {{2560, 1440}, {5120, 1440}, {1, 1}, {16384, 16384}}}) {
    for (std::int32_t quality = 0; quality <= 2; ++quality) {
      auto input = inputs();
      input.unsigned_values[0].value = dimensions[0];
      input.unsigned_values[1].value = dimensions[1];
      input.signed_values[0].value = quality;
      Context context{};
      const auto result = publish_diagnostic_optimal_settings(
          input, getters(context), setters(context));
      constexpr std::array<std::string_view, 4> expected{
          "OutWidth", "OutHeight", "Sharpness", "DLSSMode"};
      if (result.decision != OptimalSettingsDecision::success ||
          context.width != dimensions[0] || context.height != dimensions[1] ||
          context.mode != 1 || context.sharpness != 0.0F ||
          context.writes != expected || context.reads != expected ||
          result.writes_attempted != 4 || result.writes_completed != 4 ||
          result.readbacks_attempted != 4 || result.readbacks_verified != 4)
        return fail("1:1 output values, types, order or readbacks differ");
    }
  }
  for (std::size_t index = 0; index < 4; ++index) {
    for (int failure = 0; failure < 5; ++failure) {
      Context context{};
      if (failure == 0)
        context.fail_write = index;
      if (failure == 1)
        context.fail_read = index;
      if (failure == 2)
        context.mismatch_read = index;
      if (failure == 3)
        context.missing_read = index;
      if (failure == 4)
        context.error_read = index;
      const auto result = publish_diagnostic_optimal_settings(
          inputs(), getters(context), setters(context));
      const auto expected = failure == 0 ? OptimalSettingsDecision::setter_fault
                            : failure == 1
                                ? OptimalSettingsDecision::readback_fault
                                : OptimalSettingsDecision::readback_mismatch;
      if (result.decision != expected || context.write_count != index + 1 ||
          context.read_count != index + (failure == 0 ? 0U : 1U) ||
          result.readbacks_verified != index)
        return fail("publication continued after a failed setter or readback");
    }
  }
  for (int missing = 0; missing < 6; ++missing) {
    Context context{};
    auto getter = getters(context);
    auto setter = setters(context);
    if (missing == 0)
      getter.get_unsigned = nullptr;
    if (missing == 1)
      getter.get_signed = nullptr;
    if (missing == 2)
      getter.get_float = nullptr;
    if (missing == 3)
      setter.set_unsigned = nullptr;
    if (missing == 4)
      setter.set_signed = nullptr;
    if (missing == 5)
      setter.set_float = nullptr;
    if (publish_diagnostic_optimal_settings(inputs(), getter, setter)
                .decision != OptimalSettingsDecision::missing_accessor ||
        context.write_count != 0)
      return fail("missing accessor was not rejected before publication");
  }
  for (int invalid = 0; invalid < 15; ++invalid) {
    auto input = inputs();
    if (invalid == 0)
      input.summary.complete = false;
    if (invalid == 1)
      input.summary.fault_latched = true;
    if (invalid == 2)
      input.summary.queries_attempted = 3;
    if (invalid == 3)
      input.summary.stop_reason = bridge::CaptureStopReason::callback_fault;
    if (invalid == 4)
      input.unsigned_values[0].value = 0;
    if (invalid == 5)
      input.unsigned_values[1].value = 0;
    if (invalid == 6)
      input.unsigned_values[0].value = 16385;
    if (invalid == 7)
      input.unsigned_values[1].value =
          std::numeric_limits<std::uint32_t>::max();
    if (invalid == 8)
      input.signed_values[0].value = -1;
    if (invalid == 9)
      input.signed_values[0].value = 3;
    if (invalid == 10)
      input.signed_values[1].value = 1;
    if (invalid == 11)
      input.unsigned_values[0].present = false;
    if (invalid == 12)
      input.unsigned_values[1].getter_result = 0xBAD00005U;
    if (invalid == 13)
      input.signed_values[0].callback_completed = false;
    if (invalid == 14)
      input.signed_values[1].key = bridge::CreateSignedKey::hint_hdr;
    if (!rejected_without_writes(input))
      return fail("invalid input allowed output writes");
  }
  std::cout << "Diagnostic sizing publication and failure boundaries passed\n";
  return 0;
}
