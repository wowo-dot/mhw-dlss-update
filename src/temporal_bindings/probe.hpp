#pragma once
#include "inputs.hpp"
#include <string>
#include <iosfwd>
namespace mhw::temporal_bindings {
bool install(std::uintptr_t,std::string &);
bool done() noexcept;
bool finish() noexcept;
bool settled() noexcept;
void write(std::ostream &);
bool save_shader(const std::wstring &);
void cancel() noexcept;
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const Config &,void *,bool consumer=false,bool jitter=false,bool scene=false,bool routes=false);
void dispatch(NativeFrame *);
void fixture_before_unhook(void (*)()) noexcept;
void fixture_before_drain(void (*)()) noexcept;
#endif
}
