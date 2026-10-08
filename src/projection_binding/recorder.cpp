#include "memory.hpp"
#include "provenance.hpp"
#include "../bridge_probe/readable_region.hpp"
#include <windows.h>
#include <MinHook.h>
#include <hde64.h>
#include <atomic>
#include <algorithm>
#include <cstring>

extern "C" {
void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
void binding_producer(); void binding_publish(); void binding_consume(); void binding_state();
void binding_draw0(); void binding_draw1(); void binding_draw2(); void binding_draw3(); void binding_draw4();
void binding_root_cbv(); void binding_root_table(); void binding_root_signature(); void binding_pipeline();
void binding_viewport(); void binding_api_draw(); void binding_api_indexed(); void binding_api_indirect();
void binding_create_cbv(); void binding_reset(); void binding_rollover();
void binding_outputs();void binding_barriers();void binding_create_pipeline();void binding_scissor();
}
namespace mhw::projection_binding {
namespace {
Capture capture;
Runtime rt;
std::uintptr_t image{};
std::array<void *,kind_count> originals{};
std::array<bool,kind_count> created{},enabled{};
constexpr std::array<void(*)(),kind_count> entries={binding_producer,binding_publish,binding_consume,binding_state,
 binding_draw0,binding_draw1,binding_draw2,binding_draw3,binding_draw4,binding_root_cbv,binding_root_table,
 binding_root_signature,binding_pipeline,binding_viewport,binding_api_draw,binding_api_indexed,binding_api_indirect,binding_create_cbv,binding_reset,binding_rollover,
 binding_outputs,binding_barriers,binding_create_pipeline,binding_scissor};
constexpr unsigned open_bit=0x80000000U;
std::atomic<unsigned> gate{},visits{},drops{};
std::atomic<Stop> stopping{Stop::idle};
std::atomic<Phase> phase{};
std::atomic<Fault> fault{};
SRWLOCK lock=SRWLOCK_INIT;
Install installation{};
std::uint64_t started{},sequence{};
bool prepared{},used{},producer_busy{},publication_busy{},consumer_busy{};
std::atomic<unsigned> rollover_pending{},source_arena{3};
std::atomic<bool> producer_active{};
std::atomic<unsigned> cpu_calls{};
std::atomic<unsigned> recovery_deferrals{},draw_waits{};
unsigned sample_count() noexcept {return capture.stimulus.sequence?sequence_samples:3U;}
bool injected_stage(unsigned stage) noexcept {return stage>0 && stage+1<sample_count();}
std::atomic<std::uint64_t> lock_owner{},contention_owner{};
std::atomic<unsigned> contention_site{},contention_thread{};
thread_local unsigned current_draw{};
thread_local unsigned native_depth{};
void end(Stop s) noexcept {
  gate.fetch_and(~open_bit,std::memory_order_acq_rel);
  auto old=Stop::recording; stopping.compare_exchange_strong(old,s);
}
void fail(Fault f) noexcept {
  // Later cleanup checks must not hide the first failure that closed admission.
  auto expected=Fault::none;fault.compare_exchange_strong(expected,f);end(Stop::invalid);
}
bool enter() noexcept {
  auto value=gate.load(std::memory_order_acquire);
  while(value&open_bit) {
    if((value&~open_bit)>=4096) { fail(Fault::overflow); return false; }
    if(gate.compare_exchange_weak(value,value+1,std::memory_order_acq_rel)) return true;
  }
  return false;
}
void leave() noexcept { gate.fetch_sub(1,std::memory_order_release); }
bool acquire(unsigned kind,bool exiting) noexcept {
  const auto site=2*(kind+1)+(exiting?1:0);
  const auto prior_owner=lock_owner.load();
  if(TryAcquireSRWLockExclusive(&lock)) {
    lock_owner.store((static_cast<std::uint64_t>(GetCurrentThreadId())<<32)|site);
    return true;
  }
  // Recovery has no mutation and has not reserved a source at this entry.
  // Leave this native call untouched and select a later eligible frame.
  // Never defer stimulus, an admitted call's exit, or binding evidence.
  if(kind==producer && !exiting && capture.stimulus.enabled && cpu_calls.load()+1==sample_count() && !producer_active.load()) {
    if(recovery_deferrals.fetch_add(1)>=max_recovery_deferrals)end(Stop::capacity);
    return false;
  }
  unsigned empty{};
  if(contention_site.compare_exchange_strong(empty,site)) {
    const auto observed_owner=lock_owner.load();
    contention_owner.store(prior_owner==observed_owner?observed_owner:0);
    contention_thread.store(GetCurrentThreadId());
  }
  drops.fetch_add(1); fail(Fault::contention); return false;
}
void unlock() noexcept {lock_owner.store(0);ReleaseSRWLockExclusive(&lock);}
bool bytes_equal(std::uintptr_t at,const std::byte *data,unsigned size) noexcept {
  std::array<std::byte,64> chunk{};
  for(unsigned i=0;i<size;i+=64) { const auto n=(std::min)(64U,size-i);
    if(!copy_memory(at+i,chunk.data(),n) || std::memcmp(chunk.data(),data+i,n)) return false;
  }
  return true;
}
bool provider(std::uintptr_t renderer) noexcept {
  unsigned sizing{}; std::uintptr_t backend{},wrapper{},device{}; unsigned char use{};
  return read(renderer+0xa5,use) && use<=1 && read(renderer+0xb4,sizing) && !(sizing&1) &&
    read(renderer+0x78,backend) && backend==rt.backend && read(backend+0x18,device) && device==rt.device &&
    read(backend+0x42700,wrapper) && !wrapper;
}
bool provider_snapshot(std::uintptr_t renderer,Provider &p) noexcept {
  unsigned char use{};
  p.valid=read(renderer+0xa5,use) && use<=1 && read(renderer+0xb4,p.sizing) && !(p.sizing&1) &&
    read(renderer+0x78,p.backend) && p.backend==rt.backend && read(p.backend+0x18,p.device) && p.device==rt.device &&
    read(p.backend+0x42700,p.wrapper) && !p.wrapper;
  p.use=use;return p.valid;
}
bool anchor(Source &s,const Frame &f) noexcept {
  auto &a=s.anchor; unsigned char active{},display{},async{}; unsigned stamp{},view_flags{};
  std::uint64_t jitter{};
  if(!read(image+0x51c4480,a.renderer) || !a.renderer || a.renderer>UINTPTR_MAX-0x1f570 ||
     !read(image+0x51c4368,a.table) || !a.table || !read(image+0x51c4400,s.camera) ||
     !s.camera || s.camera>UINTPTR_MAX-0x100 || f.gpr[1]!=s.camera+0x50 || f.gpr[2]!=a.renderer+0x710) return false;
#ifndef MHW_BINDING_FIXTURE
  if(f.caller!=image+0x228e834) return false;
#endif
  a.context=f.gpr[2];
  if(!read(s.camera,s.camera_type) || (s.camera_type!=image+0x350d188 && s.camera_type!=image+0x33f82a8) ||
     !read(s.camera+0x70,active) || !active || !read(s.camera+0x75,display) || display ||
     !read(s.camera+0x38,jitter) || jitter || !read(s.camera+0x40,stamp) ||
     !read(a.renderer+0x1f210,a.frame) || !read(a.renderer+0x1f208,a.epoch) || stamp!=static_cast<unsigned>(a.epoch) ||
     !read(a.renderer+0x1f450,a.view) || !a.view || a.view>UINTPTR_MAX-0x60 ||
     !read(a.view+0x58,view_flags) || (view_flags&0x840000) || !provider(a.renderer) ||
     !read(a.renderer+0x1f200,a.bank) || a.bank>1 || !read(a.renderer+0x93,async) || async>1) return false;
  a.async_mode=async; return true;
}
bool source_same() noexcept {
  const auto &s=capture.source;
  return s.valid && provider(s.anchor.renderer) && matrix_ids_same(image,s) && bytes_equal(s.projection.constants,s.data.data(),s.bytes);
}
bool arena_location(Arena &a,Location &p,const Source &s) noexcept {
  if(!read_arena(rt.backend,a) || a.cpu_resource!=rt.cpu_resource || a.gpu_resource!=rt.gpu_resource || a.mode!=rt.mode) return false;
  p=locate(a,s.projection.constants,s.bytes);
  if(!p.valid || a.gpu[0]!=rt.gpu_base || static_cast<std::uint64_t>(a.stride)*3>rt.gpu_bytes ||
      static_cast<std::uint64_t>(a.stride)*3>rt.cpu_bytes) return false;
  return true;
}
bool packet(std::uintptr_t at,unsigned &count,unsigned &index) noexcept {
  unsigned kind{}; const auto &s=capture.source;
  if(!at || at>UINTPTR_MAX-0x118 || !read(at,kind) || (kind&31)!=11 || !read(at+0x10,count) || !count || count>16) return false;
  unsigned matches{};
  for(unsigned i=0;i<count;++i) { unsigned id{}; std::uintptr_t p{};
    if(!read(at+0x18+16*i,p) || !read(at+0x20+16*i,id)) return false;
    if(id==s.projection.buffer_id && p==s.projection.constants) {index=i;++matches;}
  }
  return matches==1;
}
bool command(Draw &d) noexcept {
  std::uintptr_t state{},backend{},bound{}; unsigned char mode{};
  if(!read(d.context+8,backend) || backend!=rt.backend ||
     !read(d.context+0x1860+8*(capture.source.projection.buffer_id&0xfff),bound) || bound!=capture.source.projection.constants ||
     !read(d.executor+8,state) || !state || state>UINTPTR_MAX-0x400 ||
     !read(state+0x20,d.command) || !d.command || !read(state+0x108,d.pso) || !read(state+0x110,d.root) ||
     !read(state+0x1a8,mode) || mode>1 || !read(state+8,d.heap_cpu) || !read(state+0x10,d.heap_gpu) ||
     !read(state+0x40,d.descriptor_stride)) return false;
  d.binding_mode=mode; d.device=rt.device; return true;
}
bool shader(Draw &d,std::uintptr_t graphics) noexcept {
  std::uintptr_t handle{};
  if(!graphics || graphics>UINTPTR_MAX-16 || !read(graphics+8,handle) || !handle ||
     !read(handle,d.shader_set) || !d.shader_set || d.shader_set>UINTPTR_MAX-0x78) return false;
  d.graphics=graphics;
  for(unsigned i=0;i<5;++i) {
    auto &entry=d.stage_entries[i]; std::uintptr_t desc{},data{}; unsigned size{};
    if(!read(d.shader_set+0x50+8*i,entry)) return false;
    if(!entry) continue;
    if(entry>UINTPTR_MAX-0x18 || !read(entry+0x10,desc) || !desc || desc>UINTPTR_MAX-16 ||
       !read(desc,size) || !size || size>max_shader || !read(desc+8,data) || !data) return false;
    bool found=false;
    for(unsigned j=0;j<capture.blob_count;++j) if(capture.blobs[j].size==size && bytes_equal(data,capture.blobs[j].bytes.data(),size)) {
      d.blobs[i]=j+1;found=true;break;
    }
    if(found) continue;
    if(capture.blob_count>=max_blobs) return false;
    auto &blob=capture.blobs[capture.blob_count];
    if(!copy_memory(data,blob.bytes.data(),size) || !dxbc_valid(blob.bytes.data(),size) || !bytes_equal(data,blob.bytes.data(),size)) return false;
    std::uintptr_t desc2{},data2{}; unsigned size2{};
    if(!read(entry+0x10,desc2) || desc2!=desc || !read(desc,size2) || size2!=size || !read(desc+8,data2) || data2!=data) return false;
    blob.size=size;d.blobs[i]=++capture.blob_count;
  }
  return d.blobs[0]!=0 && capture_root(rt.backend,d.shader_set,d.stage_entries,d.selection);
}
bool shader_same(const Draw &d) noexcept {
  std::uintptr_t handle{},set{};
  if(!read(d.graphics+8,handle) || !handle || !read(handle,set) || set!=d.shader_set)return false;
  for(unsigned i=0;i<5;++i) {
    std::uintptr_t entry{},desc{},data{};unsigned size{};
    if(!read(set+0x50+8*i,entry) || entry!=d.stage_entries[i])return false;
    if(!entry)continue;
    if(!d.blobs[i] || d.blobs[i]>capture.blob_count || !read(entry+0x10,desc) || !desc ||
       !read(desc,size) || !read(desc+8,data))return false;
    const auto &blob=capture.blobs[d.blobs[i]-1];
    if(size!=blob.size || !bytes_equal(data,blob.bytes.data(),size))return false;
  }
  return true;
}
bool root_same(const Draw &d) noexcept {
  RootSelection now{};
  return capture_root(rt.backend,d.shader_set,d.stage_entries,now) && now==d.selection &&
    now.descriptor_mode==d.binding_mode && d.root==now.signatures[now.index];
}
// A token refers only to owned storage. The callback never keeps a lock while
// calling the game, so nested native/API hooks retain their normal call order.
struct Token { unsigned type{},index{},kind{}; bool pending{},draw_scope{}; };
bool same_camera(const Source &a,const Source &b) noexcept {
  return a.camera==b.camera && a.camera_type==b.camera_type && a.thread==b.thread &&
    a.anchor.renderer==b.anchor.renderer && a.anchor.context==b.anchor.context &&
    a.anchor.view==b.anchor.view && a.anchor.table==b.anchor.table;
}
void complete_if_ready() noexcept {
  if(capture.phase==Phase::drawn && (!capture.stimulus.enabled || cpu_calls.load()==sample_count())) {
    end(Stop::complete);
  }
}
Token before(unsigned kind,const Frame &f) noexcept {
  Token t{};t.kind=kind;
  const auto p=phase.load(std::memory_order_acquire);
  if((kind==producer && (capture.stimulus.enabled?cpu_calls.load()>=sample_count():p!=Phase::empty)) || (kind==publish && p!=Phase::source) ||
     (kind==consume && p!=Phase::published) || (kind>=draw0 && kind<=draw4 && p!=Phase::consumed)) return t;
  if(kind>=state_setup && !current_draw && !(kind>=draw0 && kind<=draw4) && kind!=rollover) return t;
  // These keys are immutable once their phase is published with release/acquire
  // ordering. Unrelated traffic must not enter the capture's bookkeeping lock.
  if(kind==producer) {
    std::uintptr_t camera{},renderer{};
    if(!read(image+0x51c4400,camera) || !camera || camera>UINTPTR_MAX-0x50 || f.gpr[1]!=camera+0x50 ||
       !read(image+0x51c4480,renderer) || !renderer || renderer>UINTPTR_MAX-0x710 || f.gpr[2]!=renderer+0x710)return t;
#ifndef MHW_BINDING_FIXTURE
    if(f.caller!=image+0x228e834)return t;
#endif
  }
  if(kind==publish && (f.gpr[1]!=capture.source.anchor.context || static_cast<unsigned>(f.gpr[2])!=capture.source.projection.buffer_id))return t;
  if(kind==consume && f.gpr[3]!=capture.publication.packet)return t;
  if(kind>=draw0 && kind<=draw4 && f.gpr[2]!=capture.consumption.context)return t;
  if(kind==rollover && f.gpr[1]!=rt.backend)return t;
  // Later stimuli cannot reuse the primary source arena until its draw returns.
  // These are unreserved entry skips; no mutation or copied source exists yet.
  if(kind==producer && capture.stimulus.sequence && cpu_calls.load()==2 && p!=Phase::drawn) {
    if(!(gate.load(std::memory_order_acquire)&open_bit))return t;
    if(draw_waits.fetch_add(1)>=max_draw_waits)end(Stop::capacity);
    return t;
  }
  if(!enter()) return t;
  t.pending=true;
  if(GetTickCount64()-started>=window_ms) {end(Stop::deadline);return t;}
  if(visits.fetch_add(1)>=100000) {end(Stop::capacity);return t;}
  if(kind==rollover) {
    // The lifetime guard is independent of capture serialization. It never
    // waits for a shader copy or contends with an unrelated packet callback.
    rollover_pending.fetch_add(1);t.type=7;
    unsigned index{};
    const auto selected=source_arena.load();
    if(!read(rt.backend+0x103c,index) || index>=3 || producer_active.load() ||
       (selected<3 && (index+1)%3==selected))fail(Fault::changed);
    return t;
  }
  if(!acquire(kind,false)) return t;
  if(stopping.load()!=Stop::recording) {unlock();return t;}
  const auto order=++sequence,tick=GetTickCount64(); const auto thread=GetCurrentThreadId();
  if(kind==producer && capture.stimulus.enabled && producer_busy)fail(Fault::reentrant);
  else if(kind==producer && (capture.stimulus.enabled || capture.phase==Phase::empty) && !producer_busy) {
    Source s{};
    if(!rollover_pending.load() && anchor(s,f)) {
      s.thread=thread;s.order=order;s.tick=tick;
      const auto stage=cpu_calls.load();
      // Another producer may have completed recovery after our lock-free
      // admission check while the selected draw is still outstanding.
      if(capture.stimulus.enabled && stage>=sample_count()){unlock();return t;}
      if(capture.stimulus.enabled && stage) {
        const auto &prior=capture.stimulus.calls[stage-1].source;
        if(s.anchor.frame==prior.anchor.frame && s.anchor.epoch==prior.anchor.epoch){unlock();return t;}
        if(!same_camera(s,capture.stimulus.calls[0].source) || s.anchor.frame<=prior.anchor.frame || s.anchor.epoch<=prior.anchor.epoch) {
          fail(Fault::changed);unlock();return t;
        }
      }
      producer_active.store(true);
      if(!rollover_pending.load()) {
        producer_busy=true;t.type=1;t.index=stage;
        if(!capture.stimulus.enabled)capture.source=s;
        else {
          auto &j=capture.stimulus.calls[stage];j.source=s;capture.stimulus.count=stage+1;
          if(s.anchor.table>UINTPTR_MAX-0xe740 || !read(s.anchor.table+0xe6a0+8*(s.anchor.frame%20),j.candidate) ||
             !jitter_finite(j.candidate) || !provider_snapshot(s.anchor.renderer,j.before))fail(Fault::identity);
          else if(injected_stage(stage)) {
            bool repeated=false;
            if(capture.stimulus.sequence)for(unsigned i=1;i<stage;++i)repeated=repeated || capture.stimulus.calls[i].candidate==j.candidate;
            if(repeated){fail(Fault::changed);unlock();return t;}
            if(!jitter_exchange(s.camera+0x38,0,j.candidate))fail(Fault::changed);
            else {j.injected=true;++capture.stimulus.injections;}
          }
        }
      }
      else producer_active.store(false);
    }
  } else if(kind==publish && capture.phase==Phase::source && !publication_busy &&
      f.gpr[1]==capture.source.anchor.context && static_cast<unsigned>(f.gpr[2])==capture.source.projection.buffer_id) {
    if(!source_same()) fail(Fault::changed);
    else {publication_busy=true;t.type=2;}
  } else if(kind==consume && capture.phase==Phase::published && !consumer_busy && f.gpr[3]==capture.publication.packet) {
    unsigned count{},index{}; std::uintptr_t backend{};
    if(!packet(f.gpr[3],count,index) || index!=capture.publication.index || !read(f.gpr[2]+8,backend) || backend!=rt.backend || !source_same()) fail(Fault::changed);
    else { auto &c=capture.consumption; c.thread=thread;c.order=order;c.tick=tick;c.packet=f.gpr[3];c.executor=f.gpr[1];c.context=f.gpr[2];
      c.pointer=capture.source.projection.constants;consumer_busy=true;t.type=3; }
  } else if(kind>=draw0 && kind<=draw4 && capture.phase==Phase::consumed && f.gpr[2]==capture.consumption.context) {
    if(native_depth || current_draw) fail(Fault::reentrant);
    else if(capture.draw_count>=max_draws) end(Stop::capacity);
    else {
      const auto n=capture.draw_count++;auto &d=capture.draws[n];
      d.kind=kind;d.thread=thread;d.order=order;d.tick=tick;d.executor=f.gpr[1];d.context=f.gpr[2];d.packet=f.gpr[3];
      Arena a{};Location loc{};
      if(!command(d) || !arena_location(a,loc,capture.source) || loc.gpu!=capture.consumption.location.gpu ||
         a.delta!=capture.consumption.arena.delta || !source_same()) {d.rejection=1;fail(Fault::changed);}
      else if(capture.attachments_enabled && !(d.attachments.enabled=true,snapshot_attachments(image,d,d.attachments.entry))) {d.rejection=6;fail(Fault::layout);}
      else {d.gpu=loc.gpu;current_draw=n+1;++native_depth;t.type=4;t.index=n;t.draw_scope=true;}
    }
  } else if(current_draw) {
    auto &d=capture.draws[current_draw-1];
    if(kind==state_setup) {
      if(f.gpr[1]!=d.executor || f.gpr[2]!=d.context || f.gpr[4]!=d.packet || d.state_seen) fail(Fault::identity);
      else if(!shader(d,f.gpr[3])) {d.rejection=2;fail(Fault::shader);}
      else if(d.attachments.enabled && (!snapshot_attachments(image,d,d.attachments.ready) ||
          !attachment_inputs_equal(d.attachments.entry,d.attachments.ready) || !attachment_cache_matches(d.attachments.ready))) {d.rejection=7;fail(Fault::changed);}
      else {d.state_seen=true;t.type=5;t.index=current_draw-1;}
      if(d.attachments.enabled)d.attachments.state_order=order;
    } else if(kind>=root_cbv && kind!=rollover) {
      const auto object=f.gpr[1];std::uintptr_t vt{},target{};
      if(object!=((kind==create_cbv || kind==create_pipeline)?rt.device:d.command)) {fail(Fault::identity);}
      else if(!read(object,vt) || !read(vt+8*method_slots[kind],target) || target!=rt.targets[kind]) fail(Fault::api_target);
      else if(capture.api_count>=max_api) end(Stop::capacity);
      else {
        auto &a=capture.apis[capture.api_count];t.type=6;t.index=capture.api_count++;
        a.kind=kind;a.draw=current_draw;a.thread=thread;a.order=order;a.tick=tick;a.object=object;a.caller=f.caller;
        a.args={f.gpr[2],f.gpr[3],f.gpr[4],f.stack[0],f.stack[1],f.stack[2],0};
        const unsigned argc=kind==api_indirect?6:kind==api_indexed?5:kind==api_draw?4:
          (kind==pipeline || kind==root_signature)?1:kind==output_binding?4:kind==create_pipeline?3:2;
        for(unsigned j=argc;j<a.args.size();++j)a.args[j]=0;
        if(kind==viewport && f.gpr[2]==1 && !copy_memory(f.gpr[3],a.viewport.data(),24)) fail(Fault::read);
        if(kind==create_cbv) {
          std::uint64_t gpu{};unsigned size{};
          if(!f.gpr[2] || !read(f.gpr[2],gpu) || !read(f.gpr[2]+8,size)) fail(Fault::read);
          else {a.args[0]=gpu;a.size=size;}
        }
        if(kind==reset) {d.rejection=3;fail(Fault::changed);}
        if(d.attachments.enabled) {
          if(kind==output_binding && (!capture_output_binding(f,a.outputs) || !output_binding_matches(a.outputs,d.attachments.entry)))fail(Fault::layout);
          if(kind==barriers) {
            if(!capture_barriers(f,a.barrier_records,a.barrier_count) || a.barrier_count>max_barriers-d.attachments.barrier_count)fail(Fault::overflow);
            else d.attachments.barrier_count+=a.barrier_count;
          }
          if(kind==scissor && (f.gpr[2]!=1 || !copy_memory(f.gpr[3],a.scissor.data(),16)))fail(Fault::layout);
          if(kind==create_pipeline && !capture_pipeline_creation(f,d,capture,d.creation))fail(Fault::layout);
          if(kind==api_draw || kind==api_indexed || kind==api_indirect) {
            AttachmentSnapshot now{};std::uintptr_t pso{},root{};
            if(!d.attachments.described || !snapshot_attachments(image,d,now) || now!=d.attachments.ready ||
               !read(now.state+0x108,pso) || pso!=d.pso || !read(now.state+0x110,root) || root!=d.root)fail(Fault::changed);
            else {d.attachments.at_draw=true;d.attachments.draw_order=order;}
          }
        }
      }
    }
  }
  unlock();return t;
}
// COM calls occur with no recorder lock held, under the same pending native
// draw token. Pinning is bounded and never submits GPU work.
void enrich_before(const Token &t) noexcept {
  if(!capture.attachments_enabled || !t.type || stopping.load()!=Stop::recording)return;
  if(t.type==5) {
    auto &d=capture.draws[t.index];AttachmentSnapshot now{};
    if(!describe_attachments(d.attachments,rt.device) || !snapshot_attachments(image,d,now) || now!=d.attachments.ready)fail(Fault::identity);
  } else if(t.type==6) {
    const auto &a=capture.apis[t.index];
    if(a.kind==output_binding)provenance::mark_binding(false);
    if(a.kind==api_draw || a.kind==api_indexed || a.kind==api_indirect) {
      if(!recheck_resources(capture.draws[a.draw-1].attachments,1))fail(Fault::changed);
      else provenance::mark_draw(capture.draws[a.draw-1]);
    }
  }
}
void enrich_after(const Token &t,bool returned) noexcept {
  if(capture.attachments_enabled && t.type==6 && returned && capture.apis[t.index].kind==output_binding)provenance::mark_binding(true);
  if(capture.attachments_enabled && t.type==4 && returned && !recheck_resources(capture.draws[t.index].attachments,2))fail(Fault::changed);
}
void after(Token t,const Frame &f,bool returned) noexcept {
  if(!t.type) return;
  if(t.type==7) {
    unsigned index{};const auto selected=source_arena.load();
    if(!returned)fail(Fault::exception);
    else if(!read(rt.backend+0x103c,index) || index>=3 || (selected<3 && index==selected))fail(Fault::changed);
    rollover_pending.fetch_sub(1);return;
  }
  if(!acquire(t.kind,true)) return;
  const auto exit_order=++sequence;
  if(!returned) fail(Fault::exception);
  else if(t.type==1) {
    auto &s=capture.stimulus.enabled?capture.stimulus.calls[t.index].source:capture.source;
    s.projection=bridge_probe::capture_projection_snapshot(image,s.anchor,bridge_probe::read_bounded_scalar,nullptr);
    const auto &p=s.projection;
    if(rollover_pending.load() || !(f.gpr[0]&255) || !p.valid || !p.matrix_relation || p.manager!=s.camera || p.vtable!=s.camera_type ||
       p.tagged_constants!=p.constants || !p.words || p.words>max_constants/4 || !provider(s.anchor.renderer)) fail(Fault::layout);
    else {
      s.bytes=p.words*4;
      if(!arena_location(s.arena,s.location,s) || !copy_memory(p.constants,s.data.data(),s.bytes) ||
         !bytes_equal(p.constants,s.data.data(),s.bytes)) fail(Fault::arena);
      else if(std::memcmp(s.data.data()+4*((p.projection_id>>12)&0x3ff),p.projection_bits.data(),64) ||
              std::memcmp(s.data.data()+4*((p.unjittered_id>>12)&0x3ff),p.unjittered_bits.data(),64))fail(Fault::changed);
      else if(!capture_matrices(image,s))fail(Fault::layout);
      else {
        s.valid=true;
        if(!capture.stimulus.enabled) {
          if(!provenance::select_source(s))fail(Fault::identity);
          source_arena.store(s.location.arena);capture.phase=Phase::source;
        }
        else {
          auto &j=capture.stimulus.calls[t.index];j.returned=true;
          const auto observed=static_cast<std::uint64_t>(p.jitter_bits[0]) | (static_cast<std::uint64_t>(p.jitter_bits[1])<<32);
          const auto sample=static_cast<std::uint64_t>(p.table20_bits[0]) | (static_cast<std::uint64_t>(p.table20_bits[1])<<32);
          if(observed!=(j.injected?j.candidate:0) || sample!=j.candidate ||
             !provider_snapshot(s.anchor.renderer,j.after) || j.before!=j.after ||
             (t.index && p.viewport_bits!=capture.stimulus.calls[0].source.projection.viewport_bits))fail(Fault::changed);
        }
      }
    }
  } else if(t.type==2) {
    auto &p=capture.publication;p.context=capture.source.anchor.context;p.id=capture.source.projection.buffer_id;
    p.pointer=capture.source.projection.constants;p.thread=GetCurrentThreadId();p.order=exit_order;p.tick=GetTickCount64();
    if(!read(p.context+0x1a8,p.packet) || !packet(p.packet,p.count,p.index) || !source_same()) fail(Fault::changed);
    else {p.valid=true;capture.phase=Phase::published;}
  } else if(t.type==3) {
    auto &c=capture.consumption;std::uintptr_t bound{};
    if(!read(c.context+0x1860+8*(capture.source.projection.buffer_id&0xfff),bound) || bound!=c.pointer ||
       !arena_location(c.arena,c.location,capture.source) || c.pointer>UINTPTR_MAX-c.arena.delta ||
       c.pointer+c.arena.delta!=c.location.gpu || !source_same()) fail(Fault::arena);
    else {c.valid=true;capture.phase=Phase::consumed;}
  } else if(t.type==4) {
    auto &d=capture.draws[t.index];d.returned=true;d.exit_order=exit_order;d.source_unchanged=source_same();
    std::uintptr_t state{},root{};
    d.root_stable=root_same(d) && read(d.executor+8,state) && state && read(state+0x110,root) && root==d.root;
    if(!d.source_unchanged || !d.root_stable || !shader_same(d)) {d.rejection=4;fail(Fault::changed);}
    if(d.attachments.enabled) {
      AttachmentSnapshot now{};std::uintptr_t pso{};
      d.attachments.stable=d.attachments.described && d.attachments.at_draw && snapshot_attachments(image,d,now) && now==d.attachments.ready &&
        read(now.state+0x108,pso) && pso==d.pso;
      if(!d.attachments.stable || (d.creation.seen && (!d.creation.succeeded || d.creation.pso!=d.pso || d.creation.root!=d.root))) {d.rejection=8;fail(Fault::changed);}
    }
    // One complete native draw is the deliberately bounded first experiment.
    // A missing actual bind is retained as partial evidence, never promoted.
    // GPU submission may precede the recovery CPU sample. Publish this return
    // at the validated native return, independently of overall completion.
    if(fault.load()==Fault::none)provenance::end_frame();
    capture.phase=Phase::drawn;source_arena.store(3);complete_if_ready();
  } else if(t.type==5) {
    auto &d=capture.draws[t.index];
    if(!command(d) || !shader_same(d) || !root_same(d)) {d.rejection=5;fail(Fault::changed);}
    if(d.attachments.enabled && d.attachments.entry.incoming_pso && d.pso!=d.attachments.entry.incoming_pso)fail(Fault::changed);
  } else if(t.type==6) {
    auto &a=capture.apis[t.index];auto &d=capture.draws[a.draw-1];a.returned=true;a.exit_order=exit_order;
    if(a.kind==create_pipeline && d.attachments.enabled && !finish_pipeline_creation(f,d,capture,d.creation))fail(Fault::layout);
    if(a.kind==root_cbv && a.args[1]==d.gpu) d.root_binding_observed=true;
    if(a.kind==create_cbv && a.args[0]==d.gpu && a.size==capture.consumption.location.aligned_bytes) d.descriptor_observed=true;
    if(a.kind==api_draw || a.kind==api_indexed || a.kind==api_indirect) d.draw_api_observed=true;
  }
  phase.store(capture.phase,std::memory_order_release);unlock();
}
void finish_producer(Token t,bool returned) noexcept {
  if(!capture.stimulus.enabled || t.type!=1)return;
  auto &j=capture.stimulus.calls[t.index];
  // Restoration must not depend on obtaining the bookkeeping lock or on the
  // capture still being open. This path also runs during SEH unwind.
  if(j.injected) {
    j.restored=jitter_exchange(j.source.camera+0x38,j.candidate,0);
    if(j.restored)++capture.stimulus.restorations;else fail(Fault::changed);
  } else j.restored=true;
  if(!read(j.source.camera+0x38,j.restored_bits) || j.restored_bits!=0)fail(Fault::changed);
  if(!returned)fail(Fault::exception);
  if(!acquire(producer,true))return;
  j.exit_order=++sequence;j.complete=true;producer_busy=false;
  if(returned && j.returned && j.source.valid && j.restored && !j.restored_bits && fault.load()==Fault::none) {
    if(!provenance::select_source(j.source,t.index))fail(Fault::identity);
    if(t.index==1) {
      capture.source=j.source;source_arena.store(j.source.location.arena);capture.phase=Phase::source;
    }
    cpu_calls.store(t.index+1,std::memory_order_release);
    complete_if_ready();
  }
  phase.store(capture.phase,std::memory_order_release);unlock();
}
bool install() noexcept {
  const auto init=MH_Initialize();installation={1,0,init,GetLastError()};
  if(init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED) return false;
  for(unsigned i=0;i<kind_count;++i) {
    if(!rt.targets[i]) continue;
    // MinHook trampolines have no unwind metadata. Reject a relocated CALL:
    // an exception in that callee could otherwise unwind through anonymous code.
    // Decode a conservative 14-byte prefix, bounded by a real instruction end.
    std::array<std::byte,32> code{};
    bool safe=copy_memory(rt.targets[i],code.data(),static_cast<unsigned>(code.size()));
    for(unsigned n=0;safe && n<14;) {
      hde64s h{};const auto length=hde64_disasm(code.data()+n,&h);
      if(!length || (h.flags&F_ERROR) || h.opcode==0xe8 || (h.opcode==0xff && (h.modrm_reg==2 || h.modrm_reg==3)))safe=false;
      else {n+=length;if(h.opcode==0xe9 || h.opcode==0xeb || (h.opcode==0xff && h.modrm_reg==4) || h.opcode==0xc3)break;}
    }
    if(!safe) {installation={4,i,MH_ERROR_UNSUPPORTED_FUNCTION,0};return false;}
    const auto s=MH_CreateHook(reinterpret_cast<void *>(rt.targets[i]),reinterpret_cast<void *>(entries[i]),&originals[i]);
    installation={2,i,s,GetLastError()};if(s!=MH_OK)return false;created[i]=true;
  }
  // Enable one owned target at a time; no process-wide MinHook operation.
  for(unsigned i=0;i<kind_count;++i) if(created[i]) {
    const auto s=MH_EnableHook(reinterpret_cast<void *>(rt.targets[i]));installation={3,i,s,GetLastError()};
    if(s!=MH_OK)return false;enabled[i]=true;
  }
  return true;
}
}
bool copy_memory(std::uintptr_t at,void *out,unsigned bytes) noexcept {
  if(!at || !out || !bytes || bytes>max_shader || at>UINTPTR_MAX-bytes) return false;
  for(unsigned done=0;done<bytes;) {
    const auto p=at+done,end=bridge_probe::readable_end(p);
    if(!end) return false;
    done+=static_cast<unsigned>((std::min)(static_cast<std::uintptr_t>(bytes-done),end-p));
  }
  __try {std::memcpy(out,reinterpret_cast<const void *>(at),bytes);return true;}
  __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool read_arena(std::uintptr_t b,Arena &a) noexcept {
  a.backend=b;
  if(!b || b>UINTPTR_MAX-0x10a0 || !read(b+0xff0,a.mode) || !read(b+0x1000,a.stride) || !read(b+0x103c,a.index) ||
     !read(b+0x1058,a.cpu_resource) || !read(b+0x1098,a.delta))return false;
  if(a.mode==2)a.gpu_resource=a.cpu_resource;else if(!read(b+0x1078,a.gpu_resource))return false;
  for(unsigned i=0;i<3;++i) if(!read(b+0x1018+8*i,a.cpu[i]) || !read(b+0x1080+8*i,a.gpu[i]) || !read(b+0x1030+4*i,a.used[i]))return false;
  return a.cpu_resource && a.gpu_resource && a.mode<=2;
}
// Defined in discovery.cpp. Only this worker entry performs COM discovery.
bool discover(std::uintptr_t,Runtime &,std::string &);
bool prepare(std::uintptr_t base,std::string &reason) {
  if(prepared || used) {reason="already-prepared";return false;}
  if(!pool_state().ready) {reason="startup-pool-unavailable";return false;}
  if(!discover(base,rt,reason))return false;
  image=base;capture.stimulus.enabled=true;capture.stimulus.sequence=true;capture.attachments_enabled=true;prepared=true;return true;
}
bool arm(std::string &reason) {
  if(!prepared || used || !pool_state().ready) {reason="not-prepared-used-or-pool-unavailable";return false;}
  used=true;
  if(!install()) {reason="hook-install";disable();return false;}
  if(!provenance::start_frame(rt)){reason="provenance-frame-start";disable();return false;}
  // Observe a bounded prefix before choosing a projection source. This only
  // improves coverage; replay still requires the selected list's actual Reset.
  if(provenance::active()){Sleep(50);if(!provenance::healthy()){reason="provenance-prefix";disable();return false;}}
  started=GetTickCount64();stopping.store(Stop::recording);gate.store(open_bit,std::memory_order_release);return true;
}
void stop() noexcept {end(Stop::requested);}
void poll() noexcept {if(stopping.load()==Stop::recording && GetTickCount64()-started>=window_ms)end(Stop::deadline);}
bool settled() noexcept {return stopping.load()!=Stop::recording && gate.load(std::memory_order_acquire)==0;}
bool disable() noexcept {
  stop();bool ok=true;
  for(unsigned i=0;i<kind_count;++i)if(enabled[i]) {const auto s=MH_DisableHook(reinterpret_cast<void *>(rt.targets[i]));
    if(s==MH_OK || s==MH_ERROR_DISABLED)enabled[i]=false;else ok=false;}
  if(capture.attachments_enabled && settled()) {
    const bool released=release_attachment_resources();if(!released)fail(Fault::exception);
    for(unsigned i=0;i<capture.draw_count;++i)capture.draws[i].attachments.resources_released=released;
    ok=ok && released;
  }
  return ok;
}
Status status() noexcept {
  const auto owner=contention_owner.load();
  return {stopping.load(),phase.load(),fault.load(),gate.load()&~open_bit,visits.load(),drops.load(),
    std::none_of(enabled.begin(),enabled.end(),[](bool b){return b;}),
    contention_site.load(),contention_thread.load(),static_cast<unsigned>(owner),static_cast<unsigned>(owner>>32)};
}
Install install_status() noexcept {return installation;}
const Runtime &runtime() noexcept {return rt;}
const Capture *finished_capture() noexcept {
  if(!settled())return nullptr;
  capture.fault=fault.load();capture.drops=drops.load();capture.stimulus.recovery_deferrals=recovery_deferrals.load();capture.stimulus.draw_waits=draw_waits.load();return &capture;
}
#ifdef MHW_BINDING_FIXTURE
bool install_owned(std::uintptr_t base,const Runtime &r,const std::array<std::uintptr_t,kind_count> &targets,bool stimulus,bool attachments,bool sequence_mode) noexcept {
  std::array<std::uintptr_t,10> native{};
  for(unsigned i=0;i<9;++i)native[i]=targets[i];native[9]=targets[rollover];
  std::string reason;if(!reserve_pool(native,reason))return false;
  image=base;rt=r;rt.targets=targets;capture.stimulus.enabled=stimulus;capture.stimulus.sequence=sequence_mode;capture.attachments_enabled=attachments;prepared=true;return true;
}
bool remove_owned() noexcept {if(!settled() || !disable())return false;
  for(unsigned i=0;i<kind_count;++i)if(created[i] && MH_RemoveHook(reinterpret_cast<void *>(rt.targets[i]))!=MH_OK)return false;
  return true;
}
void expire_owned() noexcept {end(Stop::deadline);}
bool hold_owned(unsigned kind,bool exiting) noexcept {return acquire(kind,exiting);}
void release_owned() noexcept {unlock();}
#endif
}
extern "C" void mhw_binding_dispatch(unsigned kind,mhw::projection_binding::Frame *frame) {
  using namespace mhw::projection_binding;
  if(kind>=kind_count){provenance::dispatch(kind-kind_count,frame);return;}
  const auto incoming=GetLastError();
  const auto history_token=(kind==reset || kind==barriers)?provenance::history_before(kind,*frame):provenance::Token{};
  const auto token=before(kind,*frame);
  enrich_before(token);
  bool returned=false;DWORD outgoing=incoming;
  SetLastError(incoming);
  __try {mhw_binding_forward(originals[kind],frame);outgoing=GetLastError();returned=true;}
  __finally {
    const auto error=returned?outgoing:GetLastError();
    provenance::after(history_token,*frame,returned);
    enrich_after(token,returned);
    after(token,*frame,returned);
    finish_producer(token,returned);
    if(token.type==1)producer_active.store(false);
    if(token.draw_scope) {current_draw=0;--native_depth;}
    if(token.pending)leave();SetLastError(error);
  }
}
