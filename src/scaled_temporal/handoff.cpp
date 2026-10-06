#include "handoff.hpp"
#include "downstream.hpp"
#ifdef MHW_SCALED_OUTPUT
#include "output_route.hpp"
#endif
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <d3d12.h>
#include <MinHook.h>
#include <atomic>
#include <algorithm>
#include <ostream>
extern "C" void scaled_handoff_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::scaled_temporal::handoff {
namespace {
namespace tb=temporal_bindings;namespace pb=projection_binding;
struct Copy {
  std::uintptr_t object{},source{},destination{},optional{};
  unsigned opcode{},mode{},destination_subresource{},source_subresource{};
  std::array<unsigned,2> xy{};std::array<unsigned,4> rectangle{};
  bool operator==(const Copy &)const=default;
};
struct Pin {ID3D12Resource *held{};std::uintptr_t identity{};D3D12_RESOURCE_DESC desc{};};
struct Record {
  tb::Snapshot expected{};unsigned thread{},observed_thread{},count_before{},count_after{};
  std::uintptr_t caller{},argument_this{},attachment{},array_before{},array_after{};
  tb::Owner source{},destination{};Pin source_pin{},destination_pin{};
  std::array<unsigned,4> rectangle{};Copy copy{};
  bool valid{},returned{},owners_stable{},stamp_stable{},rectangle_stable{},copy_branch{};
};
tb::Config cfg{};std::array<Record,limit> records;
std::atomic<unsigned> state{},fault{},pending{},published{},completed{};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
void *original{};std::uintptr_t target{},expected_caller{};
bool installed{},disabled{},drained{};unsigned acquired{},released{};
bool read(void *,std::uintptr_t p,std::span<std::byte> out) {
  return bridge_probe::read_bounded_scalar(nullptr,p,out)==bridge_probe::ScalarReadResult::success;
}
const tb::Reader reader{nullptr,read};
template<std::size_t N>bool read_array(std::uintptr_t at,std::array<unsigned,N> &out) {
  if(!at || at>UINTPTR_MAX-sizeof(out))return false;
  for(unsigned i=0;i<N;++i)if(!reader.get(at+4*i,out[i]))return false;return true;
}
void fail(unsigned value) noexcept {unsigned zero{};fault.compare_exchange_strong(zero,value);state.store(2,std::memory_order_release);}
bool pointer(std::uintptr_t p,std::uintptr_t size) {return p && p<=UINTPTR_MAX-size;}
bool methods(std::uintptr_t object,std::initializer_list<unsigned> slots) {
  for(auto slot:slots){std::uintptr_t fn{};if(!pb::system_method(object,slot,fn))return false;}return true;
}
bool release(IUnknown *p) noexcept {
  if(!p)return true;if(!methods(reinterpret_cast<std::uintptr_t>(p),{2}))return false;
  __try{p->Release();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool pin(const tb::Owner &owner,Pin &out) noexcept {
  ID3D12Resource *resource{};ID3D12Device *device{};IUnknown *identity{};bool ok=false;
  if(!methods(owner.resource,{0,2,7,10}))return false;
  __try {
    auto p=reinterpret_cast<ID3D12Resource *>(owner.resource);
    if(SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&resource))) && reinterpret_cast<std::uintptr_t>(resource)==owner.resource &&
       SUCCEEDED(resource->GetDevice(IID_PPV_ARGS(&device))) && reinterpret_cast<std::uintptr_t>(device)==cfg.device &&
       SUCCEEDED(resource->QueryInterface(IID_PPV_ARGS(&identity))) && identity) {
      out.identity=reinterpret_cast<std::uintptr_t>(identity);out.desc=resource->GetDesc();const auto &d=out.desc;
      ok=d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width==owner.width && d.Height==owner.height &&
        d.Width<=16384 && d.Height<=16384 && d.DepthOrArraySize==1 && d.MipLevels==1 && d.SampleDesc.Count==1;
    }
  }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  const bool a=release(identity),b=release(device);ok=ok&&a&&b;
  if(ok){out.held=resource;++acquired;}else if(!release(resource))fail(9);
  return ok;
}
bool stamp(const tb::Snapshot &s) {
  std::uint64_t frame{},epoch{};unsigned bank{};unsigned char async{};std::uintptr_t view{},primary{},output{};
  return s.context==cfg.renderer+0x710 && reader.get(cfg.renderer+0x1f210,frame) && frame==s.frame &&
    reader.get(cfg.renderer+0x1f208,epoch) && epoch==s.epoch && reader.get(cfg.renderer+0x1f200,bank) && bank==s.bank &&
    reader.get(cfg.renderer+0x93,async) && async==s.async && reader.get(s.context+0x1d0,view) && view==s.view &&
    reader.get(cfg.renderer+0x1f450,primary) && primary==s.view && pointer(s.layer,0x180) &&
    reader.get(s.layer+0x168+8*(s.frame%3),output) && output==s.inputs[4].object;
}
bool queue(std::uintptr_t context,unsigned &count,std::uintptr_t &array) {
  return reader.get(context+0x2b80,count) && count<65535 && reader.get(context+0x2b78,array) && pointer(array,16ULL*(count+1));
}
bool read_copy(std::uintptr_t object,Copy &p) {
  p={};p.object=object;unsigned raw{};
  if(!pointer(object,0x50) || !reader.get(object,raw))return false;p.opcode=raw&31;
  if(p.opcode!=13)return false;
  return reader.get(object+0x10,p.destination) && reader.get(object+0x18,p.source) &&
    reader.get(object+0x20,p.mode) && reader.get(object+0x24,p.destination_subresource) &&
    reader.get(object+0x28,p.source_subresource) && read_array(object+0x30,p.xy) &&
    read_array(object+0x38,p.rectangle) && reader.get(object+0x48,p.optional);
}
unsigned before(pb::Frame &f) noexcept {
  if(state.load(std::memory_order_acquire)!=1 || f.caller!=expected_caller)return UINT_MAX;
  // Only a TAA dispatch that the main recorder has qualified can select a copy.
  // Ordinary startup/other-layer traffic never consumes this three-record budget.
  if(completed.load(std::memory_order_acquire)>=published.load(std::memory_order_acquire))return UINT_MAX;
  if(busy.test_and_set(std::memory_order_acquire)){fail(1);return UINT_MAX;}
  const auto i=completed.load();
  if(state.load()!=1 || i>=published.load() || i>=limit){busy.clear(std::memory_order_release);return UINT_MAX;}
  auto &r=records[i];r.caller=f.caller;r.argument_this=f.gpr[1];r.attachment=f.gpr[4];r.observed_thread=GetCurrentThreadId();
  auto &s=r.expected;std::uintptr_t destination{};
  r.valid=r.observed_thread==r.thread && f.gpr[2]==s.context && f.gpr[3]==s.inputs[4].object && stamp(s) &&
    pointer(r.attachment,0x60) && reader.get(r.attachment+0x58,destination) &&
    tb::read_owner(reader,f.gpr[3],r.source) && tb::read_owner(reader,destination,r.destination) &&
    read_array(s.context+0x5c,r.rectangle) && queue(s.context,r.count_before,r.array_before);
  auto selected=s.inputs[4];selected.id=0;
  r.valid=r.valid && r.source==selected && r.source.resource!=r.destination.resource &&
    r.rectangle[0]<r.rectangle[2] && r.rectangle[1]<r.rectangle[3] && r.rectangle[2]<=16384 && r.rectangle[3]<=16384;
  r.copy_branch=r.valid && r.source.format==r.destination.format && r.rectangle[2]<=r.source.width && r.rectangle[3]<=r.source.height;
  if(r.valid)r.valid=pin(r.source,r.source_pin) && pin(r.destination,r.destination_pin);
  if(r.valid)r.valid=downstream::select(i,s,r.destination.object);
  if(!r.valid)fail(3);
  return i;
}
void after(unsigned i,bool returned) noexcept {
  if(i==UINT_MAX)return;
  auto &r=records[i];r.returned=returned;const auto &s=r.expected;
  tb::Owner source{},destination{};std::uintptr_t owner{},packet{},stable_packet{},array{};unsigned count{};Copy again{};
  std::array<unsigned,4> rectangle{};
  r.stamp_stable=stamp(s);
  r.owners_stable=tb::read_owner(reader,r.source.object,source) && source==r.source &&
    reader.get(r.attachment+0x58,owner) && owner==r.destination.object &&
    tb::read_owner(reader,owner,destination) && destination==r.destination;
  r.rectangle_stable=read_array(s.context+0x5c,rectangle) && rectangle==r.rectangle;
  // Inspect only the one new packet expected from the qualified copy branch.
  // A System_Copy draw is reported as unsupported, never interpreted as a copy.
  const bool q=queue(s.context,r.count_after,r.array_after);
  const bool packet_ok=q && r.count_after==r.count_before+1 &&
    reader.get(r.array_after+16ULL*r.count_before+8,packet) && read_copy(packet,r.copy) &&
    queue(s.context,count,array) && count==r.count_after && array==r.array_after &&
    reader.get(array+16ULL*r.count_before+8,stable_packet) && stable_packet==packet &&
    read_copy(packet,again) && again==r.copy;
  const auto &p=r.copy;
  r.valid=r.valid && returned && r.stamp_stable && r.owners_stable && r.rectangle_stable && r.copy_branch && packet_ok &&
    p.source==r.source.object && p.destination==r.destination.object && p.rectangle==r.rectangle &&
    !p.mode && !p.source_subresource && !p.destination_subresource && !p.xy[0] && !p.xy[1] && !p.optional &&
    r.source_pin.desc.Format==r.destination_pin.desc.Format &&
    p.rectangle[2]-p.rectangle[0]<=r.destination.width && p.rectangle[3]-p.rectangle[1]<=r.destination.height;
  if(!r.valid)fail(returned?4:5);
#ifdef MHW_SCALED_OUTPUT
  if(r.valid&&!output_route::publish_copy(i,s,p.object,r.source_pin.held,r.destination_pin.held))fail(10);
#endif
  completed.store(i+1,std::memory_order_release);busy.clear(std::memory_order_release);
}
void owner_json(std::ostream &o,const tb::Owner &v,const Pin &p) {
  const auto &d=p.desc;
  o<<"{\"owner\":"<<v.object<<",\"resource\":"<<v.resource<<",\"identity\":"<<p.identity
   <<",\"type\":"<<v.type<<",\"kind\":"<<v.kind<<",\"engine_flags\":"<<v.flags
   <<",\"engine_width\":"<<v.width<<",\"engine_height\":"<<v.height<<",\"engine_format\":"<<v.format<<",\"gpu_format\":"<<v.gpu_format
   <<",\"primary\":"<<v.primary<<",\"secondary\":"<<v.secondary<<",\"backing\":"<<v.backing
   <<",\"dimension\":"<<d.Dimension<<",\"width\":"<<d.Width<<",\"height\":"<<d.Height<<",\"format\":"<<d.Format
   <<",\"array_size\":"<<d.DepthOrArraySize<<",\"mips\":"<<d.MipLevels<<",\"sample_count\":"<<d.SampleDesc.Count
   <<",\"sample_quality\":"<<d.SampleDesc.Quality<<",\"layout\":"<<d.Layout<<",\"flags\":"<<d.Flags<<'}';
}
template<std::size_t N>void array_json(std::ostream &o,const std::array<unsigned,N> &v){o<<'[';for(unsigned i=0;i<N;++i){if(i)o<<',';o<<v[i];}o<<']';}
}
void dispatch(pb::Frame *f) {
  pending.fetch_add(1,std::memory_order_acq_rel);const auto i=before(*f);bool returned=false;
  __try{mhw_binding_forward(original,f);returned=true;}
  __finally{after(i,returned);pending.fetch_sub(1,std::memory_order_release);}
}
bool active() noexcept{return state.load(std::memory_order_acquire)!=0;}
bool publish(unsigned i,const tb::Snapshot &s) noexcept {
  if(state.load(std::memory_order_acquire)!=1)return false;
  if(busy.test_and_set(std::memory_order_acquire)){fail(1);return false;}
  const bool ok=i<limit && i==published.load() && i==completed.load() && s.frame &&
    (!i || s.frame>records[i-1].expected.frame);
  if(ok){records[i].expected=s;records[i].thread=GetCurrentThreadId();published.store(i+1,std::memory_order_release);}
  else fail(2);
  busy.clear(std::memory_order_release);return ok;
}
bool install(const tb::Config &c,std::string &reason) {
  if(active()){reason="handoff-already-used";return false;}cfg=c;expected_caller=c.image+caller_rva;
  if(!pb::verify_code(c.image,0x23917a0,143,"b135188a6fd2cb924a70fae0ff9cfa5da3c524781aa175e9fbd4a00d023c405e") ||
     !pb::verify_code(c.image,0x23c9390,983,"48a945e9db1187708d672eb8a5527d3c0cca27c761d8dc25c69751ed159b85a8") ||
     !pb::verify_code(c.image,0x23c9250,313,"48dc45a224fa9a3d25c884c46039221b34a7b8e21e880889250fd997f3ecb283")) {
    reason="handoff-code";return false;
  }
  target=c.image+target_rva;
  const auto create=MH_CreateHook(reinterpret_cast<void *>(target),reinterpret_cast<void *>(scaled_handoff_entry),&original);
  if(create!=MH_OK){reason="handoff-create-"+std::to_string(create);return false;}
  state.store(1,std::memory_order_release);
  const auto enable=MH_EnableHook(reinterpret_cast<void *>(target));
  if(enable!=MH_OK){fail(6);reason="handoff-enable-"+std::to_string(enable);return false;}
  installed=true;return downstream::install(c,reason);
}
bool healthy() noexcept{return !fault.load(std::memory_order_acquire) && downstream::healthy();}
bool complete() noexcept{return healthy() && completed.load(std::memory_order_acquire)==limit && downstream::complete();}
void stop() noexcept{if(active())state.store(2,std::memory_order_release);downstream::stop();}
bool settled() noexcept{return pending.load(std::memory_order_acquire)==0 && downstream::settled();}
bool finish() noexcept {
  if(!active())return true;stop();
  const auto tick=GetTickCount64();while(!settled() && GetTickCount64()-tick<2000)Sleep(1);
  drained=settled();if(!drained){fail(7);return false;}
  const auto result=installed?MH_DisableHook(reinterpret_cast<void *>(target)):MH_ERROR_DISABLED;
  disabled=result==MH_OK || result==MH_ERROR_DISABLED;
  // The DLL and MinHook trampoline stay allocated. Forward-only callbacks may
  // return after admission closes; release captured resources only when idle.
  while(!settled() && GetTickCount64()-tick<2000)Sleep(1);
  if(!settled()){fail(7);return false;}
  for(auto &r:records)for(auto p:{&r.source_pin,&r.destination_pin})if(p->held){
    if(release(p->held)){p->held=nullptr;++released;}else fail(9);
  }
  const bool downstream_ok=downstream::finish();
  return downstream_ok && disabled && healthy() && complete() && published.load()==limit && acquired==released;
}
void write(std::ostream &o) {
  o<<"{\"schema\":1,\"policy\":\"native-final-copy-observer-v1\",\"active\":"<<active()<<",\"fault\":"<<fault.load()
   <<",\"pending\":"<<pending.load()<<",\"published\":"<<published.load()<<",\"completed\":"<<completed.load()
   <<",\"hooks_disabled\":"<<disabled<<",\"drained_before_disable\":"<<drained<<",\"pins_acquired\":"<<acquired<<",\"pins_released\":"<<released
   <<",\"target\":"<<target<<",\"expected_caller\":"<<expected_caller<<",\"mutations\":0,\"gpu_consumption_verified\":false,\"records\":[";
  for(unsigned i=0;i<(std::min)(published.load(),limit);++i){if(i)o<<',';const auto &r=records[i];const auto &s=r.expected;const auto &p=r.copy;
    o<<"{\"index\":"<<i<<",\"valid\":"<<r.valid<<",\"returned\":"<<r.returned<<",\"owners_stable\":"<<r.owners_stable
     <<",\"stamp_stable\":"<<r.stamp_stable<<",\"rectangle_stable\":"<<r.rectangle_stable<<",\"copy_branch\":"<<r.copy_branch
     <<",\"frame\":"<<s.frame<<",\"epoch\":"<<s.epoch<<",\"bank\":"<<s.bank<<",\"async\":"<<s.async<<",\"view\":"<<s.view
     <<",\"context\":"<<s.context<<",\"layer\":"<<s.layer<<",\"thread\":"<<r.thread<<",\"observed_thread\":"<<r.observed_thread
     <<",\"caller\":"<<r.caller<<",\"argument_this\":"<<r.argument_this<<",\"attachment\":"<<r.attachment
     <<",\"count_before\":"<<r.count_before<<",\"count_after\":"<<r.count_after<<",\"array_before\":"<<r.array_before<<",\"array_after\":"<<r.array_after
     <<",\"rectangle\":";array_json(o,r.rectangle);o<<",\"source\":";owner_json(o,r.source,r.source_pin);
    o<<",\"destination\":";owner_json(o,r.destination,r.destination_pin);
    o<<",\"packet\":{\"object\":"<<p.object<<",\"opcode\":"<<p.opcode<<",\"source\":"<<p.source<<",\"destination\":"<<p.destination
     <<",\"mode\":"<<p.mode<<",\"source_subresource\":"<<p.source_subresource<<",\"destination_subresource\":"<<p.destination_subresource
     <<",\"optional\":"<<p.optional<<",\"xy\":";array_json(o,p.xy);o<<",\"rectangle\":";array_json(o,p.rectangle);o<<"}}";
  }o<<']';if(downstream::active()){o<<",\"downstream\":";downstream::write(o);}o<<'}';
}
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const tb::Config &c,void *forward,std::uintptr_t caller){cfg=c;original=forward;target=c.image+target_rva;expected_caller=caller?caller:c.image+caller_rva;state.store(1);}
#endif
}
extern "C" void mhw_scaled_handoff(mhw::projection_binding::Frame *f){mhw::scaled_temporal::handoff::dispatch(f);}
