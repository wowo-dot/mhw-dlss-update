#pragma once
#include <atomic>

namespace mhw::perf {
// Lean paths for the per-frame work found in the v0.12 overhead audit. Each
// bit keeps the original fail-closed contract and only changes how it is
// checked; 0 keeps the original path. The plugin starts with all_toggles;
// tools/fg_game.py switches them live ("<log>.perf-N" markers, poll_perf) so
// one session can compare them.
enum Toggle : unsigned {
    cached_reads = 1U,        // validated memory regions reused for about one frame per thread
    cached_methods = 2U,      // system-module method targets remembered after the first check
    list_locks = 4U,          // binding observer: one lock per command list, no process-wide lock
    packet_filter = 8U,       // scaled route: lock only for packets and resources of a built plan
    quiet_status = 16U,       // no 5-second status dumps under the render locks
    no_viewport_trace = 32U,  // no per-RSSetViewports telemetry
    no_output_sentinel = 64U, // retired (0.32.0: DLSS writes the game's output, no private one to clear)
    sample_profile = 128U,    // profile = on: the stack sampler (sampler.hpp); not a lean path
    pooled_selectors = 256U,  // scaled route: a plan slot keeps its GPU selector buffer
    keyed_staging = 512U,     // FG: per-frame copies of the DLSS inputs, matched by frame (v0.12.5)
    owner_lists = 1024U,      // binding observer: no per-list lock; D3D12 lets one thread at a time
                              // record a list, and every reader is a hook on that thread (v0.23.10)
};
inline constexpr unsigned all_toggles = 1919U; // every lean path, keyed_staging and owner_lists
inline constexpr unsigned toggle_mask = 2047U; // every defined bit

inline std::atomic<unsigned> toggles{};
inline bool on(Toggle t) noexcept {
    return (toggles.load(std::memory_order_relaxed) & t) != 0;
}
} // namespace mhw::perf
