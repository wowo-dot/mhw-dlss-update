#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mhw::bridge_probe {

enum class CreateUnsignedKey : std::uint8_t {
  width,
  height,
  out_width,
  out_height,
  resource_width,
  resource_height,
  creation_node_mask,
  visibility_node_mask,
  count,
};

enum class CreateSignedKey : std::uint8_t {
  perf_quality_value,
  rtx_value,
  hint_hdr,
  count,
};

enum class EvaluateResourceKey : std::uint8_t {
  color,
  motion_vectors,
  previous_output,
  depth,
  output,
  count,
};

enum class EvaluateFloatKey : std::uint8_t {
  sharpness,
  mv_scale_x,
  mv_scale_y,
  mv_offset_x,
  mv_offset_y,
  hdr_clamp,
  count,
};

enum class EvaluateSignedKey : std::uint8_t {
  depth_inverted,
  reset,
  dlaa_mode,
  hint_run_pre_pass,
  hint_run_pass,
  hint_run_post_pass,
  count,
};

enum class CapabilitySignedKey : std::uint8_t {
  available,
  available_encoded,
  feature_init_result,
  needs_updated_driver,
  image_signal_processing_available,
  image_signal_processing_init_result,
  count,
};

enum class CapabilityUnsignedKey : std::uint8_t {
  min_driver_major,
  min_driver_minor,
  count,
};

inline constexpr std::size_t kCapabilitySignedKeyCount =
    static_cast<std::size_t>(CapabilitySignedKey::count);
inline constexpr std::size_t kCapabilityUnsignedKeyCount =
    static_cast<std::size_t>(CapabilityUnsignedKey::count);
inline constexpr std::size_t kCapabilityQueryCount =
    kCapabilitySignedKeyCount + kCapabilityUnsignedKeyCount;

inline constexpr std::size_t kCreateUnsignedKeyCount =
    static_cast<std::size_t>(CreateUnsignedKey::count);
inline constexpr std::size_t kCreateSignedKeyCount =
    static_cast<std::size_t>(CreateSignedKey::count);
inline constexpr std::size_t kEvaluateResourceKeyCount =
    static_cast<std::size_t>(EvaluateResourceKey::count);
inline constexpr std::size_t kEvaluateFloatKeyCount =
    static_cast<std::size_t>(EvaluateFloatKey::count);
inline constexpr std::size_t kEvaluateSignedKeyCount =
    static_cast<std::size_t>(EvaluateSignedKey::count);
inline constexpr std::size_t kCreateQueryCount =
    kCreateUnsignedKeyCount + kCreateSignedKeyCount;
inline constexpr std::size_t kEvaluateQueryCount =
    kEvaluateResourceKeyCount + kEvaluateFloatKeyCount +
    kEvaluateSignedKeyCount;

// One complete Create snapshot plus one complete Evaluate snapshot is the
// entire milestone-A budget. A later query latches the engine off.
inline constexpr std::size_t kContractQueryCapacity =
    kCreateQueryCount + kEvaluateQueryCount;

std::string_view key_name(CreateUnsignedKey key) noexcept;
std::string_view key_name(CreateSignedKey key) noexcept;
std::string_view key_name(EvaluateResourceKey key) noexcept;
std::string_view key_name(EvaluateFloatKey key) noexcept;
std::string_view key_name(EvaluateSignedKey key) noexcept;
std::string_view key_name(CapabilitySignedKey key) noexcept;
std::string_view key_name(CapabilityUnsignedKey key) noexcept;
// Keep the control byte in the NGX short-form key out of text logs.
std::string_view key_label(CapabilitySignedKey key) noexcept;

// getter_result is intentionally opaque. The capture engine preserves it
// exactly and does not infer vendor success or presence from its value.
struct UnsignedGetterReply {
  std::uint32_t getter_result{};
  std::uint32_t value{};
  bool callback_completed{};
  bool present{};
};

struct SignedGetterReply {
  std::uint32_t getter_result{};
  std::int32_t value{};
  bool callback_completed{};
  bool present{};
};

struct FloatGetterReply {
  std::uint32_t getter_result{};
  float value{};
  bool callback_completed{};
  bool present{};
};

struct ResourceGetterReply {
  std::uint32_t getter_result{};
  std::uintptr_t value{};
  bool callback_completed{};
  bool present{};
};

// Runtime adapters must translate any platform exception/fault into
// callback_completed=false before returning. The portable engine performs no
// SEH, pointer probing, or resource dereference.
using UnsignedGetter = UnsignedGetterReply (*)(void *context,
                                                std::string_view key) noexcept;
using SignedGetter = SignedGetterReply (*)(void *context,
                                            std::string_view key) noexcept;
using FloatGetter = FloatGetterReply (*)(void *context,
                                          std::string_view key) noexcept;
using ResourceGetter = ResourceGetterReply (*)(void *context,
                                                std::string_view key) noexcept;

struct GetterCallbacks {
  void *context{};
  UnsignedGetter get_unsigned{};
  SignedGetter get_signed{};
  FloatGetter get_float{};
  ResourceGetter get_resource{};
};

enum class CaptureStopReason : std::uint8_t {
  none,
  callback_missing,
  callback_fault,
  query_capacity_exhausted,
  invalid_state,
};

// This is the only state retained between captures. It contains no callback,
// context, parameter, resource, or vendor pointer.
struct CaptureState {
  std::size_t query_count{};
  CaptureStopReason stop_reason{CaptureStopReason::none};
  bool fault_latched{};
};

template <typename Key, typename Value> struct ValueCapture {
  Key key{};
  std::uint32_t getter_result{};
  Value value{};
  bool query_attempted{};
  bool callback_completed{};
  bool present{};
};

using CreateUnsignedCapture =
    ValueCapture<CreateUnsignedKey, std::uint32_t>;
using CreateSignedCapture = ValueCapture<CreateSignedKey, std::int32_t>;
using EvaluateFloatCapture = ValueCapture<EvaluateFloatKey, float>;
using EvaluateSignedCapture = ValueCapture<EvaluateSignedKey, std::int32_t>;

struct EvaluateResourceCapture {
  EvaluateResourceKey key{};
  std::uint32_t getter_result{};
  std::uintptr_t value{};
  bool query_attempted{};
  bool callback_completed{};
  bool present{};
  bool value_is_null{true};
};

struct CaptureSummary {
  std::size_t queries_attempted{};
  CaptureStopReason stop_reason{CaptureStopReason::none};
  bool complete{};
  bool fault_latched{};
};

struct CreateContractSnapshot {
  std::array<CreateUnsignedCapture, kCreateUnsignedKeyCount> unsigned_values{};
  std::array<CreateSignedCapture, kCreateSignedKeyCount> signed_values{};
  CaptureSummary summary{};
};

// Inputs to the separate callback stored in DLSSOptimalSettingsCallback.
// Input capture is separate from the optional diagnostic output publication.
struct OptimalSettingsSnapshot {
  std::array<CreateUnsignedCapture, 2> unsigned_values{};
  std::array<CreateSignedCapture, 2> signed_values{};
  CaptureSummary summary{};
};
OptimalSettingsSnapshot
capture_optimal_settings_contract(const GetterCallbacks &callbacks) noexcept;

struct EvaluateContractSnapshot {
  std::array<EvaluateResourceCapture, kEvaluateResourceKeyCount> resources{};
  std::array<EvaluateFloatCapture, kEvaluateFloatKeyCount> floats{};
  std::array<EvaluateSignedCapture, kEvaluateSignedKeyCount> signed_values{};
  CaptureSummary summary{};
};

struct CapabilityContractSnapshot {
  std::array<ValueCapture<CapabilitySignedKey, std::int32_t>,
             kCapabilitySignedKeyCount>
      signed_values{};
  std::array<ValueCapture<CapabilityUnsignedKey, std::uint32_t>,
             kCapabilityUnsignedKeyCount>
      unsigned_values{};
  CaptureSummary summary{};
};

enum class CapabilityScope : std::uint8_t {
  super_sampling,
  game_requirements,
};

// Six scalar attempts at Populate, eight after GetParameters. ISP publication
// follows DLSS in the core's feature loop, so query it only after that loop.
// The runtime separately admits
// each of four sources once and shares its getter-fault latch with all other
// captures. This never consumes the Create/Evaluate query budget.
CapabilityContractSnapshot capture_capability_contract(
    const GetterCallbacks &callbacks,
    CapabilityScope scope = CapabilityScope::super_sampling) noexcept;

CreateContractSnapshot
capture_create_contract(CaptureState &state,
                        const GetterCallbacks &callbacks) noexcept;

EvaluateContractSnapshot
capture_evaluate_contract(CaptureState &state,
                          const GetterCallbacks &callbacks) noexcept;

} // namespace mhw::bridge_probe
