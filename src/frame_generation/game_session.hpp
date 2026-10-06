#pragma once
#include <d3d12.h>
#include <array>
#include <cstdint>
#include <iosfwd>

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

// Worker, once, after the SDK is attached with the FG plugin loaded.
void configure(reflex::Streamline &sdk, unsigned output_width, unsigned output_height) noexcept;
// Worker. Armed requests FG On while valid inputs arrive; disarmed turns it Off
// at the next Present (Streamline itself stays alive until process exit).
void arm(bool armed) noexcept;
bool frame_generation_on() noexcept;
// Any thread. Total frame multiplier (2 = ordinary FG), applied at the next
// tagged Present and limited to the reported capability. Other values ignored.
void multiplier(unsigned total) noexcept;

// DLSS replacement thread, after a successful evaluation. No GPU work.
void stage(std::uint64_t frame, std::uint64_t epoch, ID3D12Resource *depth, ID3D12Resource *motion,
           const SlotCamera &camera) noexcept;
// Final-scene observer (stream_route::FinalSceneObserver contract): `scene` is
// the HUD-free final target in RENDER_TARGET; state must match `roots` after.
void record(ID3D12GraphicsCommandList *list, const temporal_present::Bindings &roots, ID3D12Resource *scene,
            std::uint64_t frame, std::uint64_t epoch) noexcept;
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
