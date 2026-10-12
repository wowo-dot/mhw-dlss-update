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
}  // namespace mhw::reflex
