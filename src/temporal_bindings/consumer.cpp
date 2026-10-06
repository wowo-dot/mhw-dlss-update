#include "consumer.hpp"
#ifdef MHW_SCALED_OUTPUT
#include "../scaled_temporal/output_route.hpp"
#endif
#include "owned_inputs.hpp"
#include "submission.hpp"
#include "motion_readback.hpp"
#include "depth_routes.hpp"
#include "../projection_binding/memory.hpp"
#include <MinHook.h>
#include <intrin.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <ostream>
#ifdef MHW_TEMPORAL_SCALED
#include "../continuous_dlaa/native_state_admission.hpp"
#endif
#ifdef MHW_TEMPORAL_INLINE
#include "inline_pass.hpp"
#endif
#ifdef MHW_TEMPORAL_PRESENT_STATE
#include "../temporal_present/observer.hpp"
#endif
extern "C" void temporal_consumer_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::temporal_bindings::gpu {
namespace {
namespace pb=projection_binding;
using BarrierFn=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,const D3D12_RESOURCE_BARRIER *);
using DispatchFn=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,UINT,UINT);
struct Transition {unsigned type{},flags{},subresource{},before{},after{};std::uintptr_t resource{},other{};};
struct BarrierCall {unsigned first{},count{};std::uint64_t enter{},exit{};std::uintptr_t caller{};bool returned{},stable{};};
struct ResourceState {bool known{},conflict{};unsigned value{};};
struct Receipt {
  std::uintptr_t motion_identity{},color_identity{};
  Snapshot source;Packet packet;ShaderRecipe shader;
  SceneDepth depth;ResourceState depth_state;bool scene_enabled{},depth_equal{};
  std::uintptr_t executor{},context{},recording{},command{},identity{},dispatch_caller{};
#ifdef MHW_TEMPORAL_PRESENT_STATE
  std::uintptr_t binding_root{},binding_pipeline{};
#endif
  ID3D12GraphicsCommandList *held{};
  unsigned thread{},barrier_count{},barrier_calls{},dispatch_calls{},command_type{};
  unsigned predecessor{UINT_MAX},generation{1};
  std::uint64_t producer_begin{},publication{};
  std::array<Transition,barrier_limit> barriers{};
  std::array<BarrierCall,barrier_call_limit> calls{};
  std::array<ResourceState,roles> state_at_dispatch{};
  std::uint64_t enter{},exit{},enter_tick{},exit_tick{},dispatch_enter{},dispatch_exit{};
  bool valid{},returned{},dispatch_returned{},recipe_equal{},packet_equal{},dispatch_replaced{};unsigned native_dispatch_forwards{};
#ifdef MHW_TEMPORAL_SCALED
  bool color_proof_attempted{},color_proof_accepted{},color_packet_revalidated{};
  std::uint64_t color_proof_reset{};
#endif
};
struct Slot {std::atomic<unsigned> published{},claimed{};std::atomic<std::uintptr_t> active_command{};Receipt value;};
std::array<Slot,capacity> slots;
std::array<motion::Capture,capacity> pixels;
std::array<motion::Capture,capacity> colors;
bool motion_enabled{},color_enabled{};
std::array<std::uintptr_t,3> targets{};
std::array<void *,3> originals{};
std::array<bool,3> enabled{};
Config cfg;
std::atomic<bool> accepting{};
bool drained_before_disable{};
std::atomic<unsigned> pending{},fault{},consumed{},pins{},releases{};
std::atomic<std::uint64_t> order{};
bool configured{};
std::uint64_t started{};
thread_local unsigned current=UINT_MAX;
thread_local bool injected{};
void fail(unsigned n){unsigned zero{};fault.compare_exchange_strong(zero,n);accepting.store(false,std::memory_order_release);}
bool enter(){if(!accepting.load(std::memory_order_acquire))return false;pending.fetch_add(1,std::memory_order_acq_rel);
  if(!accepting.load(std::memory_order_acquire)){pending.fetch_sub(1,std::memory_order_release);return false;}return true;}
bool read(void *,std::uintptr_t p,std::span<std::byte> out){return pb::copy_memory(p,out.data(),static_cast<unsigned>(out.size()));}
const Reader reader{nullptr,read};
bool stable_depth(const Receipt &r){
  if(!r.scene_enabled)return true;SceneDepth now;
  // The frontend can already be building another packet. Only revalidate the
  // selected view's owner/backing/SRV, not its later current attachment.
  return scene_depth(reader,r.source,now) && now.container==r.depth.container && now.texture==r.depth.texture &&
    now.srv_readable==r.depth.srv_readable && now.srv==r.depth.srv && now.cpu_descriptor==r.depth.cpu_descriptor;
}
#ifdef MHW_TEMPORAL_SCALED
std::array<continuous_dlaa::NativeStateAdmission,capacity> color_admission;
bool admit_color(Receipt &r,ID3D12GraphicsCommandList *list,const submission::RecordingStamp &stamp){
  const auto &state=r.state_at_dispatch[0];
  if(r.color_proof_attempted || state.known || state.conflict || !stamp.reset || stamp.command!=r.command)return false;
  r.color_proof_attempted=true;r.color_proof_reset=stamp.reset;
  std::array<D3D12_RESOURCE_BARRIER,barrier_limit> raw{};
  for(unsigned i=0;i<r.barrier_count;++i){const auto &b=r.barriers[i];auto &out=raw[i];
    out.Type=static_cast<D3D12_RESOURCE_BARRIER_TYPE>(b.type);out.Flags=static_cast<D3D12_RESOURCE_BARRIER_FLAGS>(b.flags);
    auto resource=reinterpret_cast<ID3D12Resource *>(b.resource);
    if(b.type==0)out.Transition={resource,b.subresource,static_cast<D3D12_RESOURCE_STATES>(b.before),static_cast<D3D12_RESOURCE_STATES>(b.after)};
    else if(b.type==1)out.Aliasing={resource,reinterpret_cast<ID3D12Resource *>(b.other)};
    else out.UAV={resource};
  }
  r.color_proof_accepted=color_admission[current].attempt(reader,r.source,r.publication,list,stamp.reset,r.packet.object,
    GetTickCount64(),std::span(raw).first(r.barrier_count));
  if(!r.color_proof_accepted)return false;
  Packet packet_now;ShaderRecipe recipe_now;std::uintptr_t active{};
  r.color_packet_revalidated=healthy() && submission::healthy() && GetTickCount64()-started<2000 &&
    packet(reader,r.packet.object,packet_now) && packet_now==r.packet &&
    reader.get(r.recording+0x158,active) && active==r.packet.object &&
    shader_recipe(reader,r.source,recipe_now) && recipe_now==r.shader && stable_depth(r);
  return r.color_packet_revalidated;
}
#endif
bool checked(std::uintptr_t p,unsigned slot,std::uintptr_t expected=0){std::uintptr_t at{};return pb::system_method(p,slot,at) && (!expected || at==expected);}
bool release(IUnknown *p) noexcept {if(!p)return true;if(!checked(reinterpret_cast<std::uintptr_t>(p),2))return false;
  __try{p->Release();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool anchor(const Receipt &r,std::uintptr_t &command) {
  std::uintptr_t backend{},recording{};
  return r.executor && r.executor<=UINTPTR_MAX-0x10 && r.context && r.context<=UINTPTR_MAX-0x10 &&
    reader.get(r.context+8,backend) && backend==cfg.backend && reader.get(r.executor+8,recording) && recording &&
    recording<=UINTPTR_MAX-0x160 && (!r.recording || r.recording==recording) && reader.get(recording+0x20,command) && command;
}
bool pin_list(Receipt &r,std::uintptr_t object) noexcept {
  if(r.held)return r.command==object;
  slots[current].active_command.store(object,std::memory_order_seq_cst);
  for(unsigned i=0;i<capacity;++i)if(i!=current && slots[i].active_command.load(std::memory_order_seq_cst)==object){fail(14);return false;}
  if(!checked(object,0) || !checked(object,2) || !checked(object,7) || !checked(object,8) ||
     !checked(object,26,targets[1]) || !checked(object,14,targets[2]))return false;
  ID3D12GraphicsCommandList *list{};ID3D12Device *device{};IUnknown *identity{};bool ok=false;
  __try {
    auto p=reinterpret_cast<ID3D12GraphicsCommandList *>(object);
    if(SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&list))) && reinterpret_cast<std::uintptr_t>(list)==object &&
       SUCCEEDED(list->GetDevice(IID_PPV_ARGS(&device))) && reinterpret_cast<std::uintptr_t>(device)==cfg.device &&
       SUCCEEDED(list->QueryInterface(IID_PPV_ARGS(&identity))) && identity) {
      r.command_type=static_cast<unsigned>(list->GetType());r.identity=reinterpret_cast<std::uintptr_t>(identity);
      ok=r.command_type==D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
  }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  const bool a=release(identity),b=release(device);ok=ok && a && b;
  if(ok){r.held=list;r.command=object;pins.fetch_add(1);}else if(!release(list))fail(11);
  return ok;
}
unsigned consumer_before(NativeFrame &f) {
  if(!enter())return UINT_MAX;
  if(GetTickCount64()-started>=2000){fail(13);pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;}
  if(current!=UINT_MAX){fail(1);pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;}
  unsigned found=UINT_MAX;
  for(unsigned i=0;i<capacity;++i)if(slots[i].published.load(std::memory_order_acquire)==1 && slots[i].value.packet.object==f.call.gpr[3]) {
    if(found!=UINT_MAX){
      if constexpr(!packet_generations){fail(2);break;}
      else {
        const auto &old=slots[found];const auto &next=slots[i].value;
        if(old.claimed.load(std::memory_order_acquire)!=2 || !old.value.valid ||
           next.predecessor!=found || next.generation!=old.value.generation+1 ||
           old.value.exit>=next.producer_begin || next.producer_begin>=next.publication){fail(2);break;}
      }
    }
    found=i;
  }
  if(found==UINT_MAX || !healthy()){pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;}
  unsigned zero{};
  if(!slots[found].claimed.compare_exchange_strong(zero,1)){fail(3);pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;}
  auto &r=slots[found].value;r.executor=f.call.gpr[1];r.context=f.call.gpr[2];r.thread=GetCurrentThreadId();r.enter=++order;r.enter_tick=GetTickCount64();
  Packet p;ShaderRecipe sh;std::uintptr_t command{};
  r.valid=packet(reader,r.packet.object,p) && p==r.packet && shader_recipe(reader,r.source,sh) && sh==r.shader &&
    anchor(r,command) && reader.get(r.executor+8,r.recording) && stable_depth(r);
  if(!r.valid)fail(4);
  current=found;return found;
}
void consumer_after(unsigned i,bool returned) {
  if(i==UINT_MAX)return;
  auto &r=slots[i].value;r.returned=returned;r.exit=++order;r.exit_tick=GetTickCount64();
  Packet p;ShaderRecipe sh;std::uintptr_t command{},current_packet{};
  r.packet_equal=packet(reader,r.packet.object,p) && p==r.packet;
  r.recipe_equal=shader_recipe(reader,r.source,sh) && sh==r.shader;
  r.depth_equal=stable_depth(r);
  r.valid=r.valid && returned && r.packet_equal && r.recipe_equal && anchor(r,command) && command==r.command &&
    reader.get(r.recording+0x158,current_packet) && current_packet==r.packet.object && r.dispatch_calls==1 && r.dispatch_returned && r.depth_equal && healthy() && r.exit_tick-started<2000;
  if(!r.valid)fail(returned?5:6);
  slots[i].active_command.store(0,std::memory_order_release);slots[i].claimed.store(2,std::memory_order_release);current=UINT_MAX;
  if(consumed.fetch_add(1,std::memory_order_acq_rel)+1==capacity)accepting.store(false,std::memory_order_release);
  pending.fetch_sub(1,std::memory_order_release);
}
// A transition proves only a recording-local state. No cross-list or queue
// ordering, implicit promotion or GPU completion is inferred here.
void states(Receipt &r) {
  for(unsigned j=0;j<roles+(r.scene_enabled?1U:0U);++j)for(unsigned k=0;k<r.barrier_count;++k) {
    const auto &b=r.barriers[k];auto &s=j==roles?r.depth_state:r.state_at_dispatch[j];const auto resource=j==roles?r.depth.texture.resource:r.source.inputs[j].resource;
    if(b.type==1 && (!b.resource || !b.other || b.resource==resource || b.other==resource)){s.known=false;s.conflict=true;}
    if(b.type!=0 || b.resource!=resource)continue;
    if(b.flags || b.subresource!=UINT_MAX){s.known=false;s.conflict=true;continue;}
    if(s.known && s.value!=b.before)s.conflict=true;
    s.value=b.after;s.known=!s.conflict;
  }
}
unsigned barrier_before(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *data,std::uintptr_t caller) {
  if(current==UINT_MAX || !healthy())return UINT_MAX;
  auto &r=slots[current].value;std::uintptr_t command{};
  if(!anchor(r,command) || command!=reinterpret_cast<std::uintptr_t>(list) || !pin_list(r,command) ||
     n>barrier_limit-r.barrier_count || r.barrier_calls>=barrier_call_limit){fail(7);return UINT_MAX;}
  std::array<D3D12_RESOURCE_BARRIER,barrier_limit> raw{};
  if(n && !pb::copy_memory(reinterpret_cast<std::uintptr_t>(data),raw.data(),n*sizeof(D3D12_RESOURCE_BARRIER))){fail(7);return UINT_MAX;}
  const auto call=r.barrier_calls++;auto &c=r.calls[call];c.first=r.barrier_count;c.count=n;c.enter=++order;c.caller=caller;
  for(unsigned i=0;i<n;++i) {
    const auto &b=raw[i];auto &out=r.barriers[r.barrier_count++];out.type=b.Type;out.flags=b.Flags;
    if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)out={static_cast<unsigned>(b.Type),static_cast<unsigned>(b.Flags),b.Transition.Subresource,
      static_cast<unsigned>(b.Transition.StateBefore),static_cast<unsigned>(b.Transition.StateAfter),reinterpret_cast<std::uintptr_t>(b.Transition.pResource),0};
    else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING){out.resource=reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore);out.other=reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter);}
    else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV)out.resource=reinterpret_cast<std::uintptr_t>(b.UAV.pResource);
    else {fail(7);break;}
  }
  return call;
}
void barrier_after(unsigned call,const D3D12_RESOURCE_BARRIER *data,bool returned) {
  if(call==UINT_MAX)return;
  auto &r=slots[current].value;auto &c=r.calls[call];c.exit=++order;c.returned=returned;c.stable=returned;
  std::array<D3D12_RESOURCE_BARRIER,barrier_limit> raw{};
  if(c.count && !pb::copy_memory(reinterpret_cast<std::uintptr_t>(data),raw.data(),c.count*sizeof(D3D12_RESOURCE_BARRIER)))c.stable=false;
  for(unsigned i=0;c.stable && i<c.count;++i) {
    const auto &b=raw[i];const auto &v=r.barriers[c.first+i];c.stable=static_cast<unsigned>(b.Type)==v.type && static_cast<unsigned>(b.Flags)==v.flags;
    if(c.stable && b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)c.stable=reinterpret_cast<std::uintptr_t>(b.Transition.pResource)==v.resource &&
      b.Transition.Subresource==v.subresource && static_cast<unsigned>(b.Transition.StateBefore)==v.before && static_cast<unsigned>(b.Transition.StateAfter)==v.after;
    else if(c.stable && b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING)c.stable=reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore)==v.resource && reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter)==v.other;
    else if(c.stable && b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV)c.stable=reinterpret_cast<std::uintptr_t>(b.UAV.pResource)==v.resource;
  }
  if(!c.stable)fail(8);
}
void barrier_call(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *data,std::uintptr_t caller) {
  if(injected){reinterpret_cast<BarrierFn>(originals[1])(list,n,data);return;}
  const auto token=barrier_before(list,n,data,caller);const auto depth=depth_routes::barrier_before(list,n,data,caller);bool returned=false;
  __try{reinterpret_cast<BarrierFn>(originals[1])(list,n,data);returned=true;}
  __finally{barrier_after(token,data,returned);depth_routes::barrier_after(depth,data,returned);
#ifdef MHW_SCALED_OUTPUT
    if(returned)scaled_temporal::output_route::barriers(list,n,data);
#endif
  }
}
void WINAPI barrier_hook(ID3D12GraphicsCommandList *list,UINT n,const D3D12_RESOURCE_BARRIER *data){barrier_call(list,n,data,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
bool dispatch_before(ID3D12GraphicsCommandList *list,unsigned x,unsigned y,unsigned z,std::uintptr_t caller) {
  if(current==UINT_MAX || !healthy())return false;
  auto &r=slots[current].value;std::uintptr_t command{},current_packet{};Packet p;ShaderRecipe sh;
  r.dispatch_caller=caller;++r.dispatch_calls;r.dispatch_enter=++order;
  const bool ok=r.dispatch_calls==1 && caller==cfg.image+0x259cfca && anchor(r,command) && command==reinterpret_cast<std::uintptr_t>(list) &&
    pin_list(r,command) && reader.get(r.recording+0x158,current_packet) && current_packet==r.packet.object &&
    packet(reader,current_packet,p) && p==r.packet && packet_matches(r.source,p,x,y,z) &&
    shader_recipe(reader,r.source,sh) && sh==r.shader && stable_depth(r);
  if(ok){
    states(r);
#ifdef MHW_TEMPORAL_PRESENT_STATE
    std::uintptr_t root{},cached_root{},pipeline{};
    if(!reader.get(cfg.backend+0x2a0,root) || !reader.get(r.recording+0x118,cached_root) || root!=cached_root ||
       !reader.get(r.recording+0x108,pipeline) ||
       !temporal_present::observer::capture(current,r.source.frame,list,root,pipeline))fail(20);
    else {r.binding_root=root;r.binding_pipeline=pipeline;}
#endif
    submission::RecordingStamp stamp;
    if(!submission::mark(current,r.source.frame,r.packet.object,list,&stamp))fail(15);
    else if(motion_enabled) {
      const auto &state=r.state_at_dispatch[5];motion::Site site;
      auto motion_source=reinterpret_cast<ID3D12Resource *>(r.source.inputs[5].resource);
      auto color_source=reinterpret_cast<ID3D12Resource *>(r.source.inputs[0].resource);
      const auto &color_state=r.state_at_dispatch[0];
      bool color_known=color_state.known && !color_state.conflict && color_state.value==0x40;
#ifdef MHW_TEMPORAL_SCALED
      if(color_enabled && !color_known && state.known && !state.conflict && state.value==0xc0)
        color_known=admit_color(r,list,stamp);
#endif
      const bool color_ready=!color_enabled || (color_known &&
        r.motion_identity!=r.color_identity && motion_source!=color_source && colors[current].source_matches(color_source));
      if(!state.known || state.conflict || state.value!=0xc0 ||
         !pixels[current].source_matches(motion_source) || !color_ready ||
         !submission::copy_begin(current,r.source.inputs[5].resource,r.motion_identity,site)){fail(16);}
      else {
        const auto barrier=reinterpret_cast<motion::BarrierFn>(originals[1]);
        const bool copied=pixels[current].issue(list,motion_source,barrier,site);
        const auto end=submission::copy_end(current,copied);if(copied)pixels[current].issue_end(end);else fail(16);
        if(copied && color_enabled) {
          if(!submission::copy_begin(current,r.source.inputs[0].resource,r.color_identity,site,true)){fail(16);}
          else {
            const bool color_copied=colors[current].issue(list,color_source,barrier,site);
            const auto color_end=submission::copy_end(current,color_copied,true);
            if(color_copied)colors[current].issue_end(color_end);else fail(16);
          }
        }
      }
    }
  }else fail(9);return true;
}
void dispatch_after(bool observed,bool returned) {
  if(!observed)return;auto &r=slots[current].value;r.dispatch_exit=++order;r.dispatch_returned=returned;
  submission::mark_end(current,returned);
  if(!returned)fail(10);
}
void dispatch_call(ID3D12GraphicsCommandList *list,unsigned x,unsigned y,unsigned z,std::uintptr_t caller) {
  if(injected){reinterpret_cast<DispatchFn>(originals[2])(list,x,y,z);return;}
  const bool observed=dispatch_before(list,x,y,z,caller);bool returned=false;
  if(observed)submission::dispatch_forward(current);
  __try{
#ifdef MHW_SCALED_OUTPUT
    bool replaced=false;
    if(observed&&current==inline_pass::selected_receipt&&healthy()){
      const auto &r=slots[current].value;const auto &state=r.state_at_dispatch[4];
      if(!state.known||state.conflict||state.value!=D3D12_RESOURCE_STATE_UNORDERED_ACCESS)fail(21);
      else{injected=true;
        __try{if(!inline_pass::record(current,list,reinterpret_cast<ID3D12Resource *>(r.source.inputs[4].resource),r.source.frame,r.source.epoch,r.recording))fail(21);replaced=inline_pass::replacement_claimed();}
        __finally{injected=false;}}
    }
    if(observed)slots[current].value.dispatch_replaced=replaced;
    if(!replaced){reinterpret_cast<DispatchFn>(originals[2])(list,x,y,z);if(observed)++slots[current].value.native_dispatch_forwards;}returned=true;
#else
    reinterpret_cast<DispatchFn>(originals[2])(list,x,y,z);returned=true;
    if(observed)++slots[current].value.native_dispatch_forwards;
#ifdef MHW_TEMPORAL_INLINE
    if(observed && current==inline_pass::selected_receipt && healthy()){
      const auto &r=slots[current].value;const auto &state=r.state_at_dispatch[4];
      if(!state.known || state.conflict || state.value!=D3D12_RESOURCE_STATE_UNORDERED_ACCESS)fail(21);
      else {
        injected=true;
        __try{if(!inline_pass::record(current,list,reinterpret_cast<ID3D12Resource *>(r.source.inputs[4].resource),r.source.frame,r.source.epoch))fail(21);}
        __finally{injected=false;}
      }
    }
#endif
#endif
  }
  __finally{dispatch_after(observed,returned);}
}
void WINAPI dispatch_hook(ID3D12GraphicsCommandList *list,UINT x,UINT y,UINT z){dispatch_call(list,x,y,z,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
}
void consume(NativeFrame *f) {
  const auto token=consumer_before(*f);bool returned=false;
  __try{mhw_binding_forward(originals[0],&f->call);returned=true;}
  __finally{consumer_after(token,returned);}
}
std::uint64_t publication_begin() noexcept {return ++order;}
bool publish(unsigned i,const Snapshot &s,const Packet &p,const ShaderRecipe &sh,std::uintptr_t motion_identity,std::uintptr_t color_identity,std::uint64_t producer_begin,const SceneDepth *scene) {
  if(!configured || i>=capacity || !healthy() || !producer_begin || slots[i].published.load()!=0)return false;
  if(color_enabled && i && s.frame!=slots[i-1].value.source.frame+1){fail(17);return false;}
  unsigned predecessor=UINT_MAX,generation=1;
  for(unsigned j=0;j<i;++j)if(slots[j].published.load(std::memory_order_acquire)==1 && slots[j].value.packet.object==p.object){
    if constexpr(!packet_generations){fail(12);return false;}
    else {
      // claimed=2 releases all CPU consumer receipts. The packet is CPU queue
      // storage, not a GPU resource. Its reuse does not retire GPU work: the
      // independent submission/fence checks still qualify each captured set.
      if(slots[j].claimed.load(std::memory_order_acquire)!=2){fail(12);return false;}
      const auto &old=slots[j].value;
      if(!old.valid || !old.returned || !old.dispatch_returned || !old.exit || old.exit>=producer_begin ||
         s.frame<=old.source.frame || s.epoch<=old.source.epoch){fail(12);return false;}
      predecessor=j;generation=old.generation+1;
    }
  }
  auto &r=slots[i].value;r.source=s;r.packet=p;r.shader=sh;r.motion_identity=motion_identity;r.color_identity=color_identity;
  if(scene){r.depth=*scene;r.scene_enabled=true;}
  r.predecessor=predecessor;r.generation=generation;r.producer_begin=producer_begin;r.publication=++order;
  slots[i].published.store(1,std::memory_order_release);return true;
}
bool complete() noexcept {return consumed.load(std::memory_order_acquire)==capacity && healthy();}
void WINAPI original_barriers(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *barriers){
  reinterpret_cast<motion::BarrierFn>(originals[1])(list,n,barriers);
}
bool healthy() noexcept {return fault.load(std::memory_order_acquire)==0;}
bool settled() noexcept {return pending.load(std::memory_order_acquire)==0;}
bool prepare_motion(unsigned width,unsigned height,std::string &reason,bool with_color) {
  if(motion_enabled){reason="motion-already-prepared";return false;}motion_enabled=true;color_enabled=with_color;
  for(unsigned i=0;i<capacity;++i)if(!pixels[i].prepare(reinterpret_cast<ID3D12Device *>(cfg.device),width,height,motion::Kind::motion,owned::input(i,2))){reason="motion-allocation";return false;}
  if(color_enabled)for(unsigned i=0;i<capacity;++i)if(!colors[i].prepare(reinterpret_cast<ID3D12Device *>(cfg.device),width,height,motion::Kind::color,owned::input(i,0))){reason="color-allocation";return false;}
  submission::enable_copies(color_enabled);return true;
}
bool finish_motion(bool accepted) noexcept {
  if(!motion_enabled)return true;if(!settled())return false;
  bool ok=accepted;
  for(unsigned i=0;i<capacity;++i){motion::Completion c;if(!accepted || !submission::copy_completion(i,c) || !pixels[i].qualify(c))ok=false;}
  if(color_enabled)for(unsigned i=0;i<capacity;++i){motion::Completion c;if(!accepted || !submission::copy_completion(i,c,true) || !colors[i].qualify(c))ok=false;}
  if(!ok){for(auto &p:pixels)p.abort();for(auto &p:colors)p.abort();}return ok;
}
bool save_motion(const std::wstring &prefix) {
  if(!motion_enabled)return true;
  for(unsigned i=0;i<capacity;++i)if(!pixels[i].save(prefix+L".motion-"+std::to_wstring(i)+L".u32"))return false;
  if(color_enabled)for(unsigned i=0;i<capacity;++i)if(!colors[i].save(prefix+L".color-"+std::to_wstring(i)+L".u32"))return false;
  return true;
}
bool owned_ready() noexcept {
  if(!motion_enabled || !color_enabled || !settled())return false;
  for(unsigned i=0;i<capacity;++i)if(!pixels[i].owned_ready(owned::input(i,2)) || !colors[i].owned_ready(owned::input(i,0)))return false;
  return true;
}
void write_motion(std::ostream &o) {
  o<<"{\"enabled\":"<<motion_enabled<<",\"retention\":\"process-lifetime-after-issue\",\"copies\":[";
  if(motion_enabled)for(unsigned i=0;i<capacity;++i){if(i)o<<',';pixels[i].write(o);}o<<"]}";
}
void write_color(std::ostream &o) {
  o<<"{\"enabled\":"<<color_enabled<<",\"retention\":\"process-lifetime-after-issue\",\"copies\":[";
  if(color_enabled)for(unsigned i=0;i<capacity;++i){if(i)o<<',';colors[i].write(o);}o<<"]}";
}
bool install(const Config &c,std::string &reason) {
  if(configured){reason="consumer-already-used";return false;}cfg=c;configured=true;
#ifdef MHW_TEMPORAL_SCALED
  if(!pb::verify_code(c.image,0x228f3b0,2000,"df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373") ||
     !pb::verify_code(c.image,0x2292610,304,"4adcc8e99a9ff16ba9af27c8db165b998958561d82da0d3c9e7bcdcb82cd37f9") ||
     !pb::verify_code(c.image,0x22927f0,736,"bef3f834e34a78dce1392d047062123d7c2d5633de7390aa0216cb654519e2ed") ||
     !pb::verify_code(c.image,0x229cee0,7756,"11f7cc4d48598967f439659c87142839c4ad3fb900cfafb7fab3631655691af3") ||
     !pb::verify_code(c.image,0x24e7a10,2620,"7ea61c6f4bbb21c5d8a25d5801a92476b825595c96937ac57d1e44870caea6fb")){
    reason="native-state-contract-code";return false;
  }
#endif
  ID3D12CommandAllocator *allocator{};ID3D12GraphicsCommandList *list{};
  auto device=reinterpret_cast<ID3D12Device *>(cfg.device);
  const bool created=SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))) &&
    SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator,nullptr,IID_PPV_ARGS(&list))) && SUCCEEDED(list->Close());
  const auto object=reinterpret_cast<std::uintptr_t>(list);
  const bool found=created && pb::system_method(object,26,targets[1]) && pb::system_method(object,14,targets[2]);
  const bool a=release(list),b=release(allocator);if(!found || !a || !b){reason="consumer-api-discovery";return false;}
  targets[0]=cfg.image+0x259cc20;
  const std::array<void *,3> detours={reinterpret_cast<void *>(temporal_consumer_entry),reinterpret_cast<void *>(barrier_hook),reinterpret_cast<void *>(dispatch_hook)};
  for(unsigned i=0;i<3;++i)for(unsigned j=0;j<i;++j)if(targets[i]==targets[j]){reason="consumer-target-alias";return false;}
  for(unsigned i=0;i<3;++i) {
    const auto result=MH_CreateHook(reinterpret_cast<void *>(targets[i]),detours[i],&originals[i]);
    if(result!=MH_OK){reason="consumer-create-"+std::to_string(i)+"-"+std::to_string(result);return false;}
  }
  for(unsigned i=0;i<3;++i) {
    const auto result=MH_EnableHook(reinterpret_cast<void *>(targets[i]));
    if(result!=MH_OK){reason="consumer-enable-"+std::to_string(i)+"-"+std::to_string(result);return false;}enabled[i]=true;
  }
  return true;
}
void begin(std::uint64_t tick) noexcept {started=tick;accepting.store(true,std::memory_order_release);}
void stop() noexcept {accepting.store(false,std::memory_order_release);}
bool finish(bool drain) noexcept {
  stop();bool ok=true;
  const auto until=GetTickCount64()+2000;while(drain && !settled() && GetTickCount64()<until)Sleep(1);
  drained_before_disable=settled();
  for(unsigned i=0;i<3;++i)if(enabled[i]){const auto result=MH_DisableHook(reinterpret_cast<void *>(targets[i]));if(result==MH_OK || result==MH_ERROR_DISABLED)enabled[i]=false;else ok=false;}
  if(!drained_before_disable || !settled())return false;
  for(auto &slot:slots)if(slot.value.held){if(release(slot.value.held)){slot.value.held=nullptr;releases.fetch_add(1);}else {fail(11);ok=false;}}
  return ok && healthy() && consumed.load()==capacity && pins.load()==releases.load();
}
void write(std::ostream &o) {
  o<<"{\"started_tick\":"<<started<<",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"completed\":"<<consumed.load()<<",\"pins\":"<<pins.load()<<",\"released\":"<<releases.load()
   <<",\"hooks_disabled\":"<<(!enabled[0] && !enabled[1] && !enabled[2])<<",\"drained_before_disable\":"<<drained_before_disable<<",\"gpu_submission_verified\":false,\"records\":[";
  bool comma=false;
  for(unsigned i=0;i<capacity;++i)if(slots[i].published.load()==1) {
    if(comma)o<<',';comma=true;const auto &r=slots[i].value;
    o<<"{\"receipt\":"<<i<<",\"frame\":"<<r.source.frame<<",\"packet\":"<<r.packet.object<<",\"valid\":"<<r.valid<<",\"returned\":"<<r.returned
     <<",\"producer_begin\":"<<r.producer_begin<<",\"publication\":"<<r.publication<<",\"packet_predecessor\":"<<r.predecessor<<",\"packet_generation\":"<<r.generation
     <<",\"packet_equal\":"<<r.packet_equal<<",\"recipe_equal\":"<<r.recipe_equal<<",\"executor\":"<<r.executor<<",\"context\":"<<r.context<<",\"recording\":"<<r.recording
     <<",\"command\":"<<r.command<<",\"identity\":"<<r.identity<<",\"command_type\":"<<r.command_type<<",\"thread\":"<<r.thread<<",\"enter\":"<<r.enter<<",\"exit\":"<<r.exit
     <<",\"enter_tick\":"<<r.enter_tick<<",\"exit_tick\":"<<r.exit_tick
     <<",\"dispatch_caller\":"<<r.dispatch_caller<<",\"dispatch_calls\":"<<r.dispatch_calls<<",\"dispatch_enter\":"<<r.dispatch_enter<<",\"dispatch_exit\":"<<r.dispatch_exit
     <<",\"dispatch_returned\":"<<r.dispatch_returned<<",\"dispatch_replaced\":"<<r.dispatch_replaced<<",\"native_dispatch_forwards\":"<<r.native_dispatch_forwards<<",\"states\":[";
    for(unsigned j=0;j<roles;++j){if(j)o<<',';const auto &s=r.state_at_dispatch[j];o<<"{\"known\":"<<s.known<<",\"conflict\":"<<s.conflict<<",\"value\":"<<s.value<<'}';}
    o<<"],\"barrier_calls\":[";
    for(unsigned j=0;j<r.barrier_calls;++j){if(j)o<<',';const auto &c=r.calls[j];o<<"{\"first\":"<<c.first<<",\"count\":"<<c.count<<",\"enter\":"<<c.enter<<",\"exit\":"<<c.exit
      <<",\"caller\":"<<c.caller<<",\"returned\":"<<c.returned<<",\"stable\":"<<c.stable<<'}';}
    o<<"],\"barriers\":[";
    for(unsigned j=0;j<r.barrier_count;++j){if(j)o<<',';const auto &b=r.barriers[j];o<<"{\"type\":"<<b.type<<",\"flags\":"<<b.flags<<",\"subresource\":"<<b.subresource
      <<",\"before\":"<<b.before<<",\"after\":"<<b.after<<",\"resource\":"<<b.resource<<",\"other\":"<<b.other<<'}';}
    o<<']';if(r.scene_enabled){const auto &s=r.depth_state;
      o<<",\"scene_depth\":{\"equal\":"<<r.depth_equal<<",\"resource\":"<<r.depth.texture.resource
       <<",\"state\":{\"known\":"<<s.known<<",\"conflict\":"<<s.conflict<<",\"value\":"<<s.value<<"}}";}
#ifdef MHW_TEMPORAL_PRESENT_STATE
    o<<",\"binding_root\":"<<r.binding_root<<",\"binding_pipeline\":"<<r.binding_pipeline;
#endif
#ifdef MHW_TEMPORAL_SCALED
    o<<",\"color_admission\":{\"attempted\":"<<r.color_proof_attempted<<",\"accepted\":"<<r.color_proof_accepted
     <<",\"packet_revalidated\":"<<r.color_packet_revalidated<<",\"reset\":"<<r.color_proof_reset<<",\"state_admission\":";
    color_admission[i].write(o);o<<'}';
#endif
    o<<'}';
  }o<<"]}";
}
#ifdef MHW_TEMPORAL_FIXTURE
bool fixture_republish(unsigned old_index,unsigned index,std::uint64_t frame,std::uint64_t epoch,bool overlap) {
  if(old_index>=capacity || slots[old_index].published.load()!=1)return false;
  const auto &r=slots[old_index].value;auto source=r.source;source.frame=frame;source.epoch=epoch;
  return publish(index,source,r.packet,r.shader,r.motion_identity,r.color_identity,overlap?r.publication:publication_begin(),r.scene_enabled?&r.depth:nullptr);
}
void fixture_start(const Config &c,ID3D12GraphicsCommandList *list,void *consumer,void *barrier,void *dispatch) {
  cfg=c;configured=true;originals={consumer,barrier,dispatch};
  pb::system_method(reinterpret_cast<std::uintptr_t>(list),26,targets[1]);pb::system_method(reinterpret_cast<std::uintptr_t>(list),14,targets[2]);
}
void fixture_barrier(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *data){barrier_call(list,n,data,0x1234);}
void fixture_dispatch(ID3D12GraphicsCommandList *list,unsigned x,unsigned y,unsigned z,std::uintptr_t caller){dispatch_call(list,x,y,z,caller);}
bool fixture_release_motion(bool safe) noexcept {bool ok=true;for(auto &p:pixels)ok=p.fixture_release_after_reset(safe) && ok;for(auto &p:colors)ok=p.fixture_release_after_reset(safe) && ok;return ok;}
void fixture_motion_map_failure() noexcept {pixels[0].fixture_map_failure();}
void fixture_color_map_failure() noexcept {colors[0].fixture_map_failure();}
#endif
}
extern "C" void mhw_temporal_consume(mhw::temporal_bindings::NativeFrame *f){mhw::temporal_bindings::gpu::consume(f);}
