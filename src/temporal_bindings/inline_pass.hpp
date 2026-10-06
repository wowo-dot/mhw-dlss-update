#pragma once
#include <d3d12.h>
#include <cstdint>
#include <string>
#include <iosfwd>
namespace mhw::temporal_bindings::inline_pass {
#ifdef MHW_SCALED_OUTPUT
inline constexpr unsigned selected_receipt=2;
bool replacement_claimed() noexcept;
#else
inline constexpr unsigned selected_receipt=4;
#endif
void prepare(ID3D12Device *,unsigned,unsigned,const std::wstring &runtime);
bool record(unsigned,ID3D12GraphicsCommandList *,ID3D12Resource *,std::uint64_t frame,std::uint64_t epoch,std::uintptr_t recording=0) noexcept;
// Adopts the journal's extra reference, which covers the selected Reset.
void completion(ID3D12Fence *) noexcept;
bool save(const std::wstring &prefix);
void write(std::ostream &);
}
