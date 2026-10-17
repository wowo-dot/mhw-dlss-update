#include "observer.hpp"
#include "../perf/toggles.hpp"
#include "../perf/hook_timing.hpp"
#include "../perf/in_flight.hpp"
#include "../astera/astera_hooks.hpp"
#include <wrl/client.h>
#include <atomic>
#include <cstring>
#include <intrin.h>

namespace mhw::temporal_present::observer {
namespace {
constexpr unsigned raster_hooks = hook_count + 5, predicate_hooks = raster_hooks + 1, max_hooks = predicate_hooks + 1;
constexpr std::array<unsigned, max_hooks> methods{11, 25, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37,
                                                  38, 39, 40, 41, 42, 20, 21, 22, 44, 46, 55, 43};
// Entries are assigned under the process-wide lock. lists[i] identifies
// entries[i] until the owner reports the list released (forget()); the slot is
// then free for another list, and `epoch` invalidates threads' cached lookups.
struct Entry {
    SRWLOCK lock = SRWLOCK_INIT;
    Bindings state;
    RasterBindings raster;
    Predicate predicate;
};
std::array<Entry, list_limit> entries;
std::array<std::atomic<ID3D12GraphicsCommandList *>, list_limit> lists{};
std::array<std::uintptr_t, max_hooks> targets;
std::array<void *, max_hooks> originals;
std::array<bool, max_hooks> enabled;
// The detours run from install to finish;
// otherwise each call continues straight to AL's stored next method.
std::atomic<bool> chain_live{};
std::atomic<unsigned> entry_count{};
std::atomic<std::uint64_t> epoch{}, forgotten{};
// Callbacks in flight, without a locked instruction a call (perf/in_flight.hpp).
perf::InFlightCount pending;
std::atomic<unsigned> fault{};
std::atomic<bool> accepting{};
bool installed{}, drained{};
ViewportListener viewport_listener{};
std::atomic<std::uint64_t> viewport_notifications{};
SRWLOCK mutex = SRWLOCK_INIT;
struct Lock {
    Lock() { AcquireSRWLockExclusive(&mutex); }
    ~Lock() { ReleaseSRWLockExclusive(&mutex); }
};
void fail(unsigned f) noexcept {
    unsigned zero{};
    fault.compare_exchange_strong(zero, f);
    accepting.store(false, std::memory_order_release);
}
bool enter() noexcept {
    if (!accepting.load(std::memory_order_acquire)) return false;
    pending.add();
    if (!accepting.load(std::memory_order_acquire)) {
        pending.sub();
        return false;
    }
    return true;
}
// A recording thread uses one list for many calls. A cached lookup holds while
// no slot was freed since (epoch); a live list's own slot never changes.
thread_local ID3D12GraphicsCommandList *last_list{};
thread_local unsigned last_entry = UINT_MAX;
thread_local std::uint64_t last_epoch{};
Entry *find(ID3D12GraphicsCommandList *p) noexcept {
    if (!p) return nullptr;
    const auto e = epoch.load(std::memory_order_acquire);
    if (p == last_list && last_entry < list_limit && last_epoch == e) return &entries[last_entry];
    const auto n = entry_count.load(std::memory_order_acquire);
    perf::hook_timing::count(perf::hook_timing::observer_find_scans);
    for (unsigned i = 0; i < n; ++i)
        if (lists[i].load(std::memory_order_relaxed) == p) {
            perf::hook_timing::count(perf::hook_timing::observer_find_steps, i + 1);
            last_list = p;
            last_entry = i;
            last_epoch = e;
            return &entries[i];
        }
    perf::hook_timing::count(perf::hook_timing::observer_find_steps, n);
    return nullptr;
}
// Entry state is guarded by the entry's own lock. Without perf::list_locks the
// process-wide lock is taken first, as before. Both paths always take the
// entry lock, so they exclude each other while the switch changes. Order:
// process-wide, then entry; never the reverse.
// With perf::owner_lists (and list_locks) no lock is taken: D3D12 lets one
// thread at a time record a command list, the entry is written and read only
// by hooks on that thread, and the game's own synchronization orders a list's
// move to another thread. Only a released list's entry is touched elsewhere
// (forget(), under both locks). game-94: the two locked instructions of the
// entry lock were a large part of about 100 ns a call.
class Locked {
public:
    explicit Locked(ID3D12GraphicsCommandList *list) noexcept
        : global_(!perf::on(perf::list_locks)), owner_(!global_ && perf::on(perf::owner_lists)) {
        if (global_) AcquireSRWLockExclusive(&mutex);
        if ((entry_ = find(list)) != nullptr && !owner_) AcquireSRWLockExclusive(&entry_->lock);
    }
    ~Locked() {
        if (entry_ && !owner_) ReleaseSRWLockExclusive(&entry_->lock);
        if (global_) ReleaseSRWLockExclusive(&mutex);
    }
    Locked(const Locked &) = delete;
    Locked &operator=(const Locked &) = delete;
    Entry *get() const noexcept { return entry_; }

private:
    bool global_, owner_;
    Entry *entry_{};
};
bool copy(const void *source, void *dest, std::size_t bytes) noexcept {
    if (!source && bytes) return false;
    __try {
        if (bytes) std::memcpy(dest, source, bytes);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
// Updates take place after successful forwarding. No lock spans a driver call.
void after(unsigned which, ID3D12GraphicsCommandList *list, bool admitted, bool returned, std::uintptr_t a = 0,
           std::uintptr_t b = 0, std::uintptr_t c = 0, [[maybe_unused]] std::uintptr_t d = 0) noexcept {
    if (!admitted) return;
    {
        Locked lock(list);
        if (auto *entry = lock.get()) {
            auto &s = entry->state;
            if (!returned) {
                s.invalidate();
                entry->raster = {};
                entry->predicate = {};
            } else if (which == 0) {
                s.reset(s.generation, a);
                entry->raster = {};
                entry->raster.generation = s.generation;
                entry->predicate = {0, 0, D3D12_PREDICATION_OP_EQUAL_ZERO, true};
            } // ClearState preserves Reset generation.
            else if (which == 1)
                s.pipeline = a;
            else if (which == 2) {
                s.invalidate();
                entry->predicate = {};
            } // Bundle mutations are intentionally unsupported.
            else if (which == 23)
                entry->predicate = {a, b, static_cast<D3D12_PREDICATION_OP>(c), true};
            else if (which == 24) {
                entry->raster.index_known = a && copy(reinterpret_cast<const void *>(a), &entry->raster.index_buffer,
                                                      sizeof(entry->raster.index_buffer));
            } else if (which == 3) {
                std::array<std::uintptr_t, 2> heaps{};
                if (a > 2 || !copy(reinterpret_cast<const void *>(b), heaps.data(), a * sizeof(std::uintptr_t)))
                    s.invalidate();
                else
                    s.descriptor_heaps(std::span(heaps).first(a));
            } else if (which == 4 || which == 5)
                s.root(which == 5, a);
            else if (which >= 6 && which <= 7)
                s.descriptor(which == 7, static_cast<unsigned>(a), ArgumentKind::table, b);
            else if (which >= 8 && which <= 9) {
                const auto value = static_cast<std::uint32_t>(b);
                s.constants(which == 9, static_cast<unsigned>(a), static_cast<unsigned>(c), std::span(&value, 1));
            } else if (which >= 10 && which <= 11) {
                std::array<std::uint32_t, 64> values{};
                const auto count = static_cast<unsigned>(b >> 32), offset = static_cast<unsigned>(b);
                if (count > 64 || !copy(reinterpret_cast<const void *>(c), values.data(), std::size_t(count) * 4))
                    s.invalidate();
                else
                    s.constants(which == 11, static_cast<unsigned>(a), offset, std::span(values).first(count));
            } else if (which < 18) {
                constexpr ArgumentKind kinds[]{ArgumentKind::cbv, ArgumentKind::srv, ArgumentKind::uav};
                s.descriptor((which & 1) != 0, static_cast<unsigned>(a), kinds[(which - 12) / 2], b);
            } else {
                auto &r = entry->raster;
                if (which == 18) {
                    r.topology = static_cast<D3D_PRIMITIVE_TOPOLOGY>(a);
                    r.topology_known = true;
                }
                if (which == 19)
                    r.viewport_known =
                        a == 1 && copy(reinterpret_cast<const void *>(b), &r.viewport, sizeof(r.viewport));
                if (which == 20)
                    r.scissor_known = a == 1 && copy(reinterpret_cast<const void *>(b), &r.scissor, sizeof(r.scissor));
                if (which == 21) {
                    if (a >= r.vertex_buffers.size() || b > r.vertex_buffers.size() - a) {
                        r.vertex_known = false;
                        r.vertex_mask = 0;
                    } else {
                        for (unsigned i = 0; i < b; ++i) {
                            const auto slot = static_cast<unsigned>(a) + i;
                            if (c && copy(reinterpret_cast<const void *>(c + i * sizeof(D3D12_VERTEX_BUFFER_VIEW)),
                                          &r.vertex_buffers[slot], sizeof(D3D12_VERTEX_BUFFER_VIEW)))
                                r.vertex_mask |= 1U << slot;
                            else
                                r.vertex_mask &= ~(1U << slot);
                        }
                        if (!a) {
                            r.vertex_known = b > 0 && (r.vertex_mask & 1U);
                            if (r.vertex_known) r.vertex_buffer = r.vertex_buffers[0];
                        }
                    }
                }
                // c is the depth descriptor pointer, not the contiguous-range flag.
                // Keep the existing rendering admission contract unchanged.
                if (which == 22) {
                    r.output_known = a == 1 && !c &&
                                     copy(reinterpret_cast<const void *>(b), &r.render_target, sizeof(r.render_target));
                }
            }
        }
    }
    pending.sub();
}
template <unsigned I> void WINAPI pointer_call(ID3D12GraphicsCommandList *list, void *value) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(I));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, void *)>(originals[I])(list, value);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(I, list, admitted, returned, reinterpret_cast<std::uintptr_t>(value));
        perf::hook_timing::end(timing);
    }
}
void WINAPI predicate_call(ID3D12GraphicsCommandList *list, ID3D12Resource *buffer, UINT64 offset,
                           D3D12_PREDICATION_OP operation) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(23));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, ID3D12Resource *, UINT64, D3D12_PREDICATION_OP)>(
            originals[23])(list, buffer, offset, operation);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(23, list, admitted, returned, reinterpret_cast<std::uintptr_t>(buffer), offset, operation);
        perf::hook_timing::end(timing);
    }
}
void WINAPI heaps_call(ID3D12GraphicsCommandList *list, UINT count, ID3D12DescriptorHeap *const *heaps) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(3));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, ID3D12DescriptorHeap *const *)>(
            originals[3])(list, count, heaps);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(3, list, admitted, returned, count, reinterpret_cast<std::uintptr_t>(heaps));
        perf::hook_timing::end(timing);
    }
}
template <unsigned I>
void WINAPI table_call(ID3D12GraphicsCommandList *list, UINT index, D3D12_GPU_DESCRIPTOR_HANDLE value) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(I));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)>(originals[I])(
            list, index, value);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(I, list, admitted, returned, index, value.ptr);
        perf::hook_timing::end(timing);
    }
}
template <unsigned I> void WINAPI constant_call(ID3D12GraphicsCommandList *list, UINT index, UINT value, UINT offset) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(I));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, UINT, UINT)>(originals[I])(list, index,
                                                                                                      value, offset);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(I, list, admitted, returned, index, value, offset);
        perf::hook_timing::end(timing);
    }
}
template <unsigned I>
void WINAPI constants_call(ID3D12GraphicsCommandList *list, UINT index, UINT count, const void *values, UINT offset) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(I));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, UINT, const void *, UINT)>(originals[I])(
            list, index, count, values, offset);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(I, list, admitted, returned, index, (std::uintptr_t(count) << 32) | offset,
              reinterpret_cast<std::uintptr_t>(values));
        perf::hook_timing::end(timing);
    }
}
template <unsigned I>
void WINAPI address_call(ID3D12GraphicsCommandList *list, UINT index, D3D12_GPU_VIRTUAL_ADDRESS value) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(I));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, D3D12_GPU_VIRTUAL_ADDRESS)>(originals[I])(
            list, index, value);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(I, list, admitted, returned, index, value);
        perf::hook_timing::end(timing);
    }
}
void WINAPI topology_call(ID3D12GraphicsCommandList *list, D3D12_PRIMITIVE_TOPOLOGY topology) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(18));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, D3D12_PRIMITIVE_TOPOLOGY)>(originals[18])(
            list, topology);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(18, list, admitted, returned, topology);
        perf::hook_timing::end(timing);
    }
}
template <unsigned I, class T> void WINAPI raster_call(ID3D12GraphicsCommandList *list, UINT count, const T *values) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(I));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, const T *)>(originals[I])(list, count,
                                                                                                     values);
        perf::hook_timing::forward_end(timing);
        returned = true;
        if constexpr (I == 19)
            if (admitted && viewport_listener) {
                ++viewport_notifications;
                viewport_listener(list, count, values, reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
            }
    } __finally {
        after(I, list, admitted, returned, count, reinterpret_cast<std::uintptr_t>(values));
        perf::hook_timing::end(timing);
    }
}
void WINAPI vertices_call(ID3D12GraphicsCommandList *list, UINT first, UINT count,
                          const D3D12_VERTEX_BUFFER_VIEW *values) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(21));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, UINT, const D3D12_VERTEX_BUFFER_VIEW *)>(
            originals[21])(list, first, count, values);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(21, list, admitted, returned, first, count, reinterpret_cast<std::uintptr_t>(values));
        perf::hook_timing::end(timing);
    }
}
void WINAPI outputs_call(ID3D12GraphicsCommandList *list, UINT count, const D3D12_CPU_DESCRIPTOR_HANDLE *values,
                         BOOL contiguous, const D3D12_CPU_DESCRIPTOR_HANDLE *depth) {
    auto timing = perf::hook_timing::begin(perf::hook_timing::observer_category(22));
    const bool admitted = enter();
    bool returned = false;
    __try {
        perf::hook_timing::forward_begin(timing);
        reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, const D3D12_CPU_DESCRIPTOR_HANDLE *, BOOL,
                                        const D3D12_CPU_DESCRIPTOR_HANDLE *)>(originals[22])(list, count, values,
                                                                                             contiguous, depth);
        perf::hook_timing::forward_end(timing);
        returned = true;
    } __finally {
        after(22, list, admitted, returned, count, reinterpret_cast<std::uintptr_t>(values),
              reinterpret_cast<std::uintptr_t>(depth), static_cast<unsigned>(contiguous));
        perf::hook_timing::end(timing);
    }
}
} // namespace
namespace {
template <auto Detour, unsigned I> using Chained = astera::Gate<Detour, chain_live, originals, I>;
const std::array<void *, max_hooks> chained{
    reinterpret_cast<void *>(Chained<pointer_call<0>, 0>::call),
    reinterpret_cast<void *>(Chained<pointer_call<1>, 1>::call),
    reinterpret_cast<void *>(Chained<pointer_call<2>, 2>::call),
    reinterpret_cast<void *>(Chained<heaps_call, 3>::call),
    reinterpret_cast<void *>(Chained<pointer_call<4>, 4>::call),
    reinterpret_cast<void *>(Chained<pointer_call<5>, 5>::call),
    reinterpret_cast<void *>(Chained<table_call<6>, 6>::call),
    reinterpret_cast<void *>(Chained<table_call<7>, 7>::call),
    reinterpret_cast<void *>(Chained<constant_call<8>, 8>::call),
    reinterpret_cast<void *>(Chained<constant_call<9>, 9>::call),
    reinterpret_cast<void *>(Chained<constants_call<10>, 10>::call),
    reinterpret_cast<void *>(Chained<constants_call<11>, 11>::call),
    reinterpret_cast<void *>(Chained<address_call<12>, 12>::call),
    reinterpret_cast<void *>(Chained<address_call<13>, 13>::call),
    reinterpret_cast<void *>(Chained<address_call<14>, 14>::call),
    reinterpret_cast<void *>(Chained<address_call<15>, 15>::call),
    reinterpret_cast<void *>(Chained<address_call<16>, 16>::call),
    reinterpret_cast<void *>(Chained<address_call<17>, 17>::call),
    reinterpret_cast<void *>(Chained<topology_call, 18>::call),
    reinterpret_cast<void *>(Chained<raster_call<19, D3D12_VIEWPORT>, 19>::call),
    reinterpret_cast<void *>(Chained<raster_call<20, D3D12_RECT>, 20>::call),
    reinterpret_cast<void *>(Chained<vertices_call, 21>::call),
    reinterpret_cast<void *>(Chained<outputs_call, 22>::call),
    reinterpret_cast<void *>(Chained<predicate_call, 23>::call),
    reinterpret_cast<void *>(Chained<pointer_call<24>, 24>::call)};
} // namespace
void astera_hooks(std::vector<astera::Hook> &out) {
    for (unsigned i = 0; i < max_hooks; ++i)
        out.push_back({astera::method(astera::command_list, methods[i]), chained[i], &originals[i]});
}
namespace {
bool install_chained(ID3D12GraphicsCommandList *list, Resolver resolve, std::string &reason) {
    // AL registered all max_hooks methods at PRE_MAIN and installed them at
    // DEVICE_CREATED. The targets are still resolved to check them.
    for (unsigned i = 0; i < max_hooks; ++i) {
        if (!resolve(reinterpret_cast<std::uintptr_t>(list), methods[i], targets[i])) {
            reason = "bindings-system-method-" + std::to_string(methods[i]);
            return false;
        }
        for (unsigned j = 0; j < i; ++j)
            if (targets[i] == targets[j]) {
                reason = "bindings-method-alias";
                return false;
            }
        if (!originals[i]) {
            reason = "bindings-al-chain-" + std::to_string(methods[i]);
            return false;
        }
    }
    enabled.fill(true);
    chain_live.store(true, std::memory_order_release);
    return true;
}
} // namespace
bool install(ID3D12Device *device, Resolver resolve, std::string &reason, ViewportListener listener) {
    if (installed || !device || !resolve) {
        reason = "bindings-install-contract";
        return false;
    }
    installed = true;
    viewport_listener = listener;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                         IID_PPV_ARGS(&list))) ||
        FAILED(list->Close())) {
        reason = "bindings-discovery-list";
        return false;
    }
    return install_chained(list.Get(), resolve, reason);
}
void begin() noexcept {
    accepting.store(true, std::memory_order_release);
}
void reset(ID3D12GraphicsCommandList *list, std::uint64_t generation, ID3D12PipelineState *pipeline) noexcept {
    if (!accepting.load(std::memory_order_acquire)) return;
    const auto apply = [&](Entry *e) {
        e->state.reset(generation, reinterpret_cast<std::uintptr_t>(pipeline));
        e->raster = {};
        e->raster.generation = generation;
        e->predicate = {0, 0, D3D12_PREDICATION_OP_EQUAL_ZERO, true};
    };
    {
        Locked lock(list);
        if (auto *e = lock.get()) {
            apply(e);
            return;
        }
    }
    // First Reset of this list: a freed slot or a new one, under the
    // process-wide lock. The entry's state is reset before the list is
    // published in the slot.
    Lock lock;
    auto *e = find(list);
    if (!e) {
        const auto n = entry_count.load(std::memory_order_relaxed);
        unsigned i = 0;
        while (i < n && lists[i].load(std::memory_order_relaxed)) ++i;
        if (i == list_limit) {
            fail(1);
            return;
        }
        e = &entries[i];
        AcquireSRWLockExclusive(&e->lock);
        apply(e);
        ReleaseSRWLockExclusive(&e->lock);
        lists[i].store(list, std::memory_order_release);
        if (i == n) entry_count.store(n + 1, std::memory_order_release);
        return;
    }
    AcquireSRWLockExclusive(&e->lock);
    apply(e);
    ReleaseSRWLockExclusive(&e->lock);
}
void forget(ID3D12GraphicsCommandList *list) noexcept {
    if (!list) return;
    Lock lock;
    const auto n = entry_count.load(std::memory_order_relaxed);
    for (unsigned i = 0; i < n; ++i)
        if (lists[i].load(std::memory_order_relaxed) == list) {
            auto &e = entries[i];
            AcquireSRWLockExclusive(&e.lock);
            lists[i].store(nullptr, std::memory_order_release);
            e.state = {};
            e.raster = {};
            e.predicate = {};
            epoch.fetch_add(1, std::memory_order_acq_rel);
            forgotten.fetch_add(1, std::memory_order_relaxed);
            ReleaseSRWLockExclusive(&e.lock);
            return;
        }
}
bool matches_current(ID3D12GraphicsCommandList *list, const Bindings &state) noexcept {
    Locked lock(list);
    const auto *e = lock.get();
    return healthy() && e && e->state == state;
}
bool current(ID3D12GraphicsCommandList *list, Bindings &state) noexcept {
    Locked lock(list);
    const auto *e = lock.get();
    if (!healthy() || !e || !e->state.ready()) return false;
    state = e->state;
    return true;
}
bool inspect(ID3D12GraphicsCommandList *list, Bindings &state) noexcept {
    Locked lock(list);
    const auto *e = lock.get();
    if (!e) return false;
    state = e->state;
    return true;
}
bool raster_current(ID3D12GraphicsCommandList *list, RasterBindings &state) noexcept {
    Locked lock(list);
    const auto *e = lock.get();
    if (!accepting.load() || !healthy() || !e || !e->state.known || !e->state.valid || !e->raster.ready()) return false;
    state = e->raster;
    return true;
}
bool predicate_current(ID3D12GraphicsCommandList *list, Predicate &state) noexcept {
    Locked lock(list);
    const auto *e = lock.get();
    if (!accepting.load() || !healthy() || !e || !e->state.known || !e->state.valid || !e->predicate.known)
        return false;
    state = e->predicate;
    return true;
}
bool hooks_disabled() noexcept {
    Lock lock;
    for (const auto v : enabled)
        if (v) return false;
    return true;
}
void stop() noexcept {
    accepting.store(false, std::memory_order_release);
}
bool settled() noexcept {
    return pending.zero();
}
bool healthy() noexcept {
    return fault.load(std::memory_order_acquire) == 0;
}
bool finish() noexcept {
    if (!installed) return true;
    stop();
    const auto start = GetTickCount64();
    while (!settled() && GetTickCount64() - start < 2000) Sleep(1);
    drained = settled();
    bool ok = drained;
    chain_live.store(false, std::memory_order_release);
    enabled.fill(false);
    return ok && healthy() && settled();
}
} // namespace mhw::temporal_present::observer
