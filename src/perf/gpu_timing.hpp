#pragma once
// GPU timing (0.31.1, diagnostic: the ini's "gpu_timing = on"; alalalalan01:
// on an RTX 3070 Ti, Quality cost 1.1 ms a frame more than another mod's at
// the same preset and render size). Timestamps on the TAA list around this
// mod's own work in each reconstructed frame:
//   begin      the TAA dispatch (before the depth and input copies)
//   inputs     the copies done (runtime_resolve.cpp dispatch_before)
//   converted  the conversion dispatch done (dlss_sdk/inline.cpp)
//   evaluated  DLSS's evaluation done
//   end        the hand-off to the game's target and frame generation's
//              staging done
// resolved into a readback buffer in the same list, and read once the
// runtime's fence passes the value signalled after that list's submission.
// The work after the TAA slot (the game's post chain at the output size) is
// the rest of the frame, as PresentMon's GPU busy shows it. Every 16th slot
// is reused; a slot not yet read is skipped and counted as dropped.
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <ostream>

namespace mhw::perf::gpu_timing {
enum Point : unsigned { begin, inputs, converted, evaluated, end, point_count };
inline constexpr unsigned segment_count = point_count - 1;
inline constexpr const char *segment_names[segment_count]{"inputs", "conversion", "dlss", "output"};
inline std::atomic<bool> enabled{};

// Pure (tests): the four segments in microseconds from five timestamps of a
// queue with `frequency` ticks a second; false unless they never decrease
// and span at most 100 ms.
inline bool segments(const std::array<std::uint64_t, point_count> &t, std::uint64_t frequency,
                     std::array<double, segment_count> &us) noexcept {
    if (!frequency || t[end] - t[begin] > frequency / 10) return false;
    for (unsigned i = 1; i < point_count; ++i)
        if (t[i] < t[i - 1]) return false;
    for (unsigned i = 0; i < segment_count; ++i)
        us[i] = static_cast<double>(t[i + 1] - t[i]) * 1e6 / static_cast<double>(frequency);
    return true;
}

namespace detail {
inline constexpr unsigned slots = 16;
struct Slot {
    ID3D12GraphicsCommandList *list{}; // recording or resolved in this list
    unsigned marks{};                  // points written, one bit each
    bool resolved{};
    std::uint64_t fence{}; // the runtime's value signalled after the list's submission
};
struct State {
    SRWLOCK lock = SRWLOCK_INIT;
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> heap;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback;
    const std::uint64_t *mapped{};
    bool failed{};
    unsigned next{};
    int current = -1;
    std::array<Slot, slots> slot{};
    std::uint64_t frequency{};
    // This window's frames, those skipped (slot busy) or invalid, and the
    // segments' sums and maxima.
    std::uint64_t frames{}, dropped{}, invalid{};
    std::array<double, segment_count> sum{}, max{};
};
inline State state;
struct Lock {
    Lock() { AcquireSRWLockExclusive(&state.lock); }
    ~Lock() { ReleaseSRWLockExclusive(&state.lock); }
};
inline bool create(ID3D12GraphicsCommandList *list) noexcept {
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    if (FAILED(list->GetDevice(IID_PPV_ARGS(&device)))) return false;
    D3D12_QUERY_HEAP_DESC q{D3D12_QUERY_HEAP_TYPE_TIMESTAMP, slots * point_count, 0};
    if (FAILED(device->CreateQueryHeap(&q, IID_PPV_ARGS(&state.heap)))) return false;
    D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_READBACK};
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = sizeof(std::uint64_t) * slots * point_count;
    buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = 1;
    buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer, D3D12_RESOURCE_STATE_COPY_DEST,
                                               nullptr, IID_PPV_ARGS(&state.readback))))
        return false;
    void *data{};
    if (FAILED(state.readback->Map(0, nullptr, &data))) return false;
    state.mapped = static_cast<const std::uint64_t *>(data);
    return true;
}
inline void query(ID3D12GraphicsCommandList *list, unsigned slot, Point point) noexcept {
    list->EndQuery(state.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, slot * point_count + point);
    state.slot[slot].marks |= 1U << point;
}
} // namespace detail

// The TAA dispatch of a candidate frame (render thread).
inline void start(ID3D12GraphicsCommandList *list) noexcept {
    if (!enabled.load(std::memory_order_relaxed) || !list) return;
    using namespace detail;
    Lock lock;
    state.current = -1;
    if (state.failed) return;
    if (!state.heap && !create(list)) {
        state.failed = true;
        return;
    }
    const auto i = state.next++ % slots;
    auto &s = state.slot[i];
    // A submitted slot whose fence has not passed may still be written by the
    // GPU; one resolved but never submitted 16 frames later never ran.
    if (s.fence) {
        ++state.dropped;
        return;
    }
    s = {list};
    state.current = static_cast<int>(i);
    query(list, i, begin);
}
inline void mark(ID3D12GraphicsCommandList *list, Point point) noexcept {
    if (!enabled.load(std::memory_order_relaxed)) return;
    using namespace detail;
    Lock lock;
    if (state.current < 0 || state.slot[state.current].list != list) return;
    query(list, static_cast<unsigned>(state.current), point);
}
// After the replacement: a reconstructed frame with every point is resolved
// in the same list; anything else is let go.
inline void finish(ID3D12GraphicsCommandList *list, bool reconstructed) noexcept {
    if (!enabled.load(std::memory_order_relaxed)) return;
    using namespace detail;
    Lock lock;
    if (state.current < 0) return;
    const auto i = static_cast<unsigned>(state.current);
    auto &s = state.slot[i];
    state.current = -1;
    if (s.list != list) return;
    constexpr unsigned before_end = (1U << end) - 1;
    if (!reconstructed || s.marks != before_end) {
        s = {};
        return;
    }
    query(list, i, end);
    list->ResolveQueryData(state.heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, i * point_count, point_count,
                           state.readback.Get(), sizeof(std::uint64_t) * i * point_count);
    s.resolved = true;
}
// ExecuteCommandLists (runtime_lists.cpp): `list` was submitted to `queue`,
// followed by the runtime's fence `value`.
inline void submitted(ID3D12GraphicsCommandList *list, std::uint64_t value, ID3D12CommandQueue *queue) noexcept {
    if (!enabled.load(std::memory_order_relaxed)) return;
    using namespace detail;
    Lock lock;
    if (!state.frequency && queue && FAILED(queue->GetTimestampFrequency(&state.frequency))) state.frequency = 0;
    for (auto &s : state.slot)
        if (s.list == list && s.resolved && !s.fence) {
            s.fence = value;
            return;
        }
}
// The runtime's fence reached `completed`: read the slots it covers. Skipped
// while another thread holds the lock (the next call reads them).
inline void collect(std::uint64_t completed) noexcept {
    if (!enabled.load(std::memory_order_relaxed)) return;
    using namespace detail;
    if (!TryAcquireSRWLockExclusive(&state.lock)) return;
    for (unsigned i = 0; i < slots; ++i) {
        auto &s = state.slot[i];
        if (!s.fence || s.fence > completed || completed == UINT64_MAX) continue;
        std::array<std::uint64_t, point_count> t{};
        for (unsigned p = 0; p < point_count; ++p) t[p] = state.mapped[i * point_count + p];
        std::array<double, segment_count> us{};
        if (segments(t, state.frequency, us)) {
            ++state.frames;
            for (unsigned k = 0; k < segment_count; ++k) {
                state.sum[k] += us[k];
                if (us[k] > state.max[k]) state.max[k] = us[k];
            }
        } else
            ++state.invalid;
        s = {};
    }
    ReleaseSRWLockExclusive(&state.lock);
}
// The worker's status: this window's means and maxima in microseconds, then a
// new window.
inline void write(std::ostream &out) {
    if (!enabled.load(std::memory_order_relaxed)) return;
    using namespace detail;
    Lock lock;
    out << "{\"event\":\"gpu_timing\",\"frames\":" << state.frames << ",\"dropped\":" << state.dropped
        << ",\"invalid\":" << state.invalid << ",\"failed\":" << state.failed << ",\"frequency\":" << state.frequency
        << ",\"mean_us\":{";
    for (unsigned k = 0; k < segment_count; ++k)
        out << (k ? "," : "") << '"' << segment_names[k]
            << "\":" << (state.frames ? state.sum[k] / static_cast<double>(state.frames) : 0.0);
    out << "},\"max_us\":{";
    for (unsigned k = 0; k < segment_count; ++k)
        out << (k ? "," : "") << '"' << segment_names[k] << "\":" << state.max[k];
    out << "}}\n" << std::flush;
    state.frames = state.dropped = state.invalid = 0;
    state.sum = {};
    state.max = {};
}
} // namespace mhw::perf::gpu_timing
