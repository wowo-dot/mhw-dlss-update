#pragma once
#include "post_queue.hpp"
#include <iosfwd>
namespace mhw::aa_control::deferred {
bool prepare(std::uintptr_t,const projection_binding::Runtime &) noexcept;
bool enable() noexcept;
bool publish(unsigned,const PostQueue &,const temporal_bindings::Owner &) noexcept;
bool finished() noexcept;
bool close_observation() noexcept;
bool stop() noexcept;
bool disabled() noexcept;
unsigned fault() noexcept;
void write(std::ostream &);
}
