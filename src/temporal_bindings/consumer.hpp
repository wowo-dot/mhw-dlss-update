#pragma once
#include "shader.hpp"
#include <d3d12.h>
namespace mhw::temporal_bindings::gpu {
inline constexpr unsigned barrier_limit=128, barrier_call_limit=16;
bool install(const Config &,std::string &);
void begin(std::uint64_t) noexcept;
void stop() noexcept;
std::uint64_t publication_begin() noexcept;
bool publish(unsigned,const Snapshot &,const Packet &,const ShaderRecipe &,std::uintptr_t,std::uintptr_t,std::uint64_t,const SceneDepth *scene=nullptr);
bool prepare_motion(unsigned,unsigned,std::string &,bool with_color=false);
bool save_motion(const std::wstring &);
bool finish_motion(bool) noexcept;
bool owned_ready() noexcept;
void write_motion(std::ostream &);
void write_color(std::ostream &);
void WINAPI original_barriers(ID3D12GraphicsCommandList *,unsigned,const D3D12_RESOURCE_BARRIER *);
bool complete() noexcept;
bool healthy() noexcept;
bool settled() noexcept;
bool finish(bool drain=true) noexcept;
void write(std::ostream &);
void consume(NativeFrame *);
#ifdef MHW_TEMPORAL_FIXTURE
bool fixture_republish(unsigned,unsigned,std::uint64_t,std::uint64_t,bool overlap=false);
void fixture_start(const Config &,ID3D12GraphicsCommandList *,void *,void *,void *);
void fixture_barrier(ID3D12GraphicsCommandList *,unsigned,const D3D12_RESOURCE_BARRIER *);
void fixture_dispatch(ID3D12GraphicsCommandList *,unsigned,unsigned,unsigned,std::uintptr_t);
bool fixture_release_motion(bool) noexcept;
void fixture_motion_map_failure() noexcept;
void fixture_color_map_failure() noexcept;
#endif
}
