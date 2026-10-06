#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace mhw::bridge_probe {

// This policy intentionally knows names and ordering only. It contains no NGX
// declarations, result codes, Windows handles, or module-discovery behavior.
enum class ExportId : std::uint8_t {
  unknown,
  d3d12_init,
  d3d12_get_scratch_buffer_size,
  populate_parameters,
  d3d12_create_feature,
  d3d12_evaluate_feature,
  d3d12_release_feature,
  d3d12_shutdown,
  d3d12_shutdown1,
  d3d12_create_feature1,
  d3d12_init_ext,
  d3d12_init_ext1,
  d3d12_init_ext2,
  d3d12_populate_parameters_impl,
  d3d12_populate_device_parameters_impl,
};

inline constexpr std::string_view kD3d12InitExport =
    "NVSDK_NGX_D3D12_Init";
inline constexpr std::string_view kPopulateParametersExport =
    "NVSDK_NGX_PopulateParameters";
inline constexpr std::string_view kD3d12GetScratchBufferSizeExport =
    "NVSDK_NGX_D3D12_GetScratchBufferSize";
inline constexpr std::string_view kD3d12CreateFeatureExport =
    "NVSDK_NGX_D3D12_CreateFeature";
inline constexpr std::string_view kD3d12EvaluateFeatureExport =
    "NVSDK_NGX_D3D12_EvaluateFeature";
inline constexpr std::string_view kD3d12ReleaseFeatureExport =
    "NVSDK_NGX_D3D12_ReleaseFeature";
inline constexpr std::string_view kD3d12ShutdownExport =
    "NVSDK_NGX_D3D12_Shutdown";
inline constexpr std::string_view kD3d12Shutdown1Export =
    "NVSDK_NGX_D3D12_Shutdown1";
inline constexpr std::string_view kD3d12CreateFeature1Export =
    "NVSDK_NGX_D3D12_CreateFeature1";
inline constexpr std::string_view kD3d12InitExtExport =
    "NVSDK_NGX_D3D12_Init_Ext";
inline constexpr std::string_view kD3d12InitExt1Export =
    "NVSDK_NGX_D3D12_Init_Ext1";
inline constexpr std::string_view kD3d12InitExt2Export =
    "NVSDK_NGX_D3D12_Init_Ext2";
inline constexpr std::string_view kD3d12PopulateParametersImplExport =
    "NVSDK_NGX_D3D12_PopulateParameters_Impl";
inline constexpr std::string_view kD3d12PopulateDeviceParametersImplExport =
    "NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl";

ExportId classify_export_name(std::string_view name) noexcept;
std::string_view export_name(ExportId export_id) noexcept;
bool is_bridge_lifecycle_export(ExportId export_id) noexcept;
bool is_provider_guard_export(ExportId export_id) noexcept;
inline constexpr std::uint32_t kProviderRouteMaskComplete = 0x3FU;
inline constexpr std::uint32_t kProviderGuardMaskComplete = 0x2FC0U;
inline constexpr std::uint32_t kProviderCallbackMaskComplete = 0x1000U;
inline constexpr std::uint32_t kProviderLookupMaskComplete = 0x3FFFU;
std::uint32_t provider_route_bit(ExportId export_id) noexcept;
std::uint32_t provider_lookup_bit(ExportId export_id) noexcept;
// Returns the next base or guard-only export in the exact resolver
// transaction. Any other seen-mask shape is invalid/complete and returns
// unknown.
ExportId expected_provider_lookup(std::uint32_t seen_mask) noexcept;

enum class ProviderId : std::uint8_t { dlss, isp, count };
inline constexpr std::array<ProviderId, 2> kProviders{ProviderId::dlss,
                                                      ProviderId::isp};
constexpr std::int32_t feature_id(const ProviderId provider) noexcept {
  return provider == ProviderId::dlss  ? 1
         : provider == ProviderId::isp ? 9
                                       : -1;
}
constexpr std::string_view provider_name(const ProviderId provider) noexcept {
  return provider == ProviderId::dlss  ? "dlss"
         : provider == ProviderId::isp ? "isp"
                                       : "invalid";
}

// DLSS Populate publishes a stateful function pointer, not only scalar
// metadata. Once Init is synthetic it must never call the old Populate.
constexpr bool forward_original_populate(ProviderId provider,
                                         bool synthetic_lifecycle) noexcept {
  return (provider == ProviderId::dlss || provider == ProviderId::isp) &&
         (!synthetic_lifecycle || provider == ProviderId::isp);
}

// Read under the runtime's shared route lock. Neither provider may synthesize
// Init success until two distinct modules completed the same core/thread's
// independently verified resolver transactions.
struct ProviderAdmission {
  ProviderId provider{ProviderId::count};
  std::uintptr_t module{};
  std::uintptr_t core{};
  std::uint32_t thread{};
  std::uint32_t route_mask{};
  std::uint32_t lookup_mask{};
  bool identity_verified{};
  bool committed{};
};
bool all_provider_routes_verified(
    std::span<const ProviderAdmission> providers) noexcept;

enum class Mode : std::uint8_t {
  disabled,
  capture_create,
  capture_one_evaluate,
  capture_three_evaluates,
  native_taa_fallback,
  native_depth_probe,
};
constexpr bool reusable_fallback_mode(Mode mode) noexcept {
  return mode==Mode::native_taa_fallback || mode==Mode::native_depth_probe;
}

inline constexpr std::string_view kDisabledMode = "disabled";
inline constexpr std::string_view kCaptureCreateMode = "capture-create";
inline constexpr std::string_view kCaptureOneEvaluateMode =
    "capture-one-evaluate";

constexpr unsigned evaluate_limit(Mode mode) noexcept {
  return mode == Mode::capture_three_evaluates ? 3U :
      mode == Mode::capture_one_evaluate || reusable_fallback_mode(mode) ? 1U : 0U;
}

struct ModeParseResult {
  Mode mode{Mode::disabled};
  bool recognized{};
};

// Parsing is deliberately exact: empty, misspelled, padded, or differently
// cased values are unrecognized and therefore disabled.
ModeParseResult parse_mode(std::string_view text) noexcept;
ModeParseResult parse_local_capture_opt_in(ModeParseResult environment, std::string_view text) noexcept;

// The two-provider runtime admits bounded Evaluate capture only for DLSS; its provider-mode
// policy keeps ISP in capture-create. Empty/unknown text remains disabled.
ModeParseResult parse_two_provider_mode(std::string_view text) noexcept;

// Runtime code is responsible for producing this evidence. Keeping it as
// neutral POD makes every required identity check independently unit-testable.
struct IdentityEvidence {
  bool host_binary_verified{};
  bool system_resolver_verified{};
  bool driver_core_verified{};
  bool bridge_binary_verified{};
  bool d3d12_path_verified{};
};

bool identity_is_verified(const IdentityEvidence &evidence) noexcept;

// The D3D12 Init export receives the NGX API version in its fourth argument.
// Feature snippet versions are separate metadata and must never be accepted at
// this boundary. Keep every gate independently visible for fail-closed runtime
// telemetry and unit tests.
inline constexpr std::uint64_t kExpectedInitApplicationId = 0x0B9D2F5CULL;
inline constexpr std::uint32_t kExpectedInitApiVersion = 0x12U;
inline constexpr std::uint32_t kInitGateCallerVerified = 1U << 0U;
inline constexpr std::uint32_t kInitGateLifecycleReady = 1U << 1U;
inline constexpr std::uint32_t kInitGateApplicationId = 1U << 2U;
inline constexpr std::uint32_t kInitGateApplicationPath = 1U << 3U;
inline constexpr std::uint32_t kInitGateApiVersion = 1U << 4U;
inline constexpr std::uint32_t kInitGateRouteTable = 1U << 5U;
inline constexpr std::uint32_t kInitGateD3d12Device = 1U << 6U;
inline constexpr std::uint32_t kInitValidationMaskWithoutDevice = 0x3FU;
inline constexpr std::uint32_t kInitValidationMaskComplete = 0x7FU;

struct InitValidationEvidence {
  bool caller_verified{};
  bool lifecycle_ready{};
  std::uint64_t application_id{};
  bool application_path_readable{};
  std::uint32_t api_version{};
  bool route_table_complete{};
  bool d3d12_device_verified{};
};

std::uint32_t
init_validation_mask(const InitValidationEvidence &evidence) noexcept;
bool init_validation_is_complete(const InitValidationEvidence &evidence) noexcept;

// Addresses and RVAs are scalar evidence only; this policy never reads them.
// The runtime must establish the memory predicates without passing pointers to
// untrusted storage through this layer.
struct CallerEvidence {
  ExportId requested_export{ExportId::unknown};
  std::uintptr_t module_base{};
  std::uintptr_t return_address{};
  std::uintptr_t expected_return_rva{};
  std::uintptr_t image_size{};
  bool caller_binary_verified{};
  bool memory_image{};
  bool allocation_base_matches{};
  bool executable_section{};
  bool executable_page{};
};

bool caller_is_verified_for(const CallerEvidence &evidence,
                            ExportId expected_export) noexcept;

enum class LifecycleState : std::uint8_t {
  disabled,
  awaiting_verification,
  verified,
  initialized,
  populated,
  create_captured,
  evaluate_token_issued,
  evaluate_captured,
  released,
  inert,
};

enum class LifecycleOperation : std::uint8_t {
  verify,
  initialize,
  populate,
  capture_create,
  capture_evaluate,
  release,
  shutdown,
  abort,
};

enum class TransitionDecision : std::uint8_t {
  identity_verified,
  initialized,
  populated,
  create_captured_fail_closed,
  evaluate_token_issued,
  evaluate_captured_fail_closed,
  released,
  shut_down,
  aborted,
  mode_disabled,
  invalid_configuration,
  identity_rejected,
  caller_rejected,
  invalid_order,
  token_rejected,
  already_inert,
  capacity_exhausted,
};

// These are abstract provider responses, not vendor result codes. In
// particular, success_with_token only allows the host to reach one observed
// Evaluate call; it does not claim that any rendering work occurred.
enum class BoundaryResponse : std::uint8_t {
  none,
  fail_closed,
  success,
  success_with_token,
};

struct TransitionInput {
  LifecycleOperation operation{LifecycleOperation::abort};
  IdentityEvidence identity{};
  CallerEvidence caller{};
  std::uint64_t presented_token{};
};

struct TransitionResult {
  LifecycleState before{LifecycleState::disabled};
  LifecycleState after{LifecycleState::disabled};
  TransitionDecision decision{TransitionDecision::mode_disabled};
  BoundaryResponse response{BoundaryResponse::fail_closed};
  std::uint64_t issued_token{};
  std::uint32_t sequence{};
  bool transition_applied{};
  bool capture_committed{};
  bool event_recorded{};
};

struct LifecycleEvent {
  LifecycleOperation operation{LifecycleOperation::abort};
  LifecycleState before{LifecycleState::disabled};
  LifecycleState after{LifecycleState::disabled};
  TransitionDecision decision{TransitionDecision::mode_disabled};
  BoundaryResponse response{BoundaryResponse::fail_closed};
  std::uint64_t issued_token{};
  std::uint32_t sequence{};
  bool transition_applied{};
  bool capture_committed{};
};

inline constexpr std::size_t kLifecycleEventCapacity = 10U;

// The high DWORD remains the DLSS feature index used by the pinned core.
// Never wrap/reuse a low DWORD within this process.
inline constexpr std::uint32_t kFallbackFirstToken = 0xB12DU;
inline constexpr std::uint32_t kFallbackMaxGeneration = 0xFFFFFFFFU - kFallbackFirstToken + 1U;
constexpr std::uint64_t fallback_generation_token(std::uint32_t generation) noexcept {
  return generation != 0U && generation <= kFallbackMaxGeneration
             ? (std::uint64_t{1} << 32U) | (kFallbackFirstToken + generation - 1U)
             : 0U;
}
constexpr bool is_fallback_generation_token(std::uint64_t token) noexcept {
  return (token >> 32U) == 1U && static_cast<std::uint32_t>(token) >= kFallbackFirstToken;
}
constexpr bool create_lifecycle_ready(Mode mode, LifecycleState state) noexcept {
  return state == LifecycleState::initialized || state == LifecycleState::populated ||
         (reusable_fallback_mode(mode) && state == LifecycleState::released);
}

// A single instance permits at most one Create capture and, only in the
// explicit opt-in mode, at most one Evaluate capture. The verified Populate
// callback may occur twice (capability and device parameter objects), and
// Create may follow Init directly if the driver's later population pass is
// skipped. Every rejected invariant makes the instance inert. The fixed event
// ledger never allocates. Explicit native fallback modes are reusable
// exceptions: they issue generation tokens after Release and treat full event
// storage as lost diagnostics, never as an operational failure.
class OneShotLifecycle final {
public:
  OneShotLifecycle() noexcept = default;
  explicit OneShotLifecycle(Mode mode,
                            std::uint64_t one_evaluate_token = 0U) noexcept;

  TransitionResult apply(const TransitionInput &input) noexcept;

  Mode mode() const noexcept { return mode_; }
  LifecycleState state() const noexcept { return state_; }
  bool configuration_valid() const noexcept { return configuration_valid_; }
  bool populate_budget_available() const noexcept {
    return populate_count_ < 2U;
  }
  bool overflowed() const noexcept { return overflowed_; }
  std::uint32_t generation() const noexcept { return generation_; }
  std::uint32_t dropped_event_count() const noexcept {
    return dropped_event_count_;
  }
  std::span<const LifecycleEvent> events() const noexcept {
    return std::span<const LifecycleEvent>(events_.data(), event_count_);
  }

private:
  TransitionResult commit(LifecycleOperation operation,
                          LifecycleState next_state,
                          TransitionDecision decision,
                          BoundaryResponse response,
                          std::uint64_t issued_token,
                          bool transition_applied,
                          bool capture_committed) noexcept;
  TransitionResult reject(LifecycleOperation operation,
                          TransitionDecision decision) noexcept;

  Mode mode_{Mode::disabled};
  LifecycleState state_{LifecycleState::disabled};
  std::uint64_t one_evaluate_token_{};
  std::array<LifecycleEvent, kLifecycleEventCapacity> events_{};
  std::size_t event_count_{};
  std::uint32_t dropped_event_count_{};
  std::uint8_t populate_count_{};
  unsigned evaluate_count_{};
  std::uint32_t generation_{};
  std::uint32_t transition_count_{};
  bool configuration_valid_{true};
  bool overflowed_{};
};

} // namespace mhw::bridge_probe
