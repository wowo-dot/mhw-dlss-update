// The stream route after DLSS: the final-screen route, the game's post chain
// at output size (full_post) and the silhouette replay. One translation unit
// on purpose: its parts share the route's state and lock (the anonymous
// namespace below), and the per-draw hooks (indexed_draw runs about 2,600
// times a frame) call into every part; the build has no link-time code
// generation, so separate files would stop those calls from inlining. Each
// part is included where the state and helpers it uses are declared:
// - silhouette_probe.inl: the late packets a plan replays and the sampled
//   indexed draws for the log;
// - full_post.inl: full_post's render-thread state, the post chain's capture,
//   the tone-map and projection hooks;
// - post_census.inl: the sampled frame's post-chain census for the log;
// - full_post_slot.inl: the inputs' upscale slot and the final copy at
//   execution;
// - silhouette_route.inl: the silhouette replay (recipes, pipelines, the
//   recorded draws).
#include "stream_route.hpp"
#include "filter_shader.hpp"
#include "stream_lifetime.hpp"
#include "readiness_trace.hpp"
#include "output_diagnostics.hpp"
#include "frame_handoff.hpp"
#include "recording_states.hpp"
#include "silhouette_shader.hpp"
#include "silhouette_draw.hpp"
#include "silhouette_material.hpp"
#include "watch_filter.hpp"
#include "full_post.hpp"
#include "full_post_inputs.hpp"
#include "../projection_binding/memory.hpp"
#include "../temporal_present/observer.hpp"
#include "../temporal_bindings/copy_execution.hpp"
#include "../perf/toggles.hpp"
#include "../perf/hook_timing.hpp"
#include <wrl/client.h>
#include <atomic>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <intrin.h>
#include <ostream>
#include <stdexcept>
extern "C" void scaled_stream_final_entry();
extern "C" void scaled_copy_execution_entry();
extern "C" void scaled_indexed_execution_entry();
extern "C" void scaled_full_tone_map_entry();
extern "C" void scaled_full_project_entry();
extern "C" void mhw_binding_forward(void *, mhw::projection_binding::Frame *);
namespace mhw::scaled_temporal::stream_route {
namespace {
namespace tb = temporal_bindings;
namespace pb = projection_binding;
namespace obs = temporal_present::observer;
using Microsoft::WRL::ComPtr;
using CopyFn = void(WINAPI *)(ID3D12GraphicsCommandList *, const D3D12_TEXTURE_COPY_LOCATION *, UINT, UINT, UINT,
                              const D3D12_TEXTURE_COPY_LOCATION *, const D3D12_BOX *);
// The identity plan's resources: TAA's output, the motion filter's input (the
// TAA copy), the grade's output and the view's attachment (the final copy).
constexpr unsigned resource_capacity = 4;
// A native pipeline's input layout, read once and compared before each reuse
// (the silhouette recipes).
struct InputLayout {
    pb::NativePipeline pipeline;
    std::array<D3D12_INPUT_ELEMENT_DESC, 32> elements{};
    std::array<std::array<char, 64>, 32> semantics{};
};
struct SilhouetteCpu {
    pb::NativePipeline native;
    InputLayout layout;
    struct Code {
        std::uintptr_t entry{}, descriptor{}, data{};
        unsigned size{};
    };
    std::array<Code, 2> identity;
    std::array<std::vector<std::byte>, 2> bytes;
    std::uintptr_t set{};
    ShaderHash vs_hash{}, ps_hash{}; // for diagnostics: which programs a rejected draw used
};
struct LatePacket {
    std::array<std::uintptr_t, 4> bindings{};
    std::array<unsigned, 3> arguments{};
    std::shared_ptr<SilhouetteCpu> recipe;
    pb::NativePipeline native;
    bool silent{}; // writes no colour, depth or stencil (silhouette_silent): stays native
};
// v0.26.0: every plan is an identity plan (dlaa-identity-path.md,
// full-size-post-chain.md): at render scale 1.0, and for each frame full_post
// arms. The resolve stays scaled; the post chain runs natively at the output.
struct Plan {
    tb::Snapshot stamp;
    tb::Packet temporal;
    std::uintptr_t copy{};
    std::array<std::byte, 64> copy_fields{};
    std::array<ComPtr<ID3D12Resource>, resource_capacity> resources;
    bool published{}, built{}, claimed{}, queue_finished{};
    // The thread that published the frame (TAA's producer, inside TAA's
    // callback): the post queue completes on that render thread.
    unsigned publish_thread{};
    // Full-output ownership is fixed before native recording workers start.
    // GPU order is proved separately from the order in which workers visit us.
    FrameHandoff handoff;
    ComPtr<ID3D12Resource> selector; // one immutable GPU choice per prepared frame
    struct Recording {
        ID3D12GraphicsCommandList *list{};
        std::uint64_t generation{};
        std::array<bool, resource_capacity> known{};
        std::array<unsigned, resource_capacity> states{};
    };
    std::array<Recording, 8> recordings{};
    bool prepared{}, resolved{}, copied{}, completed{};
    // The frame ends with a native copy from the grade output to the final
    // attachment (at render scale 1.0 the game's own; under full_post the DLAA
    // copy recorded in the final quad's key), not the System_Copy quad.
    bool identity{}, final_copied{};
    std::uintptr_t final_copy{};
    std::array<std::byte, 64> final_copy_fields{};
    // full_post: the inputs' upscale slot (full_post.inl), in the hand-off
    // between the TAA copy and the final copy (node 2; the final copy is then
    // node 3), and the depth's and GBuffer4's states at the TAA consumer.
    std::uintptr_t full_slot{}, full_depth_resource{}, full_gbuffer4_resource{};
    bool full_slot_done{}, full_states_known{};
    unsigned full_depth_state{}, full_gbuffer4_state{};
    // The depth's render region kept at the slot, and the late draws' restore.
    bool full_kept{}, full_restore_tried{}, full_restore_recorded{};
    // Late indexed packets are frozen before recording workers start, sorted
    // in native key/insertion order and included in GPU ownership. About 17 per
    // frame in combat; Alatreon's arrival exceeded the former 32 (game-45).
    static constexpr unsigned late_capacity = 64;
    std::array<std::uintptr_t, late_capacity> late_packets{};
    std::array<unsigned, late_capacity> late_keys{};
    std::array<LatePacket, late_capacity> late{};
    unsigned late_count{}, late_recorded{};
    std::uint64_t late_executed{};
};
// The hand-off: TAA, its copy, full_post's slot, the late draws, the final copy.
unsigned handoff_indexed(const Plan &p, unsigned position) {
    return 2 + (p.full_slot ? 1U : 0U) + position;
}
unsigned handoff_count(const Plan &p) {
    return 3 + p.late_count + (p.full_slot ? 1U : 0U);
}
unsigned final_node(const Plan &p) {
    return 2 + (p.full_slot ? 1U : 0U) + p.late_count;
}
unsigned resource_count(const Plan &) {
    return resource_capacity;
}
tb::Config cfg;
unsigned width{}, height{};
// The game's render extent inside the output (render_resolution::plan): 2:1
// for even scenes, truncated per axis for a pillarboxed odd scene (3413 -> 1706).
unsigned render_w{}, render_h{};
// The first render-extent mismatch of each kind: the final quad's source
// rectangle and the TAA copy's box {left, top, right, bottom}.
std::array<unsigned, 4> final_miss{}, copy_miss{};
bool final_missed{}, copy_missed{};
std::array<Plan, 32> plans;
// perf::packet_filter (v0.12): the packets and resources each ring slot's plan
// can match, written under the route lock when the plan is built and cleared
// when the slot is published again. A hook takes the route lock only for a
// listed packet, or a barrier on a listed resource; any other call cannot
// match a plan and is skipped without it. A stale entry only costs a locked
// check. The slot being rewritten holds a plan whose packets have not run yet.
constexpr unsigned watch_packets = 96;
struct Watch {
    std::array<std::atomic<std::uintptr_t>, watch_packets> packets{};
    std::array<std::atomic<std::uintptr_t>, resource_capacity> resources{};
    std::atomic<unsigned> packet_count{}, resource_count{};
    std::atomic<bool> all{};
    PlanFilter packet_filter, resource_filter; // v0.24.3: this slot's lists (watch_filter.hpp)
};
std::array<Watch, 32> watches;
std::atomic<std::uint32_t> watch_mask{};
// The union of every watched slot's filters: most hooked calls (every engine
// draw, every barrier) match no plan and are rejected here.
UnionFilter any_packet, any_resource;
static_assert(watches.size() == plans.size() && plans.size() <= 32, "one watch bit per plan slot");
bool filtering() noexcept {
    return perf::on(perf::packet_filter);
}
// Under the route lock.
void unwatch(unsigned slot) noexcept {
    const auto mask = watch_mask.fetch_and(~(1U << slot), std::memory_order_acq_rel) & ~(1U << slot);
    auto &w = watches[slot];
    w.packet_count.store(0, std::memory_order_release);
    w.resource_count.store(0, std::memory_order_release);
    w.all.store(false, std::memory_order_release);
    w.packet_filter.clear();
    w.resource_filter.clear();
    // The unions rebuilt from the slots still watched.
    std::array<std::uint64_t, UnionFilter::words> packets{}, resources{};
    bool overflow = false;
    for (auto m = mask; m; m &= m - 1) {
        const auto &x = watches[static_cast<unsigned>(std::countr_zero(m))];
        overflow = overflow || x.all.load(std::memory_order_relaxed);
        for (unsigned i = 0, n = x.packet_count.load(std::memory_order_relaxed); i < n; ++i)
            UnionFilter::set(packets, x.packets[i].load(std::memory_order_relaxed));
        for (unsigned i = 0, n = x.resource_count.load(std::memory_order_relaxed); i < n; ++i)
            UnionFilter::set(resources, x.resources[i].load(std::memory_order_relaxed));
    }
    if (overflow) packets.fill(~0ULL);
    any_packet.assign(packets);
    any_resource.assign(resources);
}
template <class Fn> bool watched(Fn &&listed) noexcept {
    for (auto mask = watch_mask.load(std::memory_order_acquire); mask; mask &= mask - 1) {
        const auto &w = watches[static_cast<unsigned>(std::countr_zero(mask))];
        if (w.all.load(std::memory_order_acquire) || listed(w)) return true;
    }
    return false;
}
bool watched_packet(std::uintptr_t packet) noexcept {
    if (!any_packet.may_contain(packet)) return false;
    return watched([&](const Watch &w) {
        if (!w.packet_filter.may_contain(packet)) return false;
        const auto n = w.packet_count.load(std::memory_order_acquire);
        for (unsigned i = 0; i < n; ++i)
            if (w.packets[i].load(std::memory_order_relaxed) == packet) return true;
        return false;
    });
}
bool watched_resource(std::uintptr_t resource) noexcept {
    if (!any_resource.may_contain(resource)) return false;
    return watched([&](const Watch &w) {
        if (!w.resource_filter.may_contain(resource)) return false;
        const auto n = w.resource_count.load(std::memory_order_acquire);
        for (unsigned i = 0; i < n; ++i)
            if (w.resources[i].load(std::memory_order_relaxed) == resource) return true;
        return false;
    });
}
// Under the route lock: when a plan is built, and again when a dependency
// refresh pins another resource for it. Rewritten entries keep their values.
void watch(unsigned slot, const Plan &p) noexcept {
    auto &w = watches[slot];
    unsigned n{}, r{};
    bool overflow = false;
    // Filter bits first (only added while the slot is watched), then the lists.
    const auto add = [&](std::uintptr_t packet) {
        if (!packet) return;
        if (n == watch_packets) {
            overflow = true;
            return;
        }
        w.packet_filter.add(packet);
        any_packet.add(packet);
        w.packets[n++].store(packet, std::memory_order_relaxed);
    };
    for (unsigned i = 0; i < p.late_count && i < p.late_packets.size(); ++i) add(p.late_packets[i]);
    add(p.copy);
    add(p.final_copy);
    add(p.temporal.object);
    for (unsigned i = 0; i < resource_count(p) && i < p.resources.size(); ++i)
        if (auto *x = p.resources[i].Get()) {
            const auto address = reinterpret_cast<std::uintptr_t>(x);
            w.resource_filter.add(address);
            any_resource.add(address);
            w.resources[r++].store(address, std::memory_order_relaxed);
        }
    if (overflow) {
        w.packet_filter.fill();
        any_packet.fill();
    }
    w.all.store(overflow, std::memory_order_release);
    w.packet_count.store(n, std::memory_order_release);
    w.resource_count.store(r, std::memory_order_release);
    watch_mask.fetch_or(1U << slot, std::memory_order_release);
}
// tBaseMap is a mutable shader binding. Capture it while the final quad is
// enqueued, before later filter preparation replaces that context slot.
struct FinalQuad {
    tb::Snapshot stamp;
    tb::Owner source, destination;
    std::uintptr_t attachment{}, packet{}, queue{};
    unsigned before{}, after{}, base_id{};
    bool entered{}, valid{};
};
std::array<FinalQuad, 32> final_quads;
StreamLifetime lifetime;
ReadinessTrace readiness;
OutputDiagnostics diagnostics;
using StateJournal = RecordingStates<128, 32, ComPtr<ID3D12Resource>>;
StateJournal recording_states;
std::uint64_t state_observations{}, state_unscoped{}, state_imports{};
struct Active {
    unsigned slot{};
    ID3D12GraphicsCommandList *list{};
    std::uint64_t generation{};
    std::uintptr_t recording{};
    bool resolved{}, copied{};
} active;
// 2: CopyTextureRegion; 3-5: the final quad, the copy and indexed-draw
// executors; 7-8: full_post (tone map callback, projection helper). 0, 1 and 6
// are unused (the per-effect route's post-filter and draw hooks until v0.26.0).
std::array<std::uintptr_t, 9> targets{};
std::array<void *, 9> originals{};
std::array<bool, 9> enabled{};
// AL: hook 2 (CopyTextureRegion) is an AL chain, live while the runtime
// has the stream hooks applied (chains_live), otherwise continuing straight on.
constexpr unsigned api_hook = 2;
// v0.29.0: the game hooks (3-5, 7, 8) on al.hooks under AL (astera::OwnedHook),
// first in their chains (the stubs read their caller); live as each goes in,
// passing everything on until begin(). Patterns: each target's first 16
// original bytes (clean image; the gates in prepare() cover them).
struct HookSpec {
    unsigned slot;
    const char *name, *pattern;
};
constexpr HookSpec hook_specs[]{{3, "mhw_dlss Post:FinalQuad", "48 8B C4 48 89 58 08 48 89 70 10 48 89 78 18 55"},
                                {4, "mhw_dlss Post:CopyExecutor", "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48"},
                                {5, "mhw_dlss Post:IndexedExecutor", "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 48"},
                                {7, "mhw_dlss Post:ToneMap", "40 55 53 56 57 48 8D 6C 24 B8 48 81 EC 48 01 00"},
                                {8, "mhw_dlss Post:Projection", "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 40 0F"}};
static_assert(
    [] {
        for (const auto &h : hook_specs)
            if (!astera::valid_pattern<16>(h.pattern)) return false;
        return true;
    }(),
    "each stream pattern is its target's first 16 bytes");
std::array<astera::OwnedHook, 9> owned{};
std::atomic<bool> chain_live{};
std::atomic<bool> accepting{};
std::atomic<unsigned> pending{}, fault{};
std::uint64_t published{}, built{}, unqualified{}, claimed{}, copies{}, complete{};
// Scaled frames full_post did not arm: not planned, the game's own upscale.
std::uint64_t unarmed{};
std::uint64_t final_captured{}, final_rejected{};
std::uint64_t identity_plans{}, identity_final_copies{}, identity_observed{};
std::uint64_t spatial_frames{}, native_frames{}, prepared_frames{}, multilist_frames{}, submitted_frames{};
std::array<std::uint64_t, 19> rejection_counts{}; // rejected plan builds by graph reason (18: other)
std::uint64_t summarized_rejections{};
bool abandoned{};
std::string failure;
unsigned plan_rejection{}, plan_detail{};
SRWLOCK mutex = SRWLOCK_INIT;
struct Lock {
    Lock() { AcquireSRWLockExclusive(&mutex); }
    ~Lock() { ReleaseSRWLockExclusive(&mutex); }
};
struct Guard {
    Guard() { ++pending; }
    ~Guard() { --pending; }
};
thread_local bool injecting{};
thread_local unsigned copy_slot = UINT_MAX, copy_calls{}, resolve_slot = UINT_MAX;
thread_local bool copy_final{}; // the executing copy is an identity plan's final copy
thread_local std::uintptr_t copy_recording{};
void fail(unsigned n) {
    unsigned zero{};
    fault.compare_exchange_strong(zero, n);
    accepting = false;
}
template <class T> bool field(std::uintptr_t at, unsigned off, T &v) {
    return at && at <= UINTPTR_MAX - off - sizeof(T) && pb::copy_memory(at + off, &v, sizeof(T));
}
bool read(void *, std::uintptr_t at, std::span<std::byte> b) {
    return pb::copy_memory(at, b.data(), static_cast<unsigned>(b.size()));
}
const tb::Reader reader{nullptr, read};
bool same_stamp(const tb::Snapshot &);
std::shared_ptr<SilhouetteCpu> silhouette_cpu(std::uintptr_t packet, bool &silent);
std::uint64_t silhouette_planned{};
#include "silhouette_probe.inl"
bool layout(InputLayout &r, bool compare) {
    const auto &p = r.pipeline;
    if (p.input_count > 32 || (!p.input_elements && p.input_count)) return false;
    for (unsigned i = 0; i < p.input_count; ++i) {
        D3D12_INPUT_ELEMENT_DESC element{};
        std::array<char, 64> semantic{};
        bool terminated = false;
        if (!field(p.input_elements, i * sizeof(element), element) || !element.SemanticName) return false;
        for (unsigned j = 0; j < semantic.size(); ++j) {
            if (!field(reinterpret_cast<std::uintptr_t>(element.SemanticName), j, semantic[j])) return false;
            if (!semantic[j]) {
                terminated = true;
                break;
            }
            if (semantic[j] < 32 || semantic[j] > 126) return false;
        }
        if (!terminated) return false;
        if (compare) {
            if (std::memcmp(&element, &r.elements[i], sizeof(element)) || semantic != r.semantics[i]) return false;
        } else {
            r.elements[i] = element;
            r.semantics[i] = semantic;
        }
    }
    return true;
}

bool same_stamp(const tb::Snapshot &s) {
    std::uint64_t frame{}, epoch{};
    std::uintptr_t view{}, primary{};
    unsigned bank{};
    unsigned char async{};
    return field(cfg.renderer, 0x1f210, frame) && frame == s.frame && field(cfg.renderer, 0x1f208, epoch) &&
           epoch == s.epoch && field(cfg.renderer, 0x1f200, bank) && bank == s.bank &&
           field(cfg.renderer, 0x93, async) && async == s.async && field(s.context, 0x1d0, view) && view == s.view &&
           field(cfg.renderer, 0x1f450, primary) && primary == view;
}
bool owner(std::uintptr_t at, tb::Owner &out) {
    return tb::read_owner(reader, at, out) && out.width == width && out.height == height && out.resource;
}
bool final_identity(const FinalQuad &q, const tb::Snapshot &s) {
    std::uintptr_t attachment{}, destination{};
    tb::Owner a{}, b{};
    return q.valid && q.stamp.frame == s.frame && q.stamp.epoch == s.epoch && q.stamp.context == s.context &&
           q.stamp.view == s.view && q.stamp.bank == s.bank && q.stamp.async == s.async && same_stamp(s) &&
           field(s.view, 0x460, attachment) && attachment == q.attachment && field(attachment, 0x58, destination) &&
           destination == q.destination.object && tb::read_owner(reader, q.source.object, a) && a == q.source &&
           tb::read_owner(reader, q.destination.object, b) && b == q.destination;
}
unsigned final_before(const pb::Frame &f) {
    if (!accepting.load() || f.caller != cfg.image + 0x23c7b08 || f.gpr[2] != cfg.renderer + 0x710) return UINT_MAX;
    Lock lock;
    std::uint64_t frame{};
    if (!field(cfg.renderer, 0x1f210, frame) || !frame) return UINT_MAX;
    const auto slot = static_cast<unsigned>(frame % final_quads.size());
    auto &q = final_quads[slot];
    if ((q.entered || q.valid) && q.stamp.frame == frame) {
        q.valid = false;
        ++final_rejected;
        return UINT_MAX;
    }
    q = {};
    q.stamp.frame = frame;
    q.stamp.context = f.gpr[2];
    auto &s = q.stamp;
    std::uintptr_t source{}, destination{};
    std::array<unsigned, 4> rectangle{}, viewport{};
    std::array<unsigned, 2> dimensions{};
    std::array<float, 2> offset{};
    // The stored identity is widened, but the native flag occupies one byte.
    unsigned char async{};
    if (!field(cfg.renderer, 0x93, async)) {
        ++final_rejected;
        return UINT_MAX;
    }
    s.async = async;
    const bool ok = field(cfg.renderer, 0x1f208, s.epoch) && field(cfg.renderer, 0x1f200, s.bank) &&
                    field(s.context, 0x1d0, s.view) && same_stamp(s) && field(f.gpr[3], 0, rectangle) &&
                    rectangle == std::array<unsigned, 4>{0, 0, render_w, render_h} && field(f.gpr[4], 0, dimensions) &&
                    dimensions == std::array<unsigned, 2>{width, height} && field(f.stack[0], 0, offset) &&
                    offset == std::array<float, 2>{-.5F / width, -.5F / height} && (f.stack[1] & 255) == 1 &&
                    field(s.context, 0x5c, viewport) && viewport == std::array<unsigned, 4>{0, 0, width, height} &&
                    field(cfg.image, 0x4be7450, q.base_id) && q.base_id != UINT_MAX &&
                    field(s.context, 0x710 + 8 * (q.base_id & 0xfff), source) && owner(source, q.source) &&
                    field(s.view, 0x460, q.attachment) && field(q.attachment, 0x58, destination) &&
                    owner(destination, q.destination) && q.source.resource != q.destination.resource &&
                    field(s.context, 0x2b80, q.before) && q.before < 65534 && field(s.context, 0x2b78, q.queue) &&
                    q.queue && q.queue <= UINTPTR_MAX - 16ULL * (q.before + 1);
    if (!ok) {
        ++final_rejected;
        if (rectangle[2] && rectangle != std::array<unsigned, 4>{0, 0, render_w, render_h} && !final_missed) {
            final_missed = true;
            final_miss = rectangle;
        }
        return UINT_MAX;
    }
    q.entered = true;
    return slot;
}
void final_after(unsigned slot, bool returned) {
    if (slot == UINT_MAX) return;
    Lock lock;
    auto &q = final_quads[slot];
    std::uintptr_t source{}, queue{}, packet{};
    unsigned key{}, opcode{};
    q.valid = returned && q.entered && same_stamp(q.stamp) &&
              field(q.stamp.context, 0x710 + 8 * (q.base_id & 0xfff), source) && source == q.source.object &&
              field(q.stamp.context, 0x2b80, q.after) && q.after == q.before + 1 &&
              field(q.stamp.context, 0x2b78, queue) && queue == q.queue && field(queue + 16ULL * q.before, 0, key) &&
              key == 0x80000000 && field(queue + 16ULL * q.before, 8, packet) && field(packet, 0, opcode) &&
              (opcode & 31) == 0;
    q.entered = false;
    q.packet = packet;
    q.valid = q.valid && final_identity(q, q.stamp);
    if (q.valid)
        ++final_captured;
    else
        ++final_rejected;
}
#include "full_post.inl"
bool pin(const tb::Owner &o, ComPtr<ID3D12Resource> &out) {
    std::uintptr_t method{};
    for (unsigned slot : {0U, 2U, 7U, 10U})
        if (!pb::system_method(o.resource, slot, method)) return false;
    auto *r = reinterpret_cast<ID3D12Resource *>(o.resource);
    ComPtr<ID3D12Device> d;
    if (FAILED(r->GetDevice(IID_PPV_ARGS(&d))) || reinterpret_cast<std::uintptr_t>(d.Get()) != cfg.device) return false;
    const auto desc = r->GetDesc();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Width != width || desc.Height != height ||
        desc.DepthOrArraySize != 1 || desc.MipLevels != 1 || desc.SampleDesc.Count != 1 || desc.SampleDesc.Quality ||
        desc.Format != DXGI_FORMAT_R11G11B10_FLOAT)
        return false;
    out = r;
    return true;
}
#include "post_census.inl"
Plan::Recording *track_recording(Plan &p, ID3D12GraphicsCommandList *list, std::uint64_t gen, bool create) {
    if (!list || !gen) return nullptr;
    Plan::Recording *empty{};
    for (auto &r : p.recordings) {
        if (r.list == list) return r.generation == gen ? &r : nullptr;
        if (!r.list && !empty) empty = &r;
    }
    if (!create || !empty) return nullptr;
    empty->list = list;
    empty->generation = gen;
    for (unsigned i = 0; i < resource_count(p); ++i) {
        const auto state = recording_states.get(reinterpret_cast<std::uintptr_t>(list), gen,
                                                reinterpret_cast<std::uintptr_t>(p.resources[i].Get()));
        empty->known[i] = state.known;
        empty->states[i] = state.value;
        if (state.known) ++state_imports;
    }
    return empty;
}
void completed(Plan &p) {
    if (!p.completed && p.resolved && p.copied && p.final_copied && (!p.full_slot || p.full_slot_done) &&
        p.handoff.recorded() == p.handoff.count()) {
        p.completed = true;
        ++complete;
        unsigned count{};
        for (const auto &r : p.recordings)
            if (r.list) ++count;
        if (count > 1) ++multilist_frames;
    }
}
bool prepare_selector(Plan &p) {
    // perf::pooled_selectors: a buffer kept from the slot's retired plan (publish) is rewritten.
    if (!p.selector || !perf::on(perf::pooled_selectors)) {
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = 8;
        desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        auto *device = reinterpret_cast<ID3D12Device *>(cfg.device);
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                   D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                   IID_PPV_ARGS(&p.selector))))
            return false;
    }
    void *mapped{};
    D3D12_RANGE none{};
    if (FAILED(p.selector->Map(0, &none, &mapped))) return false;
    const std::uint64_t native = 0;
    std::memcpy(mapped, &native, 8);
    const D3D12_RANGE written{0, 8};
    p.selector->Unmap(0, &written);
    return true;
}
bool choose(Plan &p, FrameHandoff::Choice choice) {
    if (!p.selector || p.handoff.choice() != FrameHandoff::Choice::undecided || p.handoff.submitted_count())
        return false;
    void *mapped{};
    D3D12_RANGE none{};
    if (FAILED(p.selector->Map(0, &none, &mapped))) return false;
    const std::uint64_t value = choice == FrameHandoff::Choice::native ? 0 : 1;
    std::memcpy(mapped, &value, 8);
    const D3D12_RANGE written{0, 8};
    p.selector->Unmap(0, &written);
    const bool ok = p.handoff.choose(choice) == FrameHandoff::Result::ok;
    return ok;
}
// Predication gates only the draw/copy commands. Native resource barriers and
// binding restoration always execute; only one branch does GPU image work.
struct PredicateScope {
    ID3D12GraphicsCommandList *list;
    explicit PredicateScope(ID3D12GraphicsCommandList *command) : list(command) {
        obs::Predicate current;
        if (!obs::predicate_current(list, current) || current.buffer)
            throw std::runtime_error("stream native predicate is not disabled");
    }
    void native(ID3D12Resource *selector) { list->SetPredication(selector, 0, D3D12_PREDICATION_OP_NOT_EQUAL_ZERO); }
    void full(ID3D12Resource *selector) { list->SetPredication(selector, 0, D3D12_PREDICATION_OP_EQUAL_ZERO); }
    ~PredicateScope() { list->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO); }
};
bool prepare_frame(Plan &p) {
    if (handoff_count(p) > FrameHandoff::capacity) return false;
    tb::Packet actual;
    if (!tb::packet(reader, p.temporal.object, actual) || actual != p.temporal) return false;
    ComPtr<ID3D12Resource> color;
    if (!pin(p.stamp.inputs[0], color) || color == p.resources[0] ||
        !(p.resources[0]->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        return false;
    std::array<std::uintptr_t, FrameHandoff::capacity> packets{p.temporal.object, p.copy};
    for (unsigned i = 0; i < p.late_count; ++i) packets[handoff_indexed(p, i)] = p.late_packets[i];
    if (p.full_slot) packets[2] = p.full_slot;
    packets[final_node(p)] = p.final_copy;
    if (!prepare_selector(p) ||
        p.handoff.prepare({p.stamp.frame, p.stamp.epoch, p.stamp.context, p.stamp.view},
                          std::span(packets).first(handoff_count(p))) != FrameHandoff::Result::ok)
        return false;
    p.prepared = true;
    p.claimed = true;
    silhouette_planned += p.late_count;
    ++prepared_frames;
    ++claimed;
    return true;
}
bool retain_recording(Plan &p, ID3D12GraphicsCommandList *list, std::uint64_t gen,
                      std::span<IUnknown *const> extra = {}) {
    std::array<IUnknown *, resource_capacity + 16> pins{};
    unsigned count = resource_count(p);
    for (unsigned i = 0; i < count; ++i) pins[i] = p.resources[i].Get();
    if (p.selector)
        pins[count++] = p.selector.Get();
    else
        return false;
    if (extra.size() > pins.size() - count) return false;
    for (auto *value : extra) pins[count++] = value;
    return lifetime.retain(list, gen, std::span(pins).first(count));
}
// DLAA (dlaa-identity-path.md): at render scale 1.0 the game ends the frame with
// a native copy (opcode 13, sub_1423C9250, from 0x23c7ad6) from the colour-
// grading output to the view's final attachment instead of the System_Copy quad.
// Every effect then renders at full size and stays native. The plan proves the
// TAA hand-off copy (where the reconstruction enters, as in the scaled presets)
// and that final copy (frame generation's HUD-less anchor): one each, with the
// same owner, packet and extent identities as the scaled route's.
bool build_identity_plan(Plan &p, OutputDiagnostics::Graph &observation) {
    const auto &s = p.stamp;
    std::uintptr_t filters{}, motion_in{}, grade{}, grade_out{}, attachment{}, final_dest{};
    unsigned count{}, taas{}, grades{};
    if (!same_stamp(s) || !field(cfg.image, 0x51cc238, filters) || !field(filters, 0x3c, count) || !count ||
        count > 16) {
        plan_rejection = 1;
        return false;
    }
    for (unsigned i = 0; i < count; ++i) {
        std::uintptr_t e{}, u{}, type{}, input{}, output{};
        unsigned stage{}, order{};
        if (!field(filters, 0x40 + 8 * i, e) || !field(e, 0, stage) || !field(e, 4, order) || !field(e, 8, u) ||
            !field(e, 0x10, input) || !field(e, 0x18, output) || (u && !field(u, 0, type))) {
            plan_rejection = 2;
            return false;
        }
        observation.filters[observation.count++] = {e, stage, order, u, type, input, output};
        if (e == s.layer + 0x140) {
            if (stage != 15 || order != 2 || u != s.layer || input != s.inputs[0].object) {
                plan_rejection = 3;
                return false;
            }
            motion_in = output;
            ++taas;
        }
        if (type == cfg.image + 0x3538400 && stage == 15 && order == 0) {
            grade = u;
            ++grades;
        }
    }
    observation.links = {0, grade, 0, motion_in, 0, 0, 0, 0};
    if (taas != 1 || grades != 1) {
        plan_rejection = 6;
        plan_detail = 1;
        return false;
    }
    if (!field(grade, 0x250, grade_out) || !grade_out) {
        plan_rejection = 6;
        plan_detail = 4;
        return false;
    }
    observation.links[6] = grade_out;
    if (!field(s.view, 0x460, attachment) || !attachment || !field(attachment, 0x58, final_dest) || !final_dest) {
        plan_rejection = 6;
        plan_detail = 6;
        return false;
    }
    std::uintptr_t array{};
    unsigned commands{};
    if (!field(s.context, 0x2b80, commands) || !commands || commands >= 65535 || !field(s.context, 0x2b78, array) ||
        !array || array > UINTPTR_MAX - 16ULL * commands) {
        plan_rejection = 8;
        return false;
    }
    struct Command {
        unsigned key{}, padding{};
        std::uintptr_t packet{};
    };
    static_assert(sizeof(Command) == 16);
    // Copy packets (sub_1423C9250): +0x20 mode, +0x24/+0x28 subresources,
    // +0x30 destination x/y, +0x38 source rectangle, +0x48 an optional pointer.
    const auto copy_shape = [&](std::uintptr_t packet, const std::array<unsigned, 4> &expected) {
        unsigned mode{}, ds{}, ss{};
        std::array<unsigned, 2> xy{};
        std::array<unsigned, 4> rect{};
        std::uintptr_t optional{};
        return field(packet, 0x20, mode) && field(packet, 0x24, ds) && field(packet, 0x28, ss) &&
               field(packet, 0x30, xy) && field(packet, 0x38, rect) && field(packet, 0x48, optional) && !mode && !ds &&
               !ss && !xy[0] && !xy[1] && !optional && rect == expected;
    };
    // full_post: the inputs' upscale slot this thread recorded after TAA's
    // callback (full_post.inl), when verified, joins the plan if it is in this
    // queue exactly once.
    std::uintptr_t slot{};
    unsigned slots{};
    if (full_frame.load() == s.frame && !(render_w == width && render_h == height)) {
        const auto &q = full_slots[s.frame % full_slots.size()];
        if (q.frame == s.frame && q.context == s.context && q.valid && !q.executed &&
            q.source.object == s.inputs[4].object && q.gbuffer4.object == s.inputs[5].object)
            slot = q.packet;
    }
    std::array<Command, 256> block;
    unsigned handoffs{}, finals{};
    for (unsigned base = 0; base < commands; base += 256) {
        const auto n = (std::min)(256U, commands - base);
        if (!pb::copy_memory(array + 16ULL * base, block.data(), n * sizeof(Command))) {
            plan_rejection = 9;
            return false;
        }
        for (unsigned j = 0; j < n; ++j) {
            const auto &c = block[j];
            unsigned raw{};
            if (slot && c.packet == slot) slots += c.key == full_slot_key ? 1U : 2U;
            if (c.key != 0x78000002 && (c.key >> 27) != 16) continue;
            if (!field(c.packet, 0, raw)) {
                plan_rejection = 10;
                return false;
            }
            if ((raw & 31) != 13) continue;
            std::uintptr_t src{}, dst{};
            if (!field(c.packet, 0x18, src) || !field(c.packet, 0x10, dst)) {
                plan_rejection = 11;
                return false;
            }
            if (c.key == 0x78000002) {
                if (src != s.inputs[4].object || dst != motion_in) continue;
                if (++handoffs != 1 || !pb::copy_memory(c.packet + 0x10, p.copy_fields.data(), 64)) {
                    plan_rejection = 12;
                    return false;
                }
                p.copy = c.packet;
                if (!copy_shape(c.packet, s.rectangle)) {
                    plan_rejection = 13;
                    return false;
                }
            } else if (src == grade_out && dst == final_dest) {
                if (++finals != 1 || !pb::copy_memory(c.packet + 0x10, p.final_copy_fields.data(), 64)) {
                    plan_rejection = 12;
                    return false;
                }
                p.final_copy = c.packet;
                if (!copy_shape(c.packet, {0, 0, width, height})) {
                    plan_rejection = 13;
                    return false;
                }
            }
        }
    }
    if (handoffs != 1 || finals != 1) {
        plan_rejection = 15;
        return false;
    }
    p.full_slot = slots == 1 ? slot : 0;
    if (p.full_slot) ++full_counts.slots_planned;
    unsigned again{};
    std::uintptr_t again_array{};
    if (!field(s.context, 0x2b80, again) || again != commands || !field(s.context, 0x2b78, again_array) ||
        again_array != array || !same_stamp(s)) {
        plan_rejection = 16;
        return false;
    }
    tb::Owner transfer, source, destination;
    if (!owner(motion_in, transfer) || !owner(grade_out, source) || !owner(final_dest, destination)) {
        plan_rejection = 7;
        return false;
    }
    if (!pin(s.inputs[4], p.resources[0]) || !pin(transfer, p.resources[1]) || !pin(source, p.resources[2]) ||
        !pin(destination, p.resources[3])) {
        plan_rejection = 17;
        return false;
    }
    for (unsigned i = 0; i < 4; ++i)
        for (unsigned j = 0; j < i; ++j)
            if (p.resources[i] == p.resources[j]) {
                plan_rejection = 17;
                return false;
            }
    return true;
}
// The completed post queue's identity plan. At a reduced render scale only a
// frame full_post armed (full_frame) is planned; any other keeps the game's
// own upscale (unarmed).
bool build_plan(Plan &p) {
    const bool dlaa = render_w == width && render_h == height;
    if (!dlaa && full_frame.load() != p.stamp.frame) {
        ++unarmed;
        return false;
    }
    OutputDiagnostics::Graph observation;
    const auto &s = p.stamp;
    observation.at = {GetTickCount64(), s.frame, s.epoch, s.context, s.view, s.layer, s.bank, s.async};
    plan_rejection = plan_detail = 0;
    p.identity = true;
    bool accepted = false;
    try {
        accepted = build_identity_plan(p, observation);
    } catch (...) {
        observation.reason = 18;
        observation.detail = plan_detail;
        diagnostics.graph(observation, false);
        throw;
    }
    if (accepted) {
        ++identity_plans;
        if (!dlaa) ++full_counts.identity_plans;
    }
    observation.reason = accepted ? 0 : plan_rejection ? plan_rejection : 18;
    observation.detail = plan_detail;
    if (!accepted)
        diagnostics.commands.observe(
            observation,
            [](std::uint64_t at, void *out, std::size_t bytes) {
                return pb::copy_memory(static_cast<std::uintptr_t>(at), out, static_cast<unsigned>(bytes));
            },
            [&] { return same_stamp(s); });
    if (!accepted) ++rejection_counts[(std::min)(observation.reason, 18U)];
    diagnostics.graph(observation, accepted);
    return accepted;
}
std::atomic<FinalSceneObserver> final_scene_observer{};

#include "full_post_slot.inl"
#include "silhouette_route.inl"

bool copy_override(ID3D12GraphicsCommandList *list, const D3D12_TEXTURE_COPY_LOCATION *destination, UINT x, UINT y,
                   UINT z, const D3D12_TEXTURE_COPY_LOCATION *source, const D3D12_BOX *box, std::uintptr_t caller) {
    if (injecting || (filtering() && copy_slot == UINT_MAX)) return false;
    Lock lock;
    if (copy_slot == UINT_MAX || fault.load()) return false;
    auto &p = plans[copy_slot];
    if (!p.prepared) return false;
    D3D12_TEXTURE_COPY_LOCATION d{}, s{};
    D3D12_BOX b{};
    if (!pb::copy_memory(reinterpret_cast<std::uintptr_t>(destination), &d, sizeof(d)) ||
        !pb::copy_memory(reinterpret_cast<std::uintptr_t>(source), &s, sizeof(s)))
        return false;
    if (copy_final) {
        // The native final copy at render scale 1.0 (DLAA): forwarded unchanged on
        // both branches (it is already full size), then frame generation's HUD-less
        // capture reads the final scene target, now in COPY_DEST.
        if (d.pResource != p.resources[3].Get() || s.pResource != p.resources[2].Get()) return false;
        temporal_present::Bindings bindings;
        if (!obs::inspect(list, bindings) || !bindings.known || !bindings.valid) {
            fail(5);
            return false;
        }
        auto *track = track_recording(p, list, bindings.generation, true);
        std::uintptr_t packet{}, command{};
        std::array<std::byte, 64> fields{};
        if (caller != cfg.image + 0x259caf0 || p.final_copied || ++copy_calls != 1 || !track || x || y || z ||
            // A native copy needs its destination in COPY_DEST, and the copy leaves
            // it there: the observer's barrier relies on that. An unknown state (its
            // barrier in an earlier list) is accepted, a contradicting one is not.
            (track->known[2] && (track->states[2] & D3D12_RESOURCE_STATE_COPY_SOURCE) == 0) ||
            (track->known[3] && track->states[3] != D3D12_RESOURCE_STATE_COPY_DEST) ||
            !field(copy_recording, 0x158, packet) || packet != p.final_copy || !field(copy_recording, 0x20, command) ||
            command != reinterpret_cast<std::uintptr_t>(list) ||
            !pb::copy_memory(p.final_copy + 0x10, fields.data(), 64) || fields != p.final_copy_fields ||
            d.Type != D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX || s.Type != d.Type || d.SubresourceIndex ||
            s.SubresourceIndex ||
            (box && (!pb::copy_memory(reinterpret_cast<std::uintptr_t>(box), &b, sizeof(b)) || b.left || b.top ||
                     b.front || b.back != 1 || b.right != width || b.bottom != height))) {
            fail(5);
            return false;
        }
        if (!retain_recording(p, list, bindings.generation) ||
            p.handoff.record(final_node(p), p.final_copy,
                             {reinterpret_cast<std::uintptr_t>(list), bindings.generation}) !=
                FrameHandoff::Result::ok) {
            fail(5);
            return false;
        }
        reinterpret_cast<CopyFn>(originals[2])(list, destination, 0, 0, 0, source, box);
        ++identity_final_copies;
        if (const auto observer = final_scene_observer.load()) {
            injecting = true;
            observer(list, bindings, p.resources[3].Get(), p.stamp.frame, p.stamp.epoch,
                     D3D12_RESOURCE_STATE_COPY_DEST);
            injecting = false;
            if (!obs::matches_current(list, bindings)) {
                fail(5);
                return true;
            }
            ++identity_observed;
        }
        p.final_copied = true;
        ++copies;
        completed(p);
        return true;
    }
    if (d.pResource != p.resources[1].Get() || s.pResource != p.resources[0].Get()) return false;
    temporal_present::Bindings bindings;
    if (!obs::inspect(list, bindings) || !bindings.known || !bindings.valid) {
        fail(5);
        return false;
    }
    // A worker may record the copy on a different list from the resolve. Import
    // that list's observed native states, as draws do; no state crosses lists.
    auto *track = track_recording(p, list, bindings.generation, true);
    std::uintptr_t packet{}, command{};
    std::array<std::byte, 64> fields{};
    if (caller != cfg.image + 0x259caf0 || p.copied || ++copy_calls != 1 || !track || x || y || z || !track->known[0] ||
        !track->known[1] || (track->states[0] & D3D12_RESOURCE_STATE_COPY_SOURCE) == 0 ||
        track->states[1] != D3D12_RESOURCE_STATE_COPY_DEST || !field(copy_recording, 0x158, packet) ||
        packet != p.copy || !field(copy_recording, 0x20, command) ||
        command != reinterpret_cast<std::uintptr_t>(list) || !pb::copy_memory(p.copy + 0x10, fields.data(), 64) ||
        fields != p.copy_fields || d.Type != D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX || s.Type != d.Type ||
        d.SubresourceIndex || s.SubresourceIndex || !box ||
        !pb::copy_memory(reinterpret_cast<std::uintptr_t>(box), &b, sizeof(b)) || b.left || b.top || b.front ||
        b.back != 1 || b.right != render_w || b.bottom != render_h) {
        if (box && b.right && (b.right != render_w || b.bottom != render_h) && !copy_missed) {
            copy_missed = true;
            copy_miss = {b.left, b.top, b.right, b.bottom};
        }
        fail(5);
        return false;
    }
    if (!retain_recording(p, list, bindings.generation) ||
        p.handoff.record(1, p.copy, {reinterpret_cast<std::uintptr_t>(list), bindings.generation}) !=
            FrameHandoff::Result::ok) {
        fail(5);
        return false;
    }
    {
        PredicateScope predicate(list);
        predicate.native(p.selector.Get());
        reinterpret_cast<CopyFn>(originals[2])(list, destination, 0, 0, 0, source, box);
        predicate.full(p.selector.Get());
        const D3D12_BOX full{0, 0, 0, width, height, 1};
        reinterpret_cast<CopyFn>(originals[2])(list, destination, 0, 0, 0, source, &full);
    }
    p.copied = true;
    ++copies;
    completed(p);
    return true;
}
void WINAPI copy_hook(ID3D12GraphicsCommandList *list, const D3D12_TEXTURE_COPY_LOCATION *d, UINT x, UINT y, UINT z,
                      const D3D12_TEXTURE_COPY_LOCATION *s, const D3D12_BOX *box) {
    Guard guard;
    bool replaced = false;
    // full_post's slot copy never runs (full_post_slot.inl).
    if (full_slot_scope != UINT_MAX && !injecting) {
        full_slot_copy(list, d, x, y, z, s, box, reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
        return;
    }
    // An unplanned DLAA copy never runs.
    if (full_final_scope != UINT_MAX && !injecting) {
        full_final_copy(list, d, x, y, z, s, box, reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
        return;
    }
    try {
        replaced = copy_override(list, d, x, y, z, s, box, reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
    } catch (const std::exception &e) {
        Lock lock;
        failure = e.what();
        fail(5);
    }
    if (!replaced) reinterpret_cast<CopyFn>(originals[2])(list, d, x, y, z, s, box);
}
} // namespace
namespace ht = perf::hook_timing;
void full_post_enter(std::uintptr_t context, bool admitted) noexcept {
    full_enter(context, admitted);
}
void full_post_taa(std::uintptr_t context) noexcept {
    full_taa(context);
}
void full_post_exit(std::uintptr_t context) noexcept {
    full_exit(context);
}
void full_post_states(const tb::Snapshot &s, unsigned depth_state, std::uintptr_t depth, unsigned gbuffer4_state,
                      std::uintptr_t gbuffer4) noexcept {
    full_states(s, depth_state, depth, gbuffer4_state, gbuffer4);
}
void post_queue_complete(std::uintptr_t manager, std::uintptr_t context, std::uintptr_t view,
                         std::uintptr_t taa) noexcept {
    const ht::Scope timing(ht::route_queue);
    Guard guard;
    Lock lock;
    if (!accepting.load()) return;
    std::uint64_t frame{};
    std::uintptr_t current_manager{};
    if (!field(cfg.renderer, 0x1f210, frame) || !field(cfg.image, 0x51cc238, current_manager) ||
        manager != current_manager)
        return;
    {
        const auto &q = plans[frame % plans.size()];
        census_observe(frame, context,
                       q.published && q.stamp.frame == frame && q.stamp.context == context ? &q.stamp : nullptr);
    }
    auto &p = plans[frame % plans.size()];
    if (!p.published || p.queue_finished || p.stamp.frame != frame || p.stamp.context != context ||
        p.stamp.view != view || p.stamp.layer != taa || !same_stamp(p.stamp) ||
        p.publish_thread != GetCurrentThreadId())
        return;
    p.queue_finished = true;
    try {
        // full_post's plans also take the late draws (the silhouettes are drawn at
        // the render size into the full-size chain); at DLAA they are native at the
        // full size. A refused late scan leaves them native.
        if (build_plan(p)) {
            p.built = true;
            ++built;
            const bool scaled = !(render_w == width && render_h == height);
            if (scaled) (void)silhouette_candidates(p);
            (void)prepare_frame(p);
            // full_post: the scene capture reads the full-size chain output.
            if (p.prepared && scaled) full_capture_apply(frame);
            watch(static_cast<unsigned>(frame % plans.size()), p);
        } else {
            ++unqualified;
            p.resources = {};
        }
    } catch (const std::exception &e) {
        failure = e.what();
        fail(1);
    }
}
void final_dispatch(pb::Frame *f) {
    auto timing = ht::begin(ht::route_final);
    ++pending;
    const auto slot = final_before(*f);
    bool returned = false;
    __try {
        ht::forward_begin(timing);
        full_candidate(*f, slot);
        mhw_binding_forward(originals[3], f);
        ht::forward_end(timing);
        returned = true;
    } __finally {
        final_after(slot, returned);
        --pending;
        ht::end(timing);
    }
}
void indexed_dispatch(pb::Frame *f) {
    auto timing = ht::begin(ht::route_indexed);
    ++pending;
    const auto previous = indexed_scope;
    indexed_scope = filtering() && !watched_packet(f->gpr[3]) ? IndexedScope{} : indexed_before(*f);
    bool returned = false;
    __try {
        ht::forward_begin(timing);
        mhw_binding_forward(originals[5], f);
        ht::forward_end(timing);
        returned = true;
    } __finally {
        indexed_after(returned);
        silhouette_after(returned);
        indexed_scope = previous;
        --pending;
        ht::end(timing);
    }
}
bool indexed_draw(ID3D12GraphicsCommandList *list, unsigned n, unsigned instances, unsigned first, int base,
                  unsigned instance, std::uintptr_t caller) noexcept {
    if (injecting || (indexed_scope.sample == UINT_MAX && indexed_scope.slot == UINT_MAX)) return false;
    Lock lock;
    try {
        if (indexed_scope.sample != UINT_MAX) indexed_sample(list, n, instances, first, base, instance, caller);
        if (indexed_scope.slot != UINT_MAX && !fault.load() && !abandoned) {
            const bool result = record_indexed(list, n, instances, first, base, instance, caller);
            return result;
        }
    } catch (const std::exception &e) {
        injecting = false;
        failure = e.what();
        fail(14);
    } catch (...) {
        injecting = false;
        fail(14);
    }
    return indexed_scope.replaced;
}
unsigned copy_before(const pb::Frame &f) {
    Lock lock;
    if (fault.load() || abandoned) return UINT_MAX;
    unsigned slot = UINT_MAX;
    bool final = false;
    for (unsigned i = 0; i < plans.size(); ++i) {
        const auto &q = plans[i];
        if (!q.prepared) continue;
        const bool handoff = q.copy == f.gpr[3] && !q.copied,
                   closing = q.identity && q.final_copy == f.gpr[3] && !q.final_copied;
        if (!handoff && !closing) continue;
        if (slot != UINT_MAX) {
            fail(5);
            return UINT_MAX;
        }
        slot = i;
        final = closing;
    }
    if (slot == UINT_MAX) return slot;
    std::uintptr_t backend{};
    if (!field(f.gpr[2], 8, backend) || backend != cfg.backend || !field(f.gpr[1], 8, copy_recording) ||
        !copy_recording) {
        fail(5);
        return UINT_MAX;
    }
    copy_calls = 0;
    copy_final = final;
    return slot;
}
void copy_after(bool returned) {
    if (copy_slot != UINT_MAX) {
        Lock lock;
        if (!returned || copy_calls != 1 || !(copy_final ? plans[copy_slot].final_copied : plans[copy_slot].copied))
            fail(5);
    }
}
void copy_dispatch(pb::Frame *f) {
    auto timing = ht::begin(ht::route_copy);
    ++pending;
    const auto old_slot = copy_slot;
    const auto old_recording = copy_recording;
    const auto old_calls = copy_calls;
    const auto old_final = copy_final;
    const auto old_full = full_slot_scope;
    const auto old_full_state = full_slot_state;
    const auto old_full_calls = full_slot_calls;
    const auto old_final_scope = full_final_scope;
    const auto old_final_calls = full_final_calls;
    // full_post's slot packet first: its copy is never the native one. Then an
    // unplanned DLAA copy, whose copy is dropped.
    full_slot_scope = full_slot_before(*f);
    full_final_scope = full_slot_scope == UINT_MAX ? full_final_before(*f) : UINT_MAX;
    copy_final = false;
    copy_slot =
        full_slot_scope != UINT_MAX || full_final_scope != UINT_MAX || (filtering() && !watched_packet(f->gpr[3]))
            ? UINT_MAX
            : copy_before(*f);
    bool returned = false;
    __try {
        ht::forward_begin(timing);
        mhw_binding_forward(originals[4], f);
        ht::forward_end(timing);
        returned = true;
    } __finally {
        copy_after(returned);
        if (full_slot_scope != UINT_MAX) full_slot_after();
        copy_slot = old_slot;
        copy_recording = old_recording;
        copy_calls = old_calls;
        copy_final = old_final;
        full_slot_scope = old_full;
        full_slot_state = old_full_state;
        full_slot_calls = old_full_calls;
        full_final_scope = old_final_scope;
        full_final_calls = old_final_calls;
        --pending;
        ht::end(timing);
    }
}
void observe_final_scene(FinalSceneObserver observer) noexcept {
    final_scene_observer.store(observer);
}
bool prepare(const tb::Config &c, unsigned w, unsigned h, unsigned rw, unsigned rh, std::string &reason) {
    cfg = c;
    width = w;
    height = h;
    render_w = rw;
    render_h = rh;
    // DLAA renders at the scene size; the route then runs at identity.
    if (!valid_filter_geometry({rw, rh, w, h})) {
        reason = "stream extent";
        return false;
    }
    struct Gate {
        unsigned rva, bytes;
        const char *sha;
    };
    constexpr Gate gates[]{{0x23d2370, 1047, "0cf82d2b8b6259fbb5cad0c7468a780bb7026d9a20f720e9bbd8b5cd501fffc3"},
                           {0x23ca8b0, 206, "b27d637cadf610c779092c55dc94cda7812d4525fbb864fc26acad0b91620edc"},
                           {0x259f330, 1664, "ec12eca9df5bc5598eed515d9e882fad9d0afcf54c0488f9eaf98127ab046c32"},
                           {0x23ca4a0, 445, "97d8e82e322796881438a9318a4f5170401bf031b28d21afd19c77256a82816e"},
                           {0x24d0780, 4636, "5853f4dd6561e66842edab799f7d87ea3042f7b873dd72ab848e612579ff3e7b"},
                           {0x259d740, 214, "82e2ac66bd7cfa4ce7a61244efaaa6f7e3db0bbb766606962bb23fcb91daafba"},
                           {0x259f9b0, 1531, "7abd32763fb614e868e425a607d67d717205988f34a520fed0884fd51cb720c8"},
                           {0x259e670, 2554, "088e67c77b7e96af64c83c8297f6fc9f6d355dbc170530dda125b41f4cb53727"},
                           {0x259c650, 1214, "299ede85b4e2bbfb25f0ae2c20e003317efc2809c591eb46677ec154495daf88"},
                           {0x259e470, 512, "7db0a032642249004ebda64a7e8442407851cec8f25208e8966a63990ddade49"},
                           {0x24e6e50, 910, "d961028c0e34625cd23343327e3ba3ad401c4d439a5c653a9ca0e6893836bc04"},
                           {0x2297e20, 656, "58fe604c883de6d505ce2211797c2efaddc360046d10bb14c2e50fafbae4ab6d"},
                           {0x23c7831, 740, "0da63ff39cc114c78483728e19221e1dd22806398522ec0095ec6c6349801247"}};
    for (const auto &g : gates)
        if (!pb::verify_code(c.image, g.rva, g.bytes, g.sha)) {
            reason = "stream code gate";
            return false;
        }
    targets[3] = c.image + 0x2297e20;
    targets[4] = c.image + 0x259c650;
    targets[5] = c.image + 0x259d740;
    // full_post (full_post.hpp): the tone map's callback and the projection
    // helper are hooked; setViewport and the copy helper are called.
    constexpr Gate full_gates[]{{0x23c9250, 313, "48dc45a224fa9a3d25c884c46039221b34a7b8e21e880889250fd997f3ecb283"},
                                {0x23d0330, 1271, "cd3085a7946eaef138f9c0af8b882a4fbc9c8df969107b954b278e157c0d907e"},
                                {0x23cb520, 301, "4fa9724b3eff99c5bddb0295302b6f73d1836521129f59f911ad7b94bb910ac8"}};
    for (const auto &g : full_gates)
        if (!pb::verify_code(c.image, g.rva, g.bytes, g.sha)) {
            reason = "full_post code gate";
            return false;
        }
    targets[7] = c.image + 0x24d0780;
    targets[8] = c.image + 0x23cb520;
    full_viewport = reinterpret_cast<FullViewportFn>(c.image + 0x23d0330);
    full_copy = reinterpret_cast<FullCopyFn>(c.image + 0x23c9250);
    // The inputs' upscale (full_post_inputs.hpp); without it no frame arms.
    if (std::string why; !full_inputs.prepare(reinterpret_cast<ID3D12Device *>(c.device), w, h, why))
        full_inputs_failure = why;
    auto *device = reinterpret_cast<ID3D12Device *>(c.device);
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator))) ||
        FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                                         IID_PPV_ARGS(&list))) ||
        FAILED(list->Close()) || !pb::system_method(reinterpret_cast<std::uintptr_t>(list.Get()), 16, targets[2])) {
        reason = "stream method discovery";
        return false;
    }
    const std::array<void *, 9> detours{nullptr,
                                        nullptr,
                                        reinterpret_cast<void *>(copy_hook),
                                        reinterpret_cast<void *>(scaled_stream_final_entry),
                                        reinterpret_cast<void *>(scaled_copy_execution_entry),
                                        reinterpret_cast<void *>(scaled_indexed_execution_entry),
                                        nullptr,
                                        reinterpret_cast<void *>(scaled_full_tone_map_entry),
                                        reinterpret_cast<void *>(scaled_full_project_entry)};
    if (!originals[api_hook]) {
        reason = "stream AL chain";
        return false;
    }
    enabled[api_hook] = true;
    for (const auto &spec : hook_specs) {
        auto &hook = owned[spec.slot];
        hook.name = spec.name;
        hook.pattern = spec.pattern;
        hook.target = reinterpret_cast<void *>(targets[spec.slot]);
        if (!astera::hook_owned(hook, detours[spec.slot], &originals[spec.slot])) {
            reason = "stream hook preparation";
            return false;
        }
        enabled[spec.slot] = true;
    }
    return true;
}
void begin() noexcept {
    accepting = true;
}
void publish(const tb::Snapshot &s, const tb::Packet &packet) noexcept {
    Lock lock;
    if (!accepting.load()) return;
    auto &p = plans[s.frame % plans.size()];
    if (p.prepared) {
        fail(7);
        return;
    }
    unwatch(static_cast<unsigned>(s.frame % plans.size()));
    // Expanded overlay recipes must not create a half-megabyte stack temporary
    // on the game's render worker when a retired ring slot is reused.
    // perf::pooled_selectors: the GPU no longer reads the previous plan's selector
    // (a prepared plan must be retired before its slot is published again, and an
    // unprepared one was never submitted), so the slot keeps the buffer.
    auto selector = perf::on(perf::pooled_selectors) ? std::move(p.selector) : ComPtr<ID3D12Resource>{};
    std::destroy_at(&p);
    std::construct_at(&p);
    p.stamp = s;
    p.temporal = packet;
    p.published = true;
    p.publish_thread = GetCurrentThreadId();
    ++published;
    p.selector = std::move(selector);
}
bool copy_region(ID3D12GraphicsCommandList *list, const D3D12_TEXTURE_COPY_LOCATION *d, UINT x, UINT y, UINT z,
                 const D3D12_TEXTURE_COPY_LOCATION *s, const D3D12_BOX *box) noexcept {
    if (!originals[2]) return false;
    reinterpret_cast<CopyFn>(originals[2])(list, d, x, y, z, s, box);
    return true;
}
bool ready(ID3D12GraphicsCommandList *list, std::uint64_t gen, const tb::Snapshot &s,
           std::uintptr_t recording) noexcept {
    Lock lock;
    auto &p = plans[s.frame % plans.size()];
    const auto note = [&](ReadinessTrace::Reason reason, unsigned stage = 12) {
        readiness.note(reason, [&] {
            ReadinessTrace::Snapshot v;
            v.at = {GetTickCount64(), s.frame, s.epoch, reinterpret_cast<std::uintptr_t>(list), gen, recording};
            v.request = {s.context, s.view, s.bank, s.async};
            v.plan = {s.frame % plans.size(), p.stamp.frame,   p.stamp.epoch, p.published,  p.built,      p.claimed, 0,
                      p.temporal.object,      p.stamp.context, p.stamp.view,  p.stamp.bank, p.stamp.async};
            v.active = {active.slot,
                        reinterpret_cast<std::uintptr_t>(active.list),
                        active.generation,
                        active.recording,
                        active.resolved,
                        active.copied,
                        0,
                        active.list ? plans[active.slot].stamp.frame : 0,
                        active.list ? plans[active.slot].stamp.epoch : 0};
            v.accepting = accepting.load();
            v.fault = fault.load();
            v.stamp_equal = p.stamp == s;
            v.stage = stage;
            return v;
        });
        return reason == ReadinessTrace::accepted;
    };
    // Keep the original short-circuit order. Recording a reason never permits a
    // rejected route, changes a packet, or acquires additional native resources.
    if (!accepting.load() && !p.prepared) return note(ReadinessTrace::inactive);
    if (fault.load()) return note(ReadinessTrace::faulted);
    if (active.list) return note(ReadinessTrace::active_route);
    if (!gen || !recording) return note(ReadinessTrace::arguments);
    if (!p.built) return note(ReadinessTrace::unbuilt);
    if (p.resolved) return note(ReadinessTrace::claimed);
    if (p.stamp != s) return note(ReadinessTrace::stamp);
    if (!p.prepared) return note(ReadinessTrace::unbuilt);
    tb::Packet packet;
    if (!tb::packet(reader, p.temporal.object, packet) || packet != p.temporal) return note(ReadinessTrace::packet);
    const FrameHandoff::Recording recording_id{reinterpret_cast<std::uintptr_t>(list), gen};
    const auto existing = p.handoff.uses()[0].recording;
    if (!track_recording(p, list, gen, true) || !retain_recording(p, list, gen) ||
        (existing ? existing != recording_id
                  : p.handoff.record(0, p.temporal.object, recording_id) != FrameHandoff::Result::ok)) {
        note(ReadinessTrace::retention);
        fail(8);
        return false;
    }
    note(ReadinessTrace::accepted);
    active = {};
    active.slot = static_cast<unsigned>(s.frame % plans.size());
    active.list = list;
    active.generation = gen;
    active.recording = recording;
    return true;
}
bool full_contract(const tb::Snapshot &s) noexcept {
    Lock lock;
    const auto &p = plans[s.frame % plans.size()];
    return p.prepared && p.stamp == s;
}
bool owns_packet(std::uintptr_t packet) noexcept {
    if (filtering() && !watched_packet(packet)) return false;
    Lock lock;
    if (fault.load() || abandoned) return false;
    for (const auto &p : plans)
        if (p.prepared && p.temporal.object == packet && !p.resolved) return true;
    return false;
}
void resolved(bool ok, bool reconstructed) noexcept {
    Lock lock;
    if (!active.list || !ok) {
        fail(9);
        return;
    }
    auto &p = plans[active.slot];
    if (!choose(p, reconstructed ? FrameHandoff::Choice::reconstructed : FrameHandoff::Choice::spatial)) {
        fail(9);
        return;
    }
    p.resolved = true;
    if (!reconstructed) ++spatial_frames;
    active = {};
    completed(p);
}
bool observe_resolve(std::uintptr_t packet, ID3D12GraphicsCommandList *list, std::uint64_t gen) noexcept {
    Lock lock;
    if (fault.load() || abandoned) return false;
    for (auto &p : plans)
        if (p.prepared && !p.resolved && p.temporal.object == packet) {
            if (resolve_slot != UINT_MAX) {
                fail(8);
                return false;
            }
            if (!track_recording(p, list, gen, true) || !retain_recording(p, list, gen) ||
                p.handoff.record(0, packet, {reinterpret_cast<std::uintptr_t>(list), gen}) !=
                    FrameHandoff::Result::ok) {
                fail(8);
                return false;
            }
            resolve_slot = static_cast<unsigned>(p.stamp.frame % plans.size());
            return true;
        }
    return false;
}
void finish_resolve(std::uintptr_t packet, bool returned) noexcept {
    Lock lock;
    if (resolve_slot != UINT_MAX && plans[resolve_slot].temporal.object == packet) resolve_slot = UINT_MAX;
    for (auto &p : plans)
        if (p.prepared && p.temporal.object == packet && !p.resolved) {
            if (!returned || !p.handoff.uses()[0].recording || !choose(p, FrameHandoff::Choice::native)) {
                fail(9);
                return;
            }
            p.resolved = true;
            ++native_frames;
            if (active.list && active.slot == p.stamp.frame % plans.size()) active = {};
            completed(p);
        }
}
void observe_submission(std::uint64_t queue, std::uint64_t tick,
                        std::span<const std::array<std::uint64_t, 2>> lists) noexcept {
    Lock lock;
    diagnostics.submit(queue, tick, lists);
}
void barriers(ID3D12GraphicsCommandList *list, unsigned n, const D3D12_RESOURCE_BARRIER *data) noexcept {
    if (injecting) return;
    // Only aliasing barriers and transitions of listed plan resources reach the
    // journal; an unreadable or oversized batch takes the locked path and fails there.
    if (filtering() && n <= 4096) {
        bool listed = false;
        for (unsigned j = 0; j < n && !listed; ++j) {
            D3D12_RESOURCE_BARRIER b{};
            listed = !pb::copy_memory(reinterpret_cast<std::uintptr_t>(data + j), &b, sizeof(b)) ||
                     b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING ||
                     (b.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION &&
                      watched_resource(reinterpret_cast<std::uintptr_t>(b.Transition.pResource)));
        }
        if (!listed) return;
    }
    Lock lock;
    if (fault.load()) return;
    const auto slot = copy_slot != UINT_MAX ? copy_slot : resolve_slot;
    temporal_present::Bindings bindings;
    if (!obs::inspect(list, bindings) || !bindings.known || !bindings.valid) return;
    if (n > 4096) {
        fail(10);
        return;
    }
    for (unsigned j = 0; j < n; ++j) {
        D3D12_RESOURCE_BARRIER b{};
        if (!pb::copy_memory(reinterpret_cast<std::uintptr_t>(data + j), &b, sizeof(b))) {
            fail(10);
            return;
        }
        // Observe the whole recording, including barriers emitted before an owned
        // native callback. Only already pinned plan resources may enter the journal.
        // A resource shared by several plans/roles is observed exactly once.
        std::array<ID3D12Resource *, 32 * resource_capacity> visited{};
        unsigned visited_count{};
        for (auto &p : plans)
            if (p.prepared && p.handoff.submitted_count() != p.handoff.count()) {
                for (unsigned i = 0; i < resource_count(p); ++i) {
                    auto *resource = p.resources[i].Get();
                    if (!resource) continue;
                    const bool alias =
                        b.Type == D3D12_RESOURCE_BARRIER_TYPE_ALIASING &&
                        (!b.Aliasing.pResourceBefore || !b.Aliasing.pResourceAfter ||
                         b.Aliasing.pResourceBefore == resource || b.Aliasing.pResourceAfter == resource);
                    if (!alias &&
                        (b.Type != D3D12_RESOURCE_BARRIER_TYPE_TRANSITION || b.Transition.pResource != resource))
                        continue;
                    if (std::find(visited.begin(), visited.begin() + visited_count, resource) ==
                        visited.begin() + visited_count) {
                        visited[visited_count++] = resource;
                        const bool full = !alias && !b.Flags &&
                                          (b.Transition.Subresource == 0 || b.Transition.Subresource == UINT_MAX);
                        const auto result = recording_states.observe(
                            reinterpret_cast<std::uintptr_t>(list), bindings.generation,
                            reinterpret_cast<std::uintptr_t>(resource), full,
                            full ? static_cast<unsigned>(b.Transition.StateBefore) : 0,
                            full ? static_cast<unsigned>(b.Transition.StateAfter) : 0, p.resources[i]);
                        if (result != StateJournal::Result::ok) {
                            fail(10);
                            return;
                        }
                        ++state_observations;
                        if (slot == UINT_MAX) ++state_unscoped;
                    }
                    // Existing plan records receive the update, without enrolling unrelated
                    // recordings in a plan or carrying knowledge across Reset generations.
                    if (auto *track = track_recording(p, list, bindings.generation, false)) {
                        const auto state =
                            recording_states.get(reinterpret_cast<std::uintptr_t>(list), bindings.generation,
                                                 reinterpret_cast<std::uintptr_t>(resource));
                        track->known[i] = state.known;
                        track->states[i] = state.value;
                    }
                }
            }
    }
}
bool intersects(const Plan &p, std::span<const std::array<std::uint64_t, 2>> batch) {
    for (const auto &use : p.handoff.uses())
        for (const auto &entry : batch)
            if (use.recording && use.recording == FrameHandoff::Recording{entry[0], entry[1]}) return true;
    return false;
}
unsigned batch_recordings(std::span<const std::array<std::uint64_t, 2>> batch,
                          std::array<FrameHandoff::Recording, 128> &out) {
    unsigned count{};
    for (const auto &entry : batch)
        if (entry[0] && entry[1]) out[count++] = {entry[0], entry[1]};
    return count;
}
bool submission_needed(std::span<const std::array<std::uint64_t, 2>> batch) noexcept {
    Lock lock;
    for (const auto &entry : batch)
        if (lifetime.tracked(reinterpret_cast<IUnknown *>(entry[0]), entry[1])) return true;
    return false;
}
bool validate_submission(std::uint64_t queue, std::span<const std::array<std::uint64_t, 2>> batch,
                         std::uint64_t signal) noexcept {
    Lock lock;
    if (batch.size() > 128) return false;
    std::array<FrameHandoff::Recording, 128> records{};
    const auto count = batch_recordings(batch, records);
    for (const auto &p : plans)
        if (p.prepared && intersects(p, batch)) {
            unsigned end{};
            if (p.handoff.validate_submission(queue, std::span(records).first(count), signal, end) !=
                FrameHandoff::Result::ok)
                return false;
        }
    return true;
}
bool submitted_batch(std::uint64_t queue, std::span<const std::array<std::uint64_t, 2>> batch,
                     std::uint64_t signal) noexcept {
    Lock lock;
    if (batch.size() > 128) {
        fail(11);
        return false;
    }
    std::array<FrameHandoff::Recording, 128> records{};
    const auto count = batch_recordings(batch, records);
    // Validate the entire batch before changing any transaction or pin receipt.
    for (const auto &p : plans)
        if (p.prepared && intersects(p, batch)) {
            unsigned end{};
            if (p.handoff.validate_submission(queue, std::span(records).first(count), signal, end) !=
                FrameHandoff::Result::ok) {
                fail(11);
                return false;
            }
        }
    for (auto &p : plans)
        if (p.prepared && intersects(p, batch)) {
            if (p.handoff.submitted(queue, std::span(records).first(count), signal) != FrameHandoff::Result::ok) {
                fail(11);
                return false;
            }
            if (p.handoff.submitted_count() == p.handoff.count()) ++submitted_frames;
        }
    for (const auto &entry : batch) {
        auto *list = reinterpret_cast<IUnknown *>(entry[0]);
        if (lifetime.tracked(list, entry[1]) && !lifetime.submitted(list, entry[1], signal)) {
            fail(11);
            return false;
        }
    }
    return true;
}
void reject_submission(std::span<const std::array<std::uint64_t, 2>> batch) noexcept {
    Lock lock;
    // Native Execute is always forwarded. Seal outstanding selectors before it,
    // so a late CPU callback cannot change a value the GPU may already be reading.
    for (auto &p : plans)
        if (p.prepared && intersects(p, batch) && p.handoff.choice() == FrameHandoff::Choice::undecided)
            (void)choose(p, FrameHandoff::Choice::native);
    fail(11);
}
bool reset(ID3D12GraphicsCommandList *list, std::uint64_t gen) noexcept {
    Lock lock;
    if (!lifetime.reset(list, gen)) {
        fail(12);
        return false;
    }
    if (!recording_states.reset(reinterpret_cast<std::uintptr_t>(list), gen)) {
        fail(12);
        return false;
    }
    for (auto &p : plans)
        if (p.prepared) {
            const auto result = p.handoff.reset({reinterpret_cast<std::uintptr_t>(list), gen});
            if (result != FrameHandoff::Result::ok && result != FrameHandoff::Result::missing) {
                fail(12);
                return false;
            }
        }
    return true;
}
void collect(std::uint64_t value) noexcept {
    Lock lock;
    if (!fault.load()) {
        lifetime.collect(value);
        for (auto &p : plans)
            if (p.prepared && p.handoff.retired(value)) p.prepared = false;
    }
}
bool drained() noexcept {
    Lock lock;
    if (pending.load() || active.list) return false;
    for (const auto &p : plans)
        if (p.prepared && p.handoff.submitted_count() != p.handoff.count()) return false;
    return true;
}
bool callbacks_idle() noexcept {
    return pending.load() == 0;
}
bool abandon() noexcept {
    Lock lock;
    if (accepting.load() || pending.load()) return false;
    // Never report unresolved recordings as submitted/completed or release pins.
    // Already published selectors are immutable; undecided selectors remain
    // native and cannot be changed by a later callback after the fault cutoff.
    fail(13);
    for (auto &p : plans)
        if (p.prepared && p.handoff.choice() == FrameHandoff::Choice::undecided)
            if (!choose(p, FrameHandoff::Choice::native)) return false;
    active = {};
    abandoned = true;
    return true;
}
void stop() noexcept {
    accepting = false;
}
namespace {
bool idle_locked() noexcept {
    if (accepting.load() || fault.load() || abandoned || active.list) return false;
    for (const auto &p : plans)
        if (p.prepared) return false;
    return true;
}
} // namespace
bool idle() noexcept {
    Lock lock;
    return idle_locked();
}
bool resize(unsigned rw, unsigned rh, unsigned w, unsigned h, std::string &reason) noexcept {
    try {
        Lock lock;
        if (!idle_locked()) {
            reason = "stream route not idle";
            return false;
        }
        if (!valid_filter_geometry({rw, rh, w, h})) {
            reason = "stream extent";
            return false;
        }
        // full_post's buffers follow the output (their old uses are complete: idle).
        if (full_inputs.ready() && (w != full_inputs.width() || h != full_inputs.height())) {
            if (std::string why; !full_inputs.prepare(reinterpret_cast<ID3D12Device *>(cfg.device), w, h, why))
                full_inputs_failure = why;
        }
        render_w = rw;
        render_h = rh;
        width = w;
        height = h;
        for (auto &p : silhouette_pipelines) p = {};
        // Built but never prepared plans hold the old geometry; publish() would
        // replace them anyway. A pooled selector stays with its slot.
        for (unsigned i = 0; i < plans.size(); ++i) {
            auto &p = plans[i];
            unwatch(i);
            auto selector = std::move(p.selector);
            std::destroy_at(&p);
            std::construct_at(&p);
            p.selector = std::move(selector);
        }
        final_missed = copy_missed = false;
        return true;
    } catch (...) {
        reason = "stream resize";
        return false;
    }
}
bool unhook() noexcept {
    bool ok = true;
    for (const auto &spec : hook_specs) ok = astera::unhook_owned(owned[spec.slot]) && ok;
    return ok;
}
void chains_live(bool yes) noexcept {
    chain_live.store(yes, std::memory_order_release);
}
void disabled(bool yes) noexcept {
    if (yes) {
        chain_live.store(false, std::memory_order_release);
        Lock lock;
        enabled.fill(false);
        recording_states.clear();
    }
}
bool healthy() noexcept {
    return !fault.load();
}
void write_summary(std::ostream &o) {
    Lock lock;
    const auto a = [&](const auto &v) {
        o << '[';
        for (unsigned i = 0; i < v.size(); ++i) {
            if (i) o << ',';
            o << v[i];
        }
        o << ']';
    };
    o << "{\"fault\":" << fault.load() << ",\"published\":" << published << ",\"built\":" << built
      << ",\"claimed\":" << claimed << ",\"identity\":{\"plans\":" << identity_plans
      << ",\"final_copies\":" << identity_final_copies << ",\"observed\":" << identity_observed << '}'
      << ",\"spatial_frames\":" << spatial_frames << ",\"native_frames\":" << native_frames
      << ",\"failure\":" << std::quoted(failure) << ",\"unarmed\":" << unarmed << ",\"plan_rejections\":";
    a(rejection_counts);
    o << ",\"readiness\":";
    a(readiness.reasons());
    // v0.24.10: in the summary (the full report is quiet in the alpha); each
    // census sample is written once.
    o << ",\"post_census\":";
    census_write(o);
    o << ",\"full_post\":";
    full_post_write(o);
    // Late indexed draws (hunter silhouette pieces): a rejected scan leaves the frame native.
    o << ",\"late_draws\":{\"planned\":" << silhouette_planned << ",\"rejected\":" << indexed_candidates_rejected
      << ",\"reasons\":";
    a(indexed_candidate_reasons);
    o << ",\"max\":" << indexed_candidates_max << ",\"capacity\":" << Plan::late_capacity << '}';
    // Which late draws drew the output-sized silhouette, and why others stayed native.
    o << ",\"silhouette\":";
    silhouette_summary(o);
    o << ",\"latest_rejection\":";
    if (diagnostics.rejected_graphs() != summarized_rejections)
        diagnostics.write_latest_rejection(o);
    else
        o << "null";
    summarized_rejections = diagnostics.rejected_graphs();
    o << '}';
}
void write(std::ostream &o) {
    Lock lock;
    bool off = true;
    for (bool b : enabled) off = off && !b;
    unsigned unresolved{}, unsubmitted{};
    for (const auto &p : plans)
        if (p.prepared) {
            unresolved += !p.resolved;
            unsubmitted += p.handoff.submitted_count() != p.handoff.count();
        }
    o << "{\"silhouette_probe\":";
    indexed_write(o);
    o << ",\"silhouette_output\":";
    silhouette_write(o);
    o << ",\"schema\":4,\"fault\":" << fault.load() << ",\"pending\":" << pending.load()
      << ",\"hooks_disabled\":" << off << ",\"published\":" << published << ",\"built\":" << built
      << ",\"unqualified\":" << unqualified << ",\"unarmed\":" << unarmed << ",\"claimed\":" << claimed
      << ",\"copies\":" << copies << ",\"complete\":" << complete << ",\"prepared_frames\":" << prepared_frames
      << ",\"spatial_frames\":" << spatial_frames << ",\"multilist_frames\":" << multilist_frames
      << ",\"submitted_frames\":" << submitted_frames << ",\"native_frames\":" << native_frames
      << ",\"unresolved_frames\":" << unresolved << ",\"unsubmitted_frames\":" << unsubmitted
      << ",\"abandoned\":" << abandoned << ",\"selection\":\"immutable-frame-predicate-v1\""
      << ",\"active\":" << (active.list != nullptr) << ",\"retained\":" << lifetime.retained()
      << ",\"released\":" << lifetime.released() << ",\"live\":" << lifetime.live() << ",\"peak\":" << lifetime.peak()
      << ",\"final_captured\":" << final_captured << ",\"final_rejected\":" << final_rejected
      << ",\"plan_rejection\":" << plan_rejection << ",\"plan_detail\":" << plan_detail
      << ",\"failure\":" << std::quoted(failure);
    o << ",\"recording_states\":{\"schema\":1,\"policy\":\"native-recording-observation-v1\",\"observations\":"
      << state_observations << ",\"outside_selected_callbacks\":" << state_unscoped << ",\"imported\":" << state_imports
      << "},\"readiness\":";
    readiness.write(o, 4);
    o << ",\"diagnostics\":";
    diagnostics.write(o);
    o << ",\"extent\":{\"render\":[" << render_w << ',' << render_h << "],\"output\":[" << width << ',' << height
      << "],\"final_miss\":[" << final_miss[0] << ',' << final_miss[1] << ',' << final_miss[2] << ',' << final_miss[3]
      << "],\"copy_miss\":[" << copy_miss[0] << ',' << copy_miss[1] << ',' << copy_miss[2] << ',' << copy_miss[3]
      << "]}";
    o << '}';
}
} // namespace mhw::scaled_temporal::stream_route
extern "C" void mhw_scaled_stream_final(mhw::projection_binding::Frame *f) {
    mhw::scaled_temporal::stream_route::final_dispatch(f);
}
extern "C" void mhw_scaled_copy_execution(mhw::projection_binding::Frame *f) {
    mhw::scaled_temporal::stream_route::copy_dispatch(f);
}
extern "C" void mhw_scaled_indexed_execution(mhw::projection_binding::Frame *f) {
    mhw::scaled_temporal::stream_route::indexed_dispatch(f);
}
extern "C" void mhw_scaled_full_tone_map(mhw::projection_binding::Frame *f) {
    mhw::scaled_temporal::stream_route::full_tone_map_dispatch(f);
}
extern "C" void mhw_scaled_full_project(mhw::projection_binding::Frame *f) {
    mhw::scaled_temporal::stream_route::full_project_dispatch(f);
}
namespace mhw::scaled_temporal::stream_route {
template <auto Detour> using Chained = astera::Gate<Detour, chain_live, originals, api_hook>;
void astera_hooks(std::vector<astera::Hook> &out) {
    out.push_back({astera::method(astera::command_list, 16), reinterpret_cast<void *>(&Chained<&copy_hook>::call),
                   &originals[api_hook]});
}
} // namespace mhw::scaled_temporal::stream_route
