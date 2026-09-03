#include "bridge_policy.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

namespace {

using mhw::bridge_probe::BoundaryResponse;
using mhw::bridge_probe::CallerEvidence;
using mhw::bridge_probe::ExportId;
using mhw::bridge_probe::IdentityEvidence;
using mhw::bridge_probe::LifecycleOperation;
using mhw::bridge_probe::LifecycleState;
using mhw::bridge_probe::Mode;
using mhw::bridge_probe::OneShotLifecycle;
using mhw::bridge_probe::TransitionDecision;
using mhw::bridge_probe::TransitionInput;
using mhw::bridge_probe::TransitionResult;

int fail(const char *message) {
  std::cerr << message << '\n';
  return 1;
}

constexpr IdentityEvidence verified_identity() noexcept {
  return {true, true, true, true, true};
}

constexpr CallerEvidence verified_caller(
    const ExportId export_id,
    const std::uintptr_t return_rva = 0x1234U) noexcept {
  constexpr std::uintptr_t module_base = 0x00007FFE00000000ULL;
  return {export_id,
          module_base,
          module_base + return_rva,
          return_rva,
          0x00100000U,
          true,
          true,
          true,
          true,
          true};
}

TransitionInput make_input(const LifecycleOperation operation,
                           const ExportId export_id = ExportId::unknown,
                           const std::uint64_t token = 0U) noexcept {
  return {operation, verified_identity(), verified_caller(export_id), token};
}

bool is_applied(const TransitionResult &result,
                const LifecycleState expected_state,
                const TransitionDecision expected_decision,
                const BoundaryResponse expected_response) noexcept {
  return result.transition_applied && result.event_recorded &&
         result.after == expected_state &&
         result.decision == expected_decision &&
         result.response == expected_response;
}

bool advance_to_populated(OneShotLifecycle &lifecycle) noexcept {
  const TransitionResult verified =
      lifecycle.apply(make_input(LifecycleOperation::verify));
  const TransitionResult initialized = lifecycle.apply(make_input(
      LifecycleOperation::initialize, ExportId::d3d12_init));
  const TransitionResult populated = lifecycle.apply(make_input(
      LifecycleOperation::populate, ExportId::populate_parameters));
  return is_applied(verified, LifecycleState::verified,
                    TransitionDecision::identity_verified,
                    BoundaryResponse::none) &&
         is_applied(initialized, LifecycleState::initialized,
                    TransitionDecision::initialized,
                    BoundaryResponse::success) &&
         is_applied(populated, LifecycleState::populated,
                    TransitionDecision::populated,
                    BoundaryResponse::success);
}

} // namespace

int main() {
  using mhw::bridge_probe::InitValidationEvidence;
  using mhw::bridge_probe::LifecycleEvent;
  using mhw::bridge_probe::ModeParseResult;
  using mhw::bridge_probe::caller_is_verified_for;
  using mhw::bridge_probe::classify_export_name;
  using mhw::bridge_probe::export_name;
  using mhw::bridge_probe::expected_provider_lookup;
  using mhw::bridge_probe::identity_is_verified;
  using mhw::bridge_probe::init_validation_is_complete;
  using mhw::bridge_probe::init_validation_mask;
  using mhw::bridge_probe::is_bridge_lifecycle_export;
  using mhw::bridge_probe::is_provider_guard_export;
  using mhw::bridge_probe::kCaptureCreateMode;
  using mhw::bridge_probe::kCaptureOneEvaluateMode;
  using mhw::bridge_probe::kDisabledMode;
  using mhw::bridge_probe::kExpectedInitApiVersion;
  using mhw::bridge_probe::kExpectedInitApplicationId;
  using mhw::bridge_probe::kInitGateApiVersion;
  using mhw::bridge_probe::kInitGateD3d12Device;
  using mhw::bridge_probe::kInitValidationMaskComplete;
  using mhw::bridge_probe::kInitValidationMaskWithoutDevice;
  using mhw::bridge_probe::kLifecycleEventCapacity;
  using mhw::bridge_probe::kProviderRouteMaskComplete;
  using mhw::bridge_probe::kProviderLookupMaskComplete;
  using mhw::bridge_probe::kProviderCallbackMaskComplete;
  using mhw::bridge_probe::kProviderGuardMaskComplete;
  using mhw::bridge_probe::parse_mode;
  using mhw::bridge_probe::provider_route_bit;
  using mhw::bridge_probe::provider_lookup_bit;

  static_assert(std::is_trivially_copyable_v<IdentityEvidence>);
  static_assert(std::is_trivially_copyable_v<InitValidationEvidence>);
  static_assert(std::is_trivially_copyable_v<CallerEvidence>);
  static_assert(std::is_trivially_copyable_v<TransitionInput>);
  static_assert(std::is_trivially_copyable_v<TransitionResult>);
  static_assert(std::is_trivially_copyable_v<LifecycleEvent>);

  InitValidationEvidence valid_init{true,
                                    true,
                                    kExpectedInitApplicationId,
                                    true,
                                    kExpectedInitApiVersion,
                                    true,
                                    true};
  if (!init_validation_is_complete(valid_init) ||
      init_validation_mask(valid_init) != kInitValidationMaskComplete) {
    return fail("valid Init evidence did not pass every gate");
  }
  InitValidationEvidence no_device = valid_init;
  no_device.d3d12_device_verified = false;
  if (init_validation_is_complete(no_device) ||
      init_validation_mask(no_device) !=
          kInitValidationMaskWithoutDevice ||
      (init_validation_mask(no_device) & kInitGateD3d12Device) != 0U) {
    return fail("missing D3D12 device evidence did not fail closed");
  }
  InitValidationEvidence snippet_as_api = valid_init;
  snippet_as_api.api_version = 0x0001010DU;
  if (init_validation_is_complete(snippet_as_api) ||
      (init_validation_mask(snippet_as_api) & kInitGateApiVersion) != 0U) {
    return fail("feature snippet version was accepted as Init API version");
  }

  struct ExportCase {
    std::string_view name;
    ExportId export_id;
  };
  constexpr std::array<ExportCase, 14> export_cases{{
      {"NVSDK_NGX_D3D12_Init", ExportId::d3d12_init},
      {"NVSDK_NGX_D3D12_GetScratchBufferSize",
       ExportId::d3d12_get_scratch_buffer_size},
      {"NVSDK_NGX_PopulateParameters", ExportId::populate_parameters},
      {"NVSDK_NGX_D3D12_CreateFeature", ExportId::d3d12_create_feature},
      {"NVSDK_NGX_D3D12_EvaluateFeature",
       ExportId::d3d12_evaluate_feature},
      {"NVSDK_NGX_D3D12_ReleaseFeature",
       ExportId::d3d12_release_feature},
      {"NVSDK_NGX_D3D12_Shutdown", ExportId::d3d12_shutdown},
      {"NVSDK_NGX_D3D12_Shutdown1", ExportId::d3d12_shutdown1},
      {"NVSDK_NGX_D3D12_CreateFeature1", ExportId::d3d12_create_feature1},
      {"NVSDK_NGX_D3D12_Init_Ext", ExportId::d3d12_init_ext},
      {"NVSDK_NGX_D3D12_Init_Ext1", ExportId::d3d12_init_ext1},
      {"NVSDK_NGX_D3D12_Init_Ext2", ExportId::d3d12_init_ext2},
      {"NVSDK_NGX_D3D12_PopulateParameters_Impl",
       ExportId::d3d12_populate_parameters_impl},
      {"NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl",
       ExportId::d3d12_populate_device_parameters_impl},
  }};
  for (const ExportCase &test_case : export_cases) {
    const bool expected_lifecycle =
        provider_route_bit(test_case.export_id) != 0U;
    const bool expected_guard =
        test_case.export_id != ExportId::populate_parameters &&
        !expected_lifecycle;
    if (classify_export_name(test_case.name) != test_case.export_id ||
        export_name(test_case.export_id) != test_case.name ||
        is_bridge_lifecycle_export(test_case.export_id) !=
            expected_lifecycle ||
        is_provider_guard_export(test_case.export_id) != expected_guard) {
      return fail("exact export-name classification failed");
    }
  }
  std::uint32_t provider_mask{};
  for (const ExportCase &test_case : export_cases) {
    provider_mask |= provider_route_bit(test_case.export_id);
  }
  if (provider_mask != kProviderRouteMaskComplete ||
      provider_route_bit(ExportId::populate_parameters) != 0U ||
      provider_route_bit(ExportId::unknown) != 0U) {
    return fail("exact six-export provider route mask changed");
  }
  constexpr std::array<ExportId, 14> provider_order{
      ExportId::d3d12_init,
      ExportId::d3d12_shutdown,
      ExportId::d3d12_shutdown1,
      ExportId::d3d12_get_scratch_buffer_size,
      ExportId::d3d12_create_feature,
      ExportId::d3d12_create_feature1,
      ExportId::d3d12_evaluate_feature,
      ExportId::d3d12_release_feature,
      ExportId::d3d12_init_ext,
      ExportId::d3d12_init_ext1,
      ExportId::d3d12_init_ext2,
      ExportId::d3d12_populate_parameters_impl,
      ExportId::populate_parameters,
      ExportId::d3d12_populate_device_parameters_impl,
  };
  std::uint32_t ordered_mask{};
  for (const ExportId export_id : provider_order) {
    if (expected_provider_lookup(ordered_mask) != export_id) {
      return fail("provider resolver transaction order changed");
    }
    ordered_mask |= provider_lookup_bit(export_id);
  }
  if (ordered_mask != kProviderLookupMaskComplete ||
      (ordered_mask & kProviderRouteMaskComplete) !=
          kProviderRouteMaskComplete ||
      (ordered_mask & kProviderGuardMaskComplete) !=
          kProviderGuardMaskComplete ||
      (ordered_mask & kProviderCallbackMaskComplete) !=
          kProviderCallbackMaskComplete ||
      expected_provider_lookup(ordered_mask) != ExportId::unknown ||
      expected_provider_lookup(0x02U) != ExportId::unknown ||
      expected_provider_lookup(0xFFFFFFFFU) != ExportId::unknown) {
    return fail("invalid or complete provider lookup mask was accepted");
  }
  if (classify_export_name({}) != ExportId::unknown ||
      !export_name(ExportId::unknown).empty() ||
      is_bridge_lifecycle_export(ExportId::unknown) ||
      is_bridge_lifecycle_export(static_cast<ExportId>(0xFFU))) {
    return fail("unknown export classification failed closed incorrectly");
  }

  constexpr std::array<std::string_view, 13> rejected_names{
      "nvsdk_ngx_d3d12_init",
      "NVSDK_NGX_D3D12_INIT",
      " NVSDK_NGX_D3D12_Init",
      "NVSDK_NGX_D3D12_Init ",
      "NVSDK_NGX_D3D12_InitX",
      "NVSDK_NGX_D3D11_Init",
      "NVSDK_NGX_D3D12_GetParameters",
      "NVSDK_NGX_D3D12_CreateFeature_C",
      "NVSDK_NGX_D3D12_EvaluateFeature_C",
      "NVSDK_NGX_D3D12_EvaluateFeature_c",
      "NVSDK_NGX_D3D12_ReleaseFeatureX",
      "NVSDK_NGX_Populateparameters",
      "NVSDK_NGX_Shutdown",
  };
  for (const std::string_view name : rejected_names) {
    if (classify_export_name(name) != ExportId::unknown) {
      return fail("non-exact export name was classified");
    }
  }
  constexpr char embedded_nul_name[] =
      "NVSDK_NGX_D3D12_Init\0unexpected";
  if (classify_export_name(std::string_view(
          embedded_nul_name, sizeof(embedded_nul_name) - 1U)) !=
      ExportId::unknown) {
    return fail("export name containing an embedded NUL was accepted");
  }

  const ModeParseResult disabled = parse_mode(kDisabledMode);
  const ModeParseResult create = parse_mode(kCaptureCreateMode);
  const ModeParseResult evaluate = parse_mode(kCaptureOneEvaluateMode);
  if (!disabled.recognized || disabled.mode != Mode::disabled ||
      !create.recognized || create.mode != Mode::capture_create ||
      !evaluate.recognized ||
      evaluate.mode != Mode::capture_one_evaluate) {
    return fail("an explicit mode was not parsed exactly");
  }
  constexpr std::array<std::string_view, 10> rejected_modes{
      "",
      "enable",
      "capture",
      "Capture-Create",
      "capture_create",
      "capture-create ",
      " capture-create",
      "CAPTURE-ONE-EVALUATE",
      "capture-one-evaluate-now",
      "0",
  };
  for (const std::string_view mode_text : rejected_modes) {
    const ModeParseResult parsed = parse_mode(mode_text);
    if (parsed.recognized || parsed.mode != Mode::disabled) {
      return fail("invalid mode did not fall back to disabled");
    }
  }

  const IdentityEvidence exact_identity = verified_identity();
  if (!identity_is_verified(exact_identity)) {
    return fail("complete identity evidence was rejected");
  }
  using IdentityFlag = bool IdentityEvidence::*;
  constexpr std::array<IdentityFlag, 5> identity_flags{
      &IdentityEvidence::host_binary_verified,
      &IdentityEvidence::system_resolver_verified,
      &IdentityEvidence::driver_core_verified,
      &IdentityEvidence::bridge_binary_verified,
      &IdentityEvidence::d3d12_path_verified,
  };
  for (const IdentityFlag flag : identity_flags) {
    IdentityEvidence incomplete = exact_identity;
    incomplete.*flag = false;
    if (identity_is_verified(incomplete)) {
      return fail("incomplete identity evidence was accepted");
    }
  }

  const CallerEvidence exact_caller =
      verified_caller(ExportId::d3d12_create_feature);
  if (!caller_is_verified_for(exact_caller,
                              ExportId::d3d12_create_feature) ||
      caller_is_verified_for(exact_caller,
                             ExportId::d3d12_evaluate_feature) ||
      caller_is_verified_for(exact_caller, ExportId::unknown)) {
    return fail("exact caller/export predicate is incorrect");
  }
  using CallerFlag = bool CallerEvidence::*;
  constexpr std::array<CallerFlag, 5> caller_flags{
      &CallerEvidence::caller_binary_verified,
      &CallerEvidence::memory_image,
      &CallerEvidence::allocation_base_matches,
      &CallerEvidence::executable_section,
      &CallerEvidence::executable_page,
  };
  for (const CallerFlag flag : caller_flags) {
    CallerEvidence rejected = exact_caller;
    rejected.*flag = false;
    if (caller_is_verified_for(rejected,
                               ExportId::d3d12_create_feature)) {
      return fail("caller with a missing memory predicate was accepted");
    }
  }
  CallerEvidence rejected_caller = exact_caller;
  rejected_caller.module_base = 0U;
  if (caller_is_verified_for(rejected_caller,
                             ExportId::d3d12_create_feature)) {
    return fail("caller with a null module base was accepted");
  }
  rejected_caller = exact_caller;
  rejected_caller.image_size = 0U;
  if (caller_is_verified_for(rejected_caller,
                             ExportId::d3d12_create_feature)) {
    return fail("caller with a zero image size was accepted");
  }
  rejected_caller = exact_caller;
  rejected_caller.return_address = rejected_caller.module_base - 1U;
  if (caller_is_verified_for(rejected_caller,
                             ExportId::d3d12_create_feature)) {
    return fail("caller below the module base was accepted");
  }
  rejected_caller = exact_caller;
  ++rejected_caller.expected_return_rva;
  if (caller_is_verified_for(rejected_caller,
                             ExportId::d3d12_create_feature)) {
    return fail("caller with the wrong return RVA was accepted");
  }
  rejected_caller = exact_caller;
  rejected_caller.return_address =
      rejected_caller.module_base + rejected_caller.image_size;
  rejected_caller.expected_return_rva = rejected_caller.image_size;
  if (caller_is_verified_for(rejected_caller,
                             ExportId::d3d12_create_feature)) {
    return fail("caller at the exclusive image end was accepted");
  }
  CallerEvidence high_address_caller = exact_caller;
  high_address_caller.module_base =
      (std::numeric_limits<std::uintptr_t>::max)() - 15U;
  high_address_caller.return_address =
      (std::numeric_limits<std::uintptr_t>::max)() - 6U;
  high_address_caller.expected_return_rva = 9U;
  high_address_caller.image_size = 16U;
  if (!caller_is_verified_for(high_address_caller,
                              ExportId::d3d12_create_feature)) {
    return fail("overflow-safe caller subtraction rejected a valid RVA");
  }

  OneShotLifecycle disabled_lifecycle;
  if (disabled_lifecycle.mode() != Mode::disabled ||
      disabled_lifecycle.state() != LifecycleState::disabled ||
      !disabled_lifecycle.configuration_valid() ||
      !disabled_lifecycle.events().empty()) {
    return fail("default lifecycle was not inert-by-default");
  }
  const TransitionResult disabled_attempt =
      disabled_lifecycle.apply(make_input(LifecycleOperation::verify));
  if (disabled_attempt.transition_applied ||
      !disabled_attempt.event_recorded ||
      disabled_attempt.decision != TransitionDecision::mode_disabled ||
      disabled_attempt.response != BoundaryResponse::fail_closed ||
      disabled_lifecycle.state() != LifecycleState::disabled) {
    return fail("disabled lifecycle did not fail closed");
  }

  OneShotLifecycle invalid_token(Mode::capture_one_evaluate, 0U);
  if (invalid_token.configuration_valid() ||
      invalid_token.state() != LifecycleState::inert) {
    return fail("zero one-Evaluate token was not rejected at construction");
  }
  const TransitionResult invalid_configuration =
      invalid_token.apply(make_input(LifecycleOperation::verify));
  if (invalid_configuration.decision !=
          TransitionDecision::invalid_configuration ||
      invalid_configuration.transition_applied ||
      invalid_configuration.response != BoundaryResponse::fail_closed) {
    return fail("invalid configuration did not remain fail closed");
  }
  OneShotLifecycle invalid_mode(static_cast<Mode>(0xFFU), 1U);
  if (invalid_mode.configuration_valid() ||
      invalid_mode.state() != LifecycleState::inert) {
    return fail("unknown mode enum was not rejected");
  }

  OneShotLifecycle double_populate(Mode::capture_create);
  if (!advance_to_populated(double_populate)) {
    return fail("double-Populate setup failed");
  }
  const TransitionResult second_populate = double_populate.apply(make_input(
      LifecycleOperation::populate, ExportId::populate_parameters));
  if (!is_applied(second_populate, LifecycleState::populated,
                  TransitionDecision::populated,
                  BoundaryResponse::success) ||
      double_populate.populate_budget_available()) {
    return fail("verified second Populate was not accepted idempotently");
  }
  const TransitionResult third_populate = double_populate.apply(make_input(
      LifecycleOperation::populate, ExportId::populate_parameters));
  if (third_populate.decision != TransitionDecision::invalid_order ||
      third_populate.transition_applied ||
      double_populate.state() != LifecycleState::inert) {
    return fail("third Populate did not make the bounded lifecycle inert");
  }

  OneShotLifecycle create_without_populate(Mode::capture_create);
  const TransitionResult direct_verified = create_without_populate.apply(
      make_input(LifecycleOperation::verify));
  const TransitionResult direct_initialized = create_without_populate.apply(
      make_input(LifecycleOperation::initialize, ExportId::d3d12_init));
  const TransitionResult direct_create = create_without_populate.apply(
      make_input(LifecycleOperation::capture_create,
                 ExportId::d3d12_create_feature));
  if (!is_applied(direct_verified, LifecycleState::verified,
                  TransitionDecision::identity_verified,
                  BoundaryResponse::none) ||
      !is_applied(direct_initialized, LifecycleState::initialized,
                  TransitionDecision::initialized,
                  BoundaryResponse::success) ||
      !is_applied(direct_create, LifecycleState::create_captured,
                  TransitionDecision::create_captured_fail_closed,
                  BoundaryResponse::fail_closed)) {
    return fail("Create could not capture after verified Init without Populate");
  }

  OneShotLifecycle create_only(Mode::capture_create);
  if (!advance_to_populated(create_only)) {
    return fail("create-only lifecycle prerequisites failed");
  }
  const TransitionResult create_capture = create_only.apply(make_input(
      LifecycleOperation::capture_create,
      ExportId::d3d12_create_feature));
  if (!is_applied(create_capture, LifecycleState::create_captured,
                  TransitionDecision::create_captured_fail_closed,
                  BoundaryResponse::fail_closed) ||
      !create_capture.capture_committed || create_capture.issued_token != 0U ||
      create_capture.sequence != 4U || create_only.events().size() != 4U) {
    return fail("default Create capture did not terminate fail closed");
  }
  const LifecycleEvent &recorded_create = create_only.events().back();
  if (recorded_create.operation != LifecycleOperation::capture_create ||
      !recorded_create.capture_committed ||
      recorded_create.before != LifecycleState::populated ||
      recorded_create.after != LifecycleState::create_captured) {
    return fail("Create capture event was recorded incorrectly");
  }
  const TransitionResult forbidden_evaluate = create_only.apply(make_input(
      LifecycleOperation::capture_evaluate,
      ExportId::d3d12_evaluate_feature, 1U));
  if (forbidden_evaluate.decision != TransitionDecision::invalid_order ||
      forbidden_evaluate.transition_applied ||
      create_only.state() != LifecycleState::inert) {
    return fail("default mode permitted Evaluate after Create capture");
  }

  OneShotLifecycle create_then_shutdown(Mode::capture_create);
  if (!advance_to_populated(create_then_shutdown) ||
      !create_then_shutdown
           .apply(make_input(LifecycleOperation::capture_create,
                             ExportId::d3d12_create_feature))
           .capture_committed) {
    return fail("Create/shutdown setup failed");
  }
  const TransitionResult create_shutdown = create_then_shutdown.apply(
      make_input(LifecycleOperation::shutdown, ExportId::d3d12_shutdown));
  if (!is_applied(create_shutdown, LifecycleState::inert,
                  TransitionDecision::shut_down,
                  BoundaryResponse::success)) {
    return fail("exact shutdown did not retire a Create capture");
  }

  constexpr std::uint64_t one_evaluate_token = 0x8A31F04C620D975BULL;
  OneShotLifecycle one_evaluate(Mode::capture_one_evaluate,
                                one_evaluate_token);
  if (!advance_to_populated(one_evaluate)) {
    return fail("one-Evaluate lifecycle prerequisites failed");
  }
  const TransitionResult issued = one_evaluate.apply(make_input(
      LifecycleOperation::capture_create,
      ExportId::d3d12_create_feature));
  if (!is_applied(issued, LifecycleState::evaluate_token_issued,
                  TransitionDecision::evaluate_token_issued,
                  BoundaryResponse::success_with_token) ||
      !issued.capture_committed ||
      issued.issued_token != one_evaluate_token) {
    return fail("one-Evaluate mode did not issue its exact token");
  }
  const TransitionResult evaluate_capture = one_evaluate.apply(make_input(
      LifecycleOperation::capture_evaluate,
      ExportId::d3d12_evaluate_feature, one_evaluate_token));
  if (!is_applied(evaluate_capture, LifecycleState::evaluate_captured,
                  TransitionDecision::evaluate_captured_fail_closed,
                  BoundaryResponse::fail_closed) ||
      !evaluate_capture.capture_committed ||
      evaluate_capture.issued_token != 0U) {
    return fail("Evaluate was not captured exactly once and failed closed");
  }
  const TransitionResult released = one_evaluate.apply(make_input(
      LifecycleOperation::release, ExportId::d3d12_release_feature,
      one_evaluate_token));
  if (!is_applied(released, LifecycleState::released,
                  TransitionDecision::released,
                  BoundaryResponse::success)) {
    return fail("issued token was not released");
  }
  const TransitionResult shutdown = one_evaluate.apply(
      make_input(LifecycleOperation::shutdown, ExportId::d3d12_shutdown));
  if (!is_applied(shutdown, LifecycleState::inert,
                  TransitionDecision::shut_down,
                  BoundaryResponse::success) ||
      one_evaluate.events().size() != 7U || one_evaluate.overflowed()) {
    return fail("one-Evaluate lifecycle did not finish inert");
  }
  for (std::size_t index = 0U; index < one_evaluate.events().size(); ++index) {
    if (one_evaluate.events()[index].sequence != index + 1U) {
      return fail("lifecycle event sequence is not monotonic");
    }
  }

  OneShotLifecycle release_without_evaluate(Mode::capture_one_evaluate,
                                            one_evaluate_token);
  if (!advance_to_populated(release_without_evaluate) ||
      release_without_evaluate
              .apply(make_input(LifecycleOperation::capture_create,
                                ExportId::d3d12_create_feature))
              .issued_token != one_evaluate_token) {
    return fail("release-before-Evaluate setup failed");
  }
  const TransitionResult early_release = release_without_evaluate.apply(
      make_input(LifecycleOperation::release,
                 ExportId::d3d12_release_feature, one_evaluate_token));
  if (!is_applied(early_release, LifecycleState::released,
                  TransitionDecision::released,
                  BoundaryResponse::success)) {
    return fail("unused one-Evaluate token could not be released");
  }

  OneShotLifecycle wrong_token(Mode::capture_one_evaluate,
                               one_evaluate_token);
  if (!advance_to_populated(wrong_token) ||
      wrong_token
              .apply(make_input(LifecycleOperation::capture_create,
                                ExportId::d3d12_create_feature))
              .issued_token != one_evaluate_token) {
    return fail("wrong-token setup failed");
  }
  const TransitionResult token_rejected = wrong_token.apply(make_input(
      LifecycleOperation::capture_evaluate,
      ExportId::d3d12_evaluate_feature, one_evaluate_token ^ 1U));
  if (token_rejected.decision != TransitionDecision::token_rejected ||
      token_rejected.transition_applied ||
      token_rejected.capture_committed ||
      wrong_token.state() != LifecycleState::inert) {
    return fail("wrong Evaluate token did not make the policy inert");
  }

  OneShotLifecycle invalid_order(Mode::capture_create);
  const TransitionResult out_of_order = invalid_order.apply(make_input(
      LifecycleOperation::initialize, ExportId::d3d12_init));
  if (out_of_order.decision != TransitionDecision::invalid_order ||
      out_of_order.transition_applied ||
      invalid_order.state() != LifecycleState::inert) {
    return fail("out-of-order initialization did not fail closed");
  }

  OneShotLifecycle bad_identity(Mode::capture_create);
  TransitionInput bad_identity_input = make_input(LifecycleOperation::verify);
  bad_identity_input.identity.bridge_binary_verified = false;
  const TransitionResult identity_rejected =
      bad_identity.apply(bad_identity_input);
  if (identity_rejected.decision != TransitionDecision::identity_rejected ||
      identity_rejected.transition_applied ||
      bad_identity.state() != LifecycleState::inert) {
    return fail("bad identity did not make the policy inert");
  }

  OneShotLifecycle bad_caller(Mode::capture_create);
  const TransitionResult initially_verified =
      bad_caller.apply(make_input(LifecycleOperation::verify));
  TransitionInput bad_caller_input = make_input(
      LifecycleOperation::initialize, ExportId::d3d12_init);
  bad_caller_input.caller.executable_page = false;
  const TransitionResult caller_rejected =
      bad_caller.apply(bad_caller_input);
  if (!initially_verified.transition_applied ||
      caller_rejected.decision != TransitionDecision::caller_rejected ||
      caller_rejected.transition_applied ||
      bad_caller.state() != LifecycleState::inert) {
    return fail("bad caller did not make the policy inert");
  }

  OneShotLifecycle aborted(Mode::capture_create);
  TransitionInput abort_input{};
  abort_input.operation = LifecycleOperation::abort;
  const TransitionResult abort_result = aborted.apply(abort_input);
  if (!is_applied(abort_result, LifecycleState::inert,
                  TransitionDecision::aborted,
                  BoundaryResponse::fail_closed)) {
    return fail("explicit abort did not retire the policy");
  }

  OneShotLifecycle bounded;
  for (std::size_t index = 0U; index < kLifecycleEventCapacity; ++index) {
    const TransitionResult recorded =
        bounded.apply(make_input(LifecycleOperation::verify));
    if (!recorded.event_recorded ||
        recorded.decision != TransitionDecision::mode_disabled ||
        bounded.overflowed()) {
      return fail("fixed event ledger filled incorrectly");
    }
  }
  const TransitionResult overflow =
      bounded.apply(make_input(LifecycleOperation::verify));
  if (overflow.event_recorded || overflow.transition_applied ||
      overflow.decision != TransitionDecision::capacity_exhausted ||
      overflow.response != BoundaryResponse::fail_closed ||
      bounded.state() != LifecycleState::inert || !bounded.overflowed() ||
      bounded.events().size() != kLifecycleEventCapacity ||
      bounded.dropped_event_count() != 1U) {
    return fail("event-ledger overflow did not latch inert/fail-closed state");
  }
  const TransitionResult second_drop =
      bounded.apply(make_input(LifecycleOperation::verify));
  if (second_drop.decision != TransitionDecision::capacity_exhausted ||
      bounded.dropped_event_count() != 2U ||
      bounded.events().size() != kLifecycleEventCapacity) {
    return fail("post-overflow events were not counted without allocation");
  }

  std::cout << "bridge policy tests passed\n";
  return 0;
}
