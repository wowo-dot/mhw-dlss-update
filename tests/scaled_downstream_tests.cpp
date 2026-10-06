#include "../src/scaled_temporal/downstream.hpp"
#include <windows.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
namespace d=mhw::scaled_temporal::downstream;namespace tb=mhw::temporal_bindings;
extern "C" std::uintptr_t downstream_fixture_call(std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t);
extern "C" void downstream_fixture_return();
void check(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
template<class T>void put(std::uintptr_t p,const T &v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
template<class T>T get(std::uintptr_t p){T v{};std::memcpy(&v,reinterpret_cast<void *>(p),sizeof(v));return v;}
struct Block {std::uintptr_t p;explicit Block(std::size_t n):p(reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))){check(p!=0,"allocation");}~Block(){VirtualFree(reinterpret_cast<void *>(p),0,MEM_RELEASE);}};
tb::Config cfg;std::string mode;std::uintptr_t array_at{},packet_at{},view_at{};unsigned forwarded{};
std::array<std::uintptr_t,6> arguments{};
std::uintptr_t native_quad(std::uintptr_t a,std::uintptr_t b,std::uintptr_t c,std::uintptr_t e,std::uintptr_t f,std::uintptr_t g){
  ++forwarded;check(std::array<std::uintptr_t,6>{a,b,c,e,f,g}==arguments,"register and stack arguments preserved");
  if(mode=="exception")RaiseException(0xe0424242,0,0,nullptr);
  if(mode=="nested"&&forwarded==1)downstream_fixture_call(a,b,c,e,f,g);
  if(mode=="stop"){d::stop();check(!d::settled(),"pending callback during stop");}
  const auto count=get<unsigned>(b+0x2b80);const auto packet=packet_at+count*64;
  put(packet,1U);put(array_at+16*count,16U<<27);put(array_at+16*count+8,packet);
  put(b+0x2b80,count+(mode=="overflow"?9U:1U));
  if(mode=="viewport_change")put(b+0x64,7U);
  if(mode=="frame_change")put(cfg.renderer+0x1f210,std::uint64_t{999});
  if(mode=="attachment_change")put(view_at+0x460,std::uintptr_t{});
  return 0xfedcba9876543210ULL;
}
bool invoke(){__try{check(downstream_fixture_call(arguments[0],arguments[1],arguments[2],arguments[3],arguments[4],arguments[5])==0xfedcba9876543210ULL,"return preserved");return true;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}}
int main(int argc,char **argv){
  check(argc>=2,"case");mode=argv[1];const bool positive=mode=="success";
  Block image(0x5200000),renderer(0x21000),view(0x1000),layer(0x1000),mem(0x5000);
  cfg={image.p,renderer.p,0,0};view_at=view.p;
  const auto context=renderer.p+0x710,source=mem.p,destination=mem.p+0x100,attachment=mem.p+0x800;
  const auto filter=mem.p+0x1000;array_at=mem.p+0x2000;packet_at=mem.p+0x3000;
  for(unsigned i=0;i<2;++i){auto p=mem.p+256*i;put(p,mem.p+0x900);put(p+0x58,1U);put(p+0x60,mem.p+0x400+64*i);
    put(p+0x88,2560U+(1440U<<16));put(p+0x90,34U);put(mem.p+0x420+64*i,mem.p+0x900+64*i);}
  put(context+0x1d0,view.p);put(renderer.p+0x1f450,view.p);put(context+0x2b78,array_at);put(attachment+0x58,destination);
  put(view.p+0x460,attachment);put(view.p+0x420,attachment+0x80);put(attachment+0xd8,source);
  put(context+0x154,16U);put(image.p+0x4be7450,7U);put(context+0x710+8*7,source);put(image.p+0x51cc238,filter);
  put(filter+0x3c,1U);put(filter+0x40,layer.p+0x140);put(layer.p,std::uintptr_t{0x140123450});
  put(layer.p+0x140,15U);put(layer.p+0x144,2U);put(layer.p+0x148,layer.p);put(layer.p+0x150,source);put(layer.p+0x158,destination);
  const auto rect=mem.p+0x1800,size=rect+32,offset=size+16;
  put(rect,std::array<unsigned,4>{0,0,1280,720});put(size,std::array<unsigned,2>{2560,1440});put(offset,std::array<float,2>{0.5f/2560,0.5f/1440});
  d::fixture_start(cfg,reinterpret_cast<void *>(native_quad),reinterpret_cast<std::uintptr_t>(downstream_fixture_return)+(mode=="foreign"?1:0));
  arguments={renderer.p,context,rect,size,offset,1};
  for(unsigned i=0;i<(positive?3U:1U);++i){
    tb::Snapshot s{};s.context=context;s.view=view.p;s.layer=layer.p;s.frame=10+i;s.epoch=20+i;s.bank=i%2;s.inputs[0].object=source;
    put(renderer.p+0x1f210,s.frame);put(renderer.p+0x1f208,s.epoch);put(renderer.p+0x1f200,s.bank);
    put(context+0x5c,std::array<unsigned,4>{0,0,2560,1440});
    if(mode=="unreadable")arguments[2]=1;
    if(mode=="triangle")arguments[5]=2;
    check(invoke()==(mode!="exception"),"SEH propagation");
    if(mode=="late_frame")put(renderer.p+0x1f210,s.frame+1);
    if(mode=="owner_change")put(source+0x90,35U);
    if(mode=="packet_change")put(packet_at,2U);
    if(mode=="key_change")put(array_at,15U<<27);
    if(mode=="filter_overflow")put(filter+0x3c,17U);
    if(mode=="wrong_destination")put(layer.p+0x158,source);
    if(mode=="missing_taa")put(filter+0x40,layer.p+0x240);
    bool accepted{};
    if(mode=="thread") {std::thread t([&]{accepted=d::select(i,s,destination);});t.join();}
    else accepted=d::select(i,s,destination);
    check(accepted==positive,"capture acceptance");
  }
  check(d::finish()==positive,"finish acceptance");check(d::settled(),"drained");
  std::ostringstream log;log<<std::boolalpha;d::write(log);if(argc==3)std::ofstream(argv[2])<<log.str();
  std::cout<<log.str()<<'\n';return 0;
}
