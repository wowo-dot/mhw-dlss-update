#pragma once
#include "../projection_binding/recorder.hpp"
#include <array>
#include <span>
namespace mhw::temporal_bindings {
#ifdef MHW_TEMPORAL_SCALED
inline constexpr unsigned capacity=3;
inline constexpr bool packet_generations=true;
#elif defined(MHW_AA_GPU_TRACE)
inline constexpr unsigned capacity=8;
inline constexpr bool packet_generations=true;
#elif defined(MHW_TEMPORAL_LONG_HISTORY)
inline constexpr unsigned capacity=9;
inline constexpr bool packet_generations=true;
#else
inline constexpr unsigned capacity=3;
inline constexpr bool packet_generations=false;
#endif
inline constexpr unsigned stimulus_count=capacity-2;
inline constexpr unsigned roles=7, dispatch_rva=0x23c9980, caller_rva=0x2392f31;
inline constexpr std::array<unsigned,roles> id_rvas={0x4be60e0,0x4be60e4,0x4be60e8,0x4be60ec,0x4be60f0,0x4be60f4,0x4be60f8};
inline constexpr std::array<const char *,roles> names={"tBaseMap","tBlendMap","tDepthMap","tGBuffer1","rwOutput","tGBuffer4","tPrevGBufferID"};
struct Config {std::uintptr_t image{},renderer{},backend{},device{};};
using Read=bool(*)(void *,std::uintptr_t,std::span<std::byte>);
struct Reader {
  void *user{};Read read{};
  template<class T>bool get(std::uintptr_t p,T &v)const {return read && read(user,p,std::as_writable_bytes(std::span(&v,1)));}
};
struct Owner {
  unsigned id{},kind{},flags{},width{},height{},format{},gpu_format{};
  std::uintptr_t object{},type{},primary{},secondary{},backing{},resource{};
  bool operator==(const Owner &)const=default;
};
struct Snapshot {
  std::uintptr_t layer{},context{},view{},gbuffer{},state{};
  std::uint64_t frame{},epoch{};unsigned bank{},async{},technique{},taa_bytes{};
  std::array<unsigned,4> rectangle{};
  std::array<Owner,roles> inputs{};
  bool operator==(const Snapshot &)const=default;
};
// The native optional-depth getter follows view+628 -> container+A0. This
// candidate is separate from the seven actual TAA bindings (often a fallback).
struct SceneDepth {
  std::uintptr_t container{},srv{},cpu_descriptor{},attachment{},attachment_owner{},attachment_resource{};
  Owner texture{};
  bool attachment_readable{},srv_readable{};
  bool operator==(const SceneDepth &)const=default;
};
bool scene_depth(const Reader &,const Snapshot &,SceneDepth &);
bool read_owner(const Reader &,std::uintptr_t,Owner &);
struct Packet {
  std::uintptr_t object{},state{},indirect{};
  unsigned opcode{},x{},y{},z{},mode{};
  bool operator==(const Packet &)const=default;
};
// Bounded reads only. These routines neither invoke game code nor dereference COM.
bool snapshot(const Reader &,const Config &,std::uintptr_t layer,std::uintptr_t context,Snapshot &);
bool packet(const Reader &,std::uintptr_t,Packet &);
bool packet_matches(const Snapshot &,const Packet &,unsigned x,unsigned y,unsigned z);
struct NativeFrame {projection_binding::Frame call;std::uintptr_t layer{};};
static_assert(offsetof(NativeFrame,layer)==0xc8);
}
