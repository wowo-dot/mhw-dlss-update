#pragma once
#include <filesystem>
#include <iosfwd>
namespace mhw::aa_control::stream {
bool install(std::uintptr_t,const std::filesystem::path &,std::string &);
bool healthy() noexcept;
void stop() noexcept;
void finish() noexcept;
void write(std::ostream &);
}
