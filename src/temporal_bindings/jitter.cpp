#include "jitter.hpp"
#include "submission.hpp"
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <cstring>
#include <ostream>
#include <algorithm>
#include <bit>
#include <cmath>
extern "C" void temporal_jitter_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::temporal_bindings::jitter {
namespace {
namespace pb=projection_binding;
struct Record {pb::Source source;pb::Provider before,after;std::uint64_t candidate{},exit_tick{},restored_bits{};std::uintptr_t caller{};
  bool injected{},restored{},returned{},valid{},acknowledged{},frame_scope_admitted{};};
Config cfg;std::array<Record,capacity> records;
std::atomic<unsigned> open{},pending{},fault{},completed{},matched{};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
void *original{};std::uintptr_t target{};std::uint64_t started{};
bool installed{},disabled{};unsigned injections{},restorations{};
unsigned selection_attempts{},selection_skipped{};
FrameAdmission frame_admission{};unsigned frame_scope_waits{};
std::uint64_t selected_candidate{},selected_frame{};
void fail(unsigned f) noexcept {unsigned zero{};fault.compare_exchange_strong(zero,f);open.store(0,std::memory_order_release);}
bool strong(std::uint64_t pair) noexcept {
  return pb::jitter_finite(pair) && std::abs(std::bit_cast<float>(static_cast<unsigned>(pair)))>=.3125F &&
    std::abs(std::bit_cast<float>(static_cast<unsigned>(pair>>32)))>=.3125F;
}
bool select(const pb::Source &s) noexcept {
  // Wait before admitting the unchanged baseline; never skip within a triplet.
  // Preview the NEXT native table entry, then recheck its exact bits at injection.
  ++selection_attempts;std::uint64_t next{};const auto &a=s.anchor;
  if(selection_attempts>20 || a.frame==UINT64_MAX || a.epoch==UINT64_MAX ||
     !pb::read(a.table+0xe6a0+8*((a.frame%20+1)%20),next) || !pb::jitter_finite(next)){fail(12);return false;}
  if(!strong(next)){++selection_skipped;if(selection_attempts==20)fail(13);return false;}
  selected_candidate=next;selected_frame=a.frame+1;return true;
}
bool provider(pb::Provider &p) noexcept {
  unsigned char use{};
  p.valid=pb::read(cfg.renderer+0xa5,use) && use<=1 && pb::read(cfg.renderer+0xb4,p.sizing) && !(p.sizing&1) &&
    pb::read(cfg.renderer+0x78,p.backend) && p.backend==cfg.backend && pb::read(p.backend+0x18,p.device) && p.device==cfg.device &&
    pb::read(p.backend+0x42700,p.wrapper) && !p.wrapper;
  p.use=use;return p.valid;
}
bool anchor(pb::Source &s,const NativeFrame &f) noexcept {
  auto &a=s.anchor;unsigned char active{},display{},async{};unsigned stamp{},flags{};std::uint64_t zero{};
  if(!pb::read(cfg.image+0x51c4480,a.renderer) || a.renderer!=cfg.renderer || a.renderer>UINTPTR_MAX-0x1f570 ||
     !pb::read(cfg.image+0x51c4368,a.table) || !a.table || a.table>UINTPTR_MAX-0xe740 ||
     !pb::read(cfg.image+0x51c4400,s.camera) || !s.camera || s.camera>UINTPTR_MAX-0x100 ||
     f.call.gpr[1]!=s.camera+0x50 || f.call.gpr[2]!=a.renderer+0x710)return false;
  a.context=f.call.gpr[2];
  if(!pb::read(s.camera,s.camera_type) || (s.camera_type!=cfg.image+0x350d188 && s.camera_type!=cfg.image+0x33f82a8) ||
     !pb::read(s.camera+0x70,active) || !active || !pb::read(s.camera+0x75,display) || display ||
     !pb::read(s.camera+0x38,zero) || zero || !pb::read(s.camera+0x40,stamp) ||
     !pb::read(a.renderer+0x1f210,a.frame) || !pb::read(a.renderer+0x1f208,a.epoch) || stamp!=static_cast<unsigned>(a.epoch) ||
     !pb::read(a.renderer+0x1f450,a.view) || !a.view || a.view>UINTPTR_MAX-0x60 ||
     !pb::read(a.view+0x58,flags) || (flags&0x840000) ||
     !pb::read(a.renderer+0x1f200,a.bank) || a.bank>1 || !pb::read(a.renderer+0x93,async) || async>1)return false;
  a.async_mode=async;return true;
}
unsigned before(const NativeFrame &f) noexcept {
  if(!open.load(std::memory_order_acquire) || f.call.caller!=cfg.image+0x228e834 || !submission::warmed() || completed.load()==capacity)return UINT_MAX;
  pending.fetch_add(1,std::memory_order_acq_rel);
  if(!open.load(std::memory_order_acquire)){pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;}
  if(busy.test_and_set(std::memory_order_acquire)){fail(1);pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;}
  const auto i=completed.load(std::memory_order_acquire);pb::Source s;
  if(GetTickCount64()-started>=2000)fail(2);
  else if(i<capacity && anchor(s,f)) {
    const auto &a=s.anchor;
    // No additional eligible projection may replace an admitted frame, and no
    // stimulus is allowed before the previous TAA producer acknowledged it.
    if(frame_admission && !frame_admission(a.context,a.view)) {
      // Installation can expose this hook before native activation has entered
      // an owned camera frame. Wait only before the baseline, never mid-triplet.
      if(i)fail(16);else ++frame_scope_waits;
    }
    else if(i && (matched.load(std::memory_order_acquire)!=i || a.frame!=records[i-1].source.anchor.frame+1 ||
       a.epoch!=records[i-1].source.anchor.epoch+1 || s.camera!=records[0].source.camera ||
       s.camera_type!=records[0].source.camera_type || a.view!=records[0].source.anchor.view ||
       a.table!=records[0].source.anchor.table || a.async_mode!=records[0].source.anchor.async_mode))fail(3);
    else if(!i && !select(s)) { /* Forward this unselected call unchanged. */ }
    else {
      auto &r=records[i];r.source=s;r.caller=f.call.caller;r.source.thread=GetCurrentThreadId();r.source.tick=GetTickCount64();
      r.frame_scope_admitted=frame_admission!=nullptr;
      if(!provider(r.before) || !pb::read(a.table+0xe6a0+8*(a.frame%20),r.candidate) || !pb::jitter_finite(r.candidate))fail(4);
      else if(i>0 && i+1<capacity) {
        if(i==1 && (a.frame!=selected_frame || r.candidate!=selected_candidate || !strong(r.candidate)))fail(14);
        for(unsigned j=1;j<i;++j)if(records[j].candidate==r.candidate)fail(15);
        if(!fault.load()){
          if(!pb::jitter_exchange(s.camera+0x38,0,r.candidate))fail(5);
          else {r.injected=true;++injections;}
        }
      }
      // Return a token even on mutation failure so scoped cleanup always runs.
      return i;
    }
  } else if(i)fail(6);
  busy.clear(std::memory_order_release);pending.fetch_sub(1,std::memory_order_release);return UINT_MAX;
}
void inspect(unsigned i,const NativeFrame &f) noexcept {
  if(i==UINT_MAX || fault.load())return;
  auto &r=records[i];auto &s=r.source;
  s.projection=bridge_probe::capture_projection_snapshot(cfg.image,s.anchor,bridge_probe::read_bounded_scalar,nullptr);
  const auto &p=s.projection;
  const auto observed=std::uint64_t{p.jitter_bits[0]}|(std::uint64_t{p.jitter_bits[1]}<<32);
  const auto candidate=std::uint64_t{p.table20_bits[0]}|(std::uint64_t{p.table20_bits[1]}<<32);
  r.returned=true;
  if(!(f.call.gpr[0]&255) || !p.valid || !p.matrix_relation || p.manager!=s.camera || p.vtable!=s.camera_type ||
     p.tagged_constants!=p.constants || !p.words || p.words>pb::max_constants/4 ||
     observed!=(r.injected?r.candidate:0) || candidate!=r.candidate ||
     !provider(r.after) || r.before!=r.after || (i && p.viewport_bits!=records[0].source.projection.viewport_bits)){fail(7);return;}
  s.bytes=p.words*4;std::array<std::byte,pb::max_constants> again{};
  if(!pb::copy_memory(p.constants,s.data.data(),s.bytes) || !pb::copy_memory(p.constants,again.data(),s.bytes) ||
     std::memcmp(s.data.data(),again.data(),s.bytes) ||
     std::memcmp(s.data.data()+4*((p.projection_id>>12)&0x3ff),p.projection_bits.data(),64) ||
     std::memcmp(s.data.data()+4*((p.unjittered_id>>12)&0x3ff),p.unjittered_bits.data(),64) ||
     !pb::capture_matrices(cfg.image,s)){fail(8);return;}
  s.valid=true;
}
void restore(unsigned i,bool returned) noexcept {
  if(i==UINT_MAX)return;
  auto &r=records[i];
  // Independent of stop/fault state, runs during SEH unwind as well.
  if(r.injected){r.restored=pb::jitter_exchange(r.source.camera+0x38,r.candidate,0);if(r.restored)++restorations;}
  else r.restored=true;
  if(!r.restored || !pb::read(r.source.camera+0x38,r.restored_bits) || r.restored_bits)fail(9);
  if(!returned)fail(10);
  r.exit_tick=GetTickCount64();if(r.exit_tick-started>=2000)fail(2);
  r.valid=returned && r.returned && r.source.valid && r.restored && !r.restored_bits && !fault.load();
  completed.store(i+1,std::memory_order_release);
  busy.clear(std::memory_order_release);pending.fetch_sub(1,std::memory_order_release);
}
}
void dispatch(NativeFrame *f) {
  const auto i=before(*f);bool returned=false;
  __try {mhw_binding_forward(original,&f->call);returned=true;inspect(i,*f);}
  __finally {restore(i,returned);}
}
bool prepare(const Config &c,std::string &reason,FrameAdmission admission) {
  cfg=c;frame_admission=admission;target=c.image+0x228f3b0;
  // This exact helper was already byte-gated by discover; preserve that check
  // here too so the mutation target cannot be installed independently ungated.
  if(!pb::verify_code(c.image,0x228f3b0,2000,"df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373")){reason="jitter-code";return false;}
  const auto s=MH_CreateHook(reinterpret_cast<void *>(target),reinterpret_cast<void *>(temporal_jitter_entry),&original);
  if(s!=MH_OK){reason="jitter-create-"+std::to_string(s);return false;}return true;
}
bool begin(std::uint64_t tick,std::string &reason) {
  started=tick;open.store(1,std::memory_order_release);
  const auto s=MH_EnableHook(reinterpret_cast<void *>(target));installed=s==MH_OK;
  if(!installed){stop();reason="jitter-enable-"+std::to_string(s);}return installed;
}
bool wanted(unsigned i) noexcept {return healthy() && i<completed.load(std::memory_order_acquire) && records[i].valid;}
bool matches(unsigned i,const Snapshot &s) noexcept {
  if(!wanted(i) || matched.load()!=i)return false;
  const auto &a=records[i].source.anchor;
  return s.frame==a.frame && s.epoch==a.epoch && s.bank==a.bank && s.async==a.async_mode && s.context==a.context && s.view==a.view;
}
bool acknowledge(unsigned i,const Snapshot &s) noexcept {
  if(!matches(i,s)){fail(11);return false;}
  records[i].acknowledged=true;matched.store(i+1,std::memory_order_release);return true;
}
bool healthy() noexcept {return fault.load(std::memory_order_acquire)==0;}
bool complete() noexcept {return healthy() && completed.load(std::memory_order_acquire)==capacity && matched.load(std::memory_order_acquire)==capacity;}
bool settled() noexcept {return pending.load(std::memory_order_acquire)==0;}
bool hooks_disabled() noexcept {return disabled;}
void stop() noexcept {open.store(0,std::memory_order_release);}
bool history_sample(unsigned i,HistorySample &out) noexcept {
  if(!disabled || !settled() || !complete() || i>=capacity || injections!=stimulus_count || restorations!=stimulus_count)return false;
  const auto &r=records[i];
  if(!r.valid || !r.acknowledged || !r.restored || r.restored_bits)return false;
  out={r.source.anchor.frame,r.source.anchor.epoch,r.source.projection.jitter_bits};return true;
}
bool current_history_sample(unsigned i,HistorySample &out) noexcept {
  // completed and matched publish immutable per-frame projection data. Later
  // receipts may still be rendering; no whole-capture completion is inferred.
  if(i>=capacity || !healthy() || completed.load(std::memory_order_acquire)<=i || matched.load(std::memory_order_acquire)<=i)return false;
  const auto &r=records[i];
  if(!r.valid || !r.returned || !r.acknowledged || !r.restored || r.restored_bits)return false;
  out={r.source.anchor.frame,r.source.anchor.epoch,r.source.projection.jitter_bits};return true;
}
bool finish(bool drain) noexcept {
  stop();const auto s=installed?MH_DisableHook(reinterpret_cast<void *>(target)):MH_ERROR_DISABLED;
  disabled=s==MH_OK || s==MH_ERROR_DISABLED;
  const auto until=GetTickCount64()+2000;while(drain && !settled() && GetTickCount64()<until)Sleep(1);
  return disabled && settled() && complete() && injections==stimulus_count && restorations==stimulus_count;
}
void write(std::ostream &o) {
  o<<"{\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"hooks_disabled\":"<<disabled
   <<",\"completed\":"<<completed.load()<<",\"matched\":"<<matched.load()<<",\"injections\":"<<injections<<",\"restorations\":"<<restorations
   <<",\"selection_attempts\":"<<selection_attempts<<",\"selection_skipped\":"<<selection_skipped
   <<",\"frame_scope_required\":"<<(frame_admission!=nullptr)<<",\"frame_scope_waits\":"<<frame_scope_waits
   <<",\"selected_candidate\":"<<selected_candidate<<",\"selected_frame\":"<<selected_frame<<",\"records\":[";
  for(unsigned i=0;i<(std::min)(completed.load(),capacity);++i) {
    if(i)o<<',';const auto &r=records[i];const auto &s=r.source;const auto &a=s.anchor;const auto &p=s.projection;
    o<<"{\"stage\":"<<i<<",\"valid\":"<<r.valid<<",\"returned\":"<<r.returned<<",\"acknowledged\":"<<r.acknowledged
     <<",\"frame_scope_admitted\":"<<r.frame_scope_admitted
     <<",\"injected\":"<<r.injected<<",\"restored\":"<<r.restored<<",\"restored_bits\":"<<r.restored_bits
     <<",\"candidate\":"<<r.candidate<<",\"enter_tick\":"<<s.tick<<",\"exit_tick\":"<<r.exit_tick<<",\"thread\":"<<s.thread<<",\"caller\":"<<r.caller
     <<",\"frame\":"<<a.frame<<",\"epoch\":"<<a.epoch<<",\"bank\":"<<a.bank<<",\"async\":"<<a.async_mode
     <<",\"renderer\":"<<a.renderer<<",\"context\":"<<a.context<<",\"view\":"<<a.view<<",\"table\":"<<a.table
     <<",\"camera\":"<<s.camera<<",\"camera_type\":"<<s.camera_type<<",\"provider_stable\":"<<(r.before.valid && r.before==r.after)
     <<",\"provider_use\":"<<r.before.use<<",\"provider_sizing\":"<<r.before.sizing<<",\"backend\":"<<r.before.backend
     <<",\"device\":"<<r.before.device<<",\"wrapper\":"<<r.before.wrapper
     <<",\"projection_valid\":"<<p.valid<<",\"matrix_relation\":"<<p.matrix_relation<<",\"constants\":"<<p.constants
     <<",\"words\":"<<p.words<<",\"buffer_id\":"<<p.buffer_id<<",\"projection_id\":"<<p.projection_id<<",\"unjittered_id\":"<<p.unjittered_id
     <<",\"matrices_valid\":"<<s.matrices.valid;
    const auto array=[&](const char *name,const auto &values){o<<",\""<<name<<"\":[";bool first=true;for(const auto v:values){if(!first)o<<',';first=false;o<<v;}o<<']';};
    array("matrix_ids",s.matrices.ids);array("jitter_bits",p.jitter_bits);array("table20_bits",p.table20_bits);array("viewport_bits",p.viewport_bits);
    array("projection_bits",p.projection_bits);array("unjittered_bits",p.unjittered_bits);
    array("view_projection_bits",s.matrices.bits[0]);array("view_bits",s.matrices.bits[1]);array("unjittered_view_projection_bits",s.matrices.bits[2]);o<<'}';
  }o<<"]}";
}
#ifdef MHW_TEMPORAL_FIXTURE
void fixture_start(const Config &c,void *forward,std::uint64_t tick,FrameAdmission admission){cfg=c;original=forward;started=tick;frame_admission=admission;open.store(1,std::memory_order_release);}
#endif
}
extern "C" void mhw_temporal_jitter(mhw::temporal_bindings::NativeFrame *f){mhw::temporal_bindings::jitter::dispatch(f);}
