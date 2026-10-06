// Included by the owned WARP fixture; no game process or game code is used.
namespace dr=tb::depth_routes;
ID3D12Resource *route_source{};
bool depth_pixels_fixture{};D3D12_CPU_DESCRIPTOR_HANDLE depth_dsv{};
std::uintptr_t route_source_owner{},route_rect{},route_xy{};
unsigned route_producers{},route_consumers{},packet_stride=64;
#ifdef MHW_TEMPORAL_SCALED
#include "temporal_depth_draw_fixture.hpp"
#endif
HANDLE tail_admitted{},tail_continue{};
void pause_tail_marker(){
  check(SetEvent(tail_admitted)!=0,"signal admitted depth callback");
  check(WaitForSingleObject(tail_continue,5000)==WAIT_OBJECT_0,"resume admitted depth callback");
}
void release_tail_at_drain(){
  check(!dr::settled(),"shutdown reaches drain with admitted callback still pending");
  check(SetEvent(tail_continue)!=0,"release depth callback at drain");
}
extern "C" std::uint64_t temporal_depth_producer_entry(std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t,unsigned);
extern "C" std::uint64_t temporal_depth_consumer_entry(std::uintptr_t,std::uintptr_t,std::uintptr_t);
std::uint64_t mock_depth_producer(std::uintptr_t context,std::uintptr_t rect,std::uintptr_t xy,std::uintptr_t destination,unsigned kind){
  ++route_producers;check(context==cfg.renderer+0x710 && rect==route_rect && xy==route_xy && destination==depth_owner && kind==8,"copy producer five-argument ABI");
  if(mode=="route_exception")RaiseException(0xe0424242,0,0,nullptr);
  const auto n=get<unsigned>(context+0x2b80);const auto p=packet_at+packet_stride*n;
  put(p,10U);put(p+0x10,8U);put(p+0x18,destination);put(p+0x20,route_source_owner);
  std::memcpy(reinterpret_cast<void *>(p+0x28),reinterpret_cast<void *>(rect),16);
  std::memcpy(reinterpret_cast<void *>(p+0x38),reinterpret_cast<void *>(xy),8);put(p+0x40,std::uintptr_t{});
#ifdef MHW_TEMPORAL_SCALED
  if(draw_fixture){
    const auto nested=p+0x50,state=route_rect+0x100;
    put(p+0x40,nested);put(nested+0x10,state);put(nested+0x18,state);put(nested+0x20,state);put(nested+0x28,state);
    put(nested+0x40,3U);put(nested+0x44,pixel_writer(route_producers)?3U:0U);put(state+0x4c,std::array<unsigned,4>{0,0,8,6});
  }
#endif
  put(array_at+16*n+8,p);put(context+0x2b80,n+1);
  if(mode=="route_frame_changed")put(cfg.renderer+0x1f208,get<std::uint64_t>(cfg.renderer+0x1f208)+1);
  return 0xaabbccdd11223344ULL;
}
std::uint64_t mock_depth_consumer(std::uintptr_t executor,std::uintptr_t context,std::uintptr_t packet){
  ++route_consumers;check(executor==executor_at && context==consumer_context && packet,"copy consumer ABI");put(recording_at+0x158,packet);
  if(mode=="depth_cancel_exception" && route_consumers==tb::capacity+1)RaiseException(0xe0424242,0,0,nullptr);
  if(depth_pixels_fixture)native_list->ClearDepthStencilView(depth_dsv,D3D12_CLEAR_FLAG_DEPTH,.25F+(tb::capacity==3?.125F:.0625F)*float(get<std::uint64_t>(cfg.renderer+0x1f210)-5),0,0,nullptr);
#ifdef MHW_TEMPORAL_SCALED
  if(draw_fixture){depth_draw_commands(packet);return 0x1122334455667788ULL;}
#endif
  std::array<D3D12_RESOURCE_BARRIER,2> b{};
  b[0].Transition.pResource=route_source;b[0].Transition.Subresource=UINT_MAX;b[0].Transition.StateBefore=D3D12_RESOURCE_STATE_DEPTH_WRITE;b[0].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
  b[1].Transition.pResource=depth_resource;b[1].Transition.Subresource=UINT_MAX;
  b[1].Transition.StateBefore=route_consumers==1?D3D12_RESOURCE_STATE_RENDER_TARGET:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;b[1].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
  if(mode=="depth_wrong_state" || (mode=="depth_multi" && route_consumers%3))b[0].Transition.StateAfter=static_cast<D3D12_RESOURCE_STATES>(0x840);
  const auto source_copy_state=b[0].Transition.StateAfter;
  tb::gpu::fixture_barrier(native_list,2,b.data());
  dr::fixture_copy(native_list,depth_resource,route_source,cfg.image+0x259e100+(mode=="route_wrong_call"?1:0));
  b[0].Transition.StateBefore=source_copy_state;b[0].Transition.StateAfter=D3D12_RESOURCE_STATE_DEPTH_WRITE;
  b[1].Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;b[1].Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
  tb::gpu::fixture_barrier(native_list,2,b.data());return 0x1122334455667788ULL;
}
bool invoke_depth_producer(){
  __try {check(temporal_depth_producer_entry(cfg.renderer+0x710,route_rect,route_xy,depth_owner,8)==0xaabbccdd11223344ULL,"copy producer return");return true;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}
}
bool invoke_depth_consumer(std::uintptr_t packet){
  __try{check(temporal_depth_consumer_entry(executor_at,consumer_context,packet)==0x1122334455667788ULL,"native depth consumer return");return true;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}
}
