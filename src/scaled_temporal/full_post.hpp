#pragma once
#include <atomic>
#include <cstdint>

// The full-size post chain (prototype, `full_post = on`; docs/research/full-size-post-chain.md).
//
// After DLSS, the game's own post chain runs at the output size, as it does at
// render scale 1.0 (DLAA), instead of the route adapting each effect.
// On the render thread, per armed frame:
// - The final upscale quad stays native. At the post scheduler's entry
//   (0x24f1fc0) the queue must be one the runtime admitted, the registration
//   pass must list TAA's callback, and the draw context must hold the render
//   rectangle.
// - When TAA's callback returns, for a frame the runtime published there, the
//   game's DLAA copy (sub_1423C9250) is recorded in the quad's key after it,
//   overwriting it, so the frame has the identity plan's shape: TAA dispatch,
//   TAA copy, final copy. The route plans it as at DLAA, with the resolve still
//   scaled. Executed without a prepared plan, that copy is dropped and the
//   quad's native upscale stays.
// - Then the context is set to the full rectangle (setViewport 0x23d0330).
//   Every later callback inherits it, and a full-size CBScreen with it.
// - The scene capture (the quest result's background, sub_142268890) reads
//   the chain's output: its texture coordinates are widened to the whole
//   texture once the plan is prepared.
// - The tone map's callback (0x24d0780) runs before TAA's; it runs entirely
//   at the full rectangle (its grade reuses coordinates from its start), and
//   its return restores the render rectangle.
// - The projection helper (0x23cb520) divides by the view scale; while
//   switched, that division is undone (radial blur, god rays).
// - At the scheduler's exit, the render rectangle is restored.
// - The post chain's render-size inputs (scene depth, GBuffer4/5 motion) are
//   upscaled over their output-size textures, at a copy slot the scheduler
//   records right after TAA (full_post.inl, full_post_inputs.hpp).
namespace mhw::scaled_temporal {
inline std::atomic<bool> full_post_enabled{};
}
