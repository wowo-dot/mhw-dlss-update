// The D3D12 hooks: barriers, draws and copies inside a claimed consumer, and
// the command lists' lifecycle (Reset, Close, ExecuteCommandLists) that
// orders and retires the private recordings.
#include "runtime_state.hpp"
#include "../game_memory/layout.hpp"

namespace mhw::continuous_dlaa::detail {
void WINAPI barrier_hook(ID3D12GraphicsCommandList *list, UINT n, const D3D12_RESOURCE_BARRIER *data) {
    perf::hook_timing::Scope timing(perf::hook_timing::barrier);
    Guard guard;
    {
        perf::hook_timing::ForwardScope forward(timing.sample);
        reinterpret_cast<BarrierFn>(originals[8])(list, n, data);
    }
    if (!injected) sr::barriers(list, n, data);
    if (injected || !current_kind || !accepting.load()) return;
    if (list != current_list || n > barriers.size() - barrier_count ||
        !pb::copy_memory(reinterpret_cast<std::uintptr_t>(data), barriers.data() + barrier_count, n * sizeof(*data))) {
        fail(11);
        return;
    }
    barrier_count += n;
}

using DrawFn = void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, UINT, UINT, UINT);
using IndexedFn = void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, UINT, UINT, INT, UINT);
void WINAPI indexed_hook(ID3D12GraphicsCommandList *list, UINT n, UINT instances, UINT first, INT base, UINT instance) {
    perf::hook_timing::Scope timing(perf::hook_timing::indexed);
    Guard guard;
    if (!sr::indexed_draw(list, n, instances, first, base, instance,
                          reinterpret_cast<std::uintptr_t>(_ReturnAddress()))) {
        perf::hook_timing::ForwardScope forward(timing.sample);
        reinterpret_cast<IndexedFn>(originals[12])(list, n, instances, first, base, instance);
    }
}
struct DepthDraw {
    std::array<std::uintptr_t, 4> bindings{};
    std::array<unsigned, 2> arguments{};
    std::array<unsigned, 4> rectangle{};
    std::uintptr_t packet{}, pipeline{};
    unsigned rejection{};
};
bool depth_draw_snapshot(DepthDraw &out) {
    const auto &d = depth_consumer;
    const auto nested = d.packet.nested;
    const auto gate = [&](unsigned reason, bool ok) {
        if (!ok) out.rejection = reason;
        return ok;
    };
    return gate(1, nested && reader.get(nested + layout::draw_packet::bindings, out.bindings) &&
                       reader.get(nested + layout::draw_packet::arguments, out.arguments)) &&
           gate(2, reader.get(current_recording + layout::recording::packet, out.packet) && out.packet == nested) &&
           gate(3, reader.get(current_recording + layout::recording::pipeline, out.pipeline) && out.pipeline) &&
           gate(4, reader.get(out.bindings[0] + layout::target_set::rectangle, out.rectangle) &&
                       out.rectangle == std::array<unsigned, 4>{0, 0, render_width(), render_height()}) &&
           gate(5, d.packet.rectangle == std::array<unsigned, 6>{0, 0, render_width(), render_height(), 0, 0}) &&
           gate(6, out.arguments[0] > 0 && out.arguments[0] <= 6);
}
void WINAPI draw_hook(ID3D12GraphicsCommandList *list, UINT n, UINT instances, UINT first, UINT instance) {
    perf::hook_timing::Scope timing(perf::hook_timing::draw);
    Guard guard;
    const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    DepthDraw before;
    if (!injected && current_kind == 2 && accepting.load()) {
        unsigned reason{};
        if (list != current_list || caller != cfg.image + 0x259d72e)
            reason = 1;
        else if (++copy_count != 1)
            reason = 2;
        else if (!depth_draw_snapshot(before))
            reason = 3;
        else if (before.arguments != std::array<unsigned, 2>{n, first} || instances != 1 || instance)
            reason = 4;
        else if (!dr::draw_states(std::span(barriers).first(barrier_count), depth_consumer.source.resource,
                                  depth_consumer.destination.resource))
            reason = 5;
        note_depth(4, reason, depth_consumer,
                   {reinterpret_cast<std::uintptr_t>(list), caller, copy_count, n, instances, first, instance,
                    before.rejection, before.packet, before.pipeline, before.rectangle[0], before.rectangle[1],
                    before.rectangle[2], before.rectangle[3], before.arguments[0], before.arguments[1]});
        if (reason == 5) {
            ScaledDepthTrace::Values values{};
            values[0] = barrier_count;
            for (const auto &b : std::span(barriers).first(barrier_count)) {
                if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING) {
                    ++values[11];
                    continue;
                }
                if (b.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) continue;
                const auto resource = reinterpret_cast<std::uintptr_t>(b.Transition.pResource);
                const unsigned at = resource == depth_consumer.source.resource        ? 1U
                                    : resource == depth_consumer.destination.resource ? 6U
                                                                                      : 0U;
                if (at) {
                    ++values[at];
                    values[at + 1] = b.Flags;
                    values[at + 2] = b.Transition.Subresource;
                    values[at + 3] = b.Transition.StateBefore;
                    values[at + 4] = b.Transition.StateAfter;
                }
            }
            note_depth(6, 0, depth_consumer, values);
        }
    }
    {
        perf::hook_timing::ForwardScope forward(timing.sample);
        reinterpret_cast<DrawFn>(originals[11])(list, n, instances, first, instance);
    }
}
// Only forwards. Its chain gives the private copies (copy_input, the depth
// copy at the TAA dispatch) their CopyResource continuation, past the hooks
// ahead of this one, and the hook timing its copy count. Depth used to be
// leased at the game's own copy; the runtime copies it at the dispatch at
// every render scale since v0.19.3 (dlaa-identity-path.md, game-70/74/75).
void WINAPI copy_hook(ID3D12GraphicsCommandList *list, ID3D12Resource *destination, ID3D12Resource *source) {
    perf::hook_timing::Scope timing(perf::hook_timing::copy);
    Guard guard;
    perf::hook_timing::ForwardScope forward(timing.sample);
    reinterpret_cast<CopyFn>(originals[10])(list, destination, source);
}
// The game creates and releases command lists while it runs (the
// nDraw::ExternalRegion proxies each own one; game-86 filled the 128 slots
// over a few minutes), so the registry cannot pin every list it has seen.
// A list whose last reference is the registry's own was released by its
// creator: nothing can Reset, record or submit it again. Its generation is
// retired in every per-list table as its next Reset would retire it, the
// observer forgets it, and the slot is freed. A list with a recording of ours
// not yet submitted, or pinned elsewhere (the stream route's lifetime, the
// spatial output), has further references and stays. Under Lock.
unsigned prune_released() {
    unsigned pruned{};
    for (unsigned i = 0; i < list_count && !fault.load(); ++i) {
        auto &l = lists[i];
        auto *p = l.value.Get();
        if (!p || lease.list == p) continue;
        p->AddRef();
        if (p->Release() != 1) continue;
        if (recording_pins.pending(p, l.generation) || unfiltered_lifetime.pending(p, l.generation)) continue;
        if (!sr::reset(p, l.generation)) {
            fail(42);
            break;
        }
        if (!recording_pins.reset(p, l.generation)) {
            fail(16);
            break;
        }
        if (!unfiltered_lifetime.reset(p, l.generation)) {
            fail(35);
            break;
        }
        frame_generation::game::reset(reinterpret_cast<std::uintptr_t>(p), l.generation);
        obs::forget(p);
        l = List{};
        ++pruned;
    }
    while (list_count && !lists[list_count - 1].value) --list_count;
    lists_pruned += pruned;
    return pruned;
}
// A free slot for a newly seen list: an empty one, a new one, or one freed by
// pruning the released lists. Under Lock.
List *list_slot() {
    const auto empty = []() -> List * {
        for (unsigned i = 0; i < list_count; ++i)
            if (!lists[i].value) return &lists[i];
        return list_count < lists.size() ? &lists[list_count++] : nullptr;
    };
    auto *l = empty();
    if (!l && prune_released()) l = empty();
    return l;
}
HRESULT WINAPI reset_hook(ID3D12GraphicsCommandList *list, ID3D12CommandAllocator *allocator,
                          ID3D12PipelineState *pipeline) {
    perf::hook_timing::Scope timing(perf::hook_timing::reset_close);
    Guard guard;
    HRESULT result;
    {
        perf::hook_timing::ForwardScope forward(timing.sample);
        result = reinterpret_cast<HRESULT(WINAPI *)(ID3D12GraphicsCommandList *, ID3D12CommandAllocator *,
                                                    ID3D12PipelineState *)>(originals[5])(list, allocator, pipeline);
    }
    if (FAILED(result) || !child(list) || list->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT) return result;
    Lock lock;
    auto *l = find(list);
    if (!l) {
        if (!accepting.load()) return result;
        l = list_slot();
        if (!l) {
            fail(15);
            return result;
        }
        *l = List{};
        l->value = list;
        if (const auto n = live_lists(); n > lists_peak) lists_peak = n;
    }
    if (lease.list == list) {
        if (!l->submitted) fail(16);
        lease = {};
    }
    if (!sr::reset(list, l->generation)) fail(42);
    if (fence) sr::collect(fence->GetCompletedValue());
    if (!recording_pins.reset(list, l->generation)) fail(16);
    if (!unfiltered_lifetime.reset(list, l->generation)) fail(35);
    if (fence && !fault.load()) {
        recording_pins.collect(fence->GetCompletedValue());
        unfiltered_lifetime.collect();
    }
    frame_generation::game::reset(reinterpret_cast<std::uintptr_t>(list), l->generation);
    l->generation = ++generation;
    l->closed = false;
    l->submitted = false;
    obs::reset(list, l->generation, pipeline);
    return result;
}
HRESULT WINAPI close_hook(ID3D12GraphicsCommandList *list) {
    perf::hook_timing::Scope close_timing(perf::hook_timing::reset_close);
    Guard guard;
    // FG brightness capture: the list's last commands, before the native Close.
    if (!injected && frame_generation::game::capture_wanted()) {
        bool open = false;
        {
            Lock lock;
            const auto *l = find(list);
            open = l && !l->closed;
        }
        temporal_present::Bindings roots;
        if (open && obs::inspect(list, roots)) {
            injected = true;
            frame_generation::game::closing(list, roots);
            injected = false;
        }
    }
    HRESULT result;
    {
        perf::hook_timing::ForwardScope forward(close_timing.sample);
        result = reinterpret_cast<HRESULT(WINAPI *)(ID3D12GraphicsCommandList *)>(originals[6])(list);
    }
    Lock lock;
    if (auto *l = find(list)) {
        if (SUCCEEDED(result))
            l->closed = true;
        else if (lease.list == list || unfiltered_lifetime.tracked(list, l->generation))
            fail(17);
    }
    return result;
}
void WINAPI execute_hook(ID3D12CommandQueue *queue, UINT count, ID3D12CommandList *const *submitted) {
    // Keep native Execute and both completion signals in the same CPU order.
    // Reserving a fence value under Lock alone cannot prevent two submitters
    // from publishing the larger value first after leaving that lock.
    perf::hook_timing::Scope execute_timing(perf::hook_timing::execute);
    SubmissionGuard submission_guard;
    Guard guard;
    std::array<ID3D12CommandList *, 128> copy{};
    bool selected = false, signal_requested = false;
    ID3D12GraphicsCommandList *selected_list{};
    std::uint64_t value{};
    UnfilteredLifetime::Submission unfiltered_submission;
    bool unfiltered_valid = false;
    std::array<std::array<std::uint64_t, 2>, 128> observed_lists{};
    bool observed_batch = false, route_selected = false;
    if (count <= copy.size() && (!count || pb::copy_memory(reinterpret_cast<std::uintptr_t>(submitted), copy.data(),
                                                           count * sizeof(copy[0])))) {
        Lock lock;
        unsigned matches{};
        observed_batch = true;
        for (unsigned i = 0; i < count; ++i) {
            auto *l = find(reinterpret_cast<ID3D12GraphicsCommandList *>(copy[i]));
            observed_lists[i] = {reinterpret_cast<std::uintptr_t>(copy[i]), l ? l->generation : 0};
            if (!l || l->injected_generation != l->generation) continue;
            if (l->submitted || !l->closed || lease.list != l->value.Get() || lease.generation != l->generation) {
                fail(18);
                continue;
            }
            ++matches;
            selected_list = l->value.Get();
        }
        route_selected = sr::submission_needed(std::span(observed_lists).first(count));
        bool route_closed = true;
        if (route_selected)
            for (unsigned i = 0; i < count; ++i)
                if (sr::submission_needed(std::span(observed_lists).subspan(i, 1))) {
                    const auto *l = find(reinterpret_cast<ID3D12GraphicsCommandList *>(copy[i]));
                    route_closed = route_closed && l && l->closed;
                }
        const bool owned_batch = matches || route_selected;
        if (owned_batch) {
            const bool queue_valid =
                matches <= 1 && child(queue) && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT && route_closed;
            selected = queue_valid && matches == 1;
            if (!queue_valid) {
                if (route_selected) sr::reject_submission(std::span(observed_lists).first(count));
                fail(19);
            } else {
                // Only one private recording may await submission. If the game
                // changes DIRECT queues, explicitly chain our private work.
                bool ordered = true;
                if (last_queue && last_queue.Get() != queue && FAILED(queue->Wait(fence.Get(), signal_value))) {
                    selected = false;
                    ordered = false;
                    fail(20);
                }
                if (ordered && route_selected &&
                    !sr::validate_submission(reinterpret_cast<std::uintptr_t>(queue),
                                             std::span(observed_lists).first(count), signal_value + 1)) {
                    sr::reject_submission(std::span(observed_lists).first(count));
                    selected = false;
                    ordered = false;
                    fail(43);
                }
                if (ordered) {
                    value = ++signal_value;
                    last_queue = queue;
                    signal_requested = true;
                    if (selected) find(selected_list)->submitted = true;
                    if (route_selected && !selected) ++route_only_signals;
                }
            }
        }
        const auto batch = std::span<ID3D12CommandList *const>(copy.data(), count);
        unfiltered_valid = unfiltered_lifetime.before(queue, batch, unfiltered_submission);
        if (!unfiltered_valid) fail(36);
    } else if (accepting.load())
        fail(21);
    {
        perf::hook_timing::ForwardScope forward(execute_timing.sample);
        reinterpret_cast<void(WINAPI *)(ID3D12CommandQueue *, UINT, ID3D12CommandList *const *)>(originals[7])(
            queue, count, submitted);
    }
    if (observed_batch)
        sr::observe_submission(reinterpret_cast<std::uintptr_t>(queue), GetTickCount64(),
                               std::span(observed_lists).first(count));
    if (observed_batch) {
        static std::atomic<std::uint64_t> fg_batches{};
        const auto fg_batch = ++fg_batches;
        for (unsigned i = 0; i < count; ++i)
            if (observed_lists[i][1])
                frame_generation::game::submitted(observed_lists[i][0], observed_lists[i][1], fg_batch);
    }
    if (signal_requested) {
        const auto result = queue->Signal(fence.Get(), value);
        Lock lock;
        if (FAILED(result)) {
            fail(22);
            return;
        }
        if (route_selected && !sr::submitted_batch(reinterpret_cast<std::uintptr_t>(queue),
                                                   std::span(observed_lists).first(count), value)) {
            fail(43);
            return;
        }
        if (selected) {
            auto *l = find(selected_list);
            if (!l || l->generation != lease.generation) {
                fail(23);
                return;
            }
            if (!recording_pins.submitted(selected_list, l->generation, value)) {
                fail(28);
                return;
            }
            l->signal = value;
            lease = {};
        }
    }
    if (unfiltered_valid) {
        Lock lock;
        if (!unfiltered_lifetime.after(queue, unfiltered_submission)) fail(37);
    }
}
} // namespace mhw::continuous_dlaa::detail
