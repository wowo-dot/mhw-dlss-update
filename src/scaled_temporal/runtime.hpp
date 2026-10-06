#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
namespace mhw::scaled_temporal {
bool install(std::uintptr_t,std::string &);
bool healthy() noexcept;
bool finish() noexcept;
void write(std::ostream &);
}
