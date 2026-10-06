#pragma once
#include <d3d12.h>
#include <array>
#include <cstdint>
#include <iosfwd>

namespace sl { struct FrameToken; }
namespace mhw::reflex { class Streamline; }

// In-game ordinary frame generation (opt-in target only). Process-lived state.
//
// Render side (game DIRECT list, inside the DLSS replacement scope): copy the
// converted depth/motion of a reconstructed frame into an FG-owned slot.
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

void record(ID3D12GraphicsCommandList *list, std::uint64_t generation, std::uint64_t frame, std::uint64_t epoch,
            ID3D12Resource *depth, ID3D12Resource *motion, const SlotCamera &camera) noexcept;
void submitted(std::uint64_t list, std::uint64_t generation, std::uint64_t batch) noexcept;
void reset(std::uint64_t list, std::uint64_t generation) noexcept;

void present_begin(const sl::FrameToken *token, std::uint64_t present_frame) noexcept;
void present_end(bool presented) noexcept;

void write(std::ostream &out);

}  // namespace mhw::frame_generation::game
