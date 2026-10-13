#pragma once

namespace mhw::reflex {
// The Reflex mode the player chose (alpha). The ini sets it at start and the
// menu changes it live; the worker applies it to Streamline
// (slReflexSetOptions) at arming and then on its next poll (100 ms). Values
// are sl::ReflexMode: 0 off, 1 low latency, 2 low latency with boost. The
// worker refuses off while DLSS Frame Generation is loaded, which needs Reflex.
void request_mode(unsigned mode) noexcept;
// The mode Streamline last accepted; 3 before the frame hooks are armed.
unsigned applied_mode() noexcept;
// DLSS Frame Generation on this card and system, known once the game's device
// exists: 0 not known yet, 1 no (or frame_generation_plugin = off), 2 yes.
unsigned frame_generation_support() noexcept;
// DLSS-G is loaded: Reflex stays on until the game closes (Streamline paces
// the swap chain; without Reflex the frame rate halves).
bool frame_generation_loaded() noexcept;
}  // namespace mhw::reflex
