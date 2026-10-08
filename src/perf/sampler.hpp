#pragma once
#include <ostream>

namespace mhw::perf::sampler {
// Wall-clock stack sampler for the overhead work (v0.12.2), active only while
// perf::sample_profile is on. About 1,000 times a second it suspends the host
// Present thread (frame_stats.hpp present_thread) and, in rotation, two other
// threads of the process; it records the running function, the innermost run
// of this DLL's frames (first frame and the hook the game called) and the
// innermost game frame, and resumes the thread. Nothing
// allocates or takes a lock while a thread is suspended: modules and function
// tables are read up front and the stack is unwound with RtlVirtualUnwind
// against them. Waiting threads are sampled too, so the Present thread's
// counts are where its frame time goes. Symbols: tools/profile_report.py.
void start() noexcept;
// One "profile" event with the samples since the last call, if there are any.
void write(std::ostream& out);
}  // namespace mhw::perf::sampler
