#pragma once
#include "../temporal_bindings/inputs.hpp"
#include <iosfwd>
namespace mhw::scaled_temporal::downstream {
inline constexpr unsigned target_rva=0x2297e20,caller_rva=0x23c7b08;
bool install(const temporal_bindings::Config &,std::string &);
bool active() noexcept;
bool healthy() noexcept;
bool complete() noexcept;
bool select(unsigned,const temporal_bindings::Snapshot &,std::uintptr_t destination) noexcept;
bool finish() noexcept;
void stop() noexcept;
bool settled() noexcept;
void write(std::ostream &);
void dispatch(projection_binding::Frame *);
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const temporal_bindings::Config &,void *,std::uintptr_t caller=0);
#endif
}
