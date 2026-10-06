#include "provenance.hpp"
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sstream>
#include <thread>
#include <vector>
namespace pb=mhw::projection_binding;
namespace pv=pb::provenance;
using U=std::uintptr_t;using F=U(*)(U,U,U,U,U,U,U);
extern "C" void mhw_binding_forward(void *,pb::Frame *);
using Microsoft::WRL::ComPtr;
#define DECL(n) extern "C" U binding_fixture##n(U,U,U,U,U,U,U);
DECL(0) DECL(1) DECL(2) DECL(3) DECL(4) DECL(5) DECL(6) DECL(7) DECL(8) DECL(9) DECL(10) DECL(11) DECL(12)
DECL(13) DECL(14) DECL(15) DECL(16) DECL(17) DECL(18) DECL(19) DECL(20) DECL(21) DECL(22) DECL(23) DECL(24) DECL(25)
DECL(26) DECL(27)
namespace {
std::string mode;
std::string readback_report;
ComPtr<ID3D12Device> device;
ComPtr<ID3D12DescriptorHeap> heap;
ComPtr<ID3D12Resource> texture;
ComPtr<ID3D12CommandAllocator> allocator;
ComPtr<ID3D12GraphicsCommandList> command;
ComPtr<ID3D12CommandQueue> queue;
ComPtr<ID3D12Fence> completion_fence;
std::vector<ComPtr<ID3D12Resource>> copy_owned;
D3D12_PLACED_SUBRESOURCE_FOOTPRINT copied_footprint{};
std::array<std::byte,0x2000> backend{};
std::array<std::byte,0x40> backing{};
std::array<std::array<std::byte,0x40>,257> objects{};
std::array<unsigned,5> recipe={2,0,1,39,0};
std::atomic<unsigned> constructed{},destroyed{};
std::atomic<bool> waiting{},unblock{};
U image{};pv::Config config;pb::Runtime runtime;
std::array<std::byte,0x22000> frame_renderer{};
std::array<std::byte,0x100> frame_source{},frame_destination{};
std::array<std::byte,0x60> frame_view{},frame_backing{},frame_destination_backing{},frame_packet{},frame_executor{},frame_state{},frame_context{};
std::array<U,2048> frame_queue{};
std::array<unsigned,4> frame_rect={0,0,16,16};std::array<unsigned,2> frame_offset{};
void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
template<class T>void put(U p,unsigned off,T value){std::memcpy(reinterpret_cast<void *>(p+off),&value,sizeof(value));}
template<class T>U addr(T &v){return reinterpret_cast<U>(v.data());}
U method(IUnknown *p,unsigned slot){return (*reinterpret_cast<U **>(p))[slot];}
U construct(unsigned index=0) {
  put(addr(objects[index]),0x10,config.heap_start+index*config.stride);
  return binding_fixture24(addr(objects[index]),addr(backing),addr(recipe),0,0,0,0);
}
void destroy(unsigned index=0){binding_fixture25(addr(objects[index]),1,0,0,0,0,0);}
bool exception_call() {
  __try{construct();}
  __except(GetExceptionCode()==0xe0123456?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
bool frame_exception_call() {
  __try{binding_fixture27(addr(frame_executor),addr(frame_context),addr(frame_packet),0,0,0,0);}
  __except(GetExceptionCode()==0xe0123457?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
  return false;
}
void setup() {
  check(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"create SDK device");
  D3D12_DESCRIPTOR_HEAP_DESC h{};h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;h.NumDescriptors=1024;
  check(SUCCEEDED(device->CreateDescriptorHeap(&h,IID_PPV_ARGS(&heap))),"create SDK heap");
  D3D12_RESOURCE_DESC r{};r.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;r.Width=16;r.Height=16;r.DepthOrArraySize=1;
  r.MipLevels=1;r.Format=DXGI_FORMAT_R32_TYPELESS;r.SampleDesc.Count=1;r.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
  check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&r,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&texture))),"create SDK texture");
  check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))) &&
    SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&command))) && SUCCEEDED(command->Close()),"SDK list");
  D3D12_COMMAND_QUEUE_DESC q{};check(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))),"SDK queue");
  check(SUCCEEDED(device->CreateFence(0,mode=="sync_shared"?D3D12_FENCE_FLAG_SHARED:D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&completion_fence))),"SDK fence");
  image=reinterpret_cast<U>(VirtualAlloc(nullptr,0x55d3000,MEM_RESERVE,PAGE_NOACCESS));check(image!=0,"reserve image");
  check(VirtualAlloc(reinterpret_cast<void *>(image+0x51c7000),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"image page");
  config.image=image;config.backend=addr(backend);config.device=reinterpret_cast<U>(device.Get());config.heap=reinterpret_cast<U>(heap.Get());
  config.heap_start=heap->GetCPUDescriptorHandleForHeapStart().ptr;config.heap_count=1024;
  config.stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  put(image,0x51c7050,config.backend);put(addr(backend),0x18,config.device);put(addr(backend),0x188,config.heap);
  put(addr(backend),0x1a8,config.heap_start);put(addr(backend),0x1dc,config.stride);put(addr(backing),0x20,reinterpret_cast<U>(texture.Get()));
  config.targets={reinterpret_cast<U>(&binding_fixture24),reinterpret_cast<U>(&binding_fixture25),method(device.Get(),21),
    method(device.Get(),23),method(device.Get(),24),method(command.Get(),9),method(command.Get(),47),method(command.Get(),27),method(queue.Get(),10),
    method(queue.Get(),14),method(queue.Get(),15),method(completion_fence.Get(),8),method(completion_fence.Get(),10),
    method(command.Get(),16),method(command.Get(),17)};
  if(mode.rfind("readback_frame_",0)==0 || mode.rfind("readback_aligned_",0)==0 || mode.rfind("readback_triplet_",0)==0 || mode.rfind("readback_sequence_",0)==0) {
    check(VirtualAlloc(reinterpret_cast<void *>(image+0x51c4000),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"frame image page");
    config.frame_renderer=addr(frame_renderer);put(image,0x51c4480,config.frame_renderer);
    config.sequence_readback=mode.rfind("readback_sequence_",0)==0;
    config.triplet_readback=mode.rfind("readback_triplet_",0)==0 || config.sequence_readback;
    config.same_frame_readback=mode.rfind("readback_aligned_",0)==0 || config.triplet_readback;
    config.targets[15]=reinterpret_cast<U>(&binding_fixture26);config.targets[16]=reinterpret_cast<U>(&binding_fixture27);
    put(config.frame_renderer,0x1f210,UINT64{101});put(config.frame_renderer,0x1f208,UINT64{91});
    const auto context=config.frame_renderer+0x710;
    put(context,0x2b78,addr(frame_queue));put(context,0x50,addr(frame_view));put(addr(frame_view),0x58,addr(frame_source));
    put(addr(frame_source),0x60,addr(frame_backing));put(addr(frame_backing),0x20,reinterpret_cast<U>(texture.Get()));
    put(addr(frame_destination),0x60,addr(frame_destination_backing));
    put(addr(frame_executor),8,addr(frame_state));put(addr(frame_state),0x20,reinterpret_cast<U>(command.Get()));
    put(addr(frame_context),8,config.backend);
  }
  std::array<F,24> f={binding_fixture0,binding_fixture1,binding_fixture2,binding_fixture3,binding_fixture4,binding_fixture5,
    binding_fixture6,binding_fixture7,binding_fixture8,binding_fixture9,binding_fixture10,binding_fixture11,binding_fixture12,
    binding_fixture13,binding_fixture14,binding_fixture15,binding_fixture16,binding_fixture17,binding_fixture18,binding_fixture19,
    binding_fixture20,binding_fixture21,binding_fixture22,binding_fixture23};
  std::array<U,pb::kind_count> targets{};for(unsigned i=0;i<24;++i)targets[i]=reinterpret_cast<U>(f[i]);
  targets[pb::reset]=method(command.Get(),10);targets[pb::barriers]=method(command.Get(),26);
  runtime.backend=config.backend;runtime.device=config.device;
  check(pb::install_owned(image,runtime,targets),"reserve and prepare core");
}
void transition(D3D12_RESOURCE_BARRIER_FLAGS flags=D3D12_RESOURCE_BARRIER_FLAG_NONE) {
  D3D12_RESOURCE_BARRIER b{};b.Flags=flags;b.Transition.pResource=texture.Get();b.Transition.Subresource=UINT_MAX;
  b.Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;b.Transition.StateAfter=D3D12_RESOURCE_STATE_DEPTH_WRITE;
  command->ResourceBarrier(1,&b);
}
void mark(ID3D12GraphicsCommandList *selected=nullptr) {
  pb::Draw d{};auto &v=d.attachments.ready.views[8];v.backend=addr(objects[0]);v.handle=config.heap_start;v.resource=reinterpret_cast<U>(texture.Get());
  d.attachments.resource_count=1;auto &r=d.attachments.resources[0];r.resource=v.resource;
  ComPtr<IUnknown> identity;check(SUCCEEDED(texture.As(&identity)),"canonical identity");r.identity=reinterpret_cast<U>(identity.Get());
  d.command=reinterpret_cast<U>(selected?selected:command.Get());pv::mark_draw(d);
}
void aligned_readback_case(const std::filesystem::path &path) {
  construct();std::string why;runtime.width=16;runtime.height=16;
  check(pv::prepare_readback(runtime) && pb::arm(why),"aligned prepare/arm");
  pb::Source selected{};selected.valid=selected.projection.valid=true;selected.order=1;selected.thread=GetCurrentThreadId();
  selected.anchor.renderer=config.frame_renderer;selected.anchor.context=config.frame_renderer+0x710;
  selected.anchor.frame=101;selected.anchor.epoch=91;selected.projection.constants=addr(backing);selected.projection.buffer_id=11;
  if(mode=="readback_aligned_source_changed")put(config.frame_renderer,0x1f210,UINT64{102});
  if(mode!="readback_aligned_no_source")check(pv::select_source(selected)==(mode!="readback_aligned_source_changed"),"source selection");
  if(mode=="readback_aligned_wrong_frame")put(config.frame_renderer,0x1f210,UINT64{102});
  ComPtr<ID3D12CommandAllocator> draw_allocator;ComPtr<ID3D12GraphicsCommandList> draw;
  check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&draw_allocator))) &&
    SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,draw_allocator.Get(),nullptr,IID_PPV_ARGS(&draw))) &&
    SUCCEEDED(draw->Close()),"aligned draw list");
  auto desc=texture->GetDesc();desc.Format=DXGI_FORMAT_R32_FLOAT;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> dst;
  check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,static_cast<D3D12_RESOURCE_STATES>(0xc0),nullptr,IID_PPV_ARGS(&dst))),"aligned destination");
  // Record the copy first; its GPU execution still follows the selected clear.
  check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"aligned copy Reset");
  D3D12_RESOURCE_BARRIER b[2]{};b[0].Transition.pResource=texture.Get();b[0].Transition.Subresource=UINT_MAX;
  b[0].Transition.StateBefore=D3D12_RESOURCE_STATE_DEPTH_WRITE;b[0].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
  b[1].Transition.pResource=dst.Get();b[1].Transition.Subresource=UINT_MAX;
  b[1].Transition.StateBefore=static_cast<D3D12_RESOURCE_STATES>(0xc0);b[1].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
  command->ResourceBarrier(2,b);put(addr(frame_destination_backing),0x20,reinterpret_cast<U>(dst.Get()));
  binding_fixture26(config.frame_renderer+0x710,addr(frame_rect),addr(frame_offset),addr(frame_destination),8,0,0);
  binding_fixture27(addr(frame_executor),addr(frame_context),addr(frame_packet),0,0,0,0);
  check(SUCCEEDED(command->Close()),"aligned copy Close");
  check(SUCCEEDED(draw->Reset(draw_allocator.Get(),nullptr)),"aligned draw Reset");
  b[0].Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;b[0].Transition.StateAfter=D3D12_RESOURCE_STATE_DEPTH_WRITE;
  draw->ResourceBarrier(1,b);draw->ClearDepthStencilView({config.heap_start},D3D12_CLEAR_FLAG_DEPTH,.375F,0,0,nullptr);
  if(pv::healthy()){pv::mark_binding(false);pv::mark_binding(true);mark(draw.Get());pv::end_frame();}
  check(SUCCEEDED(draw->Close()),"aligned draw Close");
  if(mode=="readback_aligned_view_changed")put(addr(objects[0]),0x18,reinterpret_cast<U>(texture.Get())+16);
  ID3D12CommandList *batch[]={draw.Get(),command.Get()};
  if(mode=="readback_aligned_separate"){queue->ExecuteCommandLists(1,batch);queue->ExecuteCommandLists(1,batch+1);}
  else queue->ExecuteCommandLists(2,batch);
  check(SUCCEEDED(queue->Signal(completion_fence.Get(),1)),"aligned native fence");
  const auto deadline=GetTickCount64()+5000;
  while(completion_fence->GetCompletedValue()<1 && GetTickCount64()<deadline){pv::poll_watch();Sleep(1);}
  check(completion_fence->GetCompletedValue()==1,"aligned GPU completion");
  check(SUCCEEDED(allocator->Reset()) && SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"aligned recording retirement");
  pv::poll_watch();const bool accepted=mode=="readback_aligned_pixels" || mode=="readback_aligned_separate";
  if(accepted)check(pv::healthy() && !pv::awaiting_submission(),"aligned completion stop");
  pb::stop();pv::stop();check(pb::disable() && pv::disable(),"aligned hook cleanup");
  const bool saved=pv::save_readback(path.wstring());check(saved==accepted,"aligned saved result");
  if(saved) {
    check(std::filesystem::file_size(path)==1024,"aligned pixel extent");std::array<float,256> pixels{};
    std::ifstream input(path,std::ios::binary);input.read(reinterpret_cast<char *>(pixels.data()),sizeof(pixels));
    check(static_cast<bool>(input),"aligned pixel read");for(float p:pixels)check(p==.375F,"aligned GPU ordered pixels");
  }
  std::ostringstream report;pv::write_readback(report);readback_report=report.str();
  if(mode!="readback_aligned_view_changed")check(pv::owned_release_readback(true),"aligned private refs");
}
#include "projection_triplet_fixture.inl"
void native_copy() {
  auto desc=texture->GetDesc();D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes{};
  const bool buffer=mode=="gpu_copy_footprint";
  if(buffer) {
    device->GetCopyableFootprints(&desc,0,1,512,&footprint,nullptr,nullptr,&bytes);
    copied_footprint=footprint;
    desc={};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=bytes+512;desc.Height=1;
    desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    hp.Type=D3D12_HEAP_TYPE_READBACK;
  }
  ComPtr<ID3D12Resource> dst;
  check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&dst))),"copy destination");
  D3D12_RESOURCE_BARRIER b{};b.Transition.pResource=texture.Get();b.Transition.Subresource=UINT_MAX;
  b.Transition.StateBefore=D3D12_RESOURCE_STATE_DEPTH_WRITE;b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
  command->ResourceBarrier(1,&b);
  if(mode=="gpu_copy_whole")command->CopyResource(dst.Get(),texture.Get());
  else {
    D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=texture.Get();destination.pResource=dst.Get();
    if(buffer){destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=footprint;}
    command->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
  }
  std::swap(b.Transition.StateBefore,b.Transition.StateAfter);command->ResourceBarrier(1,&b);
  copy_owned.push_back(std::move(dst));
}
void readback_case(const std::filesystem::path &path) {
  construct();std::string why;runtime.width=16;runtime.height=16;
  check(pv::prepare_readback(runtime),"readback allocation before arm");
  check(pb::arm(why),why.c_str());
  check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"readback Reset");
  transition();command->ClearDepthStencilView({config.heap_start},D3D12_CLEAR_FLAG_DEPTH,.375F,0,0,nullptr);
  pv::mark_binding(false);pv::mark_binding(true);mark();pv::end_frame();
  auto desc=texture->GetDesc();desc.Format=DXGI_FORMAT_R32_FLOAT;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
  D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> dst;
  const auto shader=static_cast<D3D12_RESOURCE_STATES>(0xc0);
  check(SUCCEEDED(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&desc,shader,nullptr,IID_PPV_ARGS(&dst))),"readback native destination");
  D3D12_RESOURCE_BARRIER b[2]{};b[0].Transition.pResource=texture.Get();b[0].Transition.Subresource=UINT_MAX;
  b[0].Transition.StateBefore=D3D12_RESOURCE_STATE_DEPTH_WRITE;
  b[0].Transition.StateAfter=static_cast<D3D12_RESOURCE_STATES>(mode=="readback_combined"?0x840:0x800);
  b[1].Transition.pResource=dst.Get();b[1].Transition.Subresource=UINT_MAX;
  b[1].Transition.StateBefore=shader;b[1].Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_DEST;
  command->ResourceBarrier(2,b);
  if(mode=="readback_view_changed")put(addr(objects[0]),0x18,reinterpret_cast<U>(texture.Get())+16);
  if(config.frame_renderer) {
    put(addr(frame_destination_backing),0x20,reinterpret_cast<U>(dst.Get()));
    if(mode=="readback_frame_empty" || mode=="readback_frame_self") {
      const auto rectangle=frame_rect;
      if(mode=="readback_frame_empty")frame_rect[2]=frame_rect[0];
      const U destination=mode=="readback_frame_self"?addr(frame_source):addr(frame_destination);
      check(binding_fixture26(config.frame_renderer+0x710,addr(frame_rect),addr(frame_offset),destination,8,0,0)==0x12345678 && GetLastError()==0x5151,"noop producer forwarding");
      check(pv::healthy(),"noop producer poisoned recorder");frame_rect=rectangle;
    }
    if(mode!="readback_frame_orphan") {
      check(binding_fixture26(config.frame_renderer+0x710,addr(frame_rect),addr(frame_offset),addr(frame_destination),8,0,0)==0x12345678 && GetLastError()==0x5151,"producer ABI / last error");
    } else {
      put(addr(frame_packet),0,10U);put(addr(frame_packet),0x10,8U);
      put(addr(frame_packet),0x18,addr(frame_destination));put(addr(frame_packet),0x20,addr(frame_source));
    }
    if(mode=="readback_frame_changed")put(addr(frame_packet),0x28,1U);
    if(mode=="readback_frame_reuse") {
      put(config.frame_renderer,0x1f210,UINT64{102});put(config.frame_renderer,0x1f208,UINT64{92});
      binding_fixture26(config.frame_renderer+0x710,addr(frame_rect),addr(frame_offset),addr(frame_destination),8,0,0);
    }
    if(mode=="readback_frame_capacity")for(unsigned i=0;i<256;++i)
      binding_fixture26(config.frame_renderer+0x710,addr(frame_rect),addr(frame_offset),addr(frame_destination),8,0,0);
    if(mode=="readback_frame_exception")check(frame_exception_call() && GetLastError()==0x7171,"consumer exception / last error");
    else check(binding_fixture27(addr(frame_executor),addr(frame_context),addr(frame_packet),0,0,0,0)==0x87654321 && GetLastError()==0x6161,"consumer ABI / last error");
  } else command->CopyResource(dst.Get(),texture.Get());
  if(mode=="readback_twice")command->CopyResource(dst.Get(),texture.Get());
  check(SUCCEEDED(command->Close()),"readback Close");
  if(mode=="readback_discard")check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)) && SUCCEEDED(command->Close()),"discard recorded copy");
  ID3D12CommandList *batch=command.Get();queue->ExecuteCommandLists(1,&batch);
  check(SUCCEEDED(queue->Signal(completion_fence.Get(),1)),"native fixture fence");
  const auto deadline=GetTickCount64()+5000;
  while(completion_fence->GetCompletedValue()<1 && GetTickCount64()<deadline){pv::poll_watch();Sleep(1);}
  check(completion_fence->GetCompletedValue()==1,"fixture GPU completion");
  if(mode=="readback_resubmit") {
    queue->ExecuteCommandLists(1,&batch);check(SUCCEEDED(queue->Signal(completion_fence.Get(),2)),"repeat fence");
    while(completion_fence->GetCompletedValue()<2 && GetTickCount64()<deadline)Sleep(1);
    check(completion_fence->GetCompletedValue()==2,"repeated fixture work retired");
  }
  if(mode!="readback_discard")check(SUCCEEDED(allocator->Reset()) && SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"native recording retirement");
  pv::poll_watch();
  const bool admitted=mode=="readback_pixels" || mode=="readback_twice" || mode=="readback_frame_pixels" || mode=="readback_frame_reuse" ||
    mode=="readback_frame_empty" || mode=="readback_frame_self";
  if(admitted)check(pv::healthy() && !pv::awaiting_submission(),"private readback completion did not stop cleanly");
  else if(mode=="readback_discard" || mode=="readback_resubmit")check(!pv::healthy(),"discarded/resubmitted generation accepted");
  pb::stop();pv::stop();check(pb::disable() && pv::disable(),"readback hook disable");
  const bool saved=pv::save_readback(path.wstring());
  if(saved!=admitted){pv::write_readback(std::cerr);std::cerr<<'\n';pv::write_result(std::cerr);}
  check(saved==admitted,"readback admission result");
  if(saved) {
    check(std::filesystem::file_size(path)==16*16*4,"readback file extent");
    std::ifstream in(path,std::ios::binary);std::array<float,256> pixels{};in.read(reinterpret_cast<char *>(pixels.data()),sizeof(pixels));
    check(static_cast<bool>(in),"readback pixels file");for(float v:pixels)check(v==.375F,"production hook readback GPU pixels");
  }
  {std::ostringstream report;pv::write_readback(report);readback_report=report.str();}
  if(mode!="readback_discard" && mode!="readback_resubmit") {
    check(pv::owned_release_readback(true),"release fixture private resources after GPU completion and Reset");
  }
}
void bad_copy() {
  construct();std::string why;check(pb::arm(why),why.c_str());
  check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"copy admission reset");
  pv::mark_binding(false);pv::mark_binding(true);mark();pv::end_frame();
  pb::Frame f{};f.gpr[1]=reinterpret_cast<U>(command.Get());
  f.gpr[2]=reinterpret_cast<U>(texture.Get());f.gpr[3]=f.gpr[2];
  auto kind=pv::copy_resource;
  if(mode=="copy_bad_location"){kind=pv::copy_texture;f.gpr[2]=1;}
  if(mode=="copy_null_resource")f.gpr[3]=0;
  const unsigned count=mode=="copy_capacity"?513:1;
  for(unsigned i=0;i<count;++i) {
    auto token=pv::owned_copy_before(kind,f);
    pv::after(token,f,mode!="copy_exception");
  }
  check(!pv::healthy() && pv::settled(),"invalid copy did not fail/drain");
  check(pv::owned_error()==(mode=="copy_capacity"?pv::Error::capacity:mode=="copy_exception"?pv::Error::exception:pv::Error::read),"copy error reason");
}
void copy_arguments() {
  // Decoder component only: these deliberately distinctive coordinates are
  // never forwarded to the driver or submitted as GPU work.
  construct();std::string why;check(pb::arm(why),why.c_str());
  check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"copy arguments reset");
  pv::mark_binding(false);pv::mark_binding(true);mark();pv::end_frame();
  D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=destination.pResource=texture.Get();
  D3D12_BOX box{1,2,0,4,5,1};pb::Frame f{};f.gpr[1]=reinterpret_cast<U>(command.Get());
  f.gpr[2]=reinterpret_cast<U>(&destination);f.gpr[3]=0x100000007ULL;f.gpr[4]=0x10000000bULL;
  f.stack={0x10000000dULL,reinterpret_cast<U>(&source),reinterpret_cast<U>(&box),0};
  auto token=pv::owned_copy_before(pv::copy_texture,f);
  check(token.admitted && pv::healthy() && !pv::settled(),"copy arguments admission");
  pv::stop();check(!pv::disable(),"copy reference released before drain");
  pv::after(token,f,true);check(pv::healthy() && pv::settled(),"copy arguments drain");
}
void concurrent_copy() {
  // Admission-only concurrency fixture: a second list observes the same
  // resource while the first query is deliberately paused. No copy is sent
  // to the driver, and no renderer-wide lock can make the second call wait.
  construct();std::string why;check(pb::arm(why),why.c_str());
  check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"concurrent copy reset");
  pv::mark_binding(false);pv::mark_binding(true);mark();pv::end_frame();
  ComPtr<ID3D12CommandAllocator> a;ComPtr<ID3D12GraphicsCommandList> other;
  check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a))) &&
    SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,a.Get(),nullptr,IID_PPV_ARGS(&other))),"concurrent owned list");
  pb::Frame first{};first.gpr[1]=reinterpret_cast<U>(command.Get());first.gpr[2]=first.gpr[3]=reinterpret_cast<U>(texture.Get());
  auto second=first;second.gpr[1]=reinterpret_cast<U>(other.Get());
  pv::owned_pause_copy_pin();
  std::thread worker([&]{const auto token=pv::owned_copy_before(pv::copy_resource,first);pv::after(token,first,mode!="copy_concurrent_exception");});
  const auto deadline=GetTickCount64()+1000;
  while(!pv::owned_copy_pin_waiting() && GetTickCount64()<deadline)SwitchToThread();
  const bool paused=pv::owned_copy_pin_waiting();
  const auto token=pv::owned_copy_before(pv::copy_resource,second);
  const bool admitted=token.admitted && pv::healthy();pv::after(token,second,true);
  pv::stop();const bool retained=!pv::settled() && !pv::disable();
  pv::owned_resume_copy_pin();worker.join();
  check(paused && admitted && retained && pv::settled(),"concurrent copy progress/drain");
  check(pv::healthy()==(mode!="copy_concurrent_exception"),"concurrent copy error handling");
}
void copy_merge_damage() {
  construct();std::string why;check(pb::arm(why),why.c_str());
  check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"merge reset");
  pv::mark_binding(false);pv::mark_binding(true);mark();pv::end_frame();
  pb::Frame f{};f.gpr[1]=reinterpret_cast<U>(command.Get());f.gpr[2]=f.gpr[3]=reinterpret_cast<U>(texture.Get());
  const unsigned count=mode=="copy_merge_capacity"?65:2;
  for(unsigned i=0;i<count;++i){auto token=pv::owned_copy_before(pv::copy_resource,f);pv::after(token,f,true);}
  check(pv::healthy(),"merge fixture admission failed");
  pv::owned_change_copy_pin(mode=="copy_merge_desc"?0:mode=="copy_merge_identity"?1:mode=="copy_merge_device"?2:3);
  pv::stop();check(!pv::disable() && pv::settled(),"inconsistent resource observations accepted");
  check(pv::owned_error()==(mode=="copy_merge_capacity"?pv::Error::capacity:pv::Error::changed),"merge failure reason");
}
void submission_case() {
  construct();std::string why;check(pb::arm(why),why.c_str());
  ComPtr<ID3D12CommandAllocator> a;ComPtr<ID3D12GraphicsCommandList> before;
  check(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a))) &&
    SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,a.Get(),nullptr,IID_PPV_ARGS(&before))) &&
    SUCCEEDED(before->Close()) && SUCCEEDED(before->Reset(a.Get(),nullptr)),"predecessor generation");
  check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"selected reset");
  pv::mark_binding(false);pv::mark_binding(true);mark();pv::end_frame();
  check(pv::awaiting_submission(),"journal closed at draw instead of submission");
  // Recorded AFTER the selected marker, submitted BEFORE its list. This is
  // precisely the distinction that CPU-time-only replay must preserve.
  D3D12_RESOURCE_BARRIER b{};b.Transition.pResource=texture.Get();b.Transition.Subresource=UINT_MAX;
  b.Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;b.Transition.StateAfter=D3D12_RESOURCE_STATE_DEPTH_WRITE;
  before->ResourceBarrier(1,&b);
  if(mode.rfind("gpu_copy_",0)==0)before->ClearDepthStencilView({config.heap_start},D3D12_CLEAR_FLAG_DEPTH,.375F,0,0,nullptr);
  if(mode.rfind("gpu_copy_",0)==0)native_copy();
  check(SUCCEEDED(before->Close()) && SUCCEEDED(command->Close()),"submission closes");
  if(mode=="submit_reset")check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)) && SUCCEEDED(command->Close()),"re-recorded list");
  if(mode=="submit_timeout") {
    pv::owned_expire();pv::poll_watch();check(pv::owned_error()==pv::Error::deadline && !pv::awaiting_submission(),"submission deadline");return;
  }
  ID3D12CommandList *batch[]={before.Get(),command.Get()};
  if(mode=="submit_badcount" || mode=="submit_badread" || mode=="submit_duplicate" || mode=="submit_pending" || mode=="submit_exception") {
    // Exercise the admission/cleanup component without forwarding deliberately
    // invalid work to the driver. Real dispatch is tested by submit_batch.
    pb::Frame f{};f.gpr[1]=reinterpret_cast<U>(queue.Get());f.gpr[2]=2;f.gpr[3]=reinterpret_cast<U>(batch);
    if(mode=="submit_badcount")f.gpr[2]=129;
    if(mode=="submit_badread")f.gpr[3]=1;
    if(mode=="submit_duplicate")batch[1]=batch[0];
    const auto token=pv::owned_execute_before(f);
    check(token.admitted && !pv::settled(),"submission token missing");
    if(mode=="submit_pending") {
      pv::stop();check(!pv::disable(),"released queue/list refs during admitted submission");
    }
    pv::after(token,f,mode!="submit_exception");
    check(pv::settled() && (pv::healthy()==(mode=="submit_pending")),"submission error/pending cleanup");return;
  }
  if(mode=="submit_reversed")std::swap(batch[0],batch[1]);
  check(completion_fence->GetCompletedValue()==0,"baseline completion read");
  if(mode=="sync_chain") {
    ComPtr<ID3D12CommandQueue> compute;ComPtr<ID3D12Fence> handoff;
    D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_COMPUTE;
    check(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&compute))) &&
      SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&handoff))),"handoff objects");
    queue->ExecuteCommandLists(1,batch);
    check(SUCCEEDED(queue->Signal(handoff.Get(),1)) && SUCCEEDED(compute->Wait(handoff.Get(),1)),"outgoing handoff");
    queue->ExecuteCommandLists(1,batch+1);
    check(SUCCEEDED(compute->Signal(handoff.Get(),2)) && SUCCEEDED(queue->Wait(handoff.Get(),2)),"return handoff");
  } else queue->ExecuteCommandLists(2,batch);
  check(pv::healthy() && pv::awaiting_submission(),"journal stopped before completion observation");
  // Test-owned work only: production adds no commands, signals or waits.
  if(mode=="sync_cpu_signal")check(SUCCEEDED(completion_fence->Signal(1)),"CPU fence writer");
  if(mode=="sync_incomplete")pv::stop();
  constexpr UINT64 value=0x100000001ULL;
  check(SUCCEEDED(queue->Signal(completion_fence.Get(),value)),"owned fence signal");
  if(mode=="sync_reuse" || mode=="sync_resubmit") {
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(event!=nullptr,"owned completion event");
    const bool done=SUCCEEDED(completion_fence->SetEventOnCompletion(value,event)) && WaitForSingleObject(event,5000)==WAIT_OBJECT_0;
    CloseHandle(event);check(done,"owned GPU wait before list reuse");
    if(mode=="sync_reuse")check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)) && SUCCEEDED(command->Close()),"later generation");
    ID3D12CommandList *again=command.Get();queue->ExecuteCommandLists(1,&again);
  }
  const auto deadline=GetTickCount64()+5000;
  while(completion_fence->GetCompletedValue()<value && GetTickCount64()<deadline)Sleep(1);
  check(completion_fence->GetCompletedValue()==value,"owned submission completion");
  check(pv::healthy() && !pv::awaiting_submission() && pv::settled(),"completion stop/drain");
  if(mode=="gpu_copy_footprint") {
    // Verify the real seven-argument forwarding with GPU-written bytes, after
    // the owned fence. This is fixture data, never a Map of a game resource.
    const auto &f=copied_footprint;void *mapped{};
    D3D12_RANGE range{static_cast<SIZE_T>(f.Offset),static_cast<SIZE_T>(f.Offset+f.Footprint.RowPitch*15+16*4)};
    check(range.End<=copy_owned.back()->GetDesc().Width,"readback range exceeds footprint allocation");
    check(SUCCEEDED(copy_owned.back()->Map(0,&range,&mapped)),"owned readback Map");
    bool equal=true;
    for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x) {
      float pixel{};std::memcpy(&pixel,static_cast<const std::byte *>(mapped)+f.Offset+y*f.Footprint.RowPitch+x*4,4);
      equal=equal && pixel==.375F;
    }
    D3D12_RANGE written{};copy_owned.back()->Unmap(0,&written);check(equal,"depth copy GPU contents / argument forwarding");
  }
  if(mode=="sync_reuse" || mode=="sync_resubmit") {
    // Ensure even the later test-owned batch has retired before teardown.
    check(SUCCEEDED(queue->Signal(completion_fence.Get(),value+1)),"later owned fence");
    while(completion_fence->GetCompletedValue()<value+1 && GetTickCount64()<deadline)Sleep(1);
    check(completion_fence->GetCompletedValue()==value+1,"later owned completion");
  }
}
__declspec(noinline) U eight_arguments(U a,U b,U c,U d,U e,U f,U g,U h) {
  SetLastError(0x6161);
  return a==0x11 && b==0x22 && c==0x33 && d==0x44 && e==0x55 && f==0x66 && g==0x77 && h==0x88?0x123456789abcdef0ULL:0;
}
void policy() {
  pb::Frame frame{};frame.gpr[1]=0x11;frame.gpr[2]=0x22;frame.gpr[3]=0x33;frame.gpr[4]=0x44;
  frame.stack={0x55,0x66,0x77,0x88};mhw_binding_forward(reinterpret_cast<void *>(&eight_arguments),&frame);
  check(frame.gpr[0]==0x123456789abcdef0ULL && GetLastError()==0x6161,"eight argument forwarding");
  std::array<pv::Range,pv::range_capacity> out{};std::array<U,2> starts={0x1000,0x2000};std::array<unsigned,2> sizes={2,3};
  check(pv::destination_ranges(2,starts.data(),nullptr,32,out) && out[0].count==1 && out[1].count==1,"omitted counts");
  check(pv::destination_ranges(2,starts.data(),sizes.data(),32,out) && out[0].count==2 && out[1].count==3,"explicit counts");
  check(!pv::destination_ranges(33,starts.data(),nullptr,32,out),"range bound");
  starts[0]=UINTPTR_MAX-15;check(!pv::destination_ranges(1,starts.data(),nullptr,32,out),"range overflow");
  starts[0]=0x1000;sizes[0]=0;check(!pv::destination_ranges(1,starts.data(),sizes.data(),32,out),"empty count");
  pb::ResourceDescription r{};r.valid=true;r.dimension=3;r.format=39;r.flags=2;r.samples=1;r.mips=1;r.depth_or_array=1;
  std::array<unsigned,6> d={40,3,0,0,0,0};unsigned sub{};
  check(pv::valid_depth_view(d,r,sub) && !sub,"2D subresource");
  d[2]=1;check(!pv::valid_depth_view(d,r,sub),"readonly refused");d[2]=0;d[3]=1;check(!pv::valid_depth_view(d,r,sub),"mip bounds");
  r.depth_or_array=2;r.mips=3;d={40,4,0,2,1,1};check(pv::valid_depth_view(d,r,sub) && sub==5,"array mapping");
  d[5]=2;check(!pv::valid_depth_view(d,r,sub),"multi slice refused");
}
}
extern "C" U binding_fixture_body(U a,U b,U c,U fourth,U fifth,U,U,U kind) {
  if(kind==26) {
    std::array<unsigned,4> rect{};std::memcpy(rect.data(),reinterpret_cast<void *>(b),16);
    if(rect[0]==rect[2] || rect[1]==rect[3] || fourth==addr(frame_source)){SetLastError(0x5151);return 0x12345678;}
    unsigned count{};std::memcpy(&count,reinterpret_cast<void *>(a+0x2b80),4);
    put(addr(frame_packet),0,10U);put(addr(frame_packet),0x10,static_cast<unsigned>(fifth));
    put(addr(frame_packet),0x18,fourth);put(addr(frame_packet),0x20,addr(frame_source));
    std::memcpy(frame_packet.data()+0x28,reinterpret_cast<void *>(b),16);
    std::memcpy(frame_packet.data()+0x38,reinterpret_cast<void *>(c),8);
    frame_queue[2*count+1]=addr(frame_packet);put(a,0x2b80,count+1);
    if(mode=="readback_frame_stamp")put(config.frame_renderer,0x1f210,UINT64{999});
    SetLastError(0x5151);return 0x12345678;
  }
  if(kind==27) {
    if(mode=="readback_frame_exception"){SetLastError(0x7171);RaiseException(0xe0123457,0,0,nullptr);}
    U destination{};std::memcpy(&destination,frame_destination_backing.data()+0x20,8);
    command->CopyResource(reinterpret_cast<ID3D12Resource *>(destination),texture.Get());
    SetLastError(0x6161);return 0x87654321;
  }
  if(kind==24) {
    ++constructed;D3D12_DEPTH_STENCIL_VIEW_DESC d{};d.Format=DXGI_FORMAT_D32_FLOAT;d.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
    if(mode=="deadline_slot")pv::owned_expire();
    const auto input=reinterpret_cast<const unsigned *>(c);d.Texture2D.MipSlice=input[1];d.Flags=input[4]?D3D12_DSV_FLAG_READ_ONLY_DEPTH:D3D12_DSV_FLAG_NONE;
    U resource{},handle{};std::memcpy(&resource,reinterpret_cast<void *>(b+0x20),8);std::memcpy(&handle,reinterpret_cast<void *>(a+0x10),8);
    device->CreateDepthStencilView(reinterpret_cast<ID3D12Resource *>(resource),mode=="null_desc"?nullptr:&d,{handle});
    put(a,0x18,resource);put<unsigned char>(a,0x20,static_cast<unsigned char>(input[4]));
    if(mode=="exception"){SetLastError(0x3333);RaiseException(0xe0123456,0,0,nullptr);}
    if(mode=="pending"){waiting=true;while(!unblock.load())SwitchToThread();}
    if(mode=="changed")put(a,0x20,1U);
    SetLastError(0x2222);return a;
  }
  if(kind==25){++destroyed;put(a,0x10,U{});put(a,0x18,U{});SetLastError(0x4444);return a;}
  return 1;
}
int main(int argc,char **argv) {
  try {
    check(argc>1,"case required");mode=argv[1];if(mode=="policy"){policy();std::cout<<"pass: policy\n";return 0;}
    setup();const auto before_refs=texture->AddRef();texture->Release();
    const auto list_refs=command->AddRef(),queue_refs=queue->AddRef(),fence_refs=completion_fence->AddRef();command->Release();queue->Release();completion_fence->Release();
    if(mode=="prewatch")construct();
    check(pv::install_owned_watch(config),"install watcher");std::string why;
    if(mode.rfind("readback_",0)==0) {
      const auto path=std::filesystem::path(argc>2?std::string(argv[2])+".depth.f32":"readback-"+mode+"-"+std::to_string(GetCurrentProcessId())+".f32");
      if(config.triplet_readback)triplet_readback_case(path);else if(config.same_frame_readback)aligned_readback_case(path);else readback_case(path);
      if(mode=="readback_discard" || mode=="readback_resubmit") {
        check(pb::remove_owned() && pv::remove_owned_watch(),"discard hook cleanup");
        // No private completion proof: the one-shot buffer deliberately remains pinned until exit.
        std::cout<<"pass: "<<mode<<" (quarantined)\n";return 0;
      }
    }
    else if(mode.rfind("submit_",0)==0 || mode.rfind("sync_",0)==0 || mode.rfind("gpu_copy_",0)==0)submission_case();
    else if(mode=="copy_bad_location" || mode=="copy_null_resource" || mode=="copy_exception" || mode=="copy_capacity")bad_copy();
    else if(mode=="copy_arguments")copy_arguments();
    else if(mode=="copy_concurrent" || mode=="copy_concurrent_exception")concurrent_copy();
    else if(mode.rfind("copy_merge_",0)==0)copy_merge_damage();
    else if(mode=="deadline_slot") {
      construct();check(pv::owned_error()==pv::Error::deadline && pv::settled(),"deadline reservation drain");
    } else if(mode=="exception") {
      check(exception_call() && GetLastError()==0x3333,"exception/LastError forwarding");
      check(pv::owned_error()==pv::Error::exception && pv::settled(),"exception drain");
    } else if(mode=="pending") {
      std::thread t([]{construct();});while(!waiting.load())SwitchToThread();pv::stop();check(!pv::settled(),"pending token lost");
      check(!pv::disable(),"released pending pins");unblock=true;t.join();check(pv::settled(),"pending failed to drain");
    } else if(mode=="pin_capacity") {
      for(unsigned i=0;i<=pv::pin_capacity;++i)construct(i);
      check(pv::owned_error()==pv::Error::capacity && pv::owned_pins()==pv::pin_capacity,"pin cap");
    } else if(mode=="history_capacity") {
      check(pb::arm(why),why.c_str());
      for(unsigned i=0;i<pv::history_capacity;++i) {
        check(completion_fence->GetCompletedValue()==0,"history fence forward");
        check(pv::healthy(),"history failed before exact bound");
      }
      completion_fence->GetCompletedValue();
      check(pv::owned_error()==pv::Error::capacity,"history overflow not rejected");
      for(unsigned i=0;i<16;++i)check(completion_fence->GetCompletedValue()==0,"closed history forward");
    } else if(mode=="capacity") {
      for(unsigned i=0;i<800 && pv::healthy();++i){construct();destroy();}
      check(pv::owned_error()==pv::Error::capacity,"watch capacity not bounded");
    } else if(mode=="concurrent") {
      std::vector<std::thread> threads;
      for(unsigned i=0;i<16;++i)threads.emplace_back([i]{construct(i);destroy(i);});
      for(auto &t:threads)t.join();check(pv::healthy() && pv::owned_pins()==0 && constructed==16,"parallel watch");
    } else {
      if(mode=="readonly")recipe[4]=1;
      if(mode!="prewatch"){SetLastError(0x1111);check(construct()==addr(objects[0]) && GetLastError()==0x2222,"constructor forward/LastError");}
      if(mode=="changed")check(pv::owned_error()==pv::Error::changed,"changed constructor admitted");
      else {
        check(pv::healthy(),"watch fault after creation");
        if(mode=="reuse"){destroy();check(pv::owned_pins()==0,"retire reference");construct();}
        if(mode=="destroyed"){destroy();check(GetLastError()==0x4444,"destructor LastError");}
        if(mode=="overwrite") {
          D3D12_DEPTH_STENCIL_VIEW_DESC d{};d.Format=DXGI_FORMAT_D32_FLOAT;d.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
          device->CreateDepthStencilView(texture.Get(),&d,{config.heap_start});
        }
        if(mode=="copy_simple" || mode=="copy_ranges" || mode=="copy_implicit") {
          construct(1);D3D12_CPU_DESCRIPTOR_HANDLE dst{config.heap_start},src{config.heap_start+config.stride};unsigned n=1;
          if(mode=="copy_simple")device->CopyDescriptorsSimple(1,dst,src,D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
          else device->CopyDescriptors(1,&dst,mode=="copy_implicit"?nullptr:&n,1,&src,nullptr,D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        }
        check(pb::arm(why),why.c_str());
        if(mode!="no_reset")check(SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"SDK Reset");
        if(mode=="heap_changed")put(addr(backend),0x188,config.heap+16);
        if(mode=="device_changed")put(addr(backend),0x18,config.device+16);
        if(mode=="expired")pv::owned_expire();
        if(mode!="no_reset" && mode!="no_barrier")transition(mode=="split"?D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY:D3D12_RESOURCE_BARRIER_FLAG_NONE);
        if(mode=="roundtrip")command->ClearDepthStencilView({config.heap_start},D3D12_CLEAR_FLAG_DEPTH,.375F,0,0,nullptr);
        if(mode=="reset_after_barrier")check(SUCCEEDED(command->Close()) && SUCCEEDED(command->Reset(allocator.Get(),nullptr)),"reset generation");
        if(mode=="closed")check(SUCCEEDED(command->Close()),"closed list");
        if(pv::healthy()){pv::mark_binding(false);pv::mark_binding(true);}
        mark();pv::end_frame();
        const bool fault=mode=="heap_changed" || mode=="device_changed" || mode=="expired";
        check(pv::healthy()!=fault,"unexpected watcher status");
        if(!fault)check(pv::owned_marker().order!=0,"missing marker");
      }
    }
    pb::stop();pv::stop();check(pb::remove_owned() && pv::remove_owned_watch(),"hook/reference cleanup");
    const bool quarantined=mode=="readback_aligned_view_changed";
    check(pv::owned_pins()==0,"pins leaked");const auto after_refs=texture->AddRef();texture->Release();
    check(quarantined?after_refs>before_refs:after_refs==before_refs,"SDK resource ref balance/retention");
    const auto list_after=command->AddRef(),queue_after=queue->AddRef();command->Release();queue->Release();
    check((quarantined?list_after>list_refs:list_after==list_refs) && queue_after==queue_refs,"SDK list/queue ref balance/retention");
    const auto fence_after=completion_fence->AddRef();completion_fence->Release();check(fence_after==fence_refs,"SDK fence ref balance");
    for(const auto &resource:copy_owned){const auto refs=resource->AddRef();resource->Release();check(refs==2,"copy destination reference leak");}
    if(argc>2) {
      check(!std::filesystem::exists(argv[2]),"preserve fixture output");std::ofstream out(argv[2]);pv::write_result(out);check(static_cast<bool>(out),"trace output");
      std::ofstream meta(std::string(argv[2])+".ready.jsonl");pv::write_ready(meta);
      if(mode.rfind("readback_",0)==0){std::ofstream rb(std::string(argv[2])+".readback.json");rb<<readback_report;}
    }
    std::cout<<"pass: "<<mode<<" (owned SDK fixture)\n";return 0;
  } catch(const std::exception &e){std::cerr<<e.what()<<" watcher_error="<<static_cast<unsigned>(pv::owned_error())<<'\n';return 1;}
}
