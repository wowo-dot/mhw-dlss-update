#include "contract_snapshot.hpp"

namespace mhw::bridge_probe {
namespace {

constexpr std::array<CreateUnsignedKey, kCreateUnsignedKeyCount>
    kCreateUnsignedKeys{
        CreateUnsignedKey::width,
        CreateUnsignedKey::height,
        CreateUnsignedKey::out_width,
        CreateUnsignedKey::out_height,
        CreateUnsignedKey::resource_width,
        CreateUnsignedKey::resource_height,
        CreateUnsignedKey::creation_node_mask,
        CreateUnsignedKey::visibility_node_mask,
    };

constexpr std::array<CreateSignedKey, kCreateSignedKeyCount>
    kCreateSignedKeys{
        CreateSignedKey::perf_quality_value,
        CreateSignedKey::rtx_value,
        CreateSignedKey::hint_hdr,
    };

constexpr std::array<EvaluateResourceKey, kEvaluateResourceKeyCount>
    kEvaluateResourceKeys{
        EvaluateResourceKey::color,
        EvaluateResourceKey::motion_vectors,
        EvaluateResourceKey::previous_output,
        EvaluateResourceKey::depth,
        EvaluateResourceKey::output,
    };

constexpr std::array<EvaluateFloatKey, kEvaluateFloatKeyCount>
    kEvaluateFloatKeys{
        EvaluateFloatKey::sharpness,
        EvaluateFloatKey::mv_scale_x,
        EvaluateFloatKey::mv_scale_y,
        EvaluateFloatKey::mv_offset_x,
        EvaluateFloatKey::mv_offset_y,
        EvaluateFloatKey::hdr_clamp,
    };

constexpr std::array<EvaluateSignedKey, kEvaluateSignedKeyCount>
    kEvaluateSignedKeys{
        EvaluateSignedKey::depth_inverted,
        EvaluateSignedKey::reset,
        EvaluateSignedKey::dlaa_mode,
        EvaluateSignedKey::hint_run_pre_pass,
        EvaluateSignedKey::hint_run_pass,
        EvaluateSignedKey::hint_run_post_pass,
    };

void validate_state(CaptureState &state) noexcept {
  const bool reason_latched = state.stop_reason != CaptureStopReason::none;
  if (state.query_count > kContractQueryCapacity ||
      state.fault_latched != reason_latched) {
    state.fault_latched = true;
    state.stop_reason = CaptureStopReason::invalid_state;
  }
}

bool begin_query(CaptureState &state, const bool callback_available,
                 bool &query_attempted) noexcept {
  if (state.fault_latched) {
    return false;
  }
  if (state.query_count >= kContractQueryCapacity) {
    state.fault_latched = true;
    state.stop_reason = CaptureStopReason::query_capacity_exhausted;
    return false;
  }
  if (!callback_available) {
    state.fault_latched = true;
    state.stop_reason = CaptureStopReason::callback_missing;
    return false;
  }

  query_attempted = true;
  ++state.query_count;
  return true;
}

void latch_callback_fault(CaptureState &state,
                          const bool callback_completed) noexcept {
  if (!callback_completed) {
    state.fault_latched = true;
    state.stop_reason = CaptureStopReason::callback_fault;
  }
}

template <typename Key, typename Value, typename Callback, typename Reply>
void capture_value(CaptureState &state, void *const context,
                   const Callback callback, const Key key,
                   ValueCapture<Key, Value> &capture) noexcept {
  capture.key = key;
  if (!begin_query(state, callback != nullptr, capture.query_attempted)) {
    return;
  }

  const Reply reply = callback(context, key_name(key));
  capture.getter_result = reply.getter_result;
  capture.value = reply.value;
  capture.callback_completed = reply.callback_completed;
  capture.present = reply.present;
  latch_callback_fault(state, reply.callback_completed);
}

void capture_resource(CaptureState &state, void *const context,
                      const ResourceGetter callback,
                      const EvaluateResourceKey key,
                      EvaluateResourceCapture &capture) noexcept {
  capture.key = key;
  if (!begin_query(state, callback != nullptr, capture.query_attempted)) {
    return;
  }

  const ResourceGetterReply reply = callback(context, key_name(key));
  capture.getter_result = reply.getter_result;
  capture.value = reply.value;
  capture.callback_completed = reply.callback_completed;
  capture.present = reply.present;
  capture.value_is_null = reply.value == 0U;
  latch_callback_fault(state, reply.callback_completed);
}

CaptureSummary finish_capture(const CaptureState &state,
                              const std::size_t starting_query_count) noexcept {
  const std::size_t queries_attempted =
      state.query_count >= starting_query_count
          ? state.query_count - starting_query_count
          : 0U;
  return {queries_attempted, state.stop_reason, !state.fault_latched,
          state.fault_latched};
}

} // namespace

std::string_view key_name(const CreateUnsignedKey key) noexcept {
  switch (key) {
  case CreateUnsignedKey::width:
    return "Width";
  case CreateUnsignedKey::height:
    return "Height";
  case CreateUnsignedKey::out_width:
    return "OutWidth";
  case CreateUnsignedKey::out_height:
    return "OutHeight";
  case CreateUnsignedKey::resource_width:
    return "ResourceWidth";
  case CreateUnsignedKey::resource_height:
    return "ResourceHeight";
  case CreateUnsignedKey::creation_node_mask:
    return "CreationNodeMask";
  case CreateUnsignedKey::visibility_node_mask:
    return "VisibilityNodeMask";
  case CreateUnsignedKey::count:
    break;
  }
  return {};
}

std::string_view key_name(const CreateSignedKey key) noexcept {
  switch (key) {
  case CreateSignedKey::perf_quality_value:
    return "PerfQualityValue";
  case CreateSignedKey::rtx_value:
    return "RTXValue";
  case CreateSignedKey::hint_hdr:
    return "Hint.HDR";
  case CreateSignedKey::count:
    break;
  }
  return {};
}

std::string_view key_name(const EvaluateResourceKey key) noexcept {
  switch (key) {
  case EvaluateResourceKey::color:
    return "Color";
  case EvaluateResourceKey::motion_vectors:
    return "MotionVectors";
  case EvaluateResourceKey::previous_output:
    return "PreviousOutput";
  case EvaluateResourceKey::depth:
    return "Depth";
  case EvaluateResourceKey::output:
    return "Output";
  case EvaluateResourceKey::count:
    break;
  }
  return {};
}

std::string_view key_name(const EvaluateFloatKey key) noexcept {
  switch (key) {
  case EvaluateFloatKey::sharpness:
    return "Sharpness";
  case EvaluateFloatKey::mv_scale_x:
    return "MV.Scale.X";
  case EvaluateFloatKey::mv_scale_y:
    return "MV.Scale.Y";
  case EvaluateFloatKey::mv_offset_x:
    return "MV.Offset.X";
  case EvaluateFloatKey::mv_offset_y:
    return "MV.Offset.Y";
  case EvaluateFloatKey::hdr_clamp:
    return "HDRClamp";
  case EvaluateFloatKey::count:
    break;
  }
  return {};
}

std::string_view key_name(const EvaluateSignedKey key) noexcept {
  switch (key) {
  case EvaluateSignedKey::depth_inverted:
    return "DepthInverted";
  case EvaluateSignedKey::reset:
    return "Reset";
  case EvaluateSignedKey::dlaa_mode:
    return "DLAAMode";
  case EvaluateSignedKey::hint_run_pre_pass:
    return "Hint.RunPrePass";
  case EvaluateSignedKey::hint_run_pass:
    return "Hint.RunPass";
  case EvaluateSignedKey::hint_run_post_pass:
    return "Hint.RunPostPass";
  case EvaluateSignedKey::count:
    break;
  }
  return {};
}

std::string_view key_name(const CapabilitySignedKey key) noexcept {
  switch (key) {
  case CapabilitySignedKey::available:
    return "SuperSampling.Available";
  case CapabilitySignedKey::available_encoded:
    return "#\x01";
  case CapabilitySignedKey::feature_init_result:
    return "SuperSampling.FeatureInitResult";
  case CapabilitySignedKey::needs_updated_driver:
    return "SuperSampling.NeedsUpdatedDriver";
  case CapabilitySignedKey::image_signal_processing_available:
    return "ImageSignalProcessing.Available";
  case CapabilitySignedKey::image_signal_processing_init_result:
    return "ImageSignalProcessing.FeatureInitResult";
  case CapabilitySignedKey::count:
    return {};
  }
  return {};
}

std::string_view key_label(const CapabilitySignedKey key) noexcept {
  return key == CapabilitySignedKey::available_encoded
             ? "SuperSampling.Available[encoded-0x01]"
             : key_name(key);
}

std::string_view key_name(const CapabilityUnsignedKey key) noexcept {
  switch (key) {
  case CapabilityUnsignedKey::min_driver_major:
    return "SuperSampling.MinDriverVersionMajor";
  case CapabilityUnsignedKey::min_driver_minor:
    return "SuperSampling.MinDriverVersionMinor";
  case CapabilityUnsignedKey::count:
    return {};
  }
  return {};
}

CapabilityContractSnapshot
capture_capability_contract(const GetterCallbacks &callbacks,
                            const CapabilityScope scope) noexcept {
  static_assert(kCapabilityQueryCount <= kContractQueryCapacity);
  CaptureState state{};
  CapabilityContractSnapshot snapshot{};
  for (std::size_t index = 0; index < kCapabilitySignedKeyCount; ++index) {
    const auto key = static_cast<CapabilitySignedKey>(index);
    snapshot.signed_values[index].key = key;
    if (scope == CapabilityScope::super_sampling &&
        (key == CapabilitySignedKey::image_signal_processing_available ||
         key == CapabilitySignedKey::image_signal_processing_init_result)) {
      continue;
    }
    capture_value<CapabilitySignedKey, std::int32_t, SignedGetter,
                  SignedGetterReply>(state, callbacks.context,
                                     callbacks.get_signed, key,
                                     snapshot.signed_values[index]);
  }
  for (std::size_t index = 0; index < kCapabilityUnsignedKeyCount; ++index) {
    capture_value<CapabilityUnsignedKey, std::uint32_t, UnsignedGetter,
                  UnsignedGetterReply>(
        state, callbacks.context, callbacks.get_unsigned,
        static_cast<CapabilityUnsignedKey>(index),
        snapshot.unsigned_values[index]);
  }
  snapshot.summary = finish_capture(state, 0U);
  return snapshot;
}

CreateContractSnapshot
capture_create_contract(CaptureState &state,
                        const GetterCallbacks &callbacks) noexcept {
  validate_state(state);
  const std::size_t starting_query_count = state.query_count;
  CreateContractSnapshot snapshot{};

  for (std::size_t index = 0U; index < kCreateUnsignedKeys.size(); ++index) {
    capture_value<CreateUnsignedKey, std::uint32_t, UnsignedGetter,
                  UnsignedGetterReply>(state, callbacks.context,
                                       callbacks.get_unsigned,
                                       kCreateUnsignedKeys[index],
                                       snapshot.unsigned_values[index]);
  }
  for (std::size_t index = 0U; index < kCreateSignedKeys.size(); ++index) {
    capture_value<CreateSignedKey, std::int32_t, SignedGetter,
                  SignedGetterReply>(state, callbacks.context,
                                     callbacks.get_signed,
                                     kCreateSignedKeys[index],
                                     snapshot.signed_values[index]);
  }

  snapshot.summary = finish_capture(state, starting_query_count);
  return snapshot;
}

OptimalSettingsSnapshot
capture_optimal_settings_contract(const GetterCallbacks &callbacks) noexcept {
  CaptureState state{};
  OptimalSettingsSnapshot snapshot{};
  for (std::size_t i = 0; i < 2; ++i) {
    capture_value<CreateUnsignedKey, std::uint32_t, UnsignedGetter,
                  UnsignedGetterReply>(
        state, callbacks.context, callbacks.get_unsigned,
        static_cast<CreateUnsignedKey>(i), snapshot.unsigned_values[i]);
  }
  for (std::size_t i = 0; i < 2; ++i) {
    capture_value<CreateSignedKey, std::int32_t, SignedGetter,
                  SignedGetterReply>(
        state, callbacks.context, callbacks.get_signed,
        static_cast<CreateSignedKey>(i), snapshot.signed_values[i]);
  }
  snapshot.summary = finish_capture(state, 0U);
  return snapshot;
}

EvaluateContractSnapshot
capture_evaluate_contract(CaptureState &state,
                          const GetterCallbacks &callbacks) noexcept {
  validate_state(state);
  const std::size_t starting_query_count = state.query_count;
  EvaluateContractSnapshot snapshot{};

  for (std::size_t index = 0U; index < kEvaluateResourceKeys.size(); ++index) {
    capture_resource(state, callbacks.context, callbacks.get_resource,
                     kEvaluateResourceKeys[index], snapshot.resources[index]);
  }
  for (std::size_t index = 0U; index < kEvaluateFloatKeys.size(); ++index) {
    capture_value<EvaluateFloatKey, float, FloatGetter, FloatGetterReply>(
        state, callbacks.context, callbacks.get_float,
        kEvaluateFloatKeys[index], snapshot.floats[index]);
  }
  for (std::size_t index = 0U; index < kEvaluateSignedKeys.size(); ++index) {
    capture_value<EvaluateSignedKey, std::int32_t, SignedGetter,
                  SignedGetterReply>(state, callbacks.context,
                                     callbacks.get_signed,
                                     kEvaluateSignedKeys[index],
                                     snapshot.signed_values[index]);
  }

  snapshot.summary = finish_capture(state, starting_query_count);
  return snapshot;
}

} // namespace mhw::bridge_probe
