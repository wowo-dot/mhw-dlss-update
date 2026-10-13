#pragma once
#include <atomic>
#include <cstdint>

// The full-size post chain (prototype, `full_post = on`; docs/research/full-size-post-chain.md).
//
// After DLSS, the game's own post chain runs at the output size, as it does at
// render scale 1.0 (DLAA), instead of the route adapting each effect.
// On the render thread, per armed frame:
// - The final upscale quad becomes the game's DLAA copy (sub_1423C9250), so
//   the frame has the identity plan's shape: TAA dispatch, TAA copy, final
//   copy. The route plans it as at DLAA, with the resolve still scaled.
// - At the post scheduler's entry (0x24f1fc0), the draw context must hold the
//   render rectangle.
// - When TAA's callback returns, the context is set to the full rectangle
//   (setViewport 0x23d0330). Every later callback inherits it, and a full-size
//   CBScreen with it.
// - The tone map's callback (0x24d0780) runs before TAA's. Its endpoint key
//   (table index 324, set by 0x2585410) switches to the full rectangle; its
//   return restores the render rectangle.
// - The projection helper (0x23cb520) divides by the view scale; while
//   switched, that division is undone (radial blur, god rays).
// - At the scheduler's exit, the render rectangle is restored.
namespace mhw::scaled_temporal {
inline std::atomic<bool> full_post_enabled{};
}
