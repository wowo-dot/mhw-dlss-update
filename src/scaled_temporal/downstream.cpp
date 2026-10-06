#include "downstream.hpp"
#include "postfilter.hpp"
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <ostream>
extern "C" void scaled_downstream_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::scaled_temporal::downstream {
namespace {
namespace tb=temporal_bindings;namespace pb=projection_binding;
constexpr unsigned capacity=3,packet_limit=8,filter_limit=16;
struct Command {std::uintptr_t object{};unsigned key{},opcode{};bool operator==(const Command &)const=default;};
struct Filter {
  std::uintptr_t entry{},unit{},type{},input{},output{};
  unsigned stage{},order{};std::array<unsigned char,3> flags{};
};
struct Quad {
  std::uint64_t frame{},epoch{};unsigned bank{},thread{},sort{},base_id{},count_before{},count_after{},packet_count{};
  unsigned char async{};std::uintptr_t context{},view{},caller{},attachment{};
  std::array<unsigned,4> viewport{},rectangle{};std::array<unsigned,2> dimensions{};
  std::array<float,2> offset{};unsigned triangle{};
  tb::Owner source{},destination{};
  std::array<Command,packet_limit> packets{};
  bool valid{},returned{},stable{};
};
struct Record {
  Quad quad{};std::uintptr_t queue{},taa_destination{},view_color{},view_output{};
  unsigned filter_count{},taa_index{UINT_MAX};std::array<Filter,filter_limit> filters{};bool valid{};
};
tb::Config cfg{};std::array<Record,capacity> records{};
std::atomic<unsigned> state{},pending{},fault{},selected{},seen{};
std::atomic_flag selecting=ATOMIC_FLAG_INIT;
thread_local Quad prior{};thread_local bool in_quad{};
void *original{};std::uintptr_t target{},caller{};bool installed{},disabled{},drained{};
bool read(void *,std::uintptr_t at,std::span<std::byte> out){return bridge_probe::read_bounded_scalar(nullptr,at,out)==bridge_probe::ScalarReadResult::success;}
const tb::Reader reader{nullptr,read};
template<class T,std::size_t N>bool array(std::uintptr_t at,std::array<T,N> &v){
  if(!at||at>UINTPTR_MAX-sizeof(v))return false;
  for(unsigned i=0;i<N;++i)if(!reader.get(at+i*sizeof(T),v[i]))return false;return true;
}
bool pointer(std::uintptr_t p,unsigned bytes){return p&&p<=UINTPTR_MAX-bytes;}
void fail(unsigned n){unsigned zero{};fault.compare_exchange_strong(zero,n);state.store(2,std::memory_order_release);}
bool stamp(Quad &q){
  std::uintptr_t primary{};
  return q.context==cfg.renderer+0x710 && reader.get(q.context+0x1d0,q.view) && pointer(q.view,0x6bc) &&
    reader.get(cfg.renderer+0x1f450,primary) && primary==q.view &&
    reader.get(cfg.renderer+0x1f210,q.frame) && q.frame && reader.get(cfg.renderer+0x1f208,q.epoch) &&
    reader.get(cfg.renderer+0x1f200,q.bank) && q.bank<=1 && reader.get(cfg.renderer+0x93,q.async) && q.async<=1;
}
bool same_stamp(const Quad &q){
  Quad now{};now.context=q.context;
  return stamp(now)&&now.view==q.view&&now.frame==q.frame&&now.epoch==q.epoch&&now.bank==q.bank&&now.async==q.async;
}
bool queue(std::uintptr_t context,unsigned &count,std::uintptr_t &at){
  return reader.get(context+0x2b80,count)&&count<65535&&reader.get(context+0x2b78,at)&&pointer(at,16*(count+1));
}
bool command(std::uintptr_t at,Command &c){
  unsigned raw{};return reader.get(at,c.key)&&reader.get(at+8,c.object)&&pointer(c.object,4)&&
    reader.get(c.object,raw)&&(c.opcode=raw&31,true);
}
bool owners(const Quad &q){
  tb::Owner s{},d{};std::uintptr_t object{},attachment{};
  return reader.get(q.context+0x710+8ULL*(q.base_id&0xfff),object)&&object==q.source.object&&
    reader.get(q.view+0x460,attachment)&&attachment==q.attachment&&reader.get(attachment+0x58,object)&&object==q.destination.object&&
    tb::read_owner(reader,q.source.object,s)&&s==q.source&&tb::read_owner(reader,q.destination.object,d)&&d==q.destination;
}
bool begin(const pb::Frame &f){
  if(state.load(std::memory_order_acquire)!=1||f.caller!=caller||f.gpr[2]!=cfg.renderer+0x710)return false;
  if(in_quad){fail(1);return false;}in_quad=true;prior={};auto &q=prior;
  q.context=f.gpr[2];q.caller=f.caller;q.thread=GetCurrentThreadId();q.triangle=static_cast<unsigned>(f.stack[1]&255);
  std::uintptr_t source{},destination{},at{};
  q.valid=stamp(q)&&array(q.context+0x5c,q.viewport)&&array(f.gpr[3],q.rectangle)&&array(f.gpr[4],q.dimensions)&&
    array(f.stack[0],q.offset)&&reader.get(q.context+0x154,q.sort)&&
    reader.get(cfg.image+0x4be7450,q.base_id)&&q.base_id!=UINT_MAX&&
    reader.get(q.context+0x710+8ULL*(q.base_id&0xfff),source)&&tb::read_owner(reader,source,q.source)&&
    reader.get(q.view+0x460,q.attachment)&&pointer(q.attachment,0x60)&&reader.get(q.attachment+0x58,destination)&&
    tb::read_owner(reader,destination,q.destination)&&queue(q.context,q.count_before,at)&&q.triangle<=1;
  q.valid=q.valid&&q.rectangle[0]<q.rectangle[2]&&q.rectangle[1]<q.rectangle[3]&&
    q.rectangle[2]<=q.dimensions[0]&&q.rectangle[3]<=q.dimensions[1]&&q.dimensions[0]<=16384&&q.dimensions[1]<=16384&&
    q.viewport[0]<q.viewport[2]&&q.viewport[1]<q.viewport[3]&&q.viewport[2]<=16384&&q.viewport[3]<=16384&&
    std::isfinite(q.offset[0])&&std::isfinite(q.offset[1])&&std::abs(q.offset[0])<=1&&std::abs(q.offset[1])<=1;
  ++seen;return true;
}
void end(bool participating,bool returned){
  if(!participating)return;auto &q=prior;q.returned=returned;
  std::array<unsigned,4> viewport{};std::uintptr_t at{};
  q.stable=q.valid&&returned&&same_stamp(q)&&owners(q)&&array(q.context+0x5c,viewport)&&viewport==q.viewport;
  q.valid=q.stable&&queue(q.context,q.count_after,at)&&q.count_after>q.count_before&&q.count_after-q.count_before<=packet_limit;
  if(q.valid){q.packet_count=q.count_after-q.count_before;
    for(unsigned i=0;i<q.packet_count;++i)if(!command(at+16ULL*(q.count_before+i),q.packets[i]))q.valid=false;
  }in_quad=false;
}
bool filter_queue(Record &r,const tb::Snapshot &s){
  if(!reader.get(cfg.image+0x51cc238,r.queue)||!pointer(r.queue,0xd0)||!reader.get(r.queue+0x3c,r.filter_count)||
     !r.filter_count||r.filter_count>filter_limit)return false;
  for(unsigned i=0;i<r.filter_count;++i){auto &f=r.filters[i];
    if(!reader.get(r.queue+0x40+8*i,f.entry)||!pointer(f.entry,0x23)||!reader.get(f.entry,f.stage)||f.stage>31||
       !reader.get(f.entry+4,f.order)||!reader.get(f.entry+8,f.unit)||!reader.get(f.entry+0x10,f.input)||
       !reader.get(f.entry+0x18,f.output)||!array(f.entry+0x20,f.flags)||(f.unit&&!reader.get(f.unit,f.type)))return false;
    if(f.entry==s.layer+0x140){if(r.taa_index!=UINT_MAX||f.unit!=s.layer||f.stage!=15||f.order!=2||
       f.input!=s.inputs[0].object||f.output!=r.taa_destination)return false;r.taa_index=i;}
  }
  std::uintptr_t attachment{};
  return r.taa_index!=UINT_MAX&&reader.get(s.view+0x420,attachment)&&pointer(attachment,0x60)&&
    reader.get(attachment+0x58,r.view_color)&&reader.get(s.view+0x460,attachment)&&pointer(attachment,0x60)&&
    reader.get(attachment+0x58,r.view_output)&&r.view_output==r.quad.destination.object;
}
template<class T,std::size_t N>void values(std::ostream &o,const std::array<T,N> &v){o<<'[';for(unsigned i=0;i<N;++i){if(i)o<<',';o<<+v[i];}o<<']';}
void owner_json(std::ostream &o,const tb::Owner &v){o<<"{\"owner\":"<<v.object<<",\"resource\":"<<v.resource<<",\"type\":"<<v.type<<",\"width\":"<<v.width<<",\"height\":"<<v.height<<",\"format\":"<<v.format<<'}';}
}
void dispatch(pb::Frame *f){pending.fetch_add(1,std::memory_order_acq_rel);const bool participating=begin(*f);bool returned=false;
  __try{mhw_binding_forward(original,f);returned=true;}__finally{end(participating,returned);pending.fetch_sub(1,std::memory_order_release);}
}
bool active() noexcept{return state.load(std::memory_order_acquire)!=0;}
bool healthy() noexcept{return !fault.load(std::memory_order_acquire)&&postfilter::healthy();}
bool complete() noexcept{return (!active()||selected.load(std::memory_order_acquire)==capacity)&&postfilter::complete();}
bool select(unsigned i,const tb::Snapshot &s,std::uintptr_t destination) noexcept {
  if(!active())return true;if(state.load(std::memory_order_acquire)!=1)return false;
  if(selecting.test_and_set(std::memory_order_acquire)){fail(2);return false;}
  bool ok=i<capacity&&i==selected.load();
  if(ok){auto &r=records[i];r.quad=prior;r.taa_destination=destination;const auto &q=r.quad;
    tb::Owner source_now{},destination_now{};
    ok=q.valid&&q.returned&&q.stable&&q.thread==GetCurrentThreadId()&&q.context==s.context&&q.view==s.view&&
      q.frame==s.frame&&q.epoch==s.epoch&&q.bank==s.bank&&q.async==s.async&&same_stamp(q)&&
      tb::read_owner(reader,q.source.object,source_now)&&source_now==q.source&&
      tb::read_owner(reader,q.destination.object,destination_now)&&destination_now==q.destination&&filter_queue(r,s);
    unsigned count{};std::uintptr_t at{};ok=ok&&queue(s.context,count,at)&&count>=q.count_after;
    for(unsigned n=0;ok&&n<q.packet_count;++n){Command now{};ok=command(at+16ULL*(q.count_before+n),now)&&now==q.packets[n];}
    std::uintptr_t filter{};
    for(unsigned n=0;ok&&n<r.filter_count;++n){const auto &f=r.filters[n];
      if(f.type==cfg.image+0x3537a70&&f.stage==15&&f.order==100){if(filter)ok=false;else filter=f.unit;}}
    if(ok)ok=postfilter::select(i,s,filter,q.source.object,q.packet_count==1?q.packets[0].object:0,q.destination);
    r.valid=ok;selected.store(i+1,std::memory_order_release);
  }
  if(!ok)fail(3);selecting.clear(std::memory_order_release);return ok;
}
bool install(const tb::Config &c,std::string &reason){
  if(active()){reason="downstream-already-used";return false;}cfg=c;target=c.image+target_rva;caller=c.image+caller_rva;
  if(!pb::verify_code(c.image,0x2297e20,656,"58fe604c883de6d505ce2211797c2efaddc360046d10bb14c2e50fafbae4ab6d")||
     !pb::verify_code(c.image,0x23c7831,740,"0da63ff39cc114c78483728e19221e1dd22806398522ec0095ec6c6349801247")||
     !pb::verify_code(c.image,0x24f1fc0,835,"1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190")){
    reason="downstream-code";return false;
  }
  const auto create=MH_CreateHook(reinterpret_cast<void *>(target),reinterpret_cast<void *>(scaled_downstream_entry),&original);
  if(create!=MH_OK){reason="downstream-create-"+std::to_string(create);return false;}
  state.store(1,std::memory_order_release);const auto enable=MH_EnableHook(reinterpret_cast<void *>(target));
  if(enable!=MH_OK){fail(4);reason="downstream-enable-"+std::to_string(enable);return false;}installed=true;return postfilter::install(c,reason);
}
void stop() noexcept{if(active())state.store(2,std::memory_order_release);postfilter::stop();}
bool settled() noexcept{return pending.load(std::memory_order_acquire)==0&&postfilter::settled();}
bool finish() noexcept{
  if(!active())return true;stop();const auto tick=GetTickCount64();while(!settled()&&GetTickCount64()-tick<2000)Sleep(1);
  drained=settled();if(!drained){fail(5);return false;}
  const auto result=installed?MH_DisableHook(reinterpret_cast<void *>(target)):MH_ERROR_DISABLED;
  disabled=result==MH_OK||result==MH_ERROR_DISABLED;
  while(!settled()&&GetTickCount64()-tick<2000)Sleep(1);
  const bool post=postfilter::finish();return disabled&&settled()&&!fault.load()&&selected.load()==capacity&&post;
}
void write(std::ostream &o){
  o<<"{\"schema\":1,\"policy\":\"native-downstream-scale-observer-v1\",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()
   <<",\"selected\":"<<selected.load()<<",\"seen\":"<<seen.load()<<",\"hooks_disabled\":"<<disabled<<",\"drained_before_disable\":"<<drained
   <<",\"target\":"<<target<<",\"expected_caller\":"<<caller<<",\"mutations\":0,\"gpu_consumption_verified\":false,\"records\":[";
  for(unsigned i=0;i<(std::min)(selected.load(),capacity);++i){if(i)o<<',';const auto &r=records[i];const auto &q=r.quad;
    o<<"{\"index\":"<<i<<",\"valid\":"<<r.valid<<",\"returned\":"<<q.returned<<",\"stable\":"<<q.stable<<",\"frame\":"<<q.frame
     <<",\"epoch\":"<<q.epoch<<",\"bank\":"<<q.bank<<",\"async\":"<<+q.async<<",\"thread\":"<<q.thread<<",\"context\":"<<q.context
     <<",\"view\":"<<q.view<<",\"caller\":"<<q.caller<<",\"sort\":"<<q.sort<<",\"base_id\":"<<q.base_id<<",\"attachment\":"<<q.attachment
     <<",\"triangle\":"<<q.triangle<<",\"count_before\":"<<q.count_before<<",\"count_after\":"<<q.count_after<<",\"viewport\":";values(o,q.viewport);
    o<<",\"rectangle\":";values(o,q.rectangle);o<<",\"dimensions\":";values(o,q.dimensions);o<<",\"offset\":";values(o,q.offset);
    o<<",\"source\":";owner_json(o,q.source);o<<",\"destination\":";owner_json(o,q.destination);o<<",\"packets\":[";
    for(unsigned n=0;n<q.packet_count;++n){if(n)o<<',';const auto &p=q.packets[n];o<<"{\"object\":"<<p.object<<",\"key\":"<<p.key<<",\"opcode\":"<<p.opcode<<'}';}
    o<<"],\"queue\":"<<r.queue<<",\"taa_index\":"<<r.taa_index<<",\"taa_destination\":"<<r.taa_destination
     <<",\"view_color\":"<<r.view_color<<",\"view_output\":"<<r.view_output<<",\"filters\":[";
    for(unsigned n=0;n<r.filter_count&&n<filter_limit;++n){if(n)o<<',';const auto &f=r.filters[n];o<<"{\"entry\":"<<f.entry<<",\"unit\":"<<f.unit
      <<",\"type\":"<<f.type<<",\"stage\":"<<f.stage<<",\"order\":"<<f.order<<",\"input\":"<<f.input<<",\"output\":"<<f.output<<",\"flags\":";values(o,f.flags);o<<'}';}
    o<<"]}";
  }o<<"],\"postfilter\":";postfilter::write(o);o<<'}';
}
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const tb::Config &c,void *forward,std::uintptr_t expected){cfg=c;original=forward;target=c.image+target_rva;caller=expected?expected:c.image+caller_rva;state.store(1);}
#endif
}
extern "C" void mhw_scaled_downstream(mhw::projection_binding::Frame *f){mhw::scaled_temporal::downstream::dispatch(f);}
