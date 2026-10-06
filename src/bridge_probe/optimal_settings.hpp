#pragma once

#include "contract_snapshot.hpp"
#include "bridge_policy.hpp"

namespace mhw::bridge_probe {

// Diagnostic limits, not vendor capability claims. No resolution change or GPU
// allocation is performed. The runtime admits only the pinned host callback.
inline constexpr std::size_t kOptimalSettingsCallCapacity = 16U;
inline constexpr std::uint32_t kOptimalSettingsMaxDimension = 16'384U;
constexpr bool
optimal_settings_call_within_budget(std::uint64_t ordinal) noexcept {
  return ordinal != 0U && ordinal <= kOptimalSettingsCallCapacity;
}

enum class OptimalSettingsCallPolicy { reject, capture, unrecorded };

// Fallback operation must not end when its bounded diagnostic slots are full.
// Every unrecorded call still needs the same caller, lifecycle and output checks.
constexpr OptimalSettingsCallPolicy
optimal_settings_call_policy(Mode mode, std::uint64_t ordinal) noexcept {
  if (mode == Mode::disabled || ordinal == 0U)
    return OptimalSettingsCallPolicy::reject;
  if (optimal_settings_call_within_budget(ordinal))
    return OptimalSettingsCallPolicy::capture;
  return reusable_fallback_mode(mode)
             ? OptimalSettingsCallPolicy::unrecorded
             : OptimalSettingsCallPolicy::reject;
}

using UnsignedSetter = bool (*)(void *, std::string_view,
                                std::uint32_t) noexcept;
using SignedSetter = bool (*)(void *, std::string_view, std::int32_t) noexcept;
using FloatSetter = bool (*)(void *, std::string_view, float) noexcept;
struct SetterCallbacks {
  void *context{};
  UnsignedSetter set_unsigned{};
  SignedSetter set_signed{};
  FloatSetter set_float{};
};

enum class OptimalSettingsDecision : std::uint8_t {
  admission_rejected,
  invalid_input,
  missing_accessor,
  setter_fault,
  readback_fault,
  readback_mismatch,
  success,
};
std::string_view decision_name(OptimalSettingsDecision decision) noexcept;

struct OptimalSettingsPublication {
  OptimalSettingsDecision decision{OptimalSettingsDecision::invalid_input};
  std::uint32_t out_width{};
  std::uint32_t out_height{};
  std::int32_t dlss_mode{};
  float sharpness{};
  std::uint32_t writes_attempted{};
  std::uint32_t writes_completed{};
  std::uint32_t readbacks_attempted{};
  std::uint32_t readbacks_verified{};
};

// Each void vendor setter must be adapted to true only on normal return.
// Read back each value before writing the next; DLSSMode is published last.
// The host ignores all output metadata when the callback returns failure.
OptimalSettingsPublication
publish_diagnostic_optimal_settings(const OptimalSettingsSnapshot &inputs,
                                    const GetterCallbacks &getters,
                                    const SetterCallbacks &setters) noexcept;

} // namespace mhw::bridge_probe
