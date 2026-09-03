#include "bridge_policy.hpp"

#include <limits>

namespace mhw::bridge_probe {
namespace {

bool is_enabled_mode(const Mode mode) noexcept {
  return mode == Mode::capture_create ||
         mode == Mode::capture_one_evaluate;
}

} // namespace

ExportId classify_export_name(const std::string_view name) noexcept {
  if (name == kD3d12InitExport) {
    return ExportId::d3d12_init;
  }
  if (name == kPopulateParametersExport) {
    return ExportId::populate_parameters;
  }
  if (name == kD3d12GetScratchBufferSizeExport) {
    return ExportId::d3d12_get_scratch_buffer_size;
  }
  if (name == kD3d12CreateFeatureExport) {
    return ExportId::d3d12_create_feature;
  }
  if (name == kD3d12EvaluateFeatureExport) {
    return ExportId::d3d12_evaluate_feature;
  }
  if (name == kD3d12ReleaseFeatureExport) {
    return ExportId::d3d12_release_feature;
  }
  if (name == kD3d12ShutdownExport) {
    return ExportId::d3d12_shutdown;
  }
  if (name == kD3d12Shutdown1Export) {
    return ExportId::d3d12_shutdown1;
  }
  if (name == kD3d12CreateFeature1Export) {
    return ExportId::d3d12_create_feature1;
  }
  if (name == kD3d12InitExtExport) {
    return ExportId::d3d12_init_ext;
  }
  if (name == kD3d12InitExt1Export) {
    return ExportId::d3d12_init_ext1;
  }
  if (name == kD3d12InitExt2Export) {
    return ExportId::d3d12_init_ext2;
  }
  if (name == kD3d12PopulateParametersImplExport) {
    return ExportId::d3d12_populate_parameters_impl;
  }
  if (name == kD3d12PopulateDeviceParametersImplExport) {
    return ExportId::d3d12_populate_device_parameters_impl;
  }
  return ExportId::unknown;
}

std::string_view export_name(const ExportId export_id) noexcept {
  switch (export_id) {
  case ExportId::d3d12_init:
    return kD3d12InitExport;
  case ExportId::populate_parameters:
    return kPopulateParametersExport;
  case ExportId::d3d12_get_scratch_buffer_size:
    return kD3d12GetScratchBufferSizeExport;
  case ExportId::d3d12_create_feature:
    return kD3d12CreateFeatureExport;
  case ExportId::d3d12_evaluate_feature:
    return kD3d12EvaluateFeatureExport;
  case ExportId::d3d12_release_feature:
    return kD3d12ReleaseFeatureExport;
  case ExportId::d3d12_shutdown:
    return kD3d12ShutdownExport;
  case ExportId::d3d12_shutdown1:
    return kD3d12Shutdown1Export;
  case ExportId::d3d12_create_feature1:
    return kD3d12CreateFeature1Export;
  case ExportId::d3d12_init_ext:
    return kD3d12InitExtExport;
  case ExportId::d3d12_init_ext1:
    return kD3d12InitExt1Export;
  case ExportId::d3d12_init_ext2:
    return kD3d12InitExt2Export;
  case ExportId::d3d12_populate_parameters_impl:
    return kD3d12PopulateParametersImplExport;
  case ExportId::d3d12_populate_device_parameters_impl:
    return kD3d12PopulateDeviceParametersImplExport;
  case ExportId::unknown:
    break;
  }
  return {};
}

bool is_bridge_lifecycle_export(const ExportId export_id) noexcept {
  switch (export_id) {
  case ExportId::d3d12_init:
  case ExportId::d3d12_get_scratch_buffer_size:
  case ExportId::d3d12_create_feature:
  case ExportId::d3d12_evaluate_feature:
  case ExportId::d3d12_release_feature:
  case ExportId::d3d12_shutdown:
    return true;
  case ExportId::unknown:
  case ExportId::populate_parameters:
  case ExportId::d3d12_shutdown1:
  case ExportId::d3d12_create_feature1:
  case ExportId::d3d12_init_ext:
  case ExportId::d3d12_init_ext1:
  case ExportId::d3d12_init_ext2:
  case ExportId::d3d12_populate_parameters_impl:
  case ExportId::d3d12_populate_device_parameters_impl:
    return false;
  }
  return false;
}

bool is_provider_guard_export(const ExportId export_id) noexcept {
  switch (export_id) {
  case ExportId::d3d12_shutdown1:
  case ExportId::d3d12_create_feature1:
  case ExportId::d3d12_init_ext:
  case ExportId::d3d12_init_ext1:
  case ExportId::d3d12_init_ext2:
  case ExportId::d3d12_populate_parameters_impl:
  case ExportId::d3d12_populate_device_parameters_impl:
    return true;
  case ExportId::unknown:
  case ExportId::d3d12_init:
  case ExportId::d3d12_get_scratch_buffer_size:
  case ExportId::populate_parameters:
  case ExportId::d3d12_create_feature:
  case ExportId::d3d12_evaluate_feature:
  case ExportId::d3d12_release_feature:
  case ExportId::d3d12_shutdown:
    return false;
  }
  return false;
}

std::uint32_t provider_route_bit(const ExportId export_id) noexcept {
  switch (export_id) {
  case ExportId::d3d12_init:
    return 1U << 0U;
  case ExportId::d3d12_shutdown:
    return 1U << 1U;
  case ExportId::d3d12_get_scratch_buffer_size:
    return 1U << 2U;
  case ExportId::d3d12_create_feature:
    return 1U << 3U;
  case ExportId::d3d12_evaluate_feature:
    return 1U << 4U;
  case ExportId::d3d12_release_feature:
    return 1U << 5U;
  case ExportId::unknown:
  case ExportId::populate_parameters:
  case ExportId::d3d12_shutdown1:
  case ExportId::d3d12_create_feature1:
  case ExportId::d3d12_init_ext:
  case ExportId::d3d12_init_ext1:
  case ExportId::d3d12_init_ext2:
  case ExportId::d3d12_populate_parameters_impl:
  case ExportId::d3d12_populate_device_parameters_impl:
    return 0U;
  }
  return 0U;
}

std::uint32_t provider_lookup_bit(const ExportId export_id) noexcept {
  const std::uint32_t route_bit = provider_route_bit(export_id);
  if (route_bit != 0U) {
    return route_bit;
  }
  switch (export_id) {
  case ExportId::d3d12_shutdown1:
    return 1U << 6U;
  case ExportId::d3d12_create_feature1:
    return 1U << 7U;
  case ExportId::d3d12_init_ext:
    return 1U << 8U;
  case ExportId::d3d12_init_ext1:
    return 1U << 9U;
  case ExportId::d3d12_init_ext2:
    return 1U << 10U;
  case ExportId::d3d12_populate_parameters_impl:
    return 1U << 11U;
  case ExportId::populate_parameters:
    return 1U << 12U;
  case ExportId::d3d12_populate_device_parameters_impl:
    return 1U << 13U;
  case ExportId::unknown:
  case ExportId::d3d12_init:
  case ExportId::d3d12_get_scratch_buffer_size:
  case ExportId::d3d12_create_feature:
  case ExportId::d3d12_evaluate_feature:
  case ExportId::d3d12_release_feature:
  case ExportId::d3d12_shutdown:
    return 0U;
  }
  return 0U;
}

ExportId expected_provider_lookup(const std::uint32_t seen_mask) noexcept {
  switch (seen_mask) {
  case 0x00U:
    return ExportId::d3d12_init;
  case 0x01U:
    return ExportId::d3d12_shutdown;
  case 0x03U:
    return ExportId::d3d12_shutdown1;
  case 0x43U:
    return ExportId::d3d12_get_scratch_buffer_size;
  case 0x47U:
    return ExportId::d3d12_create_feature;
  case 0x4FU:
    return ExportId::d3d12_create_feature1;
  case 0xCFU:
    return ExportId::d3d12_evaluate_feature;
  case 0xDFU:
    return ExportId::d3d12_release_feature;
  case 0xFFU:
    return ExportId::d3d12_init_ext;
  case 0x1FFU:
    return ExportId::d3d12_init_ext1;
  case 0x3FFU:
    return ExportId::d3d12_init_ext2;
  case 0x7FFU:
    return ExportId::d3d12_populate_parameters_impl;
  case 0xFFFU:
    return ExportId::populate_parameters;
  case 0x1FFFU:
    return ExportId::d3d12_populate_device_parameters_impl;
  case kProviderLookupMaskComplete:
  default:
    return ExportId::unknown;
  }
}

ModeParseResult parse_mode(const std::string_view text) noexcept {
  if (text == kDisabledMode) {
    return {Mode::disabled, true};
  }
  if (text == kCaptureCreateMode) {
    return {Mode::capture_create, true};
  }
  if (text == kCaptureOneEvaluateMode) {
    return {Mode::capture_one_evaluate, true};
  }
  return {Mode::disabled, false};
}

bool identity_is_verified(const IdentityEvidence &evidence) noexcept {
  return evidence.host_binary_verified &&
         evidence.system_resolver_verified && evidence.driver_core_verified &&
         evidence.bridge_binary_verified && evidence.d3d12_path_verified;
}

bool caller_is_verified_for(const CallerEvidence &evidence,
                            const ExportId expected_export) noexcept {
  if (expected_export == ExportId::unknown ||
      evidence.requested_export != expected_export ||
      evidence.module_base == 0U || evidence.image_size == 0U ||
      !evidence.caller_binary_verified || !evidence.memory_image ||
      !evidence.allocation_base_matches || !evidence.executable_section ||
      !evidence.executable_page ||
      evidence.return_address < evidence.module_base) {
    return false;
  }

  const std::uintptr_t return_rva =
      evidence.return_address - evidence.module_base;
  return return_rva < evidence.image_size &&
         return_rva == evidence.expected_return_rva;
}

OneShotLifecycle::OneShotLifecycle(
    const Mode mode, const std::uint64_t one_evaluate_token) noexcept
    : mode_(mode), one_evaluate_token_(one_evaluate_token) {
  switch (mode_) {
  case Mode::disabled:
    state_ = LifecycleState::disabled;
    break;
  case Mode::capture_create:
    state_ = LifecycleState::awaiting_verification;
    break;
  case Mode::capture_one_evaluate:
    configuration_valid_ = one_evaluate_token_ != 0U;
    state_ = configuration_valid_ ? LifecycleState::awaiting_verification
                                  : LifecycleState::inert;
    break;
  default:
    configuration_valid_ = false;
    state_ = LifecycleState::inert;
    break;
  }
}

TransitionResult OneShotLifecycle::commit(
    const LifecycleOperation operation, const LifecycleState next_state,
    const TransitionDecision decision, const BoundaryResponse response,
    const std::uint64_t issued_token, const bool transition_applied,
    const bool capture_committed) noexcept {
  const LifecycleState before = state_;
  if (event_count_ >= events_.size()) {
    overflowed_ = true;
    state_ = LifecycleState::inert;
    if (dropped_event_count_ !=
        (std::numeric_limits<std::uint32_t>::max)()) {
      ++dropped_event_count_;
    }
    return {before,
            state_,
            TransitionDecision::capacity_exhausted,
            BoundaryResponse::fail_closed,
            0U,
            0U,
            false,
            false,
            false};
  }

  state_ = next_state;
  const std::uint32_t sequence =
      static_cast<std::uint32_t>(event_count_ + 1U);
  events_[event_count_] = {operation,
                          before,
                          state_,
                          decision,
                          response,
                          issued_token,
                          sequence,
                          transition_applied,
                          capture_committed};
  ++event_count_;
  return {before,
          state_,
          decision,
          response,
          issued_token,
          sequence,
          transition_applied,
          capture_committed,
          true};
}

TransitionResult OneShotLifecycle::reject(
    const LifecycleOperation operation,
    const TransitionDecision decision) noexcept {
  return commit(operation, LifecycleState::inert, decision,
                BoundaryResponse::fail_closed, 0U, false, false);
}

TransitionResult
OneShotLifecycle::apply(const TransitionInput &input) noexcept {
  if (!configuration_valid_) {
    return reject(input.operation,
                  TransitionDecision::invalid_configuration);
  }
  if (!is_enabled_mode(mode_)) {
    return commit(input.operation, LifecycleState::disabled,
                  TransitionDecision::mode_disabled,
                  BoundaryResponse::fail_closed, 0U, false, false);
  }
  if (state_ == LifecycleState::inert) {
    return reject(input.operation, TransitionDecision::already_inert);
  }
  if (input.operation == LifecycleOperation::abort) {
    return commit(input.operation, LifecycleState::inert,
                  TransitionDecision::aborted,
                  BoundaryResponse::fail_closed, 0U, true, false);
  }
  if (!identity_is_verified(input.identity)) {
    return reject(input.operation, TransitionDecision::identity_rejected);
  }

  switch (input.operation) {
  case LifecycleOperation::verify:
    if (state_ != LifecycleState::awaiting_verification) {
      return reject(input.operation, TransitionDecision::invalid_order);
    }
    return commit(input.operation, LifecycleState::verified,
                  TransitionDecision::identity_verified,
                  BoundaryResponse::none, 0U, true, false);

  case LifecycleOperation::initialize:
    if (state_ != LifecycleState::verified) {
      return reject(input.operation, TransitionDecision::invalid_order);
    }
    if (!caller_is_verified_for(input.caller, ExportId::d3d12_init)) {
      return reject(input.operation, TransitionDecision::caller_rejected);
    }
    return commit(input.operation, LifecycleState::initialized,
                  TransitionDecision::initialized, BoundaryResponse::success,
                  0U, true, false);

  case LifecycleOperation::populate:
    // The pinned modern core invokes the legacy generic Populate callback for
    // its capability and device-parameter objects. Accept those two verified
    // calls only; a third callback is version drift or re-entry.
    if (state_ != LifecycleState::initialized &&
        state_ != LifecycleState::populated) {
      return reject(input.operation, TransitionDecision::invalid_order);
    }
    if (!caller_is_verified_for(input.caller,
                                ExportId::populate_parameters)) {
      return reject(input.operation, TransitionDecision::caller_rejected);
    }
    if (populate_count_ >= 2U) {
      return reject(input.operation, TransitionDecision::invalid_order);
    }
    {
      const TransitionResult result = commit(
          input.operation, LifecycleState::populated,
          TransitionDecision::populated, BoundaryResponse::success, 0U, true,
          false);
      if (result.transition_applied) {
        ++populate_count_;
      }
      return result;
    }

  case LifecycleOperation::capture_create:
    // Populate is conditional on the driver core completing the rest of its
    // feature-init loop. Creation capture is still useful when our exact Init
    // call was verified but that later, unrelated population pass was skipped.
    if (state_ != LifecycleState::initialized &&
        state_ != LifecycleState::populated) {
      return reject(input.operation, TransitionDecision::invalid_order);
    }
    if (!caller_is_verified_for(input.caller,
                                ExportId::d3d12_create_feature)) {
      return reject(input.operation, TransitionDecision::caller_rejected);
    }
    if (mode_ == Mode::capture_create) {
      return commit(input.operation, LifecycleState::create_captured,
                    TransitionDecision::create_captured_fail_closed,
                    BoundaryResponse::fail_closed, 0U, true, true);
    }
    return commit(input.operation, LifecycleState::evaluate_token_issued,
                  TransitionDecision::evaluate_token_issued,
                  BoundaryResponse::success_with_token,
                  one_evaluate_token_, true, true);

  case LifecycleOperation::capture_evaluate:
    if (mode_ != Mode::capture_one_evaluate ||
        state_ != LifecycleState::evaluate_token_issued) {
      return reject(input.operation, TransitionDecision::invalid_order);
    }
    if (input.caller.requested_export !=
            ExportId::d3d12_evaluate_feature ||
        !caller_is_verified_for(input.caller,
                                input.caller.requested_export)) {
      return reject(input.operation, TransitionDecision::caller_rejected);
    }
    if (input.presented_token == 0U ||
        input.presented_token != one_evaluate_token_) {
      return reject(input.operation, TransitionDecision::token_rejected);
    }
    return commit(input.operation, LifecycleState::evaluate_captured,
                  TransitionDecision::evaluate_captured_fail_closed,
                  BoundaryResponse::fail_closed, 0U, true, true);

  case LifecycleOperation::release:
    if (mode_ != Mode::capture_one_evaluate ||
        (state_ != LifecycleState::evaluate_token_issued &&
         state_ != LifecycleState::evaluate_captured)) {
      return reject(input.operation, TransitionDecision::invalid_order);
    }
    if (!caller_is_verified_for(input.caller,
                                ExportId::d3d12_release_feature)) {
      return reject(input.operation, TransitionDecision::caller_rejected);
    }
    if (input.presented_token == 0U ||
        input.presented_token != one_evaluate_token_) {
      return reject(input.operation, TransitionDecision::token_rejected);
    }
    return commit(input.operation, LifecycleState::released,
                  TransitionDecision::released, BoundaryResponse::success,
                  0U, true, false);

  case LifecycleOperation::shutdown:
    if (!caller_is_verified_for(input.caller,
                                ExportId::d3d12_shutdown)) {
      return reject(input.operation, TransitionDecision::caller_rejected);
    }
    return commit(input.operation, LifecycleState::inert,
                  TransitionDecision::shut_down, BoundaryResponse::success,
                  0U, true, false);

  case LifecycleOperation::abort:
    break;
  }

  return reject(input.operation, TransitionDecision::invalid_order);
}

} // namespace mhw::bridge_probe
