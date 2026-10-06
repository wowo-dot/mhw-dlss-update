#include "../src/scaled_temporal/handoff.hpp"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
namespace h=mhw::scaled_temporal::handoff;namespace tb=mhw::temporal_bindings;
using Microsoft::WRL::ComPtr;
extern "C" std::uintptr_t handoff_fixture_call(std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t);
extern "C" void handoff_fixture_return();
void check(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
template<class T>void put(std::uintptr_t p,const T &v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
template<class T>T get(std::uintptr_t p){T v{};std::memcpy(&v,reinterpret_cast<void *>(p),sizeof(v));return v;}
struct Block {
  std::uintptr_t p;
  explicit Block(std::size_t n):p(reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))){check(p!=0,"allocation");}
  ~Block(){VirtualFree(reinterpret_cast<void *>(p),0,MEM_RELEASE);}
};
tb::Config cfg;std::string mode;std::uintptr_t queue_at{},packet_at{},dst_owner{},attachment{};unsigned forwarded{};
std::array<std::uintptr_t,4> arguments;
std::uintptr_t native_copy(std::uintptr_t a,std::uintptr_t b,std::uintptr_t c,std::uintptr_t d) {
  ++forwarded;check(std::array<std::uintptr_t,4>{a,b,c,d}==arguments,"register arguments preserved");
  if(mode=="exception")RaiseException(0xe0424242,0,0,nullptr);
  if(mode=="stop"){h::stop();check(!h::settled(),"forward callback remains pending across stop");}
  if(mode=="nested" && forwarded==1)handoff_fixture_call(a,b,c,d);
  const auto context=cfg.renderer+0x710;const auto count=get<unsigned>(context+0x2b80);
  const auto packet=packet_at+128*count;
  put(packet,mode=="opcode"?6U:13U);put(packet+0x10,dst_owner);put(packet+0x18,c);
  put(packet+0x20,mode=="mode"?1U:0U);put(packet+0x24,0U);put(packet+0x28,0U);
  put(packet+0x30,std::array<unsigned,2>{});put(packet+0x38,get<std::array<unsigned,4>>(context+0x5c));put(packet+0x48,std::uintptr_t{});
  if(mode=="rectangle")put(packet+0x40,7U);
  if(mode=="packet_source")put(packet+0x18,dst_owner);
  if(mode=="packet_destination")put(packet+0x10,c);
  put(queue_at+16*count+8,packet);put(context+0x2b80,count+(mode=="extra_packet"?2U:1U));
  if(mode=="owner_change")put(dst_owner+0x70,1U);
  if(mode=="attachment_change")put(attachment+0x58,c);
  if(mode=="frame_change")put(cfg.renderer+0x1f210,std::uint64_t{999});
  if(mode=="queue_overflow")put(context+0x2b80,65535U);
  // Native lifetime bookkeeping is expected to change without changing ownership.
  put(c+0x40,std::uint64_t{123});put(dst_owner+0x40,std::uint64_t{456});
  return 0x123456789abcdef0ULL;
}
bool invoke(std::uintptr_t a,std::uintptr_t b,std::uintptr_t c,std::uintptr_t d) {
  __try{check(handoff_fixture_call(a,b,c,d)==0x123456789abcdef0ULL,"integer return preserved");return true;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}
}
int main(int argc,char **argv) {
  check(argc>=2,"case");mode=argv[1];
  ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> adapter;ComPtr<ID3D12Device> device;
  check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) && SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))) &&
    SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"WARP device");
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=16;desc.Height=12;
  desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.SampleDesc.Count=1;
  std::array<ComPtr<ID3D12Resource>,2> resources;
  for(auto &r:resources)check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&r))),"resource");
  Block renderer(0x21000),layer(0x1000),owners(0x1000),queue(0x1000),packets(0x1000),view(0x1000);
  cfg={0x140000000,renderer.p,0,reinterpret_cast<std::uintptr_t>(device.Get())};queue_at=queue.p;packet_at=packets.p;
  const auto context=renderer.p+0x710;dst_owner=owners.p+0x100;attachment=owners.p+0x800;
  for(unsigned i=0;i<2;++i){auto p=owners.p+256*i;put(p,owners.p+0x900);put(p+0x58,1U);put(p+0x60,owners.p+0x400+64*i);
    put(p+0x88,16U+(12U<<16));put(p+0x90,7U);put(p+0x94,static_cast<unsigned>(desc.Format));
    put(owners.p+0x420+64*i,reinterpret_cast<std::uintptr_t>(resources[i].Get()));}
  put(context+0x1d0,view.p);put(renderer.p+0x1f450,view.p);put(context+0x2b78,queue.p);put(attachment+0x58,dst_owner);
  put(context+0x5c,std::array<unsigned,4>{0,0,8,6});
  const tb::Reader reader{nullptr,+[](void *,std::uintptr_t at,std::span<std::byte> out){std::memcpy(out.data(),reinterpret_cast<void *>(at),out.size());return true;}};
  h::fixture_start(cfg,reinterpret_cast<void *>(native_copy),reinterpret_cast<std::uintptr_t>(handoff_fixture_return)+(mode=="foreign"?1:0));
  arguments={layer.p,context,owners.p,attachment};
  // Before publication the real entry forwards without consuming a record.
  if(mode=="success"){check(invoke(arguments[0],arguments[1],arguments[2],arguments[3]),"startup forwarding");check(!h::complete()&&h::healthy(),"startup ignored");}
  const bool positive=mode=="success";
  for(unsigned i=0;i<(positive?3U:1U);++i){
    tb::Snapshot s{};s.context=context;s.view=view.p;s.layer=layer.p;s.frame=10+i;s.epoch=20+i;s.bank=i%2;
    put(renderer.p+0x1f210,s.frame);put(renderer.p+0x1f208,s.epoch);put(renderer.p+0x1f200,s.bank);put(layer.p+0x168+8*(s.frame%3),owners.p);
    check(tb::read_owner(reader,owners.p,s.inputs[4]),"source owner");s.inputs[4].id=99;
    check(h::publish(i,s),"publish frame");
    if(mode=="duplicate_publish")check(!h::publish(i,s),"duplicate publication rejected");
    if(mode=="source_identity")arguments[2]=dst_owner;
    if(mode=="wrong_frame")put(renderer.p+0x1f210,std::uint64_t{99});
    if(mode=="alias")put(owners.p+0x460,reinterpret_cast<std::uintptr_t>(resources[0].Get()));
    if(mode=="draw_branch")put(dst_owner+0x90,8U);
    if(mode=="description")put(dst_owner+0x88,15U+(12U<<16));
    check(invoke(arguments[0],arguments[1],arguments[2],arguments[3])==(mode!="exception"),"SEH propagation");
    if(positive){if(!h::healthy()){std::cerr<<std::boolalpha;h::write(std::cerr);std::cerr<<'\n';}check(h::healthy() && h::complete()==(i==2),"capture progress");}
  }
  check(h::finish()==positive,"acceptance");check(h::settled(),"callback drained");
  std::ostringstream log;log<<std::boolalpha;h::write(log);
  const unsigned expected_fault=(positive || mode=="foreign" || mode=="stop")?0U:mode=="nested"?1U:mode=="duplicate_publish"?2U:
    (mode=="source_identity" || mode=="wrong_frame" || mode=="alias" || mode=="description")?3U:mode=="exception"?5U:4U;
  check(log.str().find("\"fault\":"+std::to_string(expected_fault)+",")!=std::string::npos,"intended rejection stage");
  if(argc==3)std::ofstream(argv[2])<<log.str();
  // Captured pins must not keep either resource alive after shutdown.
  for(auto &r:resources){auto p=r.Detach();check(p->Release()==0,"all capture pins released");}
  std::cout<<log.str()<<'\n';return 0;
}
