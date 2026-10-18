#pragma once

namespace mhw::reflex {
// The Reflex mode the player chose. The ini sets it at start and the
// menu changes it live; the worker applies it to Streamline
// (slReflexSetOptions) at arming and then on its next poll (100 ms). Values
// are sl::ReflexMode: 0 off, 1 low latency, 2 low latency with boost. The
// worker refuses off while DLSS Frame Generation is loaded, which needs Reflex.
void request_mode(unsigned mode) noexcept;
// DLSS-G is loaded: Reflex stays on until the game closes (Streamline paces
// the swap chain; without Reflex the frame rate halves).
bool frame_generation_loaded() noexcept;
} // namespace mhw::reflex
