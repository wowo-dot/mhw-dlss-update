#include "../src/scaled_temporal/postfilter.hpp"
#include "../src/scaled_temporal/draw_execution.hpp"
#include <windows.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
namespace pf=mhw::scaled_temporal::postfilter;namespace tb=mhw::temporal_bindings;
extern "C" std::uintptr_t postfilter_fixture_call(std::uintptr_t,std::uintptr_t);
extern "C" void postfilter_fixture_return();
void check(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
template<class T>void put(std::uintptr_t p,const T &v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
template<class T>T get(std::uintptr_t p){T v{};std::memcpy(&v,reinterpret_cast<void *>(p),sizeof(v));return v;}
struct Block {std::uintptr_t p;explicit Block(std::size_t n):p(reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))){check(p!=0,"allocation");}~Block(){VirtualFree(reinterpret_cast<void *>(p),0,MEM_RELEASE);}};
tb::Config cfg;std::string mode;std::uintptr_t unit_at{},queue_at{},draw_at{},vertices_at{},output_at{},grade_at{};unsigned forwarded{};
std::uintptr_t native_filter(std::uintptr_t unit,std::uintptr_t context){
  check(unit==unit_at&&context==cfg.renderer+0x710,"arguments preserved");++forwarded;
  if(mode=="exception")RaiseException(0xe0424242,0,0,nullptr);
  if(mode=="nested"&&forwarded==1)postfilter_fixture_call(unit,context);
  if(mode=="stop"){pf::stop();check(!pf::settled(),"callback retained during stop");}
  const auto count=get<unsigned>(context+0x2b80);put(queue_at+16*count,(15U<<27)|100U);put(queue_at+16*count+8,draw_at);
  put(context+0x2b80,count+(mode=="overflow"?33U:1U));
  if(mode=="frame_change")put(cfg.renderer+0x1f210,std::uint64_t{999});
  if(mode=="vertices_change")put(vertices_at,1.f);
  if(mode=="owner_change")put(output_at+0x90,35U);
  if(mode=="viewport_change")put(context+0x64,1U);
  if(mode=="grade_owner")put(grade_at+0x248,output_at+0x100);
  if(mode=="grade_vertices")put(vertices_at+256,1.f);
  return 0x123456789abcdef0ULL;
}
bool invoke(){__try{check(postfilter_fixture_call(unit_at,cfg.renderer+0x710)==0x123456789abcdef0ULL,"return preserved");return true;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}}
int main(int argc,char **argv){
  check(argc>=2,"case");mode=argv[1];const bool publishing=mode=="publish_draws";
  const bool positive=mode=="success"||publishing;
  Block image(0x5200000),renderer(0x21000),view(0x1000),unit(0x1000),mem(0x6000),grade(0x1000);
  cfg={image.p,renderer.p,0,0};const auto context=renderer.p+0x710;unit_at=unit.p;
  queue_at=mem.p+0x1000;const auto final_packet=mem.p+0x2000;draw_at=mem.p+0x2100;vertices_at=mem.p+0x3000;
  output_at=mem.p+0x100;const auto graphics=mem.p+0x4000,handle=graphics+0x100,set=handle+0x100,entry=set+0x100,desc=entry+0x100;
  grade_at=grade.p;const auto filters=mem.p+0x5500,grade_packet=mem.p+0x2200;
  put(image.p+0x51cc238,filters);put(filters+0x3c,2U);put(filters+0x40,grade.p+0x140);put(filters+0x48,grade.p+0x238);
  put(grade.p,image.p+0x3538400);put(grade.p+0x140,15U);put(grade.p+0x148,grade.p);
  put(grade.p+0x238,15U);put(grade.p+0x23c,0x7fffff1U);put(grade.p+0x248,output_at);put(grade.p+0x250,mem.p+0x200);put(grade.p+0x258,static_cast<unsigned char>(1));
  for(unsigned i=0;i<4;++i){auto p=mem.p+0x100*i;put(p,mem.p+0x900);put(p+0x58,1U);put(p+0x60,mem.p+0x600+64*i);
    put(p+0x88,2560U+(1440U<<16));put(p+0x90,34U);put(mem.p+0x620+64*i,mem.p+0x900+64*i);}
  put(context+0x1d0,view.p);put(renderer.p+0x1f450,view.p);put(context+0x2b78,queue_at);put(context+0x2b50,vertices_at);
  put(unit.p,image.p+0x3537a70);put(unit.p+0x150,mem.p);put(unit.p+0x158,output_at);
  for(unsigned i=0;i<3;++i){const auto p=final_packet+0x100*i,s=p+0x80;
    put(p,0U);put(p+0x10,mem.p+0x5800);put(p+0x18,s);put(p+0x20,graphics);put(p+0x40,i?4U:3U);put(s+0x20,static_cast<unsigned char>(16));put(s+0x24,128U*i);}
  // Distinguish the view attachment from its +0x58 texture owner. The observer
  // must reuse the already-qualified texture, never interpret the view as one.
  put(view.p+0x460,mem.p+0x5a00);put(mem.p+0x5a58,mem.p+0x300);
  tb::Reader reader{nullptr,[](void *,std::uintptr_t p,std::span<std::byte> out){std::memcpy(out.data(),reinterpret_cast<void *>(p),out.size());return true;}};
  tb::Owner destination{};check(tb::read_owner(reader,mem.p+0x300,destination),"texture destination");
  put(vertices_at,std::array<float,12>{-1,1,0,0,-1,-3,0,.999305556f,3,1,.999609375f,0});
  put(vertices_at+128,std::array<float,16>{-1,1,0,0,-1,-1,0,1,1,1,1,0,1,-1,1,1});
  put(vertices_at+256,std::array<float,16>{-1,1,0,0,-1,-1,0,.5F,1,1,.5F,0,1,-1,.5F,.5F});
  Microsoft::WRL::ComPtr<ID3DBlob> code,error;
  const char hlsl[]="float4 main(float4 p:SV_Position):SV_Target{return float4(p.xy,0,1);}";
  check(SUCCEEDED(D3DCompile(hlsl,sizeof(hlsl),nullptr,nullptr,nullptr,"main","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&error)),"pixel fixture shader");
  put(graphics+8,handle);put(graphics+0x18,7U);put(handle,set);put(set+0x58,entry);put(entry+0x10,desc);
  put(desc,static_cast<unsigned>(code->GetBufferSize()));put(desc+8,reinterpret_cast<std::uintptr_t>(code->GetBufferPointer()));
  pf::fixture_start(cfg,reinterpret_cast<void *>(native_filter),reinterpret_cast<std::uintptr_t>(postfilter_fixture_return)+(mode=="foreign"?1:0));
  if(publishing)mhw::scaled_temporal::draw_execution::fixture_start(cfg,nullptr);
  for(unsigned i=0;i<(positive?3U:1U);++i){tb::Snapshot s{};s.context=context;s.view=view.p;s.frame=10+i;s.epoch=20+i;s.bank=i%2;
    put(renderer.p+0x1f210,s.frame);put(renderer.p+0x1f208,s.epoch);put(renderer.p+0x1f200,s.bank);
    put(context+0x5c,std::array<unsigned,4>{0,0,1280,720});
    put(context+0x2b80,1U);put(queue_at,0x7ffffff1U);put(queue_at+8,grade_packet);
    if(mode=="grade_missing")put(context+0x2b80,0U);
    if(mode=="grade_duplicate"){put(context+0x2b80,2U);put(queue_at+16,0x7ffffff1U);put(queue_at+24,grade_packet);}
    if(mode=="grade_endpoint")put(filters+0x48,grade.p+0x240);
    if(mode=="unreadable")put(final_packet+0x18,std::uintptr_t{1});
    if(mode=="stride")put(final_packet+0xa0,static_cast<unsigned char>(32));
    const bool selected=pf::select(i,s,unit.p,mem.p+0x200,final_packet,destination);
    check(selected==(mode!="unreadable"&&mode!="stride"&&mode!="grade_missing"&&mode!="grade_duplicate"&&mode!="grade_endpoint"),"selection admission");
    check(!pf::complete(),"completion waits for native filter return");
    if(mode=="shader_size")put(desc,262145U);
    if(mode=="no_draw")put(draw_at,6U);
    if(mode=="thread"){std::thread t([]{invoke();});t.join();}else check(invoke()==(mode!="exception"),"exception forwarded");
    if(positive)check(pf::complete()==(i==2&&!publishing),"completion includes all selected filters and draws");
    if(publishing)check(pf::healthy()&&mhw::scaled_temporal::draw_execution::fixture_publications()==3*(i+1),"all three packet publications use verified destination");
  }
  check(pf::finish()==(positive&&!publishing),"finish");check(pf::settled(),"drain");
  if(positive&&argc==3)check(pf::save(std::wstring(argv[2],argv[2]+std::strlen(argv[2]))),"shader save");
  std::ostringstream log;log<<std::boolalpha;pf::write(log);std::cout<<log.str()<<'\n';
  if(argc==3)std::ofstream(std::string(argv[2])+".json")<<log.str();return 0;
}
