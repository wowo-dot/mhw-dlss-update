#pragma once
#include <atomic>

namespace mhw::perf {
// Lean paths for the per-frame work found in the v0.12 overhead audit. Each
// bit keeps the original fail-closed contract and only changes how it is
// checked; 0 keeps the original path. The research build starts at 0 and
// switches live ("<log>.perf-N" markers) so one session can compare them; the
// alpha build starts with every bit set.
enum Toggle : unsigned {
    cached_reads = 1U,        // validated memory regions reused for about one frame per thread
    cached_methods = 2U,      // system-module method targets remembered after the first check
    list_locks = 4U,          // binding observer: one lock per command list, no process-wide lock
    packet_filter = 8U,       // scaled route: lock only for packets and resources of a built plan
    quiet_status = 16U,       // no 5-second status dumps under the render locks
    no_viewport_trace = 32U,  // no per-RSSetViewports telemetry
    no_output_sentinel = 64U, // no diagnostic clear of the DLSS output before evaluation
    sample_profile = 128U,    // research build: stack sampler (sampler.hpp); not a lean path
    pooled_selectors = 256U,  // scaled route: a plan slot keeps its GPU selector buffer
};
inline constexpr unsigned all_toggles = 383U;  // every lean path
inline constexpr unsigned toggle_mask = 511U;  // every defined bit

inline std::atomic<unsigned> toggles{};
inline bool on(Toggle t) noexcept { return (toggles.load(std::memory_order_relaxed) & t) != 0; }
}  // namespace mhw::perf
