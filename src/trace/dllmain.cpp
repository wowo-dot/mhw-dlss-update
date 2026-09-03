#include <windows.h>

#include <bcrypt.h>
#include <dxgi1_2.h>
#include <intrin.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <MinHook.h>

#if defined(MHW_DLSS_BRIDGE_PROBE)
#include "../bridge_probe/bridge_policy.hpp"
#include "../bridge_probe/contract_snapshot.hpp"
#include <d3d12.h>
#endif

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#if defined(_MSC_VER)
#pragma intrinsic(_ReturnAddress)
#endif

namespace {

#if defined(MHW_DLSS_BRIDGE_PROBE)
namespace bridge = mhw::bridge_probe;
#endif

using NgxResult = std::uint32_t;
using NgxParameter = void;
using PopulateParametersFn = NgxResult(__cdecl *)(NgxParameter *);
using GetParametersFn = NgxResult(__cdecl *)(NgxParameter **);
// Exact internal ABI at _nvngx.dll+0x264E0 on the pinned modern-core build.
// All pointer arguments remain opaque; the observer records only nullness and
// never dereferences them.
using FeatureStatusRecorderFn = void(__cdecl *)(
    void *, const void *, void *, std::int32_t, void *, std::uint32_t,
    NgxResult);
using GetProcAddressForCallerFn = FARPROC(WINAPI *)(HMODULE, LPCSTR, void *);

#if defined(MHW_DLSS_BRIDGE_PROBE)
using Feature = std::int32_t;
using BridgeInitFn = NgxResult(__cdecl *)(std::uint64_t, const wchar_t *,
                                         ID3D12Device *, std::uint32_t);
using BridgeScratchFn = NgxResult(__cdecl *)(Feature, const NgxParameter *,
                                            std::size_t *);
using BridgeCreateFn = NgxResult(__cdecl *)(ID3D12GraphicsCommandList *,
                                           Feature, const NgxParameter *,
                                           void **);
using BridgeProgressCallback = void(__cdecl *)(float, bool &);
using BridgeEvaluateFn = NgxResult(__cdecl *)(ID3D12GraphicsCommandList *,
                                             const void *,
                                             const NgxParameter *,
                                             BridgeProgressCallback);
using BridgeReleaseFn = NgxResult(__cdecl *)(void *);
using BridgeShutdownFn = NgxResult(__cdecl *)();
using ParameterGetIntFn = NgxResult(__cdecl *)(NgxParameter *, const char *,
                                              std::int32_t *);
using ParameterGetUIntFn = NgxResult(__cdecl *)(NgxParameter *, const char *,
                                               std::uint32_t *);
using ParameterGetFloatFn = NgxResult(__cdecl *)(NgxParameter *, const char *,
                                                float *);
using ParameterGetResourceFn = NgxResult(__cdecl *)(NgxParameter *,
                                                   const char *,
                                                   ID3D12Resource **);
#endif

constexpr std::size_t kHashBufferSize = 1U << 20U;
constexpr std::uint64_t kMaximumHashedFileSize = 128ULL << 20U;
constexpr DWORD kTraceDurationMilliseconds = 30'000U;
#if defined(MHW_DLSS_BRIDGE_PROBE)
constexpr DWORD kBridgeTraceDurationMilliseconds = 300'000U;
#endif
constexpr DWORD kTracePollMilliseconds = 25U;
constexpr DWORD kCensusIntervalMilliseconds = 250U;
constexpr DWORD kQuiesceTimeoutMilliseconds = 5'000U;
constexpr std::size_t kEventCapacity = 4U;
constexpr std::size_t kResolverEventCapacity = 32U;
constexpr std::size_t kSlotCount = 7U;
constexpr std::uint32_t kMaximumSnapshotEntries = 2'048U;
constexpr std::size_t kMaximumCensusModules = 16U;
constexpr std::uint32_t kResultSuccess = 0x00000001U;
constexpr std::uint32_t kResultFail = 0xBAD00000U;
constexpr std::uint32_t kResultFeatureNotSupported = 0xBAD00001U;
constexpr std::uint32_t kResultPlatformError = 0xBAD00002U;
#if defined(MHW_DLSS_BRIDGE_PROBE)
constexpr std::uint32_t kResultInvalidParameter = 0xBAD00005U;
constexpr std::uint32_t kResultNotInitialized = 0xBAD00007U;
constexpr std::uint32_t kResultNotImplemented = 0xBAD00012U;
constexpr std::uint64_t kOneEvaluateToken = 0x000000010000B12DULL;
constexpr std::size_t kExpectedScratchBufferSize = 0x00300000U;
constexpr std::size_t kMaximumDriverStoreDirectories = 16'384U;
constexpr std::size_t kResolverPathCapacity = 1'024U;
#endif
constexpr std::uint32_t kExpectedHostTimestamp = 0x66D13CB6U;
constexpr std::uint64_t kExpectedHostPreferredImageBase = 0x140000000ULL;
constexpr std::uint32_t kExpectedHostImageSize = 0x55D3000U;
constexpr std::uint64_t kExpectedNvidiaPreferredImageBase = 0x180000000ULL;

constexpr std::array<unsigned char, 32> kExpectedHostHash{
    0xC2, 0xEB, 0xBB, 0xD2, 0xC4, 0x9F, 0x21, 0x6D,
    0x48, 0x4E, 0x31, 0xA5, 0x21, 0x9B, 0xED, 0x41,
    0x9E, 0xB1, 0xE5, 0xE7, 0xD2, 0x06, 0xD0, 0x2C,
    0xBA, 0x04, 0x0A, 0x3A, 0xB7, 0x9D, 0x90, 0xEA};
constexpr std::array<unsigned char, 32> kExpectedKernelBaseHash{
    0x9D, 0x26, 0x85, 0x24, 0x59, 0x74, 0x53, 0x22,
    0xB7, 0x04, 0xE2, 0xA4, 0x9E, 0x44, 0x89, 0x22,
    0x83, 0x9C, 0x58, 0x4D, 0x06, 0xE2, 0x08, 0xCF,
    0x9A, 0xBC, 0xD0, 0xD8, 0x57, 0xC4, 0xB7, 0xCD};
constexpr std::array<unsigned char, 32> kExpectedKernel32Hash{
    0x0A, 0xB6, 0x1F, 0x2E, 0x0D, 0x41, 0x2A, 0x58,
    0x52, 0x33, 0xF1, 0xB3, 0x08, 0xC1, 0x20, 0xCB,
    0xA7, 0x4F, 0x80, 0x0C, 0x10, 0x30, 0xBF, 0xFE,
    0x0A, 0xAB, 0xD2, 0x0D, 0xF8, 0xC6, 0xD9, 0x07};

constexpr std::array<unsigned char, 32> kLocalFeatureHash{
    0xD4, 0xCC, 0x11, 0xBC, 0xEC, 0x69, 0x3D, 0xFE,
    0xFD, 0x6E, 0xDD, 0xBA, 0x6D, 0xD2, 0x9B, 0xDF,
    0xC1, 0x6B, 0x40, 0x43, 0xE7, 0xB8, 0x31, 0x50,
    0xF0, 0x09, 0x7D, 0x14, 0xB7, 0x65, 0x12, 0xD8};
constexpr std::array<unsigned char, 32> kCacheFeatureHash{
    0xDB, 0x20, 0x38, 0x81, 0xEA, 0x50, 0x28, 0xCB,
    0x58, 0x4A, 0xA6, 0xC5, 0x8D, 0x45, 0x73, 0x61,
    0xDD, 0x0E, 0x6A, 0x28, 0x17, 0x67, 0x98, 0x78,
    0x09, 0x8B, 0x08, 0xB9, 0x40, 0x54, 0x9A, 0x03};
constexpr std::array<unsigned char, 32> kLegacyCoreHash{
    0x3F, 0x59, 0x85, 0x82, 0xD4, 0xC2, 0x35, 0x30,
    0xEF, 0x2F, 0x8B, 0x31, 0x2A, 0xCA, 0x25, 0x61,
    0x75, 0x3E, 0x1C, 0x7B, 0x54, 0x0A, 0x78, 0x22,
    0xDE, 0xFD, 0xC6, 0xDC, 0x23, 0xAC, 0x8D, 0x12};
constexpr std::array<unsigned char, 32> kModernCoreHash{
    0xB3, 0xC8, 0x26, 0x38, 0xC6, 0x0F, 0x29, 0xAE,
    0xFF, 0x68, 0x05, 0x2F, 0xFE, 0x71, 0x47, 0x24,
    0x6A, 0x73, 0xC8, 0x0C, 0xBE, 0xA5, 0x88, 0x3B,
    0x35, 0xA9, 0xB0, 0xD1, 0xED, 0x57, 0xDD, 0xE7};

constexpr std::array<unsigned char, 21> kKernelBaseResolverPrologue{
    0x40, 0x53, 0x55, 0x56, 0x57, 0x48, 0x83, 0xEC, 0x48, 0x33, 0xFF,
    0x4C, 0x89, 0x74, 0x24, 0x70, 0x4C, 0x89, 0x7C, 0x24, 0x40};
constexpr std::array<unsigned char, 20> kLocalPopulatePrologue{
    0x40, 0x55, 0x53, 0x41, 0x56, 0x48, 0x8D, 0xAC, 0x24, 0x50,
    0xFD, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0xB0, 0x03, 0x00, 0x00};
constexpr std::array<unsigned char, 20> kCachePopulatePrologue{
    0x40, 0x55, 0x53, 0x41, 0x56, 0x48, 0x8D, 0xAC, 0x24, 0x40,
    0xFD, 0xFF, 0xFF, 0x48, 0x81, 0xEC, 0xC0, 0x03, 0x00, 0x00};
constexpr std::array<unsigned char, 21> kModernCapabilityPrologue{
    0x40, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05, 0x4B, 0x51,
    0x07, 0x00, 0x48, 0x8B, 0xF9, 0x48, 0x85, 0xC0, 0x75, 0x3B};
constexpr std::array<unsigned char, 21> kModernParametersPrologue{
    0x40, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05, 0xEB, 0x4E,
    0x07, 0x00, 0x48, 0x8B, 0xF9, 0x48, 0x85, 0xC0, 0x75, 0x46};
constexpr std::array<unsigned char, 28> kModernFeatureStatusPrologue{
    0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41,
    0x56, 0x41, 0x57, 0x48, 0x8D, 0xAC, 0x24, 0x18, 0xFA, 0xFF,
    0xFF, 0x48, 0x81, 0xEC, 0xE8, 0x06, 0x00, 0x00};

#if defined(MHW_DLSS_BRIDGE_PROBE)
constexpr std::array<unsigned char, 16> kCacheShutdownPrologue{
    0x48, 0x83, 0xEC, 0x28, 0x48, 0x83, 0x3D, 0x34,
    0xBA, 0xB2, 0x00, 0x00, 0x75, 0x0A, 0xB8, 0x07};
constexpr std::array<unsigned char, 16> kCacheScratchPrologue{
    0x48, 0x83, 0x3D, 0xD8, 0xB9, 0xB2, 0x00, 0x00,
    0x75, 0x06, 0xB8, 0x07, 0x00, 0xD0, 0xBA, 0xC3};
constexpr std::array<unsigned char, 16> kCacheCreatePrologue{
    0x48, 0x83, 0x3D, 0xA8, 0xB9, 0xB2, 0x00, 0x00,
    0x75, 0x06, 0xB8, 0x07, 0x00, 0xD0, 0xBA, 0xC3};
constexpr std::array<unsigned char, 16> kCacheReleasePrologue{
    0x48, 0x83, 0x3D, 0x88, 0xB9, 0xB2, 0x00, 0x00,
    0x75, 0x06, 0xB8, 0x07, 0x00, 0xD0, 0xBA, 0xC3};
constexpr std::array<unsigned char, 16> kCacheEvaluatePrologue{
    0x48, 0x83, 0xEC, 0x38, 0x48, 0x83, 0x3D, 0x64,
    0xB9, 0xB2, 0x00, 0x00, 0x75, 0x0A, 0xB8, 0x07};
constexpr std::array<unsigned char, 16> kCacheInitPrologue{
    0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x81,
    0xEC, 0x60, 0x02, 0x00, 0x00, 0x48, 0x8B, 0x05};
#endif

enum class Slot : std::uint32_t {
  local_populate,
  cache_populate,
  legacy_capability,
  legacy_parameters,
  modern_capability,
  modern_parameters,
  modern_d3d12_feature_status,
};

constexpr std::array<Slot, kSlotCount> kAllSlots{
    Slot::local_populate,
    Slot::cache_populate,
    Slot::legacy_capability,
    Slot::legacy_parameters,
    Slot::modern_capability,
    Slot::modern_parameters,
    Slot::modern_d3d12_feature_status};

// The modern core exposes the downstream status boundary. Verify it first so
// the worker can arm that hook before feature DLL initialization begins.
constexpr std::array<Slot, kSlotCount> kIdentityVerificationOrder{
    Slot::modern_capability,
    Slot::modern_parameters,
    Slot::legacy_capability,
    Slot::legacy_parameters,
    Slot::local_populate,
    Slot::cache_populate,
    Slot::modern_d3d12_feature_status};

enum class TargetName : std::uint32_t {
  unknown,
  populate,
  d3d12_init,
  capability_parameters,
  parameters,
#if defined(MHW_DLSS_BRIDGE_PROBE)
  d3d12_scratch,
  d3d12_create,
  d3d12_evaluate,
  d3d12_evaluate_c,
  d3d12_release,
  d3d12_shutdown,
  d3d12_shutdown1,
  d3d12_create1,
  d3d12_init_ext,
  d3d12_init_ext1,
  d3d12_init_ext2,
  d3d12_populate_parameters_impl,
  d3d12_populate_device_parameters_impl,
#endif
};

enum class ResolverDisposition : std::uint32_t {
  passthrough,
  substituted,
  rejected,
  null_result,
  guarded_null,
};

enum class IdentityState : std::uint32_t {
  empty,
  pending,
  verified,
  rejected,
};

enum class EventState : std::uint32_t {
  empty,
  writing,
  ready,
  logged,
};

struct PeIdentity {
  std::uint16_t machine{};
  std::uint16_t optional_magic{};
  std::uint32_t timestamp{};
  std::uint64_t image_base{};
  std::uint32_t image_size{};
};

struct TraceEventPayload {
  std::uint64_t sequence{};
  ULONGLONG tick{};
  ULONGLONG duration_ticks{};
  DWORD thread_id{};
  std::uintptr_t caller{};
  std::uintptr_t argument{};
  NgxResult result{};
  std::int32_t feature_index{-1};
  std::uint32_t api{};
  bool argument_nonnull{};
};

struct TraceEvent {
  std::atomic<EventState> state{EventState::empty};
  TraceEventPayload payload{};
};

struct SlotRuntime {
  std::atomic<std::uintptr_t> target{};
  std::atomic<std::uintptr_t> module{};
  std::atomic<std::uintptr_t> resolver_caller{};
  std::atomic<std::uint64_t> resolver_tick{};
  std::atomic<IdentityState> identity{IdentityState::empty};
  std::atomic<std::uint64_t> resolves{};
  std::atomic<std::uint64_t> calls{};
  std::atomic<std::uint64_t> success{};
  std::atomic<std::uint64_t> bad00000{};
  std::atomic<std::uint64_t> bad00001{};
  std::atomic<std::uint64_t> bad00002{};
  std::atomic<std::uint64_t> other{};
  std::atomic<std::uint64_t> recursion{};
  std::atomic<std::uint64_t> conflicts{};
  std::atomic<std::uint64_t> dropped{};
  std::atomic<std::uint32_t> reserved_events{};
  std::array<TraceEvent, kEventCapacity> events{};
};

struct ResolverEventPayload {
  std::uint64_t sequence{};
  ULONGLONG tick{};
  DWORD thread_id{};
  TargetName name{TargetName::unknown};
  ResolverDisposition disposition{ResolverDisposition::rejected};
  std::uintptr_t module{};
  std::uintptr_t target{};
  std::uintptr_t target_rva{};
  std::uintptr_t caller{};
  std::uint32_t mapped_timestamp{};
  std::uint32_t mapped_image_size{};
  bool target_rva_valid{};
  bool mapped_identity_available{};
};

struct ResolverEvent {
  std::atomic<EventState> state{EventState::empty};
  ResolverEventPayload payload{};
};

#if defined(MHW_DLSS_BRIDGE_PROBE)
inline constexpr std::size_t kBridgeEventCapacity = 16U;

struct BridgeEventPayload {
  std::uint64_t sequence{};
  ULONGLONG tick{};
  DWORD thread_id{};
  bridge::ExportId export_id{bridge::ExportId::unknown};
  std::uintptr_t caller{};
  NgxResult result{};
  bridge::LifecycleState before{bridge::LifecycleState::disabled};
  bridge::LifecycleState after{bridge::LifecycleState::disabled};
  bridge::TransitionDecision decision{bridge::TransitionDecision::mode_disabled};
  bridge::BoundaryResponse response{bridge::BoundaryResponse::fail_closed};
  std::uint64_t token{};
  std::uint64_t argument0{};
  std::uint64_t argument1{};
  std::uint32_t init_validation_mask{};
  bool capture_committed{};
  bool d3d12_object_verified{};
  bool observation_only{};
  bool has_create_snapshot{};
  bool has_evaluate_snapshot{};
  bridge::CreateContractSnapshot create_snapshot{};
  bridge::EvaluateContractSnapshot evaluate_snapshot{};
};

struct BridgeEvent {
  std::atomic<EventState> state{EventState::empty};
  BridgeEventPayload payload{};
};
#endif

static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<EventState>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(kSlotCount <= 32U);

std::array<SlotRuntime, kSlotCount> g_slots{};
std::array<ResolverEvent, kResolverEventCapacity> g_resolver_events{};
std::atomic<std::uint64_t> g_event_sequence{};
std::atomic<std::uint64_t> g_resolver_calls{};
std::atomic<std::uint64_t> g_name_rejects{};
std::atomic<std::uint64_t> g_target_rejects{};
std::atomic<std::uint64_t> g_resolver_events_reserved{};
std::atomic<std::uint64_t> g_resolver_events_dropped{};
std::atomic<std::uint64_t> g_d3d12_init_passthroughs{};
std::atomic<std::uint64_t> g_feature_d3d12_init_lookups{};
std::atomic<std::uint64_t> g_first_feature_d3d12_init_lookup_tick{};
// 0=no feature lookup, 1=hook not ready at first lookup, 2=hook ready.
std::atomic<int> g_hook_readiness_at_first_feature_init_lookup{};
std::atomic<std::uint64_t> g_target_nulls{};
std::atomic<std::uint64_t> g_resolver_before_ready{};
std::atomic<std::uint64_t> g_active_resolver_detours{};
std::atomic<std::uint64_t> g_active_wrapper_calls{};
std::atomic<bool> g_trace_enabled{};
std::atomic<int> g_trace_state{};
std::atomic<std::uintptr_t> g_host_base{};
std::atomic<std::uint32_t> g_host_image_size{};
std::atomic<std::uintptr_t> g_original_resolver{};
std::atomic<std::uintptr_t> g_original_feature_status_recorder{};
// 0=waiting for verified modern core, 1=preparing, 2=enabled, -1=unavailable.
std::atomic<int> g_feature_status_hook_state{};
std::atomic<std::uint64_t> g_feature_status_hook_tick{};
std::atomic<bool> g_feature_status_hook_ready{};
std::atomic<std::uint64_t> g_feature_status_detours{};
std::atomic<std::uint64_t> g_feature_status_gate_rejects{};
std::atomic<std::uintptr_t> g_slot_ready_event{};
#if defined(MHW_DLSS_BRIDGE_PROBE)
std::array<BridgeEvent, kBridgeEventCapacity> g_bridge_events{};
std::atomic<std::uint32_t> g_bridge_events_reserved{};
std::atomic<std::uint32_t> g_bridge_events_dropped{};
std::atomic<std::uint64_t> g_bridge_routes{};
std::atomic<std::uint64_t> g_bridge_route_rejects{};
std::atomic<std::uint64_t> g_bridge_guard_lookups{};
std::atomic<std::uint64_t> g_bridge_guard_rejects{};
std::atomic<std::uint64_t> g_bridge_guard_suppressions{};
std::atomic<std::uint64_t> g_bridge_callback_routes{};
std::atomic<bool> g_bridge_cache_ready{};
std::atomic<bool> g_bridge_environment_ready{};
// 0=not armed, 1=exact transaction armed, -1=intercepted then rejected,
// -2=Init lookup missed; this run remains entirely pass-through/inconclusive.
std::atomic<int> g_bridge_route_state{};
std::atomic<bool> g_bridge_ever_armed{};
std::atomic<bool> g_bridge_interception_started{};
std::atomic<std::uintptr_t> g_bridge_cache_base{};
std::atomic<std::uintptr_t> g_bridge_modern_core_base{};
std::atomic<std::uint32_t> g_bridge_route_mask{};
std::atomic<std::uint32_t> g_bridge_lookup_seen_mask{};
std::atomic<bool> g_bridge_lookup_transaction_complete{};
std::atomic<DWORD> g_bridge_route_thread_id{};
std::atomic<bool> g_bridge_init_succeeded{};
std::atomic<bool> g_bridge_token_issued{};
std::atomic<std::uint64_t> g_bridge_scratch_calls{};
std::atomic<std::uintptr_t> g_bridge_first_scratch_caller{};
std::atomic<NgxResult> g_bridge_last_scratch_result{};
// 0=idle, 1=owned/clean, 2=owned/poisoned, 3=idle/poisoned. A contender
// poisons the active transaction instead of waiting through foreign callbacks.
std::atomic<std::uint32_t> g_bridge_boundary_state{};
std::atomic<std::uint64_t> g_bridge_boundary_contentions{};
std::atomic<std::uintptr_t> g_parameter_get_int{};
std::atomic<std::uintptr_t> g_parameter_get_uint{};
std::atomic<std::uintptr_t> g_parameter_get_float{};
std::atomic<std::uintptr_t> g_parameter_get_resource{};
std::atomic<std::uint32_t> g_expected_cache_path_length{};
wchar_t g_expected_cache_path[32'768]{};
std::atomic<std::uint32_t> g_expected_modern_core_path_length{};
wchar_t g_expected_modern_core_path[32'768]{};
SRWLOCK g_bridge_lifecycle_lock = SRWLOCK_INIT;
SRWLOCK g_bridge_route_lock = SRWLOCK_INIT;
std::atomic<bridge::Mode> g_bridge_mode{bridge::Mode::disabled};
bridge::OneShotLifecycle g_bridge_lifecycle{};
bridge::CaptureState g_bridge_capture_state{};
constinit const std::uint64_t g_bridge_token_storage{kOneEvaluateToken};
#endif
HMODULE g_self{};
ULONGLONG g_attach_tick{};
thread_local std::uint32_t g_active_slots{};
std::ofstream g_log;

constexpr std::size_t slot_index(const Slot slot) noexcept {
  return static_cast<std::size_t>(slot);
}

SlotRuntime &slot_runtime(const Slot slot) noexcept {
  return g_slots[slot_index(slot)];
}

const char *slot_name(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return "local-populate";
  case Slot::cache_populate:
    return "cache-populate";
  case Slot::legacy_capability:
    return "legacy-core-capability";
  case Slot::legacy_parameters:
    return "legacy-core-parameters";
  case Slot::modern_capability:
    return "modern-core-capability";
  case Slot::modern_parameters:
    return "modern-core-parameters";
  case Slot::modern_d3d12_feature_status:
    return "modern-d3d12-feature-status";
  }
  return "unknown";
}

bool is_feature_status_slot(const Slot slot) noexcept {
  return slot == Slot::modern_d3d12_feature_status;
}

const char *target_name_text(const TargetName name) noexcept {
  switch (name) {
  case TargetName::populate:
    return "NVSDK_NGX_PopulateParameters";
  case TargetName::d3d12_init:
    return "NVSDK_NGX_D3D12_Init";
  case TargetName::capability_parameters:
    return "NVSDK_NGX_D3D12_GetCapabilityParameters";
  case TargetName::parameters:
    return "NVSDK_NGX_D3D12_GetParameters";
#if defined(MHW_DLSS_BRIDGE_PROBE)
  case TargetName::d3d12_scratch:
    return "NVSDK_NGX_D3D12_GetScratchBufferSize";
  case TargetName::d3d12_create:
    return "NVSDK_NGX_D3D12_CreateFeature";
  case TargetName::d3d12_evaluate:
    return "NVSDK_NGX_D3D12_EvaluateFeature";
  case TargetName::d3d12_evaluate_c:
    return "NVSDK_NGX_D3D12_EvaluateFeature_C";
  case TargetName::d3d12_release:
    return "NVSDK_NGX_D3D12_ReleaseFeature";
  case TargetName::d3d12_shutdown:
    return "NVSDK_NGX_D3D12_Shutdown";
  case TargetName::d3d12_shutdown1:
    return "NVSDK_NGX_D3D12_Shutdown1";
  case TargetName::d3d12_create1:
    return "NVSDK_NGX_D3D12_CreateFeature1";
  case TargetName::d3d12_init_ext:
    return "NVSDK_NGX_D3D12_Init_Ext";
  case TargetName::d3d12_init_ext1:
    return "NVSDK_NGX_D3D12_Init_Ext1";
  case TargetName::d3d12_init_ext2:
    return "NVSDK_NGX_D3D12_Init_Ext2";
  case TargetName::d3d12_populate_parameters_impl:
    return "NVSDK_NGX_D3D12_PopulateParameters_Impl";
  case TargetName::d3d12_populate_device_parameters_impl:
    return "NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl";
#endif
  case TargetName::unknown:
    break;
  }
  return "unknown";
}

const char *resolver_disposition_text(
    const ResolverDisposition disposition) noexcept {
  switch (disposition) {
  case ResolverDisposition::passthrough:
    return "passthrough";
  case ResolverDisposition::substituted:
    return "substituted";
  case ResolverDisposition::rejected:
    return "rejected";
  case ResolverDisposition::null_result:
    return "null-result";
  case ResolverDisposition::guarded_null:
    return "guarded-null";
  }
  return "unknown";
}

const char *identity_name(const IdentityState state) noexcept {
  switch (state) {
  case IdentityState::empty:
    return "empty";
  case IdentityState::pending:
    return "pending";
  case IdentityState::verified:
    return "verified";
  case IdentityState::rejected:
    return "rejected";
  }
  return "unknown";
}

const char *feature_status_hook_state_name(const int state) noexcept {
  switch (state) {
  case -1:
    return "unavailable";
  case 0:
    return "waiting";
  case 1:
    return "preparing";
  case 2:
    return "enabled";
  default:
    return "unknown";
  }
}

const char *ngx_result_name(const NgxResult result) noexcept {
  switch (result) {
  case kResultSuccess:
    return "success";
  case kResultFail:
    return "fail";
  case kResultFeatureNotSupported:
    return "feature-not-supported";
  case kResultPlatformError:
    return "platform-error";
  case 0xBAD00003U:
    return "already-exists";
  case 0xBAD00004U:
    return "not-found";
  case 0xBAD00005U:
    return "invalid-parameter";
  case 0xBAD00006U:
    return "scratch-buffer-too-small";
  case 0xBAD00007U:
    return "not-initialized";
  case 0xBAD00008U:
    return "unsupported-input-format";
  case 0xBAD00009U:
    return "rw-flag-missing";
  case 0xBAD0000AU:
    return "missing-input";
  case 0xBAD0000BU:
    return "unable-to-initialize-feature";
  case 0xBAD0000CU:
    return "out-of-date";
  case 0xBAD0000DU:
    return "out-of-gpu-memory";
  case 0xBAD0000EU:
    return "unsupported-format";
  case 0xBAD0000FU:
    return "unable-to-write-app-data-path";
  case 0xBAD00010U:
    return "unsupported-parameter";
  case 0xBAD00011U:
    return "denied";
  case 0xBAD00012U:
    return "not-implemented";
  default:
    return "other";
  }
}

#if defined(MHW_DLSS_BRIDGE_PROBE)
const char *bridge_state_text(const bridge::LifecycleState state) noexcept {
  switch (state) {
  case bridge::LifecycleState::disabled:
    return "disabled";
  case bridge::LifecycleState::awaiting_verification:
    return "awaiting-verification";
  case bridge::LifecycleState::verified:
    return "verified";
  case bridge::LifecycleState::initialized:
    return "initialized";
  case bridge::LifecycleState::populated:
    return "populated";
  case bridge::LifecycleState::create_captured:
    return "create-captured";
  case bridge::LifecycleState::evaluate_token_issued:
    return "evaluate-token-issued";
  case bridge::LifecycleState::evaluate_captured:
    return "evaluate-captured";
  case bridge::LifecycleState::released:
    return "released";
  case bridge::LifecycleState::inert:
    return "inert";
  }
  return "unknown";
}

const char *bridge_decision_text(
    const bridge::TransitionDecision decision) noexcept {
  switch (decision) {
  case bridge::TransitionDecision::identity_verified:
    return "identity-verified";
  case bridge::TransitionDecision::initialized:
    return "initialized";
  case bridge::TransitionDecision::populated:
    return "populated";
  case bridge::TransitionDecision::create_captured_fail_closed:
    return "create-captured-fail-closed";
  case bridge::TransitionDecision::evaluate_token_issued:
    return "evaluate-token-issued";
  case bridge::TransitionDecision::evaluate_captured_fail_closed:
    return "evaluate-captured-fail-closed";
  case bridge::TransitionDecision::released:
    return "released";
  case bridge::TransitionDecision::shut_down:
    return "shut-down";
  case bridge::TransitionDecision::aborted:
    return "aborted";
  case bridge::TransitionDecision::mode_disabled:
    return "mode-disabled";
  case bridge::TransitionDecision::invalid_configuration:
    return "invalid-configuration";
  case bridge::TransitionDecision::identity_rejected:
    return "identity-rejected";
  case bridge::TransitionDecision::caller_rejected:
    return "caller-rejected";
  case bridge::TransitionDecision::invalid_order:
    return "invalid-order";
  case bridge::TransitionDecision::token_rejected:
    return "token-rejected";
  case bridge::TransitionDecision::already_inert:
    return "already-inert";
  case bridge::TransitionDecision::capacity_exhausted:
    return "capacity-exhausted";
  }
  return "unknown";
}

const char *bridge_response_text(
    const bridge::BoundaryResponse response) noexcept {
  switch (response) {
  case bridge::BoundaryResponse::none:
    return "none";
  case bridge::BoundaryResponse::fail_closed:
    return "fail-closed";
  case bridge::BoundaryResponse::success:
    return "success";
  case bridge::BoundaryResponse::success_with_token:
    return "success-with-token";
  }
  return "unknown";
}

const char *capture_stop_reason_text(
    const bridge::CaptureStopReason reason) noexcept {
  switch (reason) {
  case bridge::CaptureStopReason::none:
    return "none";
  case bridge::CaptureStopReason::callback_missing:
    return "callback-missing";
  case bridge::CaptureStopReason::callback_fault:
    return "callback-fault";
  case bridge::CaptureStopReason::query_capacity_exhausted:
    return "query-capacity-exhausted";
  case bridge::CaptureStopReason::invalid_state:
    return "invalid-state";
  }
  return "unknown";
}
#endif

std::filesystem::path module_path(const HMODULE module) {
  wchar_t path[32'768]{};
  const DWORD length =
      GetModuleFileNameW(module, path, static_cast<DWORD>(_countof(path)));
  if (length == 0U || length >= _countof(path)) {
    return {};
  }
  return std::filesystem::path(path, path + length);
}

std::filesystem::path trace_log_path() {
  wchar_t local_app_data[32'768]{};
  const DWORD length = GetEnvironmentVariableW(
      L"LOCALAPPDATA", local_app_data,
      static_cast<DWORD>(_countof(local_app_data)));
  if (length != 0U && length < _countof(local_app_data)) {
#if defined(MHW_DLSS_BRIDGE_PROBE)
    return std::filesystem::path(local_app_data) / L"MhwDlssBridgeProbe" /
           (L"bridge-" + std::to_wstring(GetCurrentProcessId()) + L".log");
#else
    return std::filesystem::path(local_app_data) / L"MhwDlssUnlock" /
           (L"unlock-" + std::to_wstring(GetCurrentProcessId()) + L".log");
#endif
  }
#if defined(MHW_DLSS_BRIDGE_PROBE)
  return std::filesystem::temp_directory_path() /
         (L"mhw-dlss-bridge-" + std::to_wstring(GetCurrentProcessId()) +
          L".log");
#else
  return std::filesystem::temp_directory_path() /
         (L"mhw-dlss-unlock-" + std::to_wstring(GetCurrentProcessId()) +
          L".log");
#endif
}

void open_log() noexcept {
  try {
    const std::filesystem::path path = trace_log_path();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    g_log.open(path, std::ios::out | std::ios::trunc);
  } catch (...) {
  }
}

void write_log(const std::string_view message) {
  if (!g_log.is_open()) {
    return;
  }
  g_log << GetTickCount64() << "\ttid=" << GetCurrentThreadId() << '\t'
        << message << '\n';
  g_log.flush();
}

std::string utf8(const std::wstring_view value) {
  if (value.empty()) {
    return {};
  }
  const int needed = WideCharToMultiByte(
      CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0,
      nullptr, nullptr);
  if (needed <= 0) {
    return "<conversion-failed>";
  }
  std::string result(static_cast<std::size_t>(needed), '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, value.data(),
                          static_cast<int>(value.size()), result.data(), needed,
                          nullptr, nullptr) != needed) {
    return "<conversion-failed>";
  }
  return result;
}

std::string hexadecimal(const std::uint64_t value) {
  char buffer[19]{};
  _snprintf_s(buffer, _TRUNCATE, "0x%llX",
              static_cast<unsigned long long>(value));
  return buffer;
}

std::string sha256_text(const std::array<unsigned char, 32> &hash) {
  constexpr char digits[] = "0123456789ABCDEF";
  std::string result(hash.size() * 2U, '0');
  for (std::size_t index = 0U; index < hash.size(); ++index) {
    const unsigned char value = hash[index];
    result[index * 2U] = digits[value >> 4U];
    result[index * 2U + 1U] = digits[value & 0x0FU];
  }
  return result;
}

bool sha256_handle(const HANDLE file,
                   std::array<unsigned char, 32> *const output) noexcept {
  if (file == nullptr || file == INVALID_HANDLE_VALUE || output == nullptr) {
    return false;
  }

  BCRYPT_ALG_HANDLE algorithm{};
  BCRYPT_HASH_HANDLE hash{};
  unsigned char *hash_object{};
  unsigned char *buffer{};
  DWORD object_size{};
  DWORD result_size{};
  DWORD hash_size{};
  LARGE_INTEGER file_size{};
  LARGE_INTEGER zero{};
  std::array<unsigned char, 32> digest{};
  std::uint64_t total_bytes{};
  bool success = false;
  NTSTATUS status{};

  if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart < 0 ||
      static_cast<std::uint64_t>(file_size.QuadPart) >
          kMaximumHashedFileSize ||
      !SetFilePointerEx(file, zero, nullptr, FILE_BEGIN)) {
    goto cleanup;
  }
  status =
      BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
  if (status != 0) {
    goto cleanup;
  }
  status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                             reinterpret_cast<PUCHAR>(&object_size),
                             sizeof(object_size), &result_size, 0);
  if (status != 0) {
    goto cleanup;
  }
  status = BCryptGetProperty(algorithm, BCRYPT_HASH_LENGTH,
                             reinterpret_cast<PUCHAR>(&hash_size),
                             sizeof(hash_size), &result_size, 0);
  if (status != 0 || hash_size != digest.size()) {
    goto cleanup;
  }
  hash_object =
      static_cast<unsigned char *>(HeapAlloc(GetProcessHeap(), 0, object_size));
  if (hash_object == nullptr) {
    goto cleanup;
  }
  status = BCryptCreateHash(algorithm, &hash, hash_object, object_size, nullptr,
                            0, 0);
  if (status != 0) {
    goto cleanup;
  }
  buffer = static_cast<unsigned char *>(
      HeapAlloc(GetProcessHeap(), 0, kHashBufferSize));
  if (buffer == nullptr) {
    goto cleanup;
  }
  for (;;) {
    DWORD bytes_read{};
    if (!ReadFile(file, buffer, static_cast<DWORD>(kHashBufferSize), &bytes_read,
                  nullptr)) {
      goto cleanup;
    }
    if (bytes_read == 0U) {
      break;
    }
    total_bytes += bytes_read;
    status = BCryptHashData(hash, buffer, bytes_read, 0);
    if (status != 0) {
      goto cleanup;
    }
  }
  if (total_bytes != static_cast<std::uint64_t>(file_size.QuadPart)) {
    goto cleanup;
  }
  status = BCryptFinishHash(hash, digest.data(),
                            static_cast<ULONG>(digest.size()), 0);
  if (status != 0) {
    goto cleanup;
  }
  *output = digest;
  success = true;

cleanup:
  if (hash != nullptr) {
    BCryptDestroyHash(hash);
  }
  if (algorithm != nullptr) {
    BCryptCloseAlgorithmProvider(algorithm, 0);
  }
  if (buffer != nullptr) {
    HeapFree(GetProcessHeap(), 0, buffer);
  }
  if (hash_object != nullptr) {
    HeapFree(GetProcessHeap(), 0, hash_object);
  }
  return success;
}

bool sha256_file(const std::filesystem::path &path,
                 std::array<unsigned char, 32> *const output) noexcept {
  const HANDLE file =
      CreateFileW(path.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING,
                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  const bool result = sha256_handle(file, output);
  CloseHandle(file);
  return result;
}

bool hash_matches(const std::filesystem::path &path,
                  const std::array<unsigned char, 32> &expected) noexcept {
  std::array<unsigned char, 32> actual{};
  return sha256_file(path, &actual) && actual == expected;
}

bool read_mapped_pe_identity(const HMODULE module,
                             PeIdentity *const output) noexcept {
  if (module == nullptr || output == nullptr) {
    return false;
  }
  const auto *base = reinterpret_cast<const unsigned char *>(module);
  __try {
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::uint32_t>(dos->e_lfanew) > 0x10'0000U) {
      return false;
    }
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
        base + static_cast<std::size_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64)) {
      return false;
    }
    output->machine = nt->FileHeader.Machine;
    output->optional_magic = nt->OptionalHeader.Magic;
    output->timestamp = nt->FileHeader.TimeDateStamp;
    output->image_base = nt->OptionalHeader.ImageBase;
    output->image_size = nt->OptionalHeader.SizeOfImage;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool read_disk_pe_identity(const std::filesystem::path &path,
                           PeIdentity *const output) noexcept {
  if (output == nullptr) {
    return false;
  }
  const HANDLE file =
      CreateFileW(path.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  IMAGE_DOS_HEADER dos{};
  IMAGE_NT_HEADERS64 nt{};
  DWORD bytes_read{};
  bool result = false;
  if (ReadFile(file, &dos, static_cast<DWORD>(sizeof(dos)), &bytes_read,
               nullptr) &&
      bytes_read == sizeof(dos) && dos.e_magic == IMAGE_DOS_SIGNATURE &&
      dos.e_lfanew > 0 &&
      static_cast<std::uint32_t>(dos.e_lfanew) <= 0x10'0000U) {
    LARGE_INTEGER position{};
    position.QuadPart = dos.e_lfanew;
    if (SetFilePointerEx(file, position, nullptr, FILE_BEGIN) &&
        ReadFile(file, &nt, static_cast<DWORD>(sizeof(nt)), &bytes_read,
                 nullptr) &&
        bytes_read == sizeof(nt) && nt.Signature == IMAGE_NT_SIGNATURE &&
        nt.FileHeader.SizeOfOptionalHeader >= sizeof(IMAGE_OPTIONAL_HEADER64)) {
      output->machine = nt.FileHeader.Machine;
      output->optional_magic = nt.OptionalHeader.Magic;
      output->timestamp = nt.FileHeader.TimeDateStamp;
      output->image_base = nt.OptionalHeader.ImageBase;
      output->image_size = nt.OptionalHeader.SizeOfImage;
      result = true;
    }
  }
  CloseHandle(file);
  return result;
}

std::array<std::uint16_t, 4>
file_version(const std::filesystem::path &path) noexcept {
  std::array<std::uint16_t, 4> result{};
  try {
    DWORD ignored{};
    const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (size == 0U || size > (16U << 20U)) {
      return result;
    }
    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0U, size, data.data())) {
      return result;
    }
    void *raw{};
    UINT raw_size{};
    if (!VerQueryValueW(data.data(), L"\\", &raw, &raw_size) ||
        raw == nullptr || raw_size < sizeof(VS_FIXEDFILEINFO)) {
      return result;
    }
    const auto *info = static_cast<const VS_FIXEDFILEINFO *>(raw);
    if (info->dwSignature != VS_FFI_SIGNATURE) {
      return result;
    }
    result = {
        static_cast<std::uint16_t>(HIWORD(info->dwFileVersionMS)),
        static_cast<std::uint16_t>(LOWORD(info->dwFileVersionMS)),
        static_cast<std::uint16_t>(HIWORD(info->dwFileVersionLS)),
        static_cast<std::uint16_t>(LOWORD(info->dwFileVersionLS))};
  } catch (...) {
  }
  return result;
}

wchar_t ascii_lower(const wchar_t value) noexcept {
  if (value >= L'A' && value <= L'Z') {
    return static_cast<wchar_t>(value + (L'a' - L'A'));
  }
  return value;
}

bool equal_path_text(const std::wstring_view left,
                     const std::wstring_view right) noexcept {
  return left.size() == right.size() &&
         (left.empty() ||
          CompareStringOrdinal(left.data(), static_cast<int>(left.size()),
                               right.data(), static_cast<int>(right.size()),
                               TRUE) == CSTR_EQUAL);
}

bool contains_ascii_case_insensitive(const std::wstring_view text,
                                     const std::wstring_view needle) noexcept {
  if (needle.empty()) {
    return true;
  }
  if (needle.size() > text.size()) {
    return false;
  }
  for (std::size_t offset = 0U; offset <= text.size() - needle.size();
       ++offset) {
    bool matches = true;
    for (std::size_t index = 0U; index < needle.size(); ++index) {
      if (ascii_lower(text[offset + index]) != ascii_lower(needle[index])) {
        matches = false;
        break;
      }
    }
    if (matches) {
      return true;
    }
  }
  return false;
}

std::filesystem::path cache_feature_path() noexcept {
  PWSTR program_data{};
  if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT,
                                  nullptr, &program_data)) ||
      program_data == nullptr) {
    return {};
  }
  std::filesystem::path result;
  try {
    result = std::filesystem::path(program_data) / L"NVIDIA" / L"NGX" /
             L"models" / L"dlss" / L"versions" / L"66158" / L"files" /
             L"160_B9D2F5C.bin";
  } catch (...) {
    result.clear();
  }
  CoTaskMemFree(program_data);
  return result;
}

#if defined(MHW_DLSS_BRIDGE_PROBE)
bool executable_image_address(HMODULE module,
                              const void *address) noexcept;

const char *bridge_mode_text(const bridge::Mode mode) noexcept {
  switch (mode) {
  case bridge::Mode::disabled:
    return "disabled";
  case bridge::Mode::capture_create:
    return "capture-create";
  case bridge::Mode::capture_one_evaluate:
    return "capture-one-evaluate";
  }
  return "invalid";
}

bool configure_bridge_mode() noexcept {
  char value[64]{};
  SetLastError(ERROR_SUCCESS);
  const DWORD length = GetEnvironmentVariableA(
      "MHW_DLSS_BRIDGE_MODE", value, static_cast<DWORD>(_countof(value)));
  bridge::ModeParseResult parsed{};
  if (length == 0U && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
    parsed = bridge::parse_mode(bridge::kDisabledMode);
  } else if (length == 0U || length >= _countof(value)) {
    parsed = {bridge::Mode::disabled, false};
  } else {
    parsed = bridge::parse_mode(std::string_view(value, length));
  }

  AcquireSRWLockExclusive(&g_bridge_lifecycle_lock);
  g_bridge_lifecycle =
      bridge::OneShotLifecycle(parsed.mode, kOneEvaluateToken);
  ReleaseSRWLockExclusive(&g_bridge_lifecycle_lock);
  g_bridge_mode.store(parsed.mode, std::memory_order_release);
  return parsed.recognized;
}

bool modern_core_disk_identity_matches(
    const std::filesystem::path &path) noexcept {
  PeIdentity disk{};
  return !path.empty() && hash_matches(path, kModernCoreHash) &&
         read_disk_pe_identity(path, &disk) &&
         disk.machine == IMAGE_FILE_MACHINE_AMD64 &&
         disk.optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
         disk.timestamp == 0x6A0CB15EU &&
         disk.image_base == kExpectedNvidiaPreferredImageBase &&
         disk.image_size == 0x00173000U &&
         file_version(path) ==
             std::array<std::uint16_t, 4>{32U, 0U, 16U, 1047U};
}

bool locate_exact_modern_core(std::filesystem::path *const output) {
  if (output == nullptr) {
    return false;
  }
  output->clear();
  wchar_t windows_directory[32'768]{};
  const UINT length = GetWindowsDirectoryW(
      windows_directory, static_cast<UINT>(_countof(windows_directory)));
  if (length == 0U || length >= _countof(windows_directory)) {
    return false;
  }
  const std::filesystem::path repository =
      std::filesystem::path(windows_directory, windows_directory + length) /
      L"System32" / L"DriverStore" / L"FileRepository";
  std::error_code error;
  std::filesystem::directory_iterator iterator(
      repository, std::filesystem::directory_options::skip_permission_denied,
      error);
  const std::filesystem::directory_iterator end;
  if (error) {
    return false;
  }

  std::size_t visited{};
  std::filesystem::path match;
  while (iterator != end && visited < kMaximumDriverStoreDirectories) {
    ++visited;
    std::error_code type_error;
    if (iterator->is_directory(type_error) && !type_error) {
      const std::filesystem::path candidate = iterator->path() / L"_nvngx.dll";
      std::error_code exists_error;
      if (std::filesystem::is_regular_file(candidate, exists_error) &&
          !exists_error && modern_core_disk_identity_matches(candidate)) {
        if (!match.empty() &&
            !equal_path_text(match.wstring(), candidate.wstring())) {
          return false;
        }
        match = candidate;
      }
    }
    iterator.increment(error);
    if (error) {
      return false;
    }
  }
  if (iterator != end || match.empty()) {
    return false;
  }
  *output = match;
  return true;
}

bool copy_fixed_path(const std::filesystem::path &path, wchar_t *const output,
                     const std::size_t capacity,
                     std::atomic<std::uint32_t> &output_length) {
  if (output == nullptr || capacity == 0U) {
    return false;
  }
  const std::wstring text = path.wstring();
  if (text.empty() || text.size() >= capacity ||
      text.size() > (std::numeric_limits<std::uint32_t>::max)()) {
    return false;
  }
  std::memcpy(output, text.data(), text.size() * sizeof(wchar_t));
  output[text.size()] = L'\0';
  output_length.store(static_cast<std::uint32_t>(text.size()),
                      std::memory_order_release);
  return true;
}

bool prepare_bridge_preflight(const HMODULE host,
                              const std::filesystem::path &cache_path) {
  if (g_bridge_mode.load(std::memory_order_acquire) ==
      bridge::Mode::disabled) {
    return true;
  }
  if (GetModuleHandleW(L"mhw_dlss_unlock.dll") != nullptr) {
    write_log("bridge.preflight result=rejected reason=legacy-observer-loaded");
    return false;
  }
  if (host == nullptr || cache_path.empty()) {
    write_log("bridge.preflight result=rejected reason=required-path-missing");
    return false;
  }

  PeIdentity disk{};
  if (!hash_matches(cache_path, kCacheFeatureHash) ||
      !read_disk_pe_identity(cache_path, &disk) ||
      disk.machine != IMAGE_FILE_MACHINE_AMD64 ||
      disk.optional_magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      disk.timestamp != 0x64405395U ||
      disk.image_base != kExpectedNvidiaPreferredImageBase ||
      disk.image_size != 0x00B57000U ||
      file_version(cache_path) !=
          std::array<std::uint16_t, 4>{1U, 2U, 110U, 0U}) {
    write_log("bridge.preflight result=rejected reason=cache-identity-mismatch "
              "path=" +
              utf8(cache_path.wstring()));
    return false;
  }

  std::filesystem::path modern_core_path;
  if (!locate_exact_modern_core(&modern_core_path)) {
    write_log("bridge.preflight result=rejected "
              "reason=modern-core-identity-ambiguous-or-missing");
    return false;
  }

  const FARPROC get_int =
      GetProcAddress(host, "NVSDK_NGX_Parameter_GetI");
  const FARPROC get_uint =
      GetProcAddress(host, "NVSDK_NGX_Parameter_GetUI");
  const FARPROC get_float =
      GetProcAddress(host, "NVSDK_NGX_Parameter_GetF");
  const FARPROC get_resource =
      GetProcAddress(host, "NVSDK_NGX_Parameter_GetD3d12Resource");
  const auto host_base = reinterpret_cast<std::uintptr_t>(host);
  if (get_int == nullptr || get_uint == nullptr || get_float == nullptr ||
      get_resource == nullptr ||
      reinterpret_cast<std::uintptr_t>(get_resource) !=
          host_base + 0x273E430U ||
      reinterpret_cast<std::uintptr_t>(get_float) !=
          host_base + 0x273E490U ||
      reinterpret_cast<std::uintptr_t>(get_int) !=
          host_base + 0x273E4F0U ||
      reinterpret_cast<std::uintptr_t>(get_uint) !=
          host_base + 0x273E550U ||
      !executable_image_address(host,
                                reinterpret_cast<const void *>(get_int)) ||
      !executable_image_address(host,
                                reinterpret_cast<const void *>(get_uint)) ||
      !executable_image_address(host,
                                reinterpret_cast<const void *>(get_float)) ||
      !executable_image_address(
          host, reinterpret_cast<const void *>(get_resource))) {
    write_log("bridge.preflight result=rejected reason=host-getter-boundary");
    return false;
  }

  if (!copy_fixed_path(cache_path, g_expected_cache_path,
                       kResolverPathCapacity,
                       g_expected_cache_path_length) ||
      !copy_fixed_path(modern_core_path, g_expected_modern_core_path,
                       kResolverPathCapacity,
                       g_expected_modern_core_path_length)) {
    write_log("bridge.preflight result=rejected reason=cache-path-length");
    return false;
  }
  g_parameter_get_int.store(reinterpret_cast<std::uintptr_t>(get_int),
                            std::memory_order_release);
  g_parameter_get_uint.store(reinterpret_cast<std::uintptr_t>(get_uint),
                             std::memory_order_release);
  g_parameter_get_float.store(reinterpret_cast<std::uintptr_t>(get_float),
                              std::memory_order_release);
  g_parameter_get_resource.store(
      reinterpret_cast<std::uintptr_t>(get_resource),
      std::memory_order_release);
  g_bridge_cache_ready.store(true, std::memory_order_release);
  write_log("bridge.preflight result=verified cache-sha256=" +
            sha256_text(kCacheFeatureHash) + " modern-core-sha256=" +
            sha256_text(kModernCoreHash) + " parameter-getters=4");
  return true;
}
#endif

std::uint32_t expected_timestamp(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return 0x5DE9C0EBU;
  case Slot::cache_populate:
    return 0x64405395U;
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return 0x60E65AC5U;
  case Slot::modern_capability:
  case Slot::modern_parameters:
  case Slot::modern_d3d12_feature_status:
    return 0x6A0CB15EU;
  }
  return 0U;
}

std::uint32_t expected_image_size(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return 0x00583000U;
  case Slot::cache_populate:
    return 0x00B57000U;
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return 0x00078000U;
  case Slot::modern_capability:
  case Slot::modern_parameters:
  case Slot::modern_d3d12_feature_status:
    return 0x00173000U;
  }
  return 0U;
}

std::uintptr_t expected_rva(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return 0x180E0U;
  case Slot::cache_populate:
    return 0x1DFA0U;
  case Slot::legacy_capability:
    return 0x0A53BU;
  case Slot::legacy_parameters:
    return 0x0A582U;
  case Slot::modern_capability:
    return 0x669F0U;
  case Slot::modern_parameters:
    return 0x66C50U;
  case Slot::modern_d3d12_feature_status:
    return 0x264E0U;
  }
  return 0U;
}

const std::array<unsigned char, 32> &expected_hash(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return kLocalFeatureHash;
  case Slot::cache_populate:
    return kCacheFeatureHash;
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return kLegacyCoreHash;
  case Slot::modern_capability:
  case Slot::modern_parameters:
  case Slot::modern_d3d12_feature_status:
    return kModernCoreHash;
  }
  return kLocalFeatureHash;
}

std::array<std::uint16_t, 4> expected_version(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return {1U, 1U, 13U, 0U};
  case Slot::cache_populate:
    return {1U, 2U, 110U, 0U};
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return {30U, 0U, 14U, 9516U};
  case Slot::modern_capability:
  case Slot::modern_parameters:
  case Slot::modern_d3d12_feature_status:
    return {32U, 0U, 16U, 1047U};
  }
  return {};
}

bool executable_image_address(const HMODULE module,
                              const void *const address) noexcept {
  if (module == nullptr || address == nullptr) {
    return false;
  }
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(address, &information, sizeof(information)) !=
          sizeof(information) ||
      information.State != MEM_COMMIT || information.Type != MEM_IMAGE ||
      information.AllocationBase != module ||
      (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0U) {
    return false;
  }
  const DWORD protection = information.Protect & 0xFFU;
  return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
         protection == PAGE_EXECUTE_READWRITE ||
         protection == PAGE_EXECUTE_WRITECOPY;
}

template <std::size_t Size>
bool exact_bytes(const void *const address,
                 const std::array<unsigned char, Size> &expected) noexcept {
  if (address == nullptr) {
    return false;
  }
  __try {
    return std::memcmp(address, expected.data(), expected.size()) == 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool legacy_stub_matches(const HMODULE module, const void *const address,
                         const Slot slot) noexcept {
  if (slot != Slot::legacy_capability && slot != Slot::legacy_parameters) {
    return false;
  }
  const auto *bytes = static_cast<const unsigned char *>(address);
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  __try {
    for (std::size_t index = 0U; index < 20U; ++index) {
      if (bytes[index] != 0x90U) {
        return false;
      }
    }
    constexpr std::array<unsigned char, 9> first_tail{
        0x48, 0x8B, 0x00, 0x48, 0x83, 0xF8, 0x00, 0x74, 0xED};
    constexpr std::array<unsigned char, 9> second_tail{
        0x48, 0x8B, 0x00, 0x48, 0x83, 0xF8, 0x00, 0x75, 0x0B};
    constexpr std::array<unsigned char, 13> ending{
        0x48, 0xB8, 0x02, 0x00, 0xD0, 0xBA, 0x00,
        0x00, 0x00, 0x00, 0xC3, 0x50, 0xC3};
    if (bytes[20] != 0x48U || bytes[21] != 0xB8U ||
        std::memcmp(bytes + 30U, first_tail.data(), first_tail.size()) != 0 ||
        bytes[39] != 0x48U || bytes[40] != 0xB8U ||
        std::memcmp(bytes + 49U, second_tail.data(), second_tail.size()) != 0 ||
        std::memcmp(bytes + 58U, ending.data(), ending.size()) != 0) {
      return false;
    }
    std::uint64_t ready_slot{};
    std::uint64_t target_slot{};
    std::memcpy(&ready_slot, bytes + 22U, sizeof(ready_slot));
    std::memcpy(&target_slot, bytes + 41U, sizeof(target_slot));
    const std::uintptr_t expected_target_slot =
        base + (slot == Slot::legacy_capability ? 0x6E3C8U : 0x6E3D0U);
    return ready_slot == base + 0x6E4B8U &&
           target_slot == expected_target_slot;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool slot_prologue_matches(const Slot slot, const HMODULE module,
                           const void *const address) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return exact_bytes(address, kLocalPopulatePrologue);
  case Slot::cache_populate:
    return exact_bytes(address, kCachePopulatePrologue);
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return legacy_stub_matches(module, address, slot);
  case Slot::modern_capability:
    return exact_bytes(address, kModernCapabilityPrologue);
  case Slot::modern_parameters:
    return exact_bytes(address, kModernParametersPrologue);
  case Slot::modern_d3d12_feature_status:
    return exact_bytes(address, kModernFeatureStatusPrologue);
  }
  return false;
}

bool mapped_slot_matches(const Slot slot, const HMODULE module,
                         const FARPROC target) noexcept {
  if (module == nullptr || target == nullptr ||
      !executable_image_address(module, reinterpret_cast<const void *>(target))) {
    return false;
  }
  PeIdentity identity{};
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  return read_mapped_pe_identity(module, &identity) &&
         identity.machine == IMAGE_FILE_MACHINE_AMD64 &&
         identity.optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
         identity.timestamp == expected_timestamp(slot) &&
         identity.image_size == expected_image_size(slot) &&
         identity.image_base == base &&
         reinterpret_cast<std::uintptr_t>(target) == base + expected_rva(slot) &&
         slot_prologue_matches(slot, module,
                               reinterpret_cast<const void *>(target));
}

bool caller_is_host_executable(const void *const caller) noexcept {
  const auto host = g_host_base.load(std::memory_order_acquire);
  const auto image_size = g_host_image_size.load(std::memory_order_acquire);
  const auto address = reinterpret_cast<std::uintptr_t>(caller);
  if (host == 0U || image_size == 0U || address < host ||
      address >= host + image_size ||
      !executable_image_address(reinterpret_cast<HMODULE>(host), caller)) {
    return false;
  }

  const auto *base = reinterpret_cast<const unsigned char *>(host);
  __try {
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::uint32_t>(dos->e_lfanew) > 0x10'0000U) {
      return false;
    }
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(
        base + static_cast<std::size_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.NumberOfSections == 0U ||
        nt->FileHeader.NumberOfSections > 96U) {
      return false;
    }
    const std::uintptr_t rva = address - host;
    const IMAGE_SECTION_HEADER *section = IMAGE_FIRST_SECTION(nt);
    for (std::uint16_t index = 0U; index < nt->FileHeader.NumberOfSections;
         ++index) {
      const std::uint32_t section_size = section[index].Misc.VirtualSize;
      const std::uintptr_t begin = section[index].VirtualAddress;
      const std::uintptr_t end = begin + section_size;
      if (section_size != 0U && rva >= begin && rva < end) {
        return (section[index].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0U;
      }
    }
    return false;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool caller_is_module_rva(const void *const caller,
                          const std::uint32_t timestamp,
                          const std::uint32_t image_size,
                          const std::uintptr_t expected_caller_rva) noexcept {
  if (caller == nullptr) {
    return false;
  }
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(caller, &information, sizeof(information)) !=
          sizeof(information) ||
      information.Type != MEM_IMAGE || information.AllocationBase == nullptr) {
    return false;
  }
  const auto module = static_cast<HMODULE>(information.AllocationBase);
  PeIdentity identity{};
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  return read_mapped_pe_identity(module, &identity) &&
         identity.machine == IMAGE_FILE_MACHINE_AMD64 &&
         identity.optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
         identity.timestamp == timestamp && identity.image_size == image_size &&
         identity.image_base == base &&
         reinterpret_cast<std::uintptr_t>(caller) ==
              base + expected_caller_rva &&
         executable_image_address(module, caller);
}

bool expected_slot_caller(const Slot slot, const void *const caller) noexcept {
  switch (slot) {
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return caller_is_host_executable(caller);
  case Slot::modern_capability:
    return caller_is_module_rva(caller, 0x60E65AC5U, 0x00078000U,
                                0x09A12U);
  case Slot::modern_parameters:
    return caller_is_module_rva(caller, 0x60E65AC5U, 0x00078000U,
                                0x09A2AU);
  case Slot::local_populate:
  case Slot::cache_populate:
    return caller_is_module_rva(caller, 0x6A0CB15EU, 0x00173000U,
                                 0x43351U);
  case Slot::modern_d3d12_feature_status:
    return caller_is_module_rva(caller, 0x6A0CB15EU, 0x00173000U,
                                0x67704U);
  }
  return false;
}

bool procedure_name_equals(const LPCSTR name,
                           const char *const expected) noexcept {
  if (name == nullptr || expected == nullptr ||
      reinterpret_cast<std::uintptr_t>(name) <= 0xFFFFU) {
    return false;
  }
  __try {
    for (std::size_t index = 0U;; ++index) {
      if (name[index] != expected[index]) {
        return false;
      }
      if (expected[index] == '\0') {
        return true;
      }
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

TargetName classify_target_name(const LPCSTR name) noexcept {
  if (procedure_name_equals(name, "NVSDK_NGX_PopulateParameters")) {
    return TargetName::populate;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_Init")) {
    return TargetName::d3d12_init;
  }
  if (procedure_name_equals(name,
                            "NVSDK_NGX_D3D12_GetCapabilityParameters")) {
    return TargetName::capability_parameters;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_GetParameters")) {
    return TargetName::parameters;
  }
#if defined(MHW_DLSS_BRIDGE_PROBE)
  if (procedure_name_equals(name,
                            "NVSDK_NGX_D3D12_GetScratchBufferSize")) {
    return TargetName::d3d12_scratch;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_CreateFeature")) {
    return TargetName::d3d12_create;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_EvaluateFeature")) {
    return TargetName::d3d12_evaluate;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_EvaluateFeature_C")) {
    return TargetName::d3d12_evaluate_c;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_ReleaseFeature")) {
    return TargetName::d3d12_release;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_Shutdown")) {
    return TargetName::d3d12_shutdown;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_Shutdown1")) {
    return TargetName::d3d12_shutdown1;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_CreateFeature1")) {
    return TargetName::d3d12_create1;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_Init_Ext")) {
    return TargetName::d3d12_init_ext;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_Init_Ext1")) {
    return TargetName::d3d12_init_ext1;
  }
  if (procedure_name_equals(name, "NVSDK_NGX_D3D12_Init_Ext2")) {
    return TargetName::d3d12_init_ext2;
  }
  if (procedure_name_equals(name,
                            "NVSDK_NGX_D3D12_PopulateParameters_Impl")) {
    return TargetName::d3d12_populate_parameters_impl;
  }
  if (procedure_name_equals(
          name, "NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl")) {
    return TargetName::d3d12_populate_device_parameters_impl;
  }
#endif
  return TargetName::unknown;
}

void publish_event(const Slot slot, const void *const caller,
                   const void *const argument, const NgxResult result,
                   const ULONGLONG duration_ticks,
                   const std::int32_t feature_index = -1,
                   const std::uint32_t api = 0U) noexcept {
  SlotRuntime &runtime = slot_runtime(slot);
  runtime.calls.fetch_add(1U, std::memory_order_relaxed);
  if (result == kResultSuccess) {
    runtime.success.fetch_add(1U, std::memory_order_relaxed);
  } else if (result == kResultFail) {
    runtime.bad00000.fetch_add(1U, std::memory_order_relaxed);
  } else if (result == kResultFeatureNotSupported) {
    runtime.bad00001.fetch_add(1U, std::memory_order_relaxed);
  } else if (result == kResultPlatformError) {
    runtime.bad00002.fetch_add(1U, std::memory_order_relaxed);
  } else {
    runtime.other.fetch_add(1U, std::memory_order_relaxed);
  }

  const std::uint32_t reserved =
      runtime.reserved_events.fetch_add(1U, std::memory_order_relaxed);
  if (reserved >= runtime.events.size()) {
    runtime.dropped.fetch_add(1U, std::memory_order_relaxed);
    return;
  }
  TraceEvent &event = runtime.events[reserved];
  EventState expected = EventState::empty;
  if (!event.state.compare_exchange_strong(
          expected, EventState::writing, std::memory_order_acq_rel,
          std::memory_order_relaxed)) {
    runtime.dropped.fetch_add(1U, std::memory_order_relaxed);
    return;
  }
  event.payload.sequence =
      g_event_sequence.fetch_add(1U, std::memory_order_relaxed) + 1U;
  event.payload.tick = GetTickCount64();
  event.payload.duration_ticks = duration_ticks;
  event.payload.thread_id = GetCurrentThreadId();
  event.payload.caller = reinterpret_cast<std::uintptr_t>(caller);
  event.payload.argument = reinterpret_cast<std::uintptr_t>(argument);
  event.payload.result = result;
  event.payload.feature_index = feature_index;
  event.payload.api = api;
  event.payload.argument_nonnull = argument != nullptr;
  event.state.store(EventState::ready, std::memory_order_release);
}

void publish_resolver_event(const ResolverDisposition disposition,
                            const TargetName name, const HMODULE module,
                            const FARPROC target,
                            const void *const caller) noexcept {
  const std::uint64_t reserved =
      g_resolver_events_reserved.fetch_add(1U, std::memory_order_relaxed);
  if (reserved >= g_resolver_events.size()) {
    g_resolver_events_dropped.fetch_add(1U, std::memory_order_relaxed);
    return;
  }

  ResolverEvent &event =
      g_resolver_events[static_cast<std::size_t>(reserved)];
  EventState expected = EventState::empty;
  if (!event.state.compare_exchange_strong(
          expected, EventState::writing, std::memory_order_acq_rel,
          std::memory_order_relaxed)) {
    g_resolver_events_dropped.fetch_add(1U, std::memory_order_relaxed);
    return;
  }

  const auto module_value = reinterpret_cast<std::uintptr_t>(module);
  const auto target_value = reinterpret_cast<std::uintptr_t>(target);
  PeIdentity identity{};
  event.payload.sequence =
      g_event_sequence.fetch_add(1U, std::memory_order_relaxed) + 1U;
  event.payload.tick = GetTickCount64();
  event.payload.thread_id = GetCurrentThreadId();
  event.payload.name = name;
  event.payload.disposition = disposition;
  event.payload.module = module_value;
  event.payload.target = target_value;
  event.payload.caller = reinterpret_cast<std::uintptr_t>(caller);
  event.payload.mapped_identity_available =
      read_mapped_pe_identity(module, &identity);
  if (event.payload.mapped_identity_available) {
    event.payload.mapped_timestamp = identity.timestamp;
    event.payload.mapped_image_size = identity.image_size;
    event.payload.target_rva_valid = target_value >= module_value &&
                                     target_value - module_value <
                                         identity.image_size;
    if (event.payload.target_rva_valid) {
      event.payload.target_rva = target_value - module_value;
    }
  }
  event.state.store(EventState::ready, std::memory_order_release);
}

template <Slot TargetSlot>
NgxResult invoke_populate(NgxParameter *const parameters, const void *const caller,
                          const DWORD incoming_last_error) noexcept {
  SlotRuntime &runtime = slot_runtime(TargetSlot);
  const auto original = reinterpret_cast<PopulateParametersFn>(
      runtime.target.load(std::memory_order_acquire));
  if (original == nullptr) {
    SetLastError(incoming_last_error);
    return kResultFail;
  }

  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  const bool observe =
      g_trace_enabled.load(std::memory_order_seq_cst) &&
      runtime.identity.load(std::memory_order_acquire) !=
          IdentityState::rejected;
  if (!observe) {
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return original(parameters);
  }
  constexpr std::uint32_t mask = 1U << slot_index(TargetSlot);
  const bool recursive = (g_active_slots & mask) != 0U;
  if (recursive) {
    runtime.recursion.fetch_add(1U, std::memory_order_relaxed);
  }
  const std::uint32_t previous_mask = g_active_slots;
  g_active_slots |= mask;
  const ULONGLONG start = GetTickCount64();
  SetLastError(incoming_last_error);
  const NgxResult result = original(parameters);
  const DWORD outgoing_last_error = GetLastError();
  const ULONGLONG duration = GetTickCount64() - start;
  g_active_slots = previous_mask;
  if (!recursive) {
    publish_event(TargetSlot, caller, parameters, result, duration);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(outgoing_last_error);
  return result;
}

template <Slot TargetSlot>
NgxResult invoke_get_parameters(NgxParameter **const parameters,
                                const void *const caller,
                                const DWORD incoming_last_error) noexcept {
  SlotRuntime &runtime = slot_runtime(TargetSlot);
  const auto original = reinterpret_cast<GetParametersFn>(
      runtime.target.load(std::memory_order_acquire));
  if (original == nullptr) {
    SetLastError(incoming_last_error);
    return kResultFail;
  }

  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  const bool observe =
      g_trace_enabled.load(std::memory_order_seq_cst) &&
      runtime.identity.load(std::memory_order_acquire) !=
          IdentityState::rejected;
  if (!observe) {
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return original(parameters);
  }
  constexpr std::uint32_t mask = 1U << slot_index(TargetSlot);
  const bool recursive = (g_active_slots & mask) != 0U;
  if (recursive) {
    runtime.recursion.fetch_add(1U, std::memory_order_relaxed);
  }
  const std::uint32_t previous_mask = g_active_slots;
  g_active_slots |= mask;
  const ULONGLONG start = GetTickCount64();
  SetLastError(incoming_last_error);
  const NgxResult result = original(parameters);
  const DWORD outgoing_last_error = GetLastError();
  const ULONGLONG duration = GetTickCount64() - start;
  g_active_slots = previous_mask;
  if (!recursive) {
    publish_event(TargetSlot, caller, parameters, result, duration);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(outgoing_last_error);
  return result;
}

#if defined(MHW_DLSS_BRIDGE_PROBE)
bridge::IdentityEvidence bridge_identity_evidence() noexcept {
  const bool environment_verified =
      g_bridge_environment_ready.load(std::memory_order_acquire);
  const bool route_armed =
      g_bridge_route_state.load(std::memory_order_acquire) == 1;
  const bool cache_verified =
      g_bridge_cache_ready.load(std::memory_order_acquire);
  return {environment_verified,
          g_original_resolver.load(std::memory_order_acquire) != 0U,
          route_armed,
          cache_verified && route_armed,
          route_armed};
}

std::uintptr_t expected_bridge_invocation_rva(
    const bridge::ExportId export_id, const void *const caller) noexcept {
  switch (export_id) {
  case bridge::ExportId::d3d12_init:
    return 0x6769AU;
  case bridge::ExportId::populate_parameters:
    if (caller_is_module_rva(caller, 0x6A0CB15EU, 0x00173000U,
                             0x67ACEU)) {
      return 0x67ACEU;
    }
    return 0x67BC2U;
  case bridge::ExportId::d3d12_create_feature:
    return 0x67DE6U;
  case bridge::ExportId::d3d12_evaluate_feature:
    return 0x6704AU;
  case bridge::ExportId::d3d12_get_scratch_buffer_size:
    return 0U;
  case bridge::ExportId::d3d12_release_feature:
    return 0x08AB7U;
  case bridge::ExportId::d3d12_shutdown:
    return 0x43815U;
  case bridge::ExportId::d3d12_shutdown1:
  case bridge::ExportId::d3d12_create_feature1:
  case bridge::ExportId::d3d12_init_ext:
  case bridge::ExportId::d3d12_init_ext1:
  case bridge::ExportId::d3d12_init_ext2:
  case bridge::ExportId::d3d12_populate_parameters_impl:
  case bridge::ExportId::d3d12_populate_device_parameters_impl:
  case bridge::ExportId::unknown:
    break;
  }
  return 0U;
}

bridge::CallerEvidence bridge_caller_evidence(
    const bridge::ExportId export_id, const void *const caller) noexcept {
  bridge::CallerEvidence evidence{};
  evidence.requested_export = export_id;
  evidence.return_address = reinterpret_cast<std::uintptr_t>(caller);
  evidence.expected_return_rva =
      expected_bridge_invocation_rva(export_id, caller);
  evidence.image_size = 0x00173000U;

  MEMORY_BASIC_INFORMATION information{};
  if (caller == nullptr ||
      VirtualQuery(caller, &information, sizeof(information)) !=
          sizeof(information) ||
      information.AllocationBase == nullptr) {
    return evidence;
  }
  const auto module = static_cast<HMODULE>(information.AllocationBase);
  evidence.module_base = reinterpret_cast<std::uintptr_t>(module);
  evidence.caller_binary_verified =
      g_bridge_ever_armed.load(std::memory_order_acquire) &&
      g_bridge_modern_core_base.load(std::memory_order_acquire) ==
          evidence.module_base;
  evidence.memory_image = information.Type == MEM_IMAGE;
  evidence.allocation_base_matches = evidence.caller_binary_verified;
  evidence.executable_section = executable_image_address(module, caller);
  evidence.executable_page = evidence.executable_section;
  return evidence;
}

bool bridge_caller_is_pinned_modern_core(
    const void *const caller) noexcept {
  const std::uintptr_t modern_base =
      g_bridge_modern_core_base.load(std::memory_order_acquire);
  if (caller == nullptr || modern_base == 0U) {
    return false;
  }
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(caller, &information, sizeof(information)) !=
          sizeof(information) ||
      information.Type != MEM_IMAGE || information.AllocationBase == nullptr ||
      reinterpret_cast<std::uintptr_t>(information.AllocationBase) !=
          modern_base) {
    return false;
  }
  return executable_image_address(
      static_cast<HMODULE>(information.AllocationBase), caller);
}

bridge::TransitionResult bridge_apply(
    const bridge::LifecycleOperation operation,
    const bridge::ExportId export_id, const void *const caller,
    const std::uint64_t token = 0U) noexcept {
  const bridge::TransitionInput input{operation, bridge_identity_evidence(),
                                      bridge_caller_evidence(export_id, caller),
                                      token};
  AcquireSRWLockExclusive(&g_bridge_lifecycle_lock);
  const bridge::TransitionResult result = g_bridge_lifecycle.apply(input);
  ReleaseSRWLockExclusive(&g_bridge_lifecycle_lock);
  return result;
}

void poison_bridge_boundary() noexcept {
  std::uint32_t state =
      g_bridge_boundary_state.load(std::memory_order_acquire);
  for (;;) {
    if (state >= 2U) {
      return;
    }
    const std::uint32_t poisoned = state == 0U ? 3U : 2U;
    if (g_bridge_boundary_state.compare_exchange_weak(
            state, poisoned, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      return;
    }
  }
}

class BridgeBoundaryLease final {
public:
  BridgeBoundaryLease() noexcept {
    std::uint32_t idle{};
    acquired_ = g_bridge_boundary_state.compare_exchange_strong(
        idle, 1U, std::memory_order_acq_rel, std::memory_order_acquire);
    if (!acquired_) {
      g_bridge_boundary_contentions.fetch_add(1U,
                                              std::memory_order_relaxed);
      poison_bridge_boundary();
    }
  }

  ~BridgeBoundaryLease() noexcept {
    if (acquired_ && !finished_) {
      std::uint32_t owned_clean = 1U;
      if (!g_bridge_boundary_state.compare_exchange_strong(
              owned_clean, 0U, std::memory_order_acq_rel,
              std::memory_order_acquire)) {
        g_bridge_boundary_state.store(3U, std::memory_order_release);
      }
    }
  }

  BridgeBoundaryLease(const BridgeBoundaryLease &) = delete;
  BridgeBoundaryLease &operator=(const BridgeBoundaryLease &) = delete;

  explicit operator bool() const noexcept { return acquired_; }

  bool finish() noexcept {
    if (!acquired_ || finished_) {
      return false;
    }
    finished_ = true;
    std::uint32_t owned_clean = 1U;
    if (g_bridge_boundary_state.compare_exchange_strong(
            owned_clean, 0U, std::memory_order_acq_rel,
            std::memory_order_acquire)) {
      return true;
    }
    g_bridge_boundary_state.store(3U, std::memory_order_release);
    return false;
  }

private:
  bool acquired_{};
  bool finished_{};
};

void publish_bridge_event(const bridge::ExportId export_id,
                          const void *const caller,
                          const bridge::TransitionResult &transition,
                          const NgxResult result,
                          const std::uint64_t argument0 = 0U,
                          const std::uint64_t argument1 = 0U,
                          const bool d3d12_verified = false,
                          const bridge::CreateContractSnapshot *const
                              create_snapshot = nullptr,
                          const bridge::EvaluateContractSnapshot *const
                              evaluate_snapshot = nullptr,
                          const bool observation_only = false,
                          const std::uint32_t init_validation_mask = 0U) noexcept {
  const std::uint32_t reserved =
      g_bridge_events_reserved.fetch_add(1U, std::memory_order_relaxed);
  if (reserved >= g_bridge_events.size()) {
    g_bridge_events_dropped.fetch_add(1U, std::memory_order_relaxed);
    return;
  }
  BridgeEvent &event = g_bridge_events[reserved];
  EventState expected = EventState::empty;
  if (!event.state.compare_exchange_strong(
          expected, EventState::writing, std::memory_order_acq_rel,
          std::memory_order_relaxed)) {
    g_bridge_events_dropped.fetch_add(1U, std::memory_order_relaxed);
    return;
  }
  event.payload.sequence =
      g_event_sequence.fetch_add(1U, std::memory_order_relaxed) + 1U;
  event.payload.tick = GetTickCount64();
  event.payload.thread_id = GetCurrentThreadId();
  event.payload.export_id = export_id;
  event.payload.caller = reinterpret_cast<std::uintptr_t>(caller);
  event.payload.result = result;
  event.payload.before = transition.before;
  event.payload.after = transition.after;
  event.payload.decision = transition.decision;
  event.payload.response = transition.response;
  event.payload.token = transition.issued_token;
  event.payload.argument0 = argument0;
  event.payload.argument1 = argument1;
  event.payload.init_validation_mask = init_validation_mask;
  event.payload.capture_committed = transition.capture_committed;
  event.payload.d3d12_object_verified = d3d12_verified;
  event.payload.observation_only = observation_only;
  if (create_snapshot != nullptr) {
    event.payload.has_create_snapshot = true;
    event.payload.create_snapshot = *create_snapshot;
  }
  if (evaluate_snapshot != nullptr) {
    event.payload.has_evaluate_snapshot = true;
    event.payload.evaluate_snapshot = *evaluate_snapshot;
  }
  event.state.store(EventState::ready, std::memory_order_release);
}

bridge::LifecycleState bridge_lifecycle_state() noexcept {
  AcquireSRWLockShared(&g_bridge_lifecycle_lock);
  const bridge::LifecycleState state = g_bridge_lifecycle.state();
  ReleaseSRWLockShared(&g_bridge_lifecycle_lock);
  return state;
}

bool bridge_populate_budget_available() noexcept {
  AcquireSRWLockShared(&g_bridge_lifecycle_lock);
  const bool available = g_bridge_lifecycle.populate_budget_available();
  ReleaseSRWLockShared(&g_bridge_lifecycle_lock);
  return available;
}

bridge::TransitionResult bridge_observation_transition() noexcept {
  bridge::TransitionResult result{};
  result.before = bridge_lifecycle_state();
  result.after = result.before;
  result.response = bridge::BoundaryResponse::none;
  return result;
}

bool bridge_route_table_complete() noexcept {
  if (!g_bridge_lookup_transaction_complete.load(
          std::memory_order_acquire)) {
    return false;
  }
  return g_bridge_route_state.load(std::memory_order_acquire) == 1 &&
         g_bridge_route_mask.load(std::memory_order_acquire) ==
             bridge::kProviderRouteMaskComplete &&
         g_bridge_lookup_seen_mask.load(std::memory_order_acquire) ==
             bridge::kProviderLookupMaskComplete;
}

struct BridgeLifecycleDiagnostics {
  bool overflowed{};
  std::uint32_t dropped_events{};
};

BridgeLifecycleDiagnostics bridge_lifecycle_diagnostics() noexcept {
  AcquireSRWLockShared(&g_bridge_lifecycle_lock);
  const BridgeLifecycleDiagnostics diagnostics{
      g_bridge_lifecycle.overflowed(),
      g_bridge_lifecycle.dropped_event_count()};
  ReleaseSRWLockShared(&g_bridge_lifecycle_lock);
  return diagnostics;
}

bool verify_d3d12_device(ID3D12Device *const device) noexcept {
  if (device == nullptr) {
    return false;
  }
  ID3D12Device *verified{};
  HRESULT result = E_FAIL;
  bool release_completed = false;
  __try {
    result = device->QueryInterface(__uuidof(ID3D12Device),
                                    reinterpret_cast<void **>(&verified));
    if (SUCCEEDED(result) && verified != nullptr) {
      verified->Release();
      release_completed = true;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    result = E_FAIL;
    verified = nullptr;
  }
  return SUCCEEDED(result) && verified != nullptr && release_completed;
}

bool verify_d3d12_command_list(
    ID3D12GraphicsCommandList *const command_list) noexcept {
  if (command_list == nullptr) {
    return false;
  }
  ID3D12GraphicsCommandList *verified{};
  HRESULT result = E_FAIL;
  bool release_completed = false;
  __try {
    result = command_list->QueryInterface(
        __uuidof(ID3D12GraphicsCommandList),
        reinterpret_cast<void **>(&verified));
    if (SUCCEEDED(result) && verified != nullptr) {
      verified->Release();
      release_completed = true;
    }
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    result = E_FAIL;
    verified = nullptr;
  }
  return SUCCEEDED(result) && verified != nullptr && release_completed;
}

bool store_bridge_output(void **const output, void *const value) noexcept {
  if (output == nullptr) {
    return false;
  }
  __try {
    *output = value;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool store_bridge_size(std::size_t *const output,
                       const std::size_t value) noexcept {
  if (output == nullptr) {
    return false;
  }
  __try {
    *output = value;
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool readable_nonempty_path(const wchar_t *const path) noexcept {
  if (path == nullptr) {
    return false;
  }
  __try {
    return path[0] != L'\0';
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

std::uint64_t read_bridge_token(const void *const handle) noexcept {
  if (handle == nullptr) {
    return 0U;
  }
  __try {
    return *static_cast<const std::uint64_t *>(handle);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return 0U;
  }
}

struct BridgeParameterContext {
  NgxParameter *parameters{};
};

bridge::UnsignedGetterReply bridge_get_unsigned(
    void *const opaque_context, const std::string_view key) noexcept {
  bridge::UnsignedGetterReply reply{};
  const auto function = reinterpret_cast<ParameterGetUIntFn>(
      g_parameter_get_uint.load(std::memory_order_acquire));
  if (opaque_context == nullptr || function == nullptr || key.empty()) {
    return reply;
  }
  auto &context = *static_cast<BridgeParameterContext *>(opaque_context);
  if (context.parameters == nullptr) {
    return reply;
  }
  __try {
    reply.getter_result =
        function(context.parameters, key.data(), &reply.value);
    reply.callback_completed = true;
    reply.present = reply.getter_result == kResultSuccess;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    reply.callback_completed = false;
  }
  return reply;
}

bridge::SignedGetterReply bridge_get_signed(
    void *const opaque_context, const std::string_view key) noexcept {
  bridge::SignedGetterReply reply{};
  const auto function = reinterpret_cast<ParameterGetIntFn>(
      g_parameter_get_int.load(std::memory_order_acquire));
  if (opaque_context == nullptr || function == nullptr || key.empty()) {
    return reply;
  }
  auto &context = *static_cast<BridgeParameterContext *>(opaque_context);
  if (context.parameters == nullptr) {
    return reply;
  }
  __try {
    reply.getter_result =
        function(context.parameters, key.data(), &reply.value);
    reply.callback_completed = true;
    reply.present = reply.getter_result == kResultSuccess;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    reply.callback_completed = false;
  }
  return reply;
}

bridge::FloatGetterReply bridge_get_float(
    void *const opaque_context, const std::string_view key) noexcept {
  bridge::FloatGetterReply reply{};
  const auto function = reinterpret_cast<ParameterGetFloatFn>(
      g_parameter_get_float.load(std::memory_order_acquire));
  if (opaque_context == nullptr || function == nullptr || key.empty()) {
    return reply;
  }
  auto &context = *static_cast<BridgeParameterContext *>(opaque_context);
  if (context.parameters == nullptr) {
    return reply;
  }
  __try {
    reply.getter_result =
        function(context.parameters, key.data(), &reply.value);
    reply.callback_completed = true;
    reply.present = reply.getter_result == kResultSuccess;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    reply.callback_completed = false;
  }
  return reply;
}

bridge::ResourceGetterReply bridge_get_resource(
    void *const opaque_context, const std::string_view key) noexcept {
  bridge::ResourceGetterReply reply{};
  const auto function = reinterpret_cast<ParameterGetResourceFn>(
      g_parameter_get_resource.load(std::memory_order_acquire));
  if (opaque_context == nullptr || function == nullptr || key.empty()) {
    return reply;
  }
  auto &context = *static_cast<BridgeParameterContext *>(opaque_context);
  if (context.parameters == nullptr) {
    return reply;
  }
  ID3D12Resource *resource{};
  __try {
    reply.getter_result =
        function(context.parameters, key.data(), &resource);
    reply.value = reinterpret_cast<std::uintptr_t>(resource);
    reply.callback_completed = true;
    reply.present = reply.getter_result == kResultSuccess;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    reply.callback_completed = false;
  }
  return reply;
}

bridge::GetterCallbacks bridge_getter_callbacks(
    BridgeParameterContext &context) noexcept {
  return {&context, &bridge_get_unsigned, &bridge_get_signed,
          &bridge_get_float, &bridge_get_resource};
}

bridge::CreateContractSnapshot capture_bridge_create_contract(
    NgxParameter *const parameters) noexcept {
  BridgeParameterContext context{parameters};
  return bridge::capture_create_contract(g_bridge_capture_state,
                                         bridge_getter_callbacks(context));
}

bridge::EvaluateContractSnapshot capture_bridge_evaluate_contract(
    NgxParameter *const parameters) noexcept {
  BridgeParameterContext context{parameters};
  return bridge::capture_evaluate_contract(g_bridge_capture_state,
                                           bridge_getter_callbacks(context));
}
#endif

} // namespace

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
local_populate_wrapper(NgxParameter *parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_populate<Slot::local_populate>(parameters, caller,
                                               incoming_last_error);
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
cache_populate_wrapper(NgxParameter *parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
#if defined(MHW_DLSS_BRIDGE_PROBE)
  const bool bridge_mode_active =
      g_bridge_mode.load(std::memory_order_acquire) !=
          bridge::Mode::disabled;
  const int route_state =
      g_bridge_route_state.load(std::memory_order_acquire);
  const bool transaction_claimed =
      g_bridge_interception_started.load(std::memory_order_acquire);
  if (!bridge_mode_active || !transaction_claimed || route_state == -2) {
    return invoke_populate<Slot::cache_populate>(parameters, caller,
                                                 incoming_last_error);
  }

  // Keep the bridge phase visible to close_trace_phase and serialize the
  // cache's parameter mutation with Create/Evaluate contract capture.
  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  BridgeBoundaryLease boundary;
  if (!boundary) {
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return kResultNotInitialized;
  }
  const bridge::CallerEvidence caller_evidence = bridge_caller_evidence(
      bridge::ExportId::populate_parameters, caller);
  const bridge::LifecycleState lifecycle_state = bridge_lifecycle_state();
  const bool exact_invocation =
      parameters != nullptr && bridge_route_table_complete() &&
      slot_runtime(Slot::cache_populate)
              .identity.load(std::memory_order_acquire) !=
          IdentityState::rejected &&
      bridge::caller_is_verified_for(
          caller_evidence, bridge::ExportId::populate_parameters) &&
      bridge_populate_budget_available() &&
      (lifecycle_state == bridge::LifecycleState::initialized ||
       lifecycle_state == bridge::LifecycleState::populated);
  if (!exact_invocation) {
    g_bridge_lookup_transaction_complete.store(false,
                                               std::memory_order_release);
    g_bridge_route_state.store(-1, std::memory_order_release);
    const bridge::TransitionResult transition = bridge_apply(
        bridge::LifecycleOperation::abort,
        bridge::ExportId::populate_parameters, caller);
    poison_bridge_boundary();
    static_cast<void>(boundary.finish());
    publish_bridge_event(
        bridge::ExportId::populate_parameters, caller, transition,
        kResultNotInitialized, reinterpret_cast<std::uintptr_t>(parameters));
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return kResultNotInitialized;
  }
#endif
  const NgxResult result = invoke_populate<Slot::cache_populate>(
      parameters, caller, incoming_last_error);
#if defined(MHW_DLSS_BRIDGE_PROBE)
  const DWORD outgoing_last_error = GetLastError();
  const bridge::TransitionResult transition = bridge_apply(
      result == kResultSuccess ? bridge::LifecycleOperation::populate
                               : bridge::LifecycleOperation::abort,
      bridge::ExportId::populate_parameters, caller);
  const bool transition_accepted =
      result == kResultSuccess && transition.transition_applied &&
      transition.after == bridge::LifecycleState::populated &&
      transition.decision == bridge::TransitionDecision::populated &&
      transition.response == bridge::BoundaryResponse::success &&
      slot_runtime(Slot::cache_populate)
              .identity.load(std::memory_order_acquire) !=
          IdentityState::rejected;
  if (!transition_accepted) {
    g_bridge_lookup_transaction_complete.store(false,
                                               std::memory_order_release);
    g_bridge_route_state.store(-1, std::memory_order_release);
    poison_bridge_boundary();
  }

  const bool boundary_clean = boundary.finish();
  if (!boundary_clean) {
    g_bridge_lookup_transaction_complete.store(false,
                                               std::memory_order_release);
    g_bridge_route_state.store(-1, std::memory_order_release);
  }
  const NgxResult returned_result = transition_accepted && boundary_clean
                                        ? kResultSuccess
                                        : kResultNotInitialized;
  publish_bridge_event(
      bridge::ExportId::populate_parameters, caller, transition,
      returned_result, reinterpret_cast<std::uintptr_t>(parameters), 0U,
      false, nullptr, nullptr, false);
  if (!boundary_clean && transition.after != bridge::LifecycleState::inert) {
    const bridge::TransitionResult abort_transition = bridge_apply(
        bridge::LifecycleOperation::abort,
        bridge::ExportId::populate_parameters, caller);
    publish_bridge_event(bridge::ExportId::populate_parameters, caller,
                         abort_transition, kResultNotInitialized,
                         reinterpret_cast<std::uintptr_t>(parameters));
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(outgoing_last_error);
  return returned_result;
#else
  return result;
#endif
}

#if defined(MHW_DLSS_BRIDGE_PROBE)
extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
bridge_d3d12_init_wrapper(const std::uint64_t application_id,
                          const wchar_t *const application_data_path,
                          ID3D12Device *const device,
                          const std::uint32_t api_version) noexcept {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  BridgeBoundaryLease boundary;
  if (!boundary) {
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return kResultNotInitialized;
  }

  const bridge::CallerEvidence caller_evidence = bridge_caller_evidence(
      bridge::ExportId::d3d12_init, caller);
  const bool caller_verified = bridge::caller_is_verified_for(
      caller_evidence, bridge::ExportId::d3d12_init);
  const bool lifecycle_ready =
      bridge_lifecycle_state() == bridge::LifecycleState::verified;
  const bool route_table_complete = bridge_route_table_complete();
  if (!route_table_complete) {
    g_bridge_lookup_transaction_complete.store(false,
                                               std::memory_order_release);
    g_bridge_route_state.store(-1, std::memory_order_release);
    poison_bridge_boundary();
  }
  bridge::InitValidationEvidence validation{
      caller_verified,
      lifecycle_ready,
      application_id,
      readable_nonempty_path(application_data_path),
      api_version,
      route_table_complete,
      false};
  const std::uint32_t preliminary_validation_mask =
      bridge::init_validation_mask(validation);
  const bool preliminary_arguments_valid =
      preliminary_validation_mask == bridge::kInitValidationMaskWithoutDevice;
  const bool device_verified =
      preliminary_arguments_valid && verify_d3d12_device(device);
  validation.d3d12_device_verified = device_verified;
  const std::uint32_t validation_mask =
      bridge::init_validation_mask(validation);
  const bool arguments_valid = bridge::init_validation_is_complete(validation);
  const bridge::TransitionResult transition = bridge_apply(
      arguments_valid ? bridge::LifecycleOperation::initialize
                      : bridge::LifecycleOperation::abort,
      bridge::ExportId::d3d12_init, caller);
  NgxResult result =
      !arguments_valid
          ? kResultInvalidParameter
          : (transition.response == bridge::BoundaryResponse::success
                 ? kResultSuccess
                 : kResultNotInitialized);
  if (result == kResultSuccess) {
    g_bridge_init_succeeded.store(true, std::memory_order_release);
  }
  const bool boundary_clean = boundary.finish();
  if (!boundary_clean) {
    g_bridge_init_succeeded.store(false, std::memory_order_release);
    result = kResultNotInitialized;
  }
  publish_bridge_event(bridge::ExportId::d3d12_init, caller, transition,
                       result, application_id, api_version, device_verified,
                       nullptr, nullptr, false, validation_mask);
  if (!boundary_clean && transition.after != bridge::LifecycleState::inert) {
    const bridge::TransitionResult abort_transition = bridge_apply(
        bridge::LifecycleOperation::abort, bridge::ExportId::d3d12_init,
        caller);
    publish_bridge_event(bridge::ExportId::d3d12_init, caller,
                         abort_transition, kResultNotInitialized,
                         application_id, api_version, device_verified, nullptr,
                         nullptr, false, validation_mask);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(incoming_last_error);
  return result;
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
bridge_d3d12_scratch_wrapper(const Feature feature,
                             const NgxParameter *const parameters,
                             std::size_t *const scratch_size) noexcept {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  const bool output_cleared = store_bridge_size(scratch_size, 0U);
  BridgeBoundaryLease boundary;
  if (!boundary) {
    g_bridge_last_scratch_result.store(kResultNotInitialized,
                                       std::memory_order_release);
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return kResultNotInitialized;
  }
  const bridge::LifecycleState state = bridge_lifecycle_state();
  const bool state_valid = state == bridge::LifecycleState::initialized ||
                           state == bridge::LifecycleState::populated;
  const bool arguments_valid =
      output_cleared && bridge_caller_is_pinned_modern_core(caller) &&
      feature == 1 && parameters != nullptr && bridge_route_table_complete();
  const bool operation_valid = arguments_valid && state_valid;
  NgxResult result = kResultNotInitialized;
  bool operation_poisoned = false;
  if (operation_valid &&
      store_bridge_size(scratch_size, kExpectedScratchBufferSize)) {
    result = kResultSuccess;
  } else {
    operation_poisoned = true;
    g_bridge_lookup_transaction_complete.store(false,
                                               std::memory_order_release);
    g_bridge_route_state.store(-1, std::memory_order_release);
    static_cast<void>(bridge_apply(
        bridge::LifecycleOperation::abort,
        bridge::ExportId::d3d12_get_scratch_buffer_size, caller));
    poison_bridge_boundary();
    static_cast<void>(store_bridge_size(scratch_size, 0U));
  }
  const bool boundary_clean = boundary.finish();
  if (!boundary_clean) {
    g_bridge_lookup_transaction_complete.store(false,
                                               std::memory_order_release);
    g_bridge_route_state.store(-1, std::memory_order_release);
    static_cast<void>(store_bridge_size(scratch_size, 0U));
    result = kResultNotInitialized;
    if (!operation_poisoned) {
      static_cast<void>(bridge_apply(
          bridge::LifecycleOperation::abort,
          bridge::ExportId::d3d12_get_scratch_buffer_size, caller));
    }
  }
  g_bridge_scratch_calls.fetch_add(1U, std::memory_order_relaxed);
  std::uintptr_t no_caller{};
  static_cast<void>(g_bridge_first_scratch_caller.compare_exchange_strong(
      no_caller, reinterpret_cast<std::uintptr_t>(caller),
      std::memory_order_release, std::memory_order_relaxed));
  g_bridge_last_scratch_result.store(result, std::memory_order_release);
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(incoming_last_error);
  return result;
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
bridge_d3d12_create_wrapper(ID3D12GraphicsCommandList *const command_list,
                            const Feature feature,
                            const NgxParameter *const parameters,
                            void **const output_handle) noexcept {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);

  const bridge::CallerEvidence caller_evidence = bridge_caller_evidence(
      bridge::ExportId::d3d12_create_feature, caller);
  const bool caller_verified = bridge::caller_is_verified_for(
      caller_evidence, bridge::ExportId::d3d12_create_feature);
  const bool output_cleared =
      caller_verified && store_bridge_output(output_handle, nullptr);
  BridgeBoundaryLease boundary;
  if (!boundary) {
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return kResultFeatureNotSupported;
  }
  const bridge::LifecycleState lifecycle_state = bridge_lifecycle_state();
  const bool lifecycle_ready =
      lifecycle_state == bridge::LifecycleState::initialized ||
      lifecycle_state == bridge::LifecycleState::populated;
  const bool preliminary_arguments_valid =
      output_cleared && caller_verified && lifecycle_ready && feature == 1 &&
      parameters != nullptr && bridge_route_table_complete();
  const bool command_list_verified =
      preliminary_arguments_valid &&
      verify_d3d12_command_list(command_list);
  const bool arguments_valid =
      preliminary_arguments_valid && command_list_verified;
  bridge::TransitionResult transition = bridge_apply(
      arguments_valid ? bridge::LifecycleOperation::capture_create
                      : bridge::LifecycleOperation::abort,
      bridge::ExportId::d3d12_create_feature, caller);

  bridge::CreateContractSnapshot snapshot{};
  bool snapshot_captured = false;
  if (arguments_valid && transition.capture_committed) {
    snapshot = capture_bridge_create_contract(
        const_cast<NgxParameter *>(parameters));
    snapshot_captured = true;
  }

  NgxResult result = kResultFeatureNotSupported;
  const bool token_requested =
      transition.response == bridge::BoundaryResponse::success_with_token;
  const bool snapshot_complete =
      snapshot_captured && snapshot.summary.complete;
  bool token_written =
      token_requested && snapshot_complete &&
      store_bridge_output(
          output_handle,
          const_cast<std::uint64_t *>(&g_bridge_token_storage));
  if (token_written) {
    g_bridge_token_issued.store(true, std::memory_order_release);
    result = kResultSuccess;
  }
  const bool boundary_clean = boundary.finish();
  if (!boundary_clean) {
    if (token_written) {
      g_bridge_token_issued.store(false, std::memory_order_release);
      static_cast<void>(store_bridge_output(output_handle, nullptr));
      token_written = false;
    }
    result = kResultFeatureNotSupported;
  }
  publish_bridge_event(
      bridge::ExportId::d3d12_create_feature, caller, transition, result,
      static_cast<std::uint64_t>(feature),
      reinterpret_cast<std::uintptr_t>(parameters), command_list_verified,
      snapshot_captured ? &snapshot : nullptr, nullptr);
  if (transition.capture_committed &&
      (!snapshot_complete || (token_requested && !token_written) ||
       !boundary_clean)) {
    const bridge::TransitionResult abort_transition = bridge_apply(
        bridge::LifecycleOperation::abort,
        bridge::ExportId::d3d12_create_feature, caller);
    publish_bridge_event(bridge::ExportId::d3d12_create_feature, caller,
                         abort_transition, kResultFeatureNotSupported,
                         static_cast<std::uint64_t>(feature),
                          reinterpret_cast<std::uintptr_t>(parameters),
                          command_list_verified);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(incoming_last_error);
  return result;
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
bridge_d3d12_evaluate_wrapper(
    ID3D12GraphicsCommandList *const command_list,
    const void *const feature_handle,
    const NgxParameter *const parameters,
    const BridgeProgressCallback) noexcept {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  BridgeBoundaryLease boundary;
  if (!boundary) {
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return kResultNotImplemented;
  }

  const bridge::CallerEvidence caller_evidence = bridge_caller_evidence(
      bridge::ExportId::d3d12_evaluate_feature, caller);
  const bool caller_verified = bridge::caller_is_verified_for(
      caller_evidence, bridge::ExportId::d3d12_evaluate_feature);
  const bool lifecycle_ready = bridge_lifecycle_state() ==
                               bridge::LifecycleState::evaluate_token_issued;
  const std::uint64_t token =
      caller_verified && lifecycle_ready ? read_bridge_token(feature_handle)
                                         : 0U;
  const bool preliminary_arguments_valid =
      caller_verified && lifecycle_ready && parameters != nullptr &&
      token == kOneEvaluateToken && bridge_route_table_complete();
  const bool command_list_verified =
      preliminary_arguments_valid &&
      verify_d3d12_command_list(command_list);
  const bool arguments_valid =
      preliminary_arguments_valid && command_list_verified;
  bridge::TransitionResult transition = bridge_apply(
      arguments_valid ? bridge::LifecycleOperation::capture_evaluate
                      : bridge::LifecycleOperation::abort,
      bridge::ExportId::d3d12_evaluate_feature, caller, token);

  bridge::EvaluateContractSnapshot snapshot{};
  bool snapshot_captured = false;
  if (arguments_valid && transition.capture_committed) {
    snapshot = capture_bridge_evaluate_contract(
        const_cast<NgxParameter *>(parameters));
    snapshot_captured = true;
  }
  const bool boundary_clean = boundary.finish();
  publish_bridge_event(
      bridge::ExportId::d3d12_evaluate_feature, caller, transition,
      kResultNotImplemented, token,
      reinterpret_cast<std::uintptr_t>(parameters), command_list_verified,
      nullptr, snapshot_captured ? &snapshot : nullptr);
  if (transition.capture_committed &&
      ((!snapshot_captured || !snapshot.summary.complete) ||
       !boundary_clean)) {
    const bridge::TransitionResult abort_transition = bridge_apply(
        bridge::LifecycleOperation::abort,
        bridge::ExportId::d3d12_evaluate_feature, caller, token);
    publish_bridge_event(bridge::ExportId::d3d12_evaluate_feature, caller,
                         abort_transition, kResultNotImplemented, token,
                          reinterpret_cast<std::uintptr_t>(parameters),
                          command_list_verified);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(incoming_last_error);
  return kResultNotImplemented;
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
bridge_d3d12_release_wrapper(void *const feature_handle) noexcept {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  const bridge::CallerEvidence caller_evidence = bridge_caller_evidence(
      bridge::ExportId::d3d12_release_feature, caller);
  const bool caller_verified = bridge::caller_is_verified_for(
      caller_evidence, bridge::ExportId::d3d12_release_feature);
  const std::uint64_t token =
      caller_verified ? read_bridge_token(feature_handle) : 0U;
  BridgeBoundaryLease boundary;
  if (!boundary) {
    bool expected_outstanding = true;
    const bool cleanup_authorized =
        caller_verified && token == kOneEvaluateToken &&
        g_bridge_token_issued.compare_exchange_strong(
            expected_outstanding, false, std::memory_order_acq_rel,
            std::memory_order_acquire);
    const bool owner_finished =
        g_bridge_boundary_state.load(std::memory_order_acquire) == 3U;
    const bridge::TransitionResult transition =
        owner_finished
            ? bridge_apply(cleanup_authorized
                               ? bridge::LifecycleOperation::release
                               : bridge::LifecycleOperation::abort,
                           bridge::ExportId::d3d12_release_feature, caller,
                           token)
            : bridge_observation_transition();
    const NgxResult result =
        cleanup_authorized ? kResultSuccess : kResultNotInitialized;
    publish_bridge_event(bridge::ExportId::d3d12_release_feature, caller,
                         transition, result, token, 0U, false, nullptr,
                         nullptr, !owner_finished);
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return result;
  }
  bool expected_outstanding = true;
  const bool cleanup_authorized =
      caller_verified && token == kOneEvaluateToken &&
      g_bridge_token_issued.compare_exchange_strong(
          expected_outstanding, false, std::memory_order_acq_rel,
          std::memory_order_acquire);
  const bridge::TransitionResult transition = bridge_apply(
      cleanup_authorized ? bridge::LifecycleOperation::release
                         : bridge::LifecycleOperation::abort,
      bridge::ExportId::d3d12_release_feature, caller, token);
  const NgxResult result = cleanup_authorized ? kResultSuccess
                                               : kResultNotInitialized;
  const bool boundary_clean = boundary.finish();
  publish_bridge_event(bridge::ExportId::d3d12_release_feature, caller,
                       transition, result, token);
  if (!boundary_clean && transition.after != bridge::LifecycleState::inert) {
    const bridge::TransitionResult abort_transition = bridge_apply(
        bridge::LifecycleOperation::abort,
        bridge::ExportId::d3d12_release_feature, caller, token);
    publish_bridge_event(bridge::ExportId::d3d12_release_feature, caller,
                         abort_transition, result, token);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(incoming_last_error);
  return result;
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
bridge_d3d12_shutdown_wrapper() noexcept {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  const bridge::CallerEvidence caller_evidence =
      bridge_caller_evidence(bridge::ExportId::d3d12_shutdown, caller);
  BridgeBoundaryLease boundary;
  if (!boundary) {
    bool expected_initialized = true;
    const bool cleanup_authorized =
        bridge::caller_is_verified_for(caller_evidence,
                                       bridge::ExportId::d3d12_shutdown) &&
        g_bridge_init_succeeded.compare_exchange_strong(
            expected_initialized, false, std::memory_order_acq_rel,
            std::memory_order_acquire);
    const bool owner_finished =
        g_bridge_boundary_state.load(std::memory_order_acquire) == 3U;
    const bridge::TransitionResult transition =
        owner_finished
            ? bridge_apply(cleanup_authorized
                               ? bridge::LifecycleOperation::shutdown
                               : bridge::LifecycleOperation::abort,
                           bridge::ExportId::d3d12_shutdown, caller)
            : bridge_observation_transition();
    const NgxResult result =
        cleanup_authorized ? kResultSuccess : kResultNotInitialized;
    publish_bridge_event(bridge::ExportId::d3d12_shutdown, caller,
                         transition, result, 0U, 0U, false, nullptr,
                         nullptr, !owner_finished);
    g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return result;
  }
  bool expected_initialized = true;
  const bool cleanup_authorized =
      bridge::caller_is_verified_for(caller_evidence,
                                     bridge::ExportId::d3d12_shutdown) &&
      g_bridge_init_succeeded.compare_exchange_strong(
          expected_initialized, false, std::memory_order_acq_rel,
          std::memory_order_acquire);
  const bridge::TransitionResult transition = bridge_apply(
      cleanup_authorized ? bridge::LifecycleOperation::shutdown
                         : bridge::LifecycleOperation::abort,
      bridge::ExportId::d3d12_shutdown, caller);
  const NgxResult result = cleanup_authorized ? kResultSuccess
                                               : kResultNotInitialized;
  const bool boundary_clean = boundary.finish();
  publish_bridge_event(bridge::ExportId::d3d12_shutdown, caller, transition,
                       result);
  if (!boundary_clean && transition.after != bridge::LifecycleState::inert) {
    const bridge::TransitionResult abort_transition = bridge_apply(
        bridge::LifecycleOperation::abort,
        bridge::ExportId::d3d12_shutdown, caller);
    publish_bridge_event(bridge::ExportId::d3d12_shutdown, caller,
                         abort_transition, result);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(incoming_last_error);
  return result;
}
#endif

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
legacy_capability_wrapper(NgxParameter **parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_get_parameters<Slot::legacy_capability>(
      parameters, caller, incoming_last_error);
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
legacy_parameters_wrapper(NgxParameter **parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_get_parameters<Slot::legacy_parameters>(
      parameters, caller, incoming_last_error);
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
modern_capability_wrapper(NgxParameter **parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_get_parameters<Slot::modern_capability>(
      parameters, caller, incoming_last_error);
}

extern "C" __declspec(dllexport) __declspec(noinline) NgxResult __cdecl
modern_parameters_wrapper(NgxParameter **parameters) {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  return invoke_get_parameters<Slot::modern_parameters>(
      parameters, caller, incoming_last_error);
}

extern "C" __declspec(noinline) void __cdecl
modern_d3d12_feature_status_detour(
    void *core, const void *feature_path, void *feature_state,
    const std::int32_t feature_index, void *context, const std::uint32_t api,
    const NgxResult status) noexcept {
  const DWORD incoming_last_error = GetLastError();
  const void *const caller = _ReturnAddress();
  const auto original = reinterpret_cast<FeatureStatusRecorderFn>(
      g_original_feature_status_recorder.load(std::memory_order_acquire));
  if (original == nullptr) {
    SetLastError(incoming_last_error);
    return;
  }

  g_active_wrapper_calls.fetch_add(1U, std::memory_order_seq_cst);
  SlotRuntime &runtime = slot_runtime(Slot::modern_d3d12_feature_status);
  const bool trace_window =
      g_trace_enabled.load(std::memory_order_seq_cst);
  if (trace_window) {
    g_feature_status_detours.fetch_add(1U, std::memory_order_relaxed);
  }
  const bool trace_active =
      trace_window &&
      g_feature_status_hook_state.load(std::memory_order_acquire) == 2 &&
      runtime.identity.load(std::memory_order_acquire) ==
          IdentityState::verified;
  const std::uintptr_t verified_module =
      runtime.module.load(std::memory_order_acquire);
  const bool gate_matches =
      verified_module != 0U &&
      reinterpret_cast<std::uintptr_t>(caller) ==
          verified_module + 0x67704U &&
      feature_index == 1 && api == 2U;
  if (trace_active && !gate_matches) {
    g_feature_status_gate_rejects.fetch_add(1U, std::memory_order_relaxed);
  }

  constexpr std::uint32_t mask =
      1U << slot_index(Slot::modern_d3d12_feature_status);
  const bool recursive = (g_active_slots & mask) != 0U;
  if (recursive) {
    runtime.recursion.fetch_add(1U, std::memory_order_relaxed);
  }
  const std::uint32_t previous_mask = g_active_slots;
  g_active_slots |= mask;
  const ULONGLONG start = GetTickCount64();
  SetLastError(incoming_last_error);
  original(core, feature_path, feature_state, feature_index, context, api,
           status);
  const DWORD outgoing_last_error = GetLastError();
  const ULONGLONG duration = GetTickCount64() - start;
  g_active_slots = previous_mask;
  if (trace_active && gate_matches && !recursive) {
    publish_event(Slot::modern_d3d12_feature_status, caller, context, status,
                  duration, feature_index, api);
  }
  g_active_wrapper_calls.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(outgoing_last_error);
}

namespace {

FARPROC wrapper_for_slot(const Slot slot) noexcept {
  switch (slot) {
  case Slot::local_populate:
    return reinterpret_cast<FARPROC>(&local_populate_wrapper);
  case Slot::cache_populate:
    return reinterpret_cast<FARPROC>(&cache_populate_wrapper);
  case Slot::legacy_capability:
    return reinterpret_cast<FARPROC>(&legacy_capability_wrapper);
  case Slot::legacy_parameters:
    return reinterpret_cast<FARPROC>(&legacy_parameters_wrapper);
  case Slot::modern_capability:
    return reinterpret_cast<FARPROC>(&modern_capability_wrapper);
  case Slot::modern_parameters:
    return reinterpret_cast<FARPROC>(&modern_parameters_wrapper);
  case Slot::modern_d3d12_feature_status:
    break;
  }
  return nullptr;
}

FARPROC bind_slot(Slot slot, HMODULE module, FARPROC target,
                  const void *caller) noexcept;

#if defined(MHW_DLSS_BRIDGE_PROBE)
enum class BridgeRouteResult : std::uint8_t {
  not_candidate,
  substituted,
  guarded_null,
  rejected,
};

bool is_bridge_target_name(const TargetName name) noexcept {
  switch (name) {
  case TargetName::populate:
  case TargetName::d3d12_init:
  case TargetName::d3d12_scratch:
  case TargetName::d3d12_create:
  case TargetName::d3d12_evaluate:
  case TargetName::d3d12_release:
  case TargetName::d3d12_shutdown:
  case TargetName::d3d12_shutdown1:
  case TargetName::d3d12_create1:
  case TargetName::d3d12_init_ext:
  case TargetName::d3d12_init_ext1:
  case TargetName::d3d12_init_ext2:
  case TargetName::d3d12_populate_parameters_impl:
  case TargetName::d3d12_populate_device_parameters_impl:
    return true;
  case TargetName::unknown:
  case TargetName::capability_parameters:
  case TargetName::parameters:
  case TargetName::d3d12_evaluate_c:
    return false;
  }
  return false;
}

bridge::ExportId bridge_export_for_target(TargetName name) noexcept;

std::uint32_t bridge_route_bit(const TargetName name) noexcept {
  return bridge::provider_route_bit(bridge_export_for_target(name));
}

std::uint32_t bridge_lookup_bit(const TargetName name) noexcept {
  return bridge::provider_lookup_bit(bridge_export_for_target(name));
}

bridge::ExportId bridge_export_for_target(const TargetName name) noexcept {
  switch (name) {
  case TargetName::populate:
    return bridge::ExportId::populate_parameters;
  case TargetName::d3d12_init:
    return bridge::ExportId::d3d12_init;
  case TargetName::d3d12_create:
    return bridge::ExportId::d3d12_create_feature;
  case TargetName::d3d12_evaluate:
    return bridge::ExportId::d3d12_evaluate_feature;
  case TargetName::d3d12_evaluate_c:
    return bridge::ExportId::unknown;
  case TargetName::d3d12_release:
    return bridge::ExportId::d3d12_release_feature;
  case TargetName::d3d12_shutdown:
    return bridge::ExportId::d3d12_shutdown;
  case TargetName::d3d12_scratch:
    return bridge::ExportId::d3d12_get_scratch_buffer_size;
  case TargetName::d3d12_shutdown1:
    return bridge::ExportId::d3d12_shutdown1;
  case TargetName::d3d12_create1:
    return bridge::ExportId::d3d12_create_feature1;
  case TargetName::d3d12_init_ext:
    return bridge::ExportId::d3d12_init_ext;
  case TargetName::d3d12_init_ext1:
    return bridge::ExportId::d3d12_init_ext1;
  case TargetName::d3d12_init_ext2:
    return bridge::ExportId::d3d12_init_ext2;
  case TargetName::d3d12_populate_parameters_impl:
    return bridge::ExportId::d3d12_populate_parameters_impl;
  case TargetName::d3d12_populate_device_parameters_impl:
    return bridge::ExportId::d3d12_populate_device_parameters_impl;
  case TargetName::unknown:
  case TargetName::capability_parameters:
  case TargetName::parameters:
    return bridge::ExportId::unknown;
  }
  return bridge::ExportId::unknown;
}

std::uintptr_t bridge_target_rva(const TargetName name) noexcept {
  switch (name) {
  case TargetName::d3d12_shutdown:
    return 0x08310U;
  case TargetName::d3d12_scratch:
    return 0x08370U;
  case TargetName::d3d12_create:
    return 0x083A0U;
  case TargetName::d3d12_release:
    return 0x083C0U;
  case TargetName::d3d12_evaluate:
    return 0x083E0U;
  case TargetName::d3d12_init:
    return 0x08410U;
  case TargetName::populate:
    return 0x1DFA0U;
  case TargetName::unknown:
  case TargetName::capability_parameters:
  case TargetName::parameters:
  case TargetName::d3d12_evaluate_c:
  case TargetName::d3d12_shutdown1:
  case TargetName::d3d12_create1:
  case TargetName::d3d12_init_ext:
  case TargetName::d3d12_init_ext1:
  case TargetName::d3d12_init_ext2:
  case TargetName::d3d12_populate_parameters_impl:
  case TargetName::d3d12_populate_device_parameters_impl:
    return 0U;
  }
  return 0U;
}

std::uintptr_t bridge_resolver_return_rva(const TargetName name) noexcept {
  switch (name) {
  case TargetName::d3d12_init:
    return 0x4320CU;
  case TargetName::d3d12_shutdown:
    return 0x43222U;
  case TargetName::d3d12_shutdown1:
    return 0x43241U;
  case TargetName::d3d12_scratch:
    return 0x43257U;
  case TargetName::d3d12_create:
    return 0x4326DU;
  case TargetName::d3d12_create1:
    return 0x43283U;
  case TargetName::d3d12_evaluate:
    return 0x43299U;
  case TargetName::d3d12_release:
    return 0x432AFU;
  case TargetName::d3d12_init_ext:
    return 0x432C5U;
  case TargetName::d3d12_init_ext1:
    return 0x432DBU;
  case TargetName::d3d12_init_ext2:
    return 0x432F1U;
  case TargetName::d3d12_populate_parameters_impl:
    return 0x43333U;
  case TargetName::populate:
    return 0x43351U;
  case TargetName::d3d12_populate_device_parameters_impl:
    return 0x43367U;
  case TargetName::unknown:
  case TargetName::capability_parameters:
  case TargetName::parameters:
  case TargetName::d3d12_evaluate_c:
    return 0U;
  }
  return 0U;
}

FARPROC bridge_wrapper_for_target(const TargetName name) noexcept {
  switch (name) {
  case TargetName::d3d12_init:
    return reinterpret_cast<FARPROC>(&bridge_d3d12_init_wrapper);
  case TargetName::d3d12_scratch:
    return reinterpret_cast<FARPROC>(&bridge_d3d12_scratch_wrapper);
  case TargetName::d3d12_create:
    return reinterpret_cast<FARPROC>(&bridge_d3d12_create_wrapper);
  case TargetName::d3d12_evaluate:
    return reinterpret_cast<FARPROC>(&bridge_d3d12_evaluate_wrapper);
  case TargetName::d3d12_release:
    return reinterpret_cast<FARPROC>(&bridge_d3d12_release_wrapper);
  case TargetName::d3d12_shutdown:
    return reinterpret_cast<FARPROC>(&bridge_d3d12_shutdown_wrapper);
  case TargetName::populate:
    return reinterpret_cast<FARPROC>(&cache_populate_wrapper);
  case TargetName::unknown:
  case TargetName::capability_parameters:
  case TargetName::parameters:
  case TargetName::d3d12_evaluate_c:
  case TargetName::d3d12_shutdown1:
  case TargetName::d3d12_create1:
  case TargetName::d3d12_init_ext:
  case TargetName::d3d12_init_ext1:
  case TargetName::d3d12_init_ext2:
  case TargetName::d3d12_populate_parameters_impl:
  case TargetName::d3d12_populate_device_parameters_impl:
    return nullptr;
  }
  return nullptr;
}

bool bridge_target_prologue_matches(const TargetName name,
                                    const void *const target) noexcept {
  switch (name) {
  case TargetName::d3d12_init:
    return exact_bytes(target, kCacheInitPrologue);
  case TargetName::d3d12_shutdown:
    return exact_bytes(target, kCacheShutdownPrologue);
  case TargetName::d3d12_scratch:
    return exact_bytes(target, kCacheScratchPrologue);
  case TargetName::d3d12_create:
    return exact_bytes(target, kCacheCreatePrologue);
  case TargetName::d3d12_evaluate:
    return exact_bytes(target, kCacheEvaluatePrologue);
  case TargetName::d3d12_release:
    return exact_bytes(target, kCacheReleasePrologue);
  case TargetName::populate:
    return exact_bytes(target, kCachePopulatePrologue);
  case TargetName::unknown:
  case TargetName::capability_parameters:
  case TargetName::parameters:
  case TargetName::d3d12_evaluate_c:
  case TargetName::d3d12_shutdown1:
  case TargetName::d3d12_create1:
  case TargetName::d3d12_init_ext:
  case TargetName::d3d12_init_ext1:
  case TargetName::d3d12_init_ext2:
  case TargetName::d3d12_populate_parameters_impl:
  case TargetName::d3d12_populate_device_parameters_impl:
    return false;
  }
  return false;
}

bool mapped_cache_candidate(const HMODULE module) noexcept {
  PeIdentity identity{};
  return read_mapped_pe_identity(module, &identity) &&
         identity.machine == IMAGE_FILE_MACHINE_AMD64 &&
         identity.optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
         identity.timestamp == 0x64405395U &&
         identity.image_size == 0x00B57000U &&
         identity.image_base == reinterpret_cast<std::uintptr_t>(module);
}

bool module_path_matches_fixed(
    const HMODULE module, const wchar_t *const expected,
    const std::atomic<std::uint32_t> &expected_length) noexcept {
  if (module == nullptr || expected == nullptr) {
    return false;
  }
  const std::uint32_t wanted =
      expected_length.load(std::memory_order_acquire);
  if (wanted == 0U || wanted >= kResolverPathCapacity) {
    return false;
  }
  wchar_t actual[kResolverPathCapacity]{};
  const DWORD actual_length = GetModuleFileNameW(
      module, actual, static_cast<DWORD>(_countof(actual)));
  return actual_length == wanted && actual_length < _countof(actual) &&
         equal_path_text(std::wstring_view(actual, actual_length),
                         std::wstring_view(expected, wanted));
}

bool bridge_current_target_matches(const TargetName name,
                                   const HMODULE module,
                                   const FARPROC target) noexcept {
  const std::uintptr_t rva = bridge_target_rva(name);
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  return rva != 0U && mapped_cache_candidate(module) && target != nullptr &&
         reinterpret_cast<std::uintptr_t>(target) == base + rva &&
         executable_image_address(module,
                                  reinterpret_cast<const void *>(target)) &&
         bridge_target_prologue_matches(
             name, reinterpret_cast<const void *>(target));
}

bool bridge_cache_module_matches(const HMODULE module) noexcept {
  if (!mapped_cache_candidate(module) ||
      !module_path_matches_fixed(module, g_expected_cache_path,
                                 g_expected_cache_path_length)) {
    return false;
  }

  const auto base = reinterpret_cast<std::uintptr_t>(module);
  const auto exact_executable = [module, base](
                                    const std::uintptr_t rva,
                                    const auto &bytes) noexcept {
    const auto *const address = reinterpret_cast<const void *>(base + rva);
    return executable_image_address(module, address) &&
           exact_bytes(address, bytes);
  };
  return exact_executable(0x08310U, kCacheShutdownPrologue) &&
         exact_executable(0x08370U, kCacheScratchPrologue) &&
         exact_executable(0x083A0U, kCacheCreatePrologue) &&
         exact_executable(0x083C0U, kCacheReleasePrologue) &&
         exact_executable(0x083E0U, kCacheEvaluatePrologue) &&
         exact_executable(0x08410U, kCacheInitPrologue) &&
         exact_executable(0x1DFA0U, kCachePopulatePrologue);
}

bool bridge_modern_core_caller_matches(
    const TargetName name, const void *const caller,
    std::uintptr_t *const output_base) noexcept {
  if (output_base == nullptr) {
    return false;
  }
  *output_base = 0U;
  const std::uintptr_t return_rva = bridge_resolver_return_rva(name);
  if (return_rva == 0U ||
      !caller_is_module_rva(caller, 0x6A0CB15EU, 0x00173000U,
                            return_rva)) {
    return false;
  }
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(caller, &information, sizeof(information)) !=
          sizeof(information) ||
      information.AllocationBase == nullptr) {
    return false;
  }
  const auto module = static_cast<HMODULE>(information.AllocationBase);
  if (!module_path_matches_fixed(module, g_expected_modern_core_path,
                                 g_expected_modern_core_path_length)) {
    return false;
  }
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  if (!executable_image_address(
          module, reinterpret_cast<const void *>(base + 0x66C50U)) ||
      !exact_bytes(reinterpret_cast<const void *>(base + 0x66C50U),
                   kModernParametersPrologue)) {
    return false;
  }
  *output_base = base;
  return true;
}

bool bridge_fast_route_matches(const TargetName name, const HMODULE module,
                               const FARPROC target,
                               const void *const caller) noexcept {
  const std::uintptr_t cache_base =
      g_bridge_cache_base.load(std::memory_order_acquire);
  const std::uintptr_t modern_base =
      g_bridge_modern_core_base.load(std::memory_order_acquire);
  return cache_base != 0U && modern_base != 0U &&
         reinterpret_cast<std::uintptr_t>(module) == cache_base &&
         GetModuleHandleW(L"mhw_dlss_unlock.dll") == nullptr &&
         module_path_matches_fixed(module, g_expected_cache_path,
                                   g_expected_cache_path_length) &&
         bridge_current_target_matches(name, module, target) &&
         caller_is_module_rva(caller, 0x6A0CB15EU, 0x00173000U,
                              bridge_resolver_return_rva(name)) &&
         reinterpret_cast<std::uintptr_t>(caller) ==
             modern_base + bridge_resolver_return_rva(name);
}

bool bridge_fast_guard_matches(const TargetName name, const HMODULE module,
                               const FARPROC target,
                               const void *const caller) noexcept {
  const std::uintptr_t cache_base =
      g_bridge_cache_base.load(std::memory_order_acquire);
  const std::uintptr_t modern_base =
      g_bridge_modern_core_base.load(std::memory_order_acquire);
  return bridge::is_provider_guard_export(bridge_export_for_target(name)) &&
         cache_base != 0U && modern_base != 0U && target == nullptr &&
         reinterpret_cast<std::uintptr_t>(module) == cache_base &&
         GetModuleHandleW(L"mhw_dlss_unlock.dll") == nullptr &&
         mapped_cache_candidate(module) &&
         module_path_matches_fixed(module, g_expected_cache_path,
                                   g_expected_cache_path_length) &&
         caller_is_module_rva(caller, 0x6A0CB15EU, 0x00173000U,
                              bridge_resolver_return_rva(name)) &&
         reinterpret_cast<std::uintptr_t>(caller) ==
             modern_base + bridge_resolver_return_rva(name);
}

BridgeRouteResult maybe_route_bridge_export(
    const TargetName name, const HMODULE module, const FARPROC target,
    const void *const caller, FARPROC *const output) {
  if (output == nullptr ||
      g_bridge_mode.load(std::memory_order_acquire) ==
          bridge::Mode::disabled ||
      !is_bridge_target_name(name)) {
    return BridgeRouteResult::not_candidate;
  }
  *output = nullptr;
  const bridge::ExportId export_id = bridge_export_for_target(name);
  const std::uint32_t lookup_bit = bridge_lookup_bit(name);
  const bool guard_only = bridge::is_provider_guard_export(export_id);
  const bool generic_callback =
      export_id == bridge::ExportId::populate_parameters;
  const FARPROC wrapper = bridge_wrapper_for_target(name);
  if (export_id == bridge::ExportId::unknown || lookup_bit == 0U ||
      (!guard_only && wrapper == nullptr)) {
    return BridgeRouteResult::rejected;
  }

  std::uintptr_t observed_modern_base{};
  const bool exact_resolver_origin = bridge_modern_core_caller_matches(
      name, caller, &observed_modern_base);

  AcquireSRWLockExclusive(&g_bridge_route_lock);
  int route_state = g_bridge_route_state.load(std::memory_order_acquire);
  if (route_state == -2) {
    ReleaseSRWLockExclusive(&g_bridge_route_lock);
    return BridgeRouteResult::not_candidate;
  }
  const bool interception_started =
      g_bridge_interception_started.load(std::memory_order_acquire);
  const std::uint32_t seen_mask =
      g_bridge_lookup_seen_mask.load(std::memory_order_acquire);
  const std::uintptr_t pinned_cache_base =
      g_bridge_cache_base.load(std::memory_order_acquire);
  const DWORD route_thread_id =
      g_bridge_route_thread_id.load(std::memory_order_acquire);
  const bool route_thread_matches =
      route_thread_id != 0U && route_thread_id == GetCurrentThreadId();
  const bool pinned_cache_module =
      pinned_cache_base != 0U &&
      reinterpret_cast<std::uintptr_t>(module) == pinned_cache_base;
  const bool first_transaction_member =
      !interception_started && exact_resolver_origin;
  const bool incomplete_transaction_member =
      interception_started &&
      seen_mask != bridge::kProviderLookupMaskComplete &&
      (route_thread_matches || exact_resolver_origin || pinned_cache_module);
  const bool completed_transaction_member =
      interception_started &&
      seen_mask == bridge::kProviderLookupMaskComplete &&
      exact_resolver_origin &&
      pinned_cache_module &&
      observed_modern_base ==
          g_bridge_modern_core_base.load(std::memory_order_acquire);

  // Joining after Init would leave the modern core with a mixed table of raw
  // and bridge pointers. Treat that startup-worker race as an untouched,
  // inconclusive run instead.
  if (!interception_started && exact_resolver_origin &&
      name != TargetName::d3d12_init) {
    g_bridge_route_state.store(-2, std::memory_order_release);
    g_bridge_route_thread_id.store(GetCurrentThreadId(),
                                   std::memory_order_release);
    g_bridge_lookup_seen_mask.fetch_or(lookup_bit,
                                       std::memory_order_release);
    ReleaseSRWLockExclusive(&g_bridge_route_lock);
    static_cast<void>(bridge_apply(bridge::LifecycleOperation::abort,
                                   export_id, caller));
    g_bridge_route_rejects.fetch_add(1U, std::memory_order_relaxed);
    return BridgeRouteResult::not_candidate;
  }
  if (!first_transaction_member && !incomplete_transaction_member &&
      !completed_transaction_member) {
    ReleaseSRWLockExclusive(&g_bridge_route_lock);
    return BridgeRouteResult::not_candidate;
  }

  if (first_transaction_member) {
    g_bridge_interception_started.store(true, std::memory_order_release);
    g_bridge_route_thread_id.store(GetCurrentThreadId(),
                                   std::memory_order_release);
    g_bridge_modern_core_base.store(observed_modern_base,
                                    std::memory_order_release);
  }

  bool route_poisoned = false;
  const auto poison_route = [&]() noexcept {
    route_poisoned = true;
    g_bridge_lookup_transaction_complete.store(false,
                                               std::memory_order_release);
    if (route_state == -1) {
      return;
    }
    g_bridge_route_state.store(-1, std::memory_order_release);
    route_state = -1;
    poison_bridge_boundary();
    static_cast<void>(bridge_apply(bridge::LifecycleOperation::abort,
                                   export_id, caller));
  };
  if (seen_mask != bridge::kProviderLookupMaskComplete &&
      export_id != bridge::expected_provider_lookup(seen_mask)) {
    poison_route();
  }
  g_bridge_lookup_seen_mask.fetch_or(lookup_bit,
                                     std::memory_order_release);

  if (route_state == 0) {
    std::uintptr_t modern_base{};
    const bool verified =
        g_bridge_environment_ready.load(std::memory_order_acquire) &&
        GetModuleHandleW(L"mhw_dlss_unlock.dll") == nullptr &&
        export_id == bridge::ExportId::d3d12_init &&
        bridge_cache_module_matches(module) &&
        bridge_current_target_matches(name, module, target) &&
        bridge_modern_core_caller_matches(name, caller, &modern_base);
    if (verified) {
      g_bridge_cache_base.store(reinterpret_cast<std::uintptr_t>(module),
                                std::memory_order_release);
      g_bridge_modern_core_base.store(modern_base,
                                      std::memory_order_release);
      g_bridge_route_state.store(1, std::memory_order_release);
      const bridge::TransitionResult transition = bridge_apply(
          bridge::LifecycleOperation::verify,
          export_id, caller);
      if (transition.transition_applied &&
          transition.decision == bridge::TransitionDecision::identity_verified) {
        g_bridge_ever_armed.store(true, std::memory_order_release);
        g_bridge_route_mask.fetch_or(bridge_route_bit(name),
                                     std::memory_order_release);
        route_state = 1;
      } else {
        g_bridge_lookup_transaction_complete.store(
            false, std::memory_order_release);
        g_bridge_route_state.store(-1, std::memory_order_release);
        route_state = -1;
        route_poisoned = true;
        poison_bridge_boundary();
      }
    } else {
      g_bridge_lookup_transaction_complete.store(false,
                                                 std::memory_order_release);
      g_bridge_route_state.store(-1, std::memory_order_release);
      route_state = -1;
      route_poisoned = true;
      poison_bridge_boundary();
      static_cast<void>(bridge_apply(bridge::LifecycleOperation::abort,
                                     export_id, caller));
    }
  }

  const bool sequence_thread_valid =
      seen_mask == bridge::kProviderLookupMaskComplete ||
      route_thread_matches || first_transaction_member;
  const bool exact_member =
      sequence_thread_valid &&
      (guard_only ? bridge_fast_guard_matches(name, module, target, caller)
                  : bridge_fast_route_matches(name, module, target, caller));
  if (route_state == 1 && !exact_member) {
    poison_route();
  }
  if (route_state == 1 && bridge_route_bit(name) != 0U) {
    g_bridge_route_mask.fetch_or(bridge_route_bit(name),
                                 std::memory_order_release);
  }

  BridgeRouteResult result = BridgeRouteResult::substituted;
  if (guard_only) {
    // These seven symbols have different ABIs or bypass the generic Populate
    // callback. The pinned provider exports none of them. Preserve that exact
    // null contract, and suppress a drifted non-null result rather than ever
    // coercing it through one of the base wrappers.
    *output = nullptr;
    result = BridgeRouteResult::guarded_null;
  } else if (generic_callback) {
    // Populate is the only non-lifecycle callback in the transaction. Bind its
    // exact original before returning the observer wrapper. Once poisoned, do
    // not let the raw callback create a mixed modern/cache provider table.
    if (route_state == 1 && exact_member &&
        bind_slot(Slot::cache_populate, module, target, caller) == wrapper) {
      *output = wrapper;
      g_bridge_callback_routes.fetch_add(1U, std::memory_order_relaxed);
    } else {
      *output = nullptr;
      poison_route();
    }
  } else {
    // Every admitted base lifecycle member receives a local wrapper. If one
    // member fails validation, the table therefore fails closed instead of
    // leaking a mixture of original and substituted lifecycle exports.
    *output = wrapper;
  }
  const std::uint32_t completed_lookup_mask =
      seen_mask | lookup_bit;
  if (route_state == 1 && exact_member && !route_poisoned &&
      g_bridge_route_state.load(std::memory_order_acquire) == 1 &&
      completed_lookup_mask == bridge::kProviderLookupMaskComplete) {
    // This release store is the transaction commit point. Runtime wrappers
    // cannot synthesize provider success until every preceding route/guard
    // validation and the generic Populate binding are visible.
    g_bridge_lookup_transaction_complete.store(true,
                                               std::memory_order_release);
  }
  ReleaseSRWLockExclusive(&g_bridge_route_lock);

  if (guard_only) {
    g_bridge_guard_lookups.fetch_add(1U, std::memory_order_relaxed);
    if (target != nullptr) {
      g_bridge_guard_suppressions.fetch_add(1U,
                                            std::memory_order_relaxed);
    }
    if (route_state != 1 || route_poisoned || target != nullptr) {
      g_bridge_guard_rejects.fetch_add(1U, std::memory_order_relaxed);
    }
  } else {
    g_bridge_routes.fetch_add(1U, std::memory_order_relaxed);
  }
  if (route_state != 1 || route_poisoned) {
    g_bridge_route_rejects.fetch_add(1U, std::memory_order_relaxed);
  }
  return result;
}
#endif

bool find_matching_slot(const TargetName name, const HMODULE module,
                         const FARPROC target, const void *const caller,
                         Slot *const output) noexcept {
  if (output == nullptr) {
    return false;
  }
  if (name == TargetName::populate) {
    for (const Slot slot : {Slot::local_populate, Slot::cache_populate}) {
      if (mapped_slot_matches(slot, module, target) &&
          expected_slot_caller(slot, caller)) {
        *output = slot;
        return true;
      }
    }
    return false;
  }
  if (name == TargetName::capability_parameters) {
    for (const Slot slot : {Slot::legacy_capability,
                            Slot::modern_capability}) {
      if (mapped_slot_matches(slot, module, target) &&
          expected_slot_caller(slot, caller)) {
        *output = slot;
        return true;
      }
    }
    return false;
  }
  if (name == TargetName::parameters) {
    for (const Slot slot : {Slot::legacy_parameters,
                            Slot::modern_parameters}) {
      if (mapped_slot_matches(slot, module, target) &&
          expected_slot_caller(slot, caller)) {
        *output = slot;
        return true;
      }
    }
    return false;
  }
  return false;
}

FARPROC bind_slot(const Slot slot, const HMODULE module, const FARPROC target,
                   const void *const caller) noexcept {
  SlotRuntime &runtime = slot_runtime(slot);
  runtime.resolves.fetch_add(1U, std::memory_order_relaxed);
  if (runtime.identity.load(std::memory_order_acquire) ==
      IdentityState::rejected) {
    return target;
  }
  const std::uintptr_t target_value =
      reinterpret_cast<std::uintptr_t>(target);
  std::uintptr_t expected = 0U;
  if (runtime.target.compare_exchange_strong(
          expected, target_value, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    runtime.module.store(reinterpret_cast<std::uintptr_t>(module),
                         std::memory_order_release);
    runtime.resolver_caller.store(reinterpret_cast<std::uintptr_t>(caller),
                                  std::memory_order_release);
    runtime.resolver_tick.store(GetTickCount64(), std::memory_order_release);
    runtime.identity.store(IdentityState::pending, std::memory_order_release);
    const auto ready_event = reinterpret_cast<HANDLE>(
        g_slot_ready_event.load(std::memory_order_acquire));
    if (ready_event != nullptr) {
      SetEvent(ready_event);
    }
    return wrapper_for_slot(slot);
  }
  if (expected == target_value) {
    return wrapper_for_slot(slot);
  }
  runtime.conflicts.fetch_add(1U, std::memory_order_relaxed);
  return target;
}

extern "C" __declspec(noinline) FARPROC WINAPI
get_proc_address_for_caller_detour(const HMODULE module, const LPCSTR name,
                                   void *const caller) {
  const DWORD incoming_last_error = GetLastError();
  g_active_resolver_detours.fetch_add(1U, std::memory_order_seq_cst);
  const bool observe = g_trace_enabled.load(std::memory_order_seq_cst);
  const auto original = reinterpret_cast<GetProcAddressForCallerFn>(
      g_original_resolver.load(std::memory_order_acquire));
  if (original == nullptr) {
    g_active_resolver_detours.fetch_sub(1U, std::memory_order_seq_cst);
    SetLastError(incoming_last_error);
    return nullptr;
  }
  SetLastError(incoming_last_error);
  const FARPROC target = original(module, name, caller);
  const DWORD outgoing_last_error = GetLastError();
  FARPROC returned = target;
  const TargetName target_name = classify_target_name(name);
  bool bridge_handled = false;
#if defined(MHW_DLSS_BRIDGE_PROBE)
  FARPROC bridge_target{};
  const BridgeRouteResult bridge_route = maybe_route_bridge_export(
      target_name, module, target, caller, &bridge_target);
  if (bridge_route == BridgeRouteResult::substituted) {
    returned = bridge_target;
    bridge_handled = true;
    if (observe) {
      publish_resolver_event(ResolverDisposition::substituted, target_name,
                             module, target, caller);
    }
  } else if (bridge_route == BridgeRouteResult::guarded_null) {
    returned = nullptr;
    bridge_handled = true;
    if (observe) {
      publish_resolver_event(ResolverDisposition::guarded_null, target_name,
                             module, target, caller);
    }
  } else if (bridge_route == BridgeRouteResult::rejected) {
    bridge_handled = true;
    if (observe) {
      publish_resolver_event(ResolverDisposition::rejected, target_name,
                             module, target, caller);
    }
  }
#endif
  if (!observe) {
    if (g_trace_state.load(std::memory_order_acquire) == 0) {
      g_resolver_before_ready.fetch_add(1U, std::memory_order_relaxed);
    }
  } else {
    g_resolver_calls.fetch_add(1U, std::memory_order_relaxed);
    if (bridge_handled) {
      // The exact bridge decision has already been recorded above.
    } else if (target_name == TargetName::unknown) {
      g_name_rejects.fetch_add(1U, std::memory_order_relaxed);
    } else if (target == nullptr) {
      g_target_nulls.fetch_add(1U, std::memory_order_relaxed);
      publish_resolver_event(ResolverDisposition::null_result, target_name,
                             module, target, caller);
    } else if (target_name == TargetName::d3d12_init) {
      g_d3d12_init_passthroughs.fetch_add(1U, std::memory_order_relaxed);
      if (caller_is_module_rva(caller, 0x6A0CB15EU, 0x00173000U,
                               0x4320CU)) {
        g_feature_d3d12_init_lookups.fetch_add(1U,
                                              std::memory_order_relaxed);
        const bool hook_ready =
            g_feature_status_hook_ready.load(std::memory_order_acquire) &&
            g_feature_status_hook_state.load(std::memory_order_acquire) == 2 &&
            slot_runtime(Slot::modern_d3d12_feature_status)
                    .identity.load(std::memory_order_acquire) ==
                IdentityState::verified;
        int no_feature_lookup{};
        if (g_hook_readiness_at_first_feature_init_lookup
                .compare_exchange_strong(
                    no_feature_lookup, hook_ready ? 2 : 1,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
          g_first_feature_d3d12_init_lookup_tick.store(
              GetTickCount64(), std::memory_order_release);
        }
      }
      publish_resolver_event(ResolverDisposition::passthrough, target_name,
                             module, target, caller);
    } else {
      Slot slot{};
      if (find_matching_slot(target_name, module, target, caller, &slot)) {
        returned = bind_slot(slot, module, target, caller);
      } else {
        g_target_rejects.fetch_add(1U, std::memory_order_relaxed);
        publish_resolver_event(ResolverDisposition::rejected, target_name,
                               module, target, caller);
      }
    }
  }

  g_active_resolver_detours.fetch_sub(1U, std::memory_order_seq_cst);
  SetLastError(outgoing_last_error);
  return returned;
}

bool mapped_identity_matches(const HMODULE module,
                             const std::uint32_t timestamp,
                             const std::uint32_t image_size) noexcept {
  PeIdentity identity{};
  return read_mapped_pe_identity(module, &identity) &&
         identity.machine == IMAGE_FILE_MACHINE_AMD64 &&
         identity.optional_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC &&
         identity.timestamp == timestamp && identity.image_size == image_size &&
         identity.image_base == reinterpret_cast<std::uintptr_t>(module);
}

bool kernel32_stub_targets_resolver(const FARPROC kernel32_get_proc_address,
                                    const FARPROC resolver) noexcept {
  if (kernel32_get_proc_address == nullptr || resolver == nullptr) {
    return false;
  }
  const auto *bytes =
      reinterpret_cast<const unsigned char *>(kernel32_get_proc_address);
  __try {
    constexpr std::array<unsigned char, 7> prefix{
        0x4C, 0x8B, 0x04, 0x24, 0x48, 0xFF, 0x25};
    if (std::memcmp(bytes, prefix.data(), prefix.size()) != 0) {
      return false;
    }
    std::int32_t displacement{};
    std::memcpy(&displacement, bytes + prefix.size(), sizeof(displacement));
    const auto next = reinterpret_cast<std::uintptr_t>(bytes + 11U);
    const auto slot_address = static_cast<std::uintptr_t>(
        static_cast<std::intptr_t>(next) + displacement);
    const auto *slot = reinterpret_cast<const void *const *>(slot_address);
    return *slot == reinterpret_cast<const void *>(resolver);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool resolve_and_verify_system_target(void **const output) {
  if (output == nullptr) {
    return false;
  }
  *output = nullptr;
  const HMODULE kernel_base = GetModuleHandleW(L"KernelBase.dll");
  const HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
  if (kernel_base == nullptr || kernel32 == nullptr) {
    write_log("trace.abort reason=system-resolver-modules-missing");
    return false;
  }
  const std::filesystem::path kernel_base_path = module_path(kernel_base);
  const std::filesystem::path kernel32_path = module_path(kernel32);
  if (kernel_base_path.empty() || kernel32_path.empty() ||
      !hash_matches(kernel_base_path, kExpectedKernelBaseHash) ||
      !hash_matches(kernel32_path, kExpectedKernel32Hash)) {
    write_log("trace.abort reason=system-resolver-hash-mismatch");
    return false;
  }
  if (!mapped_identity_matches(kernel_base, 0xEF4C9CABU, 0x003FF000U) ||
      !mapped_identity_matches(kernel32, 0xFB84BF00U, 0x000C9000U)) {
    write_log("trace.abort reason=system-resolver-pe-mismatch");
    return false;
  }

  const FARPROC resolver =
      GetProcAddress(kernel_base, "GetProcAddressForCaller");
  const FARPROC kernel32_get_proc_address =
      GetProcAddress(kernel32, "GetProcAddress");
  const auto kernel_base_address =
      reinterpret_cast<std::uintptr_t>(kernel_base);
  const auto kernel32_address = reinterpret_cast<std::uintptr_t>(kernel32);
  if (resolver == nullptr || kernel32_get_proc_address == nullptr ||
      reinterpret_cast<std::uintptr_t>(resolver) !=
          kernel_base_address + 0x0B30B0U ||
      reinterpret_cast<std::uintptr_t>(kernel32_get_proc_address) !=
          kernel32_address + 0x032060U ||
      !executable_image_address(kernel_base,
                                reinterpret_cast<const void *>(resolver)) ||
      !exact_bytes(reinterpret_cast<const void *>(resolver),
                   kKernelBaseResolverPrologue) ||
      !kernel32_stub_targets_resolver(kernel32_get_proc_address, resolver)) {
    write_log("trace.abort reason=system-resolver-control-flow-mismatch");
    return false;
  }
  *output = reinterpret_cast<void *>(resolver);
  write_log("trace.system-boundary verified=true kernelbase-sha256=" +
            sha256_text(kExpectedKernelBaseHash) + " kernel32-sha256=" +
            sha256_text(kExpectedKernel32Hash));
  return true;
}

bool verify_nvidia_adapter_inventory() {
  IDXGIFactory1 *factory{};
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || factory == nullptr) {
    write_log("gpu.enumeration result=factory-failed");
    return false;
  }
  std::uint32_t hardware_count{};
  std::uint32_t nvidia_count{};
  std::uint32_t rtx5090_count{};
  bool complete = true;
  for (UINT index = 0U;; ++index) {
    IDXGIAdapter1 *adapter{};
    const HRESULT status = factory->EnumAdapters1(index, &adapter);
    if (status == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (FAILED(status) || adapter == nullptr) {
      complete = false;
      if (adapter != nullptr) {
        adapter->Release();
      }
      break;
    }
    DXGI_ADAPTER_DESC1 description{};
    if (FAILED(adapter->GetDesc1(&description))) {
      complete = false;
      adapter->Release();
      break;
    }
    const bool software_or_remote =
        (description.Flags &
         (DXGI_ADAPTER_FLAG_SOFTWARE | DXGI_ADAPTER_FLAG_REMOTE)) != 0U;
    const std::wstring_view name(description.Description);
    write_log("gpu.adapter index=" + std::to_string(index) + " vendor=" +
              hexadecimal(description.VendorId) + " software-or-remote=" +
              (software_or_remote ? "true" : "false") + " name=" +
              utf8(name));
    if (!software_or_remote) {
      ++hardware_count;
      if (description.VendorId == 0x10DEU) {
        ++nvidia_count;
      }
      if (description.VendorId == 0x10DEU &&
          name == L"NVIDIA GeForce RTX 5090") {
        ++rtx5090_count;
      }
    }
    adapter->Release();
  }
  factory->Release();
  write_log("gpu.inventory hardware=" + std::to_string(hardware_count) +
            " nvidia-hardware=" + std::to_string(nvidia_count) +
            " allowlisted-rtx5090=" + std::to_string(rtx5090_count) +
            " complete=" + (complete ? "true" : "false"));
  return complete && hardware_count == 1U && nvidia_count == 1U &&
         rtx5090_count == 1U;
}

bool relevant_ngx_module_path(const std::filesystem::path &path) {
  const std::wstring filename = path.filename().wstring();
  const std::wstring full = path.wstring();
  return equal_path_text(filename, L"nvngx.dll") ||
         equal_path_text(filename, L"_nvngx.dll") ||
         equal_path_text(filename, L"nvngx_dlss.dll") ||
         contains_ascii_case_insensitive(full, L"\\NVIDIA\\NGX\\models\\");
}

bool ngx_module_already_loaded() noexcept {
  const HANDLE snapshot = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (snapshot == INVALID_HANDLE_VALUE) {
    return true;
  }
  MODULEENTRY32W entry{};
  entry.dwSize = static_cast<DWORD>(sizeof(entry));
  BOOL has_entry = Module32FirstW(snapshot, &entry);
  if (has_entry == FALSE) {
    CloseHandle(snapshot);
    return true;
  }
  std::uint32_t entries{};
  bool found = false;
  bool enumeration_failed = false;
  try {
    while (has_entry != FALSE && entries < kMaximumSnapshotEntries) {
      ++entries;
      if (relevant_ngx_module_path(std::filesystem::path(entry.szExePath))) {
        found = true;
        break;
      }
      SetLastError(ERROR_SUCCESS);
      has_entry = Module32NextW(snapshot, &entry);
      if (has_entry == FALSE && GetLastError() != ERROR_NO_MORE_FILES) {
        enumeration_failed = true;
      }
    }
  } catch (...) {
    found = true;
  }
  CloseHandle(snapshot);
  return found || enumeration_failed || has_entry != FALSE;
}

bool install_resolver_hook(void *const target) {
  const MH_STATUS initialize = MH_Initialize();
  if (initialize != MH_OK) {
    write_log("trace.abort reason=minhook-initialize status=" +
              std::to_string(static_cast<int>(initialize)));
    return false;
  }
  void *trampoline{};
  const MH_STATUS create = MH_CreateHook(
      target, reinterpret_cast<void *>(&get_proc_address_for_caller_detour),
      &trampoline);
  if (create != MH_OK) {
    write_log("trace.abort reason=resolver-hook-create status=" +
              std::to_string(static_cast<int>(create)));
    MH_Uninitialize();
    return false;
  }
  const MH_STATUS queue = MH_QueueEnableHook(target);
  if (queue != MH_OK) {
    write_log("trace.abort reason=resolver-hook-queue status=" +
              std::to_string(static_cast<int>(queue)));
    MH_RemoveHook(target);
    MH_Uninitialize();
    return false;
  }
  if (ngx_module_already_loaded()) {
    write_log("trace.abort reason=ngx-module-present-before-resolver-enable");
    MH_RemoveHook(target);
    MH_Uninitialize();
    return false;
  }
  g_original_resolver.store(reinterpret_cast<std::uintptr_t>(trampoline),
                            std::memory_order_release);
  g_trace_enabled.store(true, std::memory_order_seq_cst);
  const MH_STATUS apply = MH_ApplyQueued();
  if (apply != MH_OK) {
    g_trace_enabled.store(false, std::memory_order_seq_cst);
    const MH_STATUS disable = MH_DisableHook(target);
    const MH_STATUS remove = MH_RemoveHook(target);
    const MH_STATUS uninitialize = MH_Uninitialize();
    write_log("trace.abort reason=resolver-hook-apply status=" +
              std::to_string(static_cast<int>(apply)) + " cleanup-disable=" +
              std::to_string(static_cast<int>(disable)) + " cleanup-remove=" +
              std::to_string(static_cast<int>(remove)) +
              " cleanup-uninitialize=" +
              std::to_string(static_cast<int>(uninitialize)));
    return false;
  }
  write_log("trace.resolver-ready target=KernelBase!GetProcAddressForCaller "
            "pre-enable-ngx-present=false lifetime=process mutations=0");
  return true;
}

std::string version_text(const std::array<std::uint16_t, 4> &version) {
  return std::to_string(version[0]) + "." + std::to_string(version[1]) + "." +
         std::to_string(version[2]) + "." + std::to_string(version[3]);
}

bool expected_slot_path(const Slot slot, const std::filesystem::path &path,
                        const std::filesystem::path &host_directory,
                        const std::filesystem::path &cache_path) {
  const std::wstring full = path.wstring();
  switch (slot) {
  case Slot::local_populate:
    return equal_path_text(
        full, (host_directory / L"nvngx_dlss.dll").wstring());
  case Slot::cache_populate:
    return !cache_path.empty() &&
           equal_path_text(full, cache_path.wstring());
  case Slot::legacy_capability:
  case Slot::legacy_parameters:
    return equal_path_text(path.filename().wstring(), L"nvngx.dll") &&
           contains_ascii_case_insensitive(
               full, L"\\System32\\DriverStore\\FileRepository\\");
  case Slot::modern_capability:
  case Slot::modern_parameters:
  case Slot::modern_d3d12_feature_status:
    return equal_path_text(path.filename().wstring(), L"_nvngx.dll") &&
           contains_ascii_case_insensitive(
               full, L"\\System32\\DriverStore\\FileRepository\\");
  }
  return false;
}

bool verify_slot_identity(const Slot slot,
                          const std::filesystem::path &host_directory,
                          const std::filesystem::path &cache_path) {
  SlotRuntime &runtime = slot_runtime(slot);
  if (runtime.identity.load(std::memory_order_acquire) !=
      IdentityState::pending) {
    return true;
  }
  const auto target_value = runtime.target.load(std::memory_order_acquire);
  const auto module_value = runtime.module.load(std::memory_order_acquire);
  const auto target = reinterpret_cast<FARPROC>(target_value);
  const auto module = reinterpret_cast<HMODULE>(module_value);
  std::string rejection;
  std::filesystem::path path;
  std::array<unsigned char, 32> hash{};
  PeIdentity disk{};
  if (target == nullptr || module == nullptr) {
    rejection = "binding-incomplete";
  } else if (!mapped_slot_matches(slot, module, target)) {
    rejection = "mapped-identity-changed";
  } else {
    path = module_path(module);
    if (path.empty()) {
      rejection = "path-unavailable";
    } else if (!expected_slot_path(slot, path, host_directory, cache_path)) {
      rejection = "path-mismatch";
    } else if (!sha256_file(path, &hash)) {
      rejection = "hash-read-failed";
    } else if (hash != expected_hash(slot)) {
      rejection = "hash-mismatch";
    } else if (!read_disk_pe_identity(path, &disk) ||
               disk.machine != IMAGE_FILE_MACHINE_AMD64 ||
               disk.optional_magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
               disk.timestamp != expected_timestamp(slot) ||
               disk.image_base != kExpectedNvidiaPreferredImageBase ||
               disk.image_size != expected_image_size(slot)) {
      rejection = "disk-pe-mismatch";
    } else if (file_version(path) != expected_version(slot)) {
      rejection = "file-version-mismatch";
    }
  }

  if (rejection.empty()) {
    HMODULE pinned{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(target), &pinned) ||
        pinned != module) {
      rejection = "module-pin-failed";
    }
  }
  if (!rejection.empty()) {
    runtime.identity.store(IdentityState::rejected, std::memory_order_release);
    write_log("trace.identity slot=" + std::string(slot_name(slot)) +
              " state=rejected reason=" + rejection + " path=" +
              (path.empty() ? "unavailable" : utf8(path.wstring())) +
              " mutations=0");
    return false;
  }

  runtime.identity.store(IdentityState::verified, std::memory_order_release);
  write_log("trace.identity slot=" + std::string(slot_name(slot)) +
            " state=verified path=" + utf8(path.wstring()) + " sha256=" +
            sha256_text(hash) + " version=" +
            version_text(expected_version(slot)) + " target=" +
            hexadecimal(target_value) + " mutations=0");
  return true;
}

void mark_feature_status_hook_unavailable(const std::string &reason,
                                          const MH_STATUS status) {
  g_feature_status_hook_state.store(-1, std::memory_order_release);
  write_log("trace.boundary-unavailable slot=" +
            std::string(slot_name(Slot::modern_d3d12_feature_status)) +
            " reason=" + reason + " status=" +
            std::to_string(static_cast<int>(status)) +
            " init-replayed=false mutations=0");
}

void try_install_feature_status_hook(
    const std::filesystem::path &host_directory,
    const std::filesystem::path &cache_path) {
  if (!g_trace_enabled.load(std::memory_order_seq_cst)) {
    return;
  }
  const int current_state =
      g_feature_status_hook_state.load(std::memory_order_acquire);
  if (current_state != 0) {
    if (current_state == 2) {
      const std::uintptr_t installed_module =
          slot_runtime(Slot::modern_d3d12_feature_status)
              .module.load(std::memory_order_acquire);
      for (const Slot slot : {Slot::modern_capability,
                              Slot::modern_parameters}) {
        const SlotRuntime &candidate = slot_runtime(slot);
        if (candidate.identity.load(std::memory_order_acquire) ==
                IdentityState::verified &&
            candidate.module.load(std::memory_order_acquire) !=
                installed_module) {
          int enabled = 2;
          if (g_feature_status_hook_state.compare_exchange_strong(
                  enabled, -1, std::memory_order_acq_rel,
                  std::memory_order_acquire)) {
            write_log(
                "trace.boundary-unavailable slot=" +
                std::string(
                    slot_name(Slot::modern_d3d12_feature_status)) +
                " reason=verified-modern-module-conflict "
                "hook-retained=true trampoline-retained=true telemetry=false "
                "init-replayed=false mutations=0");
          }
          break;
        }
      }
    }
    return;
  }

  std::uintptr_t modern_module{};
  for (const Slot slot : {Slot::modern_capability, Slot::modern_parameters}) {
    const SlotRuntime &runtime = slot_runtime(slot);
    if (runtime.identity.load(std::memory_order_acquire) !=
        IdentityState::verified) {
      continue;
    }
    const std::uintptr_t candidate =
        runtime.module.load(std::memory_order_acquire);
    if (candidate == 0U) {
      continue;
    }
    if (modern_module != 0U && modern_module != candidate) {
      int expected = 0;
      if (g_feature_status_hook_state.compare_exchange_strong(
              expected, -1, std::memory_order_acq_rel,
              std::memory_order_acquire)) {
        write_log("trace.boundary-unavailable slot=" +
                  std::string(
                      slot_name(Slot::modern_d3d12_feature_status)) +
                  " reason=verified-modern-module-conflict status=unavailable "
                  "init-replayed=false mutations=0");
      }
      return;
    }
    modern_module = candidate;
  }
  if (modern_module == 0U) {
    return;
  }

  int expected = 0;
  if (!g_feature_status_hook_state.compare_exchange_strong(
          expected, 1, std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return;
  }

  SlotRuntime &runtime = slot_runtime(Slot::modern_d3d12_feature_status);
  const std::uintptr_t target =
      modern_module + expected_rva(Slot::modern_d3d12_feature_status);
  runtime.target.store(target, std::memory_order_release);
  runtime.module.store(modern_module, std::memory_order_release);
  runtime.resolver_caller.store(modern_module + 0x67704U,
                                std::memory_order_release);
  runtime.resolver_tick.store(GetTickCount64(), std::memory_order_release);
  runtime.resolves.store(1U, std::memory_order_release);
  runtime.identity.store(IdentityState::pending, std::memory_order_release);

  try {
    if (!verify_slot_identity(Slot::modern_d3d12_feature_status,
                              host_directory, cache_path)) {
      mark_feature_status_hook_unavailable("identity-rejected", MH_UNKNOWN);
      return;
    }
  } catch (...) {
    runtime.identity.store(IdentityState::rejected, std::memory_order_release);
    mark_feature_status_hook_unavailable("identity-verification-exception",
                                         MH_UNKNOWN);
    return;
  }

  void *trampoline{};
  const MH_STATUS create = MH_CreateHook(
      reinterpret_cast<void *>(target),
      reinterpret_cast<void *>(&modern_d3d12_feature_status_detour),
      &trampoline);
  if (create != MH_OK) {
    mark_feature_status_hook_unavailable("hook-create", create);
    return;
  }
  if (trampoline == nullptr) {
    const MH_STATUS remove = MH_RemoveHook(reinterpret_cast<void *>(target));
    mark_feature_status_hook_unavailable(
        "hook-create-null-trampoline-remove-" +
            std::to_string(static_cast<int>(remove)),
        MH_UNKNOWN);
    return;
  }
  g_original_feature_status_recorder.store(
      reinterpret_cast<std::uintptr_t>(trampoline), std::memory_order_release);

  const MH_STATUS queue =
      MH_QueueEnableHook(reinterpret_cast<void *>(target));
  if (queue != MH_OK) {
    g_original_feature_status_recorder.store(0U, std::memory_order_release);
    const MH_STATUS remove = MH_RemoveHook(reinterpret_cast<void *>(target));
    mark_feature_status_hook_unavailable(
        "hook-queue-remove-" + std::to_string(static_cast<int>(remove)), queue);
    return;
  }

  const ULONGLONG attempt_tick = GetTickCount64();
  // Publish enabled before applying: once the detour can execute, its original
  // trampoline and strict observation gate are already fully initialized.
  g_feature_status_hook_state.store(2, std::memory_order_release);
  const MH_STATUS apply = MH_ApplyQueued();
  if (apply != MH_OK) {
    g_feature_status_hook_state.store(-1, std::memory_order_release);
    write_log(
        "trace.boundary-unavailable slot=" +
        std::string(slot_name(Slot::modern_d3d12_feature_status)) +
        " reason=hook-apply status=" +
        std::to_string(static_cast<int>(apply)) +
        " hook-retained=true trampoline-retained=true telemetry=false "
        "init-replayed=false mutations=0");
    return;
  }

  const ULONGLONG ready_tick = GetTickCount64();
  g_feature_status_hook_tick.store(ready_tick, std::memory_order_relaxed);
  g_feature_status_hook_ready.store(true, std::memory_order_release);

  write_log(
      "trace.boundary-ready slot=" +
      std::string(slot_name(Slot::modern_d3d12_feature_status)) +
      " target=_nvngx.dll+0x264E0 expected-caller=_nvngx.dll+0x67704 "
      "feature-index=1 api=2 apply-attempt-tick=" +
      std::to_string(attempt_tick) + " ready-tick=" +
      std::to_string(ready_tick) +
      " init-proxied=false init-replayed=false mutations=0");
}

void try_install_feature_status_hook_guarded(
    const std::filesystem::path &host_directory,
    const std::filesystem::path &cache_path) noexcept {
  try {
    try_install_feature_status_hook(host_directory, cache_path);
  } catch (...) {
    g_feature_status_hook_state.store(-1, std::memory_order_release);
    try {
      write_log(
          "trace.boundary-unavailable slot=" +
          std::string(slot_name(Slot::modern_d3d12_feature_status)) +
          " reason=unexpected-install-exception hook-retained=true "
          "telemetry=false init-replayed=false mutations=0");
    } catch (...) {
    }
  }
}

std::string address_origin(const std::uintptr_t address) {
  if (address == 0U) {
    return "unavailable";
  }
  MEMORY_BASIC_INFORMATION information{};
  if (VirtualQuery(reinterpret_cast<const void *>(address), &information,
                   sizeof(information)) != sizeof(information) ||
      information.Type != MEM_IMAGE || information.AllocationBase == nullptr) {
    return "unknown@" + hexadecimal(address);
  }
  const auto module = static_cast<HMODULE>(information.AllocationBase);
  const std::filesystem::path path = module_path(module);
  const auto base = reinterpret_cast<std::uintptr_t>(module);
  return (path.empty() ? "unknown" : utf8(path.filename().wstring())) + "+" +
         hexadecimal(address - base);
}

#if defined(MHW_DLSS_BRIDGE_PROBE)
const char *boolean_text(const bool value) noexcept {
  return value ? "true" : "false";
}

void write_bridge_capture_summary(const std::uint64_t sequence,
                                  const char *const phase,
                                  const bridge::CaptureSummary &summary) {
  write_log("bridge.contract-summary sequence=" + std::to_string(sequence) +
            " phase=" + phase + " queries=" +
            std::to_string(summary.queries_attempted) + " complete=" +
            boolean_text(summary.complete) + " fault-latched=" +
            boolean_text(summary.fault_latched) + " stop-reason=" +
            capture_stop_reason_text(summary.stop_reason));
}

template <typename Capture>
void write_bridge_scalar_capture(const std::uint64_t sequence,
                                 const char *const phase,
                                 const char *const kind,
                                 const std::string_view key,
                                 const Capture &capture,
                                 const std::string &value) {
  write_log("bridge.contract sequence=" + std::to_string(sequence) +
            " phase=" + phase + " kind=" + kind + " key=" +
            std::string(key) + " attempted=" +
            boolean_text(capture.query_attempted) +
            " callback-completed=" +
            boolean_text(capture.callback_completed) + " present=" +
            boolean_text(capture.present) + " getter-result=" +
            hexadecimal(capture.getter_result) + " value=" + value);
}

void drain_bridge_events() {
  for (BridgeEvent &event : g_bridge_events) {
    if (event.state.load(std::memory_order_acquire) != EventState::ready) {
      continue;
    }
    const BridgeEventPayload payload = event.payload;
    const std::string_view export_view =
        bridge::export_name(payload.export_id);
    const std::string init_validation =
        payload.export_id == bridge::ExportId::d3d12_init
            ? " init-validation-mask=" +
                  hexadecimal(payload.init_validation_mask) +
                  " init-validation-complete=" +
                  boolean_text(payload.init_validation_mask ==
                               bridge::kInitValidationMaskComplete) +
                  " init-caller-verified=" +
                  boolean_text((payload.init_validation_mask &
                                bridge::kInitGateCallerVerified) != 0U) +
                  " init-lifecycle-ready=" +
                  boolean_text((payload.init_validation_mask &
                                bridge::kInitGateLifecycleReady) != 0U) +
                  " init-application-id-valid=" +
                  boolean_text((payload.init_validation_mask &
                                bridge::kInitGateApplicationId) != 0U) +
                  " init-application-path-readable=" +
                  boolean_text((payload.init_validation_mask &
                                bridge::kInitGateApplicationPath) != 0U) +
                  " init-api-version-valid=" +
                  boolean_text((payload.init_validation_mask &
                                bridge::kInitGateApiVersion) != 0U) +
                  " init-route-table-complete=" +
                  boolean_text((payload.init_validation_mask &
                                bridge::kInitGateRouteTable) != 0U) +
                  " init-device-verified=" +
                  boolean_text((payload.init_validation_mask &
                                bridge::kInitGateD3d12Device) != 0U)
            : std::string{};
    write_log(
        "bridge.call sequence=" + std::to_string(payload.sequence) +
        " export=" +
        (export_view.empty() ? std::string("unknown")
                             : std::string(export_view)) +
        " hit-tick=" + std::to_string(payload.tick) + " hit-tid=" +
        std::to_string(payload.thread_id) + " caller=" +
        address_origin(payload.caller) + " caller-address=" +
        hexadecimal(payload.caller) + " before=" +
        bridge_state_text(payload.before) + " after=" +
        bridge_state_text(payload.after) + " decision=" +
        (payload.observation_only
             ? std::string("observation-only")
             : std::string(bridge_decision_text(payload.decision))) +
        " response=" +
        bridge_response_text(payload.response) + " result=" +
        hexadecimal(payload.result) + " result-name=" +
        ngx_result_name(payload.result) + " token=" +
        hexadecimal(payload.token) + " argument0=" +
        hexadecimal(payload.argument0) + " argument1=" +
        hexadecimal(payload.argument1) + " capture-committed=" +
        boolean_text(payload.capture_committed) +
        " d3d12-object-verified=" +
        boolean_text(payload.d3d12_object_verified) +
        " lifecycle-observation-only=" +
        boolean_text(payload.observation_only) + init_validation);

    if (payload.has_create_snapshot) {
      for (const auto &capture : payload.create_snapshot.unsigned_values) {
        write_bridge_scalar_capture(
            payload.sequence, "create", "uint", bridge::key_name(capture.key),
            capture, std::to_string(capture.value));
      }
      for (const auto &capture : payload.create_snapshot.signed_values) {
        write_bridge_scalar_capture(
            payload.sequence, "create", "int", bridge::key_name(capture.key),
            capture, std::to_string(capture.value));
      }
      write_bridge_capture_summary(payload.sequence, "create",
                                   payload.create_snapshot.summary);
    }
    if (payload.has_evaluate_snapshot) {
      for (const auto &capture : payload.evaluate_snapshot.resources) {
        write_log(
            "bridge.contract sequence=" + std::to_string(payload.sequence) +
            " phase=evaluate kind=resource key=" +
            std::string(bridge::key_name(capture.key)) + " attempted=" +
            boolean_text(capture.query_attempted) +
            " callback-completed=" +
            boolean_text(capture.callback_completed) + " present=" +
            boolean_text(capture.present) + " getter-result=" +
            hexadecimal(capture.getter_result) + " value=" +
            hexadecimal(capture.value) + " value-is-null=" +
            boolean_text(capture.value_is_null));
      }
      for (const auto &capture : payload.evaluate_snapshot.floats) {
        write_bridge_scalar_capture(
            payload.sequence, "evaluate", "float",
            bridge::key_name(capture.key), capture,
            std::to_string(capture.value));
      }
      for (const auto &capture : payload.evaluate_snapshot.signed_values) {
        write_bridge_scalar_capture(
            payload.sequence, "evaluate", "int",
            bridge::key_name(capture.key), capture,
            std::to_string(capture.value));
      }
      write_bridge_capture_summary(payload.sequence, "evaluate",
                                   payload.evaluate_snapshot.summary);
    }
    event.state.store(EventState::logged, std::memory_order_release);
  }
}
#endif

void drain_slot_events(const Slot slot) {
  SlotRuntime &runtime = slot_runtime(slot);
  for (TraceEvent &event : runtime.events) {
    if (event.state.load(std::memory_order_acquire) != EventState::ready) {
      continue;
    }
    const TraceEventPayload payload = event.payload;
    const std::string prefix =
        "trace.call sequence=" + std::to_string(payload.sequence) +
        " slot=" + slot_name(slot) + " identity=" +
        identity_name(runtime.identity.load(std::memory_order_acquire)) +
        " hit-tick=" + std::to_string(payload.tick) + " hit-tid=" +
        std::to_string(payload.thread_id) + " caller=" +
        address_origin(payload.caller) + " caller-address=" +
        hexadecimal(payload.caller);
    if (is_feature_status_slot(slot)) {
      write_log(prefix + " feature-index=" +
                std::to_string(payload.feature_index) + " api=" +
                std::to_string(payload.api) + " context-nonnull=" +
                (payload.argument_nonnull ? "true" : "false") +
                " status=" + hexadecimal(payload.result) + " status-name=" +
                ngx_result_name(payload.result) + " failed=" +
                (((payload.result & 0xFFF00000U) == 0xBAD00000U) ? "true"
                                                                : "false") +
                " duration-ms=" +
                std::to_string(payload.duration_ticks) + " mutated=false");
    } else {
      write_log(prefix + " argument-nonnull=" +
                (payload.argument_nonnull ? "true" : "false") +
                " argument=" + hexadecimal(payload.argument) + " result=" +
                hexadecimal(payload.result) + " duration-ms=" +
                std::to_string(payload.duration_ticks) + " mutated=false");
    }
    event.state.store(EventState::logged, std::memory_order_release);
  }
}

void drain_resolver_events() {
  for (ResolverEvent &event : g_resolver_events) {
    if (event.state.load(std::memory_order_acquire) != EventState::ready) {
      continue;
    }
    const ResolverEventPayload payload = event.payload;
    std::filesystem::path path;
    MEMORY_BASIC_INFORMATION information{};
    if (payload.module != 0U &&
        VirtualQuery(reinterpret_cast<const void *>(payload.module),
                     &information, sizeof(information)) == sizeof(information) &&
        information.Type == MEM_IMAGE &&
        reinterpret_cast<std::uintptr_t>(information.AllocationBase) ==
            payload.module) {
      path = module_path(reinterpret_cast<HMODULE>(payload.module));
    }
    write_log(
        "trace.resolver-event sequence=" + std::to_string(payload.sequence) +
        " disposition=" + resolver_disposition_text(payload.disposition) +
        " name=" + target_name_text(payload.name) + " hit-tick=" +
        std::to_string(payload.tick) + " hit-tid=" +
        std::to_string(payload.thread_id) + " module=" +
        hexadecimal(payload.module) + " module-file=" +
        (path.empty() ? "unavailable" : utf8(path.filename().wstring())) +
        " mapped-timestamp=" +
        (payload.mapped_identity_available
             ? hexadecimal(payload.mapped_timestamp)
             : "unavailable") +
        " mapped-size-of-image=" +
        (payload.mapped_identity_available
             ? hexadecimal(payload.mapped_image_size)
             : "unavailable") +
        " target=" + hexadecimal(payload.target) + " target-rva=" +
        (payload.target_rva_valid ? hexadecimal(payload.target_rva)
                                  : "unavailable") +
        " caller=" + address_origin(payload.caller) + " caller-address=" +
        hexadecimal(payload.caller) + " returned-original=" +
        ((payload.disposition == ResolverDisposition::substituted ||
          (payload.disposition == ResolverDisposition::guarded_null &&
           payload.target != 0U))
             ? "false"
             : "true") +
        " mutations=" +
        ((payload.disposition == ResolverDisposition::substituted ||
          (payload.disposition == ResolverDisposition::guarded_null &&
           payload.target != 0U))
             ? "1"
             : "0"));
    event.state.store(EventState::logged, std::memory_order_release);
  }
}

struct SeenModule {
  std::uintptr_t base{};
  std::wstring path;
};

struct CensusState {
  std::vector<SeenModule> seen;
  bool failure_logged{};
  bool truncated_logged{};
};

bool census_seen(const CensusState &state, const std::uintptr_t base,
                 const std::wstring_view path) noexcept {
  for (const SeenModule &seen : state.seen) {
    if (seen.base == base && equal_path_text(seen.path, path)) {
      return true;
    }
  }
  return false;
}

void census_modules(CensusState &state) {
  const HANDLE snapshot = CreateToolhelp32Snapshot(
      TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
  if (snapshot == INVALID_HANDLE_VALUE) {
    if (!state.failure_logged) {
      write_log("trace.census result=snapshot-failed error=" +
                std::to_string(GetLastError()));
      state.failure_logged = true;
    }
    return;
  }

  MODULEENTRY32W entry{};
  entry.dwSize = static_cast<DWORD>(sizeof(entry));
  BOOL has_entry = Module32FirstW(snapshot, &entry);
  std::uint32_t entries{};
  while (has_entry != FALSE && entries < kMaximumSnapshotEntries) {
    ++entries;
    const std::filesystem::path snapshot_path(entry.szExePath);
    if (relevant_ngx_module_path(snapshot_path)) {
      const auto base = reinterpret_cast<std::uintptr_t>(entry.hModule);
      const std::wstring full = snapshot_path.wstring();
      if (!census_seen(state, base, full) &&
          state.seen.size() < kMaximumCensusModules) {
        state.seen.push_back({base, full});
        PeIdentity mapped{};
        std::array<unsigned char, 32> hash{};
        const bool has_pe = read_mapped_pe_identity(entry.hModule, &mapped);
        const bool has_hash = sha256_file(snapshot_path, &hash);
        write_log("trace.module path=" + utf8(full) + " base=" +
                  hexadecimal(base) + " timestamp=" +
                  (has_pe ? hexadecimal(mapped.timestamp) : "unavailable") +
                  " size-of-image=" +
                  (has_pe ? hexadecimal(mapped.image_size) : "unavailable") +
                  " sha256=" +
                  (has_hash ? sha256_text(hash) : "unavailable") +
                  " version=" + version_text(file_version(snapshot_path)));
      } else if (state.seen.size() >= kMaximumCensusModules &&
                 !state.truncated_logged) {
        write_log("trace.census truncated=true reason=module-limit");
        state.truncated_logged = true;
      }
    }
    has_entry = Module32NextW(snapshot, &entry);
  }
  if (has_entry != FALSE && !state.truncated_logged) {
    write_log("trace.census truncated=true reason=snapshot-entry-limit");
    state.truncated_logged = true;
  }
  CloseHandle(snapshot);
}

bool prepare_host_boundary(std::filesystem::path *const host_path) {
  if (host_path == nullptr) {
    return false;
  }
  const HMODULE host = GetModuleHandleW(nullptr);
  const std::filesystem::path path = module_path(host);
  PeIdentity mapped{};
  if (host == nullptr || path.empty() ||
      !equal_path_text(path.filename().wstring(), L"MonsterHunterWorld.exe") ||
      !read_mapped_pe_identity(host, &mapped) ||
      mapped.machine != IMAGE_FILE_MACHINE_AMD64 ||
      mapped.optional_magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      mapped.timestamp != kExpectedHostTimestamp ||
      mapped.image_base != reinterpret_cast<std::uintptr_t>(host) ||
      mapped.image_size != kExpectedHostImageSize) {
    write_log("trace.abort reason=host-mapped-identity-mismatch path=" +
              (path.empty() ? "unavailable" : utf8(path.wstring())));
    return false;
  }

  g_host_base.store(reinterpret_cast<std::uintptr_t>(host),
                    std::memory_order_release);
  g_host_image_size.store(mapped.image_size, std::memory_order_release);
  *host_path = path;
  write_log("trace.host-boundary mapped=true path=" + utf8(path.wstring()) +
            " actual-base=" +
            hexadecimal(reinterpret_cast<std::uintptr_t>(host)) +
            " mapped-image-base=" + hexadecimal(mapped.image_base) +
            " timestamp=" + hexadecimal(mapped.timestamp) +
            " size-of-image=" + hexadecimal(mapped.image_size));
  return true;
}

bool verify_host_disk_identity(const std::filesystem::path &host_path) {
  PeIdentity disk{};
  std::array<unsigned char, 32> hash{};
  if (!read_disk_pe_identity(host_path, &disk) ||
      disk.machine != IMAGE_FILE_MACHINE_AMD64 ||
      disk.optional_magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
      disk.timestamp != kExpectedHostTimestamp ||
      disk.image_base != kExpectedHostPreferredImageBase ||
      disk.image_size != kExpectedHostImageSize ||
      !sha256_file(host_path, &hash) || hash != kExpectedHostHash) {
    write_log("trace.abort reason=host-disk-identity-mismatch path=" +
              utf8(host_path.wstring()));
    return false;
  }
  write_log("trace.host-identity verified=true sha256=" + sha256_text(hash) +
            " mutations=0");
  return true;
}

void verify_pending_slot_identities(
    const std::filesystem::path &host_directory,
    const std::filesystem::path &cache_path) noexcept {
  for (const Slot slot : kIdentityVerificationOrder) {
    if (slot_runtime(slot).identity.load(std::memory_order_acquire) !=
        IdentityState::pending) {
      continue;
    }
    try {
      verify_slot_identity(slot, host_directory, cache_path);
    } catch (...) {
      slot_runtime(slot).identity.store(IdentityState::rejected,
                                        std::memory_order_release);
      write_log("trace.identity slot=" + std::string(slot_name(slot)) +
                " state=rejected reason=unexpected-verification-exception "
                "mutations=0");
    }
    if ((slot == Slot::modern_capability ||
         slot == Slot::modern_parameters) &&
        slot_runtime(slot).identity.load(std::memory_order_acquire) ==
            IdentityState::verified) {
      try_install_feature_status_hook_guarded(host_directory, cache_path);
    }
  }
}

void drain_all_slot_events() {
  for (const Slot slot : kAllSlots) {
    drain_slot_events(slot);
  }
}

#if defined(MHW_DLSS_BRIDGE_PROBE)
bool bridge_capture_goal_reached() noexcept {
  const bridge::Mode mode =
      g_bridge_mode.load(std::memory_order_acquire);
  const bridge::LifecycleState state = bridge_lifecycle_state();
  if (g_bridge_init_succeeded.load(std::memory_order_acquire) ||
      g_bridge_token_issued.load(std::memory_order_acquire)) {
    return false;
  }
  if (mode == bridge::Mode::capture_create) {
    return state == bridge::LifecycleState::create_captured ||
           state == bridge::LifecycleState::inert;
  }
  if (mode == bridge::Mode::capture_one_evaluate) {
    return state == bridge::LifecycleState::released ||
           state == bridge::LifecycleState::inert;
  }
  return false;
}
#endif

void log_slot_summary(const Slot slot) {
  const SlotRuntime &runtime = g_slots[slot_index(slot)];
  const auto target = runtime.target.load(std::memory_order_acquire);
  const auto module = runtime.module.load(std::memory_order_acquire);
  const auto caller = runtime.resolver_caller.load(std::memory_order_acquire);
  const std::string binding =
      is_feature_status_slot(slot)
          ? " boundary-installs=" +
                std::to_string(
                    runtime.resolves.load(std::memory_order_relaxed)) +
                " first-boundary-bind-tick=" +
                std::to_string(
                    runtime.resolver_tick.load(std::memory_order_relaxed)) +
                " expected-caller=" + address_origin(caller)
          : " resolves=" +
                std::to_string(
                    runtime.resolves.load(std::memory_order_relaxed)) +
                " first-resolve-tick=" +
                std::to_string(
                    runtime.resolver_tick.load(std::memory_order_relaxed)) +
                " resolver-caller=" + address_origin(caller);
  write_log(
      "trace.summary slot=" + std::string(slot_name(slot)) + " identity=" +
      identity_name(runtime.identity.load(std::memory_order_acquire)) +
      " module=" + hexadecimal(module) + " target=" + hexadecimal(target) +
      binding + " calls=" +
      std::to_string(runtime.calls.load(std::memory_order_relaxed)) +
      " success=" +
      std::to_string(runtime.success.load(std::memory_order_relaxed)) +
      " bad00000=" +
      std::to_string(runtime.bad00000.load(std::memory_order_relaxed)) +
      " bad00001=" +
      std::to_string(runtime.bad00001.load(std::memory_order_relaxed)) +
      " bad00002=" +
      std::to_string(runtime.bad00002.load(std::memory_order_relaxed)) +
      " other=" +
      std::to_string(runtime.other.load(std::memory_order_relaxed)) +
      " recursion=" +
      std::to_string(runtime.recursion.load(std::memory_order_relaxed)) +
      " conflicts=" +
      std::to_string(runtime.conflicts.load(std::memory_order_relaxed)) +
      " dropped-events=" +
      std::to_string(runtime.dropped.load(std::memory_order_relaxed)) +
      " mutated=false");
}

bool close_trace_phase() noexcept {
  g_trace_enabled.store(false, std::memory_order_seq_cst);
  const ULONGLONG start = GetTickCount64();
  while (g_active_resolver_detours.load(std::memory_order_seq_cst) != 0U ||
         g_active_wrapper_calls.load(std::memory_order_seq_cst) != 0U) {
    if (GetTickCount64() - start >= kQuiesceTimeoutMilliseconds) {
      return false;
    }
    Sleep(1U);
  }
  return true;
}

void run_bounded_trace(const std::filesystem::path &host_directory,
                       const std::filesystem::path &cache_path) {
  CensusState census{};
  census.seen.reserve(kMaximumCensusModules);
  const ULONGLONG start = GetTickCount64();
  ULONGLONG next_census = start;
  DWORD trace_duration = kTraceDurationMilliseconds;
#if defined(MHW_DLSS_BRIDGE_PROBE)
  if (g_bridge_mode.load(std::memory_order_acquire) !=
      bridge::Mode::disabled) {
    trace_duration = kBridgeTraceDurationMilliseconds;
  }
#endif
  write_log("trace.window start=true duration-ms=" +
            std::to_string(trace_duration));

  for (;;) {
    verify_pending_slot_identities(host_directory, cache_path);
    try_install_feature_status_hook_guarded(host_directory, cache_path);
    drain_all_slot_events();
    drain_resolver_events();
#if defined(MHW_DLSS_BRIDGE_PROBE)
    drain_bridge_events();
#endif
    const ULONGLONG now = GetTickCount64();
    if (now >= next_census) {
      try {
        census_modules(census);
      } catch (...) {
        if (!census.failure_logged) {
          write_log("trace.census result=unexpected-exception");
          census.failure_logged = true;
        }
      }
      next_census = now + kCensusIntervalMilliseconds;
    }
    const ULONGLONG elapsed = now - start;
    bool capture_goal_reached = false;
#if defined(MHW_DLSS_BRIDGE_PROBE)
    capture_goal_reached = bridge_capture_goal_reached();
#endif
    if (elapsed >= trace_duration || capture_goal_reached) {
      break;
    }
    const ULONGLONG remaining = trace_duration - elapsed;
    const DWORD wait_milliseconds =
        remaining < kTracePollMilliseconds
            ? static_cast<DWORD>(remaining)
            : kTracePollMilliseconds;
    const auto ready_event = reinterpret_cast<HANDLE>(
        g_slot_ready_event.load(std::memory_order_acquire));
    if (ready_event == nullptr ||
        WaitForSingleObject(ready_event, wait_milliseconds) == WAIT_FAILED) {
      Sleep(wait_milliseconds);
    }
  }

  const bool quiesced = close_trace_phase();
#if defined(MHW_DLSS_BRIDGE_PROBE)
  write_log("trace.window closed=true observer-bindings-enabled=false "
             "provider-routes-retained=" +
            std::string(
                g_bridge_interception_started.load(std::memory_order_acquire)
                            ? "true"
                            : "false") +
            " quiesced=" + std::string(quiesced ? "true" : "false") +
            " hook-retained=true");
#else
  write_log("trace.window closed=true bindings-enabled=false quiesced=" +
            std::string(quiesced ? "true" : "false") +
            " hook-retained=true");
#endif
  verify_pending_slot_identities(host_directory, cache_path);
  drain_all_slot_events();
  drain_resolver_events();
#if defined(MHW_DLSS_BRIDGE_PROBE)
  drain_bridge_events();
#endif
  try {
    census_modules(census);
  } catch (...) {
    if (!census.failure_logged) {
      write_log("trace.census result=unexpected-final-exception");
    }
  }
  for (const Slot slot : kAllSlots) {
    log_slot_summary(slot);
  }

  const std::uint64_t before_ready =
      g_resolver_before_ready.load(std::memory_order_relaxed);
  const std::uint64_t feature_init_tick =
      g_first_feature_d3d12_init_lookup_tick.load(std::memory_order_relaxed);
  const std::uint64_t status_hook_tick =
      g_feature_status_hook_tick.load(std::memory_order_relaxed);
  const int status_hook_state =
      g_feature_status_hook_state.load(std::memory_order_acquire);
  const int first_lookup_readiness =
      g_hook_readiness_at_first_feature_init_lookup.load(
          std::memory_order_acquire);
  const char *status_coverage = "inconclusive";
  if (slot_runtime(Slot::modern_d3d12_feature_status)
          .calls.load(std::memory_order_relaxed) != 0U) {
    status_coverage = "target-status-observed";
  } else if (status_hook_state == 2 && first_lookup_readiness == 0) {
    status_coverage = "armed-no-feature-init-lookup";
  } else if (status_hook_state == 2 && first_lookup_readiness == 2) {
    status_coverage = "armed-before-feature-init-lookup";
  } else if (status_hook_state == 2 && first_lookup_readiness == 1) {
    status_coverage =
        "armed-after-first-init-lookup-call-coverage-inconclusive";
  }
#if defined(MHW_DLSS_BRIDGE_PROBE)
  const BridgeLifecycleDiagnostics lifecycle_diagnostics =
      bridge_lifecycle_diagnostics();
  const int final_route_state =
      g_bridge_route_state.load(std::memory_order_acquire);
  const std::uint32_t final_lookup_mask =
      g_bridge_lookup_seen_mask.load(std::memory_order_acquire);
  const std::string startup_coverage =
      final_route_state == -2
          ? "inconclusive-init-lookup-missed"
          : (bridge_route_table_complete()
                 ? "complete-init-first-14-lookup-transaction"
                 : "inconclusive-no-or-partial-transaction");
  const std::string mutation_summary =
      g_bridge_mode.load(std::memory_order_acquire) == bridge::Mode::disabled
          ? "0"
          : std::to_string(
                g_bridge_routes.load(std::memory_order_relaxed) +
                g_bridge_guard_suppressions.load(
                    std::memory_order_relaxed));
  write_log(
      "bridge.summary mode=" +
      std::string(bridge_mode_text(
          g_bridge_mode.load(std::memory_order_acquire))) +
       " route-state=" +
       std::to_string(final_route_state) +
       " interception-started=" +
       boolean_text(
           g_bridge_interception_started.load(std::memory_order_acquire)) +
       " route-mask=" +
       hexadecimal(g_bridge_route_mask.load(std::memory_order_acquire)) +
       " lookup-seen-mask=" + hexadecimal(final_lookup_mask) +
       " guard-seen-mask=" +
       hexadecimal(final_lookup_mask & bridge::kProviderGuardMaskComplete) +
       " callback-seen-mask=" +
       hexadecimal(final_lookup_mask &
                   bridge::kProviderCallbackMaskComplete) +
       " route-table-complete=" +
      boolean_text(bridge_route_table_complete()) + " routes=" +
      std::to_string(g_bridge_routes.load(std::memory_order_relaxed)) +
      " route-rejects=" +
      std::to_string(g_bridge_route_rejects.load(std::memory_order_relaxed)) +
      " guard-lookups=" +
      std::to_string(g_bridge_guard_lookups.load(std::memory_order_relaxed)) +
      " guard-rejects=" +
      std::to_string(g_bridge_guard_rejects.load(std::memory_order_relaxed)) +
      " guard-suppressions=" +
      std::to_string(
          g_bridge_guard_suppressions.load(std::memory_order_relaxed)) +
      " callback-routes=" +
      std::to_string(
          g_bridge_callback_routes.load(std::memory_order_relaxed)) +
      " lifecycle-state=" + bridge_state_text(bridge_lifecycle_state()) +
      " scratch-calls=" +
      std::to_string(g_bridge_scratch_calls.load(std::memory_order_relaxed)) +
      " first-scratch-caller=" +
      address_origin(
          g_bridge_first_scratch_caller.load(std::memory_order_acquire)) +
      " scratch-result=" +
      hexadecimal(g_bridge_last_scratch_result.load(
          std::memory_order_acquire)) +
      " bridge-events=" +
      std::to_string(g_bridge_events_reserved.load(std::memory_order_relaxed)) +
       " bridge-events-dropped=" +
       std::to_string(g_bridge_events_dropped.load(std::memory_order_relaxed)) +
       " boundary-contentions=" +
       std::to_string(
           g_bridge_boundary_contentions.load(std::memory_order_relaxed)) +
       " lifecycle-overflowed=" +
       boolean_text(lifecycle_diagnostics.overflowed) +
       " lifecycle-events-dropped=" +
       std::to_string(lifecycle_diagnostics.dropped_events) +
       " init-cleanup-outstanding=" +
      boolean_text(g_bridge_init_succeeded.load(std::memory_order_acquire)) +
       " token-cleanup-outstanding=" +
       boolean_text(g_bridge_token_issued.load(std::memory_order_acquire)) +
       " startup-coverage=" + startup_coverage);
#else
  const std::string mutation_summary = "0";
  const std::string startup_coverage = "observer-worker-gap-not-covered";
#endif
  write_log(
      "trace.complete duration-ms=" +
      std::to_string(GetTickCount64() - start) + " census-modules=" +
      std::to_string(census.seen.size()) + " resolver-calls=" +
      std::to_string(g_resolver_calls.load(std::memory_order_relaxed)) +
      " resolver-nontarget-names=" +
      std::to_string(g_name_rejects.load(std::memory_order_relaxed)) +
      " target-null-results=" +
      std::to_string(g_target_nulls.load(std::memory_order_relaxed)) +
      " target-identity-or-caller-rejects=" +
      std::to_string(g_target_rejects.load(std::memory_order_relaxed)) +
      " resolver-events-dropped=" +
      std::to_string(
          g_resolver_events_dropped.load(std::memory_order_relaxed)) +
      " d3d12-init-passthrough-lookups=" +
      std::to_string(
          g_d3d12_init_passthroughs.load(std::memory_order_relaxed)) +
      " feature-d3d12-init-lookups=" +
      std::to_string(
          g_feature_d3d12_init_lookups.load(std::memory_order_relaxed)) +
      " first-feature-d3d12-init-lookup-tick=" +
      std::to_string(feature_init_tick) +
      " hook-readiness-at-first-feature-init-lookup=" +
      std::to_string(first_lookup_readiness) +
      " feature-status-hook=" +
      feature_status_hook_state_name(status_hook_state) +
      " feature-status-ready-tick=" +
      std::to_string(status_hook_tick) +
      " feature-status-coverage=" + status_coverage +
      " feature-status-detours=" +
      std::to_string(
          g_feature_status_detours.load(std::memory_order_relaxed)) +
      " feature-status-gate-rejects=" +
      std::to_string(
          g_feature_status_gate_rejects.load(std::memory_order_relaxed)) +
      " resolver-calls-before-ready=" + std::to_string(before_ready) +
      " quiesced=" + (quiesced ? "true" : "false") +
      " coverage=" +
      (before_ready == 0U && quiesced
           ? "best-effort-complete-after-enable"
           : "incomplete-after-enable") +
      " startup-coverage=" + startup_coverage + " mutations=" +
      mutation_summary);
}

DWORD WINAPI trace_worker(void *) noexcept {
  try {
    HMODULE pinned_self{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(
                                &get_proc_address_for_caller_detour),
                            &pinned_self) ||
        pinned_self != g_self) {
      g_trace_state.store(-1, std::memory_order_release);
      return 0U;
    }

    open_log();
    const HANDLE ready_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (ready_event != nullptr) {
      g_slot_ready_event.store(reinterpret_cast<std::uintptr_t>(ready_event),
                               std::memory_order_release);
    }
#if defined(MHW_DLSS_BRIDGE_PROBE)
    const bool mode_recognized = configure_bridge_mode();
    const bridge::Mode configured_mode =
        g_bridge_mode.load(std::memory_order_acquire);
    write_log(
        "trace.start version=0.5.1 mode=modern-provider-bridge-probe "
        "bridge-mode=" +
        std::string(bridge_mode_text(configured_mode)) +
        " mode-recognized=" + (mode_recognized ? "true" : "false") +
        " expected-loader=mo2-force-load slot-wake-event=" +
        std::string(ready_event != nullptr ? "true" : "false") +
        " default-inert=true attach-to-worker-ms=" +
        std::to_string(GetTickCount64() - g_attach_tick));
#else
    write_log(
        "trace.start version=0.4.1 mode=ngx-d3d12-status-boundary-observer "
        "expected-loader=mo2-force-load slot-wake-event=" +
        std::string(ready_event != nullptr ? "true" : "false") +
        " mutations=0 attach-to-worker-ms=" +
        std::to_string(GetTickCount64() - g_attach_tick));
#endif

    std::filesystem::path host_path;
    if (!prepare_host_boundary(&host_path)) {
      g_trace_state.store(-1, std::memory_order_release);
      return 0U;
    }

    const std::filesystem::path cache_path = cache_feature_path();
#if defined(MHW_DLSS_BRIDGE_PROBE)
    bool environment_verified = false;
    if (configured_mode != bridge::Mode::disabled) {
      environment_verified =
          mode_recognized && verify_host_disk_identity(host_path) &&
          verify_nvidia_adapter_inventory() &&
          prepare_bridge_preflight(GetModuleHandleW(nullptr), cache_path);
      if (!environment_verified) {
        g_bridge_lookup_transaction_complete.store(
            false, std::memory_order_release);
        g_bridge_route_state.store(-1, std::memory_order_release);
        g_trace_state.store(-1, std::memory_order_release);
        write_log("bridge.inert reason=preflight-rejected "
                  "resolver-installed=false provider-routes=0");
        return 0U;
      }
    }
#endif

    void *resolver{};
    if (!resolve_and_verify_system_target(&resolver)) {
      g_trace_state.store(-1, std::memory_order_release);
      return 0U;
    }
#if defined(MHW_DLSS_BRIDGE_PROBE)
    if (environment_verified) {
      g_bridge_environment_ready.store(true, std::memory_order_release);
    }
#endif
    if (!install_resolver_hook(resolver)) {
#if defined(MHW_DLSS_BRIDGE_PROBE)
      g_bridge_environment_ready.store(false, std::memory_order_release);
#endif
      g_trace_state.store(-1, std::memory_order_release);
      return 0U;
    }
    g_trace_state.store(1, std::memory_order_release);

#if defined(MHW_DLSS_BRIDGE_PROBE)
    const bool post_hook_environment_valid =
        configured_mode != bridge::Mode::disabled ||
        (verify_host_disk_identity(host_path) &&
         verify_nvidia_adapter_inventory());
#else
    const bool post_hook_environment_valid =
        verify_host_disk_identity(host_path) &&
        verify_nvidia_adapter_inventory();
#endif
    if (!post_hook_environment_valid) {
      const bool quiesced = close_trace_phase();
      g_trace_state.store(-1, std::memory_order_release);
      write_log("trace.inert reason=environment-verification-failed "
                "future-resolutions-pass-through=true "
                "previously-issued-wrappers-transparent=true quiesced=" +
                std::string(quiesced ? "true" : "false") + " mutations=0");
      return 0U;
    }

#if defined(MHW_DLSS_BRIDGE_PROBE)
    write_log("trace.active host-verified=true gpu-allowlist=RTX_5090 "
              "bridge-mode=" +
              std::string(bridge_mode_text(configured_mode)) +
              " hook-lifetime=process provider-mutations-possible=" +
              (configured_mode == bridge::Mode::disabled ? "false"
                                                          : "true"));
#else
    write_log("trace.active host-verified=true gpu-allowlist=RTX_5090 "
              "hook-lifetime=process mutations=0");
#endif
    run_bounded_trace(host_path.parent_path(), cache_path);
    g_trace_state.store(2, std::memory_order_release);
  } catch (...) {
    const bool quiesced = close_trace_phase();
    g_trace_state.store(-1, std::memory_order_release);
    write_log("trace.failure reason=unexpected-exception "
              "future-resolutions-pass-through=true "
              "previously-issued-wrappers-transparent=true quiesced=" +
              std::string(quiesced ? "true" : "false") + " mutations=0");
  }
  return 0U;
}

} // namespace

#if defined(MHW_DLSS_BRIDGE_PROBE)
extern "C" __declspec(dllexport) int __cdecl
MhwDlssBridgeProbeGetState() noexcept {
#else
extern "C" __declspec(dllexport) int __cdecl MhwDlssUnlockGetState() noexcept {
#endif
  return g_trace_state.load(std::memory_order_acquire);
}

BOOL APIENTRY DllMain(const HMODULE module, const DWORD reason, LPVOID) {
  if (reason != DLL_PROCESS_ATTACH) {
    return TRUE;
  }
  g_self = module;
  g_attach_tick = GetTickCount64();
  DisableThreadLibraryCalls(module);
  const HANDLE thread = CreateThread(nullptr, 0, &trace_worker, nullptr, 0,
                                     nullptr);
  if (thread != nullptr) {
    CloseHandle(thread);
  } else {
    g_trace_state.store(-1, std::memory_order_release);
  }
  return TRUE;
}
