#include "barrier_observer.hpp"
#include "barrier_journal.hpp"
#include "capture_mailbox.hpp"
#include <MinHook.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <thread>

namespace {
namespace b = mhw::bridge_probe;
using Microsoft::WRL::ComPtr;
constexpr std::array<unsigned, 4> role_indices{0, 1, 3, 4};
void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
void hr(HRESULT value, const char *message) { check(SUCCEEDED(value), message); }
b::EvaluateContractSnapshot contract(const std::array<ID3D12Resource *, 5> &resources) {
  b::EvaluateContractSnapshot s{};
  s.summary.complete = true; s.summary.queries_attempted = b::kEvaluateQueryCount;
  for (std::size_t i = 0; i < resources.size(); ++i) {
    auto &r = s.resources[i]; r.key = static_cast<b::EvaluateResourceKey>(i);
    r.value = reinterpret_cast<std::uintptr_t>(resources[i]); r.value_is_null = !r.value;
    r.present = r.query_attempted = r.callback_completed = true; r.getter_result = 1;
  }
  return s;
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
  D3D12_RESOURCE_BARRIER b{}; b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  b.Transition = {resource, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, before, after}; return b;
}
b::BarrierCapture clean() {
  b::BarrierCapture s{}; s.installed = s.armed = s.finished = s.methods_match = true; return s;
}
void print_target(const wchar_t *kind, void *target, unsigned slot) {
  MEMORY_BASIC_INFORMATION m{}; VirtualQuery(target, &m, sizeof(m));
  wchar_t path[MAX_PATH]{}; GetModuleFileNameW(static_cast<HMODULE>(m.AllocationBase), path, MAX_PATH);
  const auto name = wcsrchr(path, L'\\');
  std::wcout << kind << L" slot=" << slot << L" module=" << (name ? name + 1 : path) << L" rva=" << std::hex
             << (reinterpret_cast<std::uintptr_t>(target) - reinterpret_cast<std::uintptr_t>(m.AllocationBase)) << std::dec << L'\n';
}
void replay_tests() {
  const std::array<ID3D12Resource *, 5> pointers{reinterpret_cast<ID3D12Resource *>(0x1000),
      reinterpret_cast<ID3D12Resource *>(0x2000), nullptr, reinterpret_cast<ID3D12Resource *>(0x3000),
      reinterpret_cast<ID3D12Resource *>(0x4000)};
  auto input = contract(pointers);
  std::array<D3D12_RESOURCE_BARRIER, 4> descriptions{};
  for (unsigned i = 0; i < descriptions.size(); ++i)
    descriptions[i] = transition(pointers[role_indices[i]], D3D12_RESOURCE_STATE_RENDER_TARGET,
        i == 3 ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  std::array<b::BarrierCall, 1> calls{{{0x8000, 0x9000, 10, 4, 1, 0, 4, b::BarrierCallKind::barrier, S_OK}}};
  auto replay = [&](b::BarrierCapture status = clean()) { return b::summarize_barriers(calls, descriptions, 0x8000, 4, input, status); };
  auto s = replay();
  check(s.coverage_complete && s.resources[3].expected_state_matches && s.resources[4].expected_state_matches,
        "four whole transitions correlate to command list and roles");
  check(!s.resources[2].present && !s.resources[2].declaration_available, "null previous is not state zero");
  for (const auto state : {0x40U, 0xC0U, 0xE0U, 0x840U}) {
    descriptions[2].Transition.StateAfter = static_cast<D3D12_RESOURCE_STATES>(state);
    check(replay().resources[3].expected_state_matches, "non-pixel input may combine supported read states");
  }
  for (const auto state : {0U, 0x80U, 0x48U, 0x50U, 0x440U, 0x80000040U, 0xAC3U}) {
    descriptions[2].Transition.StateAfter = static_cast<D3D12_RESOURCE_STATES>(state);
    check(!replay().resources[3].expected_state_matches, "missing read bit, writes and unsupported texture states rejected");
  }
  descriptions[2].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  descriptions[3].Transition.StateAfter = static_cast<D3D12_RESOURCE_STATES>(0x48);
  check(!replay().resources[4].expected_state_matches, "output UAV must be exclusive");
  descriptions[3].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
  calls[0].command = 0x8888;
  check(!replay().resources[0].declaration_available, "other command list cannot supply state evidence");
  calls[0].command = 0x8000;
  for (auto flag : {&b::BarrierCapture::faulted, &b::BarrierCapture::overflow, &b::BarrierCapture::contention,
                    &b::BarrierCapture::expired, &b::BarrierCapture::in_flight}) {
    auto status = clean(); status.*flag = true;
    check(!replay(status).resources[0].declaration_available, "gaps reject declaration acceptance");
  }
  auto status = clean(); status.methods_match = false;
  check(!replay(status).coverage_complete, "unhooked method implementation rejected");
  input.resources[0].value_is_null = true;
  check(!replay().contract_valid, "inconsistent input contract rejected");
  input = contract(pointers);
  descriptions[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
  check(replay().resources[0].ambiguous && !replay().resources[0].declaration_available, "split barrier not full transition");
  descriptions[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE; descriptions[0].Transition.Subresource = 0;
  check(!replay().resources[0].declaration_available, "subresource transition never promoted to whole texture");
  descriptions[0] = transition(pointers[0], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  auto ds = std::vector<D3D12_RESOURCE_BARRIER>(descriptions.begin(), descriptions.end());
  ds.push_back(transition(pointers[0], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
  calls[0].count = 5;
  s = b::summarize_barriers(calls, ds, 0x8000, 4, input, clean());
  check(s.resources[0].continuity_mismatch && !s.resources[0].declaration_available, "inconsistent before state rejected");
  ds[4] = {}; ds[4].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
  s = b::summarize_barriers(calls, ds, 0x8000, 4, input, clean());
  check(s.resources[0].uav == 1 && s.resources[0].last_declared_state == 0x40, "global UAV orders but does not change state");
  ds[4].Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
  check(!b::summarize_barriers(calls, ds, 0x8000, 4, input, clean()).resources[0].declaration_available,
        "global alias invalidates all inferred declarations");
  calls[0].count = 100;
  check(!replay().coverage_complete, "malformed range rejected without overread");
  calls[0].count = 4;
  std::array<b::BarrierCall, 2> reset_calls{calls[0], {0x8000, 0, 11, 4, 2, 0, 0, b::BarrierCallKind::reset, S_OK}};
  s = b::summarize_barriers(reset_calls, descriptions, 0x8000, 4, input, clean());
  check(s.resets == 1 && !s.resources[0].declaration_available, "Reset discards old recording even at same address");
  reset_calls[1].result = E_FAIL;
  check(b::summarize_barriers(reset_calls, descriptions, 0x8000, 4, input, clean()).reset_failed, "failed Reset rejects coverage");
  for (auto kind : {b::BarrierCallKind::close, b::BarrierCallKind::bundle}) {
    reset_calls[1].kind = kind; reset_calls[1].result = S_OK;
    check(!b::summarize_barriers(reset_calls, descriptions, 0x8000, 4, input, clean()).resources[0].declaration_available,
          "Close or opaque bundle blocks declaration acceptance");
  }
  std::vector<b::BarrierCall> many;
  D3D12_RESOURCE_BARRIER uav{}; uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV; uav.UAV.pResource = pointers[0];
  for (unsigned i = 0; i < 20; ++i) many.push_back({0x8000, 0, i, 4, i + 1, 0, 1, b::BarrierCallKind::barrier, S_OK});
  s = b::summarize_barriers(many, std::span(&uav, 1), 0x8000, 4, input, clean());
  check(s.resources[0].matches == 20 && s.resources[0].retained == 4 && s.resources[0].recent[0].sequence == 17,
        "bounded history preserves latest four with total count");
  std::array<b::BarrierCall, 4> cross{calls[0], reset_calls[1], {}, {}};
  cross[0].command = 0x7777; cross[0].entry_order = 1; cross[0].return_order = 2;
  cross[1].command = 0x7777; cross[1].kind = b::BarrierCallKind::close; cross[1].result = S_OK;
  cross[1].entry_order = 3; cross[1].return_order = 4;
  cross[2].kind = b::BarrierCallKind::execute; cross[2].queue = 0x6666; cross[2].first = 0; cross[2].count = 1;
  cross[2].entry_order = 5; cross[2].return_order = 6;
  cross[3].kind = b::BarrierCallKind::signal; cross[3].queue = 0x6666; cross[3].fence = 0x5555; cross[3].fence_value = 123;
  const std::array<std::uintptr_t, 1> submitted{0x7777};
  auto t = b::depth_timeline(cross, descriptions, submitted, 0x3000, 0x8000);
  check(t.matches == 1 && t.recent[0].command == 0x7777 && t.controls_retained == 1 &&
        t.queues_retained == 2 && t.submitted_lists[0] == 0x7777 && t.queue_events[1].fence_value == 123,
        "other-list depth and queue records survive selected-recording exclusion");
  cross[2].count = 2;
  check(b::depth_timeline(cross, descriptions, submitted, 0x3000, 0x8000).malformed, "submission overread rejected");
  std::vector<b::BarrierCall> queue_flood(80, cross[3]);
  check(b::depth_timeline(queue_flood, {}, {}, 0x3000, 0x8000).truncated, "bounded timeline reports missing queue events");
}
void live_test(std::string_view mode) {
  const bool completion = mode == "--completion" || mode == "--hardware-completion" || mode == "--completion-drain";
  const bool continuation = mode == "--continuation" || mode == "--hardware-continuation" || completion;
  const bool hardware = mode == "--hardware" || mode == "--hardware-continuation" || mode == "--hardware-completion";
  const bool queue_test = hardware || mode == "--queue" || continuation;
  b::BarrierJournalReader journal;
  if (continuation) {
    check(b::create_barrier_journal(), "create continuation journal before hooks");
    check(journal.open(GetCurrentProcessId(), b::process_creation_stamp(GetCurrentProcess())), "open continuation read-only");
  }
  ComPtr<IDXGIFactory4> factory; ComPtr<IDXGIAdapter> adapter; ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> list;
  hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "create factory");
  if (hardware) hr(factory->EnumAdapters(0, &adapter), "hardware adapter");
  else hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)), "WARP adapter");
  hr(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)), "create device");
  hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)), "allocator");
  hr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)), "list");
  // Print only image basename and RVA; no local account paths.
  auto table = *reinterpret_cast<void ***>(list.Get());
  for (auto slot : {26, 10, 9, 27}) print_target(L"command-list", table[slot], slot);
  check(MH_Initialize() == MH_OK, "MinHook init");
  check(b::arm_barrier_observer(list.Get(), completion), "system D3D12 observer arm");
  check(!b::arm_barrier_observer(list.Get()), "second arm rejected");
  hr(list->Close(), "close old recording");
  hr(list->Reset(allocator.Get(), nullptr), "reset for new recording");
  std::array<ComPtr<ID3D12Resource>, 4> resources;
  std::array<ID3D12Resource *, 5> pointers{};
  std::array<D3D12_RESOURCE_BARRIER, 4> barriers{};
  for (unsigned i = 0; i < resources.size(); ++i) {
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC d{}; d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = 32; d.Height = 16; d.DepthOrArraySize = 1; d.MipLevels = 1; d.SampleDesc.Count = 1;
    d.Format = DXGI_FORMAT_R32_FLOAT; d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    hr(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                       nullptr, IID_PPV_ARGS(&resources[i])), "texture");
    pointers[role_indices[i]] = resources[i].Get();
    barriers[i] = transition(pointers[role_indices[i]], D3D12_RESOURCE_STATE_RENDER_TARGET,
        i == 3 ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  }
  ComPtr<ID3D12CommandAllocator> producer_allocator;
  ComPtr<ID3D12GraphicsCommandList> producer;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12Fence> fence;
  if (queue_test) {
    D3D12_COMMAND_QUEUE_DESC q{}; q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr(device->CreateCommandQueue(&q, IID_PPV_ARGS(&queue)), "fixture queue");
    const auto queue_table = *reinterpret_cast<void ***>(queue.Get());
    for (auto slot : {10, 14, 15}) print_target(L"queue", queue_table[slot], slot);
    hr(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), "fixture fence");
    const auto fence_table = *reinterpret_cast<void ***>(fence.Get());
    for (auto slot : {8, 10}) print_target(L"fence", fence_table[slot], slot);
    hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&producer_allocator)), "producer allocator");
    hr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, producer_allocator.Get(), nullptr, IID_PPV_ARGS(&producer)), "producer list");
    producer->ResourceBarrier(1, &barriers[2]);
    hr(producer->Close(), "close producer");
    ID3D12CommandList *submitted[] = {producer.Get()};
    queue->ExecuteCommandLists(1, submitted);
    hr(queue->Signal(fence.Get(), 7), "observed queue signal");
    hr(queue->Wait(fence.Get(), 7), "observed queue wait");
    hr(queue->Signal(fence.Get(), 8), "fixture completion signal");
    const auto done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    check(done != nullptr, "completion event");
    const auto armed_event = fence->SetEventOnCompletion(8, done);
    const auto wait_result = SUCCEEDED(armed_event) ? WaitForSingleObject(done, 5000) : WAIT_FAILED;
    CloseHandle(done);
    check(wait_result == WAIT_OBJECT_0, "fixture GPU completes before resource destruction");
  }
  SetLastError(0x5678);
  if (queue_test) {
    const std::array<D3D12_RESOURCE_BARRIER, 3> remaining{barriers[0], barriers[1], barriers[3]};
    list->ResourceBarrier(static_cast<UINT>(remaining.size()), remaining.data());
  } else list->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
  check(GetLastError() == 0x5678, "barrier preserves LastError");
  if (mode == "--call-limit") {
    auto before = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    for (unsigned i = 0; i < b::kBarrierCallCapacity + 4; ++i) {
      const auto after = before == D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE ?
          D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      const auto v = transition(pointers[0], before, after);
      list->ResourceBarrier(1, &v); before = after;
    }
  } else if (mode == "--batch-limit" || mode == "--description-limit") {
    const auto count = mode == "--batch-limit" ? b::kBarrierBatchCapacity + 1 : b::kBarrierBatchCapacity;
    std::vector<D3D12_RESOURCE_BARRIER> batch;
    auto before = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    for (unsigned i = 0; i < count; ++i) {
      const auto after = before == D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE ?
          D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      batch.push_back(transition(pointers[0], before, after)); before = after;
    }
    const unsigned repeats = mode == "--batch-limit" ? 1 : b::kBarrierDescriptionCapacity / count + 1;
    for (unsigned i = 0; i < repeats; ++i) list->ResourceBarrier(count, batch.data());
  } else if (mode == "--expiry") {
    Sleep(2100);
  }
  if (continuation) b::checkpoint_barrier_observer(list.Get());
  else b::freeze_barrier_observer();
  const auto s = b::finish_barrier_observer(list.Get(), contract(pointers));
  if (mode == "--call-limit" || mode == "--batch-limit" || mode == "--description-limit" || mode == "--expiry") {
    check(!s.coverage_complete && (mode == "--expiry" ? s.expired : s.overflow), "live bound latches capture off");
    check(s.calls <= b::kBarrierCallCapacity && s.descriptions <= b::kBarrierDescriptionCapacity, "fixed storage bounds");
    for (auto i : role_indices) check(!s.resources[i].declaration_available, "incomplete live capture rejects conclusions");
  } else {
    check(s.coverage_complete && s.resets == 1 && !s.closed && s.matching_calls == 1 && s.descriptions == 4,
          "real API call captured in new recording without driver work added");
    for (auto i : role_indices) if (!queue_test || i != 3)
      check(s.resources[i].expected_state_matches, "actual barrier maps to Evaluate input");
    if (queue_test) {
      const auto &t = s.depth_timeline;
      check(!s.resources[3].declaration_available && t.matches == 1 && t.retained == 1 &&
            t.recent[0].command == reinterpret_cast<std::uintptr_t>(producer.Get()), "depth retained from other recording");
      check(!t.truncated && !t.malformed && t.queues_retained == 4 && t.list_count == 1 &&
            t.submitted_lists[0] == reinterpret_cast<std::uintptr_t>(producer.Get()), "real Execute, Signal and Wait captured");
      check(t.queue_events[0].kind == b::BarrierCallKind::execute && t.queue_events[0].count == 1 &&
            t.queue_events[1].kind == b::BarrierCallKind::signal && t.queue_events[1].fence_value == 7 &&
            t.queue_events[2].kind == b::BarrierCallKind::wait && t.queue_events[2].fence_value == 7 &&
            t.queue_events[3].kind == b::BarrierCallKind::signal && t.queue_events[3].fence_value == 8,
            "queue operations and fence values retained unchanged");
      for (unsigned i = 0; i < t.queues_retained; ++i) {
        check(t.queue_events[i].queue == reinterpret_cast<std::uintptr_t>(queue.Get()) &&
              t.queue_events[i].result == S_OK, "queue identity and result retained");
        if (i) check(t.queue_events[i].fence == reinterpret_cast<std::uintptr_t>(fence.Get()), "fence identity retained");
        check(t.queue_events[i].entry_order < t.queue_events[i].return_order, "entry and return ordered");
        if (i) check(t.queue_events[i-1].return_order < t.queue_events[i].entry_order, "serial fixture queue order retained");
      }
    } else check(s.resources[3].recent[0].thread == GetCurrentThreadId(), "callback thread correlation");
  }
  hr(list->Close(), "close after capture");
  if (continuation) {
    const auto before = journal.read();
    check(before.valid && before.flags == 0 && before.pending == 0, "complete checkpoint journal");
    hr(producer->Reset(producer_allocator.Get(), nullptr), "reset previously submitted producer after checkpoint");
    const auto copy = transition(pointers[3], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    producer->ResourceBarrier(1, &copy);
    const auto read = transition(pointers[3], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    producer->ResourceBarrier(1, &read);
    hr(producer->Close(), "close current producer recording");
    ID3D12CommandList *current[] = {producer.Get(), list.Get()};
    queue->ExecuteCommandLists(2, current);
    hr(queue->Signal(fence.Get(), 9), "post-checkpoint signal");
    hr(queue->Wait(fence.Get(), 9), "post-checkpoint wait");
    hr(queue->Signal(fence.Get(), 10), "fixture final signal");
    const auto done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    check(done != nullptr, "post-checkpoint completion event");
    hr(fence->SetEventOnCompletion(10, done), "fixture completion registration");
    const auto waited = WaitForSingleObject(done, 5000); CloseHandle(done);
    check(waited == WAIT_OBJECT_0, "submitted fixture work completes before destruction");
    if (mode == "--completion-drain") {
      ComPtr<ID3D12CommandAllocator> spare_allocator; ComPtr<ID3D12GraphicsCommandList> spare;
      hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&spare_allocator)), "spare allocator");
      hr(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, spare_allocator.Get(), nullptr, IID_PPV_ARGS(&spare)), "spare list");
      std::array<HANDLE, 4> events{};
      for (auto &event : events) { event = CreateEventW(nullptr, TRUE, FALSE, nullptr); check(event != nullptr, "owned pause event"); }
      b::pause_owned_observer(b::BarrierCallKind::fence_completed, events[0], events[1]);
      b::pause_owned_observer(b::BarrierCallKind::barrier, events[2], events[3]);
      std::uint64_t result{};
      std::thread query([&] { result = fence->GetCompletedValue(); });
      const auto query_entered = WaitForSingleObject(events[0], 2000);
      std::thread barrier([&] { spare->ResourceBarrier(0, nullptr); });
      const auto barrier_entered = WaitForSingleObject(events[2], 2000);
      SetEvent(events[1]); query.join();
      const auto draining = journal.read();
      SetEvent(events[3]); barrier.join();
      b::pause_owned_observer(b::BarrierCallKind::fence_completed, nullptr, nullptr);
      b::pause_owned_observer(b::BarrierCallKind::barrier, nullptr, nullptr);
      for (auto event : events) CloseHandle(event);
      hr(spare->Close(), "close unrelated owned list after drain");
      check(query_entered == WAIT_OBJECT_0 && barrier_entered == WAIT_OBJECT_0 && result == 10 &&
          draining.valid && draining.state == 3 && !draining.drained && draining.pending == 1,
          "admission closes while later callback remains in flight; no premature immutable terminal");
    }
    SetLastError(0x6789);
    check(fence->GetCompletedValue() == 10 && GetLastError() == 0x6789, "natural completed-value query preserves return and LastError");
    if (completion) {
      const auto stopped = journal.read();
      check(stopped.valid && stopped.drained && stopped.state == 3 && !stopped.pending && !stopped.flags &&
          stopped.termination.reason == b::JournalStopReason::completion &&
          stopped.termination.completion.status == b::CompletionStatus::observed &&
          stopped.termination.completion.value == 9 && stopped.termination.frontier == stopped.completed,
          "bound signal closes admission and publishes drained immutable termination");
      hr(fence->Signal(20), "later CPU signal after recorder closes");
      check(fence->GetCompletedValue() == 20, "stopped observer remains transparent");
      b::freeze_barrier_observer();
      const auto later = journal.read();
      check(later.valid && later.completed == stopped.completed && later.termination.order == stopped.termination.order,
          "later callbacks and explicit freeze cannot rewrite terminal evidence");
      check(MH_Uninitialize() == MH_OK, "completion fixture hooks removed after GPU drain");
      return;
    }
    hr(fence->Signal(20), "owned CPU signal after GPU work completed");
    check(fence->GetCompletedValue() == 20, "CPU value is distinguishable from GPU signal");
    hr(fence->Signal(2), "owned CPU rewind");
    check(fence->GetCompletedValue() == 2, "rewind retained without assuming monotonic completion");
    const auto after = journal.read();
    check(after.valid && !after.flags && !after.pending && after.completed == before.completed + 13,
          "Reset, two depth transitions, Close, Execute, Signal, Wait, Signal retained beyond Evaluate");
    check(after.json.find("\"before\":1024,\"after\":64") != std::string::npos, "late depth read declaration retained");
    const auto unchanged = b::finish_barrier_observer(list.Get(), contract(pointers));
    check(unchanged.calls == s.calls && unchanged.descriptions == s.descriptions && unchanged.closed == s.closed &&
          unchanged.epoch_sequence == s.epoch_sequence && unchanged.coverage_complete, "first-Evaluate prefix unchanged by later calls");
    check(after.json.find("\"value\":20") != std::string::npos &&
          after.json.find("\"kind\":7") != std::string::npos && after.json.find("\"kind\":8") != std::string::npos,
          "natural queries and CPU signals retained in append-only journal");
    hr(list->Reset(allocator.Get(), nullptr), "new recording after first Evaluate and completed fixture work");
    b::checkpoint_barrier_observer(list.Get(), 1);
    const auto second = b::finish_barrier_observer(list.Get(), contract(pointers));
    check(second.coverage_complete && !second.closed && second.epoch_sequence > s.epoch_sequence, "second Evaluate uses its own Reset generation");
    b::checkpoint_barrier_observer(producer.Get(), 2);
    const auto third = b::finish_barrier_observer(producer.Get(), contract(pointers));
    const auto markers = journal.read();
    check(third.command != second.command && third.closed && markers.checkpoints[2].command == third.command &&
          markers.checkpoints[1].prefix > markers.checkpoints[0].prefix &&
          markers.checkpoints[2].order > markers.checkpoints[1].order &&
          markers.checkpoints[0].order == before.checkpoints[0].order, "rotating command identity gets independent immutable marker; closed recording rejected");
    hr(list->Close(), "close fixture recording");
    b::freeze_barrier_observer();
    const auto stopped = journal.read();
    check(stopped.valid && stopped.state == 3 && !stopped.flags, "explicit stop retains completed journal");
    // No callbacks after stop should add observations.
    hr(list->Reset(allocator.Get(), nullptr), "quiescent reset after stop");
    hr(list->Close(), "quiescent close after stop");
    check(journal.read().completed == stopped.completed, "stopped journal admits no new calls");
  }
  // The queue fixture submits one producer barrier and waits for its fence.
  // The Evaluate list is never submitted. Remove hooks only while quiescent.
  check(MH_Uninitialize() == MH_OK,
        "clean quiescent hook removal");
}
} // namespace
int main(int argc, char **argv) {
  try {
    replay_tests(); live_test(argc == 2 ? std::string_view(argv[1]) : "");
    std::cout << "Barrier history and real D3D12 observer checks passed; observer adds no GPU commands.\n";
    return 0;
  } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
