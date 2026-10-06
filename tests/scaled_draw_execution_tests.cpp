#include "../src/scaled_temporal/draw_execution.hpp"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
namespace de=mhw::scaled_temporal::draw_execution;namespace tb=mhw::temporal_bindings;
using Microsoft::WRL::ComPtr;
extern "C" std::uintptr_t scaled_draw_execution_entry(std::uintptr_t,std::uintptr_t,std::uintptr_t);
void check(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
template<class T>void put(std::uintptr_t p,const T &v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
template<class T>T get(std::uintptr_t p){T v{};std::memcpy(&v,reinterpret_cast<void *>(p),sizeof(v));return v;}
struct Block {std::uintptr_t p;explicit Block(std::size_t n):p(reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))){check(p!=0,"allocation");}~Block(){VirtualFree(reinterpret_cast<void *>(p),0,MEM_RELEASE);}};
std::string mode;std::uintptr_t expected_executor{},expected_context{},state_at{},attachment_at{},ps_desc{},elements_at{};unsigned forwarded{};
std::uintptr_t native_draw(std::uintptr_t executor,std::uintptr_t context,std::uintptr_t packet){
  check(executor==expected_executor&&context==expected_context,"forwarded arguments");++forwarded;
  if(mode=="exception")RaiseException(0xe0434343,0,0,nullptr);
  if(mode=="nested"&&forwarded==1)scaled_draw_execution_entry(executor,context,packet);
  if(mode=="stop"){de::stop();check(!de::settled(),"pending callback retained");}
  put(state_at+0x158,packet);put(state_at+0x108,std::uintptr_t{1234});put(state_at+0x110,std::uintptr_t{5678});
  put(context+0x10,attachment_at);put(state_at+0x98,std::uintptr_t{999});
  const auto rect=get<std::array<int,4>>(attachment_at+0x4c);put(state_at+0xe0,rect);put(state_at+0xf0,rect);
  if(mode=="cache")put(state_at+0x98,std::uintptr_t{998});
  if(mode=="changed")put(attachment_at+0x4c,1);
  if(mode=="shader_change")put(ps_desc,53U);
  if(mode=="packet_change")put(state_at+0x158,packet+8);
  if(mode=="layout_change")put(elements_at+8,1U);
  return 0x123456789abcdef0ULL;
}
bool invoke(std::uintptr_t packet){__try{
  check(scaled_draw_execution_entry(expected_executor,expected_context,packet)==0x123456789abcdef0ULL,"return preserved");return true;
}__except(GetExceptionCode()==0xe0434343?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}}
int main(int argc,char **argv){
  check(argc>=2,"case");mode=argv[1];const bool positive=mode=="success";
  ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> adapter;ComPtr<ID3D12Device> device;ComPtr<ID3D12Resource> resource;
  check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))&&
    SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"WARP device");
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
  D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=desc.Height=16;desc.DepthOrArraySize=desc.MipLevels=1;
  desc.Format=DXGI_FORMAT_R11G11B10_FLOAT;desc.SampleDesc.Count=1;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&resource))),"real output resource");
  Block image(0x5200000),mem(0x10000);
  auto at=[&](unsigned off){return mem.p+off;};
  const auto renderer=at(0),backend=at(0x1000),context=at(0x4000),executor=at(0x5000),state=at(0x6000),attachment=at(0x7000);
  const auto view=at(0x7100),nativeview=at(0x7200),graphics=at(0x7300),render=at(0x7400),handle=at(0x7500),set=at(0x7600);
  const auto depth=at(0x7800),blend=at(0x7a00),raster=at(0x7d00),packet=at(0x8000);
  expected_executor=executor;expected_context=context;state_at=state;attachment_at=attachment;
  put(image.p+0x51c73d0,renderer);put(renderer+0x2480,at(0x100));put(at(0x100),depth);
  put(executor+8,state);put(state+0x20,at(0x9000));put(attachment,view);put(attachment+0x48,1U);
  put(attachment+0x4c,std::array<int,4>{0,0,8,8});put(view+0x60,nativeview);
  put(nativeview+0x10,std::uintptr_t{999});put(nativeview+0x18,reinterpret_cast<std::uintptr_t>(resource.Get()));put(nativeview+0x20,static_cast<unsigned>(desc.Format));
  put(graphics+8,handle);put(graphics+0x18,7U);put(handle,set);put(render+8,blend);put(render+0x10,depth);put(render+0x18,raster);
  const auto layout=at(0x8300),elements=at(0x8400);put(graphics+0x20,layout);put(layout+0x10,elements);put(layout+0x18,1U);
  elements_at=elements;
  D3D12_INPUT_ELEMENT_DESC element{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0};
  put(elements,element);
  if(mode=="layout_budget")put(layout+0x18,33U);
  if(mode=="layout_unreadable")put(layout+0x10,std::uintptr_t{1});
  if(mode=="extra_stage")put(set+0x68,std::uintptr_t{1});
  put(depth+0x60,depth+0x100);put(blend+0x98,blend+0x100);put(raster+0x68,raster+0x100);
  put(packet+0x10,attachment);put(packet+0x18,at(0x8100));put(packet+0x20,graphics);put(packet+0x28,render);put(packet+0x40,3U);
  ComPtr<ID3DBlob> vs,ps,error;
  const char vh[]="float4 main(uint id:SV_VertexID):SV_Position{return float4(id,0,0,1);}";
  const char ph[]="float4 main(float4 p:SV_Position):SV_Target{return float4(p.xy,0,1);}";
  check(SUCCEEDED(D3DCompile(vh,sizeof(vh),nullptr,nullptr,nullptr,"main","vs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vs,&error)),"VS");
  check(SUCCEEDED(D3DCompile(ph,sizeof(ph),nullptr,nullptr,nullptr,"main","ps_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&ps,&error)),"PS");
  for(unsigned i=0;i<2;++i){const auto entry=at(0xa000+0x100*i),descriptor=entry+0x40;auto *blob=i?ps.Get():vs.Get();
    put(set+0x50+8*i,entry);put(entry+0x10,descriptor);put(descriptor,static_cast<unsigned>(blob->GetBufferSize()));put(descriptor+8,reinterpret_cast<std::uintptr_t>(blob->GetBufferPointer()));if(i)ps_desc=descriptor;}
  tb::Config cfg{image.p,renderer,backend,reinterpret_cast<std::uintptr_t>(device.Get())};
  if(mode=="device")cfg.device+=8;
  de::fixture_start(cfg,reinterpret_cast<void *>(native_draw));
  for(unsigned i=0;i<(positive?9U:1U);++i){tb::Snapshot stamp{};stamp.frame=10+i/3;stamp.epoch=20+i/3;stamp.view=at(0xb000);stamp.context=at(0xc000);
    const int extent=i%3==2?16:8;put(attachment+0x4c,std::array<int,4>{0,0,extent,extent});
    tb::Owner out{};out.resource=reinterpret_cast<std::uintptr_t>(resource.Get());out.width=out.height=16;
    if(mode=="resource")out.resource+=8;
    if(mode=="shader_size")put(ps_desc,262145U);
    check(de::publish(i/3,i%3,stamp,packet,out),"publish");
    if(mode=="arguments")put(packet+0x40,7U);
    if(mode=="duplicate")check(!de::publish(i/3,i%3,stamp,packet,out),"duplicate rejected");
    if(mode=="ambiguous")check(de::publish(0,1,stamp,packet,out),"second candidate");
    check(invoke(packet)==(mode!="exception"),"exception preserved");
    if(positive)check(de::complete()==(i==8),"all three stages/frame required");
  }
  check(de::finish()==positive,"completion result");check(de::settled(),"callbacks drained");
  if(positive&&argc==3)check(de::save(std::wstring(argv[2],argv[2]+std::strlen(argv[2]))),"saved shaders");
  std::ostringstream log;log<<std::boolalpha;de::write(log);std::cout<<log.str()<<'\n';
  if(argc==3)std::ofstream(std::string(argv[2])+".json")<<log.str();
}
