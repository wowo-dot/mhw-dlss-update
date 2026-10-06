#pragma once
#include "../temporal_bindings/submission.hpp"
#include <array>
namespace mhw::aa_control::gpu_trace {
bool install(const temporal_bindings::Config &,const std::array<std::uintptr_t,3> &,std::string &);
void begin() noexcept;
bool matches(const std::array<std::uintptr_t,3> &) noexcept;
bool mark(unsigned,std::uint64_t,std::uintptr_t,ID3D12GraphicsCommandList *) noexcept;
void poll() noexcept;
bool healthy() noexcept;
bool complete() noexcept;
// Worker-only: close/drain observation before any slow hook removal. Successful
// captures poll completion/retirement first; partial captures remain rejected.
// This does not remove hooks, release references, or turn a failure into success.
bool close_admission() noexcept;
bool finish() noexcept;
bool disabled() noexcept;
bool safe_release() noexcept;
bool qualified() noexcept;
void write(std::ostream &);
}
