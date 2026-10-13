#pragma once
#include <d3d12.h>
#include <array>
#include <cstdint>
#include <iosfwd>
#include "scene_rect.hpp"

namespace sl { struct FrameToken; }
namespace mhw::reflex { class Streamline; }
namespace mhw::temporal_present { struct Bindings; }

// In-game ordinary frame generation (opt-in target only). Process-lived state.
//
// Render side: the DLSS replacement stages a reconstructed frame's converted
// depth/motion and camera; the replaced final-screen draw of the same frame
// (game DIRECT list, before any UI) copies them into an FG-owned slot and
// writes the HUD-less colour from the HUD-free final scene target, using the
// brightness captured from the previous screen write (closing()).
// Submission observer: mark the slot's list generation submitted or reset.
// Owner thread, before PresentStart: tag the newest earlier slot with the
// borrowed Reflex token, set camera constants and FG options. Any missing or
// invalid input turns FG Off for that Present; nothing is guessed.
namespace mhw::frame_generation::game {

struct SlotCamera {
    std::array<float, 16> view{}, projection{};  // fView, fNoJitterProj
    float jitter_x{}, jitter_y{};               // render pixels, NGX convention
    bool reset{};                               // DLSS history decision
};

// Worker, once, after the SDK is attached with the FG plugin loaded, with the
// scene rectangle in the back buffer (scene_rect.hpp).
void configure(reflex::Streamline &sdk, unsigned output_width, unsigned output_height, SceneRect scene) noexcept;
// Worker at arming, and the presentation's lifecycle listener after every
// swap-chain call (v0.24.8): the back buffer's DXGI_FORMAT. The HUD-less image
// follows it: B8G8R8A8/R8G8B8A8_UNORM through the SDR transform,
// R10G10B10A2_UNORM (the game's HDR10) through the PQ transform; any other
// format gets no HUD-less image.
void back_buffer(unsigned format) noexcept;
// Worker. Armed requests FG On while valid inputs arrive; disarmed turns it Off
// at the next Present (Streamline itself stays alive until process exit).
void arm(bool armed) noexcept;
bool frame_generation_on() noexcept;
// Worker, live preset change: disarmed, Off at the last Present, and no
// input slot holding a copy on a list not yet submitted or one the SDK may
// still read.
bool inputs_settled() noexcept;
// Worker, after inputs_settled() and a queue barrier past every earlier
// submission (continuous_dlaa::change_mode): drop the render-size images
// (slot and staging depth/motion) so the next stage() and record() create
// them at the new size; with `scene` (a live aspect change, configure()
// next) the scene-size HUD-less pass and images too. Disarmed only;
// arm(true) then resumes.
bool release_inputs(bool scene = false) noexcept;
// Any thread. Total frame multiplier (2 = ordinary FG), applied at the next
// tagged Present and limited to the reported capability. Other values ignored.
void multiplier(unsigned total) noexcept;
// How the count is chosen, applied at the next tagged Present:
// - fixed: the multiplier (DLSSGMode::eOn);
// - automatic: the same, but DLSS-G pauses generation when the game would run
//   faster without it (eAuto, NVIDIA's legacy mode);
// - dynamic: DLSS-G picks the count toward `target_fps` (0: the display's
//   refresh rate; eDynamic). Only while DLSSGState reports it supported and
//   DLSS-G accepted it; otherwise the fixed multiplier, counted as a fallback.
enum class Mode : unsigned { fixed, automatic, dynamic };
void mode(Mode mode, unsigned target_fps) noexcept;
// Any thread, lock-free: the session as status lines show it (alpha menu).
struct Status {
    bool armed{}, on{};
    unsigned requested{};                 // total multiplier asked for
    unsigned generated{};                 // generated frames per rendered frame now; 0 while Off
    unsigned maximum{};                   // generated frames DLSS-G allows; 0 until its state was read
    std::uint64_t presented_generated{};  // generated frames presented since the start
    Mode mode{};                          // asked for
    unsigned target{};                    // dynamic target asked for; 0: the display's refresh rate
    bool dynamic_applied{};               // DLSS-G runs in eDynamic now
    unsigned dynamic_support{};           // 0 not known yet, 1 no, 2 yes (DLSSGState)
    bool dynamic_refused{};               // DLSS-G refused eDynamic although supported
    unsigned presented_now{};             // frames presented for the last rendered frame
    unsigned single_streak{};             // Presents in a row, while On, that showed no generated frame
};
Status status() noexcept;

// DLSS replacement thread, after a successful evaluation. Without
// perf::keyed_staging no GPU work: the final draw copies `depth`/`motion`
// (DLSS's converted inputs, rewritten every frame). With it, they are copied
// on `list` (the DLSS list, in the converter output state) into one of three
// per-frame staging copies, so a final draw recorded after the next frame's
// DLSS (heavy scenes pipeline them) still reads its own frame's inputs.
void stage(ID3D12GraphicsCommandList *list, std::uint64_t frame, std::uint64_t epoch, ID3D12Resource *depth,
           ID3D12Resource *motion, const SlotCamera &camera) noexcept;
// Final-scene observer (stream_route::FinalSceneObserver contract): `scene` is
// the HUD-free final target in `scene_state` (RENDER_TARGET after the final
// draw; COPY_DEST after the native final copy at render scale 1.0); state must
// match `roots` after.
void record(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots, ID3D12Resource *scene,
            std::uint64_t frame, std::uint64_t epoch,
            D3D12_RESOURCE_STATES scene_state = D3D12_RESOURCE_STATE_RENDER_TARGET) noexcept;
// Close hook, before the native Close of a DIRECT list (this runtime's hooks
// bypassed). After a HUD-less record and before the next Present, records the
// brightness capture from graphics root parameter 5 as the list's last
// commands. `roots` is the list's mirrored bindings.
void closing(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots) noexcept;
// Any thread, lock-free: whether closing() can record now (skip the binding
// lookup otherwise).
bool capture_wanted() noexcept;
void submitted(std::uint64_t list, std::uint64_t generation, std::uint64_t batch) noexcept;
void reset(std::uint64_t list, std::uint64_t generation) noexcept;

// Presentation lifecycle listener (host thread, before a swap-chain resize,
// full-screen change, ResizeTarget or creation is forwarded): DLSS-G must be
// Off before any window manipulation (DLSS-G guide 12.0, 17.0). Turns it Off
// now; a later Present turns it On again only through the normal path.
// Returns whether it was On.
bool lifecycle_guard(unsigned method) noexcept;

void present_begin(const sl::FrameToken *token, std::uint64_t present_frame) noexcept;
void present_end(bool presented) noexcept;

void write(std::ostream &out);

}  // namespace mhw::frame_generation::game
