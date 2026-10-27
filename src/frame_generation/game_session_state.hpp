#pragma once
// The frame generation session's state, shared by its files and the tests;
// the public interface is game_session.hpp.
// - game_session.cpp: the worker's controls (configure, arm, enable,
//   multiplier, mode) and the Status values;
// - game_session_inputs.cpp: DLSS-G's inputs: the staged depth and motion,
//   the HUD-less image, the lists that record them;
// - game_session_present.cpp: each Present: the requests take effect, tags
//   and options, the wind-down, window changes;
// - game_session_log.cpp: the session in the log.
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

namespace mhw::frame_generation::game::detail {
using Microsoft::WRL::ComPtr;
constexpr unsigned slot_count = 3, none = InputRing<slot_count>::none;
constexpr auto tag_state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; // DLSS converter output state
inline const sl::ViewportHandle viewport{0};

enum class Off : unsigned {
    not_armed,
    no_token,
    no_ready,
    camera,
    constants,
    tags,
    options,
    disabled,
    unloaded,
    count
};
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
constexpr std::array<const char *, static_cast<unsigned>(HudSkip::count)> hud_skip_names{"disabled", "no_restore",
                                                                                         "failed", "format"};
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
    SceneRect scene; // In the back buffer; the HUD-less image is scene-sized
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
    StagingQueue<staging_count, SlotCamera> queue; // perf::keyed_staging
    std::array<StagingCopy, staging_count> staging{};
    bool staging_failed{};
    // final frame - newest staged frame at a mismatch: -2, -1, +1, +2, other.
    std::array<std::uint64_t, 5> mismatch_delta{};
    std::uint64_t keyed_hits{}, staging_copies{}, keyed_early{}, keyed_confirms{};
    std::unique_ptr<HudlessPass> hudless;
    bool hudless_failed{};
    std::array<char, 160> hudless_error{}; // First HUD-less failure, JSON-safe
    std::atomic<bool> capture_window{};    // A HUD-less pass was recorded since the last Present
    unsigned captures_this_frame{}, hdr_lists_this_frame{};
    std::uint64_t captures{}, capture_no_cbv{}, capture_limited{}, capture_failed{};
    // v0.24.8: the back buffer's DXGI_FORMAT (back_buffer()); HDR10 captures
    // (one per CBV root argument of a closing list), HDR probes with a valid
    // capture or the shader's defaults, and slot images made for a new format.
    std::atomic<unsigned> back_buffer_format{};
    std::uint64_t hdr_captures{}, hdr_probe_ok{}, hdr_probe_default{}, hudless_recreated{};
    std::uint64_t staged_count{}, staged_dropped{}, final_unstaged{}, final_mismatch{}, hudless_recorded{},
        hudless_tagged{}, probe_ok{}, probe_fallback{}, probe_invalid{};
    std::array<std::uint64_t, static_cast<unsigned>(HudSkip::count)> hud_skips{};
    // Why a HUD-less record found no restorable binding snapshot (no_restore):
    // not known, invalid, no generation, no pipeline, no graphics signature,
    // and graphics-only snapshots restored by unbinding compute (v0.13.7);
    // fresh: a list that had set no pipeline state, restored with it left
    // unset (0.32.0; before, no_pipeline skipped its HUD-less image).
    std::array<std::uint64_t, 7> restore_reasons{};
    HudlessPass::Probe last_probe{};
    std::array<SlotCamera, slot_count> cameras;
    D3D12_RESOURCE_DESC depth_desc{}, motion_desc{};
    bool created{}, create_failed{};
    gpu_memory::Check memory; // create()'s video memory check
    unsigned pending{none};
    Last last;
    ID3D12Fence *fence{}; // Owned by the SDK; only GetCompletedValue is called.
    sl::DLSSGState state{};
    bool state_valid{};
    std::uint64_t presents{}, tagged{}, recorded{}, record_failed{}, size_mismatch{}, state_failed{}, mode_changes{},
        null_tags{}, unpresented{}, exceptions{}, lifecycle_calls{}, lifecycle_offs{};
    std::atomic<unsigned> requested_multiplier{2};
    std::atomic<unsigned> requested_mode{static_cast<unsigned>(Mode::fixed)}, requested_target{};
    unsigned applied_generated{}; // 0 while Off
    sl::DLSSGMode applied_mode{sl::DLSSGMode::eOff};
    unsigned applied_target{};
    std::atomic<bool> dynamic_refused{}; // DLSS-G refused eDynamic: the fixed multiplier from then on
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
    std::uint64_t pacing_present{}; // `presents` at the last finish_pending
    unsigned pacing_count{};        // its numFramesActuallyPresented
    unsigned since_switch{~0U}, since_long{~0U};
    bool long_matched{};
    std::uint64_t count_switches{}, long_presents{}, long_near_switch{};
    std::uint64_t input_releases{}; // live preset changes (release_inputs())
    // Tagged frames with reset: all, and those whose DLSS history was reset
    // (the rest had no consecutive predecessor). DLSS-G presents no generated
    // frame for a reset.
    std::uint64_t reset_tags{}, history_reset_tags{};
    std::array<std::uint64_t, 8> presented_frames{}; // numFramesActuallyPresented per Present: 0..6, other
    std::array<std::uint64_t, static_cast<unsigned>(Off::count)> off{};
    std::array<std::uint64_t, 7> camera_failures{};
    std::array<std::uint64_t, 4> present_delta{}; // present frame - slot frame: 1, 2, 3, other
    std::array<Sample, 8> samples{};
    unsigned sample_count{};
    sl::Result last_sdk_error{sl::Result::eOk};
};
inline Session session;
// Inputs are prepared only while armed by the worker and on by the player.
inline bool active(const Session &s) noexcept {
    return s.armed.load() && s.enabled.load();
}
// Each Present and at configure(): the requests take effect
// (game_session_present.cpp).
void reconcile(Session &s);
} // namespace mhw::frame_generation::game::detail
