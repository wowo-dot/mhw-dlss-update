#pragma once
#include <d3d12.h>
#include <array>
#include <iosfwd>
#include <string>
namespace mhw::aa_control::input_readback {
using Barrier=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,const D3D12_RESOURCE_BARRIER *);
inline constexpr std::array<unsigned,3> receipts={0,3,7};
bool prepare(ID3D12Device *,const std::array<ID3D12Resource *,3> &) noexcept;
bool selected(unsigned) noexcept;
// Caller holds the matching recording lease; all native states are explicitly
// known PIXEL_SHADER_RESOURCE. This issues copies only, never pipeline bindings.
bool record(unsigned,ID3D12GraphicsCommandList *,Barrier) noexcept;
bool recorded() noexcept;
// A partial submission retains every potentially referenced object. Successful
// completion and retirement allow release after saving the CPU readbacks.
void retire(bool) noexcept;
bool save(const std::wstring &);
void write(std::ostream &);
}
