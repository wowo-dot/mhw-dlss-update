#include "provenance.hpp"
#include "memory.hpp"
#include "depth_readback.hpp"
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <hde64.h>
#include <algorithm>
#include <cstring>
#include <ostream>
extern "C" {
void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
void provenance_construct();void provenance_destroy();void provenance_dsv();void provenance_copy();
void provenance_copy_simple();void provenance_close();void provenance_clear();void provenance_bundle();
void provenance_execute();
void provenance_queue_signal();void provenance_queue_wait();void provenance_fence_completed();void provenance_fence_signal();
void provenance_copy_texture();void provenance_copy_resource();
void provenance_packet_producer();void provenance_packet_consumer();
}
namespace mhw::projection_binding::provenance {
namespace {
constexpr unsigned open_bit=0x80000000U;
Config cfg;
std::array<Slot,watch_capacity> views;
std::array<Slot,history_capacity> history;
std::array<BarrierRecord,barrier_capacity> barrier_data;
struct ObjectPin {
  std::atomic<std::uintptr_t> object{};
  std::atomic<unsigned> ready{};
  std::uintptr_t identity{},device{},pin{};
  std::array<unsigned,4> desc{};
  std::uint64_t first{};
};
std::array<ObjectPin,list_capacity> lists;
std::array<ObjectPin,queue_capacity> queues;
std::array<ObjectPin,fence_capacity> fences;
std::array<std::uintptr_t,submission_capacity> submitted;
std::atomic<unsigned> submitted_count{};
std::atomic<std::uintptr_t> selected_command{};
std::atomic<std::uint64_t> draw_return{},selected_submission{};
std::atomic<unsigned> selected_submission_slot{UINT_MAX},completion_signal_slot{UINT_MAX};
std::atomic<std::uint64_t> completion_read{};
std::atomic<unsigned> gate{},view_count{},history_count{},barrier_count{},pins{};
std::atomic<std::uint64_t> order{};
std::atomic<Error> error{};
std::atomic<bool> frame_open{};
#ifdef MHW_BINDING_FIXTURE
std::atomic<unsigned> draw_return_calls{};
#endif
std::uint64_t started{},frame_started{};
Marker marker;
bool used{},installed{},released{};
std::array<void *,own_count> originals{};
std::array<bool,own_count> created{},enabled{};
ID3D12Device *device_pin{};
ID3D12DescriptorHeap *heap_pin{};
thread_local unsigned parent{};
constexpr std::array<void(*)(),own_count> entries={provenance_construct,provenance_destroy,provenance_dsv,
  provenance_copy,provenance_copy_simple,provenance_close,provenance_clear,provenance_bundle,provenance_execute,
  provenance_queue_signal,provenance_queue_wait,provenance_fence_completed,provenance_fence_signal,
  provenance_copy_texture,provenance_copy_resource,provenance_packet_producer,provenance_packet_consumer};
constexpr std::array<unsigned,own_count> slots={0,0,21,23,24,9,47,27,10,14,15,8,10,16,17,0,0};
void fail(Error why) noexcept {
  auto empty=Error::none;error.compare_exchange_strong(empty,why);stop();
}
bool enter() noexcept {
  auto v=gate.load(std::memory_order_acquire);
  while(v&open_bit) {
    if((v&~open_bit)>=4096){fail(Error::capacity);return false;}
    if(gate.compare_exchange_weak(v,v+1,std::memory_order_acq_rel))return true;
  }
  return false;
}
bool release(IUnknown *p) noexcept {
  if(!p)return true;std::uintptr_t target{};
  if(!system_method(reinterpret_cast<std::uintptr_t>(p),2,target))return false;
  __try{p->Release();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool anchors() noexcept {
  std::uintptr_t backend{},device{},heap{},start{};unsigned stride{};
  return read(cfg.image+0x51c7050,backend) && backend==cfg.backend &&
    read(backend+0x18,device) && device==cfg.device && read(backend+0x188,heap) && heap==cfg.heap &&
    read(backend+0x1a8,start) && start==cfg.heap_start && read(backend+0x1dc,stride) && stride==cfg.stride;
}
bool methods(std::uintptr_t p,std::initializer_list<unsigned> list) noexcept {
  for(auto s:list){std::uintptr_t at{};if(!system_method(p,s,at))return false;}return true;
}
HRESULT query(std::uintptr_t p,REFIID iid,void **out) noexcept {
  if(!methods(p,{0,2}))return E_NOINTERFACE;
  __try{return reinterpret_cast<IUnknown *>(p)->QueryInterface(iid,out);}
  __except(EXCEPTION_EXECUTE_HANDLER){return E_FAIL;}
}
bool pin_resource(Event &e,Slot &slot) noexcept {
  if(!e.resource)return true;
  if(pins.fetch_add(1)>=pin_capacity){pins.fetch_sub(1);fail(Error::capacity);return false;}
  ID3D12Resource *r{};IUnknown *identity{};ID3D12Device *device{};bool ok=false;
  if(methods(e.resource,{0,2,7})) {
    __try {
      auto p=reinterpret_cast<ID3D12Resource *>(e.resource);
      if(SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&r))) && reinterpret_cast<std::uintptr_t>(r)==e.resource &&
         SUCCEEDED(r->GetDevice(IID_PPV_ARGS(&device))) && reinterpret_cast<std::uintptr_t>(device)==cfg.device &&
         SUCCEEDED(r->QueryInterface(IID_PPV_ARGS(&identity))) && identity) {
        e.identity=reinterpret_cast<std::uintptr_t>(identity);ok=true;
      }
    } __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  }
  const bool a=release(identity),b=release(device);ok=ok && a && b;
  if(ok)slot.pin.store(reinterpret_cast<std::uintptr_t>(r),std::memory_order_release);
  else {release(r);pins.fetch_sub(1);}
  return ok;
}
void unpin(Slot &s) noexcept {
  auto p=s.pin.exchange(0,std::memory_order_acq_rel);
  if(p){if(!release(reinterpret_cast<IUnknown *>(p)))fail(Error::exception);pins.fetch_sub(1);}
}
bool api_target(unsigned kind,std::uintptr_t obj) noexcept {
  if(kind>=execute)kind-=2;
  std::uintptr_t target{};
  return system_method(obj,slots[kind],target) && target==cfg.targets[kind];
}
// The claimed slot prevents duplicate initialization. Never wait or hold a lock
// across COM: overlapping first use fails closed. Pins prevent address reuse
// throughout the bounded recording and post-draw submission window.
bool pin_object(std::uintptr_t object,unsigned type,std::uint64_t first,std::uintptr_t &canonical) noexcept {
  if(!object)return false;
  auto data=type==2?fences.data():type==1?queues.data():lists.data();
  const auto count=type==2?fence_capacity:type==1?queue_capacity:list_capacity;
  ObjectPin *entry{};
  for(unsigned i=0;i<count;++i) {
    auto seen=data[i].object.load(std::memory_order_acquire);
    if(!seen && data[i].object.compare_exchange_strong(seen,object,std::memory_order_acq_rel)) {entry=&data[i];break;}
    if(seen==object) {
      if(data[i].ready.load(std::memory_order_acquire)!=1)return false;
      canonical=data[i].identity;return true;
    }
  }
  if(!entry){fail(Error::capacity);return false;}
  IUnknown *held{},*identity{},*extended_identity{};ID3D12Device *device{};ID3D12Fence1 *extended{};bool ok=false;
  if(methods(object,{0,2,7,type==1?18U:8U})) {
    __try {
      if(type==2) {
        if(SUCCEEDED(reinterpret_cast<IUnknown *>(object)->QueryInterface(__uuidof(ID3D12Fence),reinterpret_cast<void **>(&held))) && held) {
          auto p=static_cast<ID3D12Fence *>(held);
          if(reinterpret_cast<std::uintptr_t>(p)==object && SUCCEEDED(p->GetDevice(IID_PPV_ARGS(&device))) &&
             SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&extended))) && extended && methods(reinterpret_cast<std::uintptr_t>(extended),{0,2,11})) {
            entry->desc[0]=static_cast<unsigned>(extended->GetCreationFlags());ok=true;
          }
        }
      } else if(type==1) {
        if(SUCCEEDED(reinterpret_cast<IUnknown *>(object)->QueryInterface(__uuidof(ID3D12CommandQueue),reinterpret_cast<void **>(&held))) && held) {
          auto p=static_cast<ID3D12CommandQueue *>(held);
          if(reinterpret_cast<std::uintptr_t>(p)==object && SUCCEEDED(p->GetDevice(IID_PPV_ARGS(&device)))) {
            const auto d=p->GetDesc();entry->desc={static_cast<unsigned>(d.Type),static_cast<unsigned>(d.Priority),static_cast<unsigned>(d.Flags),d.NodeMask};ok=true;
          }
        }
      } else {
        if(SUCCEEDED(reinterpret_cast<IUnknown *>(object)->QueryInterface(__uuidof(ID3D12GraphicsCommandList),reinterpret_cast<void **>(&held))) && held) {
          auto p=static_cast<ID3D12GraphicsCommandList *>(held);
          if(reinterpret_cast<std::uintptr_t>(p)==object && SUCCEEDED(p->GetDevice(IID_PPV_ARGS(&device)))) {
            entry->desc[0]=static_cast<unsigned>(p->GetType());ok=true;
          }
        }
      }
      ok=ok && reinterpret_cast<std::uintptr_t>(device)==cfg.device &&
        SUCCEEDED(held->QueryInterface(IID_PPV_ARGS(&identity))) && identity;
      if(ok && extended)ok=SUCCEEDED(extended->QueryInterface(IID_PPV_ARGS(&extended_identity))) && extended_identity==identity;
      if(ok){entry->identity=reinterpret_cast<std::uintptr_t>(identity);entry->device=cfg.device;}
    } __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  }
  const bool a=release(identity),b=release(device),c=release(extended),d=release(extended_identity);ok=ok && a && b && c && d;
  if(!ok){release(held);entry->ready.store(2,std::memory_order_release);return false;}
  entry->pin=reinterpret_cast<std::uintptr_t>(held);entry->first=first;
  canonical=entry->identity;entry->ready.store(1,std::memory_order_release);return true;
}
#include "copy_capture.inl"
bool frame_copy(const Event &,unsigned &) noexcept;
std::uint64_t selected_source_order(unsigned) noexcept;
#include "readback_capture.inl"
#include "frame_capture.inl"
constexpr auto journal_storage_bytes=sizeof(views)+sizeof(history)+sizeof(barrier_data)+sizeof(lists)+sizeof(queues)+sizeof(fences)+
  sizeof(submitted)+sizeof(copies)+sizeof(copy_resources)+sizeof(copy_pins)+sizeof(frame_receipts)+sizeof(frame_consumers);
static_assert(journal_storage_bytes<=12ULL*1024*1024);
Token reserve(unsigned kind,bool is_history,const Frame &f) noexcept {
  Token t{};t.kind=kind;t.history=is_history;
  if(!enter())return t;t.admitted=true;
  const auto tick=GetTickCount64();
  if(tick-started>=watch_ms || (is_history && tick-frame_started>=window_ms)){fail(Error::deadline);return t;}
  t.index=(is_history?history_count:view_count).fetch_add(1);
  if(t.index>=(is_history?history_capacity:watch_capacity)){fail(Error::capacity);return t;}
  auto &s=is_history?history[t.index]:views[t.index];
  auto &e=s.value;e.kind=kind;e.object=f.gpr[1];e.caller=f.caller;e.thread=GetCurrentThreadId();
  e.enter=order.fetch_add(1)+1;e.tick=tick;e.parent=parent;e.result=0x8000000a;
  s.published.store(1,std::memory_order_release); // immutable header is now safe for online admission
  return t;
}
bool ranges(Event &e,const Frame &f,bool simple) noexcept {
  std::array<std::uintptr_t,range_capacity> starts{};std::array<unsigned,range_capacity> sizes{};
  if(simple){starts[0]=f.gpr[3];sizes[0]=static_cast<unsigned>(f.gpr[2]);e.count=1;}
  else {
    if(static_cast<unsigned>(f.gpr[2])>range_capacity)return false;e.count=static_cast<unsigned>(f.gpr[2]);
    if(e.count && !copy_memory(f.gpr[3],starts.data(),8*e.count))return false;
    if(f.gpr[4] && e.count && !copy_memory(f.gpr[4],sizes.data(),4*e.count))return false;
  }
  return destination_ranges(e.count,starts.data(),simple || f.gpr[4]?sizes.data():nullptr,cfg.stride,e.ranges);
}
Token before(unsigned kind,const Frame &f) noexcept {
  if(readback_internal)return {};
  if(!(gate.load(std::memory_order_acquire)&open_bit))return {};
  if(kind>=close && !frame_open.load(std::memory_order_acquire))return {};
#ifndef MHW_BINDING_FIXTURE
  // Scope is the three byte-gated native copy calls, not every engine copy.
  if(kind==copy_texture && f.caller!=cfg.image+0x259e0e2 && f.caller!=cfg.image+0x259e1a8)return {};
  if(kind==copy_resource && f.caller!=cfg.image+0x259e100)return {};
#endif
  if(kind>=create_dsv && kind<=copy_simple) {
    if(f.gpr[1]!=cfg.device)return {};
    if(kind==copy_descriptors && static_cast<unsigned>(f.stack[3])!=D3D12_DESCRIPTOR_HEAP_TYPE_DSV)return {};
    if(kind==copy_simple && static_cast<unsigned>(f.stack[0])!=D3D12_DESCRIPTOR_HEAP_TYPE_DSV)return {};
  }
  auto t=reserve(kind,kind>=close,f);
  if(!t.admitted || t.index>=(t.history?history_capacity:watch_capacity) || error.load()!=Error::none)return t;
  auto &s=t.history?history[t.index]:views[t.index];auto &e=s.value;e.valid=anchors();
  if(!e.valid){fail(Error::changed);return t;}
  if(kind>=create_dsv && !api_target(kind,f.gpr[1])){e.valid=false;fail(Error::target);return t;}
  if(t.history && !pin_object(e.object,kind==fence_completed || kind==fence_signal?2:kind>=execute && kind<=queue_wait?1:0,e.enter,e.identity)) {e.valid=false;fail(Error::identity);return t;}
  if(kind==construct) {
    if(parent){e.valid=false;fail(Error::reentrant);return t;}
    t.prior_parent=parent;t.constructor=true;parent=t.index+1;
    e.backing=f.gpr[2];e.valid=copy_memory(f.gpr[3],e.recipe.data(),20);
  } else if(kind==destroy) {
    e.valid=read(e.object+0x10,e.handle) && read(e.object+0x18,e.resource);
    // Only completed creation records own references. A concurrent unfinished
    // lifetime is rejected by replay and its reference remains until drain.
    const auto n=(std::min)(view_count.load(),watch_capacity);
    for(unsigned i=0;i<n;++i)if(views[i].published.load(std::memory_order_acquire)==2) {
      const auto &v=views[i].value;
      if(v.kind==create_dsv && v.parent && v.parent<=n &&
         views[v.parent-1].published.load(std::memory_order_acquire)==2 &&
         views[v.parent-1].value.object==e.object)unpin(views[i]);
    }
  } else if(kind==create_dsv) {
    e.resource=f.gpr[2];e.handle=f.gpr[4];e.null_desc=f.gpr[3]==0;
    e.valid=e.handle!=0 && (e.null_desc || copy_memory(f.gpr[3],e.descriptor.data(),24));
    if(parent && e.valid) {
#ifndef MHW_BINDING_FIXTURE
      if(f.caller!=cfg.image+0x24e6516)e.valid=false;
#endif
      if(e.valid)e.valid=pin_resource(e,s);
    }
  } else if(kind==copy_descriptors || kind==copy_simple)e.valid=ranges(e,f,kind==copy_simple);
  else if(kind==clear_depth) {
    e.handle=f.gpr[2];e.descriptor[0]=static_cast<unsigned>(f.gpr[3]);
    std::memcpy(&e.descriptor[1],f.xmm.data()+48,4);e.descriptor[2]=static_cast<unsigned>(f.stack[0]&255);
    e.count=static_cast<unsigned>(f.stack[1]);e.valid=e.count==0 && f.stack[2]==0;
  } else if(kind==bundle)e.resource=f.gpr[2];
  else if(kind==execute) {
    e.count=static_cast<unsigned>(f.gpr[2]);
    e.valid=e.count>0 && e.count<=batch_capacity;
    if(e.valid) {
      e.first=submitted_count.fetch_add(e.count);
      if(e.first>submission_capacity || e.count>submission_capacity-e.first){e.valid=false;fail(Error::capacity);}
      else {
        e.valid=copy_memory(f.gpr[3],submitted.data()+e.first,e.count*sizeof(submitted[0]));
        for(unsigned i=0;e.valid && i<e.count;++i) {
          std::uintptr_t id{};const auto p=submitted[e.first+i];
          e.valid=pin_object(p,false,e.enter,id);
          for(unsigned j=0;j<i;++j)if(submitted[e.first+j]==p)e.valid=false;
        }
      }
    }
  } else if(kind==queue_signal || kind==queue_wait) {
    e.resource=f.gpr[2];e.handle=f.gpr[3];
    e.valid=pin_object(e.resource,2,e.enter,e.backing);
  } else if(kind==fence_signal)e.handle=f.gpr[2];
  else if(kind==copy_texture || kind==copy_resource)e.valid=capture_copy(e,f);
  if(!e.valid)fail(Error::read);
  return t;
}
bool install() noexcept {
  for(unsigned i=0;i<own_count;++i) {
    if(i>=15 && !cfg.frame_renderer)continue;
    std::array<std::byte,32> code{};bool safe=copy_memory(cfg.targets[i],code.data(),32);
    for(unsigned n=0;safe && n<14;) {
      hde64s h{};const auto len=hde64_disasm(code.data()+n,&h);
      if(!len || (h.flags&F_ERROR) || h.opcode==0xe8 || (h.opcode==0xff && (h.modrm_reg==2 || h.modrm_reg==3)))safe=false;
      else {n+=len;if(h.opcode==0xe9 || h.opcode==0xeb || (h.opcode==0xff && h.modrm_reg==4) || h.opcode==0xc3)break;}
    }
    if(!safe || MH_CreateHook(reinterpret_cast<void *>(cfg.targets[i]),reinterpret_cast<void *>(entries[i]),&originals[i])!=MH_OK)return false;
    created[i]=true;
  }
  for(unsigned i=0;i<own_count;++i) {
    if(!created[i])continue;
    if(MH_EnableHook(reinterpret_cast<void *>(cfg.targets[i]))!=MH_OK)return false;enabled[i]=true;
  }
  installed=true;started=GetTickCount64();gate.store(open_bit,std::memory_order_release);return true;
}
bool discover_watch(std::uintptr_t base,Config &c,std::string &why) {
  c.image=base;unsigned char api{};
  if(!read(base+0x51cc080,api) || !api || !read(base+0x51c7050,c.backend) || !c.backend ||
     !read(c.backend+0x18,c.device) || !c.device || !read(c.backend+0x188,c.heap) || !c.heap ||
     !read(c.backend+0x1a8,c.heap_start) || !c.heap_start || !read(c.backend+0x1dc,c.stride) || !c.stride) {
    why="waiting-dx12-depth-heap";return false;
  }
  struct Gate {unsigned rva,size;const char *sha;};
  constexpr Gate gates[]={
    {0x24e6400,305,"8d9c64d44617936b80ed9af773bc5cf1f286b04169fcca20c9bb79a40ab796e2"},
    {0x24e8e40,107,"e3016e55164c9e7308ed350c779bcf8545382480a525f40521a39bc453129fb1"},
    {0x23a2880,130,"ffd860adae6783e79b03ce05bd7fa52a8fc074329c4f8e53808af2f61dcbcae0"},
    {0x23ab3c0,95,"1cfae8e6aaaa5a27557986a54c0e3c658ec631d2cc2fdae3cf5e794939f31569"},
    {0x23a48e0,11101,"8757a054cc1c203128fdfb73ac88975dda7dc73ef5ef1a53a80f22cd0eb8e500"},
    {0x259df30,681,"e84ae8742c4a0ef334692595bb89c04c8be80306d4416c20304566cc1e5dd85a"}};
  for(const auto &g:gates)if(!verify_code(base,g.rva,g.size,g.sha)){why="waiting-watch-code-"+std::to_string(g.rva);return false;}
  if(!verify_code(base,0x23cd170,0x6af,"8103c87804f325a15bbeb3529f3b575cc14e3bae986156d53e1b796fbbec6d31") ||
     !read(base+0x51c4480,c.frame_renderer) || !c.frame_renderer || c.frame_renderer>UINTPTR_MAX-0x1f218) {
    why="waiting-copy-producer-code-renderer";return false;
  }
  c.targets[15]=base+0x23cd170;c.targets[16]=base+0x259df30;c.same_frame_readback=true;c.triplet_readback=true;c.sequence_readback=true;
  c.targets[construct]=base+0x24e6400;c.targets[destroy]=base+0x24e8e40;
  for(unsigned i:{0U,1U,15U,16U})if(pool_state().page<c.targets[i]-0x40000000 || pool_state().page>=c.targets[i]+0x40000000-4095) {
    why="watch-pool-reachability";return false;
  }
  if(!methods(c.device,{0,2,8,15,21,23,24,36}) || !methods(c.heap,{0,2,7,8,9})){why="watch-system-methods";return false;}
  using Microsoft::WRL::ComPtr;
  ComPtr<ID3D12Device> device,heap_device;ComPtr<ID3D12DescriptorHeap> heap;
  // Worker-only queries after bounded pointer/method checks; partial outputs
  // remain scoped owners if any later discovery step fails.
  if(FAILED(query(c.device,IID_PPV_ARGS(&device))) ||
     FAILED(query(c.heap,IID_PPV_ARGS(&heap))) ||
     reinterpret_cast<std::uintptr_t>(device.Get())!=c.device || reinterpret_cast<std::uintptr_t>(heap.Get())!=c.heap ||
     FAILED(heap->GetDevice(IID_PPV_ARGS(&heap_device))) || heap_device.Get()!=device.Get()) {why="watch-identity";return false;}
  const auto hd=heap->GetDesc();c.heap_count=hd.NumDescriptors;
  if(hd.Type!=D3D12_DESCRIPTOR_HEAP_TYPE_DSV || hd.Flags!=D3D12_DESCRIPTOR_HEAP_FLAG_NONE ||
     !c.heap_count || c.heap_count>65536 || heap->GetCPUDescriptorHandleForHeapStart().ptr!=c.heap_start ||
     device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)!=c.stride ||
     c.heap_start>UINTPTR_MAX-static_cast<std::uint64_t>(c.stride)*c.heap_count) {why="watch-heap-layout";return false;}
  ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12CommandQueue> queue;
  ComPtr<ID3D12Fence> fence;
  D3D12_COMMAND_QUEUE_DESC q{};
  if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))) ||
     FAILED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))) ||
     FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))) ||
     FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))) ||
     FAILED(list->Close())) {why="watch-owned-discovery";return false;}
  for(unsigned i=create_dsv;i<15;++i)if(!system_method(i==11 || i==12?reinterpret_cast<std::uintptr_t>(fence.Get()):i>=8 && i<=10?reinterpret_cast<std::uintptr_t>(queue.Get()):i<close?c.device:reinterpret_cast<std::uintptr_t>(list.Get()),slots[i],c.targets[i])) {
    why="watch-api-method";return false;
  }
  for(unsigned i=0;i<own_count;++i)for(unsigned j=0;j<i;++j)if(c.targets[i]==c.targets[j]){why="watch-aliased-target";return false;}
  device_pin=device.Detach();heap_pin=heap.Detach();return true;
}
void event_json(std::ostream &o,const Event &e) {
  o<<"{\"kind\":"<<e.kind<<",\"thread\":"<<e.thread<<",\"parent\":"<<e.parent<<",\"enter\":"<<e.enter<<",\"exit\":"<<e.exit
   <<",\"tick\":"<<e.tick<<",\"object\":"<<e.object<<",\"caller\":"<<e.caller<<",\"resource\":"<<e.resource<<",\"identity\":"<<e.identity
   <<",\"handle\":"<<e.handle<<",\"backing\":"<<e.backing<<",\"returned\":"<<e.returned<<",\"valid\":"<<e.valid<<",\"null_desc\":"<<e.null_desc
   <<",\"result\":"<<e.result<<",\"count\":"<<e.count<<",\"first\":"<<e.first<<",\"descriptor\":[";
  for(unsigned i=0;i<6;++i){if(i)o<<',';o<<e.descriptor[i];}o<<"],\"recipe\":[";
  for(unsigned i=0;i<5;++i){if(i)o<<',';o<<e.recipe[i];}o<<"],\"ranges\":[";
  if(e.kind==copy_descriptors || e.kind==copy_simple)for(unsigned i=0;i<(std::min)(e.count,range_capacity);++i) {
    if(i)o<<',';o<<'['<<e.ranges[i].start<<','<<e.ranges[i].count<<']';
  }
  o<<"]}";
}
template<std::size_t N>void objects_json(std::ostream &o,const std::array<ObjectPin,N> &data) {
  bool comma=false;
  for(const auto &p:data)if(p.object.load()) {
    if(comma)o<<',';comma=true;
    o<<"{\"object\":"<<p.object.load()<<",\"identity\":"<<p.identity<<",\"device\":"<<p.device
     <<",\"first\":"<<p.first<<",\"ready\":"<<p.ready.load()<<",\"pin\":"<<p.pin<<",\"desc\":[";
    for(unsigned i=0;i<4;++i){if(i)o<<',';o<<p.desc[i];}o<<"]}";
  }
}
}
Start start_watch(std::uintptr_t base,std::string &why) {
  if(used){why="watch-already-used";return Start::failed;}
  Config c{};if(!discover_watch(base,c,why))return Start::waiting;
  cfg=c;used=true;
  if(!anchors() || !install()){fail(Error::install);disable();why="watch-install";return Start::failed;}
  return Start::active;
}
bool healthy() noexcept {return error.load()==Error::none;}
bool active() noexcept {return installed;}
bool select_source(const Source &s,unsigned stage) noexcept {return !installed || (!cfg.triplet_readback && stage!=1) || frame_select(s,stage);}
void poll_watch() noexcept {
  if(readback_enabled) {
    InternalReadback internal;bool terminal=true;
    for(unsigned stage=0;stage<readback_stages();++stage)if(stage==1 || cfg.triplet_readback) {
      auto &pixels=depth_pixels[stage];pixels.poll();if(healthy())readback_retirement(pixels);
      terminal=terminal && pixels.terminal();
    }
    if(terminal && completion_read.load(std::memory_order_acquire))stop();
  }
  if((gate.load()&open_bit) && (GetTickCount64()-started>=watch_ms ||
     (frame_open.load(std::memory_order_acquire) && GetTickCount64()-frame_started>=window_ms)))fail(Error::deadline);
}
bool start_frame(const Runtime &r) noexcept {
  if(!installed)return true; // Legacy fixtures do not enable this extension.
  if(!healthy() || !(gate.load()&open_bit) || !anchors() || r.backend!=cfg.backend || r.device!=cfg.device)return false;
  frame_started=GetTickCount64();frame_open.store(true,std::memory_order_release);return true;
}
Token history_before(unsigned kind,const Frame &f) noexcept {
  if(!frame_open.load(std::memory_order_acquire))return {};
  const auto k=kind==mhw::projection_binding::reset?list_reset:resource_barrier;
  auto t=reserve(k,true,f);
  if(!t.admitted || t.index>=history_capacity || !healthy())return t;
  auto &e=history[t.index].value;std::uintptr_t at{};
  const auto slot=k==list_reset?10U:26U;
  e.valid=anchors() && system_method(f.gpr[1],slot,at) && at==runtime().targets[kind];
  if(e.valid && !pin_object(e.object,false,e.enter,e.identity)){e.valid=false;fail(Error::identity);}
  if(k==list_reset) {e.resource=f.gpr[2];e.handle=f.gpr[3];}
  else if(e.valid) {
    if(static_cast<unsigned>(f.gpr[2])>1024 || !static_cast<unsigned>(f.gpr[2]))e.valid=false;
    else {
      e.count=static_cast<unsigned>(f.gpr[2]);e.first=barrier_count.fetch_add(e.count);
      if(e.first>barrier_capacity || e.count>barrier_capacity-e.first){e.valid=false;fail(Error::capacity);}
      else {
        std::array<D3D12_RESOURCE_BARRIER,1024> b{};
        if(!copy_memory(f.gpr[3],b.data(),e.count*sizeof(b[0])))e.valid=false;
        else for(unsigned i=0;i<e.count;++i) {
          const auto &v=b[i];auto &r=barrier_data[e.first+i];r.type=v.Type;r.flags=v.Flags;
          if(v.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION){r.resource=reinterpret_cast<std::uintptr_t>(v.Transition.pResource);r.subresource=v.Transition.Subresource;r.before=v.Transition.StateBefore;r.after=v.Transition.StateAfter;}
          else if(v.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING){r.resource=reinterpret_cast<std::uintptr_t>(v.Aliasing.pResourceBefore);r.other=reinterpret_cast<std::uintptr_t>(v.Aliasing.pResourceAfter);}
          else if(v.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV)r.resource=reinterpret_cast<std::uintptr_t>(v.UAV.pResource);
          else e.valid=false;
        }
      }
    }
  }
  if(!e.valid)fail(Error::read);return t;
}
void after(Token t,const Frame &f,bool returned) noexcept {
  if(!t.admitted)return;
  if(t.index<(t.history?history_capacity:watch_capacity)) {
    auto &s=t.history?history[t.index]:views[t.index];auto &e=s.value;
    if(s.published.load()==1) {
      if(!returned){e.valid=false;fail(Error::exception);}
      if(returned && t.kind==construct && e.valid) {
        unsigned char ro{};
        e.valid=f.gpr[0]==e.object && read(e.object+0x10,e.handle) && read(e.object+0x18,e.resource) &&
          read(e.object+0x20,ro) && ro==(e.recipe[4]&255) && anchors();
        if(!e.valid)fail(Error::changed);
      }
      if(t.kind==close || t.kind==list_reset || t.kind==queue_signal || t.kind==queue_wait || t.kind==fence_signal)e.result=static_cast<unsigned>(f.gpr[0]);
      if(t.kind==fence_completed) {
        e.handle=f.gpr[0];
        if(returned && e.handle==UINT64_MAX){e.valid=false;fail(Error::device_removed);}
      }
      e.returned=returned;e.exit=order.fetch_add(1)+1;s.published.store(2,std::memory_order_release);
      if(t.kind==copy_resource && returned && e.valid)readback_copy(e);
      if(t.kind==execute && returned && e.valid) {
        const auto selected=selected_command.load(std::memory_order_acquire);
        for(unsigned i=0;selected && i<e.count;++i)if(submitted[e.first+i]==selected) {
          std::uint64_t empty{};
          // Keep the first selected submission. Later reuse/reset is evidence
          // for replay, not permission to move the selected generation.
          if(selected_submission.compare_exchange_strong(empty,e.enter))selected_submission_slot.store(t.index,std::memory_order_release);
          break;
        }
      }
      if(t.kind==execute && returned && e.valid)readback_submit(e);
      if(t.kind==queue_signal && returned && e.valid && e.result<0x80000000) {
        const auto at=selected_submission_slot.load(std::memory_order_acquire);
        if(at<history_capacity && history[at].value.exit<e.enter && history[at].value.object==e.object && e.handle && e.handle!=UINT64_MAX) {
          auto empty=UINT_MAX;completion_signal_slot.compare_exchange_strong(empty,t.index,std::memory_order_acq_rel);
        }
      }
      if(t.kind==fence_completed && returned && e.valid) {
        const auto at=completion_signal_slot.load(std::memory_order_acquire);
        if(at<history_capacity) {
          const auto &signal=history[at].value;
          if(e.object==signal.resource && signal.exit<e.enter && e.handle>=signal.handle) {
            std::uint64_t empty{};completion_read.compare_exchange_strong(empty,e.enter,std::memory_order_acq_rel);
            if(!readback_enabled)stop();
          }
        }
      }
    }
  }
  if(t.constructor)parent=t.prior_parent;
  gate.fetch_sub(1,std::memory_order_release);
}
void mark_draw(const Draw &d) noexcept {
  if(!installed)return;
  if(!frame_open.load() || !healthy() || marker.order || !anchors()){fail(Error::changed);return;}
  const auto &v=d.attachments.ready.views[8];std::uintptr_t id{};
  for(unsigned i=0;i<d.attachments.resource_count;++i)if(d.attachments.resources[i].resource==v.resource)id=d.attachments.resources[i].identity;
  if(!v.backend || !v.handle || !v.resource || !id){fail(Error::identity);return;}
  marker.order=order.fetch_add(1)+1;marker.command=d.command;marker.view=v.backend;
  marker.resource=v.resource;marker.identity=id;marker.handle=v.handle;
  selected_command.store(d.command,std::memory_order_release);
}
void mark_binding(bool returned) noexcept {
  if(!installed)return;
  if(!frame_open.load() || !healthy() || !anchors() || marker.order ||
     (returned?(!marker.bind_enter || marker.bind_exit):marker.bind_enter!=0)){fail(Error::changed);return;}
  (returned?marker.bind_exit:marker.bind_enter)=order.fetch_add(1)+1;
}
void end_frame() noexcept {
#ifdef MHW_BINDING_FIXTURE
  draw_return_calls.fetch_add(1);
#endif
  if(!installed)return;
  draw_return.store(order.fetch_add(1)+1);
}
bool awaiting_submission() noexcept {return installed && frame_open.load(std::memory_order_acquire) && healthy();}
void stop() noexcept {frame_open.store(false,std::memory_order_release);gate.fetch_and(~open_bit,std::memory_order_acq_rel);}
bool settled() noexcept {return gate.load(std::memory_order_acquire)==0;}
bool disable() noexcept {
  stop();bool ok=true;
  for(unsigned i=0;i<own_count;++i)if(enabled[i]) {
    const auto s=MH_DisableHook(reinterpret_cast<void *>(cfg.targets[i]));
    if(s==MH_OK || s==MH_ERROR_DISABLED)enabled[i]=false;else ok=false;
  }
  if(!settled())return false;
  if(readback_enabled)for(auto &pixels:depth_pixels)pixels.abort();
  if(!released) {
    for(auto &s:views)unpin(s);
    for(auto &p:lists)if(p.pin){ok=release(reinterpret_cast<IUnknown *>(p.pin)) && ok;p.pin=0;}
    for(auto &p:queues)if(p.pin){ok=release(reinterpret_cast<IUnknown *>(p.pin)) && ok;p.pin=0;}
    for(auto &p:fences)if(p.pin){ok=release(reinterpret_cast<IUnknown *>(p.pin)) && ok;p.pin=0;}
    const bool merged=merge_copy_resources(),copy_released=release_copy_resources();ok=ok && merged && copy_released;
    const bool h=release(heap_pin),d=release(device_pin);heap_pin=nullptr;device_pin=nullptr;
    ok=ok && h && d;released=true;if(!h || !d)fail(Error::exception);
  }
  return ok && pins.load()==0;
}
void dispatch(unsigned kind,Frame *f) {
  if(kind>=15) {
    const auto incoming=GetLastError();const auto t=frame_before(kind,*f);bool returned=false;DWORD outgoing=incoming;
    SetLastError(incoming);
    __try{mhw_binding_forward(originals[kind],f);outgoing=GetLastError();returned=true;}
    __finally{const auto saved=returned?outgoing:GetLastError();frame_after(t,returned);SetLastError(saved);}
    return;
  }
  const auto incoming=GetLastError();const auto t=before(kind>=8?kind+2:kind,*f);bool returned=false;DWORD outgoing=incoming;
  SetLastError(incoming);
  __try{mhw_binding_forward(originals[kind],f);outgoing=GetLastError();returned=true;}
  __finally{const auto saved=returned?outgoing:GetLastError();after(t,*f,returned);SetLastError(saved);}
}
void write_ready(std::ostream &o) {
  o<<std::boolalpha<<"{\"event\":\"provenance_ready\",\"schema\":4,\"started\":"<<started<<",\"code_gates\":"<<(cfg.frame_renderer?7:6)<<",\"backend\":"<<cfg.backend
   <<",\"device\":"<<cfg.device<<",\"heap\":"<<cfg.heap<<",\"heap_start\":"<<cfg.heap_start<<",\"heap_count\":"<<cfg.heap_count<<",\"stride\":"<<cfg.stride
   <<",\"watch_ms\":"<<watch_ms<<",\"watch_capacity\":"<<watch_capacity<<",\"history_capacity\":"<<history_capacity<<",\"barrier_capacity\":"<<barrier_capacity
   <<",\"list_capacity\":"<<list_capacity<<",\"queue_capacity\":"<<queue_capacity<<",\"submission_capacity\":"<<submission_capacity<<",\"batch_capacity\":"<<batch_capacity
   <<",\"copy_capacity\":"<<copy_capacity<<",\"copy_resource_capacity\":"<<copy_resource_capacity
   <<",\"copy_pin_capacity\":"<<copy_pin_capacity<<",\"copy_retention\":\"per-call-until-drain\""
   <<",\"fence_capacity\":"<<fence_capacity<<",\"pin_capacity\":"<<pin_capacity<<",\"history_warmup_ms\":50,\"storage_bytes\":"<<journal_storage_bytes
   <<",\"frame_tagging\":"<<(cfg.frame_renderer!=0)<<",\"frame_capacity\":"<<frame_capacity
   <<",\"same_frame_readback\":"<<cfg.same_frame_readback<<",\"triplet_readback\":"<<cfg.triplet_readback<<",\"sequence_readback\":"<<cfg.sequence_readback<<",\"readback_slots\":"<<(cfg.triplet_readback?readback_stages():1)
   <<",\"hooks_installed\":true,\"targets\":[";
  for(unsigned i=0;i<(cfg.frame_renderer?own_count:15);++i){if(i)o<<',';o<<cfg.targets[i];}o<<"]}\n";
}
void write_result(std::ostream &o) {
  o<<std::boolalpha<<"{\"schema\":4,\"error\":"<<static_cast<unsigned>(error.load())<<",\"pending\":"<<(gate.load()&~open_bit)
   <<",\"pins\":"<<pins.load()<<",\"released\":"<<released<<",\"disabled\":"<<std::none_of(enabled.begin(),enabled.end(),[](bool b){return b;})
   <<",\"frame_started\":"<<frame_started<<",\"watch_reserved\":"<<view_count.load()<<",\"history_reserved\":"<<history_count.load()<<",\"barriers_reserved\":"<<barrier_count.load()
   <<",\"draw_return\":"<<draw_return.load()<<",\"selected_submission\":"<<selected_submission.load()<<",\"submitted_reserved\":"<<submitted_count.load()
   <<",\"completion_signal\":"<<(completion_signal_slot.load()<history_capacity?history[completion_signal_slot.load()].value.enter:0)<<",\"completion_read\":"<<completion_read.load()
   <<",\"marker\":{\"order\":"<<marker.order<<",\"bind_enter\":"<<marker.bind_enter<<",\"bind_exit\":"<<marker.bind_exit<<",\"command\":"<<marker.command<<",\"view\":"<<marker.view<<",\"resource\":"<<marker.resource
   <<",\"identity\":"<<marker.identity<<",\"handle\":"<<marker.handle<<"},\"watch\":[";
  for(unsigned i=0;i<(std::min)(view_count.load(),watch_capacity);++i){if(i)o<<',';event_json(o,views[i].value);}o<<"],\"history\":[";
  for(unsigned i=0;i<(std::min)(history_count.load(),history_capacity);++i){if(i)o<<',';event_json(o,history[i].value);}o<<"],\"barriers\":[";
  for(unsigned i=0;i<(std::min)(barrier_count.load(),barrier_capacity);++i) {
    if(i)o<<',';const auto &b=barrier_data[i];o<<'['<<b.type<<','<<b.flags<<','<<b.resource<<','<<b.other<<','<<b.subresource<<','<<b.before<<','<<b.after<<']';
  }
  o<<"],\"lists\":[";objects_json(o,lists);o<<"],\"queues\":[";objects_json(o,queues);o<<"],\"fences\":[";objects_json(o,fences);o<<"],\"submitted\":[";
  for(unsigned i=0;i<(std::min)(submitted_count.load(),submission_capacity);++i){if(i)o<<',';o<<submitted[i];}
  o<<']';copies_json(o);frame_json(o);o<<"}\n";
}
bool prepare_readback(const Runtime &r) noexcept {
  if(readback_enabled || !installed || !healthy() || !anchors() || r.device!=cfg.device)return false;
  InternalReadback internal;
  for(unsigned stage=0;stage<readback_stages();++stage)if(stage==1 || cfg.triplet_readback)
    if(!depth_pixels[stage].prepare(reinterpret_cast<ID3D12Device *>(r.device),r.width,r.height)) {for(auto &pixels:depth_pixels)pixels.abort();return false;}
  readback_enabled=true;return true;
}
bool save_readback(const std::wstring &path,unsigned stage) {
  InternalReadback internal;
  if(stage>=readback_stages() || !readback_enabled || (stage!=1 && !cfg.triplet_readback))return false;
  const auto &site=depth_pixels[stage].submitted_site();
  // Controls were signaled without inventing a selected-draw marker. Before
  // Map, require the same pinned depth and surviving writable DSV as the
  // actual stimulus draw. Offline review checks the entire lifetime interval.
  if(stage!=1 && (!healthy() || !selected_source_order(stage) || !draw_return.load() ||
     site.resource!=marker.resource || site.identity!=marker.identity || !readback_view()))return false;
  return depth_pixels[stage].save(path);
}
void write_readback(std::ostream &out,unsigned stage) {if(stage<readback_stages())depth_pixels[stage].write(out);}
#ifdef MHW_BINDING_FIXTURE
bool install_owned_watch(const Config &c) noexcept {if(used)return false;cfg=c;used=true;return anchors() && install();}
bool remove_owned_watch() noexcept {
  if(!disable())return false;bool ok=true;
  for(unsigned i=0;i<own_count;++i)if(created[i] && MH_RemoveHook(reinterpret_cast<void *>(cfg.targets[i]))!=MH_OK)ok=false;
  return ok;
}
Error owned_error() noexcept {return error.load();}
unsigned owned_pins() noexcept {return pins.load();}
unsigned owned_draw_return_calls() noexcept {return draw_return_calls.load();}
const Marker &owned_marker() noexcept {return marker;}
void owned_expire() noexcept {started=GetTickCount64()-watch_ms;}
Token owned_execute_before(const Frame &f) noexcept {return before(execute,f);}
Token owned_copy_before(unsigned kind,const Frame &f) noexcept {return before(kind,f);}
void owned_pause_copy_pin() noexcept {pause_first_copy_pin.store(true);}
bool owned_copy_pin_waiting() noexcept {return first_copy_pin_waiting.load(std::memory_order_acquire);}
void owned_resume_copy_pin() noexcept {resume_first_copy_pin.store(true,std::memory_order_release);}
void owned_change_copy_pin(unsigned kind) noexcept {
  if(kind==0)++copy_pins[2].desc.Width;
  else if(kind==1)++copy_pins[2].identity;
  else if(kind==2)++copy_pins[2].device;
  else for(unsigned i=0;i<130;++i){copy_pins[i].object=0x1000+16ULL*i;copy_pins[i].identity=0x2000+16ULL*i;}
}
bool owned_release_readback(bool reset) noexcept {bool ok=true;for(auto &pixels:depth_pixels)ok=pixels.fixture_release_after_reset(reset) && ok;return ok;}
#endif
}
