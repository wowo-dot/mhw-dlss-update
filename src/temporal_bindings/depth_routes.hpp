#pragma once
#include "inputs.hpp"
#include <d3d12.h>
#include <iosfwd>
namespace mhw::temporal_bindings::depth_routes {
inline constexpr unsigned limit=64,barrier_limit=32,call_limit=8;
struct Stamp {
  std::uintptr_t context{},view{},container{};
  std::uint64_t frame{},epoch{};unsigned bank{},async{};
  bool operator==(const Stamp &)const=default;
};
struct CopyPacket {
  unsigned opcode{},kind{};std::uintptr_t source{},destination{},nested{};
  std::array<unsigned,6> rectangle{};
  bool operator==(const CopyPacket &)const=default;
};
bool stamp(const Reader &,const Config &,Stamp &);
bool packet(const Reader &,std::uintptr_t,CopyPacket &);
bool copy_states(std::span<const D3D12_RESOURCE_BARRIER>,std::uintptr_t,std::uintptr_t) noexcept;
bool draw_states(std::span<const D3D12_RESOURCE_BARRIER>,std::uintptr_t,std::uintptr_t) noexcept;
bool prepare_pixels(unsigned,unsigned,std::string &);
bool finish_pixels(bool) noexcept;
bool owned_ready() noexcept;
bool save_pixels(const std::wstring &);
void write_pixels(std::ostream &);
bool install(const Config &,std::string &,bool wait_for_projection=false);
void begin(std::uint64_t) noexcept;
bool wants() noexcept;
bool select(unsigned,const Snapshot &,const SceneDepth &,const Packet &) noexcept;
void stop() noexcept;
bool healthy() noexcept;
bool settled() noexcept;
bool finish(bool drain=true) noexcept;
void write(std::ostream &);
void produce(NativeFrame *);
void consume(NativeFrame *);
unsigned barrier_before(ID3D12GraphicsCommandList *,unsigned,const D3D12_RESOURCE_BARRIER *,std::uintptr_t);
void barrier_after(unsigned,const D3D12_RESOURCE_BARRIER *,bool);
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const Config &,void *,void *,void *,bool wait_for_projection=false);
void fixture_copy(ID3D12GraphicsCommandList *,ID3D12Resource *,ID3D12Resource *,std::uintptr_t);
bool fixture_release_pixels(bool) noexcept;
void fixture_map_failure() noexcept;
void fixture_before_marker(void (*)()) noexcept;
#ifdef MHW_TEMPORAL_SCALED
void fixture_draw_start(void *) noexcept;
void fixture_draw(ID3D12GraphicsCommandList *,unsigned,unsigned,unsigned,unsigned,std::uintptr_t);
#endif
#endif
}
