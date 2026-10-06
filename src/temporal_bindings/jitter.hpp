#pragma once
#include "inputs.hpp"
namespace mhw::temporal_bindings::jitter {
struct HistorySample {std::uint64_t frame{},epoch{};std::array<unsigned,2> jitter_bits{};};
// Available only after closure, exact TAA association and scoped restoration.
bool history_sample(unsigned,HistorySample &) noexcept;
bool current_history_sample(unsigned,HistorySample &) noexcept;
using FrameAdmission=bool(*)(std::uintptr_t context,std::uintptr_t view) noexcept;
bool prepare(const Config &,std::string &,FrameAdmission=nullptr);
bool begin(std::uint64_t,std::string &);
bool wanted(unsigned) noexcept;
bool matches(unsigned,const Snapshot &) noexcept;
bool acknowledge(unsigned,const Snapshot &) noexcept;
bool healthy() noexcept;
bool complete() noexcept;
bool settled() noexcept;
bool hooks_disabled() noexcept;
void stop() noexcept;
bool finish(bool drain=true) noexcept;
void write(std::ostream &);
void dispatch(NativeFrame *);
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const Config &,void *,std::uint64_t,FrameAdmission=nullptr);
#endif
}
