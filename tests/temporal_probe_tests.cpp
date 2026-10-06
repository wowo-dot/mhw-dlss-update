#include "../src/temporal_bindings/probe.hpp"
#include "../src/temporal_bindings/shader.hpp"
#include "../src/temporal_bindings/consumer.hpp"
#include "../src/temporal_bindings/submission.hpp"
#include "../src/temporal_bindings/depth_routes.hpp"
#include "../src/temporal_bindings/owned_inputs.hpp"
#include "../src/temporal_gpu_inputs/contract.hpp"
#include <MinHook.h>
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <d3dcompiler.h>
#include <filesystem>
#include <thread>
#include <DirectXPackedVector.h>
namespace tb=mhw::temporal_bindings;
using Microsoft::WRL::ComPtr;
void check(bool v,const char *s){if(!v){std::cerr<<s<<'\n';std::exit(1);}}
template<class T>void put(std::uintptr_t p,T v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
template<class T>T get(std::uintptr_t p){T v;std::memcpy(&v,reinterpret_cast<void *>(p),sizeof(v));return v;}
struct Block {
  std::uintptr_t p{};
  explicit Block(std::size_t size){p=reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));check(p!=0,"allocate");}
  ~Block(){VirtualFree(reinterpret_cast<void *>(p),0,MEM_RELEASE);}
};
std::string mode;unsigned forwards{};tb::Config cfg;std::uintptr_t array_at{},packet_at{},state_at{},layer_at{};
bool projection_owned{};
bool projection_admission(std::uintptr_t context,std::uintptr_t view) noexcept {
  return projection_owned && context==cfg.renderer+0x710 && view==get<std::uintptr_t>(cfg.renderer+0x1f450);
}
std::uintptr_t shader_at{},words_at{};
ID3D12GraphicsCommandList *native_list{};
ID3D12CommandAllocator *native_allocator{};
std::string fixture_output;
void traffic_during_unhook(){
  std::ostringstream before,after;before<<std::boolalpha;tb::submission::write(before);
  for(unsigned i=0;i<tb::submission::event_limit;++i){
    check(SUCCEEDED(tb::submission::fixture_close(native_list)),"teardown Close still forwards");
    check(SUCCEEDED(tb::submission::fixture_reset(native_list,native_allocator)),"teardown Reset still forwards");
  }
  after<<std::boolalpha;tb::submission::write(after);
  if(!fixture_output.empty()){
    std::ofstream(fixture_output+".traffic-before.json")<<before.str();
    std::ofstream(fixture_output+".traffic-after.json")<<after.str();
  }
  check(before.str()==after.str(),"journal remains unchanged across 8192 teardown callbacks");
}
std::uintptr_t executor_at{},consumer_context{},recording_at{},native_resource{};
unsigned consumer_forwards{},barrier_forwards{},dispatch_forwards{};
bool journal_inline_fixture{};ComPtr<ID3D12Fence> inline_completed;
bool motion_mode{},pair_mode{};unsigned group_x=160,group_y=90;
ID3D12Resource *color_resource{};
ID3D12Resource *depth_resource{};std::uintptr_t depth_owner{},depth_container{};
std::uint32_t pattern(unsigned x,unsigned y,unsigned frame) {
  return ((0x3800U+((x+y*17+frame*17)&0x3ff))<<16) | (0xb000U+((x*3+y+frame*29)&0x3fe)) | ((x+y)&1);
}
std::uint32_t color_pattern(unsigned x,unsigned y,unsigned frame) {
  // Dyadic values stay exactly representable in unsigned 11/11/10-bit floats.
  DirectX::PackedVector::XMFLOAT3PK p((x+frame*2)*0.125f,(y+frame*3)*0.25f,(x+y+frame)*0.5f);return p.v;
}
#include "temporal_jitter_fixture.hpp"
#include "temporal_depth_routes_fixture.hpp"
#ifdef MHW_TEMPORAL_SCALED
#include "temporal_color_admission_fixture.hpp"
#include "temporal_handoff_fixture.hpp"
#endif
#ifdef MHW_TEMPORAL_PRESENT_STATE
#include "temporal_bindings_fixture.hpp"
#endif
void WINAPI mock_barrier(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *barriers){
  ++barrier_forwards;check(list==native_list && n>0 && barriers,"barrier forwarding");
  if(mode=="consumer_barrier_change")const_cast<D3D12_RESOURCE_BARRIER *>(barriers)[0].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
  if(motion_mode)list->ResourceBarrier(n,barriers);
}
void WINAPI mock_dispatch(ID3D12GraphicsCommandList *list,unsigned,unsigned,unsigned){
  ++dispatch_forwards;check(list==native_list,"dispatch forwarding");if(mode=="consumer_api_exception")RaiseException(0xe0424242,0,0,nullptr);
#ifdef MHW_TEMPORAL_PRESENT_STATE
  if(journal_inline_fixture && dispatch_forwards==5){
    mhw::temporal_present::Bindings state;tb::jitter::HistorySample sample;
    check(mhw::temporal_present::observer::sample(4,state)&&tb::jitter::current_history_sample(4,sample),"inline frame published before capture closure");
    check(!tb::submission::inline_begin(4,sample.frame+1,list,state.generation),"inline wrong frame");
    check(!tb::submission::inline_begin(4,sample.frame,list,state.generation+1),"inline wrong generation");
    check(!tb::submission::inline_begin(4,sample.frame,nullptr,state.generation),"inline wrong list");
    check(tb::submission::inline_begin(4,sample.frame,list,state.generation),"inline matching copies and frame");
    tb::submission::inline_end(4,true);
    check(!tb::submission::inline_begin(4,sample.frame,list,state.generation),"inline duplicate");
  }
#endif
}
std::uint64_t mock_consumer(std::uintptr_t executor,std::uintptr_t context,std::uintptr_t packet){
  ++consumer_forwards;check(executor==executor_at && context==consumer_context,"consumer forwarding");put(recording_at+0x158,packet);
  if(mode=="depth_reuse_active" && consumer_forwards==1)
    check(!tb::gpu::fixture_republish(0,1,6,6),"active packet generation rejected");
  // Model a duplicate CPU callback without recording a second invalid WARP
  // state transition. The recorder must still forward the original call.
  if(mode=="depth_reuse_duplicate" && consumer_forwards==6)return 0xfedcba9876543210ULL;
  if(mode=="consumer_exception")RaiseException(0xe0424242,0,0,nullptr);
  if(mode=="consumer_reentrant" && consumer_forwards==1){tb::NativeFrame f{};f.call.gpr[1]=executor;f.call.gpr[2]=context;f.call.gpr[3]=packet;tb::gpu::consume(&f);}
  #ifdef MHW_TEMPORAL_SCALED
  if(admission_fixture)color_admission_fixture->enter(packet);
  #endif
  if(mode!="consumer_unknown") {
    std::array<D3D12_RESOURCE_BARRIER,129> barriers{};auto &b=barriers[0];b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource=reinterpret_cast<ID3D12Resource *>(native_resource);b.Transition.Subresource=UINT_MAX;
    b.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;b.Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if(motion_mode && mode!="motion_wrong_state")b.Transition.StateAfter=static_cast<D3D12_RESOURCE_STATES>(0xc0);
    if(mode=="consumer_split")b.Flags=D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
    if(mode=="consumer_alias"){b.Type=D3D12_RESOURCE_BARRIER_TYPE_ALIASING;b.Aliasing.pResourceBefore=nullptr;b.Aliasing.pResourceAfter=nullptr;}
    tb::gpu::fixture_barrier(native_list,mode=="consumer_overflow"?129:1,barriers.data());
#ifdef MHW_TEMPORAL_SCALED
    if(admission_fixture){D3D12_RESOURCE_BARRIER u{};u.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;u.UAV.pResource=color_admission_fixture->output.Get();tb::gpu::fixture_barrier(native_list,1,&u);}
    else
#endif
    if(pair_mode){b.Transition.pResource=color_resource;b.Transition.StateAfter=mode=="pair_wrong_state"?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
      tb::gpu::fixture_barrier(native_list,1,&b);}
    if(mode=="scene_state"){b.Transition.pResource=depth_resource;b.Transition.StateBefore=D3D12_RESOURCE_STATE_DEPTH_WRITE;
      b.Transition.StateAfter=static_cast<D3D12_RESOURCE_STATES>(0x60);tb::gpu::fixture_barrier(native_list,1,&b);}
    if(mode=="consumer_conflict"){b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;tb::gpu::fixture_barrier(native_list,1,barriers.data());}
  }
  if(mode=="consumer_changed")put(words_at,get<unsigned>(words_at)^0x01000000U);
  if(mode=="scene_dispatch_changed")put(depth_container+0xa0,depth_owner+256);
#ifdef MHW_TEMPORAL_PRESENT_STATE
  native_bindings_fixture->bind(native_list,cfg.backend,recording_at);
#endif
  if(mode!="consumer_no_dispatch")tb::gpu::fixture_dispatch(native_list,mode=="consumer_wrong_groups"?159:group_x,group_y,1,cfg.image+0x259cfca+(mode=="consumer_wrong_call"?1:0));
#ifdef MHW_TEMPORAL_SCALED
  if(admission_fixture)color_admission_fixture->leave();
#endif
  return 0xfedcba9876543210ULL;
}
extern "C" std::uint64_t temporal_consumer_entry(std::uintptr_t,std::uintptr_t,std::uintptr_t);
bool invoke_consumer(tb::NativeFrame *f){
  __try{f->call.gpr[0]=temporal_consumer_entry(f->call.gpr[1],f->call.gpr[2],f->call.gpr[3]);return true;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}
}
std::uint64_t original(std::uintptr_t context,unsigned x,unsigned y,unsigned z,std::uintptr_t indirect,std::uintptr_t flag) {
  ++forwards;check(context==cfg.renderer+0x710 && x==group_x && y==group_y && z==1 && !indirect && !static_cast<unsigned char>(flag),"native arguments");
  if(mode=="exception")RaiseException(0xe0424242,0,0,nullptr);
  if(mode=="slow")Sleep(2010);
  if(mode=="reentrant" && forwards==1){tb::NativeFrame f{};f.layer=layer_at;f.call.caller=cfg.image+tb::caller_rva;
    f.call.gpr[1]=context;f.call.gpr[2]=x;f.call.gpr[3]=y;f.call.gpr[4]=z;tb::dispatch(&f);}
  const auto n=get<unsigned>(context+0x2b80);const auto p=packet_at+packet_stride*n;
  put(p,mode=="opcode"?10U:6U);put(p+0x10,state_at);put(p+0x18,x);put(p+0x1c,y);put(p+0x20,z);
  put(p+0x24,static_cast<unsigned char>(0));put(p+0x28,std::uintptr_t{0});put(array_at+16*n+8,p);put(context+0x2b80,n+1);
  if(mode=="changed")put(cfg.renderer+0x1f208,get<std::uint64_t>(cfg.renderer+0x1f208)+1);
  if(mode=="shader_changed")put(shader_at+4,get<unsigned>(shader_at+4)^1U);
  if(mode=="recipe_changed")put(words_at,get<unsigned>(words_at)^0x01000000U);
  if(mode=="scene_producer_changed")put(depth_container+0xa0,depth_owner+256);
  return 0x123456789abcdef0ULL;
}
bool invoke(tb::NativeFrame *f) {
  __try{tb::dispatch(f);return true;}__except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return false;}
}
int main(int argc,char **argv){
  check(argc==2 || argc==3,"case");mode=argv[1];
  if(argc==3)fixture_output=argv[2];
  journal_inline_fixture=mode=="jowned_inline";if(journal_inline_fixture)mode="jowned_success";
#ifdef MHW_TEMPORAL_SCALED
  const bool with_handoff=mode.starts_with("hdraw_");if(with_handoff)mode="j"+mode.substr(1);
  const bool scaled_jitter=mode.starts_with("jdraw_");if(scaled_jitter)mode=mode.substr(1);
  if(mode.starts_with("draw_")){draw_fixture=true;
    admission_fixture=mode.starts_with("draw_admission");admission_stale=mode=="draw_admission_stale";admission_alias=mode=="draw_admission_alias";
    if(admission_fixture && !admission_stale && !admission_alias)mode=mode=="draw_admission_multi"?"draw_pixel_multi":"draw_pixel";
    pixel_draw_fixture=admission_fixture || mode=="draw_pixel" || mode=="draw_pixel_multi";
    if(pixel_draw_fixture && !admission_stale && !admission_alias)mode=mode=="draw_pixel_multi"?"draw_multi":"draw_success";
    const bool standard=mode=="draw_success" || mode=="draw_multi" || mode=="draw_no_retire" || mode=="draw_signal_failure";
    mode="owned_"+(standard?mode.substr(5):mode);}
#else
  const bool scaled_jitter=false;
#endif
  const bool jitter_owned=mode.starts_with("jowned_");if(jitter_owned)mode="owned_"+mode.substr(7);
  const bool with_owned=mode.starts_with("owned_");if(with_owned)mode="depth_"+mode.substr(6);
  const bool with_jitter=jitter_owned || scaled_jitter || mode.starts_with("jpair_");
  const bool with_pixels=mode.starts_with("depth_");depth_pixels_fixture=with_pixels;
  const bool with_routes=mode.starts_with("route_") || with_pixels;if(with_routes)packet_stride=128;
#ifdef MHW_TEMPORAL_SCALED
  if(draw_fixture){packet_stride=256;group_x=1;}
#endif
  const bool with_tail=mode.starts_with("depth_cancel_") || mode=="depth_marker_closed";
  const bool threaded_tail=with_tail && mode!="depth_cancel_marker" && mode!="depth_marker_closed";
  const bool with_scene=mode.starts_with("scene_") || with_routes;
  pair_mode=mode.starts_with("pair_") || with_jitter || with_scene;motion_mode=mode.starts_with("motion_") || pair_mode;if(motion_mode){group_x=2;group_y=1;}
#ifdef MHW_TEMPORAL_SCALED
  if(draw_fixture)group_x=1;
#endif
  const bool observe_submission=mode.starts_with("submission_") || motion_mode;
  const bool real_hooks=mode=="submission_hooks" || mode=="motion_hooks" || mode=="pair_hooks";
  const bool observe_consumer=mode.starts_with("consumer_") || observe_submission;
  const bool positive=mode=="success" || mode=="consumer_success" || mode=="consumer_unknown" || mode=="consumer_split" || mode=="consumer_alias" || mode=="consumer_conflict" ||
    mode=="submission_success" || real_hooks || mode=="submission_two_queues" || mode=="motion_success" || mode=="motion_map_failure" || mode=="pair_success" || mode=="pair_map_failure" || mode=="jpair_success" || mode=="jpair_delay" ||
    mode=="scene_success" || mode=="scene_state" || mode=="scene_no_attachment" || mode=="route_success" || mode=="route_multi" || mode=="depth_success" || mode=="depth_multi" || mode=="depth_map_failure" || mode=="depth_reuse" || mode=="depth_cancel_marker" || mode=="depth_cancel_drain" || mode=="depth_shutdown_traffic" || mode=="depth_cancel_traffic" || mode=="depth_shutdown_pending";
  ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> warp;ComPtr<ID3D12Device> device;
  ComPtr<ID3D12Debug> debug;
  if(with_owned && positive){
    check(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))),"owned debug layer");debug->EnableDebugLayer();
    ComPtr<ID3D12Debug1> validation;check(SUCCEEDED(debug.As(&validation)),"owned GPU validation");validation->SetEnableGPUBasedValidation(TRUE);
  }
  check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) && SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) &&
    SUCCEEDED(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"WARP");
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;heap.CreationNodeMask=heap.VisibleNodeMask=1;
  D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=64;d.Height=64;d.DepthOrArraySize=1;d.MipLevels=1;
  d.Format=DXGI_FORMAT_R16G16_FLOAT;d.SampleDesc.Count=1;ID3D12Resource *resource{};
  if(motion_mode){d.Width=17;d.Height=13;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if(mode!="motion_wrong_format")d.Format=DXGI_FORMAT_R32_UINT;}
  if(motion_mode){using tb::motion::Capture;
    check(!Capture::extent(0,13) && !Capture::extent(8193,1) && !Capture::extent(4096,4096) && Capture::extent(2560,1440),"motion extent bounds");
    Capture layout;check(layout.prepare(device.Get(),17,13),"layout fixture");auto good=d;good.Format=DXGI_FORMAT_R32_UINT;
    check(layout.source_matches(good),"motion exact format");
    auto bad=good;bad.MipLevels=2;check(!layout.source_matches(bad),"motion mip rejection");bad=good;bad.SampleDesc.Count=2;
    check(!layout.source_matches(bad),"motion multisample rejection");bad=good;bad.Flags=D3D12_RESOURCE_FLAG_NONE;
    check(!layout.source_matches(bad),"motion flag rejection");layout.abort();
    Capture padded;check(Capture::extent(1023,4100) && !padded.prepare(device.Get(),1023,4100),"padded allocation bound");}
  check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,motion_mode?D3D12_RESOURCE_STATE_RENDER_TARGET:D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&resource))),"resource");
  ComPtr<ID3D12Resource> upload;D3D12_PLACED_SUBRESOURCE_FOOTPRINT upload_footprint{};
  if(motion_mode){UINT rows{};UINT64 row_bytes{},bytes{};device->GetCopyableFootprints(&d,0,1,0,&upload_footprint,&rows,&row_bytes,&bytes);
    auto hp=heap;hp.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC u{};u.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;u.Width=bytes;
    u.Height=u.DepthOrArraySize=u.MipLevels=1;u.SampleDesc.Count=1;u.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&u,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&upload))),"upload");}
  ComPtr<ID3D12DescriptorHeap> depth_heap;
  ComPtr<ID3D12Resource> color_upload;D3D12_PLACED_SUBRESOURCE_FOOTPRINT color_footprint{};
  if(pair_mode){auto c=d;c.Format=mode=="pair_wrong_format"?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R11G11B10_FLOAT;
    c.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET|D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&c,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,IID_PPV_ARGS(&color_resource))),"color resource");
    UINT rows{};UINT64 row_bytes{},bytes{};device->GetCopyableFootprints(&c,0,1,0,&color_footprint,&rows,&row_bytes,&bytes);
    auto hp=heap;hp.Type=D3D12_HEAP_TYPE_UPLOAD;D3D12_RESOURCE_DESC u{};u.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;u.Width=bytes;
    u.Height=u.DepthOrArraySize=u.MipLevels=1;u.SampleDesc.Count=1;u.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&u,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&color_upload))),"color upload");}
  if(with_scene){auto dep=d;dep.Format=DXGI_FORMAT_R32_TYPELESS;dep.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    auto initial=D3D12_RESOURCE_STATE_DEPTH_WRITE;
    if(with_routes){check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&dep,initial,nullptr,IID_PPV_ARGS(&route_source))),"original depth source");
      dep.Format=DXGI_FORMAT_R32_FLOAT;dep.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;initial=D3D12_RESOURCE_STATE_RENDER_TARGET;}
    check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&dep,initial,nullptr,IID_PPV_ARGS(&depth_resource))),"depth resource");}
  if(with_pixels){D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;hd.NumDescriptors=1;
    check(SUCCEEDED(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&depth_heap))),"depth DSV heap");
    depth_dsv=depth_heap->GetCPUDescriptorHandleForHeapStart();D3D12_DEPTH_STENCIL_VIEW_DESC v{};v.Format=DXGI_FORMAT_D32_FLOAT;v.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(route_source,&v,depth_dsv);}
  Block image(0x55d3000),renderer(0x80000),backend(0x43000),layer_memory(0x200),view_memory(0x700),gb(0x100),owners(0xa00),backing(0x200),queue(0x400),packets(0x4000);
  Block shader_state(0x1000),shader_storage(tb::shader_limit);
  Block execution(0x1000);
  ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
  check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))) &&
    SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))),"owned command list");
  native_list=list.Get();native_allocator=allocator.Get();native_resource=reinterpret_cast<std::uintptr_t>(resource);
  ComPtr<ID3D12CommandQueue> submit_queue,second_queue;ComPtr<ID3D12Fence> wait_fence;std::uint64_t wait_value{};
  D3D12_COMMAND_QUEUE_DESC queue_desc{};queue_desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
  if(observe_submission){
    check(SUCCEEDED(device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&submit_queue))) &&
      SUCCEEDED(device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&second_queue))) &&
      SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&wait_fence))),"submission fixture objects");
    check(SUCCEEDED(list->Close()),"initial close");
  }
  auto reset_list=[&](){check(SUCCEEDED(allocator->Reset()),"allocator reset");
    return real_hooks?list->Reset(allocator.Get(),nullptr):tb::submission::fixture_reset(list.Get(),allocator.Get());};
  auto close_list=[&](){return real_hooks?list->Close():tb::submission::fixture_close(list.Get());};
  auto fill_source=[&](unsigned frame){void *p{};D3D12_RANGE none{};check(SUCCEEDED(upload->Map(0,&none,&p)),"upload map");
    for(unsigned y=0;y<13;++y)for(unsigned x=0;x<17;++x)put(reinterpret_cast<std::uintptr_t>(p)+y*upload_footprint.Footprint.RowPitch+4*x,pattern(x,y,frame));
    upload->Unmap(0,nullptr);D3D12_RESOURCE_BARRIER b{};b.Transition.pResource=resource;b.Transition.Subresource=UINT_MAX;
    b.Transition.StateBefore=frame?static_cast<D3D12_RESOURCE_STATES>(0xc0):D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;list->ResourceBarrier(1,&b);
    D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=upload_footprint;
    dst.pResource=resource;dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
    b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;b.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;list->ResourceBarrier(1,&b);};
  auto execute_list=[&](ID3D12CommandQueue *q){ID3D12CommandList *batch[]={list.Get()};
    if(real_hooks)q->ExecuteCommandLists(1,batch);else tb::submission::fixture_execute(q,1,batch);
    check(SUCCEEDED(q->Signal(wait_fence.Get(),++wait_value)),"fixture wait signal");
    const auto until=GetTickCount64()+1000;
    while(wait_fence->GetCompletedValue()<wait_value && GetTickCount64()<until)Sleep(1);
    check(wait_fence->GetCompletedValue()==wait_value,"fixture GPU wait");
    if(mode=="submission_late_completion")Sleep(2010);
    if(mode!="submission_unpolled")tb::submission::poll();};
  auto fill_color=[&](unsigned frame){void *p{};D3D12_RANGE none{};check(SUCCEEDED(color_upload->Map(0,&none,&p)),"color upload map");
    for(unsigned y=0;y<13;++y)for(unsigned x=0;x<17;++x)put(reinterpret_cast<std::uintptr_t>(p)+y*color_footprint.Footprint.RowPitch+4*x,color_pattern(x,y,frame));
    color_upload->Unmap(0,nullptr);D3D12_RESOURCE_BARRIER b{};b.Transition.pResource=color_resource;b.Transition.Subresource=UINT_MAX;
    b.Transition.StateBefore=frame?D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_RENDER_TARGET;
    b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;list->ResourceBarrier(1,&b);
    D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=color_upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=color_footprint;
    dst.pResource=color_resource;dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
    b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;b.Transition.StateAfter=D3D12_RESOURCE_STATE_RENDER_TARGET;list->ResourceBarrier(1,&b);};
  executor_at=execution.p;consumer_context=execution.p+0x100;recording_at=execution.p+0x200;
  ComPtr<ID3DBlob> compiled,errors;
  constexpr char source[]="RWTexture2D<float4> target:register(u0); [numthreads(16,16,1)] void main(uint3 p:SV_DispatchThreadID){target[p.xy]=float4(p.xy,0,1);}";
  check(SUCCEEDED(D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&compiled,&errors)),"compile owned shader");
  const auto shader_size=static_cast<unsigned>(compiled->GetBufferSize());check(shader_size<=tb::shader_limit,"owned shader size");
  shader_at=shader_storage.p;std::memcpy(reinterpret_cast<void *>(shader_at),compiled->GetBufferPointer(),shader_size);
  cfg={image.p,renderer.p,backend.p,reinterpret_cast<std::uintptr_t>(device.Get())};
  put(executor_at+8,recording_at);put(consumer_context+8,cfg.backend);put(recording_at+0x20,reinterpret_cast<std::uintptr_t>(native_list));
  const auto context=renderer.p+0x710,layer=layer_memory.p,view=view_memory.p;
  array_at=queue.p;packet_at=packets.p;state_at=shader_state.p;layer_at=layer;
  const auto handle=state_at+0x100,set=state_at+0x200,entry=state_at+0x300,desc=state_at+0x400,ids=state_at+0x500,modes=state_at+0x600,views=state_at+0x800;
  words_at=state_at+0x700;
  put(state_at+8,handle);put(handle,set);put(set+0x78,entry);put(entry+0x10,desc);
  put(desc,mode=="shader_budget"?tb::shader_limit+1:shader_size);put(desc+8,mode=="unreadable_shader"?std::uintptr_t{1}:shader_at);
  put(state_at+0x18,9U);put(set+0x88,tb::roles);put(set+0x80,ids);put(state_at+0x10,modes);put(set,words_at);
  put(set+0x44,static_cast<std::uint16_t>(tb::roles));put(set+0x46,static_cast<std::uint16_t>(0));
  if(mode=="wrong_stage")for(unsigned i=0;i<get<unsigned>(shader_at+28);++i) {
    const auto at=shader_at+get<unsigned>(shader_at+32+4*i);
    if(get<unsigned>(at)==0x58454853 || get<unsigned>(at)==0x52444853)put(at+8,0x10050U);
  }
  put(image.p+0x51cc080,static_cast<unsigned char>(1));put(image.p+0x51c4480,renderer.p);put(renderer.p+0x78,backend.p);put(backend.p+0x18,cfg.device);
  put(layer,image.p+0x351bda8);put(context+0x1d0,view);put(renderer.p+0x1f450,view);put(renderer.p+0x1f210,std::uint64_t{5});
  put(renderer.p+0x1f208,std::uint64_t{50});put(context+0x158,static_cast<std::uint16_t>(9));put(context+0x1c8,state_at);
  put(context+0x1ea74,8U);put(layer+0x184,0x01010000U);put(view+0x620,gb.p);put(context+0x64,2560U);put(context+0x68,1440U);
  if(motion_mode){put(context+0x64,17U);put(context+0x68,13U);}
  put(context+0x2b78,array_at);put(backing.p+0x20,reinterpret_cast<std::uintptr_t>(resource));
  for(unsigned i=0;i<tb::roles;++i){const auto owner=owners.p+256*i;
    put(image.p+tb::id_rvas[i],10U+i);put(context+0x710+8*(10+i),owner);put(owner,image.p+0x351c870);
    put(owner+0x58,5U);put(owner+0x60,backing.p);put(owner+0x70,2U);put(owner+0x88,64U|(64U<<16));put(owner+0x90,34U);put(owner+0x94,UINT_MAX);
    if(motion_mode){put(owner+0x88,17U|(13U<<16));put(owner+0x90,38U);}
    put(ids+4*i,10U+i);put(state_at+0x30+8*i,owner);put(words_at+4*i,(i<<24)|((10+i)<<12)|(i<<4)|(i==4?3U:2U));
    const auto v=views+32*i;put(owner+(i==4?0x80:0x78),v);put(v+0x10,std::uintptr_t{0x10000+32*i});
  }
  if(mode=="bad_binding")put(words_at,(tb::roles<<4)|2U);
  if(pair_mode && mode!="pair_alias"){put(owners.p+0x60,backing.p+0x40);put(backing.p+0x60,reinterpret_cast<std::uintptr_t>(color_resource));put(owners.p+0x90,34U);}
  put(layer+0x150,owners.p);put(gb.p+0x60,owners.p+3*256);put(gb.p+0x78,owners.p+5*256);put(gb.p+0xc0,owners.p+6*256);
  if(with_scene){depth_owner=owners.p+7*256;depth_container=gb.p;
    std::memcpy(reinterpret_cast<void *>(depth_owner),reinterpret_cast<const void *>(owners.p+5*256),256);
    put(depth_owner+0x60,backing.p+0x80);put(backing.p+0xa0,reinterpret_cast<std::uintptr_t>(depth_resource));
    put(view+0x628,mode=="scene_missing"?std::uintptr_t{}:depth_container);put(depth_container+0xa0,depth_owner);
    put(context+0x50,mode=="scene_no_attachment"?std::uintptr_t{}:execution.p+0x800);put(execution.p+0x858,depth_owner);}
  if(with_routes){route_source_owner=owners.p+8*256;route_rect=execution.p+0x900;route_xy=execution.p+0x920;
    std::memcpy(reinterpret_cast<void *>(route_source_owner),reinterpret_cast<void *>(depth_owner),256);
    put(route_source_owner+0x60,backing.p+0xc0);put(backing.p+0xe0,reinterpret_cast<std::uintptr_t>(route_source));
    put(route_rect,std::array<unsigned,4>{0,0,17,13});put(route_xy,std::array<unsigned,2>{0,0});
    const auto vtable=get<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(native_list));
    dr::fixture_start(cfg,reinterpret_cast<void *>(mock_depth_producer),reinterpret_cast<void *>(mock_depth_consumer),reinterpret_cast<void *>(get<std::uintptr_t>(vtable+17*8)),scaled_jitter);}
#ifdef MHW_TEMPORAL_SCALED
  if(draw_fixture){
    setup_depth_draw(device.Get());dr::fixture_draw_start(reinterpret_cast<void *>(mock_native_draw));
    put(route_rect,std::array<unsigned,4>{0,0,8,6});put(context+0x64,8U);put(context+0x68,6U);
  }
#endif
#ifdef MHW_TEMPORAL_SCALED
  if(admission_fixture)color_admission_fixture=std::make_unique<ColorAdmissionFixture>(device.Get(),owners.p,backing.p);
#endif
  if(observe_consumer)tb::gpu::fixture_start(cfg,native_list,reinterpret_cast<void *>(mock_consumer),reinterpret_cast<void *>(mock_barrier),reinterpret_cast<void *>(mock_dispatch));
  if(observe_submission){
    if(real_hooks){check(MH_Initialize()==MH_OK,"MinHook init");std::string reason;check(tb::submission::install(cfg,reason),reason.c_str());}
    else check(tb::submission::fixture_start(cfg),"submission setup");
    if(mode=="submission_signal_failure" || mode=="pair_signal_failure" || mode=="depth_signal_failure")tb::submission::fixture_signal_failure();
  }
#ifdef MHW_TEMPORAL_PRESENT_STATE
  NativeBindingsFixture bindings_fixture(device.Get());native_bindings_fixture=&bindings_fixture;
#endif
  tb::fixture_start(cfg,reinterpret_cast<void *>(original),observe_consumer,with_jitter,with_scene,with_routes);
#ifdef MHW_TEMPORAL_SCALED
  std::unique_ptr<HandoffFixture> handoff_fixture;
  if(with_handoff)handoff_fixture=std::make_unique<HandoffFixture>(device.Get(),owners.p+4*256);
#endif
  if(mode=="submission_marker_reasons"){
    using Failure=tb::submission::MarkerFailure;Failure reason{};
    check(SUCCEEDED(list->Reset(allocator.Get(),nullptr)),"open unobserved list");
    check(tb::submission::native_copy_begin(list.Get(),reason)==UINT_MAX && reason==Failure::unknown_list,"unknown list reason");
    check(SUCCEEDED(close_list()),"register close without reset");
    check(tb::submission::native_copy_begin(list.Get(),reason)==UINT_MAX && reason==Failure::missing_reset,"missing reset reason");
    check(SUCCEEDED(reset_list()),"register initial reset");
    const auto marker=tb::submission::native_copy_begin(list.Get(),reason);
    check(marker!=UINT_MAX && reason==Failure::none,"qualified marker reason");
    check(tb::submission::native_copy_begin(list.Get(),reason)==UINT_MAX && reason==Failure::list_busy,"busy list reason");
    tb::submission::native_copy_end(marker,true);check(SUCCEEDED(close_list()),"close marked list");
    check(tb::submission::native_copy_begin(list.Get(),reason)==UINT_MAX && reason==Failure::list_closed,"closed list reason");
    tb::submission::stop();
    check(tb::submission::native_copy_begin(list.Get(),reason)==UINT_MAX && reason==Failure::admission_closed,"stopped admission reason");
    check(!tb::finish() && tb::settled() && tb::submission::healthy(),"unselected diagnostic drains without qualifying capture");
    check(resource->Release()==0,"marker diagnostic resource lifetime");
    std::cout<<"submission_marker_reasons passed\n";return 0;
  }
  std::unique_ptr<JitterFixture> jitter_fixture;
  if(with_jitter){jitter_fixture=std::make_unique<JitterFixture>(cfg);tb::jitter::fixture_start(cfg,reinterpret_cast<void *>(JitterFixture::forward),GetTickCount64(),scaled_jitter?projection_admission:nullptr);}
  if(with_owned){std::string reason;check(tb::owned::prepare(device.Get(),17,13,reason),reason.c_str());
    check(!tb::owned::convert_save(L"not-qualified"),"no owned consumption before producer completion");}
  if(motion_mode){std::string reason;check(tb::gpu::prepare_motion(17,13,reason,pair_mode),reason.c_str());
    check(!tb::gpu::save_motion(L"not-qualified"),"no map before completion");}
  if(with_pixels){std::string reason;check(dr::prepare_pixels(17,13,reason),reason.c_str());check(!dr::save_pixels(L"not-qualified"),"no depth map before completion");}
  if(observe_submission)while(!tb::submission::warmed())Sleep(1);
  if(scaled_jitter){
    for(unsigned n=0;n<2;++n){jitter_fixture->stage(3+n);auto jf=jitter_fixture->frame();tb::jitter::dispatch(&jf);}
    check(tb::jitter::healthy()&&!tb::jitter::wanted(0),"projection waits across activation installation gap");
    put(context+0x50,execution.p+0x800);put(execution.p+0x858,route_source_owner);
    for(unsigned n=0;n<96;++n){check(invoke_depth_producer(),"startup depth request forwards");put(context+0x2b80,0U);}
    std::ostringstream waiting;waiting<<std::boolalpha;dr::write(waiting);
    check(dr::healthy()&&waiting.str().find("\"requests\":[]")!=std::string::npos,"startup depth traffic cannot exhaust capture slots");
    put(context+0x50,std::uintptr_t{});
    projection_owned=true;
  }
  if(mode=="jpair_delay"){
    put(jitter_fixture->table.p+0xe6a0+8*5,std::uint64_t{std::bit_cast<unsigned>(.09375F)}|(std::uint64_t{std::bit_cast<unsigned>(.125F)}<<32));
    jitter_fixture->stage(4);auto jf=jitter_fixture->frame();tb::jitter::dispatch(&jf);
    check(!tb::jitter::wanted(0),"small lookahead deferred");
    tb::NativeFrame f{};f.layer=layer;f.call.caller=image.p+tb::caller_rva;
    f.call.gpr[1]=context;f.call.gpr[2]=group_x;f.call.gpr[3]=group_y;f.call.gpr[4]=1;
    check(invoke(&f) && f.call.gpr[0]==0x123456789abcdef0ULL,"unselected TAA forwards");
    // The owned producer queued its normal packet; model the next frame's
    // queue reset. No selected receipt or GPU copy may arise from this call.
    put(context+0x2b80,0U);
  }
  if(mode=="timeout"){Sleep(2010);check(tb::done(),"deadline");}
  const auto attempts=(positive || with_tail)?tb::capacity+1:mode=="depth_reuse_duplicate"?5U:mode=="depth_reuse_unconsumed"?3U:(mode=="submission_no_retire" || mode=="motion_no_retire" || mode=="pair_no_retire" || mode=="depth_no_retire")?tb::capacity:(mode=="duplicate" || mode=="pair_frame_skip")?2U:1U;
  for(unsigned i=0;i<attempts;++i){
    // Two alternating native queue banks recycle both the copy packet and
    // TAA packet. Each frame still gets independent captured GPU resources.
    if(mode=="depth_reuse" || mode=="depth_reuse_duplicate" || mode=="depth_reuse_unconsumed")
      put(context+0x2b80,2U*(i%2));
    if(observe_submission && i<tb::capacity){
      if(mode=="submission_no_reset")check(SUCCEEDED(list->Reset(allocator.Get(),nullptr)),"unobserved reset");
      else check(SUCCEEDED(reset_list()),"observed reset");
      if(motion_mode)fill_source(i);
      if(pair_mode)fill_color(i);
      if(mode=="scene_state" && i){D3D12_RESOURCE_BARRIER b{};b.Transition.pResource=depth_resource;b.Transition.Subresource=UINT_MAX;
        b.Transition.StateBefore=static_cast<D3D12_RESOURCE_STATES>(0x60);b.Transition.StateAfter=D3D12_RESOURCE_STATE_DEPTH_WRITE;list->ResourceBarrier(1,&b);}
    }
    const std::uint64_t frame=5+(mode=="duplicate"?0:mode=="pair_frame_skip"?2*i:i);put(renderer.p+0x1f210,frame);put(layer+0x168+8*((frame-1)%3),owners.p+256);put(layer+0x168+8*(frame%3),owners.p+4*256);
    if(with_jitter){jitter_fixture->stage(frame);auto jf=jitter_fixture->frame();tb::jitter::dispatch(&jf);
      if(mode=="jpair_mismatch")put(renderer.p+0x1f208,frame+1);}
    if(scaled_jitter&&i==0){
      put(context+0x50,execution.p+0x800);put(execution.p+0x858,route_source_owner);
      put(renderer.p+0x1f208,frame+1);
      for(unsigned n=0;n<3;++n){check(invoke_depth_producer(),"foreign epoch depth forwards");put(context+0x2b80,0U);}
      put(renderer.p+0x1f208,frame);const auto bank=get<unsigned>(renderer.p+0x1f200);put(renderer.p+0x1f200,bank^1);
      for(unsigned n=0;n<3;++n){check(invoke_depth_producer(),"foreign bank depth forwards");put(context+0x2b80,0U);}
      put(renderer.p+0x1f200,bank);put(context+0x50,std::uintptr_t{});
      std::ostringstream waiting;waiting<<std::boolalpha;dr::write(waiting);
      check(dr::healthy()&&waiting.str().find("\"requests\":[]")!=std::string::npos,"depth gate requires exact projection stamp");
    }
    std::vector<std::uintptr_t> pending_depth;
    if(with_routes && i<tb::capacity){
      for(unsigned j=0;j<(mode=="route_multi" || mode=="depth_multi"?3U:mode=="depth_duplicate"?2U:1U);++j){const auto index=get<unsigned>(context+0x2b80);
        put(context+0x50,execution.p+0x800);put(execution.p+0x858,route_source_owner);
        check(invoke_depth_producer()==(mode!="route_exception"),"native copy producer exception");
        if(mode!="route_exception"){
          const auto p=packet_at+packet_stride*index;if(mode=="route_packet_changed")put(p+0x28,1U);
          if(with_pixels && mode!="depth_no_selection")pending_depth.push_back(p);
          else check(temporal_depth_consumer_entry(executor_at,consumer_context,p)==0x1122334455667788ULL,"native copy consumer return");}
      }
      put(context+0x50,std::uintptr_t{});
    }
    const auto taa_index=get<unsigned>(context+0x2b80);
    tb::NativeFrame f{};f.layer=layer;f.call.caller=image.p+tb::caller_rva+(mode=="foreign"?1:0);
    f.call.gpr[1]=context;f.call.gpr[2]=group_x;f.call.gpr[3]=group_y;f.call.gpr[4]=1;f.call.stack[1]=0x123456789abcde00ULL;
    const auto returned=invoke(&f);check(returned==(mode!="exception"),"exception propagation");
    if(returned)check(f.call.gpr[0]==0x123456789abcdef0ULL,"original return");
#ifdef MHW_TEMPORAL_SCALED
    if(handoff_fixture && i<tb::capacity)handoff_fixture->invoke(layer,owners.p+4*256);
#endif
    for(auto p:pending_depth)check(temporal_depth_consumer_entry(executor_at,consumer_context,p)==0x1122334455667788ULL,"deferred native copy consumer return");
    if(observe_consumer && i<tb::capacity && !(mode=="depth_reuse_unconsumed" && i==0)){if(mode=="scene_consumer_changed")put(depth_container+0xa0,depth_owner+256);
      tb::NativeFrame consumer{};consumer.call.gpr[1]=executor_at;consumer.call.gpr[2]=consumer_context;consumer.call.gpr[3]=packet_at+packet_stride*(with_routes?taa_index:i);
      const bool consumed=invoke_consumer(&consumer);check(consumed==(mode!="consumer_exception" && mode!="consumer_api_exception"),"consumer exception propagation");
      if(consumed)check(consumer.call.gpr[0]==0xfedcba9876543210ULL,"consumer native return");
      if(mode=="consumer_duplicate"){consumer.call.gpr[1]=executor_at;consumer.call.gpr[2]=consumer_context;consumer.call.gpr[3]=packet_at+64*i;
        check(invoke_consumer(&consumer),"duplicate original forwarded");}
      if(mode=="depth_reuse_duplicate" && i==4){consumer.call.gpr[1]=executor_at;consumer.call.gpr[2]=consumer_context;consumer.call.gpr[3]=packet_at+packet_stride*taa_index;
        check(invoke_consumer(&consumer) && consumer.call.gpr[0]==0xfedcba9876543210ULL,"reused generation duplicate forwards once");}
      if(mode=="depth_reuse_epoch")check(!tb::gpu::fixture_republish(0,1,6,5),"same epoch reuse rejected");
      if(mode=="depth_reuse_frame")check(!tb::gpu::fixture_republish(0,1,5,6),"same frame reuse rejected");
      if(mode=="depth_reuse_overlap")check(!tb::gpu::fixture_republish(0,1,6,6,true),"producer overlap rejected after consumer returned");
    }
    if(observe_submission && i<tb::capacity){
#ifdef MHW_TEMPORAL_PRESENT_STATE
      if(journal_inline_fixture&&i==4){
        mhw::temporal_present::Bindings b;check(mhw::temporal_present::observer::sample(i,b),"followup sample");
        check(tb::submission::followup_begin(i,nullptr,b.generation)==UINT_MAX,"followup wrong list");
        check(tb::submission::followup_begin(i,list.Get(),b.generation+1)==UINT_MAX,"followup wrong generation");
        check(tb::submission::followup_begin(i-1,list.Get(),b.generation)==UINT_MAX,"followup unselected receipt");
        const auto t=tb::submission::followup_begin(i,list.Get(),b.generation);check(t!=UINT_MAX,"followup after dispatch");
        check(tb::submission::followup_begin(i,list.Get(),b.generation)==UINT_MAX,"followup busy recording");
        tb::submission::native_copy_end(t,true);
      }
#endif
      if(mode=="submission_no_close")continue;
      check(SUCCEEDED(close_list()),"observed close");
#ifdef MHW_TEMPORAL_PRESENT_STATE
      if(journal_inline_fixture&&i==4){mhw::temporal_present::Bindings b;check(mhw::temporal_present::observer::sample(i,b),"closed sample");
        check(tb::submission::followup_begin(i,list.Get(),b.generation)==UINT_MAX,"followup closed recording");}
#endif
      if(mode=="submission_double_close")check(FAILED(close_list()),"original close failure preserved");
      if(mode=="submission_early_reset"){check(SUCCEEDED(reset_list()),"early reset forwarded");continue;}
      auto q=(mode=="submission_two_queues" && i==1)?second_queue.Get():submit_queue.Get();
      execute_list(q);
      if(mode=="submission_duplicate")execute_list(q);
    }
  }
  if(observe_submission && (positive || with_tail)){check(SUCCEEDED(reset_list()),"retirement reset");
    if(!tb::submission::complete()){tb::submission::write(std::cerr);std::cerr<<'\n';}
    check(tb::submission::complete(),"submission completion and retirement");}
  if(mode=="submission_no_retire")check(!tb::submission::complete(),"completion does not imply retirement");
  std::thread tail_thread;
  if(with_tail){
    const auto index=get<unsigned>(context+0x2b80);
    put(context+0x50,execution.p+0x800);put(execution.p+0x858,route_source_owner);
    check(invoke_depth_producer(),"tail depth producer");put(context+0x50,std::uintptr_t{});
    // Stop after admission but before the submission marker is requested.
    const auto packet=packet_at+packet_stride*index;
    if(threaded_tail){
      tail_admitted=CreateEventW(nullptr,TRUE,FALSE,nullptr);tail_continue=CreateEventW(nullptr,TRUE,FALSE,nullptr);
      check(tail_admitted && tail_continue,"tail synchronization events");dr::fixture_before_marker(pause_tail_marker);
      tail_thread=std::thread([=]{check(invoke_depth_consumer(packet)==(mode!="depth_cancel_exception"),"tail exception propagation");});
      check(WaitForSingleObject(tail_admitted,2000)==WAIT_OBJECT_0,"tail admission reached");tb::cancel();
      if(mode!="depth_cancel_timeout")tb::fixture_before_drain(release_tail_at_drain);
    }else{
      dr::fixture_before_marker(mode=="depth_marker_closed"?+[]{tb::submission::stop();}:+[]{tb::cancel();});
      check(invoke_depth_consumer(packet),"cancelled tail native forwarding");dr::fixture_before_marker(nullptr);
      std::ostringstream tail;dr::write(tail);
      if(argc==3){std::ofstream audit(std::string(argv[2])+".tail.json");audit<<std::boolalpha;dr::write(audit);}
      check((tail.str().find("\"marker_index\":4294967295")==std::string::npos)==positive,"admitted tail marker survives cancellation");
      if(!positive)check(tail.str().find("\"fault\":16,")!=std::string::npos && tail.str().find("\"marker_failure\":2,")!=std::string::npos,"missing marker fails closed with reason");
    }
  }
  const bool pending_submission=mode=="depth_shutdown_pending";
  if(pending_submission){
    tail_admitted=CreateEventW(nullptr,TRUE,FALSE,nullptr);tail_continue=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    check(tail_admitted && tail_continue,"submission synchronization events");
    tb::submission::fixture_before_close_return(pause_tail_marker);
    tail_thread=std::thread([]{
      check(SUCCEEDED(tb::submission::fixture_close(native_list)),"admitted Close return forwarded");
      check(SUCCEEDED(tb::submission::fixture_reset(native_list,native_allocator)),"post-cutoff Reset forwards");
    });
    check(WaitForSingleObject(tail_admitted,2000)==WAIT_OBJECT_0,"Close paused before recorder return");
    tb::fixture_before_unhook(+[]{
      check(!tb::submission::admission_open() && !tb::submission::settled(),"admission closes while old D3D callback is still pending");
      check(SetEvent(tail_continue)!=0,"resume old D3D callback after cutoff");
    });
  }
  if(mode=="depth_shutdown_traffic" || mode=="depth_cancel_traffic")tb::fixture_before_unhook(traffic_during_unhook);
  if(journal_inline_fixture)tb::fixture_before_unhook(+[]{ID3D12Fence *completed{};
    check(tb::submission::finish(&completed,4)&&completed,"inline retain completed journal fence");inline_completed.Attach(completed);});
  bool result=tb::finish();check(result==positive,"acceptance");
  if(journal_inline_fixture){check(inline_completed&&inline_completed->GetCompletedValue()==1,"retained fence survives journal cleanup");inline_completed.Reset();}
  if(pending_submission){
    tail_thread.join();tb::submission::fixture_before_close_return(nullptr);
    check(CloseHandle(tail_admitted) && CloseHandle(tail_continue),"close submission events");
  }
  if(threaded_tail){
    if(mode=="depth_cancel_timeout"){
      check(!dr::settled(),"timed-out callback remains retained");check(SetEvent(tail_continue)!=0,"resume after bounded timeout");
    }
    tail_thread.join();dr::fixture_before_marker(nullptr);tb::fixture_before_drain(nullptr);
    check(CloseHandle(tail_admitted) && CloseHandle(tail_continue),"close tail events");
    if(mode=="depth_cancel_timeout"){result=tb::finish();check(!result,"timeout cannot become accepted after delayed return");}
  }
  check(tb::settled(),"drained");
  if(mode.starts_with("depth_reuse") && !positive){std::ostringstream audit;tb::gpu::write(audit);
    const unsigned expected=mode=="depth_reuse_duplicate"?3U:mode=="depth_reuse_frame"?17U:12U;
    check(audit.str().find("\"fault\":"+std::to_string(expected)+",")!=std::string::npos,"specific generation rejection");}
  check(forwards==attempts+(mode=="reentrant" || mode=="jpair_delay"?1:0),"forwarding count");
  if(argc==3 && result){auto path=std::filesystem::path(argv[2]);path+=".cs.dxbc";
    check(tb::save_shader(path.wstring()),"save shader");check(!tb::save_shader(path.wstring()),"no shader overwrite");}
  if(motion_mode){const auto prefix=argc==3?std::filesystem::path(argv[2]):std::filesystem::temp_directory_path()/("mhw-motion-fixture-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    if(mode=="motion_map_failure")tb::gpu::fixture_motion_map_failure();
    if(mode=="pair_map_failure")tb::gpu::fixture_color_map_failure();
    const bool saved=tb::gpu::save_motion(prefix.wstring());check(saved==(positive && mode!="motion_map_failure" && mode!="pair_map_failure"),"qualified motion save");
    if(saved){check(!tb::gpu::save_motion(prefix.wstring()),"no pixel overwrite");
      for(unsigned i=0;i<tb::capacity;++i){auto path=prefix;path+=".motion-"+std::to_string(i)+".u32";
        check(std::filesystem::file_size(path)==17*13*4,"padding removed");std::ifstream pixels(path,std::ios::binary);
        for(unsigned y=0;y<13;++y)for(unsigned x=0;x<17;++x){std::uint32_t v{};pixels.read(reinterpret_cast<char *>(&v),4);check(v==pattern(x,y,i),"exact GPU copied pixel");}
        pixels.close();if(argc!=3)std::filesystem::remove(path);}}
    if(saved && pair_mode)for(unsigned i=0;i<tb::capacity;++i){auto path=prefix;path+=".color-"+std::to_string(i)+".u32";
      check(std::filesystem::file_size(path)==17*13*4,"color padding removed");std::ifstream pixels(path,std::ios::binary);
      for(unsigned y=0;y<13;++y)for(unsigned x=0;x<17;++x){std::uint32_t v{};pixels.read(reinterpret_cast<char *>(&v),4);check(v==color_pattern(x,y,i),"exact paired GPU color");}
      pixels.close();if(argc!=3)std::filesystem::remove(path);}
    else if(mode=="motion_map_failure" && argc!=3){auto path=prefix;path+=".motion-0.u32";std::filesystem::remove(path);}
    if(mode=="pair_map_failure" && argc!=3){for(unsigned i=0;i<tb::capacity;++i){auto path=prefix;path+=".motion-"+std::to_string(i)+".u32";std::filesystem::remove(path);}
      auto path=prefix;path+=".color-0.u32";std::filesystem::remove(path);}
    if(with_pixels){if(mode=="depth_map_failure")dr::fixture_map_failure();const bool depth_saved=dr::save_pixels(prefix.wstring());
      check(depth_saved==(positive && mode!="depth_map_failure"),"qualified depth save");
      if(depth_saved){check(!dr::save_pixels(prefix.wstring()),"no depth overwrite");for(unsigned i=0;i<tb::capacity;++i){auto path=prefix;path+=".depth-"+std::to_string(i)+".f32";
        check(std::filesystem::file_size(path)==17*13*4,"depth padding removed");std::ifstream f(path,std::ios::binary);
        for(unsigned p=0;p<17*13;++p){float v{};f.read(reinterpret_cast<char *>(&v),4);float expected=.25F+(tb::capacity==3?.125F:.0625F)*i;
#ifdef MHW_TEMPORAL_SCALED
          if(draw_fixture && (p%17>=8 || p/17>=6))expected=.875F;
#endif
          check(v==expected,"exact GPU depth frame/active region");}
        f.close();if(argc!=3)std::filesystem::remove(path);}}
      if(mode=="depth_map_failure" && argc!=3){auto path=prefix;path+=".depth-0.f32";std::filesystem::remove(path);}}
    if(with_owned){
      const bool converted=tb::owned::convert_save(prefix.wstring());
      check(converted==(positive && mode!="depth_map_failure"),"owned conversion requires all three qualified/saved planes");
      if(converted){
        check(!tb::owned::convert_save(prefix.wstring()),"owned one-shot conversion");
        for(unsigned i=0;i<tb::capacity;++i)for(unsigned role=0;role<3;++role){
          constexpr std::array<const char *,3> names{"color","depth","motion"};
          constexpr std::array<unsigned,3> components{4,1,2};
          auto path=prefix;path+=".owned-"+std::to_string(i)+"."+names[role]+".f32";
          check(std::filesystem::file_size(path)==17*13*components[role]*4,"owned output extent");
          std::ifstream pixels(path,std::ios::binary);
          for(unsigned y=0;y<13;++y)for(unsigned x=0;x<17;++x){
            const auto color=mhw::temporal_gpu_inputs::color_reference(color_pattern(x,y,i));
            const auto motion=mhw::temporal_gpu_inputs::motion_reference(pattern(x,y,i),17,13);
            for(unsigned k=0;k<components[role];++k){
              float value{};pixels.read(reinterpret_cast<char *>(&value),4);
              float expected=role==0?color[k]:role==1?.25F+(tb::capacity==3?.125F:.0625F)*i:motion[k];
#ifdef MHW_TEMPORAL_SCALED
              if(draw_fixture && role==1 && (x>=8 || y>=6))expected=.875F;
#endif
              check(value==expected,"exact native GPU copy and owned conversion");
            }
          }
          pixels.close();if(argc!=3)std::filesystem::remove(path);
        }
      }
    }
    if(!positive){
      if(with_tail)check(SUCCEEDED(list->Close()),"close unsubmitted tail before discard");
      check(SUCCEEDED(list->Reset(allocator.Get(),nullptr)),"discard retired fixture recording");
    }
  }
  std::ostringstream log;log<<std::boolalpha;tb::write(log);
#ifdef MHW_TEMPORAL_SCALED
  if(admission_fixture){
    std::ostringstream proof;proof<<std::boolalpha;tb::gpu::write(proof);
    const std::string expected=positive?"\"attempted\":true,\"accepted\":true,\"packet_revalidated\":true":"\"attempted\":true,\"accepted\":false,\"packet_revalidated\":false";
    unsigned count{};for(std::size_t at=0;(at=proof.str().find(expected,at))!=std::string::npos;at+=expected.size())++count;
    check(count==(positive?tb::capacity:1U),"exact native admission receipts");
    if(!positive){check(proof.str().find("\"fault\":16,")!=std::string::npos && dispatch_forwards==1,"admission rejection forwards original dispatch");
      std::ostringstream copies;copies<<std::boolalpha;tb::gpu::write_motion(copies);tb::gpu::write_color(copies);
      check(copies.str().find("\"attempted\":true")==std::string::npos,"rejected proof issues no input copy");}
  }
#endif
  if(with_tail && positive){
    check(log.str().find("\"marker_index\":4294967295")==std::string::npos,"all admitted callbacks have markers");
    check(log.str().find("\"drained_before_disable\":false")==std::string::npos,"callbacks drained before hooks removed");
  }
  check(log.str().find(positive?("\"pins_acquired\":"+std::to_string((with_scene?8:7)*tb::capacity)+",\"pins_released\":"+std::to_string((with_scene?8:7)*tb::capacity)):"\"hooks_disabled\":true")!=std::string::npos,"release evidence");
  if(with_scene && positive)check(log.str().find(mode=="scene_state"?"\"state\":{\"known\":true,\"conflict\":false,\"value\":96}":"\"state\":{\"known\":false,\"conflict\":false,\"value\":0}")!=std::string::npos,"separate depth state");
  if(observe_consumer && positive){check(consumer_forwards==tb::capacity && dispatch_forwards==tb::capacity,"consumer forwarding counts");
    const bool known=mode=="consumer_success" || observe_submission;
    check(log.str().find(known?(motion_mode?"\"known\":true,\"conflict\":false,\"value\":192":"\"known\":true,\"conflict\":false,\"value\":64"):"\"known\":false")!=std::string::npos,"recording-local state");}
  if(motion_mode){if(positive || mode=="motion_no_retire")check(!tb::gpu::fixture_release_motion(false),"issued references retained without retirement");
    check(tb::gpu::fixture_release_motion(true),"owned fixture retirement releases pins");}
  if(with_pixels){if(positive || mode=="depth_no_retire")check(!dr::fixture_release_pixels(false),"depth ownership before retirement");check(dr::fixture_release_pixels(true),"depth fixture references released");}
  if(!observe_submission || positive || motion_mode || mode=="submission_no_close" || mode=="submission_early_reset")check(SUCCEEDED(list->Close()),"owned list close");
  const bool retained=mode=="submission_signal_failure" || mode=="submission_unpolled" || mode=="submission_late_completion" || mode=="pair_signal_failure" || mode=="depth_signal_failure";
  auto released_list=list.Detach();const auto remaining=released_list->Release();check(retained?remaining>0:remaining==0,"command list pin lifetime");
  if(observe_submission){check(log.str().find(retained?"\"retained\":true":"\"retained\":false")!=std::string::npos,"submission retention evidence");
    auto a=allocator.Detach();check(retained?a->Release()>0:a->Release()==0,"allocator pin lifetime");
    auto q=submit_queue.Detach();check(retained?q->Release()>0:q->Release()==0,"queue pin lifetime");}
  check(resource->Release()==0,"resource pin leak");
  if(color_resource)check(color_resource->Release()==0,"color resource pin leak");
#ifdef MHW_TEMPORAL_SCALED
  color_admission_fixture.reset();
  draw_pipeline.Reset();draw_root.Reset();draw_srv_heap.Reset();draw_rtv_heap.Reset();
#endif
  if(depth_resource)check(depth_resource->Release()==0,"depth resource pin leak");
  if(route_source)check(route_source->Release()==0,"route source pin leak");
  if(with_owned && positive){
    ComPtr<ID3D12InfoQueue> messages;check(SUCCEEDED(device.As(&messages)),"owned diagnostics");
    bool clean=true;unsigned fixture_clear_warnings{};
    for(UINT64 i=0;i<messages->GetNumStoredMessagesAllowedByRetrievalFilter();++i){
      SIZE_T bytes{};check(SUCCEEDED(messages->GetMessage(i,nullptr,&bytes)),"message length");
      std::vector<std::byte> storage(bytes);auto *m=reinterpret_cast<D3D12_MESSAGE *>(storage.data());
      check(SUCCEEDED(messages->GetMessage(i,m,&bytes)),"message");
      if((m->ID==D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE
#ifdef MHW_TEMPORAL_SCALED
          || (draw_fixture && m->ID==D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE)
#endif
          ) &&
         m->Severity==D3D12_MESSAGE_SEVERITY_WARNING){++fixture_clear_warnings;continue;}
      if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING){std::cerr<<m->pDescription<<'\n';clean=false;}
    }
    check(fixture_clear_warnings==((mode=="depth_multi"?3U:1U)*tb::capacity+(with_tail?1U:0U)
#ifdef MHW_TEMPORAL_SCALED
      +(draw_fixture?(mode=="depth_multi"?3U:1U)*tb::capacity:0U)
#endif
    ),"only expected fixture clear performance warnings");
    check(clean,"owned D3D12/GPU validation clean");
    std::cout<<"GPU validation clean; "<<fixture_clear_warnings<<" expected fixture clear-value performance warnings\n";
  }
  if(argc==3){std::ofstream saved(argv[2],std::ios::binary);saved<<log.str();check(static_cast<bool>(saved),"fixture output");}
  std::cout<<mode<<" passed\n";
}
