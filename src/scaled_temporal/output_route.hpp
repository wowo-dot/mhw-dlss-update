#pragma once
#include "effects.hpp"
#include "../temporal_bindings/inputs.hpp"
#include <iosfwd>
namespace mhw::scaled_temporal::output_route {
inline constexpr unsigned receipt=2;
bool install(const temporal_bindings::Config &,std::string &);
bool publish_draw(unsigned,unsigned,const temporal_bindings::Snapshot &,const temporal_bindings::Owner &) noexcept;
bool publish_copy(unsigned,const temporal_bindings::Snapshot &,std::uintptr_t packet,ID3D12Resource *,ID3D12Resource *) noexcept;
bool ready(ID3D12GraphicsCommandList *,std::uint64_t generation,std::uint64_t frame,std::uintptr_t recording) noexcept;
void resolved(bool) noexcept;
// Called only inside the selected native draw wrapper, at the DrawInstanced API.
bool draw(unsigned index,const EffectRecipe &,const EffectBindings &,std::uintptr_t rtv,bool &replaced) noexcept;
void barriers(ID3D12GraphicsCommandList *,unsigned,const D3D12_RESOURCE_BARRIER *) noexcept;
bool healthy() noexcept;
bool complete() noexcept;
bool finish(ID3D12Fence *) noexcept;
void write(std::ostream &);
#ifdef MHW_SCALED_ROUTE_FIXTURE
// Private executable fixture only; production retains executable-code gates.
void fixture_start(ID3D12Device *,void *copy);
bool fixture_copy(ID3D12GraphicsCommandList *,ID3D12Resource *,ID3D12Resource *,const D3D12_BOX &,bool caller=true);
#endif
}
