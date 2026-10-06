#pragma once
#include "../temporal_bindings/inputs.hpp"
#include <iosfwd>
namespace mhw::scaled_temporal::postfilter {
// CPU construction evidence only. Never changes native queue geometry/resources.
bool install(const temporal_bindings::Config &,std::string &);
bool select(unsigned,const temporal_bindings::Snapshot &,std::uintptr_t unit,
            std::uintptr_t final_source,std::uintptr_t final_packet,const temporal_bindings::Owner &final_destination) noexcept;
bool healthy() noexcept;
bool complete() noexcept;
bool settled() noexcept;
void stop() noexcept;
bool finish() noexcept;
bool save(const std::wstring &);
void write(std::ostream &);
void dispatch(projection_binding::Frame *);
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const temporal_bindings::Config &,void *,std::uintptr_t);
#endif
}
