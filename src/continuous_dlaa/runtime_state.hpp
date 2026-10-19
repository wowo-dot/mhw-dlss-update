#pragma once
// The DLSS runtime's internal state, shared by its files (runtime*.cpp) and
// the runtime test; the public interface is runtime.hpp.
// - runtime.cpp: qualification, install, stop, finish;
// - runtime_projection.cpp: the camera's jittered projection per frame;
// - runtime_requests.cpp: TAA's request and depth packets, their consumers;
// - runtime_resolve.cpp: the TAA dispatch replaced by DLSS;
// - runtime_lists.cpp: the D3D12 hooks (command lists, barriers, draws,
//   submission);
// - runtime_native.cpp: the game-function stubs' dispatch, AL's chains;
// - runtime_modes.cpp: live preset, aspect and Off changes;
// - runtime_log.cpp: the log's summary and full state.
#include "runtime.hpp"
#include "../game_memory/layout.hpp"
#include "../scaled_temporal/stream_route.hpp"
#include "../scaled_temporal/resolve.hpp"
#include "../render_resolution/runtime.hpp"
#include "../render_resolution/control.hpp"
#include "scaled_depth_trace.hpp"
#include "depth_schedule_trace.hpp"
#include "depth_consumer_admission.hpp"
#include "depth_consumer_copy.hpp"
#include "activation.hpp"
#include "game_options.hpp"
#include "texture_bias.hpp"
#include "history.hpp"
#include "requests.hpp"
#include "retention.hpp"
#include "native_taa.hpp"
#include "shader_rejection.hpp"
#include "resolve.hpp"
#include "unfiltered_lifetime.hpp"
#include "reconstruction.hpp"
#include "state_trace.hpp"
#include "native_state_admission.hpp"
#include "depth_eligibility.hpp"
#include "depth_destination.hpp"
#include "depth_lease.hpp"
#include "identity_depth_survey.hpp"
#include "../temporal_present/unfiltered.hpp"
#include "../temporal_bindings/depth_routes.hpp"
#include "../temporal_bindings/shader.hpp"
#include "../projection_binding/memory.hpp"
#include "../game_memory/readable_region.hpp"
#include "../perf/hook_timing.hpp"
#include "../perf/in_flight.hpp"
#include "../dlss_sdk/inline.hpp"
#include "../gpu_memory/budget.hpp"
#include "../temporal_present/observer.hpp"
#include "../ngx_startup/startup.hpp"
#include "../frame_generation/game_session.hpp"
#include "../astera/astera_hooks.hpp"
#include <wrl/client.h>
#include <atomic>
#include <bit>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <intrin.h>
#include <stdexcept>
namespace mhw::projection_binding {
bool discover(std::uintptr_t, Runtime &, std::string &);
bool refresh_scene(std::uintptr_t, const Runtime &, Runtime &, std::string &);
} // namespace mhw::projection_binding
namespace mhw::continuous_dlaa::detail {
namespace pb = projection_binding;
namespace tb = temporal_bindings;
namespace dr = tb::depth_routes;
namespace obs = temporal_present::observer;
using Microsoft::WRL::ComPtr;
using tb::NativeFrame;
inline tb::Config cfg;
inline pb::Runtime runtime;
namespace sr = scaled_temporal::stream_route;
constexpr unsigned hook_count = 13;
constexpr auto runtime_history_policy = HistoryPolicy::distance_only_comparison;
inline std::unique_ptr<temporal_present::Output> spatial;
inline std::uint64_t spatial_recordings{}, route_only_signals{};
inline dlss_sdk::Geometry scaled_geometry;
// The game's render extent at scale 0.5 (render_resolution::plan): 2:1 for
// even scenes, truncated per axis for a pillarboxed odd scene (3413 -> 1706).
inline dlss_sdk::Extent scaled_render{};
inline unsigned render_width() {
    return scaled_render.width;
}
inline unsigned render_height() {
    return scaled_render.height;
}
inline dlss_sdk::ReconstructionMode configured_mode{dlss_sdk::ReconstructionMode::performance};
// The player's DLSS Off (turn_off(), or start_off() before install): the game
// renders as without the mod until change_mode() takes it over again.
inline std::atomic<bool> off{};
inline bool off_at_start{};
// pending: callbacks in flight, without a locked instruction a call
// (perf/in_flight.hpp; the indexed-draw hook alone runs about 2,600 times a
// frame on several recording threads). Drain checks use zero().
inline std::atomic<bool> accepting{};
inline perf::InFlightCount pending;
inline std::atomic<unsigned> fault{};
inline std::atomic<std::uint64_t> frames{}, skips{}, jitter_count{}, restorations{}, reset_count{}, depth_count{};
inline std::atomic<unsigned> last_skip{};
inline SRWLOCK mutex = SRWLOCK_INIT;
struct Lock {
    Lock() { AcquireSRWLockExclusive(&mutex); }
    ~Lock() { ReleaseSRWLockExclusive(&mutex); }
};
struct Guard {
    Guard() { pending.add(); }
    ~Guard() { pending.sub(); }
};
inline void fail(unsigned n) noexcept {
    unsigned zero{};
    fault.compare_exchange_strong(zero, n);
    accepting.store(false);
}
inline bool read(void *, std::uintptr_t p, std::span<std::byte> out) {
    return pb::copy_memory(p, out.data(), static_cast<unsigned>(out.size()));
}
inline const tb::Reader reader{nullptr, read};
inline std::array<std::uintptr_t, hook_count> targets{};
inline std::array<void *, hook_count> originals{};
inline std::array<bool, hook_count> enabled{};
// Hooks 5.. (D3D12) are AL chains; their detours run from install to finish
// and otherwise continue straight on.
constexpr unsigned first_api_hook = 5;
inline std::atomic<bool> chain_live{};
// v0.30.0: hooks 0-4 (game functions) on al.hooks under AL (astera::OwnedHook),
// first in their chains (the stubs read their caller); live as each goes in,
// passing everything on until `accepting`. Patterns: each function's first 16
// original bytes (clean image; the gates in qualified() cover them).
constexpr std::array<const char *, first_api_hook> hook_names{
    "mhw_dlss DLSS:TaaProducer", "mhw_dlss DLSS:DispatchExecutor", "mhw_dlss DLSS:DepthProducer",
    "mhw_dlss DLSS:CopyExecutor", "mhw_dlss DLSS:ViewportProjection"};
constexpr std::array<const char *, first_api_hook> hook_patterns{
    "48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41", "48 89 5C 24 08 4C 89 44 24 18 48 89 54 24 10 55",
    "4C 89 44 24 18 55 56 57 41 55 41 56 48 8B EC 48", "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57",
    "40 55 53 57 48 8D AC 24 60 FE FF FF 48 81 EC A0"};
static_assert(
    [] {
        for (const auto *p : hook_patterns)
            if (!astera::valid_pattern<16>(p)) return false;
        return true;
    }(),
    "each runtime pattern is its function's first 16 bytes");
inline std::array<astera::OwnedHook, first_api_hook> game_hooks{};
// native: producer, consumer, depth producer, depth consumer, jitter;
// D3D: Reset, Close, ExecuteCommandLists, ResourceBarrier, Dispatch, CopyResource.
using BarrierFn = void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, const D3D12_RESOURCE_BARRIER *);
using DispatchFn = void(WINAPI *)(ID3D12GraphicsCommandList *, UINT, UINT, UINT);
using CopyFn = void(WINAPI *)(ID3D12GraphicsCommandList *, ID3D12Resource *, ID3D12Resource *);
inline RecordingPins recording_pins;
inline UnfilteredLifetime unfiltered_lifetime;
inline ReconstructionState reconstruction;
inline std::atomic<std::uint64_t> zero_jitter_projections{};
inline std::array<std::uint64_t, 12> dispatch_reasons{};
// TAA's producer refusals (taa_before), by the first failing check: 1 call
// arguments, 2 snapshot, 3 scene depth, 4 shader recipe, 5 command array,
// 6 no projection, 7 projection of another frame/epoch, 8 of another
// view/context, 9 of another bank/async mode, 10 render-size stamp, 11 jitter
// mapping, 12 rectangle, 13 resource pins. The last refusal's frame and four
// values (per reason, below) are kept.
inline std::array<std::atomic<std::uint64_t>, 14> producer_refusals{};
struct ProducerRefusal {
    std::uint64_t frame{};
    unsigned reason{};
    std::array<std::uint64_t, 4> values{};
};
inline ProducerRefusal last_producer_refusal;
// The projection capture's first refusal (fault 5; 0.30.1-diag.1: under
// Proton, devilzavacado-1 faulted at the first frame). failed holds one bit
// per check of projection_after, in its order: 0 returned, 1 native result,
// 2 jitter restored, 3 snapshot valid, 4 matrix relation, 5 camera, 6 camera
// type, 7 jitter bits, 8 tagged constants, 9 constant words, 10 first copy,
// 11 second copy, 12 copies equal, 13 matrices, 14 provider. region is
// VirtualQuery's answer for the constants.
struct ProjectionRefusal {
    bool recorded{}, queried{};
    std::uint64_t frame{};
    std::uint32_t failed{};
    unsigned rejection{}, checkpoint{}, reads{}, words{};
    std::uintptr_t constants{}, readable_end{};
    MEMORY_BASIC_INFORMATION region{};
};
inline ProjectionRefusal projection_refusal; // under the lock
inline std::atomic<bool> projection_refusal_claimed{};
inline StateTrace state_trace;
inline NativeStateAdmission state_admission;
inline DepthEligibilityTrace depth_eligibility;
inline DepthDestinationTrace depth_destination;
struct List {
    ComPtr<ID3D12GraphicsCommandList> value;
    std::uint64_t generation{}, injected_generation{}, signal{};
    bool closed{}, submitted{};
};
// Keeps native Execute and our fence signals in one CPU order (execute_hook),
// and orders the live rebuild's queue barrier with them (change_mode).
inline SRWLOCK submission_lock = SRWLOCK_INIT;
struct SubmissionGuard {
    SubmissionGuard() { AcquireSRWLockExclusive(&submission_lock); }
    ~SubmissionGuard() { ReleaseSRWLockExclusive(&submission_lock); }
};
// list_count: the slots used so far; a slot whose list was pruned is empty
// (value null) until another list takes it.
inline std::array<List, 128> lists;
inline unsigned list_count{};
inline std::uint64_t generation{}, sequence{}, signal_value{};
inline std::uint64_t lists_pruned{};
inline unsigned lists_peak{};
inline ComPtr<ID3D12CommandQueue> last_queue;
inline ComPtr<ID3D12Fence> fence;
inline std::array<ComPtr<ID3D12Resource>, 3> raw;
inline std::unique_ptr<dlss_sdk::InlineFrame> pass;
inline std::filesystem::path sdk_runtime; // install()'s NGX runtime folder
struct Lease {
    ID3D12GraphicsCommandList *list{};
    std::uint64_t generation{}, frame{};
    bool depth{}, evaluated{};
};
inline Lease lease;
inline HistoryStamp previous;
// Keep the previous depth identity alive across comparisons, so freeing old
// recordings cannot let a newly allocated resource reuse its pointer identity.
inline ComPtr<ID3D12Resource> history_depth;
inline HistoryTelemetry history_telemetry{runtime_history_policy};
struct Projection {
    pb::Source source;
    HistoryStamp history;
    std::uint64_t candidate{};
    bool valid{};
};
inline std::array<Projection, 32> projections;
// The projections captured per sCamera viewport (index 0..7).
inline std::array<std::atomic<std::uint64_t>, 8> projection_viewports{};
struct Taa {
    tb::Snapshot source;
    tb::SceneDepth depth;
    tb::Packet packet;
    tb::ShaderRecipe shader;
    Projection projection;
    std::uint64_t sequence{};
    bool live{}, forwarding{};
    ResourcePins resources;
    std::uint64_t depth_sequence{};
    std::uintptr_t depth_object{};
    dr::CopyPacket depth_packet;
    std::uint64_t bypasses{}, last_bypass_tick{}, last_bypass_caller{};
    unsigned last_bypass_publishers{};
};
struct Depth {
    dr::Stamp stamp;
    dr::CopyPacket packet;
    tb::Owner source, destination;
    std::uintptr_t object{};
    std::uint64_t sequence{};
    bool live{};
};
inline ScaledDepthTrace scaled_depth_trace;
inline DepthScheduleTrace depth_schedule_trace;
inline DepthConsumerAdmission depth_consumer_admission;
inline IdentityDepthSurvey identity_depth_survey;
inline SRWLOCK depth_trace_mutex = SRWLOCK_INIT;
struct DepthTraceLock {
    DepthTraceLock() { AcquireSRWLockExclusive(&depth_trace_mutex); }
    ~DepthTraceLock() { ReleaseSRWLockExclusive(&depth_trace_mutex); }
};
inline void note_depth(unsigned stage, unsigned reason, const Depth &d, ScaledDepthTrace::Values values = {}) noexcept {
    auto frame = d.stamp.frame, epoch = d.stamp.epoch;
    // An early producer rejection may lack a validated stamp. These two
    // diagnostic reads never grant frame/resource ownership.
    if (!frame) reader.get(cfg.renderer + layout::renderer::frame, frame);
    if (!epoch) reader.get(cfg.renderer + layout::renderer::epoch, epoch);
    DepthTraceLock lock;
    scaled_depth_trace.observe({GetTickCount64(), frame, epoch, d.sequence, stage, reason, values});
}
inline std::array<Taa, 32> requests;
inline std::array<Depth, 64> depths;
inline std::atomic<unsigned> publishing{};
inline RequestTelemetry request_telemetry;
inline ForwardedRequestTelemetry forwarded_telemetry;
// POD token lives on the native hook's stack, including through SEH cleanup.
// The matching slot retains its resource pins until this callback returns.
struct ForwardedRequest {
    std::uint64_t sequence{};
    unsigned slot{};
    tb::Packet packet;
};
inline thread_local bool native_forwarding{};
inline ResolveTelemetry resolve_telemetry;
inline ShaderRejection shader_rejection;
inline NativeShaderCache shader_cache;
inline NativeShaderCache preparation_shader_cache;
constexpr bool include_menu_shader = true;
struct PreparedResolve {
    temporal_present::Bindings bindings;
    HistoryDecision history;
    bool evaluate{}, full_output{};
};
inline thread_local PreparedResolve prepared_resolve;
inline thread_local bool injected{};
inline thread_local Taa producer, consumer;
inline thread_local Depth depth_producer, depth_consumer;
inline thread_local Projection projection;
inline thread_local unsigned producer_count{}, depth_producer_count{};
inline thread_local std::uintptr_t producer_array{};
inline thread_local ID3D12GraphicsCommandList *current_list{};
inline thread_local std::uintptr_t current_recording{};
inline thread_local unsigned current_kind{}, barrier_count{}, dispatch_count{}, copy_count{};
inline thread_local std::array<D3D12_RESOURCE_BARRIER, 128> barriers;
inline std::atomic_flag projection_busy = ATOMIC_FLAG_INIT;
inline List *find(ID3D12GraphicsCommandList *l) {
    if (!l) return nullptr;
    for (unsigned i = 0; i < list_count; ++i)
        if (lists[i].value.Get() == l) return &lists[i];
    return nullptr;
}
inline unsigned live_lists() {
    unsigned n{};
    for (unsigned i = 0; i < list_count; ++i) n += lists[i].value != nullptr;
    return n;
}
inline bool child(ID3D12DeviceChild *p) {
    if (!p) return false;
    std::uintptr_t target{};
    if (!pb::system_method(reinterpret_cast<std::uintptr_t>(p), 7, target)) return false;
    ComPtr<ID3D12Device> device;
    return SUCCEEDED(p->GetDevice(IID_PPV_ARGS(&device))) &&
           reinterpret_cast<std::uintptr_t>(device.Get()) == cfg.device;
}
inline bool pin(std::uintptr_t address, unsigned format, ComPtr<ID3D12Resource> &owned, bool uav = false) {
    auto *r = reinterpret_cast<ID3D12Resource *>(address);
    if (!child(r)) return false;
    const auto d = r->GetDesc();
    if (d.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || d.Width != runtime.width || d.Height != runtime.height ||
        d.MipLevels != 1 || d.DepthOrArraySize != 1 || d.SampleDesc.Count != 1 || d.SampleDesc.Quality ||
        static_cast<unsigned>(d.Format) != format || (uav && !(d.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)))
        return false;
    owned = r;
    return true;
}
inline void transition(ID3D12GraphicsCommandList *list, ID3D12Resource *r, D3D12_RESOURCE_STATES a,
                       D3D12_RESOURCE_STATES b) {
    D3D12_RESOURCE_BARRIER v{};
    v.Transition = {r, UINT_MAX, a, b};
    reinterpret_cast<BarrierFn>(originals[8])(list, 1, &v);
}
inline void copy_input(ID3D12GraphicsCommandList *list, ID3D12Resource *source, unsigned role,
                       D3D12_RESOURCE_STATES state) {
    transition(list, source, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
    reinterpret_cast<CopyFn>(originals[10])(list, raw[role].Get(), source);
    transition(list, source, D3D12_RESOURCE_STATE_COPY_SOURCE, state);
}
// An odd scene's effects sample up to scene x scale (1706.5 of 3413 columns),
// half a column past the 1706 the game renders. That column keeps what the
// full-size frames before the reduced scale drew there, and the game's effects
// spread it along the right edge (title-screen colours, game-22/game-24).
// After copying a native input, replicate its last rendered column (and row,
// for a fractional height) into the one just outside, from the private copy.
inline bool edge_column{}, edge_row{};
inline std::uint64_t edge_fills{};
inline void fill_edge(ID3D12GraphicsCommandList *list, ID3D12Resource *native, unsigned role,
                      D3D12_RESOURCE_STATES state) {
    if (!edge_column && !edge_row) return;
    auto *copy = raw[role].Get();
    const unsigned rw = render_width(), rh = render_height();
    transition(list, copy, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
    transition(list, native, state, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION d{}, s{};
    d.pResource = native;
    s.pResource = copy;
    d.Type = s.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    bool copied = true;
    if (edge_column) {
        const D3D12_BOX b{rw - 1, 0, 0, rw, rh, 1};
        copied = sr::copy_region(list, &d, rw, 0, 0, &s, &b) && copied;
    }
    if (edge_row) {
        const D3D12_BOX b{0, rh - 1, 0, rw, rh, 1};
        copied = sr::copy_region(list, &d, 0, rh, 0, &s, &b) && copied;
    }
    transition(list, native, D3D12_RESOURCE_STATE_COPY_DEST, state);
    transition(list, copy, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
    if (copied) ++edge_fills;
}
inline bool state_of(std::uintptr_t resource, D3D12_RESOURCE_STATES expected) {
    bool known = false;
    unsigned state{};
    for (unsigned i = 0; i < barrier_count; ++i) {
        const auto &b = barriers[i];
        if (b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING &&
            (!b.Aliasing.pResourceBefore || !b.Aliasing.pResourceAfter ||
             reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore) == resource ||
             reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter) == resource))
            return false;
        if (b.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION ||
            reinterpret_cast<std::uintptr_t>(b.Transition.pResource) != resource)
            continue;
        if (b.Flags || b.Transition.Subresource != UINT_MAX || (known && state != unsigned(b.Transition.StateBefore)))
            return false;
        known = true;
        state = b.Transition.StateAfter;
    }
    return known && state == unsigned(expected);
}
inline void trace_states(ID3D12GraphicsCommandList *list, bool temporal) {
    const auto *l = find(list);
    state_trace.capture(reader, consumer.source, consumer.sequence, list, l ? l->generation : 0, consumer.packet.object,
                        GetTickCount64(), temporal, std::span(barriers).first(barrier_count));
}
// Render scale 1.0 (DLAA): the render extent is the scene. The depth writer
// then takes the game's plain path (a native CopyResource, not the nested
// draw the scaled admission qualifies), so depth is leased at that copy, as in
// the unscaled build (dlaa-identity-path.md).
inline bool identity_extent() {
    return pass && render_width() == runtime.width && render_height() == runtime.height;
}

// --- Live changes and install (runtime_modes.cpp, runtime.cpp) ---------------
// A pending live request: the mode's value, off_request, or -1.
constexpr int off_request = 0x100;
inline std::atomic<int> mode_request{-1};
// Set once install() completed (any thread reads it; the worker owns pass).
inline std::atomic<bool> live_installed{};
// The last video memory check (install or rebuild), for the summary. Under Lock.
inline gpu_memory::Check memory_check;
// Whether `needed` more bytes fit the video memory budget; keeps the check for
// the summary and, on a refusal, the reason.
bool memory_fits(std::uint64_t needed, gpu_memory::Check &c, std::string &why);
// Polls `done` until true or `ms` pass; Sleep(1) between polls.
template <class F> bool wait_for(F &&done, std::uint64_t ms) {
    const auto end = GetTickCount64() + ms;
    while (!done()) {
        if (GetTickCount64() >= end) return false;
        Sleep(1);
    }
    return true;
}
// The player's anti-aliasing and resolution scaling again (game_options.hpp),
// once the render thread restored the owned scale.
void give_back_options() noexcept;

// --- The hooks' bodies ------------------------------------------------------------
// runtime_projection.cpp
bool projection_before(const NativeFrame &f);
void projection_after(bool admitted, bool returned, NativeFrame &f);
// runtime_requests.cpp
bool taa_before(const NativeFrame &f);
void publish_taa();
void taa_after(bool admitted, bool returned);
bool depth_before(const NativeFrame &f);
void depth_after(bool admitted, bool returned);
void finish_forwarded(ForwardedRequest &token, bool returned);
bool consumer_before(const NativeFrame &f, bool depth, ForwardedRequest *forwarded = nullptr);
void consumer_after(bool admitted, bool returned);
// runtime_resolve.cpp
ResolveResult dispatch_replace(ID3D12GraphicsCommandList *list);
void fg_final_scene(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots, ID3D12Resource *scene,
                    std::uint64_t frame, std::uint64_t epoch, D3D12_RESOURCE_STATES scene_state) noexcept;
void dispatch_call(ID3D12GraphicsCommandList *list, UINT x, UINT y, UINT z, std::uintptr_t caller);
void WINAPI dispatch_hook(ID3D12GraphicsCommandList *list, UINT x, UINT y, UINT z);
// runtime_lists.cpp
void WINAPI barrier_hook(ID3D12GraphicsCommandList *list, UINT n, const D3D12_RESOURCE_BARRIER *data);
void WINAPI indexed_hook(ID3D12GraphicsCommandList *list, UINT n, UINT instances, UINT first, INT base, UINT instance);
void WINAPI draw_hook(ID3D12GraphicsCommandList *list, UINT n, UINT instances, UINT first, UINT instance);
void WINAPI copy_hook(ID3D12GraphicsCommandList *list, ID3D12Resource *destination, ID3D12Resource *source);
HRESULT WINAPI reset_hook(ID3D12GraphicsCommandList *list, ID3D12CommandAllocator *allocator,
                          ID3D12PipelineState *pipeline);
HRESULT WINAPI close_hook(ID3D12GraphicsCommandList *list);
void WINAPI execute_hook(ID3D12CommandQueue *queue, UINT count, ID3D12CommandList *const *submitted);
// runtime_native.cpp
void native(unsigned kind, NativeFrame *f);
} // namespace mhw::continuous_dlaa::detail
