#include "barrier_observer.hpp"
#include "barrier_journal.hpp"
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <cwchar>
#include <intrin.h>

namespace mhw::bridge_probe {
namespace {
using BarrierFn = void(STDMETHODCALLTYPE *)(ID3D12GraphicsCommandList *, UINT, const D3D12_RESOURCE_BARRIER *);
using ResetFn = HRESULT(STDMETHODCALLTYPE *)(ID3D12GraphicsCommandList *, ID3D12CommandAllocator *, ID3D12PipelineState *);
using CloseFn = HRESULT(STDMETHODCALLTYPE *)(ID3D12GraphicsCommandList *);
using BundleFn = void(STDMETHODCALLTYPE *)(ID3D12GraphicsCommandList *, ID3D12GraphicsCommandList *);
using ExecuteFn = void(STDMETHODCALLTYPE *)(ID3D12CommandQueue *, UINT, ID3D12CommandList *const *);
using FenceFn = HRESULT(STDMETHODCALLTYPE *)(ID3D12CommandQueue *, ID3D12Fence *, UINT64);
BarrierFn original_barrier{};
ResetFn original_reset{};
CloseFn original_close{};
BundleFn original_bundle{};
ExecuteFn original_execute{};
FenceFn original_signal{}, original_wait{};
using CompletedFn = UINT64(STDMETHODCALLTYPE *)(ID3D12Fence *);
using CpuSignalFn = HRESULT(STDMETHODCALLTYPE *)(ID3D12Fence *, UINT64);
CompletedFn original_completed{};
CpuSignalFn original_cpu_signal{};
constexpr std::array<unsigned, 4> slots{26, 10, 9, 27};
std::array<void *, 9> targets{};
std::array<BarrierCall, kBarrierCallCapacity> calls{};
std::array<std::atomic_bool, kBarrierCallCapacity> ready{};
std::array<D3D12_RESOURCE_BARRIER, kBarrierDescriptionCapacity> descriptions{};
std::array<std::uintptr_t, kSubmittedListCapacity> submitted_lists{};
std::atomic_uint call_count{}, description_count{}, submitted_count{};
std::atomic<std::uint64_t> order{};
std::atomic_bool attempted{}, installed{}, armed{}, frozen{};
std::atomic_bool faulted{}, overflow{}, contention{}, expired{};
constexpr std::uint64_t open_bit = 1ULL << 63;
std::atomic<std::uint64_t> gate{}; // Open bit plus admitted callbacks still in flight.
std::atomic<std::uint64_t> frozen_tick{};
BarrierJournalWriter journal;
bool completion_enabled{}; // Published with gate; opt-in diagnostic only.
std::atomic<std::uintptr_t> completion_command{};
std::atomic<std::uint64_t> completion_marker{};
std::atomic<unsigned> completion_hint{}, stop_reason{};
std::atomic_flag reviewing = ATOMIC_FLAG_INIT;
std::atomic_bool terminal_published{};
unsigned reviewed_hint{}; // reviewing flag
bool checkpointed{}; // Serialized by the provider boundary; each published copy is immutable.
unsigned checkpoint_index{};
BarrierCapture checkpoint_status{};
unsigned checkpoint_calls{}, checkpoint_descriptions{}, checkpoint_lists{};
std::uint64_t deadline{}; // Published by gate's release store; never changed.
constexpr unsigned no_record = kBarrierCallCapacity;
struct Ticket { unsigned index{no_record}; bool admitted{}; };
#ifdef MHW_OWNED_OBSERVER_TEST
std::array<HANDLE, 2> pause_barrier{}, pause_query{};
void owned_pause(const std::array<HANDLE, 2> &events) noexcept {
  if (events[0] && events[1]) { SetEvent(events[0]); WaitForSingleObject(events[1], 3000); }
}
#endif
unsigned flags() noexcept {
  return (overflow ? journal_overflow : 0U) | (faulted ? journal_fault : 0U) |
      (contention ? journal_contention : 0U) | (expired ? journal_expired : 0U);
}
void finalize() noexcept;
void stop(JournalStopReason reason = JournalStopReason::requested) noexcept {
  unsigned empty{};
  stop_reason.compare_exchange_strong(empty, static_cast<unsigned>(reason));
  gate.fetch_and(~open_bit, std::memory_order_acq_rel);
  journal.stop(flags());
}
unsigned published_prefix() noexcept {
  const auto count = (std::min)(call_count.load(std::memory_order_acquire), kBarrierCallCapacity);
  unsigned n{};
  for (; n < count && ready[n].load(std::memory_order_acquire); ++n) {}
  return n;
}
SubmissionCompletion completion_review(unsigned count) noexcept {
  // Only completed slots are read. Payload pools for these immutable calls
  // were published by the same release store; unused entries are never read.
  return review_submission_completion(std::span(calls.data(), count), submitted_lists,
      completion_command.load(std::memory_order_acquire), completion_marker.load(std::memory_order_acquire));
}
void finalize() noexcept {
  if (gate.load(std::memory_order_acquire) != 0 || !stop_reason.load() || terminal_published.exchange(true)) return;
  const auto count = published_prefix();
  JournalTermination t{};
  t.reason = static_cast<JournalStopReason>(stop_reason.load());
  t.flags = flags(); t.frontier = (std::min)(call_count.load(), kBarrierCallCapacity);
  if (count != t.frontier) { faulted = true; t.flags |= journal_fault; }
  if (completion_enabled && completion_marker.load()) t.completion = completion_review(count);
  if (t.flags) t.reason = faulted ? JournalStopReason::fault : overflow ? JournalStopReason::overflow :
      contention ? JournalStopReason::contention : JournalStopReason::expired;
  else if (t.reason == JournalStopReason::completion && t.completion.status != CompletionStatus::observed)
    t.reason = JournalStopReason::ambiguous;
  t.order = order.fetch_add(1, std::memory_order_relaxed) + 1;
  journal.stop(t.flags); journal.terminate(t);
}
void consider_completion() noexcept {
  if (!completion_enabled || !completion_marker.load(std::memory_order_acquire) ||
      !(gate.load(std::memory_order_acquire) & open_bit) || reviewing.test_and_set(std::memory_order_acquire)) return;
  const auto hint = completion_hint.load(std::memory_order_acquire);
  if (hint && hint != reviewed_hint) {
    const auto prefix = published_prefix();
    if (prefix >= hint) {
      reviewed_hint = hint;
      const auto result = calls[hint - 1].entry_order > completion_marker.load() ? completion_review(prefix) : SubmissionCompletion{};
      if (result.status == CompletionStatus::observed) stop(JournalStopReason::completion);
      else if (result.status == CompletionStatus::rejected) stop(JournalStopReason::ambiguous);
    }
  }
  reviewing.clear(std::memory_order_release);
}
bool enter() noexcept {
  auto value = gate.load(std::memory_order_acquire);
  for (unsigned i = 0; i < 32 && (value & open_bit); ++i)
    if (gate.compare_exchange_weak(value, value + 1, std::memory_order_acq_rel)) return true;
  if (value & open_bit) { contention = true; stop(); }
  return false;
}
int filter(DWORD code) noexcept {
  return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR ?
      EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}
bool methods(ID3D12GraphicsCommandList *list, std::array<void *, 4> &out) noexcept {
  __try {
    const auto table = *reinterpret_cast<void ***>(list);
    for (std::size_t i = 0; i < slots.size(); ++i) out[i] = table[slots[i]];
    return true;
  } __except (filter(GetExceptionCode())) { faulted = true; return false; }
}
bool system_target(void *target) noexcept {
  MEMORY_BASIC_INFORMATION m{};
  if (!target || VirtualQuery(target, &m, sizeof(m)) != sizeof(m) || m.Type != MEM_IMAGE ||
      m.State != MEM_COMMIT || (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
      !(m.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
  wchar_t actual[MAX_PATH]{}, expected[MAX_PATH]{};
  if (!GetModuleFileNameW(static_cast<HMODULE>(m.AllocationBase), actual, MAX_PATH)) return false;
  const auto slash = wcsrchr(actual, L'\\');
  if (!slash || (_wcsicmp(slash + 1, L"D3D12Core.dll") && _wcsicmp(slash + 1, L"d3d12.dll"))) return false;
  const auto n = GetSystemDirectoryW(expected, MAX_PATH);
  if (!n || n + wcslen(slash) >= MAX_PATH || wcscat_s(expected, slash)) return false;
  return _wcsicmp(actual, expected) == 0;
}
void release_discovery(IUnknown *object) noexcept {
  if (!object) return;
  __try { object->Release(); }
  __except (filter(GetExceptionCode())) { faulted = true; }
}
bool discover_queue(ID3D12GraphicsCommandList *list) noexcept {
  ID3D12Device *device{};
  ID3D12CommandQueue *queue{};
  ID3D12Fence *fence{};
  bool ok{};
  __try {
    D3D12_COMMAND_QUEUE_DESC desc{};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (SUCCEEDED(list->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void **>(&device))) && device &&
        SUCCEEDED(device->CreateCommandQueue(&desc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void **>(&queue))) && queue) {
      auto table = *reinterpret_cast<void ***>(queue);
      targets[4] = table[10]; targets[5] = table[14]; targets[6] = table[15];
      if (SUCCEEDED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void **>(&fence))) && fence) {
        table = *reinterpret_cast<void ***>(fence);
        targets[7] = table[8]; targets[8] = table[10]; ok = true;
      }
    }
  } __except (filter(GetExceptionCode())) { faulted = true; }
  release_discovery(fence); release_discovery(queue); release_discovery(device);
  return ok && !faulted;
}
bool copy_descriptions(unsigned first, UINT count, const D3D12_RESOURCE_BARRIER *source) noexcept {
  __try {
    for (UINT i = 0; i < count; ++i) descriptions[first + i] = source[i];
    return true;
  } __except (filter(GetExceptionCode())) { faulted = true; stop(); return false; }
}
Ticket begin(ID3D12GraphicsCommandList *list, BarrierCallKind kind, const void *caller,
               UINT count = 0, const D3D12_RESOURCE_BARRIER *barriers = nullptr) noexcept {
  if (!enter()) return {};
  const auto tick = GetTickCount64();
  if (tick > deadline) { expired = true; stop(); return {no_record, true}; }
  // Each callback owns disjoint slots. No lock or wait on any rendering thread.
  if (count > kBarrierBatchCapacity) { overflow = true; stop(); return {no_record, true}; }
  const auto index = call_count.fetch_add(1, std::memory_order_relaxed);
  if (index >= calls.size()) { overflow = true; stop(); return {no_record, true}; }
  journal.reserve(index);
  const auto first = description_count.fetch_add(count, std::memory_order_relaxed);
  calls[index] = {reinterpret_cast<std::uintptr_t>(list), reinterpret_cast<std::uintptr_t>(caller),
                  tick, GetCurrentThreadId(), index + 1, first, count, kind, E_PENDING};
  calls[index].entry_order = order.fetch_add(1, std::memory_order_relaxed) + 1;
  if (first > descriptions.size() || count > descriptions.size() - first) {
    overflow = true; stop(); calls[index].count = 0;
  } else if (count && !copy_descriptions(first, count, barriers)) calls[index].count = 0;
  return {index, true};
}
void complete(Ticket ticket, HRESULT result = S_OK) noexcept {
  if (ticket.index != no_record) {
    calls[ticket.index].result = result;
    calls[ticket.index].return_order = order.fetch_add(1, std::memory_order_relaxed) + 1;
    const auto &c = calls[ticket.index];
    journal.publish(ticket.index, c,
        c.kind == BarrierCallKind::barrier && c.count ? std::span(descriptions.data() + c.first, c.count) : std::span<const D3D12_RESOURCE_BARRIER>{},
        c.kind == BarrierCallKind::execute && c.count ? std::span(submitted_lists.data() + c.first, c.count) : std::span<const std::uintptr_t>{});
    ready[ticket.index].store(true, std::memory_order_release);
    if (c.kind == BarrierCallKind::fence_completed) completion_hint.store(c.sequence, std::memory_order_release);
    consider_completion();
  }
  if (ticket.admitted && gate.fetch_sub(1, std::memory_order_acq_rel) == 1) finalize();
}
void STDMETHODCALLTYPE barrier_hook(ID3D12GraphicsCommandList *list, UINT count, const D3D12_RESOURCE_BARRIER *barriers) {
  const auto incoming = GetLastError();
  const auto index = begin(list, BarrierCallKind::barrier, _ReturnAddress(), count, barriers);
  SetLastError(incoming);
  original_barrier(list, count, barriers);
  const auto outgoing = GetLastError();
#ifdef MHW_OWNED_OBSERVER_TEST
  if (!count) owned_pause(pause_barrier);
#endif
  complete(index);
  SetLastError(outgoing);
}
HRESULT STDMETHODCALLTYPE reset_hook(ID3D12GraphicsCommandList *list, ID3D12CommandAllocator *allocator, ID3D12PipelineState *pipeline) {
  const auto incoming = GetLastError();
  const auto index = begin(list, BarrierCallKind::reset, _ReturnAddress());
  SetLastError(incoming);
  const auto result = original_reset(list, allocator, pipeline);
  const auto outgoing = GetLastError();
  complete(index, result);
  SetLastError(outgoing);
  return result;
}
HRESULT STDMETHODCALLTYPE close_hook(ID3D12GraphicsCommandList *list) {
  const auto incoming = GetLastError();
  const auto index = begin(list, BarrierCallKind::close, _ReturnAddress());
  SetLastError(incoming);
  const auto result = original_close(list);
  const auto outgoing = GetLastError();
  complete(index, result);
  SetLastError(outgoing);
  return result;
}
void STDMETHODCALLTYPE bundle_hook(ID3D12GraphicsCommandList *list, ID3D12GraphicsCommandList *bundle) {
  const auto incoming = GetLastError();
  const auto index = begin(list, BarrierCallKind::bundle, _ReturnAddress());
  SetLastError(incoming);
  original_bundle(list, bundle);
  const auto outgoing = GetLastError();
  complete(index);
  SetLastError(outgoing);
}
void queue_metadata(Ticket ticket, ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value,
                    UINT count = 0, ID3D12CommandList *const *lists = nullptr) noexcept {
  if (ticket.index == no_record) return;
  auto &c = calls[ticket.index];
  c.first = 0; c.count = 0;
  c.queue = reinterpret_cast<std::uintptr_t>(queue); c.fence = reinterpret_cast<std::uintptr_t>(fence);
  c.fence_value = value;
  if (!count) return;
  if (count > kSubmissionBatchCapacity) { overflow = true; stop(); return; }
  const auto first = submitted_count.fetch_add(count, std::memory_order_relaxed);
  if (first > submitted_lists.size() || count > submitted_lists.size() - first) { overflow = true; stop(); return; }
  c.first = first; c.count = count;
  __try { for (UINT i = 0; i < count; ++i) submitted_lists[first + i] = reinterpret_cast<std::uintptr_t>(lists[i]); }
  __except (filter(GetExceptionCode())) { faulted = true; c.count = 0; stop(); }
}
void STDMETHODCALLTYPE execute_hook(ID3D12CommandQueue *queue, UINT count, ID3D12CommandList *const *lists) {
  const auto incoming = GetLastError();
  const auto ticket = begin(nullptr, BarrierCallKind::execute, _ReturnAddress());
  queue_metadata(ticket, queue, nullptr, 0, count, lists);
  SetLastError(incoming); original_execute(queue, count, lists);
  const auto outgoing = GetLastError(); complete(ticket); SetLastError(outgoing);
}
HRESULT fence_call(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value,
                   BarrierCallKind kind, const void *caller, FenceFn original) {
  const auto incoming = GetLastError();
  const auto ticket = begin(nullptr, kind, caller);
  queue_metadata(ticket, queue, fence, value);
  SetLastError(incoming); const auto result = original(queue, fence, value);
  const auto outgoing = GetLastError(); complete(ticket, result); SetLastError(outgoing);
  return result;
}
HRESULT STDMETHODCALLTYPE signal_hook(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value) {
  return fence_call(queue, fence, value, BarrierCallKind::signal, _ReturnAddress(), original_signal);
}
HRESULT STDMETHODCALLTYPE wait_hook(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value) {
  return fence_call(queue, fence, value, BarrierCallKind::wait, _ReturnAddress(), original_wait);
}
UINT64 STDMETHODCALLTYPE completed_hook(ID3D12Fence *fence) {
  const auto incoming = GetLastError();
  const auto ticket = begin(nullptr, BarrierCallKind::fence_completed, _ReturnAddress());
  SetLastError(incoming); const auto value = original_completed(fence);
  const auto outgoing = GetLastError();
#ifdef MHW_OWNED_OBSERVER_TEST
  owned_pause(pause_query);
#endif
  queue_metadata(ticket, nullptr, fence, value); complete(ticket); SetLastError(outgoing);
  return value; // UINT64_MAX is retained verbatim; it means device removal.
}
HRESULT STDMETHODCALLTYPE cpu_signal_hook(ID3D12Fence *fence, UINT64 value) {
  const auto incoming = GetLastError();
  const auto ticket = begin(nullptr, BarrierCallKind::fence_cpu_signal, _ReturnAddress());
  queue_metadata(ticket, nullptr, fence, value);
  SetLastError(incoming); const auto result = original_cpu_signal(fence, value);
  const auto outgoing = GetLastError(); complete(ticket, result); SetLastError(outgoing);
  return result;
}
} // namespace
#ifdef MHW_OWNED_OBSERVER_TEST
void pause_owned_observer(BarrierCallKind kind, HANDLE entered, HANDLE resume) noexcept {
  (kind == BarrierCallKind::barrier ? pause_barrier : pause_query) = {entered, resume};
}
#endif
bool create_barrier_journal() noexcept { return !attempted && journal.create(); }
bool arm_barrier_observer(ID3D12GraphicsCommandList *list, bool stop_after_completion) noexcept {
  std::array<void *, 4> list_targets{};
  if (attempted.exchange(true) || frozen || !list || !methods(list, list_targets)) return false;
  std::copy(list_targets.begin(), list_targets.end(), targets.begin());
  if (!discover_queue(list)) return false;
  for (const auto target : targets) if (!system_target(target)) return false;
  for (std::size_t i = 0; i < targets.size(); ++i)
    for (std::size_t j = 0; j < i; ++j) if (targets[i] == targets[j]) return false;
  const std::array<void *, 9> detours{reinterpret_cast<void *>(&barrier_hook), reinterpret_cast<void *>(&reset_hook),
      reinterpret_cast<void *>(&close_hook), reinterpret_cast<void *>(&bundle_hook), reinterpret_cast<void *>(&execute_hook),
      reinterpret_cast<void *>(&signal_hook), reinterpret_cast<void *>(&wait_hook), reinterpret_cast<void *>(&completed_hook), reinterpret_cast<void *>(&cpu_signal_hook)};
  const std::array<void **, 9> originals{reinterpret_cast<void **>(&original_barrier), reinterpret_cast<void **>(&original_reset),
      reinterpret_cast<void **>(&original_close), reinterpret_cast<void **>(&original_bundle), reinterpret_cast<void **>(&original_execute),
      reinterpret_cast<void **>(&original_signal), reinterpret_cast<void **>(&original_wait), reinterpret_cast<void **>(&original_completed), reinterpret_cast<void **>(&original_cpu_signal)};
  for (std::size_t i = 0; i < targets.size(); ++i) {
    if (MH_CreateHook(targets[i], detours[i], originals[i]) != MH_OK) {
      for (std::size_t j = 0; j < i; ++j) MH_RemoveHook(targets[j]); // Never enabled.
      return false;
    }
  }
  for (const auto target : targets) {
    if (MH_EnableHook(target) != MH_OK) {
      for (const auto rollback : targets) MH_DisableHook(rollback);
      // Once enabled, keep all trampolines alive for any in-flight callbacks.
      return false;
    }
  }
  installed = true; armed = true; completion_enabled = stop_after_completion;
  deadline = GetTickCount64() + 2000;
  journal.start(deadline);
  gate.store(open_bit, std::memory_order_release);
  return true;
}
void freeze_barrier_observer() noexcept {
  stop();
  std::uint64_t empty{};
  frozen_tick.compare_exchange_strong(empty, GetTickCount64());
  frozen = true;
  finalize();
}
void checkpoint_barrier_observer(ID3D12GraphicsCommandList *list, unsigned index) noexcept {
  if (index >= 3 || (checkpointed && index <= checkpoint_index)) return;
  checkpoint_index = index;
  checkpoint_status = {}; checkpoint_calls = checkpoint_descriptions = checkpoint_lists = 0;
  const auto tick = GetTickCount64();
  const auto marker = order.fetch_add(1, std::memory_order_relaxed) + 1;
  auto &s = checkpoint_status;
  s.installed = installed; s.armed = armed; s.finished = true;
  s.in_flight = (gate.load(std::memory_order_acquire) & ~open_bit) != 0;
  const auto reserved = (std::min)(call_count.load(), kBarrierCallCapacity);
  for (; checkpoint_calls < reserved; ++checkpoint_calls) {
    if (!ready[checkpoint_calls].load(std::memory_order_acquire)) { s.in_flight = true; break; }
    const auto &c = calls[checkpoint_calls];
    // A racing callback may reserve/finish after our marker. Retain its data,
    // but never qualify that prefix as wholly before this Evaluate boundary.
    if (c.return_order >= marker) s.in_flight = true;
    if (c.kind == BarrierCallKind::barrier && c.count)
      checkpoint_descriptions = (std::max)(checkpoint_descriptions, c.first + c.count);
    if (c.kind == BarrierCallKind::execute && c.count)
      checkpoint_lists = (std::max)(checkpoint_lists, c.first + c.count);
  }
  s.faulted = faulted; s.overflow = overflow; s.contention = contention;
  s.expired = expired || (s.armed && tick > deadline);
  journal.checkpoint(reinterpret_cast<std::uintptr_t>(list), tick, marker, reserved, checkpoint_calls, index);
  if (completion_enabled && index == 0) {
    completion_command.store(reinterpret_cast<std::uintptr_t>(list), std::memory_order_relaxed);
    completion_marker.store(marker, std::memory_order_release);
  }
  checkpointed = true;
}
BarrierCapture finish_barrier_observer(ID3D12GraphicsCommandList *list,
                                       const EvaluateContractSnapshot &input) noexcept {
  if (checkpointed) {
    auto s = checkpoint_status;
    std::array<void *, 4> current{};
    s.methods_match = list && methods(list, current) && std::equal(current.begin(), current.end(), targets.begin());
    const auto command = reinterpret_cast<std::uintptr_t>(list);
    const auto records = std::span(calls.data(), checkpoint_calls);
    const auto desc = std::span(descriptions.data(), checkpoint_descriptions);
    s = summarize_barriers(records, desc, command, GetCurrentThreadId(), input, s);
    if (s.contract_valid)
      s.depth_timeline = depth_timeline(records, desc, std::span(submitted_lists.data(), checkpoint_lists),
          input.resources[static_cast<std::size_t>(EvaluateResourceKey::depth)].value, command);
    return s;
  }
  freeze_barrier_observer();
  BarrierCapture s{};
  s.installed = installed; s.armed = armed; s.finished = frozen;
  std::array<void *, 4> current{};
  s.methods_match = list && methods(list, current) && std::equal(current.begin(), current.end(), targets.begin());
  if (s.armed && frozen_tick.load() > deadline) s.expired = true;
  const auto command = reinterpret_cast<std::uintptr_t>(list);
  const auto thread = GetCurrentThreadId();
  s.in_flight = (gate.load(std::memory_order_acquire) & ~open_bit) != 0;
  // A driver call that has not returned cannot be accepted. Do not wait for it
  // or read its result while another callback could still publish completion.
  unsigned count{};
  const auto reserved = (std::min)(call_count.load(), kBarrierCallCapacity);
  for (; count < reserved; ++count) if (!ready[count].load(std::memory_order_acquire)) { s.in_flight = true; break; }
  s.faulted = faulted; s.overflow = overflow; s.contention = contention; s.expired = s.expired || expired;
  s = summarize_barriers(std::span(calls.data(), count),
                         std::span(descriptions.data(), (std::min)(description_count.load(), kBarrierDescriptionCapacity)),
                         command, thread, input, s);
  if (s.contract_valid)
    s.depth_timeline = depth_timeline(std::span(calls.data(), count),
        std::span(descriptions.data(), (std::min)(description_count.load(), kBarrierDescriptionCapacity)),
        std::span(submitted_lists.data(), (std::min)(submitted_count.load(), kSubmittedListCapacity)),
        input.resources[static_cast<std::size_t>(EvaluateResourceKey::depth)].value, command);
  return s;
}
} // namespace mhw::bridge_probe
