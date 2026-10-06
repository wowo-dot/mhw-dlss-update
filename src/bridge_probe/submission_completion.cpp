#include "submission_completion.hpp"
#include <algorithm>
#include <limits>

namespace mhw::bridge_probe {
SubmissionCompletion review_submission_completion(std::span<const BarrierCall> calls,
    std::span<const std::uintptr_t> lists, std::uintptr_t command, std::uint64_t marker) noexcept {
  SubmissionCompletion out{}; out.command = command; out.marker = marker;
  const auto reject = [&](CompletionIssue why) {
    out.status = CompletionStatus::rejected; out.issue = why; return out;
  };
  if (!command || !marker) return out;
  if (calls.size() > kBarrierCallCapacity || lists.size() > kSubmittedListCapacity)
    return reject(CompletionIssue::malformed);
  std::array<const BarrierCall *, kBarrierCallCapacity> ordered{};
  for (std::size_t i = 0; i < calls.size(); ++i) {
    const auto &c = calls[i];
    if (c.sequence != i + 1 || !c.entry_order || c.return_order <= c.entry_order ||
        c.entry_order == marker || c.return_order == marker ||
        static_cast<unsigned>(c.kind) > static_cast<unsigned>(BarrierCallKind::fence_cpu_signal))
      return reject(CompletionIssue::malformed);
    if (c.kind == BarrierCallKind::execute && (c.count > kSubmissionBatchCapacity ||
        c.first > lists.size() || c.count > lists.size() - c.first))
      return reject(CompletionIssue::malformed);
    ordered[i] = &c;
  }
  std::sort(ordered.begin(), ordered.begin() + calls.size(),
      [](auto a, auto b) { return a->entry_order < b->entry_order; });
  const auto sorted = std::span(ordered.data(), calls.size());
  const BarrierCall *reset{}, *close{}, *submit{}, *signal{}, *completed{};
  for (auto c : sorted) {
    if (c->command == command && c->kind == BarrierCallKind::reset && c->entry_order < marker)
      reset = c;
    if (c->kind != BarrierCallKind::execute || c->entry_order < marker) continue;
    unsigned matches{};
    for (unsigned n = 0; n < c->count; ++n) if (lists[c->first + n] == command) ++matches;
    if (matches > 1) return reject(CompletionIssue::recording);
    if (matches) { submit = c; break; }
  }
  if (!submit) return out;
  if (!reset || FAILED(reset->result) || reset->return_order >= marker)
    return reject(CompletionIssue::recording);
  if (!submit->queue || FAILED(submit->result)) return reject(CompletionIssue::recording);
  std::uint64_t previous{};
  for (auto c : sorted) {
    if (c->command != command || c->entry_order < reset->entry_order ||
        c->entry_order > submit->return_order) continue;
    if (previous >= c->entry_order || c->return_order >= submit->entry_order ||
        (c->entry_order < marker && c->return_order > marker) || close ||
        (c->kind != BarrierCallKind::barrier && c->kind != BarrierCallKind::reset && c->kind != BarrierCallKind::close) ||
        (c->kind == BarrierCallKind::reset && c != reset) || FAILED(c->result))
      return reject(CompletionIssue::recording);
    previous = c->return_order;
    if (c->kind == BarrierCallKind::close) close = c;
  }
  if (!close || close->entry_order <= marker) return reject(CompletionIssue::recording);
  out.reset = reset->sequence; out.close = close->sequence; out.submission = submit->sequence; out.queue = submit->queue;
  for (auto c : sorted) if (c->kind == BarrierCallKind::signal && c->queue == out.queue &&
      c->entry_order > submit->return_order) { signal = c; break; }
  if (!signal) return out;
  constexpr auto removed = (std::numeric_limits<std::uint64_t>::max)();
  if (FAILED(signal->result) || !signal->fence || !signal->fence_value || signal->fence_value == removed)
    return reject(CompletionIssue::writer);
  out.signal = signal->sequence; out.fence = signal->fence; out.value = signal->fence_value;
  std::uint64_t last_value{};
  for (auto c : sorted) if (c->kind == BarrierCallKind::fence_completed && c->fence == out.fence &&
      c->entry_order > signal->return_order) {
    if (c->fence_value == removed) return reject(CompletionIssue::device_removed);
    if (c->fence_value < last_value) return reject(CompletionIssue::writer);
    last_value = c->fence_value;
    if (c->fence_value >= out.value) { completed = c; break; }
  }
  if (!completed) return out;
  const BarrierCall *last_writer{}, *last_queue{};
  for (auto c : sorted) {
    if (c->fence == out.fence && c->entry_order < completed->return_order &&
        (c->kind == BarrierCallKind::signal || c->kind == BarrierCallKind::fence_cpu_signal)) {
      if (c->kind != BarrierCallKind::signal || c->queue != out.queue || FAILED(c->result) ||
          (last_writer && (last_writer->return_order >= c->entry_order || last_writer->fence_value >= c->fence_value)))
        return reject(CompletionIssue::writer);
      last_writer = c;
    }
    if (c->queue == out.queue && c->return_order >= submit->entry_order && c->entry_order <= signal->return_order &&
        (c->kind == BarrierCallKind::execute || c->kind == BarrierCallKind::signal || c->kind == BarrierCallKind::wait)) {
      if (FAILED(c->result) || (last_queue && last_queue->return_order >= c->entry_order))
        return reject(CompletionIssue::queue_overlap);
      last_queue = c;
    }
  }
  out.completed = completed->sequence; out.completed_order = completed->return_order;
  out.status = CompletionStatus::observed;
  return out;
}
} // namespace mhw::bridge_probe
