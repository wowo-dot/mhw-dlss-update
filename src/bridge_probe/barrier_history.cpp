#include "barrier_history.hpp"
#include <algorithm>

namespace mhw::bridge_probe {
namespace {
void remember(ResourceBarrierHistory &r, const BarrierCall &call,
              const D3D12_RESOURCE_BARRIER &b, unsigned index) noexcept {
  BarrierMatch m{};
  m.caller = call.caller; m.tick = call.tick; m.sequence = call.sequence;
  m.thread = call.thread; m.index = index; m.batch_size = call.count;
  m.command = call.command; m.entry_order = call.entry_order; m.return_order = call.return_order;
  m.type = b.Type; m.flags = b.Flags;
  if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
    m.subresource = b.Transition.Subresource;
    m.before = b.Transition.StateBefore; m.after = b.Transition.StateAfter;
  }
  if (r.retained == r.recent.size()) {
    std::move(r.recent.begin() + 1, r.recent.end(), r.recent.begin());
    --r.retained;
  }
  r.recent[r.retained++] = m;
  ++r.matches;
}
bool valid(const EvaluateContractSnapshot &s) noexcept {
  if (!s.summary.complete || s.summary.fault_latched || s.summary.stop_reason != CaptureStopReason::none ||
      s.summary.queries_attempted != kEvaluateQueryCount) return false;
  for (std::size_t i = 0; i < s.resources.size(); ++i) {
    const auto &r = s.resources[i];
    if (r.key != static_cast<EvaluateResourceKey>(i) || !r.present || !r.query_attempted ||
        !r.callback_completed || r.getter_result != 1 || r.value_is_null != (r.value == 0) ||
        (r.key != EvaluateResourceKey::previous_output && !r.value)) return false;
  }
  return true;
}
} // namespace
BarrierCapture summarize_barriers(std::span<const BarrierCall> calls,
                                 std::span<const D3D12_RESOURCE_BARRIER> barriers,
                                 std::uintptr_t command, unsigned thread,
                                 const EvaluateContractSnapshot &input, BarrierCapture s) noexcept {
  s.command = command; s.evaluate_thread = thread;
  s.calls = static_cast<unsigned>(calls.size()); s.descriptions = static_cast<unsigned>(barriers.size());
  s.contract_valid = command && valid(input);
  s.coverage_complete = s.installed && s.armed && s.finished && s.methods_match && s.contract_valid &&
      !s.faulted && !s.overflow && !s.contention && !s.expired && !s.in_flight;
  for (std::size_t i = 0; i < s.resources.size(); ++i) {
    s.resources[i].key = static_cast<EvaluateResourceKey>(i);
    s.resources[i].present = input.resources[i].value != 0;
  }
  if (!s.contract_valid) return s;
  // A successful Reset begins a new recording, even if the pointer was reused.
  std::size_t begin{};
  for (std::size_t i = 0; i < calls.size(); ++i) {
    const auto &c = calls[i];
    if (c.command == command && c.kind == BarrierCallKind::reset && SUCCEEDED(c.result)) {
      ++s.resets; begin = i + 1; s.epoch_sequence = c.sequence;
    }
  }
  for (std::size_t i = begin; i < calls.size(); ++i) {
    const auto &c = calls[i];
    if (c.command != command) continue;
    ++s.matching_calls;
    if (c.kind == BarrierCallKind::reset) { s.reset_failed = true; continue; }
    if (c.kind == BarrierCallKind::close) { s.closed = true; continue; }
    if (c.kind == BarrierCallKind::bundle) { s.bundle_seen = true; continue; }
    if (c.kind != BarrierCallKind::barrier) continue;
    if (c.first > barriers.size() || c.count > barriers.size() - c.first) {
      s.coverage_complete = false; continue;
    }
    for (unsigned j = 0; j < c.count; ++j) {
      const auto &b = barriers[c.first + j];
      if (b.Type > D3D12_RESOURCE_BARRIER_TYPE_UAV || b.Type < D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
        s.coverage_complete = false; continue;
      }
      for (std::size_t k = 0; k < s.resources.size(); ++k) {
        auto &r = s.resources[k];
        const auto value = input.resources[k].value;
        if (!value) continue;
        if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
          if (reinterpret_cast<std::uintptr_t>(b.Transition.pResource) != value) continue;
          remember(r, c, b, j);
          const bool whole = b.Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
          if (b.Flags != D3D12_RESOURCE_BARRIER_FLAG_NONE || !whole) r.ambiguous = true;
          if (r.transitions && r.last_declared_state != static_cast<unsigned>(b.Transition.StateBefore))
            r.continuity_mismatch = true;
          ++r.transitions;
          r.last_declared_state = b.Transition.StateAfter;
        } else if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV) {
          if (b.UAV.pResource && reinterpret_cast<std::uintptr_t>(b.UAV.pResource) != value) continue;
          remember(r, c, b, j); ++r.uav;
          if (b.Flags != D3D12_RESOURCE_BARRIER_FLAG_NONE) r.ambiguous = true;
        } else {
          if (b.Aliasing.pResourceBefore && b.Aliasing.pResourceAfter &&
              reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore) != value &&
              reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter) != value) continue;
          remember(r, c, b, j); ++r.aliasing; r.ambiguous = true;
        }
      }
    }
  }
  for (auto &r : s.resources) {
    r.declaration_available = s.coverage_complete && !s.closed && !s.bundle_seen && !s.reset_failed &&
        r.transitions && !r.ambiguous && !r.continuity_mismatch;
    // Input textures may combine read states. Keep this diagnostic conservative:
    // accept shader reads, DEPTH_READ and COPY_SOURCE, but no write/unknown bits.
    constexpr unsigned reads = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ |
        D3D12_RESOURCE_STATE_COPY_SOURCE;
    const auto state = r.last_declared_state;
    const bool matches = r.key == EvaluateResourceKey::output ? state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS :
        (state & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) != 0 && (state & ~reads) == 0;
    r.expected_state_matches = r.declaration_available && matches;
  }
  return s;
}
DepthTimeline depth_timeline(std::span<const BarrierCall> calls, std::span<const D3D12_RESOURCE_BARRIER> barriers,
                             std::span<const std::uintptr_t> lists, std::uintptr_t depth,
                             std::uintptr_t evaluate_command) noexcept {
  DepthTimeline s{};
  if (!depth) { s.malformed = true; return s; }
  for (const auto &c : calls) {
    if (c.kind != BarrierCallKind::barrier) continue;
    if (c.first > barriers.size() || c.count > barriers.size() - c.first) { s.malformed = true; continue; }
    for (unsigned i = 0; i < c.count; ++i) {
      const auto &b = barriers[c.first + i];
      bool match{};
      if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
        match = reinterpret_cast<std::uintptr_t>(b.Transition.pResource) == depth;
      else if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_UAV)
        match = !b.UAV.pResource || reinterpret_cast<std::uintptr_t>(b.UAV.pResource) == depth;
      else if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING)
        match = !b.Aliasing.pResourceBefore || !b.Aliasing.pResourceAfter ||
            reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore) == depth ||
            reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter) == depth;
      else s.malformed = true;
      if (!match) continue;
      ResourceBarrierHistory one{}; remember(one, c, b, i);
      ++s.matches;
      if (s.retained == s.recent.size()) {
        std::move(s.recent.begin() + 1, s.recent.end(), s.recent.begin());
        --s.retained; s.truncated = true;
      }
      s.recent[s.retained++] = one.recent[0];
    }
  }
  for (const auto &c : calls) {
    if (c.kind == BarrierCallKind::execute || c.kind == BarrierCallKind::signal || c.kind == BarrierCallKind::wait) {
      ++s.queues;
      if (s.queues_retained == s.queue_events.size()) { s.truncated = true; continue; }
      auto copy = c;
      if (c.kind == BarrierCallKind::execute) {
        if (c.first > lists.size() || c.count > lists.size() - c.first) { s.malformed = true; continue; }
        if (c.count > s.submitted_lists.size() - s.list_count) { s.truncated = true; continue; }
        copy.first = s.list_count;
        for (unsigned i = 0; i < c.count; ++i) s.submitted_lists[s.list_count++] = lists[c.first + i];
      }
      s.queue_events[s.queues_retained++] = copy;
    } else if (c.kind == BarrierCallKind::reset || c.kind == BarrierCallKind::close || c.kind == BarrierCallKind::bundle) {
      bool relevant = c.command == evaluate_command;
      for (unsigned i = 0; i < s.retained; ++i) relevant = relevant || s.recent[i].command == c.command;
      if (!relevant) continue;
      ++s.controls;
      if (s.controls_retained == s.control_events.size()) { s.truncated = true; continue; }
      s.control_events[s.controls_retained++] = c;
    }
  }
  return s;
}
} // namespace mhw::bridge_probe
