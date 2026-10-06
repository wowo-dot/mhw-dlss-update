#pragma once
#include <d3d12.h>
#include <string>
#include <iosfwd>

namespace mhw::temporal_bindings::owned {
bool prepare(ID3D12Device *,unsigned,unsigned,std::string &);
ID3D12Resource *input(unsigned receipt,unsigned role) noexcept; // color, depth, motion
bool convert_save(const std::wstring &);
void write(std::ostream &);
#ifdef MHW_TEMPORAL_DLSS
void configure_sdk(const std::wstring &runtime);
bool evaluate_save(const std::wstring &prefix);
#endif
}
