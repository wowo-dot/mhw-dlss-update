#include "depth_routes.hpp"
#ifdef MHW_SCALED_OUTPUT
#include "../scaled_temporal/draw_execution.hpp"
#endif
#include "owned_inputs.hpp"
#include "submission.hpp"
#include "consumer.hpp"
#include "jitter.hpp"
#include "../projection_binding/memory.hpp"
#include <MinHook.h>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <ostream>
#include <intrin.h>
extern "C" void temporal_depth_producer_entry();
extern "C" void temporal_depth_consumer_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::temporal_bindings::depth_routes {
namespace {
namespace pb=projection_binding;
using CopyFn=void(WINAPI *)(ID3D12GraphicsCommandList *,ID3D12Resource *,ID3D12Resource *);
struct Request {
  Stamp stamp;Owner source,destination;CopyPacket packet;
  std::uintptr_t caller{},object{},before_array{},after_array{};
  unsigned thread{},before_count{},after_count{};std::array<unsigned,6> rectangle{};
  std::uint64_t enter{},exit{};bool valid{},returned{};
};
struct RequestSlot {std::atomic<unsigned> published{};Request value;};
struct Resource {ID3D12Resource *held{};std::uintptr_t identity{};D3D12_RESOURCE_DESC desc{};};
struct BarrierCall {unsigned first{},count{};std::uint64_t enter{},exit{};std::uintptr_t caller{};bool returned{},stable{};};
struct DrawSnapshot {
  std::uintptr_t nested{},recorded_packet{},pipeline{},rectangle_owner{};
  std::array<std::uintptr_t,4> bindings{};std::array<unsigned,2> arguments{};
  std::array<unsigned,4> rectangle{};
  bool operator==(const DrawSnapshot &)const=default;
};
struct Consumer {
  unsigned receipt{},thread{},marker{UINT_MAX},native_calls{},barrier_count{},call_count{};
  submission::MarkerFailure marker_failure{submission::MarkerFailure::none};
  std::uintptr_t executor{},context{},recording{},command{},identity{},caller{},packet{};
  ID3D12GraphicsCommandList *held{};Resource source,destination;
  std::array<D3D12_RESOURCE_BARRIER,barrier_limit> barriers{};
  std::array<BarrierCall,call_limit> calls{};
  std::uint64_t enter{},exit{},copy{},copy_exit{};bool valid{},returned{},copy_returned{};
  unsigned pixel_receipt{UINT_MAX};std::uint64_t pixel_begin{},pixel_end{};
  bool draw{};DrawSnapshot draw_before{},draw_after{};std::array<unsigned,4> draw_arguments{};
};
struct Selection {std::uint64_t order{};Stamp stamp;std::uintptr_t owner{},resource{},packet{};};
#ifdef MHW_TEMPORAL_SCALED
constexpr unsigned hook_count=4;
#else
constexpr unsigned hook_count=3;
#endif
Config cfg;std::array<std::uintptr_t,hook_count> targets{};std::array<void *,hook_count> originals{};std::array<bool,hook_count> enabled{};
std::array<RequestSlot,limit> requests;std::array<Consumer,limit> consumers;std::array<Selection,capacity> selections;
std::array<std::atomic<bool>,capacity> selected{},pixel_claimed{};
std::array<motion::Capture,capacity> pixels;bool pixels_enabled{};
std::atomic<unsigned> request_count{},consumer_count{},pending{},fault{},pins{},released{},unmatched{};
std::atomic<std::uint64_t> order{};std::atomic<bool> accepting{};std::atomic_flag producer_busy=ATOMIC_FLAG_INIT;
std::uint64_t started{};bool configured{},drained_before_disable{};thread_local unsigned current=UINT_MAX;
bool projection_gate_required{},projection_gate_open{};unsigned projection_gate_waits{};Stamp projection_gate_stamp;
#ifdef MHW_TEMPORAL_FIXTURE
void (*before_marker)(){};
#endif
bool read(void *,std::uintptr_t p,std::span<std::byte> out){return pb::copy_memory(p,out.data(),static_cast<unsigned>(out.size()));}
const Reader reader{nullptr,read};
bool pointer(std::uintptr_t p,std::uintptr_t n){return p && p<=UINTPTR_MAX-n;}
void fail(unsigned n){unsigned zero{};fault.compare_exchange_strong(zero,n);accepting.store(false,std::memory_order_release);}
bool enter(){if(!accepting.load(std::memory_order_acquire))return false;pending.fetch_add(1);
  if(!accepting.load(std::memory_order_acquire)){pending.fetch_sub(1);return false;}return true;}
bool checked(std::uintptr_t p,unsigned slot,std::uintptr_t expected=0){std::uintptr_t at{};return pb::system_method(p,slot,at) && (!expected || at==expected);}
bool drop(IUnknown *p) noexcept {if(!p)return true;if(!checked(reinterpret_cast<std::uintptr_t>(p),2))return false;
  __try{p->Release();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<class T>bool pin(std::uintptr_t object,T **out,std::uintptr_t &identity) noexcept {
  if(!checked(object,0) || !checked(object,2) || !checked(object,7))return false;
  T *held{};ID3D12Device *device{};IUnknown *id{};bool ok=false;
  __try {auto p=reinterpret_cast<T *>(object);
    ok=SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&held))) && reinterpret_cast<std::uintptr_t>(held)==object &&
      SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&device))) && reinterpret_cast<std::uintptr_t>(device)==cfg.device &&
      SUCCEEDED(held->QueryInterface(IID_PPV_ARGS(&id))) && id;
    if(ok)identity=reinterpret_cast<std::uintptr_t>(id);
  }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  const bool a=drop(id),b=drop(device);ok=ok && a && b;
  if(ok){*out=held;pins.fetch_add(1);}else if(!drop(held))fail(1);return ok;
}
bool resource(std::uintptr_t object,Resource &r) noexcept {
  if(!checked(object,10) || !pin(object,&r.held,r.identity))return false;
  __try {r.desc=r.held->GetDesc();}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
  const auto &d=r.desc;return d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width && d.Width<=16384 && d.Height && d.Height<=16384 &&
    d.MipLevels==1 && d.DepthOrArraySize==1 && d.SampleDesc.Count==1 && !d.SampleDesc.Quality;
}
bool list(Consumer &r) noexcept {
#ifdef MHW_TEMPORAL_SCALED
  if(targets[3] && !checked(r.command,12,targets[3]))return false;
#endif
  if(!checked(r.command,8) || !checked(r.command,17,targets[2]) || !pin(r.command,&r.held,r.identity))return false;
  __try{return r.held->GetType()==D3D12_COMMAND_LIST_TYPE_DIRECT;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool anchor(const Consumer &r){
  std::uintptr_t backend{},recording{},command{};
  return pointer(r.context,16) && pointer(r.executor,16) && reader.get(r.context+8,backend) && backend==cfg.backend &&
    reader.get(r.executor+8,recording) && recording==r.recording && pointer(recording,0x28) &&
    reader.get(recording+0x20,command) && command==r.command;
}
bool stable(const Consumer &r){
  const auto &q=requests[r.receipt].value;CopyPacket p;Owner s,d;
  return anchor(r) && packet(reader,q.object,p) && p==q.packet && read_owner(reader,p.source,s) && s==q.source &&
    read_owner(reader,p.destination,d) && d==q.destination;
}
unsigned producer_before(NativeFrame &f){
  if(!accepting.load(std::memory_order_acquire) || !submission::warmed() || f.call.gpr[1]!=cfg.renderer+0x710 || static_cast<unsigned>(f.call.stack[0])!=8)return UINT_MAX;
  Stamp s;std::uintptr_t destination{};
  if(!stamp(reader,cfg,s) || !reader.get(s.container+0xa0,destination) || destination!=f.call.gpr[4])return UINT_MAX;
  std::array<unsigned,4> rect{};
  if(!reader.get(f.call.gpr[2],rect) || rect[0]>=rect[2] || rect[1]>=rect[3])return UINT_MAX;
  if(!enter())return UINT_MAX;
  if(producer_busy.test_and_set(std::memory_order_acquire)){fail(2);pending.fetch_sub(1);return UINT_MAX;}
  if(projection_gate_required && !projection_gate_open){
    Snapshot baseline;baseline.context=s.context;baseline.view=s.view;baseline.frame=s.frame;
    baseline.epoch=s.epoch;baseline.bank=s.bank;baseline.async=s.async;
    if(!jitter::matches(0,baseline)){
      ++projection_gate_waits;producer_busy.clear(std::memory_order_release);pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;
    }
    // Start once, before any receipt exists. After opening, observe every
    // producer as before so reused packet storage cannot hide a newer write.
    projection_gate_stamp=s;projection_gate_open=true;
  }
  const auto i=request_count.fetch_add(1);if(i>=limit || GetTickCount64()-started>=2000){fail(3);producer_busy.clear();pending.fetch_sub(1);return UINT_MAX;}
  auto &q=requests[i].value;q.stamp=s;q.enter=++order;q.thread=GetCurrentThreadId();q.caller=f.call.caller;
  std::copy(rect.begin(),rect.end(),q.rectangle.begin());std::array<unsigned,2> xy{};std::uintptr_t attachment{},source{};
  q.valid=reader.get(f.call.gpr[3],xy) && reader.get(s.context+0x50,attachment) && pointer(attachment,0x60) &&
    reader.get(attachment+0x58,source) && source!=destination && read_owner(reader,source,q.source) && read_owner(reader,destination,q.destination) &&
    reader.get(s.context+0x2b80,q.before_count) && q.before_count<65536 && reader.get(s.context+0x2b78,q.before_array) && q.before_array;
  q.rectangle[4]=xy[0];q.rectangle[5]=xy[1];
  if(!q.valid)fail(4);return i;
}
void producer_after(unsigned i,bool returned){
  if(i==UINT_MAX)return;auto &q=requests[i].value;q.returned=returned;Stamp s;Owner source,destination;
  q.valid=q.valid && returned && stamp(reader,cfg,s) && s==q.stamp &&
    read_owner(reader,q.source.object,source) && source==q.source && read_owner(reader,q.destination.object,destination) && destination==q.destination &&
    reader.get(s.context+0x2b80,q.after_count) && q.after_count>q.before_count && q.after_count<=65536 && q.after_count-q.before_count<=64 &&
    reader.get(s.context+0x2b78,q.after_array) && pointer(q.after_array,16ULL*q.after_count) &&
    reader.get(q.after_array+16ULL*(q.after_count-1)+8,q.object) && packet(reader,q.object,q.packet) &&
    q.packet.source==q.source.object && q.packet.destination==q.destination.object && q.packet.rectangle==q.rectangle;
  unsigned count{};std::uintptr_t array{},object{};CopyPacket p;
  q.valid=q.valid && reader.get(s.context+0x2b80,count) && count==q.after_count && reader.get(s.context+0x2b78,array) && array==q.after_array &&
    reader.get(array+16ULL*(count-1)+8,object) && object==q.object && packet(reader,object,p) && p==q.packet;
  q.valid=q.valid && GetTickCount64()-started<2000;q.exit=++order;if(!q.valid)fail(returned?5:6);requests[i].published.store(1,std::memory_order_release);
  producer_busy.clear(std::memory_order_release);pending.fetch_sub(1,std::memory_order_release);
}
unsigned consumer_before(NativeFrame &f){
  if(!accepting.load(std::memory_order_acquire))return UINT_MAX;CopyPacket p;
  // Publication in progress cannot qualify an older receipt at reused storage.
  if(producer_busy.test(std::memory_order_acquire)){unmatched.fetch_add(1);return UINT_MAX;}
  if(!packet(reader,f.call.gpr[3],p))return UINT_MAX;
  unsigned found=UINT_MAX;const auto n=(std::min)(request_count.load(),limit);
  for(unsigned i=0;i<n;++i)if(requests[i].published.load(std::memory_order_acquire)==1 && requests[i].value.object==f.call.gpr[3] &&
      (found==UINT_MAX || requests[i].value.exit>requests[found].value.exit))found=i;
  if(found==UINT_MAX || producer_busy.test(std::memory_order_acquire)){unmatched.fetch_add(1);return UINT_MAX;}
  if(!enter())return UINT_MAX;
  if(current!=UINT_MAX){fail(7);pending.fetch_sub(1);return UINT_MAX;}
  const auto i=consumer_count.fetch_add(1);if(i>=limit || GetTickCount64()-started>=2000){fail(3);pending.fetch_sub(1);return UINT_MAX;}
  auto &r=consumers[i];const auto &q=requests[found].value;r.receipt=found;r.enter=++order;r.thread=GetCurrentThreadId();
  r.executor=f.call.gpr[1];r.context=f.call.gpr[2];r.packet=f.call.gpr[3];
  r.valid=q.valid && p==q.packet && pointer(r.executor,16) && reader.get(r.executor+8,r.recording) && pointer(r.recording,0x28) &&
    reader.get(r.recording+0x20,r.command) && anchor(r) && list(r) &&
    resource(q.source.resource,r.source) && resource(q.destination.resource,r.destination) &&
    q.source.resource!=q.destination.resource && r.source.identity!=r.destination.identity && stable(r);
#ifdef MHW_TEMPORAL_FIXTURE
  if(r.valid && before_marker)before_marker();
#endif
  if(r.valid){
    r.marker=submission::native_copy_begin(r.held,r.marker_failure);
    if(r.marker==UINT_MAX){r.valid=false;fail(16);}
  }
  if(!r.valid)fail(8);current=i;return i;
}
void consumer_after(unsigned i,bool returned){
  if(i==UINT_MAX)return;auto &r=consumers[i];r.returned=returned;
  r.valid=r.valid && returned && r.native_calls==1 && r.copy_returned && stable(r) && GetTickCount64()-started<2000;r.exit=++order;
  submission::native_copy_end(r.marker,r.valid);if(!r.valid)fail(returned?9:10);current=UINT_MAX;pending.fetch_sub(1,std::memory_order_release);
}
bool copy_before(ID3D12GraphicsCommandList *command,ID3D12Resource *destination,ID3D12Resource *source,std::uintptr_t caller){
  if(current==UINT_MAX || !healthy())return false;auto &r=consumers[current];const auto &q=requests[r.receipt].value;
  ++r.native_calls;r.copy=++order;r.caller=caller;
  const bool ok=r.native_calls==1 && reinterpret_cast<std::uintptr_t>(command)==r.command && caller==cfg.image+0x259e100 &&
    reinterpret_cast<std::uintptr_t>(source)==q.source.resource && reinterpret_cast<std::uintptr_t>(destination)==q.destination.resource && stable(r);
  if(!ok){r.valid=false;fail(11);}return true;
}
void readback_after(Consumer &r){
  if(!pixels_enabled || !healthy() || !r.valid)return;const auto &q=requests[r.receipt].value;
  const auto barriers=std::span(r.barriers.data(),r.barrier_count);
  if(!(r.draw?draw_states(barriers,q.source.resource,q.destination.resource):copy_states(barriers,q.source.resource,q.destination.resource)))return;
  for(unsigned i=0;i<capacity;++i)if(selected[i].load(std::memory_order_acquire)){
    const auto &s=selections[i];if(s.stamp!=q.stamp || s.owner!=q.destination.object || s.resource!=q.destination.resource)continue;
    if(r.draw){
      // Scaled rendering has several depth draws before TAA. Select the last
      // matching published producer before that TAA request, then still require
      // the exact TAA list/generation in submission::mark and completion.
      unsigned predecessor=UINT_MAX;std::uint64_t latest{};
      for(unsigned j=0;j<(std::min)(request_count.load(),limit);++j)if(requests[j].published.load(std::memory_order_acquire)==1){
        const auto &candidate=requests[j].value;
        if(candidate.valid && candidate.stamp==s.stamp && candidate.destination.object==s.owner &&
           candidate.destination.resource==s.resource && candidate.exit<s.order && candidate.exit>latest){predecessor=j;latest=candidate.exit;}
      }
      if(predecessor!=r.receipt)continue;
    }
    if(pixel_claimed[i].exchange(true) || !stable(r) || !pixels[i].source_matches(r.destination.held)){fail(15);return;}
    motion::Site site;if(!submission::depth_begin(i,r.marker,s.stamp.frame,s.packet,s.resource,r.destination.identity,site)){fail(15);return;}
    r.pixel_receipt=i;r.pixel_begin=++order;
    const bool ok=r.draw?pixels[i].issue_depth_draw(r.held,r.destination.held,gpu::original_barriers,site):
      pixels[i].issue(r.held,r.destination.held,gpu::original_barriers,site);
    pixels[i].issue_end(submission::depth_end(i,ok));r.pixel_end=++order;if(!ok)fail(15);return;
  }
}
void copy_after(bool observed,bool returned){
  if(!observed)return;auto &r=consumers[current];r.copy_exit=++order;r.copy_returned=returned;
  if(!returned){fail(12);return;}readback_after(r);
}
#ifdef MHW_TEMPORAL_SCALED
#include "depth_draw.inl"
#endif
void copy_call(ID3D12GraphicsCommandList *command,ID3D12Resource *destination,ID3D12Resource *source,std::uintptr_t caller){
  const bool observed=copy_before(command,destination,source,caller);bool returned=false;
  __try{reinterpret_cast<CopyFn>(originals[2])(command,destination,source);returned=true;}__finally{copy_after(observed,returned);}
}
void WINAPI copy_hook(ID3D12GraphicsCommandList *l,ID3D12Resource *d,ID3D12Resource *s){copy_call(l,d,s,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
}
unsigned barrier_before(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *data,std::uintptr_t caller){
  if(current==UINT_MAX || !healthy())return UINT_MAX;auto &r=consumers[current];
  if(reinterpret_cast<std::uintptr_t>(list)!=r.command || !anchor(r) || r.call_count>=call_limit || n>barrier_limit-r.barrier_count){fail(13);return UINT_MAX;}
  const auto i=r.call_count++;auto &b=r.calls[i];b.first=r.barrier_count;b.count=n;b.enter=++order;b.caller=caller;
  if(n && !pb::copy_memory(reinterpret_cast<std::uintptr_t>(data),r.barriers.data()+r.barrier_count,n*sizeof(*data))){fail(13);return UINT_MAX;}
  r.barrier_count+=n;return i;
}
void barrier_after(unsigned i,const D3D12_RESOURCE_BARRIER *data,bool returned){
  if(i==UINT_MAX)return;auto &r=consumers[current];auto &b=r.calls[i];b.exit=++order;b.returned=returned;
  std::array<D3D12_RESOURCE_BARRIER,barrier_limit> again{};
  b.stable=returned && (!b.count || (pb::copy_memory(reinterpret_cast<std::uintptr_t>(data),again.data(),b.count*sizeof(*data)) &&
    std::memcmp(again.data(),r.barriers.data()+b.first,b.count*sizeof(*data))==0));
  if(!b.stable)fail(14);
}
void produce(NativeFrame *f){const auto i=producer_before(*f);bool returned=false;
  __try{mhw_binding_forward(originals[0],&f->call);returned=true;}__finally{producer_after(i,returned);}}
void consume(NativeFrame *f){const auto i=consumer_before(*f);bool returned=false;
  __try{mhw_binding_forward(originals[1],&f->call);returned=true;}__finally{consumer_after(i,returned);}}
bool wants() noexcept {
  if(!configured)return true;Stamp s;if(!healthy() || !stamp(reader,cfg,s))return false;
  for(unsigned i=0;i<(std::min)(request_count.load(),limit);++i)if(requests[i].published.load(std::memory_order_acquire)==1 && requests[i].value.valid && requests[i].value.stamp==s)return true;
  return false;
}
bool select(unsigned i,const Snapshot &s,const SceneDepth &d,const Packet &p) noexcept {
  if(!configured)return true;if(i>=capacity || selections[i].order || !healthy())return false;
  auto &v=selections[i];v.stamp={s.context,s.view,d.container,s.frame,s.epoch,s.bank,s.async};v.owner=d.texture.object;v.resource=d.texture.resource;v.packet=p.object;v.order=++order;selected[i].store(true,std::memory_order_release);return true;
}
bool prepare_pixels(unsigned w,unsigned h,std::string &reason){
  if(!configured || pixels_enabled){reason="depth-pixels-setup";return false;}pixels_enabled=true;
  for(unsigned i=0;i<capacity;++i)if(!pixels[i].prepare(reinterpret_cast<ID3D12Device *>(cfg.device),w,h,motion::Kind::depth,owned::input(i,1))){reason="depth-pixels-allocation";return false;}
  submission::enable_depth_copies();return true;
}
bool finish_pixels(bool accepted) noexcept {
  if(!pixels_enabled)return true;bool ok=accepted;
  for(unsigned i=0;i<capacity;++i){motion::Completion c;if(!accepted || !submission::depth_completion(i,c) || !pixels[i].qualify(c))ok=false;}
  if(!ok)for(auto &p:pixels)p.abort();return ok;
}
bool save_pixels(const std::wstring &prefix){
  if(!pixels_enabled)return true;for(unsigned i=0;i<capacity;++i)if(!pixels[i].save(prefix+L".depth-"+std::to_wstring(i)+L".f32"))return false;return true;
}
bool owned_ready() noexcept {
  if(!pixels_enabled || !settled())return false;
  for(unsigned i=0;i<capacity;++i)if(!pixels[i].owned_ready(owned::input(i,1)))return false;
  return true;
}
void write_pixels(std::ostream &o){
  o<<"{\"enabled\":"<<pixels_enabled<<",\"retention\":\"process-lifetime-after-issue\",\"copies\":[";
  if(pixels_enabled)for(unsigned i=0;i<capacity;++i){if(i)o<<',';pixels[i].write(o);}o<<"]}";
}
bool install(const Config &c,std::string &reason,bool wait_for_projection){
  projection_gate_required=wait_for_projection;
  if(configured){reason="depth-routes-already-used";return false;}configured=true;cfg=c;
  if(!pb::verify_code(c.image,0x23cd170,1711,"8103c87804f325a15bbeb3529f3b575cc14e3bae986156d53e1b796fbbec6d31") ||
     !pb::verify_code(c.image,0x259df30,681,"e84ae8742c4a0ef334692595bb89c04c8be80306d4416c20304566cc1e5dd85a")){reason="depth-routes-code";return false;}
  ID3D12CommandAllocator *a{};ID3D12GraphicsCommandList *l{};auto device=reinterpret_cast<ID3D12Device *>(cfg.device);
  const bool created=SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a))) &&
    SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,a,nullptr,IID_PPV_ARGS(&l))) && SUCCEEDED(l->Close());
  bool got=created && pb::system_method(reinterpret_cast<std::uintptr_t>(l),17,targets[2]);
#ifdef MHW_TEMPORAL_SCALED
  got=got && pb::system_method(reinterpret_cast<std::uintptr_t>(l),12,targets[3]) &&
    pb::verify_code(c.image,0x259d670,206,"d6b0b80a37a8fadba69d215cfcebd960c74f8d3554e9375b03ac7785d4f6ee39");
#endif
  const bool x=drop(l),y=drop(a);if(!got || !x || !y){reason="depth-routes-api";return false;}
  targets[0]=c.image+0x23cd170;targets[1]=c.image+0x259df30;
  const std::array<void *,hook_count> detours={reinterpret_cast<void *>(temporal_depth_producer_entry),reinterpret_cast<void *>(temporal_depth_consumer_entry),reinterpret_cast<void *>(copy_hook)
#ifdef MHW_TEMPORAL_SCALED
    ,reinterpret_cast<void *>(draw_hook)
#endif
  };
  for(unsigned i=0;i<hook_count;++i){const auto status=MH_CreateHook(reinterpret_cast<void *>(targets[i]),detours[i],&originals[i]);
    if(status!=MH_OK){reason="depth-routes-create-"+std::to_string(i)+"-"+std::to_string(status);return false;}}
  for(unsigned i=0;i<hook_count;++i){const auto status=MH_EnableHook(reinterpret_cast<void *>(targets[i]));
    if(status!=MH_OK){reason="depth-routes-enable-"+std::to_string(i)+"-"+std::to_string(status);return false;}enabled[i]=true;}return true;
}
void begin(std::uint64_t tick) noexcept {started=tick;accepting.store(true,std::memory_order_release);}
void stop() noexcept {accepting.store(false,std::memory_order_release);}
bool healthy() noexcept {return fault.load(std::memory_order_acquire)==0;}
bool settled() noexcept {return pending.load(std::memory_order_acquire)==0;}
bool finish(bool drain) noexcept {
  if(!configured)return true;stop();bool ok=true;
  // Copy/barrier interception must remain installed while an admitted native
  // consumer can still call it. Submission admission closes after this drain.
  const auto until=GetTickCount64()+2000;while(drain && !settled() && GetTickCount64()<until)Sleep(1);
  drained_before_disable=settled();
  for(unsigned i=0;i<hook_count;++i)if(enabled[i]){const auto r=MH_DisableHook(reinterpret_cast<void *>(targets[i]));if(r==MH_OK || r==MH_ERROR_DISABLED)enabled[i]=false;else ok=false;}
  if(!drained_before_disable || !settled())return false;
  for(auto &r:consumers){for(auto v:{&r.source,&r.destination})if(v->held){if(drop(v->held)){v->held=nullptr;released.fetch_add(1);}else {fail(1);ok=false;}}
    if(r.held){if(drop(r.held)){r.held=nullptr;released.fetch_add(1);}else {fail(1);ok=false;}}}
  for(const auto &s:selections){bool found=false;
    for(unsigned i=0;i<(std::min)(consumer_count.load(),limit);++i){const auto &r=consumers[i];const auto &q=requests[r.receipt].value;
      if(s.order && r.valid && r.marker!=UINT_MAX && q.stamp==s.stamp && q.destination.object==s.owner && q.destination.resource==s.resource && q.exit<s.order)found=true;}
    ok=ok && found;}
  return ok && healthy() && pins.load()==released.load();
}
void write(std::ostream &o){
  auto st=[&](const Stamp &s){o<<"\"context\":"<<s.context<<",\"view\":"<<s.view<<",\"container\":"<<s.container<<",\"frame\":"<<s.frame<<",\"epoch\":"<<s.epoch<<",\"bank\":"<<s.bank<<",\"async\":"<<s.async;};
  auto owner=[&](const Owner &v){o<<"{\"owner\":"<<v.object<<",\"type\":"<<v.type<<",\"primary\":"<<v.primary<<",\"secondary\":"<<v.secondary<<",\"backing\":"<<v.backing<<",\"resource\":"<<v.resource<<'}';};
  auto res=[&](const Resource &v){const auto &d=v.desc;o<<"{\"identity\":"<<v.identity<<",\"dimension\":"<<d.Dimension<<",\"width\":"<<d.Width<<",\"height\":"<<d.Height<<",\"mips\":"<<d.MipLevels<<",\"array_size\":"<<d.DepthOrArraySize<<",\"sample_count\":"<<d.SampleDesc.Count<<",\"sample_quality\":"<<d.SampleDesc.Quality<<",\"format\":"<<d.Format<<",\"flags\":"<<d.Flags<<",\"layout\":"<<d.Layout<<'}';};
  o<<"{\"started_tick\":"<<started<<",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"pins\":"<<pins.load()<<",\"released\":"<<released.load()<<",\"hooks_disabled\":"<<std::none_of(enabled.begin(),enabled.end(),[](bool x){return x;})<<",\"drained_before_disable\":"<<drained_before_disable<<",\"unmatched_consumers\":"<<unmatched.load()<<",\"selections\":[";
  for(unsigned i=0;i<capacity;++i){if(i)o<<',';const auto &v=selections[i];o<<'{';st(v.stamp);o<<",\"order\":"<<v.order<<",\"owner\":"<<v.owner<<",\"resource\":"<<v.resource<<",\"packet\":"<<v.packet<<'}';}
  o<<"],\"projection_gate\":{\"required\":"<<projection_gate_required<<",\"opened\":"<<projection_gate_open<<",\"waits\":"<<projection_gate_waits<<',';
  st(projection_gate_stamp);o<<"},\"requests\":[";
  for(unsigned i=0;i<(std::min)(request_count.load(),limit);++i){if(i)o<<',';const auto &q=requests[i].value;o<<'{';st(q.stamp);
    o<<",\"valid\":"<<q.valid<<",\"returned\":"<<q.returned<<",\"thread\":"<<q.thread<<",\"caller\":"<<q.caller<<",\"enter\":"<<q.enter<<",\"exit\":"<<q.exit<<",\"packet\":"<<q.object<<",\"nested\":"<<q.packet.nested<<",\"opcode\":"<<q.packet.opcode<<",\"kind\":"<<q.packet.kind
     <<",\"before_count\":"<<q.before_count<<",\"after_count\":"<<q.after_count<<",\"before_array\":"<<q.before_array<<",\"after_array\":"<<q.after_array<<",\"rectangle\":[";
    for(unsigned j=0;j<6;++j){if(j)o<<',';o<<q.rectangle[j];}o<<"],\"source\":";owner(q.source);o<<",\"destination\":";owner(q.destination);o<<'}';}
  o<<"],\"consumers\":[";
  for(unsigned i=0;i<(std::min)(consumer_count.load(),limit);++i){if(i)o<<',';const auto &r=consumers[i];
    o<<"{\"receipt\":"<<r.receipt<<",\"packet\":"<<r.packet<<",\"valid\":"<<r.valid<<",\"returned\":"<<r.returned<<",\"thread\":"<<r.thread<<",\"marker_index\":"<<r.marker<<",\"marker_failure\":"<<static_cast<unsigned>(r.marker_failure)<<",\"executor\":"<<r.executor<<",\"context\":"<<r.context<<",\"recording\":"<<r.recording<<",\"command\":"<<r.command<<",\"identity\":"<<r.identity
     <<",\"enter\":"<<r.enter<<",\"exit\":"<<r.exit<<",\"copy\":"<<r.copy<<",\"copy_exit\":"<<r.copy_exit<<",\"copy_returned\":"<<r.copy_returned<<",\"copy_caller\":"<<r.caller<<",\"native_calls\":"<<r.native_calls
     <<",\"pixel_receipt\":"<<r.pixel_receipt<<",\"pixel_begin\":"<<r.pixel_begin<<",\"pixel_end\":"<<r.pixel_end<<",\"source\":";res(r.source);o<<",\"destination\":";res(r.destination);o<<",\"barrier_calls\":[";
    for(unsigned j=0;j<r.call_count;++j){if(j)o<<',';const auto &b=r.calls[j];o<<"{\"first\":"<<b.first<<",\"count\":"<<b.count<<",\"enter\":"<<b.enter<<",\"exit\":"<<b.exit<<",\"caller\":"<<b.caller<<",\"returned\":"<<b.returned<<",\"stable\":"<<b.stable<<'}';}
    o<<"]";
#ifdef MHW_TEMPORAL_SCALED
    write_draw(o,r);
#endif
    o<<",\"barriers\":[";
    for(unsigned j=0;j<r.barrier_count;++j){if(j)o<<',';const auto &b=r.barriers[j];unsigned sub{},before{},after{};std::uintptr_t resource{},other{};
      if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION){resource=reinterpret_cast<std::uintptr_t>(b.Transition.pResource);sub=b.Transition.Subresource;before=b.Transition.StateBefore;after=b.Transition.StateAfter;}
      else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING){resource=reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore);other=reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter);}
      else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV)resource=reinterpret_cast<std::uintptr_t>(b.UAV.pResource);
      o<<"{\"type\":"<<b.Type<<",\"flags\":"<<b.Flags<<",\"subresource\":"<<sub<<",\"before\":"<<before<<",\"after\":"<<after<<",\"resource\":"<<resource<<",\"other\":"<<other<<'}';}o<<"]}";}
  o<<"]}";
}
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const Config &c,void *producer,void *consumer,void *copy,bool wait_for_projection){cfg=c;configured=true;projection_gate_required=wait_for_projection;originals[0]=producer;originals[1]=consumer;originals[2]=copy;targets[2]=reinterpret_cast<std::uintptr_t>(copy);begin(GetTickCount64());}
void fixture_copy(ID3D12GraphicsCommandList *l,ID3D12Resource *d,ID3D12Resource *s,std::uintptr_t caller){copy_call(l,d,s,caller);}
bool fixture_release_pixels(bool safe) noexcept {bool ok=true;for(auto &p:pixels)ok=p.fixture_release_after_reset(safe) && ok;return ok;}
void fixture_map_failure() noexcept {pixels[0].fixture_map_failure();}
void fixture_before_marker(void (*callback)()) noexcept {before_marker=callback;}
#ifdef MHW_TEMPORAL_SCALED
void fixture_draw_start(void *original) noexcept {originals[3]=original;}
void fixture_draw(ID3D12GraphicsCommandList *l,unsigned n,unsigned instances,unsigned first,unsigned instance,std::uintptr_t caller){draw_call(l,n,instances,first,instance,caller);}
#endif
#endif
}
extern "C" void mhw_temporal_depth_produce(mhw::temporal_bindings::NativeFrame *f){mhw::temporal_bindings::depth_routes::produce(f);}
extern "C" void mhw_temporal_depth_consume(mhw::temporal_bindings::NativeFrame *f){mhw::temporal_bindings::depth_routes::consume(f);}
