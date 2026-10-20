#include "activation.hpp"
#include "../game_memory/layout.hpp"
#include "../scaled_temporal/stream_route.hpp"
#include "../astera/astera_hooks.hpp"
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <atomic>
#include <array>
#include <ostream>

extern "C" {
// Aligned interlocked scalars are also read by the two leaf ASM gate overrides.
alignas(8) volatile LONG64 mhw_aa_context{};
alignas(8) volatile LONG64 mhw_aa_thread{};
alignas(8) volatile LONG64 mhw_aa_table_forced{}, mhw_aa_sharpen_suppressed{};
void *mhw_aa_table_original{}, *mhw_aa_sharpen_original{};
void *mhw_aa_table_resume{}, *mhw_aa_sharpen_resume{};
void mhw_aa_frame_entry();
void mhw_aa_taa_entry();
void mhw_aa_queue_entry();
void mhw_aa_jitter2_entry();
void mhw_aa_jitter1_entry();
void mhw_aa_jitter3_entry();
void mhw_aa_table_entry();
void mhw_aa_sharpen_entry();
void mhw_binding_forward(void *, mhw::projection_binding::Frame *);
}

namespace mhw::continuous_dlaa::activation {
namespace {
namespace pb = projection_binding;
std::uintptr_t image{}, renderer{};
// v0.30.0: the eight hooks on al.hooks under AL (astera::OwnedHook), first in
// their chains (the stubs read their caller). Patterns: each target's first 16
// original bytes (clean image; the gates in prepare() cover them). The two
// leaf sites are TAA2's comparisons inside 0x2392150, mid-function: AL's entry
// clobbers only r11, which is dead there (docs/architecture.md, section 5).
struct HookSpec {
    unsigned rva;
    const char *name, *pattern;
};
constexpr HookSpec hook_specs[]{
    {0x228e770, "mhw_dlss AA:CameraDraw", "48 89 5C 24 10 48 89 6C 24 18 56 41 54 41 55 41"},
    {0x2392150, "mhw_dlss AA:Taa2Execute", "48 89 54 24 10 55 53 48 8D 6C 24 88 48 81 EC 78"},
    {0x23931e0, "mhw_dlss AA:Taa2Jitter", "48 83 EC 28 48 8B 05 95 12 E3 02 48 8B 90 50 F4"},
    {0x2596090, "mhw_dlss AA:Jitter1", "0F 57 C0 48 8B D1 0F 2E 81 B0 01 00 00 74 5C 83"},
    {0x25979b0, "mhw_dlss AA:Jitter3", "48 83 EC 28 48 8B 15 C5 CA C2 02 48 8B 82 50 F4"},
    {0x239217d, "mhw_dlss AA:Taa2TableCompare", "80 B8 E4 E9 00 00 00 0F 84 39 10 00 00 48 8B 82"},
    {0x2392f8e, "mhw_dlss AA:Taa2SharpenCompare", "80 BB 87 01 00 00 00 4C 8B BC 24 50 01 00 00 4C"},
    {0x24f1fc0, "mhw_dlss AA:PostQueue", "40 55 56 41 55 41 57 48 83 EC 48 C6 41 39 00 4D"}};
static_assert(
    [] {
        for (const auto &h : hook_specs)
            if (!astera::valid_pattern<16>(h.pattern)) return false;
        return true;
    }(),
    "each activation pattern is its target's first 16 bytes");
std::array<astera::OwnedHook, std::size(hook_specs)> hooks{};
std::array<void *, std::size(hook_specs)> originals{};
std::atomic<bool> requested{};
// The player's DLSS Off (yield): the game's own AA runs as without the mod
// until the mod takes it over again. Read with requested at each use.
std::atomic<bool> yielded{};
std::atomic<bool> failure_latched{};
std::atomic<unsigned> pending{}, fault{};
std::atomic<std::uint64_t> owned_frames{}, units{}, admitted{}, rejected{}, competing{}, jitter_suppressed{};
std::array<std::atomic<std::uint64_t>, 4> choices{};
std::atomic<std::uint64_t> queue_visits{}, queue_inserted{}, queue_existing{}, queue_rejected{}, execute_unowned{};
std::atomic<std::uint64_t> queue_unfiltered{};
std::atomic<std::uint64_t> reprojection_held{}; // 0.30.2: owned_taa held mReprojection on
std::array<std::atomic<std::uint64_t>, 6> queue_reasons{};
SRWLOCK telemetry_mutex = SRWLOCK_INIT;
struct TelemetryLock {
    TelemetryLock() { AcquireSRWLockExclusive(&telemetry_mutex); }
    ~TelemetryLock() { ReleaseSRWLockExclusive(&telemetry_mutex); }
};
thread_local bool frame_owned{};
thread_local unsigned frame_thread{};
thread_local std::uintptr_t frame_view{};
thread_local std::uintptr_t queued_taa{};
LONG64 context() noexcept {
    return InterlockedCompareExchange64(&mhw_aa_context, 0, 0);
}
void fail(unsigned n) noexcept {
    unsigned zero{};
    fault.compare_exchange_strong(zero, n);
    requested = false;
}
// Whether new camera frames are taken over (requested and not yielded).
bool owning() noexcept {
    return requested.load() && !yielded.load();
}
bool primary(std::uintptr_t ctx, std::uintptr_t view) noexcept {
    std::uintptr_t r{}, primary_view{}, backend{}, wrapper{};
    unsigned flags{}, sizing{};
    unsigned char api{};
    return ctx == renderer + layout::renderer::context && pb::read(image + layout::image::graphics_api, api) &&
           api == 1 && pb::read(image + layout::image::renderer, r) && r == renderer &&
           pb::read(renderer + layout::renderer::primary_view, primary_view) && primary_view && view == primary_view &&
           pb::read(view + layout::view::flags, flags) && !(flags & 0x840000) &&
           pb::read(renderer + layout::renderer::sizing_flags, sizing) && !(sizing & 1) &&
           pb::read(renderer + layout::renderer::backend, backend) && backend &&
           pb::read(backend + layout::d3d_render::game_dlss, wrapper) && !wrapper;
}
bool begin_frame(const pb::Frame &f) noexcept {
    // R8 is the incoming view. The native body binds context+1d0 later;
    // that field may still be null or contain a previous view at entry.
    if (!owning() || f.gpr[4] != 0 || !primary(f.gpr[2], f.gpr[3])) return false;
    std::uintptr_t camera{}, type{}, table{};
    unsigned char taa{}, fxaa{};
    if (!pb::read(image + layout::image::camera, camera) || camera != f.gpr[1] || !camera || !pb::read(camera, type) ||
        (type != image + layout::image::camera_vtable && type != image + layout::image::scene_camera_vtable) ||
        !pb::read(image + layout::image::scene, table) || !table || !pb::read(table + layout::scene::taa, taa) ||
        taa > 1 || !pb::read(table + layout::scene::fxaa, fxaa) || fxaa > 1)
        return false;
    if (frame_owned || InterlockedCompareExchange64(&mhw_aa_context, static_cast<LONG64>(f.gpr[2]), 0) != 0) {
        fail(1);
        return false;
    }
    frame_owned = true;
    frame_thread = GetCurrentThreadId();
    frame_view = f.gpr[3];
    {
        TelemetryLock lock;
        ++owned_frames;
        ++choices[taa + 2U * fxaa];
    }
    return true;
}
void end_frame(bool owned, bool returned) noexcept {
    if (!owned) return;
    if (!returned) fail(2);
    InterlockedExchange64(&mhw_aa_thread, 0);
    if (!frame_owned || frame_thread != GetCurrentThreadId() ||
        InterlockedCompareExchange64(&mhw_aa_context, 0, static_cast<LONG64>(renderer + layout::renderer::context)) !=
            static_cast<LONG64>(renderer + layout::renderer::context))
        fail(3);
    frame_owned = false;
    frame_thread = 0;
    frame_view = 0;
}
bool owned_context(std::uintptr_t ctx) noexcept {
    std::uintptr_t view{};
    return frame_owned && frame_thread == GetCurrentThreadId() && ctx == static_cast<std::uintptr_t>(context()) &&
           pb::read(ctx + layout::context::view, view) && view == frame_view && primary(ctx, view);
}
bool native_gates(std::uintptr_t unit, std::uintptr_t ctx, std::uintptr_t registry) noexcept {
    unsigned state{}, flags{}, mask{}, displays{};
    std::uint16_t ctx_mask{}, display{};
    unsigned category{}, excluded{};
    if (!pb::read(unit + layout::unit::state, state) || !pb::read(unit + layout::unit::flags, flags) ||
        !pb::read(unit + layout::unit::mask, mask) || !pb::read(unit + layout::unit::displays, displays) ||
        !pb::read(ctx + layout::context::unit_mask, ctx_mask) || !pb::read(ctx + layout::context::display, display) ||
        !pb::read(unit + layout::unit::category, category) ||
        !pb::read(registry + layout::unit_manager::excluded, excluded))
        return false;
    return state == 8 && (flags & 0x10) && (mask & ctx_mask) && (displays & (1U << (display & 15))) &&
           !(category & ~excluded);
}
std::uintptr_t reject_queue(unsigned reason) noexcept {
    TelemetryLock lock;
    ++queue_rejected;
    ++queue_reasons[reason];
    return 0;
}
std::uintptr_t prepare_queue(const pb::Frame &f) {
    // Camera calls this after native job join/merge and before the queue sorts
    // and assigns ping-pong resources. No worker context is adopted or changed.
    if (!owned_context(f.gpr[2]) || f.gpr[3] != frame_view) return 0;
    ++queue_visits;
    const bool bypass = failure_latched.load();
    std::uintptr_t queue{}, registry{}, type{}, node{}, taa{};
    unsigned count{};
    unsigned char collecting{}, enabled_queue{};
    if (!pb::read(image + layout::image::post_queue, queue) || queue != f.gpr[1] || !queue ||
        !pb::read(queue + layout::post_queue::enabled, enabled_queue) || enabled_queue != 1 ||
        !pb::read(queue + layout::post_queue::collecting, collecting) || collecting != 1 ||
        !pb::read(queue + layout::post_queue::count, count) || count > 16)
        return reject_queue(0);
    if (!bypass) {
        if (!pb::read(image + layout::image::unit_manager, registry) || !registry || !pb::read(registry, type) ||
            type != image + layout::image::unit_manager_vtable ||
            !pb::read(registry + layout::unit_manager::units, node))
            return reject_queue(1);
        unsigned nodes = 0;
        while (node && nodes++ < 1024) {
            if (!pb::read(node, type)) return reject_queue(1);
            if (type == image + layout::image::taa2_vtable) {
                if (taa) return reject_queue(1);
                taa = node;
            }
            if (!pb::read(node + layout::unit::next, node)) return reject_queue(1);
        }
        if (node || !taa) return reject_queue(1);
        if (!native_gates(taa, f.gpr[2], registry)) return reject_queue(2);
    }
    std::uintptr_t owner{};
    unsigned char skip{}, passthrough{};
    if (!bypass) {
        if (!pb::read(taa + layout::taa_filter::owner, owner) || owner != taa ||
            !pb::read(taa + layout::taa_filter::skip, skip) || skip ||
            !pb::read(taa + layout::taa_filter::passthrough, passthrough) || passthrough)
            return reject_queue(3);
    }
    // The native queue has sixteen pointer slots, followed by its two scratch
    // targets at +c0. Validate every entry before touching this transient queue.
    std::array<std::uintptr_t, 16> keep{};
    unsigned kept = 0, removed = 0, found = 0;
    for (unsigned i = 0; i < count; ++i) {
        std::uintptr_t entry{}, unit{}, vt{};
        if (!pb::read(queue + layout::post_queue::entries + 8 * i, entry) || !entry ||
            !pb::read(entry + layout::post_entry::skip, skip))
            return reject_queue(3);
        if (!skip) {
            if (!pb::read(entry + layout::post_entry::unit, unit)) return reject_queue(3);
            if (unit && !pb::read(unit, vt)) return reject_queue(3);
            if (vt == image + layout::image::fxaa_vtable || vt == image + layout::image::taa_vtable ||
                vt == image + layout::image::taa3_vtable || (bypass && vt == image + layout::image::taa2_vtable)) {
                ++removed;
                continue;
            }
            if (vt == image + layout::image::taa2_vtable) {
                unsigned stage{}, order{};
                if (unit != taa || entry != taa + layout::taa_filter::queue_entry || ++found != 1 ||
                    !pb::read(entry, stage) || stage != 15 || !pb::read(entry + layout::post_entry::order, order) ||
                    order != 2)
                    return reject_queue(3);
            }
        }
        keep[kept++] = entry;
    }
    if (!bypass && !found && kept == keep.size()) return reject_queue(4);
    if (queued_taa) return reject_queue(5);
    // Equivalent to native registration/removal before sorting; no preference,
    // enable flag, cached job list, worker data or persistent resource is edited.
    for (unsigned i = 0; i < kept; ++i)
        *reinterpret_cast<std::uintptr_t *>(queue + layout::post_queue::entries + 8 * i) = keep[i];
    *reinterpret_cast<unsigned *>(queue + layout::post_queue::count) = kept;
    if (bypass) {
        TelemetryLock lock;
        competing += removed;
        ++queue_unfiltered;
        return 0;
    }
    if (!found) {
        using Register = void (*)(std::uintptr_t, unsigned, unsigned);
        reinterpret_cast<Register>(image + 0x24c9240)(taa, 15, 2);
    }
    unsigned after{};
    std::uintptr_t last{};
    if (!pb::read(queue + layout::post_queue::count, after) || after != kept + (found ? 0U : 1U) ||
        (!found && (!pb::read(queue + layout::post_queue::entries + 8 * kept, last) ||
                    last != taa + layout::taa_filter::queue_entry))) {
        fail(6);
        return reject_queue(3);
    }
    {
        TelemetryLock lock;
        competing += removed;
        if (found)
            ++queue_existing;
        else
            ++queue_inserted;
    }
    return taa;
}
bool suppress_jitter(unsigned kind, const pb::Frame &f) noexcept {
    // Update runs before camera rendering. These exact native callbacks only
    // assign AA jitter; their allocation/reset callbacks remain untouched.
    if (!owning() && !context()) return false;
    constexpr std::array<std::uintptr_t, 3> types{0x351bda8, 0x354dd78, 0x354df48};
    std::uintptr_t type{}, r{};
    if (kind < 2 || kind > 4 || !pb::read(f.gpr[1], type) || type != image + types[kind - 2] ||
        !pb::read(image + layout::image::renderer, r) || r != renderer)
        return false;
    ++jitter_suppressed;
    return true;
}
void forward_taa(pb::Frame *f) {
    // Comparison overrides belong only to an admitted TAA2 Execute, not every
    // native unit/view inside the outer camera call. Keep them through return,
    // including an explicit diagnostic stop, then clear even on an exception.
    const auto thread = reinterpret_cast<LONG64>(NtCurrentTeb());
    const auto previous = InterlockedCompareExchange64(&mhw_aa_thread, thread, 0);
    if (previous) {
        fail(5);
        InterlockedExchange64(&mhw_aa_thread, 0);
        __try {
            mhw_binding_forward(originals[1], f);
        } __finally {
            InterlockedExchange64(&mhw_aa_thread, previous);
        }
        return;
    }
    bool returned = false;
    __try {
        mhw_binding_forward(originals[1], f);
        returned = true;
    } __finally {
        InterlockedExchange64(&mhw_aa_thread, 0);
        if (!returned) fail(5);
    }
}
// A scene's data can turn TAA2's mReprojection off (the Guild Card, stage 305;
// marie-2, 0.30.2): its execute then picks the _NO_REPROJECTION technique,
// which reads no depth, motion or camera and so can't be a DLSS frame. While
// DLSS owns TAA the property is held on for the execute, as the game's own
// DLSS holds anti-aliasing and resolution scaling, and given back after it.
bool write_byte(std::uintptr_t at, unsigned char value) noexcept {
    __try {
        *reinterpret_cast<volatile unsigned char *>(at) = value;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
void owned_taa(pb::Frame *f) {
    const auto at = f->gpr[1] + layout::taa_filter::reprojection;
    unsigned char reprojection = 1;
    const bool held = pb::read(at, reprojection) && reprojection == 0 && write_byte(at, 1);
    if (held) ++reprojection_held;
    __try {
        forward_taa(f);
    } __finally {
        if (held) write_byte(at, 0);
    }
}
void record_admission() {
    TelemetryLock lock;
    ++units;
    ++admitted;
}
void dispatch_inner(unsigned kind, pb::Frame *f) {
    if (kind == 0) {
        const bool owned = begin_frame(*f);
        bool returned = false;
        __try {
            mhw_binding_forward(originals[0], f);
            returned = true;
        } __finally {
            end_frame(owned, returned);
        }
    } else if (kind == 1) {
        const auto taa_context = f->gpr[2];
        if (queued_taa && f->gpr[1] == queued_taa && owned_context(f->gpr[2])) {
            record_admission();
            owned_taa(f);
        } else {
            ++execute_unowned;
            mhw_binding_forward(originals[1], f);
        }
        // full_post: every later post callback inherits the full rectangle.
        scaled_temporal::stream_route::full_post_taa(taa_context);
    } else if (kind == 7) {
        const auto previous = queued_taa;
        const auto previous_thread = InterlockedExchange64(&mhw_aa_thread, 0);
        // Forward saves the native return registers over Frame. Preserve the
        // incoming identity before it, and notify only the admitted outer queue.
        const auto manager = f->gpr[1], ctx = f->gpr[2], view = f->gpr[3];
        // Nested queues cannot inherit the outer queue's comparison authority.
        queued_taa = 0;
        bool returned = false;
        bool full_entered = false;
        __try {
            if (previous) {
                ++queue_visits;
                queued_taa = reject_queue(5);
            } else
                queued_taa = prepare_queue(*f);
            // Admitted as for post_queue_complete below: only such a queue is planned.
            if (!previous) {
                scaled_temporal::stream_route::full_post_enter(ctx, queued_taa && owning() && !fault.load() &&
                                                                        owned_context(ctx) && view == frame_view);
                full_entered = true;
            }
            mhw_binding_forward(originals[7], f);
            returned = true;
            // Before the queue is planned: restore the render rectangle (also on
            // an exception, below).
            if (full_entered) {
                full_entered = false;
                scaled_temporal::stream_route::full_post_exit(ctx);
            }
            if (!previous && queued_taa && owning() && !fault.load() && owned_context(ctx) && view == frame_view)
                scaled_temporal::stream_route::post_queue_complete(manager, ctx, view, queued_taa);
        } __finally {
            if (full_entered) scaled_temporal::stream_route::full_post_exit(ctx);
            queued_taa = previous;
            InterlockedExchange64(&mhw_aa_thread, previous_thread);
            if (!returned) fail(7);
        }
    } else if (!suppress_jitter(kind, *f))
        mhw_binding_forward(originals[kind], f);
}
} // namespace
void dispatch(unsigned kind, pb::Frame *f) {
    ++pending;
    __try {
        dispatch_inner(kind, f);
    } __finally {
        --pending;
    }
}
bool prepare(std::uintptr_t host, std::uintptr_t r, std::string &reason) {
    if (image) {
        reason = "AA activation already prepared";
        return false;
    }
    struct Gate {
        unsigned rva, bytes;
        const char *sha;
    };
    constexpr Gate gates[]{{0x24c9240, 43, "3907659e9c8215d943edde455a4b54fe655c5a52f6b1bfd9571ec43d71c7d58f"},
                           {0x24f1fc0, 835, "1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190"},
                           {0x24f2320, 176, "2965519c5b4feea0ed564a4bfce9c87a49def3e96433a7ce7e796791d270162e"},
                           {0x228e770, 528, "33ae7df95058ac2e1ce8d920ff89c62d664068a975931bb10afb95ea88f00b97"},
                           {0x2392150, 4221, "d2c6c0f6a5ed39c998bcf62afed19132e6dd0d7cfbb19e18b80c15531af2bf91"},
                           {0x23931e0, 124, "88aa2c037b51855a2e516f683ec68c1a00565edc5f78741e8db71783e0b7e488"},
                           {0x2596090, 108, "cfbffc3008baba8b595a7ce926d63f16ce609ceb6379c538cf3231df958d3752"},
                           {0x25979b0, 124, "d390c4d7965f6b3aa683e7324b570eb574f63fe5a0d419daa469aff73de7399a"}};
    for (const auto &g : gates)
        if (!pb::verify_code(host, g.rva, g.bytes, g.sha)) {
            reason = "AA activation code gate";
            return false;
        }
    image = host;
    renderer = r;
    const std::array<void *, hooks.size()> detours{
        reinterpret_cast<void *>(mhw_aa_frame_entry),   reinterpret_cast<void *>(mhw_aa_taa_entry),
        reinterpret_cast<void *>(mhw_aa_jitter2_entry), reinterpret_cast<void *>(mhw_aa_jitter1_entry),
        reinterpret_cast<void *>(mhw_aa_jitter3_entry), reinterpret_cast<void *>(mhw_aa_table_entry),
        reinterpret_cast<void *>(mhw_aa_sharpen_entry), reinterpret_cast<void *>(mhw_aa_queue_entry)};
    // Each hook is live as it goes in: the leaf stubs continue with the very
    // pointers they jump through, and nothing is owned until `requested` below.
    mhw_aa_table_resume = reinterpret_cast<void *>(host + 0x2392184);
    mhw_aa_sharpen_resume = reinterpret_cast<void *>(host + 0x2392f95);
    for (unsigned i = 0; i < hooks.size(); ++i) {
        auto &h = hooks[i];
        h.name = hook_specs[i].name;
        h.pattern = hook_specs[i].pattern;
        h.target = reinterpret_cast<void *>(host + hook_specs[i].rva);
        void **next = i == 5 ? &mhw_aa_table_original : i == 6 ? &mhw_aa_sharpen_original : &originals[i];
        if (!astera::hook_owned(h, detours[i], next)) {
            reason = "AA activation hook creation";
            return false;
        }
    }
    requested = true;
    return true;
}
void stop() noexcept {
    requested = false;
}
void yield(bool y) noexcept {
    yielded = y;
}
void latch_failure() noexcept {
    failure_latched = true;
}
bool drained() noexcept {
    return !pending.load() && !context();
}
bool healthy() noexcept {
    return !fault.load();
}
bool unhook() noexcept {
    // After stop, new callbacks only forward. They can re-enter between the
    // parent's drain check and this call, especially during scale restoration.
    // The entry points can be detached while the pinned DLL and the
    // continuations remain alive. An owned camera scope must still finish first.
    if (requested.load() || context()) return false;
    bool ok = true;
    for (auto &h : hooks) ok = astera::unhook_owned(h) && ok;
    return ok;
}
void write(std::ostream &out) {
    TelemetryLock lock;
    bool hooks_off = true;
    for (const auto &h : hooks) hooks_off = hooks_off && !h.live;
    out << "{\"schema\":2,\"policy\":\"native-postqueue-preparation-v2\",\"settings_writes\":0,\"layer_flag_writes\":0,\"requested\":"
        << requested.load() << ",\"yielded\":" << yielded.load() << ",\"fault\":" << fault.load()
        << ",\"pending\":" << pending.load() << ",\"active_context\":" << context()
        << ",\"frames\":" << owned_frames.load() << ",\"taa_visits\":" << units.load()
        << ",\"taa_admitted\":" << admitted.load() << ",\"taa_rejected\":" << rejected.load()
        << ",\"competing_suppressed\":" << competing.load() << ",\"jitter_suppressed\":" << jitter_suppressed.load()
        << ",\"table_forced\":" << InterlockedCompareExchange64(&mhw_aa_table_forced, 0, 0)
        << ",\"sharpen_suppressed\":" << InterlockedCompareExchange64(&mhw_aa_sharpen_suppressed, 0, 0)
        << ",\"queue_visits\":" << queue_visits.load() << ",\"queue_inserted\":" << queue_inserted.load()
        << ",\"queue_existing\":" << queue_existing.load() << ",\"queue_rejected\":" << queue_rejected.load()
        << ",\"execute_unowned\":" << execute_unowned.load() << ",\"reprojection_held\":" << reprojection_held.load()
        << ",\"queue_reasons\":[";
    for (unsigned i = 0; i < queue_reasons.size(); ++i) out << (i ? "," : "") << queue_reasons[i].load();
    out << "]"
        << ",\"failure_latched\":" << failure_latched.load() << ",\"queue_unfiltered\":" << queue_unfiltered.load()
        << ",\"hooks_disabled\":" << hooks_off << ",\"native_choices\":[";
    for (unsigned i = 0; i < choices.size(); ++i) out << (i ? "," : "") << choices[i].load();
    out << "]}";
}
void write_choices(std::ostream &out) {
    TelemetryLock lock;
    out << '[';
    for (unsigned i = 0; i < choices.size(); ++i) out << (i ? "," : "") << choices[i].load();
    out << ']';
}
} // namespace mhw::continuous_dlaa::activation
extern "C" void mhw_aa_dispatch(unsigned kind, mhw::projection_binding::Frame *f) {
    mhw::continuous_dlaa::activation::dispatch(kind, f);
}
