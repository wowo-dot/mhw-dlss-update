#include "contract_snapshot.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
namespace bridge = mhw::bridge_probe;
constexpr std::array<std::string_view, 8> kExpectedKeys{
    "SuperSampling.Available",
    "#\x01",
    "SuperSampling.FeatureInitResult",
    "SuperSampling.NeedsUpdatedDriver",
    "ImageSignalProcessing.Available",
    "ImageSignalProcessing.FeatureInitResult",
    "SuperSampling.MinDriverVersionMajor",
    "SuperSampling.MinDriverVersionMinor"};
struct Context {
  std::size_t calls{};
  std::size_t fault_at{99U};
  bool missing{};
  bool missing_isp{};
  bool include_isp{true};
  bool mismatch{};
};

std::size_t key_index(const Context &context, const std::size_t call) {
  return !context.include_isp && call >= 4U ? call + 2U : call;
}

bridge::SignedGetterReply get_signed(void *opaque,
                                     const std::string_view key) noexcept {
  auto &context = *static_cast<Context *>(opaque);
  const std::size_t call = context.calls++;
  const std::size_t index = key_index(context, call);
  context.mismatch |= index >= 6U || key != kExpectedKeys[index];
  if (call == context.fault_at) {
    return {0xBAD00005U, 77, false, false};
  }
  // Missing differs from a present zero, and failed Init preserves its bits.
  const std::int32_t value = index == 2U   ? -1160773630
                             : index == 5U ? -1160773627
                                           : 0;
  const bool missing =
      context.missing || (context.missing_isp && (index == 4U || index == 5U));
  return {missing ? 0xBAD00010U : 1U, value, true, !missing};
}

bridge::UnsignedGetterReply get_unsigned(void *opaque,
                                         const std::string_view key) noexcept {
  auto &context = *static_cast<Context *>(opaque);
  const std::size_t call = context.calls++;
  const std::size_t index = key_index(context, call);
  context.mismatch |= index < 6U || index >= kExpectedKeys.size() ||
                      key != kExpectedKeys[index];
  if (call == context.fault_at) {
    return {0xBAD00005U, 77U, false, false};
  }
  return {context.missing ? 0xBAD00010U : 1U,
          (std::numeric_limits<std::uint32_t>::max)(), true, !context.missing};
}

bridge::GetterCallbacks callbacks(Context &context) noexcept {
  return {&context, get_unsigned, get_signed, nullptr, nullptr};
}

int fail(const char *message) {
  std::cerr << message << '\n';
  return 1;
}
} // namespace

int main() {
  static_assert(bridge::kCapabilityQueryCount == 8U);
  Context exact{};
  const auto snapshot = bridge::capture_capability_contract(
      callbacks(exact), bridge::CapabilityScope::game_requirements);
  if (exact.mismatch || exact.calls != 8U || !snapshot.summary.complete ||
      snapshot.summary.queries_attempted != 8U ||
      snapshot.summary.fault_latched || !snapshot.signed_values[0].present ||
      snapshot.signed_values[0].value != 0 ||
      static_cast<std::uint32_t>(snapshot.signed_values[2].value) !=
          0xBAD00002U ||
      static_cast<std::uint32_t>(snapshot.signed_values[5].value) !=
          0xBAD00005U ||
      snapshot.unsigned_values[1].value !=
          (std::numeric_limits<std::uint32_t>::max)()) {
    return fail("typed capability values or fixed query list changed");
  }
  if (bridge::key_name(bridge::CapabilitySignedKey::available_encoded).size() !=
          2U ||
      bridge::key_label(bridge::CapabilitySignedKey::available_encoded)
              .find('\x01') != std::string_view::npos) {
    return fail("encoded key or printable label is incorrect");
  }
  Context missing{};
  missing.missing = true;
  const auto absent = bridge::capture_capability_contract(
      callbacks(missing), bridge::CapabilityScope::game_requirements);
  if (!absent.summary.complete || absent.signed_values[0].present ||
      absent.signed_values[0].getter_result != 0xBAD00010U ||
      missing.calls != 8U) {
    return fail("absent capability was treated as a present zero or a fault");
  }
  Context missing_isp{};
  missing_isp.missing_isp = true;
  const auto isp_absent = bridge::capture_capability_contract(
      callbacks(missing_isp), bridge::CapabilityScope::game_requirements);
  if (!isp_absent.summary.complete || !isp_absent.signed_values[0].present ||
      isp_absent.signed_values[4].present ||
      isp_absent.signed_values[4].getter_result != 0xBAD00010U ||
      !isp_absent.signed_values[4].query_attempted) {
    return fail("missing ISP capability affected DLSS presence or became zero");
  }
  Context early{};
  early.include_isp = false;
  const auto dlss_only = bridge::capture_capability_contract(callbacks(early));
  if (early.mismatch || early.calls != 6U || !dlss_only.summary.complete ||
      dlss_only.summary.queries_attempted != 6U ||
      dlss_only.signed_values[4].query_attempted ||
      dlss_only.signed_values[5].query_attempted ||
      dlss_only.signed_values[4].key !=
          bridge::CapabilitySignedKey::image_signal_processing_available) {
    return fail("early Populate queried ISP before its publication");
  }
  for (std::size_t fault = 0; fault < 8U; ++fault) {
    Context context{};
    context.fault_at = fault;
    const auto stopped = bridge::capture_capability_contract(
        callbacks(context), bridge::CapabilityScope::game_requirements);
    if (context.mismatch || context.calls != fault + 1U ||
        stopped.summary.complete || !stopped.summary.fault_latched ||
        stopped.summary.stop_reason !=
            bridge::CaptureStopReason::callback_fault ||
        stopped.summary.queries_attempted != fault + 1U) {
      return fail("capability query continued after a callback fault");
    }
  }
  const auto no_callbacks = bridge::capture_capability_contract({});
  if (no_callbacks.summary.complete ||
      no_callbacks.summary.queries_attempted != 0U ||
      no_callbacks.summary.stop_reason !=
          bridge::CaptureStopReason::callback_missing) {
    return fail("missing callback was invoked");
  }
  std::cout << "capability snapshot tests passed\n";
  return 0;
}
