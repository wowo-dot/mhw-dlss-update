// full_post: the inputs' upscale slot at execution (full_post.inl records it;
// full_post_inputs.hpp records the upscale). Included before the copy hooks.
thread_local unsigned full_slot_scope = UINT_MAX, full_slot_calls{};
thread_local std::uintptr_t full_slot_state{};
// The game's barrier plan for the frame executing (sub_14229CEE0): its packets
// in execution order (renderer +0x7af18, count +0x7af28), the epoch (+0x1f208,
// the stamp's + 1). Each packet holds a list of state nodes (packet +8; node
// +0 next, +8 backing, +0x18/+0x1a native before/after), turned into barriers
// and flushed by its executor just before its command.
struct PlannerView {
    std::uintptr_t array{};
    unsigned count{};
    std::uint64_t epoch{};
};
bool planner_view(const Plan &p, PlannerView &v) noexcept {
    return field(cfg.renderer, layout::renderer::barrier_plan, v.array) &&
           field(cfg.renderer, layout::renderer::barrier_plan_count, v.count) &&
           field(cfg.renderer, layout::renderer::epoch, v.epoch) && v.epoch == p.stamp.epoch + 1 && v.array &&
           v.count && v.count <= 32768 && v.array <= UINTPTR_MAX - 16ULL * v.count;
}
// The positions of N packets, each present exactly once.
template <std::size_t N>
bool planner_find(const PlannerView &v, const std::array<std::uintptr_t, N> &packets,
                  std::array<unsigned, N> &at) noexcept {
    std::array<unsigned, N> seen{};
    at.fill(UINT_MAX);
    std::array<std::array<std::uint64_t, 2>, 64> block{};
    for (unsigned begin = 0; begin < v.count; begin += 64) {
        const auto n = (std::min)(64U, v.count - begin);
        if (!pb::copy_memory(v.array + 16ULL * begin, block.data(), n * 16)) return false;
        for (unsigned i = 0; i < n; ++i)
            for (unsigned k = 0; k < N; ++k)
                if (block[i][1] == packets[k]) {
                    at[k] = begin + i;
                    ++seen[k];
                }
    }
    for (unsigned k = 0; k < N; ++k)
        if (seen[k] != 1) return false;
    return true;
}
// The last node of `o` in one packet's lists: the parent's, then the child
// draw's when an opcode-10 copy executes one. 0 none, 1 found, 2 unreadable.
unsigned planner_packet_state(std::uintptr_t packet, const tb::Owner &o, unsigned &native) noexcept {
    unsigned raw{}, found{};
    std::uintptr_t head{};
    const auto walk = [&](std::uintptr_t n) {
        unsigned visited{};
        while (n) {
            std::uintptr_t next{}, backing{}, resource{};
            std::uint16_t after{};
            if (++visited > 128 || !field(n, layout::state_node::next, next) ||
                !field(n, layout::state_node::backing, backing) ||
                !field(backing, layout::backing::resource, resource) || !field(n, layout::state_node::after, after))
                return false;
            if (backing == o.backing || resource == o.resource) {
                if (backing != o.backing || resource != o.resource) return false;
                native = after;
                found = 1;
            }
            n = next;
        }
        return true;
    };
    if (!field(packet, 0, raw) || !field(packet, layout::packet::states, head) || !walk(head)) return 2;
    if ((raw & 31) == 10) {
        tb::CopyExecution c;
        std::uintptr_t child{};
        if (!tb::read_copy_execution(reader, packet, c)) return 2;
        if (c.path == tb::CopyExecutionPath::child_draw &&
            (!field(c.child, layout::packet::states, child) || !walk(child)))
            return 2;
    }
    return found;
}
// `o`'s D3D12 state when packet `at` executes its command: the after-state of
// its last node at or before `at`; with none in the frame, the tracker's
// initial state (backing +0x70: +0x10 epoch, +0x18 initial, +0x20 used epoch,
// +0x28 initialized, +0x30 immediate epoch). A tracker of another epoch, or
// one with an immediate (unplanned) state change, refuses.
bool planner_state(const PlannerView &v, unsigned at, const tb::Owner &o, unsigned &state) noexcept {
    std::uintptr_t tracker{};
    std::uint64_t epoch{}, used{}, immediate{};
    std::uint16_t initial{};
    unsigned char initialized{};
    if (at >= v.count || !field(o.backing, layout::backing::tracker, tracker) || !tracker ||
        !field(tracker, layout::state_tracker::epoch, epoch) || epoch != v.epoch ||
        !field(tracker, layout::state_tracker::initial, initial) ||
        !field(tracker, layout::state_tracker::used, used) || used != v.epoch ||
        !field(tracker, layout::state_tracker::initialized, initialized) || initialized != 1 ||
        !field(tracker, layout::state_tracker::immediate, immediate) || immediate)
        return false;
    const unsigned floor = at > 1024 ? at - 1024 : 0;
    for (unsigned i = at + 1; i-- > floor;) {
        std::uintptr_t packet{};
        unsigned native{};
        if (!field(v.array + 16ULL * i, layout::packet_entry::packet, packet) || !packet) return false;
        const auto found = planner_packet_state(packet, o, native);
        if (found == 2) return false;
        if (found == 1) {
            state = planner_d3d12_state(native);
            return true;
        }
    }
    if (floor) return false;
    state = planner_d3d12_state(initial);
    return true;
}
// The three inputs' states at the slot, from the plan (GBuffer5 in COPY_DEST,
// by the slot's own node). When TAA's consumer has already published, the
// plan's states at TAA must be the ones it proved. Shared by the slot and the
// late draws' depth restore, so both decide alike. 0, or the slot's skip
// reason: 4 no plan state, 5 a disagreement.
unsigned full_slot_states(const Plan &p, const FullSlot &q, PlannerView &v, std::array<unsigned, 2> &at,
                          std::array<unsigned, 3> &states, bool count) noexcept {
    if (!planner_view(p, v) || !planner_find(v, std::array<std::uintptr_t, 2>{p.temporal.object, q.packet}, at) ||
        at[0] >= at[1])
        return 4;
    if (!planner_state(v, at[1], q.depth, states[0]) || !planner_state(v, at[1], q.gbuffer4, states[1]) ||
        !planner_state(v, at[1], q.gbuffer5, states[2]))
        return 4;
    if (states[2] != D3D12_RESOURCE_STATE_COPY_DEST) return 5;
    if (p.full_states_known) {
        std::array<unsigned, 2> taa{};
        if (p.full_depth_resource != q.depth.resource || p.full_gbuffer4_resource != q.gbuffer4.resource ||
            !planner_state(v, at[0], q.depth, taa[0]) || !planner_state(v, at[0], q.gbuffer4, taa[1]) ||
            taa[0] != p.full_depth_state || taa[1] != p.full_gbuffer4_state) {
            if (count) full_counts.state_mismatch = {p.full_depth_state, taa[0], p.full_gbuffer4_state, taa[1]};
            return 5;
        }
        if (count) ++full_counts.states_checked;
    } else if (count)
        ++full_counts.states_planned;
    return 0;
}
// The copy executor (0x259c650) enters a registered slot packet. The plan's
// hand-off takes the slot's recording here, so the frame completes whether or
// not the upscale is recorded. Returns the slot, or UINT_MAX.
unsigned full_slot_before(const pb::Frame &f) noexcept {
    Lock lock;
    unsigned found = UINT_MAX;
    for (unsigned i = 0; i < full_slots.size(); ++i)
        if (full_slots[i].packet && full_slots[i].packet == f.gpr[3] && !full_slots[i].executed) {
            found = i;
            break;
        }
    if (found == UINT_MAX) return found;
    // A slot whose frame never executed leaves its address to a later packet:
    // only the slot's own copy (TAA's output into GBuffer5, 1x1) is taken.
    {
        const auto &q = full_slots[found];
        std::uintptr_t destination{}, source{};
        std::array<unsigned, 4> rect{};
        if (!field(q.packet, layout::region_copy_packet::destination, destination) ||
            destination != q.gbuffer5.object || !field(q.packet, layout::region_copy_packet::source, source) ||
            source != q.source.object || !field(q.packet, layout::region_copy_packet::source_rect, rect) ||
            rect != std::array<unsigned, 4>{0, 0, 1, 1}) {
            full_slots[found] = {};
            ++full_counts.slot_stale;
            return UINT_MAX;
        }
    }
    auto &q = full_slots[found];
    q.executed = true;
    full_slot_calls = 0;
    full_slot_state = 0;
    std::uintptr_t state{}, list{};
    if (field(f.gpr[1], layout::executor::recording, state) && state &&
        field(state, layout::recording::command, list) && list)
        full_slot_state = state;
    auto &p = plans[q.frame % plans.size()];
    if (p.prepared && p.stamp.frame == q.frame && p.full_slot == q.packet && !p.full_slot_done) {
        temporal_present::Bindings b;
        if (!full_slot_state || !obs::inspect(reinterpret_cast<ID3D12GraphicsCommandList *>(list), b) || !b.known ||
            !b.valid || !b.generation ||
            !track_recording(p, reinterpret_cast<ID3D12GraphicsCommandList *>(list), b.generation, true) ||
            !retain_recording(p, reinterpret_cast<ID3D12GraphicsCommandList *>(list), b.generation) ||
            p.handoff.record(2, q.packet, {list, b.generation}) != FrameHandoff::Result::ok) {
            fail(15);
            return found;
        }
        p.full_slot_done = true;
        completed(p);
    }
    return found;
}
void full_slot_after() noexcept {
    if (full_slot_calls != 1) ++full_counts.slot_skips[0];
}
// The copy executor enters a registered DLAA copy (v0.25.8). When its frame's
// identity plan is prepared and takes it as the final copy, it runs as at DLAA
// (copy_before). Otherwise its copy is dropped (full_final_copy). Returns the
// entry, or UINT_MAX.
thread_local unsigned full_final_scope = UINT_MAX, full_final_calls{};
unsigned full_final_before(const pb::Frame &f) noexcept {
    Lock lock;
    unsigned found = UINT_MAX;
    for (unsigned i = 0; i < full_finals.size(); ++i)
        if (full_finals[i].packet && full_finals[i].packet == f.gpr[3] && !full_finals[i].executed) {
            found = i;
            break;
        }
    if (found == UINT_MAX) return found;
    // A packet address a later packet reuses is not the DLAA copy.
    auto &r = full_finals[found];
    std::uintptr_t destination{}, source{};
    std::array<unsigned, 4> rect{};
    if (!field(r.packet, layout::region_copy_packet::destination, destination) || destination != r.destination.object ||
        !field(r.packet, layout::region_copy_packet::source, source) || source != r.source.object ||
        !field(r.packet, layout::region_copy_packet::source_rect, rect) ||
        rect != std::array<unsigned, 4>{0, 0, width, height}) {
        r = {};
        ++full_counts.final_stale;
        return UINT_MAX;
    }
    r.executed = true;
    const auto &p = plans[r.frame % plans.size()];
    if (p.prepared && p.stamp.frame == r.frame && p.identity && p.final_copy == r.packet) return UINT_MAX;
    full_final_calls = 0;
    return found;
}
// The unplanned DLAA copy's one copy (tBaseMap's texture into the view's
// attachment) is dropped; any other call is forwarded.
void full_final_copy(ID3D12GraphicsCommandList *list, const D3D12_TEXTURE_COPY_LOCATION *destination, UINT x, UINT y,
                     UINT z, const D3D12_TEXTURE_COPY_LOCATION *source, const D3D12_BOX *box,
                     std::uintptr_t caller) noexcept {
    bool drop{};
    {
        Lock lock;
        const auto &r = full_finals[full_final_scope];
        D3D12_TEXTURE_COPY_LOCATION d{}, s{};
        drop = ++full_final_calls == 1 && caller == cfg.image + 0x259caf0 && !x && !y && !z &&
               pb::copy_memory(reinterpret_cast<std::uintptr_t>(destination), &d, sizeof(d)) &&
               pb::copy_memory(reinterpret_cast<std::uintptr_t>(source), &s, sizeof(s)) &&
               reinterpret_cast<std::uintptr_t>(d.pResource) == r.destination.resource &&
               reinterpret_cast<std::uintptr_t>(s.pResource) == r.source.resource;
        ++(drop ? full_counts.fallbacks : full_counts.fallback_unverified);
    }
    if (!drop) reinterpret_cast<CopyFn>(originals[2])(list, destination, x, y, z, source, box);
}
// The slot's copy (TAA's output into GBuffer5, 1x1) never runs: it is replaced
// by the upscale, or by nothing. The three inputs' states are the plan's at the
// slot (GBuffer5 COPY_DEST, by the packet itself). When TAA's consumer has
// already run, the plan's states at TAA must be the ones it proved.
void full_slot_copy(ID3D12GraphicsCommandList *list, const D3D12_TEXTURE_COPY_LOCATION *destination, UINT x, UINT y,
                    UINT z, const D3D12_TEXTURE_COPY_LOCATION *source, const D3D12_BOX *box,
                    std::uintptr_t caller) noexcept {
    Lock lock;
    const auto &q = full_slots[full_slot_scope];
    const auto skip = [&](unsigned reason) { ++full_counts.slot_skips[reason]; };
    D3D12_TEXTURE_COPY_LOCATION d{}, s{};
    D3D12_BOX b{};
    std::uintptr_t packet{}, command{};
    if (++full_slot_calls != 1 || caller != cfg.image + 0x259caf0 || x || y || z || !full_slot_state ||
        !field(full_slot_state, layout::recording::packet, packet) || packet != q.packet ||
        !field(full_slot_state, layout::recording::command, command) ||
        command != reinterpret_cast<std::uintptr_t>(list) ||
        !pb::copy_memory(reinterpret_cast<std::uintptr_t>(destination), &d, sizeof(d)) ||
        !pb::copy_memory(reinterpret_cast<std::uintptr_t>(source), &s, sizeof(s)) ||
        d.Type != D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX || s.Type != d.Type || d.SubresourceIndex ||
        s.SubresourceIndex || reinterpret_cast<std::uintptr_t>(d.pResource) != q.gbuffer5.resource ||
        reinterpret_cast<std::uintptr_t>(s.pResource) != q.source.resource || !box ||
        !pb::copy_memory(reinterpret_cast<std::uintptr_t>(box), &b, sizeof(b)) || b.left || b.top || b.front ||
        b.right != 1 || b.bottom != 1 || b.back != 1) {
        skip(1);
        return;
    }
    if (!q.valid || fault.load()) {
        skip(2);
        return;
    }
    auto &p = plans[q.frame % plans.size()];
    if (!p.prepared || p.stamp.frame != q.frame || p.full_slot != q.packet || !p.full_slot_done) {
        skip(3);
        return;
    }
    // A late restore recorded on an earlier worker relies on the depth being
    // kept here (full_restore): when it is not, that restore was stale.
    const auto unkept = [&](unsigned reason) {
        skip(reason);
        if (p.full_restore_recorded) ++full_counts.restore_unsafe;
    };
    PlannerView v;
    std::array<unsigned, 2> at{};
    std::array<unsigned, 3> states{};
    if (const auto why = full_slot_states(p, q, v, at, states, true)) {
        unkept(why);
        return;
    }
    // Nothing of ours may run under an active predicate.
    obs::Predicate predicate;
    if (!obs::predicate_current(list, predicate) || !predicate.known || predicate.buffer) {
        unkept(6);
        return;
    }
    const std::array<FullPostInput, 3> inputs{
        {{reinterpret_cast<ID3D12Resource *>(q.depth.resource), static_cast<D3D12_RESOURCE_STATES>(states[0])},
         {reinterpret_cast<ID3D12Resource *>(q.gbuffer4.resource), static_cast<D3D12_RESOURCE_STATES>(states[1])},
         {reinterpret_cast<ID3D12Resource *>(q.gbuffer5.resource), D3D12_RESOURCE_STATE_COPY_DEST}}};
    // Without a known binding snapshot there is no upscale, but the depth's
    // render region is still kept (copies only) for the late restore.
    temporal_present::Bindings roots;
    if (!obs::inspect(list, roots) || !roots.known || !roots.valid || !roots.generation) {
        injecting = true;
        const bool kept = full_inputs.keep_depth(list, inputs[0], render_w, render_h);
        injecting = false;
        if (kept) {
            p.full_kept = true;
            skip(6);
        } else
            unkept(6);
        return;
    }
    injecting = true;
    const bool recorded = full_inputs.record(list, inputs, render_w, render_h, roots);
    injecting = false;
    if (!recorded) {
        unkept(7);
        return;
    }
    p.full_kept = true;
    // Everything but an unset pipeline is the game's again.
    temporal_present::Bindings after;
    if (!obs::inspect(list, after) || !(after.compute == roots.compute) || !(after.graphics == roots.graphics) ||
        after.heaps != roots.heaps || after.heap_count != roots.heap_count ||
        (roots.pipeline && after.pipeline != roots.pipeline)) {
        fail(15);
        return;
    }
    ++full_counts.upscales;
}
// The late draws (the hunter silhouettes, silhouette_route.inl) read the depth
// at the render size, after the post chain. Before the first of them, the
// depth's render region is put back from the slot's kept copy. That copy runs
// first on the GPU (key order), whatever the workers' order on the CPU; this
// restore relies on the slot keeping it, which it does whenever its states are
// derivable (the same derivation decides here). At the first late draw in key
// order (GPU order), once per frame, under the lock.
void full_restore(Plan &p, ID3D12GraphicsCommandList *list, std::uintptr_t late_packet) noexcept {
    if (!p.full_slot || p.full_restore_tried) return;
    p.full_restore_tried = true;
    const auto skip = [&](unsigned reason) { ++full_counts.restore_skips[reason]; };
    const auto &q = full_slots[p.stamp.frame % full_slots.size()];
    if (q.packet != p.full_slot || q.frame != p.stamp.frame || !q.valid) {
        skip(0);
        return;
    }
    PlannerView v;
    std::array<unsigned, 2> at{};
    std::array<unsigned, 3> states{};
    std::array<unsigned, 1> late{};
    unsigned depth{};
    // A slot that already ran without keeping the depth leaves nothing to put back.
    if (full_slots[p.stamp.frame % full_slots.size()].executed && !p.full_kept) {
        skip(1);
        return;
    }
    if (full_slot_states(p, q, v, at, states, false)) {
        skip(1);
        return;
    }
    if (!planner_find(v, std::array<std::uintptr_t, 1>{late_packet}, late) || late[0] <= at[1] ||
        !planner_state(v, late[0], q.depth, depth)) {
        skip(2);
        return;
    }
    obs::Predicate predicate;
    if (!obs::predicate_current(list, predicate) || !predicate.known || predicate.buffer) {
        skip(3);
        return;
    }
    injecting = true;
    const bool restored = full_inputs.restore_depth(
        list, {reinterpret_cast<ID3D12Resource *>(q.depth.resource), static_cast<D3D12_RESOURCE_STATES>(depth)},
        render_w, render_h);
    injecting = false;
    if (!restored) {
        skip(4);
        return;
    }
    p.full_restore_recorded = true;
    ++full_counts.depth_restores;
}
