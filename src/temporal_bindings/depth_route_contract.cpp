#include "depth_routes.hpp"
namespace mhw::temporal_bindings::depth_routes {
namespace {bool pointer(std::uintptr_t p,std::uintptr_t n){return p && p<=UINTPTR_MAX-n;}}
static bool operation_states(std::span<const D3D12_RESOURCE_BARRIER> barriers,std::uintptr_t source,std::uintptr_t destination,unsigned source_state,unsigned destination_state) noexcept {
  if(!source || !destination || source==destination)return false;
  std::array<unsigned,2> states{};std::array<bool,2> known{};const std::array<std::uintptr_t,2> resources={source,destination};
  for(const auto &b:barriers)for(unsigned i=0;i<2;++i){
    if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING && (!b.Aliasing.pResourceBefore || !b.Aliasing.pResourceAfter ||
       reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore)==resources[i] || reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter)==resources[i]))return false;
    if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION || reinterpret_cast<std::uintptr_t>(b.Transition.pResource)!=resources[i])continue;
    if(b.Flags || b.Transition.Subresource!=UINT_MAX || (known[i] && states[i]!=static_cast<unsigned>(b.Transition.StateBefore)))return false;
    states[i]=b.Transition.StateAfter;known[i]=true;
  }
  return known[0] && known[1] && states[0]==source_state && states[1]==destination_state;
}
bool copy_states(std::span<const D3D12_RESOURCE_BARRIER> b,std::uintptr_t s,std::uintptr_t d) noexcept {
  return operation_states(b,s,d,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
}
bool draw_states(std::span<const D3D12_RESOURCE_BARRIER> b,std::uintptr_t s,std::uintptr_t d) noexcept {
  // The final native writer may read only from the pixel shader. Keep both
  // observed read states exact; do not admit arbitrary masks with the PS bit.
  return operation_states(b,s,d,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET) ||
    operation_states(b,s,d,0xc0,D3D12_RESOURCE_STATE_RENDER_TARGET);
}
bool stamp(const Reader &r,const Config &c,Stamp &s){
  s={};std::uintptr_t renderer{},backend{},device{},wrapper{},primary{};unsigned sizing{},flags{};unsigned char api{},async{};
  if(!pointer(c.image,0x55d3000) || !pointer(c.renderer,0x1f570) || !pointer(c.backend,0x42708))return false;
  s.context=c.renderer+0x710;
  if(!r.get(c.image+0x51cc080,api) || !api || !r.get(c.image+0x51c4480,renderer) || renderer!=c.renderer ||
     !r.get(renderer+0x78,backend) || backend!=c.backend || !r.get(backend+0x18,device) || device!=c.device || !device ||
     !r.get(backend+0x42700,wrapper) || wrapper || !r.get(renderer+0xb4,sizing) || (sizing&1) ||
     !r.get(s.context+0x1ea74,flags) || (flags&0x54) ||
     !r.get(s.context+0x1d0,s.view) || !pointer(s.view,0x630) || !r.get(renderer+0x1f450,primary) || primary!=s.view ||
     !r.get(s.view+0x628,s.container) || !pointer(s.container,0xa8) ||
     !r.get(renderer+0x1f210,s.frame) || !s.frame || !r.get(renderer+0x1f208,s.epoch) ||
     !r.get(renderer+0x1f200,s.bank) || s.bank>1 || !r.get(renderer+0x93,async) || async>1)return false;
  s.async=async;return true;
}
bool packet(const Reader &r,std::uintptr_t at,CopyPacket &p){
  p={};unsigned raw{};
  if(!pointer(at,0x48) || !r.get(at,raw) || !r.get(at+0x10,p.kind) || p.kind!=8 ||
     !r.get(at+0x18,p.destination) || !p.destination || !r.get(at+0x20,p.source) || !p.source || p.source==p.destination ||
     !r.get(at+0x28,p.rectangle) || !r.get(at+0x40,p.nested))return false;
  p.opcode=raw&31;return p.opcode==10 && p.rectangle[0]<p.rectangle[2] && p.rectangle[1]<p.rectangle[3];
}
}
