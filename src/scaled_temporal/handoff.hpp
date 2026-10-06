#pragma once
#include "../temporal_bindings/inputs.hpp"
#include <iosfwd>
namespace mhw::scaled_temporal::handoff {
inline constexpr unsigned target_rva=0x23917a0, caller_rva=0x23931b3, limit=3;
bool install(const temporal_bindings::Config &,std::string &);
bool active() noexcept;
bool publish(unsigned,const temporal_bindings::Snapshot &) noexcept;
bool healthy() noexcept;
bool complete() noexcept;
void stop() noexcept;
bool settled() noexcept;
bool finish() noexcept;
void write(std::ostream &);
void dispatch(projection_binding::Frame *);
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const temporal_bindings::Config &,void *,std::uintptr_t caller=0);
#endif
}
