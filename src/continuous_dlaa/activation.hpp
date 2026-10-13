#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>

namespace mhw::continuous_dlaa::activation {
// Scoped native-stage activation, not the final exclusive resolve/failure mode.
// No saved settings, shared option bytes or native layer enable flags are written.
// Native registration updates the transient postqueue and its layer descriptor.
bool prepare(std::uintptr_t image,std::uintptr_t renderer,std::string &reason);
// Read-only admission inside this thread's owned camera frame. Queued/enabled
// hooks alone do not establish frame ownership.
bool owns_projection(std::uintptr_t context,std::uintptr_t view) noexcept;
void stop() noexcept;
// The player's DLSS Off (reversible, unlike stop): from the next camera frame
// nothing is taken over; the game's own AA option, its units and their jitter
// run as without the mod. yield(false) takes them over again. A frame owned
// when it is called finishes owned.
void yield(bool yielded) noexcept;
// Keep ownership and remove all native AA queue entries after an SDK failure.
// Cleared only by a new diagnostic/process initialization, never input recovery.
void latch_failure() noexcept;
bool drained() noexcept;
bool healthy() noexcept;
bool queue_disable() noexcept;
void disabled(bool success) noexcept;
void write(std::ostream &);
}
