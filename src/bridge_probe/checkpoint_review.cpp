#include "checkpoint_review.hpp"
#include <algorithm>

namespace mhw::bridge_probe {
namespace {
bool touches(const D3D12_RESOURCE_BARRIER &v, const CaptureBinding &binding) noexcept {
  auto relevant = [&](ID3D12Resource *p) {
    const auto label = reinterpret_cast<std::uintptr_t>(p);
    return !label || std::find(binding.resources.begin(), binding.resources.end(), label) != binding.resources.end();
  };
  // Unknown or split barrier semantics are not eligible for the unrelated-call exception.
  if (v.Flags != D3D12_RESOURCE_BARRIER_FLAG_NONE) return true;
  switch (v.Type) {
  case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION: return relevant(v.Transition.pResource);
  case D3D12_RESOURCE_BARRIER_TYPE_UAV: return relevant(v.UAV.pResource);
  case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
    return true; // Heap alias relationships are outside this capture contract.
  default: return true;
  }
}
} // namespace
CheckpointReview review_checkpoint(const MailboxRead &capture, const JournalRead &journal, unsigned index) {
  CheckpointReview result{};
  auto reject = [&](std::string_view reason) { result.reason = reason; return result; };
  if (capture.state != MailboxState::published || !capture.inputs_verified || index >= 3)
    return reject("input-evidence");
  const auto &binding = capture.binding;
  const auto &mark = journal.checkpoints[index];
  if (binding.index != index || !binding.command || !binding.thread || mark.state != 2 ||
      mark.command != binding.command || !mark.order || !mark.reserved || mark.prefix > mark.reserved)
    return reject("checkpoint-binding");
  if (!journal.valid || journal.flags || journal.pending || journal.failed ||
      journal.records.size() != journal.completed || journal.records.size() > kBarrierCallCapacity ||
      mark.reserved > journal.records.size() || journal.barrier_records.size() > kBarrierDescriptionCapacity)
    return reject("journal-incomplete");
  EvaluateContractSnapshot contract{};
  contract.summary.complete = true; contract.summary.queries_attempted = kEvaluateQueryCount;
  for (unsigned i = 0; i < binding.resources.size(); ++i) {
    if (i != static_cast<unsigned>(EvaluateResourceKey::previous_output) && !binding.resources[i])
      return reject("missing-resource");
    auto &r = contract.resources[i]; r.key = static_cast<EvaluateResourceKey>(i);
    r.value = binding.resources[i]; r.value_is_null = !r.value;
    r.present = r.query_attempted = r.callback_completed = true; r.getter_result = 1;
  }
  std::vector<BarrierCall> selected;
  selected.reserve(mark.reserved);
  std::vector<std::uint64_t> orders; orders.reserve(journal.records.size() * 2 + 1);
  orders.push_back(mark.order);
  for (std::size_t i = 0; i < journal.records.size(); ++i) {
    const auto &c = journal.records[i];
    if (c.sequence != i + 1 || !c.entry_order || c.return_order <= c.entry_order ||
        (c.entry_order < mark.order && c.sequence > mark.reserved) ||
        static_cast<unsigned>(c.kind) > static_cast<unsigned>(BarrierCallKind::fence_cpu_signal))
      return reject("call-order");
    orders.push_back(c.entry_order); orders.push_back(c.return_order);
    if (c.kind == BarrierCallKind::barrier && (c.count > kBarrierBatchCapacity ||
        c.first > journal.barrier_records.size() || c.count > journal.barrier_records.size() - c.first))
      return reject("barrier-range");
    if (c.entry_order < mark.order && c.return_order > mark.order) {
      if (c.command == binding.command) return reject("selected-recording-overlap");
      if (c.kind == BarrierCallKind::fence_completed) { ++result.query_overlaps; continue; }
      if (c.kind != BarrierCallKind::barrier || !c.command) return reject("unclassified-overlap");
      for (unsigned n = 0; n < c.count; ++n)
        if (touches(journal.barrier_records[c.first + n], binding)) return reject("resource-overlap");
      ++result.unrelated_overlaps;
    }
    if (c.command == binding.command && c.return_order < mark.order) selected.push_back(c);
  }
  std::sort(orders.begin(), orders.end());
  if (std::adjacent_find(orders.begin(), orders.end()) != orders.end()) return reject("duplicate-order");
  std::sort(selected.begin(), selected.end(), [](const auto &a, const auto &b) { return a.entry_order < b.entry_order; });
  for (std::size_t i = 1; i < selected.size(); ++i)
    if (selected[i-1].return_order >= selected[i].entry_order) return reject("concurrent-selected-recording");
  BarrierCapture status{};
  status.installed = status.armed = status.finished = status.methods_match = true;
  const auto summary = summarize_barriers(selected, journal.barrier_records, binding.command, binding.thread, contract, status);
  result.epoch = summary.epoch_sequence; result.selected_calls = static_cast<unsigned>(selected.size());
  if (!summary.coverage_complete || !summary.epoch_sequence || summary.closed || summary.bundle_seen || summary.reset_failed)
    return reject("recording-generation");
  for (const auto &r : summary.resources) {
    if (!r.present) continue;
    const auto bit = 1U << static_cast<unsigned>(r.key);
    if (r.ambiguous || r.continuity_mismatch) return reject("resource-declarations");
    if (r.declaration_available) result.declared_state_mask |= bit;
    else result.unknown_state_mask |= bit;
    if (r.expected_state_matches) result.expected_state_mask |= bit;
    if (r.transitions && !r.expected_state_matches) return reject("resource-declarations");
  }
  // This authorizes another bounded depth-argument observation, never rendering
  // with a modern SDK. Missing color/output transitions remain explicitly unknown;
  // a UAV barrier alone is not a state declaration. Require the depth read itself.
  if (!(result.expected_state_mask & (1U << static_cast<unsigned>(EvaluateResourceKey::depth))))
    return reject("depth-declaration-missing");
  result.accepted = true; result.reason = "selected-recording-complete";
  return result;
}
} // namespace mhw::bridge_probe
