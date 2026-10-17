#include "game_session.hpp"
#include "camera_constants.hpp"
#include "input_ring.hpp"
#include "staging_queue.hpp"
#include "hudless_pass.hpp"
#include "../temporal_present/bindings.hpp"
#include "../temporal_present/observer.hpp"
#include "../reflex/frame_generation_policy.hpp"
#include "../reflex/streamline.hpp"
#include "../perf/toggles.hpp"
#include "../gpu_memory/budget.hpp"
#include <dxgi1_3.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <mutex>
#include <ostream>

namespace mhw::frame_generation::game {
namespace {
using Microsoft::WRL::ComPtr;
constexpr unsigned slot_count = 3, none = InputRing<slot_count>::none;
constexpr auto tag_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;  // DLSS converter output state
const sl::ViewportHandle viewport{0};

enum class Off : unsigned { not_armed, no_token, no_ready, camera, constants, tags, options, disabled, unloaded, count };
constexpr std::array<const char *, static_cast<unsigned>(Off::count)> off_names{
    "not_armed", "no_token", "no_ready", "camera", "constants", "tags", "options", "disabled", "unloaded"};
// The player's Off frees DLSS-G's resources this many Presents after it, when
// its last On frames are done on the GPU (the paced chain queues one frame).
constexpr unsigned free_after_presents = 4;
// A planned Off from more than plain 2x (multi frame, Dynamic, Auto) first
// shows plain 2x for at least this many tagged Presents and this long, at
// most the longest (v0.28.3, reconcile()).
constexpr unsigned wind_down_presents = 24;
constexpr std::uint64_t wind_down_min_ms = 150, wind_down_max_ms = 400;


struct Images {
    ComPtr<ID3D12Resource> depth, motion, hudless;
};
// Graphics root parameter of the screen write's PS CBSystem (b3): parameter 5
// of its root signature (RenderDoc, EID 36144 of the HUD capture). The screen
// write is the last draw of its list, so the binding is still current when the
// list closes; the capture pass validates the contents on the GPU.
constexpr unsigned system_parameter = 5;
// Lists closed between a HUD-less record and the next Present that may record
// a capture (the screen write's list is one of them).
constexpr unsigned capture_limit = 4;
// format: the back buffer is neither SDR 8-bit nor HDR10 (v0.24.8).
enum class HudSkip : unsigned { disabled, no_restore, failed, format, count };
constexpr std::array<const char *, static_cast<unsigned>(HudSkip::count)> hud_skip_names{"disabled", "no_restore", "failed", "format"};
struct Staged {
    bool valid{};
    std::uint64_t frame{}, epoch{};
    ComPtr<ID3D12Resource> depth, motion;
    SlotCamera camera;
};
// perf::keyed_staging: staging copies of the DLSS inputs (staging_queue.hpp).
constexpr unsigned staging_count = 3;
struct StagingCopy {
    ComPtr<ID3D12Resource> depth, motion;
};
struct Last {
    bool valid{};
    std::uint64_t frame{}, epoch{};
    SlotCamera camera;
};
struct Sample {
    std::uint64_t present_frame{}, slot_frame{};
    CameraConstants constants;
};

struct Session {
    std::mutex mutex;
    reflex::Streamline *sdk{};
    unsigned output_width{}, output_height{};
    SceneRect scene;  // In the back buffer; the HUD-less image is scene-sized
    // Viewport (x, y, w, h) and scissor (l, t, r, b) of each capture candidate
    // in the latest frame that had captures; the screen write is one of them.
    std::array<std::array<int, 8>, capture_limit> candidates{};
    unsigned candidate_count{};
    std::atomic<bool> armed{}, on{};
    // The player's setting (enable()); Presents since it went Off, whether
    // DLSS-G's resources were freed for it, and how often that was done.
    std::atomic<bool> enabled{true};
    // What the worker (arm()) and the player (enable()) ask for; armed and
    // enabled follow at the next Present, after a wind-down (reconcile()).
    std::atomic<bool> armed_request{}, enabled_request{true};
    bool winding{};
    unsigned wind_presents{};
    std::uint64_t wind_began{}, winds{}, wound_presents{};
    unsigned disabled_presents{};
    bool freed{};
    std::uint64_t frees{}, free_failed{};
    InputRing<slot_count> ring;
    std::array<Images, slot_count> images;
    std::array<bool, slot_count> slot_hudless{};
    Staged staged;
    StagingQueue<staging_count, SlotCamera> queue;  // perf::keyed_staging
    std::array<StagingCopy, staging_count> staging{};
    bool staging_failed{};
    // final frame - newest staged frame at a mismatch: -2, -1, +1, +2, other.
    std::array<std::uint64_t, 5> mismatch_delta{};
    std::uint64_t keyed_hits{}, staging_copies{}, keyed_early{}, keyed_confirms{};
    std::unique_ptr<HudlessPass> hudless;
    bool hudless_failed{};
    std::array<char, 160> hudless_error{};  // First HUD-less failure, JSON-safe
    std::atomic<bool> capture_window{};  // A HUD-less pass was recorded since the last Present
    unsigned captures_this_frame{}, hdr_lists_this_frame{};
    std::uint64_t captures{}, capture_no_cbv{}, capture_limited{}, capture_failed{};
    // v0.24.8: the back buffer's DXGI_FORMAT (back_buffer()); HDR10 captures
    // (one per CBV root argument of a closing list), HDR probes with a valid
    // capture or the shader's defaults, and slot images made for a new format.
    std::atomic<unsigned> back_buffer_format{};
    std::uint64_t hdr_captures{}, hdr_probe_ok{}, hdr_probe_default{}, hudless_recreated{};
    std::uint64_t staged_count{}, staged_dropped{}, final_unstaged{}, final_mismatch{}, hudless_recorded{}, hudless_tagged{},
        probe_ok{}, probe_fallback{}, probe_invalid{};
    std::array<std::uint64_t, static_cast<unsigned>(HudSkip::count)> hud_skips{};
    // Why a HUD-less record found no restorable binding snapshot (no_restore):
    // not known, invalid, no generation, no pipeline, no graphics signature,
    // and graphics-only snapshots restored by unbinding compute (v0.13.7).
    std::array<std::uint64_t, 6> restore_reasons{};
    HudlessPass::Probe last_probe{};
    std::array<SlotCamera, slot_count> cameras;
    D3D12_RESOURCE_DESC depth_desc{}, motion_desc{};
    bool created{}, create_failed{};
    gpu_memory::Check memory;  // create()'s video memory check
    unsigned pending{none};
    Last last;
    ID3D12Fence *fence{};  // Owned by the SDK; only GetCompletedValue is called.
    sl::DLSSGState state{};
    bool state_valid{};
    std::uint64_t presents{}, tagged{}, recorded{}, record_failed{}, size_mismatch{}, state_failed{}, mode_changes{},
        null_tags{}, unpresented{}, exceptions{}, lifecycle_calls{}, lifecycle_offs{};
    std::atomic<unsigned> requested_multiplier{2};
    std::atomic<unsigned> requested_mode{static_cast<unsigned>(Mode::fixed)}, requested_target{};
    unsigned applied_generated{};  // 0 while Off
    sl::DLSSGMode applied_mode{sl::DLSSGMode::eOff};
    unsigned applied_target{};
    std::atomic<bool> dynamic_refused{};  // DLSS-G refused eDynamic: the fixed multiplier from then on
    // Mirrors for status() (any thread): applied_generated, the state's
    // numFramesToGenerateMax, and generated frames presented so far.
    std::atomic<unsigned> shown_generated{}, shown_maximum{};
    std::atomic<std::uint64_t> presented_generated{};
    // ... the applied mode, the state's dynamic support (0 unknown, 1 no, 2 yes),
    // frames presented for the last rendered frame, and the run of Presents
    // while On that showed none generated (an Auto pause, a reset, a menu).
    std::atomic<bool> shown_dynamic{};
    std::atomic<unsigned> shown_dynamic_support{}, shown_presented{}, single_streak{};
    std::uint64_t generated_changes{}, generated_limited{}, mode_option_changes{}, dynamic_fallbacks{};
    // Pacing around count changes (game-81: Dynamic near its 1x/2x boundary
    // showed ~21 ms host frames). Between consecutive tagged Presents: changes
    // of numFramesActuallyPresented (both at least 1), intervals over
    // long_present_us, and those within two Presents of a change.
    static constexpr std::uint64_t long_present_us = 16000;
    std::int64_t pacing_qpc{};
    std::uint64_t pacing_present{};  // `presents` at the last finish_pending
    unsigned pacing_count{};         // its numFramesActuallyPresented
    unsigned since_switch{~0U}, since_long{~0U};
    bool long_matched{};
    std::uint64_t count_switches{}, long_presents{}, long_near_switch{};
    std::uint64_t input_releases{};  // live preset changes (release_inputs())
    // Tagged frames with reset: all, and those whose DLSS history was reset
    // (the rest had no consecutive predecessor). DLSS-G presents no generated
    // frame for a reset.
    std::uint64_t reset_tags{}, history_reset_tags{};
    std::array<std::uint64_t, 8> presented_frames{};  // numFramesActuallyPresented per Present: 0..6, other
    std::array<std::uint64_t, static_cast<unsigned>(Off::count)> off{};
    std::array<std::uint64_t, 7> camera_failures{};
    std::array<std::uint64_t, 4> present_delta{};  // present frame - slot frame: 1, 2, 3, other
    std::array<Sample, 8> samples{};
    unsigned sample_count{};
    sl::Result last_sdk_error{sl::Result::eOk};
};
Session session;
// Inputs are prepared only while armed by the worker and on by the player.
bool active(const Session &s) noexcept { return s.armed.load() && s.enabled.load(); }

sl::float4x4 matrix(const std::array<float, 16> &m) {
    sl::float4x4 r;
    for (unsigned y = 0; y < 4; ++y) r[y] = {m[y * 4], m[y * 4 + 1], m[y * 4 + 2], m[y * 4 + 3]};
    return r;
}

// Keeps the first failure only; printable ASCII without quotes or backslashes.
void note_hudless_error(Session &s, const char *why) noexcept {
    if (s.hudless_error[0] || !why) return;
    for (std::size_t i = 0; i + 1 < s.hudless_error.size() && why[i]; ++i) {
        const char c = why[i];
        s.hudless_error[i] = c >= 0x20 && c < 0x7f && c != '"' && c != '\\' ? c : ' ';
    }
}

D3D12_RESOURCE_BARRIER transition(ID3D12Resource *r, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to) {
    D3D12_RESOURCE_BARRIER b{};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition = {r, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, from, to};
    return b;
}

// The HUD-less image covers exactly the scene rectangle.
D3D12_RESOURCE_DESC hudless_desc(const D3D12_RESOURCE_DESC &scene, DXGI_FORMAT format = HudlessPass::format) {
    D3D12_RESOURCE_DESC h{};
    h.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    h.Width = scene.Width;
    h.Height = scene.Height;
    h.DepthOrArraySize = h.MipLevels = 1;
    h.SampleDesc.Count = 1;
    h.Format = format;
    h.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    return h;
}
// The HUD-less image's format for the back buffer's (v0.24.8): DLSS-G takes
// HUD-less colour in the back buffer's colour space. UNKNOWN: none.
DXGI_FORMAT hudless_format(unsigned back_buffer, bool hdr_supported) noexcept {
    switch (static_cast<DXGI_FORMAT>(back_buffer)) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM: return HudlessPass::format;
    case DXGI_FORMAT_R10G10B10A2_UNORM: return hdr_supported ? HudlessPass::hdr_format : DXGI_FORMAT_UNKNOWN;
    default: return DXGI_FORMAT_UNKNOWN;
    }
}

bool create(Session &s, ID3D12Resource *depth, ID3D12Resource *motion, ID3D12Resource *scene) {
    ComPtr<ID3D12Device> device;
    if (FAILED(depth->GetDevice(IID_PPV_ARGS(&device)))) return false;
    s.depth_desc = depth->GetDesc();
    s.motion_desc = motion->GetDesc();
    const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
    const auto scene_desc = scene->GetDesc();
    const auto h = hudless_desc(scene_desc);
    // v0.24.0: the depth/motion ring, the keyed staging copies (made on first
    // use) and, when made here, the HUD-less images must fit the video memory
    // budget (gpu_memory/budget.hpp); otherwise frame generation stays off.
    {
        auto d = s.depth_desc, m = s.motion_desc;
        d.Flags = m.Flags = D3D12_RESOURCE_FLAG_NONE;
        const auto bytes = [&](const D3D12_RESOURCE_DESC &x) { return device->GetResourceAllocationInfo(0, 1, &x).SizeInBytes; };
        const std::uint64_t depth_bytes = bytes(d), motion_bytes = bytes(m), hudless_bytes = bytes(h);
        if (depth_bytes == UINT64_MAX || motion_bytes == UINT64_MAX || hudless_bytes == UINT64_MAX) return false;
        const unsigned copies = slot_count + (perf::on(perf::keyed_staging) ? staging_count : 0);
        s.memory = gpu_memory::check(copies * (depth_bytes + motion_bytes) +
                                     (!s.hudless && !s.hudless_failed ? slot_count * hudless_bytes : 0));
        if (!s.memory.passed) return false;
    }
    // HUD-less is optional: any failure here leaves depth/motion-only FG. It is
    // scene-sized, so a live preset change keeps it (release_inputs()).
    if (!s.hudless && !s.hudless_failed) try {
        if (scene_desc.Width != s.scene.width || scene_desc.Height != s.scene.height)
            throw std::runtime_error("hudless scene extent");
        s.hudless = std::make_unique<HudlessPass>(device.Get(), static_cast<unsigned>(scene_desc.Width), scene_desc.Height);
        // In the back buffer's format (SDR or HDR10, the same size); for any
        // other, none until it changes (record() makes a slot's image then).
        const auto wanted = hudless_format(s.back_buffer_format.load(), s.hudless->hdr_supported());
        const auto image = hudless_desc(scene_desc, wanted);
        for (auto &i : s.images) {
            if (wanted == DXGI_FORMAT_UNKNOWN) break;
            if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &image, tag_state, nullptr, IID_PPV_ARGS(&i.hudless))))
                throw std::runtime_error("hudless image");
            i.hudless->SetName(L"mhw-fg-hudless");
        }
    } catch (const std::exception &e) {
        note_hudless_error(s, e.what());
        s.hudless_failed = true;
    } catch (...) {
        note_hudless_error(s, "hudless creation");
        s.hudless_failed = true;
    }
    if (s.hudless_failed) {
        s.hudless.reset();
        for (auto &i : s.images) i.hudless.Reset();
    }
    for (auto &i : s.images) {
        auto d = s.depth_desc, m = s.motion_desc;
        d.Flags = m.Flags = D3D12_RESOURCE_FLAG_NONE;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, tag_state, nullptr,
                                                   IID_PPV_ARGS(&i.depth))) ||
            FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &m, tag_state, nullptr,
                                                   IID_PPV_ARGS(&i.motion))))
            return false;
        i.depth->SetName(L"mhw-fg-depth");
        i.motion->SetName(L"mhw-fg-motion");
    }
    return true;
}

bool same_image(const D3D12_RESOURCE_DESC &a, const D3D12_RESOURCE_DESC &b) {
    return a.Dimension == b.Dimension && a.Width == b.Width && a.Height == b.Height && a.Format == b.Format &&
           a.MipLevels == b.MipLevels && a.DepthOrArraySize == b.DepthOrArraySize && a.SampleDesc.Count == b.SampleDesc.Count;
}

// Owner thread, under the session lock. Null tags release SDK references.
void turn_off(Session &s, const sl::FrameToken *token, Off reason) {
    ++s.off[static_cast<unsigned>(reason)];
    if (!s.on.load()) return;
    if (token) {
        sl::Extent extent{0, 0, s.output_width, s.output_height};
        sl::ResourceTag tags[]{
            {nullptr, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &extent},
            {nullptr, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &extent},
            {nullptr, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &extent}};
        if (s.sdk->tags(*token, viewport, tags, 3) == sl::Result::eOk) ++s.null_tags;
    }
    const auto result = s.sdk->frame_generation_off(viewport, true);
    if (result != sl::Result::eOk) s.last_sdk_error = result;
    s.on.store(false);
    s.applied_generated = 0;
    s.applied_mode = sl::DLSSGMode::eOff;
    s.applied_target = 0;
    s.shown_generated.store(0);
    s.shown_dynamic.store(false);
    s.single_streak.store(0);
    ++s.mode_changes;
}

// Owner thread, under the session lock, after a presented tagged Present's
// state read: the pacing counters (Session). Untagged Presents in between
// break the sequence; counting starts again at the next pair.
void note_pacing(Session &s, unsigned count) {
    LARGE_INTEGER now{}, frequency{};
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&frequency);
    const bool consecutive =
        s.pacing_qpc && s.presents == s.pacing_present + 1 && now.QuadPart > s.pacing_qpc && frequency.QuadPart > 0;
    if (!consecutive) {
        s.since_switch = s.since_long = ~0U;
    } else {
        if (s.since_switch != ~0U) ++s.since_switch;
        if (s.since_long != ~0U) ++s.since_long;
        if (count >= 1 && s.pacing_count >= 1 && count != s.pacing_count) {
            ++s.count_switches;
            s.since_switch = 0;
            if (s.since_long <= 2 && !s.long_matched) {  // a long interval just before
                ++s.long_near_switch;
                s.long_matched = true;
            }
        }
        const auto ticks = static_cast<std::uint64_t>(now.QuadPart - s.pacing_qpc);
        if (ticks * 1000000 / static_cast<std::uint64_t>(frequency.QuadPart) > Session::long_present_us) {
            ++s.long_presents;
            s.since_long = 0;
            s.long_matched = s.since_switch <= 2;  // at or just after a change
            if (s.long_matched) ++s.long_near_switch;
        }
    }
    s.pacing_qpc = now.QuadPart;
    s.pacing_present = s.presents;
    s.pacing_count = count;
}

// The state's Dynamic MFG support for status(): 1 no, 2 yes.
void note_state(Session &s, const sl::DLSSGState &state) {
    s.state = state;
    s.state_valid = true;
    s.shown_maximum.store(state.numFramesToGenerateMax);
    s.shown_dynamic_support.store(state.bIsDynamicMFGSupported == sl::Boolean::eTrue ? 2U : 1U);
}

// Owner thread, under the session lock: generated frames for the requested
// multiplier. The capability comes from the last state read on this thread.
void read_state_once(Session &s) {
    if (s.state_valid) return;
    sl::DLSSGState state{};
    if (s.sdk->frame_generation_state(viewport, state) == sl::Result::eOk) note_state(s, state);
}
unsigned wanted_generated(Session &s) {
    read_state_once(s);
    const auto multiplier = s.requested_multiplier.load();
    const auto maximum = s.state_valid ? s.state.numFramesToGenerateMax : 0;
    auto generated = reflex::generated_frames_for(multiplier, maximum);
    if (!generated) generated = 1;
    if (generated + 1 != multiplier) ++s.generated_limited;
    return generated;
}

// Owner thread, under the session lock: the options for the requested mode.
// eDynamic only with the state's support and while DLSS-G has not refused it;
// its numFramesToGenerate is ignored by DLSS-G and set to the maximum allowed.
struct Wanted {
    sl::DLSSGMode mode{};
    unsigned generated{}, target{};
    bool fallback{};  // dynamic asked for, the fixed multiplier used
};
Wanted wanted_options(Session &s) {
    read_state_once(s);
    // Winding down for a planned Off: plain 2x (reconcile()).
    if (s.winding) return {sl::DLSSGMode::eOn, 1, 0, false};
    const auto mode = static_cast<Mode>(s.requested_mode.load());
    if (mode == Mode::dynamic && !s.dynamic_refused.load() && s.state_valid &&
        s.state.bIsDynamicMFGSupported == sl::Boolean::eTrue && s.state.numFramesToGenerateMax)
        return {sl::DLSSGMode::eDynamic, s.state.numFramesToGenerateMax, s.requested_target.load(), false};
    return {mode == Mode::automatic ? sl::DLSSGMode::eAuto : sl::DLSSGMode::eOn, wanted_generated(s), 0, mode == Mode::dynamic};
}

}  // namespace

void configure(reflex::Streamline &sdk, unsigned output_width, unsigned output_height, SceneRect scene) noexcept {
    std::lock_guard lock(session.mutex);
    session.sdk = &sdk;
    session.output_width = output_width;
    session.output_height = output_height;
    session.scene = scene;
}

// On applies at once; Off at the next Present (reconcile()), at once while
// DLSS-G is not on.
void arm(bool armed) noexcept {
    session.armed_request.store(armed);
    if (armed || !session.on.load()) session.armed.store(armed);
}
void enable(bool enabled) noexcept {
    session.enabled_request.store(enabled);
    if (enabled || !session.on.load()) session.enabled.store(enabled);
}

void back_buffer(unsigned format) noexcept { session.back_buffer_format.store(format); }
bool frame_generation_on() noexcept { return session.on.load(); }
bool inputs_settled() noexcept {
    auto &s = session;
    try {
        std::lock_guard lock(s.mutex);
        if (s.armed.load() || s.on.load() || s.pending != none) return false;
        if (s.fence) {
            const auto completed = s.fence->GetCompletedValue();
            if (completed == UINT64_MAX) return false;
            s.ring.reclaim(completed);
        }
        return s.ring.settled();
    } catch (...) {
        return false;
    }
}
bool release_inputs(bool scene) noexcept {
    auto &s = session;
    try {
        std::lock_guard lock(s.mutex);
        if (s.armed.load() || s.on.load() || s.pending != none || !s.ring.settled()) return false;
        s.ring.release_all();
        for (auto &i : s.images) i.depth.Reset(), i.motion.Reset();
        for (auto &c : s.staging) c = {};
        s.staging_failed = false;
        s.queue.clear();
        s.staged = {};
        s.slot_hudless = {};
        s.depth_desc = s.motion_desc = {};
        if (scene) {
            // A new scene size (configure() follows): the HUD-less pass and
            // images go too, and are created again at the new size.
            s.hudless.reset();
            for (auto &i : s.images) i.hudless.Reset();
            s.hudless_failed = false;
            s.capture_window.store(false);
        }
        // The next record() creates the depth/motion images at the new size
        // (without `scene`, the scene-sized HUD-less pass and images stay).
        s.created = s.create_failed = false;
        ++s.input_releases;
        return true;
    } catch (...) {
        return false;
    }
}
void multiplier(unsigned total) noexcept {
    if (reflex::generated_frames_for(total, 0)) session.requested_multiplier.store(total);
}
void mode(Mode mode, unsigned target_fps) noexcept {
    if (mode != Mode::fixed && mode != Mode::automatic && mode != Mode::dynamic) return;
    if (target_fps > 1000) return;
    // The target first: a Present that sees the new mode sees its target too.
    session.requested_target.store(target_fps);
    session.requested_mode.store(static_cast<unsigned>(mode));
}
Status status() noexcept {
    const auto &s = session;
    return {s.armed.load(), s.on.load(), s.requested_multiplier.load(), s.shown_generated.load(), s.shown_maximum.load(),
            s.presented_generated.load(), static_cast<Mode>(s.requested_mode.load()), s.requested_target.load(),
            s.shown_dynamic.load(), s.shown_dynamic_support.load(), s.dynamic_refused.load(), s.shown_presented.load(),
            s.single_streak.load(), s.enabled.load()};
}

void note_mismatch(Session &s, std::uint64_t staged, std::uint64_t frame) {
    const auto d = static_cast<std::int64_t>(frame - staged);
    ++s.mismatch_delta[d == -2 ? 0 : d == -1 ? 1 : d == 1 ? 2 : d == 2 ? 3 : 4];
}
bool staging_ready(Session &s, ID3D12Resource *depth, ID3D12Resource *motion) {
    if (s.staging_failed) return false;
    if (s.staging[0].depth) return same_image(depth->GetDesc(), s.staging[0].depth->GetDesc()) &&
                                   same_image(motion->GetDesc(), s.staging[0].motion->GetDesc());
    ComPtr<ID3D12Device> device;
    if (FAILED(depth->GetDevice(IID_PPV_ARGS(&device)))) return !(s.staging_failed = true);
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    auto d = depth->GetDesc(), m = motion->GetDesc();
    d.Flags = m.Flags = D3D12_RESOURCE_FLAG_NONE;
    for (auto &c : s.staging)
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
                                                   IID_PPV_ARGS(&c.depth))) ||
            FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &m, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
                                                   IID_PPV_ARGS(&c.motion)))) {
            s.staging = {};
            return !(s.staging_failed = true);
        }
    return true;
}

void stage(ID3D12GraphicsCommandList *list, std::uint64_t frame, std::uint64_t epoch, ID3D12Resource *depth,
           ID3D12Resource *motion, const SlotCamera &camera) noexcept {
    auto &s = session;
    if (!active(s)) return;
    try {
        std::lock_guard lock(s.mutex);
        if (!depth || !motion) {
            ++s.record_failed;
            return;
        }
        if (!perf::on(perf::keyed_staging)) {
            s.queue.clear();
            if (s.staged.valid) ++s.staged_dropped;  // previous frame's final draw never qualified
            s.staged = {true, frame, epoch, depth, motion, camera};
            ++s.staged_count;
            return;
        }
        s.staged = {};
        if (!list || !staging_ready(s, depth, motion)) {
            ++s.record_failed;
            return;
        }
        // A final draw recorded before this stage holds an unconfirmed slot for
        // the frame (its copy runs after this one on the GPU): confirm it.
        // Otherwise queue the frame for its final draw.
        auto &copy = s.staging[s.queue.push(frame, epoch, camera)];
        std::array<D3D12_RESOURCE_BARRIER, 4> b{transition(copy.depth.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
                                               transition(copy.motion.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COPY_DEST),
                                               transition(depth, tag_state, D3D12_RESOURCE_STATE_COPY_SOURCE),
                                               transition(motion, tag_state, D3D12_RESOURCE_STATE_COPY_SOURCE)};
        list->ResourceBarrier(4, b.data());
        list->CopyResource(copy.depth.Get(), depth);
        list->CopyResource(copy.motion.Get(), motion);
        for (auto &v : b) std::swap(v.Transition.StateBefore, v.Transition.StateAfter);
        list->ResourceBarrier(4, b.data());
        ++s.staging_copies;
        ++s.staged_count;
        if (const auto slot = s.ring.confirm(frame, epoch); slot != none) {
            s.cameras[slot] = camera;
            s.queue.erase(frame, epoch);
            ++s.keyed_confirms;
        }
    } catch (...) {
        ++session.record_failed;
    }
}

void record(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots, ID3D12Resource *scene,
            std::uint64_t frame, std::uint64_t epoch, D3D12_RESOURCE_STATES scene_state) noexcept {
    auto &s = session;
    if (!active(s)) return;
    try {
        std::lock_guard lock(s.mutex);
        if (!s.sdk || s.create_failed) return;
        const bool keyed = perf::on(perf::keyed_staging);
        Staged staged;
        bool confirmed = true;
        if (keyed) {
            const auto newest = s.queue.newest();
            const auto taken = s.queue.take(frame, epoch);
            if (!s.staging[0].depth) {  // no frame staged yet
                ++s.final_unstaged;
                return;
            }
            // GPU order: this frame's DLSS list writes its staging copy before
            // this final draw (which samples the DLSS output) reads it.
            const auto &copy = s.staging[frame % staging_count];
            if (taken) {
                staged = {true, frame, epoch, copy.depth, copy.motion, taken->entry.payload};
                ++s.keyed_hits;
            } else {
                // Recorded before the frame's DLSS stage (heavy scenes): the slot
                // waits for stage() to confirm it and supply the camera.
                staged = {true, frame, epoch, copy.depth, copy.motion, {}};
                confirmed = false;
                ++s.keyed_early;
                if (newest) note_mismatch(s, *newest, frame);
            }
        } else {
            if (!s.staged.valid) {
                ++s.final_unstaged;
                return;
            }
            staged = std::move(s.staged);
            s.staged = {};
            if (staged.frame != frame || staged.epoch != epoch) {
                ++s.final_mismatch;
                note_mismatch(s, staged.frame, frame);
                return;
            }
        }
        // Staging copies stay in COPY_SOURCE; DLSS's own images are in tag_state.
        const auto source_state = keyed ? D3D12_RESOURCE_STATE_COPY_SOURCE : tag_state;
        const auto generation = roots.generation;
        auto *depth = staged.depth.Get(), *motion = staged.motion.Get();
        if (!list || !scene || !generation) {
            ++s.record_failed;
            return;
        }
        if (!s.created) {
            s.created = create(s, depth, motion, scene);
            if (!s.created) {
                s.create_failed = true;
                s.hudless.reset();
                for (auto &i : s.images) i = {};
                return;
            }
        }
        if (!same_image(depth->GetDesc(), s.depth_desc) || !same_image(motion->GetDesc(), s.motion_desc)) {
            ++s.size_mismatch;
            return;
        }
        const auto completed = s.fence ? s.fence->GetCompletedValue() : 0;
        const auto slot = s.ring.acquire(frame, epoch, completed, confirmed);
        if (slot == none) return;
        // The slot's previous pass is complete (fence-gated): collect its probe.
        if (s.slot_hudless[slot] && s.hudless) {
            const auto probe = s.hudless->probe(slot);
            if (!probe.valid)
                ++s.probe_invalid;
            else {
                if (probe.hdr) ++(probe.hdr_ok ? s.hdr_probe_ok : s.hdr_probe_default);
                else ++(probe.parameters_ok ? s.probe_ok : s.probe_fallback);
                s.last_probe = probe;
            }
        }
        s.slot_hudless[slot] = false;
        auto &image = s.images[slot];
        std::array<D3D12_RESOURCE_BARRIER, 4> b{transition(image.depth.Get(), tag_state, D3D12_RESOURCE_STATE_COPY_DEST),
                                               transition(image.motion.Get(), tag_state, D3D12_RESOURCE_STATE_COPY_DEST),
                                               transition(depth, tag_state, D3D12_RESOURCE_STATE_COPY_SOURCE),
                                               transition(motion, tag_state, D3D12_RESOURCE_STATE_COPY_SOURCE)};
        const UINT barriers = source_state == tag_state ? 4U : 2U;  // staging copies are already sources
        list->ResourceBarrier(barriers, b.data());
        list->CopyResource(image.depth.Get(), depth);
        list->CopyResource(image.motion.Get(), motion);
        for (auto &v : b) std::swap(v.Transition.StateBefore, v.Transition.StateAfter);
        list->ResourceBarrier(barriers, b.data());

        // HUD-less colour from the HUD-free final scene; the brightness comes
        // from the last screen write's CBSystem (closing()).
        if (!s.hudless)
            ++s.hud_skips[static_cast<unsigned>(HudSkip::disabled)];
        else {
            std::unique_ptr<temporal_present::RestoreBindings> restore;
            try {
                // A worker's final-scene list may never set a compute root
                // signature (game-44..46: no_restore rising since v0.12.6).
                restore = std::make_unique<temporal_present::RestoreBindings>(list, roots, true);
                if (!roots.compute.signature) ++s.restore_reasons[5];
            } catch (...) {
                ++s.hud_skips[static_cast<unsigned>(HudSkip::no_restore)];
                const bool reasons[]{!roots.known, !roots.valid, !roots.generation, !roots.pipeline, !roots.graphics.signature};
                for (unsigned i = 0; i < 5; ++i) if (reasons[i]) ++s.restore_reasons[i];
            }
            // v0.24.8: the slot's image follows the back buffer's format (the
            // game switches SDR/HDR10 with its own resizes). The slot is free
            // (fence-gated), so its image can be replaced here.
            const auto wanted = hudless_format(s.back_buffer_format.load(), s.hudless->hdr_supported());
            if (wanted == DXGI_FORMAT_UNKNOWN) {
                ++s.hud_skips[static_cast<unsigned>(HudSkip::format)];
                restore.reset();
            } else if (restore && (!image.hudless || image.hudless->GetDesc().Format != wanted)) {
                ComPtr<ID3D12Device> device;
                const auto desc = hudless_desc(scene->GetDesc(), wanted);
                const D3D12_HEAP_PROPERTIES heap{D3D12_HEAP_TYPE_DEFAULT};
                image.hudless.Reset();
                if (SUCCEEDED(scene->GetDevice(IID_PPV_ARGS(&device))) &&
                    SUCCEEDED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, tag_state, nullptr,
                                                              IID_PPV_ARGS(&image.hudless)))) {
                    image.hudless->SetName(L"mhw-fg-hudless");
                    ++s.hudless_recreated;
                } else {
                    image.hudless.Reset();
                    note_hudless_error(s, "hudless image for a new back buffer format");
                    ++s.hud_skips[static_cast<unsigned>(HudSkip::failed)];
                    restore.reset();
                }
            }
            if (restore) {
                try {
                    s.hudless->record(list, slot, scene, image.hudless.Get(), tag_state, scene_state);
                    s.slot_hudless[slot] = true;
                    ++s.hudless_recorded;
                    s.capture_window.store(true);
                } catch (const std::exception &e) {
                    note_hudless_error(s, e.what());
                    ++s.hud_skips[static_cast<unsigned>(HudSkip::failed)];
                } catch (...) {
                    ++s.hud_skips[static_cast<unsigned>(HudSkip::failed)];
                }
                restore->apply();
            }
        }
        if (confirmed) s.cameras[slot] = staged.camera;
        if (s.ring.recorded(slot, reinterpret_cast<std::uintptr_t>(list), generation))
            ++s.recorded;
        else {
            s.ring.abandon(slot);
            ++s.record_failed;
        }
    } catch (...) {
        ++session.record_failed;
    }
}

bool capture_wanted() noexcept { return active(session) && session.capture_window.load(); }

void closing(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots) noexcept {
    auto &s = session;
    if (!active(s) || !list) return;
    try {
        std::lock_guard lock(s.mutex);
        if (!s.hudless || !s.capture_window.load()) return;
        // v0.24.8: with an HDR10 back buffer, every CBV root argument of the
        // closing list is offered to the CBRenderFrame capture, which keeps
        // only a layout-checked one (and its root parameter); at most
        // capture_limit lists a frame, as for the SDR capture.
        if (roots.known && roots.valid && s.hdr_lists_this_frame < capture_limit &&
            hudless_format(s.back_buffer_format.load(), s.hudless->hdr_supported()) == HudlessPass::hdr_format) {
            unsigned offered{};
            for (unsigned i = 0; i < roots.graphics.arguments.size() && offered < 16; ++i) {
                const auto &a = roots.graphics.arguments[i];
                if (a.kind != temporal_present::ArgumentKind::cbv || !a.address) continue;
                try {
                    s.hudless->capture_hdr(list, a.address, i);
                    ++s.hdr_captures;
                    ++offered;
                } catch (const std::exception &e) {
                    note_hudless_error(s, e.what());
                    ++s.capture_failed;
                    break;
                }
            }
            if (offered) ++s.hdr_lists_this_frame;
        }
        const auto &system = roots.graphics.arguments[system_parameter];
        if (!roots.known || !roots.valid || system.kind != temporal_present::ArgumentKind::cbv || !system.address) {
            ++s.capture_no_cbv;
            return;
        }
        if (s.captures_this_frame >= capture_limit) {
            ++s.capture_limited;
            return;
        }
        temporal_present::observer::RasterBindings raster;
        auto &c = s.candidates[s.captures_this_frame];
        c = {};
        if (temporal_present::observer::raster_current(list, raster)) {
            if (raster.viewport_known)
                c = {static_cast<int>(raster.viewport.TopLeftX), static_cast<int>(raster.viewport.TopLeftY),
                     static_cast<int>(raster.viewport.Width), static_cast<int>(raster.viewport.Height), 0, 0, 0, 0};
            if (raster.scissor_known)
                c[4] = raster.scissor.left, c[5] = raster.scissor.top, c[6] = raster.scissor.right, c[7] = raster.scissor.bottom;
        }
        s.candidate_count = s.captures_this_frame + 1;
        try {
            s.hudless->capture(list, system.address);
            ++s.captures_this_frame;
            ++s.captures;
        } catch (const std::exception &e) {
            note_hudless_error(s, e.what());
            ++s.capture_failed;
        }
    } catch (...) {
        ++s.capture_failed;
    }
}

void submitted(std::uint64_t list, std::uint64_t generation, std::uint64_t batch) noexcept {
    std::lock_guard lock(session.mutex);
    session.ring.submitted(list, generation, batch);
}

void reset(std::uint64_t list, std::uint64_t generation) noexcept {
    std::lock_guard lock(session.mutex);
    session.ring.reset(list, generation);
}

static void finish_pending(Session &s, bool presented);

// Owner thread, under the session lock, each Present: the worker's and the
// player's requests become effective. DLSS-G paces a multiplier above 2x with
// a frame limit that stays after it is turned off and unloaded (perf13, RTX
// 5090: a flat 208 fps after 6x, DLSS on or off, until the player's Reflex
// Off; a stretch at 2x before the unload left none). So a planned Off from
// anything but plain 2x first shows plain 2x (wanted_options()), as a player
// stepping down would. Window changes (lifecycle_guard()) and missing inputs
// still turn it Off at once.
void reconcile(Session &s) {
    const bool armed = s.armed_request.load(), enabled = s.enabled_request.load();
    const bool turning_off = (s.armed.load() && !armed) || (s.enabled.load() && !enabled);
    if (turning_off && s.on.load() && s.sdk->frame_generation()) {
        const auto now = GetTickCount64();
        const bool plain = s.applied_mode == sl::DLSSGMode::eOn && s.applied_generated == 1;
        if (!s.winding && !plain) {
            s.winding = true;
            s.wind_presents = 0;
            s.wind_began = now;
            ++s.winds;
        }
        if (s.winding && now - s.wind_began < wind_down_max_ms &&
            (s.wind_presents < wind_down_presents || now - s.wind_began < wind_down_min_ms))
            return;
    }
    if (s.winding) {
        s.winding = false;
        s.wound_presents += s.wind_presents;
    }
    s.armed.store(armed);
    s.enabled.store(enabled);
}

bool lifecycle_guard(unsigned) noexcept {
    auto &s = session;
    try {
        std::lock_guard lock(s.mutex);
        ++s.lifecycle_calls;
        if (!s.sdk || !s.on.load()) return false;
        // No frame token here: depth/motion/HUD-less stay tagged until the next
        // Present (eValidUntilPresent) and are re-tagged or nulled there.
        const auto result = s.sdk->frame_generation_off(viewport, true);
        if (result != sl::Result::eOk) s.last_sdk_error = result;
        s.on.store(false);
        s.applied_generated = 0;
        s.applied_mode = sl::DLSSGMode::eOff;
        s.applied_target = 0;
        s.shown_generated.store(0);
        s.shown_dynamic.store(false);
        s.single_streak.store(0);
        ++s.mode_changes;
        ++s.lifecycle_offs;
        return true;
    } catch (...) {
        ++session.exceptions;
        return false;
    }
}

void present_begin(const sl::FrameToken *token, std::uint64_t present_frame) noexcept {
    auto &s = session;
    try {
        std::lock_guard lock(s.mutex);
        ++s.presents;
        s.capture_window.store(false);
        s.captures_this_frame = 0;
        s.hdr_lists_this_frame = 0;
        if (!s.sdk) return;
        // A missed present_end must not leak a tagged slot.
        if (s.pending != none) finish_pending(s, false);
        reconcile(s);
        // v0.27.4: DLSS-G unloaded (fg-live-toggle.md): nothing to tag or set;
        // it is Off with nothing held when it loads again with a new chain.
        if (!s.sdk->frame_generation()) {
            ++s.off[static_cast<unsigned>(Off::unloaded)];
            s.on.store(false);
            s.applied_generated = 0;
            s.applied_mode = sl::DLSSGMode::eOff;
            s.shown_generated.store(0);
            s.freed = true;
            return;
        }
        if (!s.enabled.load()) {
            turn_off(s, token, Off::disabled);
            // A long-term Off (guide 6.4): eOff, then slFreeResources once its
            // last On frames are surely done.
            if (!s.freed && ++s.disabled_presents >= free_after_presents) {
                s.freed = true;
                if (const auto r = s.sdk->frame_generation_free(viewport); r == sl::Result::eOk) ++s.frees;
                else {
                    ++s.free_failed;
                    s.last_sdk_error = r;
                }
            }
            return;
        }
        s.disabled_presents = 0;
        s.freed = false;
        if (!s.armed.load()) return turn_off(s, token, Off::not_armed);
        if (!token) return turn_off(s, token, Off::no_token);
        const auto slot = s.ring.take(present_frame, static_cast<std::uint32_t>(*token));
        if (slot == none) return turn_off(s, token, Off::no_ready);
        const auto &entry = s.ring.slot(slot);
        const auto delta = present_frame - entry.frame;
        ++s.present_delta[delta >= 1 && delta <= 3 ? delta - 1 : 3];

        const auto &camera = s.cameras[slot];
        CameraFrame frame;
        frame.view = camera.view;
        frame.projection = camera.projection;
        frame.jitter_x = camera.jitter_x;
        frame.jitter_y = camera.jitter_y;
        frame.render_width = static_cast<unsigned>(s.depth_desc.Width);
        frame.render_height = s.depth_desc.Height;
        // Previous camera = the previously tagged frame, only if consecutive.
        frame.has_previous = s.last.valid && s.last.frame + 1 == entry.frame && s.last.epoch + 1 == entry.epoch;
        frame.reset = camera.reset || !frame.has_previous;
        if (frame.has_previous) {
            frame.previous_view = s.last.camera.view;
            frame.previous_projection = s.last.camera.projection;
        }
        CameraConstants c;
        const auto failure = derive_camera_constants(frame, c);
        if (failure != CameraFailure::none) {
            ++s.camera_failures[static_cast<unsigned>(failure)];
            s.ring.untag(slot);
            s.last = {};
            return turn_off(s, token, Off::camera);
        }
        sl::Constants k{};
        k.cameraViewToClip = matrix(c.view_to_clip);
        k.clipToCameraView = matrix(c.clip_to_view);
        k.clipToPrevClip = matrix(c.clip_to_prev_clip);
        k.prevClipToClip = matrix(c.prev_clip_to_clip);
        k.clipToLensClip = matrix({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
        k.jitterOffset = {c.jitter[0], c.jitter[1]};
        k.mvecScale = {c.mvec_scale[0], c.mvec_scale[1]};
        k.cameraPinholeOffset = {0, 0};
        k.cameraPos = {c.position[0], c.position[1], c.position[2]};
        k.cameraUp = {c.up[0], c.up[1], c.up[2]};
        k.cameraRight = {c.right[0], c.right[1], c.right[2]};
        k.cameraFwd = {c.forward[0], c.forward[1], c.forward[2]};
        k.cameraNear = c.near_plane;
        k.cameraFar = c.far_plane;
        k.cameraFOV = c.fov;
        k.cameraAspectRatio = c.aspect;
        k.depthInverted = sl::eTrue;
        k.cameraMotionIncluded = sl::eTrue;
        k.motionVectors3D = sl::eFalse;
        k.reset = c.reset ? sl::eTrue : sl::eFalse;
        k.orthographicProjection = sl::eFalse;
        k.motionVectorsDilated = sl::eFalse;
        k.motionVectorsJittered = sl::eFalse;
        if (const auto r = s.sdk->constants(k, *token, viewport); r != sl::Result::eOk) {
            s.last_sdk_error = r;
            s.ring.untag(slot);
            return turn_off(s, token, Off::constants);
        }
        auto &image = s.images[slot];
        sl::Resource depth{sl::ResourceType::eTex2d, image.depth.Get(), static_cast<std::uint32_t>(tag_state)};
        sl::Resource motion{sl::ResourceType::eTex2d, image.motion.Get(), static_cast<std::uint32_t>(tag_state)};
        sl::Extent render{0, 0, frame.render_width, frame.render_height};
        // The back buffer's scene rectangle ({top, left, width, height}); the
        // HUD-less image is that size, so its extent matches the subrect.
        static_assert(offsetof(sl::Extent, top) == 0 && offsetof(sl::Extent, left) == 4 && offsetof(sl::Extent, width) == 8 &&
                      offsetof(sl::Extent, height) == 12, "sl::Extent field order");
        const auto fields = extent_fields(s.scene);
        sl::Extent output{fields[0], fields[1], fields[2], fields[3]}, hudless_extent{0, 0, s.scene.width, s.scene.height};
        // A slot without HUD-less clears that tag explicitly (null resource).
        const bool with_hudless = s.slot_hudless[slot] && image.hudless;
        sl::Resource hudless{sl::ResourceType::eTex2d, with_hudless ? image.hudless.Get() : nullptr,
                             static_cast<std::uint32_t>(tag_state)};
        sl::ResourceTag tags[]{
            {&depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eValidUntilPresent, &render},
            {&motion, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eValidUntilPresent, &render},
            {nullptr, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle::eValidUntilPresent, &output},
            {with_hudless ? &hudless : nullptr, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &hudless_extent}};
        if (const auto r = s.sdk->tags(*token, viewport, tags, 4); r != sl::Result::eOk) {
            s.last_sdk_error = r;
            s.ring.untag(slot);
            return turn_off(s, token, Off::tags);
        }
        if (const auto w = wanted_options(s); !s.on.load() || w.mode != s.applied_mode || w.generated != s.applied_generated ||
                                               w.target != s.applied_target) {
            if (const auto r = s.sdk->frame_generation_mode(viewport, w.mode, w.generated, static_cast<float>(w.target), true);
                r != sl::Result::eOk) {
                s.last_sdk_error = r;
                // eDynamic refused although reported supported: the fixed
                // multiplier from the next Present on (fail closed).
                if (w.mode == sl::DLSSGMode::eDynamic) s.dynamic_refused.store(true);
                // The SDK already holds the tags for this frame; keep the slot
                // until its consumption fence like any other tagged frame.
                s.pending = slot;
                ++s.off[static_cast<unsigned>(Off::options)];
                return;
            }
            if (!s.on.load())
                ++s.mode_changes;
            else if (w.mode != s.applied_mode || w.target != s.applied_target)
                ++s.mode_option_changes;
            else
                ++s.generated_changes;
            if (w.fallback) ++s.dynamic_fallbacks;
            s.on.store(true);
            s.applied_generated = w.generated;
            s.applied_mode = w.mode;
            s.applied_target = w.target;
            s.shown_generated.store(w.generated);
            s.shown_dynamic.store(w.mode == sl::DLSSGMode::eDynamic);
        }
        if (s.winding && s.applied_mode == sl::DLSSGMode::eOn && s.applied_generated == 1) ++s.wind_presents;
        s.pending = slot;
        s.last = {true, entry.frame, entry.epoch, camera};
        ++s.tagged;
        if (frame.reset) ++s.reset_tags;
        if (camera.reset) ++s.history_reset_tags;
        if (with_hudless) ++s.hudless_tagged;
        if (s.sample_count < s.samples.size() && (s.tagged <= 4 || (s.tagged & (s.tagged - 1)) == 0))
            s.samples[s.sample_count++] = {present_frame, entry.frame, c};
    } catch (...) {
        ++session.exceptions;
    }
}

void present_end(bool presented) noexcept {
    std::lock_guard lock(session.mutex);
    if (session.pending != none && session.sdk) finish_pending(session, presented);
}

static void finish_pending(Session &s, bool presented) {
    if (!presented) ++s.unpresented;
    sl::DLSSGState state{};
    if (s.sdk->frame_generation_state(viewport, state) == sl::Result::eOk) {
        note_state(s, state);
        ++s.presented_frames[std::min<std::uint32_t>(state.numFramesActuallyPresented, 7)];
        if (state.numFramesActuallyPresented > 1) s.presented_generated.fetch_add(state.numFramesActuallyPresented - 1);
        s.shown_presented.store(state.numFramesActuallyPresented);
        if (presented) note_pacing(s, state.numFramesActuallyPresented);
        if (s.on.load() && state.numFramesActuallyPresented <= 1) s.single_streak.store(s.single_streak.load() + 1);
        else s.single_streak.store(0);
        s.fence = static_cast<ID3D12Fence *>(state.inputsProcessingCompletionFence);
        s.ring.presented(s.pending, state.lastPresentInputsProcessingCompletionFenceValue);
    } else {
        // Without a consumption fence the slot can never be proven free; keep
        // it consuming forever (fail closed) rather than risk overwriting it.
        ++s.state_failed;
        s.ring.presented(s.pending, UINT64_MAX);
    }
    s.pending = none;
}

// A raw GPU attempt can read NaN (game-45: the last attempt was -nan); JSON has
// no non-finite numbers, so those are written as null.
struct Number {
    float value;
    friend std::ostream &operator<<(std::ostream &o, Number n) {
        return std::isfinite(n.value) ? o << n.value : o << "null";
    }
};

void write(std::ostream &o) {
    auto &s = session;
    std::lock_guard lock(s.mutex);
    const auto &r = s.ring.counters();
    o << "{\"schema\":2,\"armed\":" << s.armed.load() << ",\"on\":" << s.on.load() << ",\"configured\":" << (s.sdk != nullptr)
      << ",\"enabled\":" << s.enabled.load() << ",\"frees\":" << s.frees << ",\"free_failed\":" << s.free_failed
      << ",\"winds\":" << s.winds << ",\"wound_presents\":" << s.wound_presents
      << ",\"loaded\":" << (s.sdk && s.sdk->frame_generation())
      << ",\"multiplier\":" << s.requested_multiplier.load() << ",\"generated\":" << s.applied_generated
      << ",\"generated_changes\":" << s.generated_changes << ",\"generated_limited\":" << s.generated_limited
      << ",\"mode\":{\"requested\":" << s.requested_mode.load() << ",\"target\":" << s.requested_target.load()
      << ",\"applied\":" << static_cast<unsigned>(s.applied_mode) << ",\"applied_target\":" << s.applied_target
      << ",\"changes\":" << s.mode_option_changes << ",\"dynamic_fallbacks\":" << s.dynamic_fallbacks
      << ",\"dynamic_refused\":" << s.dynamic_refused.load() << ",\"single_streak\":" << s.single_streak.load() << '}'
      << ",\"pacing\":{\"count_switches\":" << s.count_switches << ",\"long\":" << s.long_presents
      << ",\"long_near_switch\":" << s.long_near_switch << ",\"long_us\":" << Session::long_present_us << '}'
      << ",\"hudless\":{\"available\":" << (s.hudless != nullptr) << ",\"failed\":" << s.hudless_failed
      << ",\"recorded\":" << s.hudless_recorded << ",\"tagged\":" << s.hudless_tagged << ",\"skips\":{";
    for (unsigned i = 0; i < s.hud_skips.size(); ++i) o << (i ? "," : "") << '"' << hud_skip_names[i] << "\":" << s.hud_skips[i];
    o << "},\"restore_reasons\":{";
    constexpr const char *restore_names[]{"unknown", "invalid", "no_generation", "no_pipeline", "no_graphics", "graphics_only"};
    for (unsigned i = 0; i < s.restore_reasons.size(); ++i) o << (i ? "," : "") << '"' << restore_names[i] << "\":" << s.restore_reasons[i];
    o << "},\"probe_ok\":" << s.probe_ok << ",\"probe_fallback\":" << s.probe_fallback << ",\"probe_invalid\":" << s.probe_invalid
      << ",\"gamma_ex\":" << Number{s.last_probe.gamma_ex} << ",\"gamma\":" << Number{s.last_probe.gamma} << ",\"gpu_attempts\":"
      << s.last_probe.attempts << ",\"gpu_matches\":" << s.last_probe.matches << ",\"last_attempt\":["
      << Number{s.last_probe.last_gamma} << ',' << Number{s.last_probe.last_gamma_ex} << "],\"screen_region\":[" << s.last_probe.region[0] << ','
      << s.last_probe.region[1] << ',' << s.last_probe.region[2] << ',' << s.last_probe.region[3] << "],\"candidates\":[";
    for (unsigned i = 0; i < s.candidate_count; ++i) {
        o << (i ? ",[" : "[");
        for (unsigned j = 0; j < 8; ++j) o << (j ? "," : "") << s.candidates[i][j];
        o << ']';
    }
    o << "],\"captures\":" << s.captures
      << ",\"capture_no_cbv\":" << s.capture_no_cbv << ",\"capture_limited\":" << s.capture_limited
      << ",\"capture_failed\":" << s.capture_failed
      << ",\"back_buffer_format\":" << s.back_buffer_format.load() << ",\"recreated\":" << s.hudless_recreated
      << ",\"hdr\":{\"captures\":" << s.hdr_captures << ",\"probe_ok\":" << s.hdr_probe_ok << ",\"probe_default\":" << s.hdr_probe_default
      << ",\"used\":" << s.last_probe.hdr << ",\"valid\":" << s.last_probe.hdr_ok << ",\"white\":" << Number{s.last_probe.white}
      << ",\"ratio\":" << Number{s.last_probe.ratio} << ",\"gamma\":" << Number{s.last_probe.hdr_gamma}
      << ",\"gui_gamma\":" << s.last_probe.gui_gamma << ",\"gpu_attempts\":" << s.last_probe.hdr_attempts
      << ",\"gpu_matches\":" << s.last_probe.hdr_matches << ",\"parameter\":" << s.last_probe.hdr_parameter
      << ",\"last_attempt\":[" << Number{s.last_probe.last_white} << ',' << Number{s.last_probe.last_ratio} << ','
      << Number{s.last_probe.last_hdr_gamma} << ',' << s.last_probe.last_gui << "]}"
      << ",\"error\":\"" << s.hudless_error.data() << "\"},\"staged\":" << s.staged_count
      << ",\"staged_dropped\":" << s.staged_dropped + s.queue.dropped << ",\"final_unstaged\":" << s.final_unstaged
      << ",\"final_mismatch\":" << s.final_mismatch << ",\"mismatch_delta\":[" << s.mismatch_delta[0] << ','
      << s.mismatch_delta[1] << ',' << s.mismatch_delta[2] << ',' << s.mismatch_delta[3] << ',' << s.mismatch_delta[4]
      << "],\"keyed\":{\"on\":" << perf::on(perf::keyed_staging) << ",\"hits\":" << s.keyed_hits
      << ",\"copies\":" << s.staging_copies << ",\"early\":" << s.keyed_early << ",\"confirms\":" << s.keyed_confirms
      << ",\"failed\":" << s.staging_failed << '}' 
      << ",\"presented_frames\":[";
    for (unsigned i = 0; i < s.presented_frames.size(); ++i) o << (i ? "," : "") << s.presented_frames[i];
    o << "]"
      << ",\"created\":" << s.created << ",\"create_failed\":" << s.create_failed
      << ",\"video_memory\":{\"checked\":" << s.memory.done << ",\"read\":" << s.memory.read << ",\"passed\":" << s.memory.passed
      << ",\"budget_mb\":" << (s.memory.budget >> 20) << ",\"usage_mb\":" << (s.memory.usage >> 20)
      << ",\"needed_mb\":" << (s.memory.needed >> 20) << '}' << ",\"output\":[" << s.output_width << ','
      << s.output_height << "],\"scene\":[" << s.scene.x << ',' << s.scene.y << ',' << s.scene.width << ',' << s.scene.height
      << "],\"render\":[" << s.depth_desc.Width << ',' << s.depth_desc.Height << "],\"formats\":["
      << s.depth_desc.Format << ',' << s.motion_desc.Format << "],\"presents\":" << s.presents << ",\"tagged\":" << s.tagged
      << ",\"recorded\":" << s.recorded << ",\"record_failed\":" << s.record_failed << ",\"size_mismatch\":" << s.size_mismatch
      << ",\"state_failed\":" << s.state_failed << ",\"mode_changes\":" << s.mode_changes << ",\"reset_tags\":" << s.reset_tags
      << ",\"history_reset_tags\":" << s.history_reset_tags << ",\"null_tags\":" << s.null_tags
      << ",\"unpresented\":" << s.unpresented << ",\"lifecycle_calls\":" << s.lifecycle_calls << ",\"lifecycle_offs\":" << s.lifecycle_offs << ",\"exceptions\":" << s.exceptions << ",\"last_sdk_error\":" << static_cast<unsigned>(s.last_sdk_error) << ",\"off\":{";
    for (unsigned i = 0; i < s.off.size(); ++i) o << (i ? "," : "") << '"' << off_names[i] << "\":" << s.off[i];
    o << "},\"camera_failures\":[";
    for (unsigned i = 0; i < s.camera_failures.size(); ++i) o << (i ? "," : "") << s.camera_failures[i];
    o << "],\"present_delta\":[" << s.present_delta[0] << ',' << s.present_delta[1] << ',' << s.present_delta[2] << ','
      << s.present_delta[3] << "],\"ring\":{\"acquired\":" << r.acquired << ",\"starved\":" << r.starved << ",\"duplicates\":"
      << r.duplicates << ",\"abandoned\":" << r.abandoned << ",\"submitted\":" << r.submitted << ",\"discarded\":" << r.discarded
      << ",\"taken\":" << r.taken << ",\"stale\":" << r.stale << ",\"no_ready\":" << r.no_ready << ",\"consumed\":" << r.consumed
      << ",\"reclaimed\":" << r.reclaimed << ",\"unconfirmed\":" << r.unconfirmed << ",\"released\":" << r.released
      << "},\"input_releases\":" << s.input_releases << ",\"state\":{\"valid\":" << s.state_valid << ",\"status\":"
      << static_cast<unsigned>(s.state.status) << ",\"min_width_or_height\":" << s.state.minWidthOrHeight
      << ",\"frames_presented\":" << s.state.numFramesActuallyPresented << ",\"max_generated\":" << s.state.numFramesToGenerateMax
      << ",\"dynamic_supported\":" << static_cast<int>(s.state.bIsDynamicMFGSupported)
      << ",\"vram\":" << s.state.estimatedVRAMUsageInBytes << ",\"fence_value\":"
      << s.state.lastPresentInputsProcessingCompletionFenceValue << "},\"samples\":[";
    for (unsigned i = 0; i < s.sample_count; ++i) {
        const auto &x = s.samples[i];
        const auto &c = x.constants;
        o << (i ? "," : "") << "{\"present\":" << x.present_frame << ",\"slot\":" << x.slot_frame << ",\"near\":" << c.near_plane
          << ",\"far\":" << c.far_plane << ",\"far_infinite\":" << c.far_infinite << ",\"fov\":" << c.fov << ",\"aspect\":"
          << c.aspect << ",\"left_handed\":" << c.left_handed << ",\"reset\":" << c.reset << ",\"jitter\":[" << c.jitter[0] << ','
          << c.jitter[1] << "],\"position\":[" << c.position[0] << ',' << c.position[1] << ',' << c.position[2]
          << "],\"forward\":[" << c.forward[0] << ',' << c.forward[1] << ',' << c.forward[2] << "],\"up\":[" << c.up[0] << ','
          << c.up[1] << ',' << c.up[2] << "],\"pinhole\":[" << c.pinhole[0] << ',' << c.pinhole[1] << "],\"clip_to_prev\":[";
        for (unsigned k = 0; k < 16; ++k) o << (k ? "," : "") << c.clip_to_prev_clip[k];
        o << "]}";
    }
    o << "]}";
}

}  // namespace mhw::frame_generation::game
