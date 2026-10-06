#include "submission.hpp"
#include "../projection_binding/memory.hpp"
#include <MinHook.h>
#include <atomic>
#include <algorithm>
#include <ostream>
#ifdef MHW_TEMPORAL_PRESENT_STATE
#include "../temporal_present/observer.hpp"
#endif

namespace mhw::temporal_bindings::submission {
namespace {
namespace pb=projection_binding;
using ResetFn=HRESULT(WINAPI *)(ID3D12GraphicsCommandList *,ID3D12CommandAllocator *,ID3D12PipelineState *);
using CloseFn=HRESULT(WINAPI *)(ID3D12GraphicsCommandList *);
using ExecuteFn=void(WINAPI *)(ID3D12CommandQueue *,UINT,ID3D12CommandList *const *);
enum Kind : unsigned {reset,close,execute,dispatch,native_copy,resource_barrier};
struct Event {
  unsigned kind{},thread{},first{},count{},result{0x8000000a};
  std::uintptr_t object{},allocator{};
  std::uint64_t enter{},exit{},enter_tick{},exit_tick{};bool returned{},valid{};
};
struct List {
  ID3D12GraphicsCommandList *held{};std::uintptr_t object{},identity{},allocator{};
  std::uint64_t reset{},close{},busy{};
};
struct Queue {ID3D12CommandQueue *held{};std::uintptr_t object{},identity{};};
struct Allocator {ID3D12CommandAllocator *held{};std::uintptr_t object{},identity{};};
struct Sample {
  std::uint64_t inline_begin{},inline_end{};bool inline_ok{};
  motion::Site copy,color,depth;std::uint64_t forward{};bool copy_ok{},color_ok{},depth_ok{};
  ID3D12Fence *fence{};std::uintptr_t fence_object{},command{},allocator{},queue{},packet{};
  std::uint64_t frame{},reset{},dispatch{},dispatch_exit{},close{},execute{},execute_exit{};
  std::uint64_t signal{},signal_exit{},completed_order{},completed_value{},completed_tick{},retire{},retire_exit{};
  unsigned event{UINT_MAX},signal_attempts{},signal_result{0x8000000a};
  bool signaled{},signal_attempted{},completed{};
};
Config cfg;
std::array<std::uintptr_t,3> targets{};
std::array<void *,3> originals{};
std::array<bool,3> enabled{};
std::array<Event,event_limit> events;
std::array<std::uintptr_t,submitted_limit> submitted;
std::array<List,list_limit> lists;
std::array<Queue,queue_limit> queues;
std::array<Allocator,allocator_limit> allocators;
std::array<Sample,capacity> samples;
unsigned event_count{},submitted_count{},list_count{},queue_count{},allocator_count{},pins{},released{};
#ifdef MHW_TEMPORAL_FIXTURE
bool signal_failure{};
void (*before_close_return)(){};
#endif
std::uint64_t order{},started{},stopped_tick{};
std::atomic<unsigned> pending{},fault{};
std::atomic<bool> accepting{};
bool configured{},retained{},hooks_disabled{},copies_enabled{},colors_enabled{},depths_enabled{};
SRWLOCK mutex=SRWLOCK_INIT;
struct Lock {Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
void fail(unsigned n) noexcept {unsigned zero{};fault.compare_exchange_strong(zero,n);accepting.store(false,std::memory_order_release);}
bool enter() noexcept {
  if(!accepting.load(std::memory_order_acquire))return false;
  pending.fetch_add(1,std::memory_order_acq_rel);
  if(!accepting.load(std::memory_order_acquire)){pending.fetch_sub(1,std::memory_order_release);return false;}return true;
}
void leave() noexcept {pending.fetch_sub(1,std::memory_order_release);}
bool checked(std::uintptr_t p,unsigned slot,std::uintptr_t expected=0) noexcept {
  std::uintptr_t at{};return pb::system_method(p,slot,at) && (!expected || at==expected);
}
bool drop(IUnknown *p) noexcept {
  if(!p)return true;if(!checked(reinterpret_cast<std::uintptr_t>(p),2))return false;
  __try{p->Release();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// QI pins prevent address reuse throughout the journal. Only system D3D12
// objects from the discovered device are admitted. Never hold the lock across
// a hooked original Reset/Close/Execute or a private queue Signal.
template<class T> bool pin(std::uintptr_t object,T **out,std::uintptr_t &identity) noexcept {
  if(!checked(object,0) || !checked(object,2) || !checked(object,7))return false;
  T *held{};ID3D12Device *device{};IUnknown *id{};bool ok=false;
  __try {
    auto p=reinterpret_cast<T *>(object);
    ok=SUCCEEDED(p->QueryInterface(IID_PPV_ARGS(&held))) && reinterpret_cast<std::uintptr_t>(held)==object &&
       SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&device))) && reinterpret_cast<std::uintptr_t>(device)==cfg.device &&
       SUCCEEDED(held->QueryInterface(IID_PPV_ARGS(&id))) && id;
    if(ok)identity=reinterpret_cast<std::uintptr_t>(id);
  }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  const bool a=drop(id),b=drop(device);ok=ok && a && b;
  if(ok){*out=held;++pins;}else if(!drop(held))fail(1);
  return ok;
}
unsigned find_list(std::uintptr_t p) noexcept {for(unsigned i=0;i<list_count;++i)if(lists[i].object==p)return i;return UINT_MAX;}
unsigned list_for(std::uintptr_t p) noexcept {
  auto i=find_list(p);if(i!=UINT_MAX)return i;
  if(list_count==list_limit){fail(2);return UINT_MAX;}
  if(!checked(p,8) || !checked(p,9,targets[1]) || !checked(p,10,targets[0])){fail(3);return UINT_MAX;}
  auto &v=lists[list_count];
  if(!pin(p,&v.held,v.identity))return UINT_MAX;
  bool direct=false;__try{direct=v.held->GetType()==D3D12_COMMAND_LIST_TYPE_DIRECT;}__except(EXCEPTION_EXECUTE_HANDLER){fail(3);}
  if(!direct){if(drop(v.held)){++released;v={};}else fail(1);return UINT_MAX;}
  v.object=p;return list_count++;
}
bool allocator_for(std::uintptr_t p) noexcept {
  for(unsigned i=0;i<allocator_count;++i)if(allocators[i].object==p)return true;
  if(allocator_count==allocator_limit){fail(2);return false;}
  auto &v=allocators[allocator_count];if(!pin(p,&v.held,v.identity)){fail(3);return false;}
  v.object=p;++allocator_count;return true;
}
unsigned queue_for(std::uintptr_t p) noexcept {
  for(unsigned i=0;i<queue_count;++i)if(queues[i].object==p)return i;
  if(queue_count==queue_limit){fail(2);return UINT_MAX;}
  if(!checked(p,10,targets[2]) || !checked(p,14) || !checked(p,18)){fail(3);return UINT_MAX;}
  auto &v=queues[queue_count];if(!pin(p,&v.held,v.identity))return UINT_MAX;
  bool direct=false;__try{direct=v.held->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT;}__except(EXCEPTION_EXECUTE_HANDLER){fail(3);}
  if(!direct){if(drop(v.held)){++released;v={};}else fail(1);return UINT_MAX;}
  v.object=p;return queue_count++;
}
unsigned reserve(unsigned kind,std::uintptr_t object) noexcept {
  const auto tick=GetTickCount64();if(tick-started>=2000){fail(16);return UINT_MAX;}
  if(event_count==event_limit){fail(2);return UINT_MAX;}
  const auto i=event_count++;auto &e=events[i];e.kind=kind;e.object=object;e.thread=GetCurrentThreadId();e.enter=++order;e.enter_tick=tick;return i;
}
unsigned list_before(unsigned kind,ID3D12GraphicsCommandList *p,ID3D12CommandAllocator *allocator) noexcept {
  if(!enter())return UINT_MAX;
  Lock lock;const auto object=reinterpret_cast<std::uintptr_t>(p);const auto i=list_for(object);
  if(i==UINT_MAX || !healthy()){leave();return UINT_MAX;}
  auto &v=lists[i];const auto a=reinterpret_cast<std::uintptr_t>(allocator);
  if(v.busy || (kind==reset && !allocator_for(a))){fail(4);leave();return UINT_MAX;}
  const auto token=reserve(kind,object);if(token==UINT_MAX){leave();return token;}
  events[token].allocator=a;v.busy=events[token].enter;return token;
}
void list_after(unsigned token,HRESULT hr,bool returned,ID3D12PipelineState *initial=nullptr) noexcept {
  (void)initial;
  if(token==UINT_MAX)return;
  Lock lock;auto &e=events[token];e.exit=++order;e.exit_tick=GetTickCount64();e.returned=returned;e.result=static_cast<unsigned>(hr);
  if(e.exit_tick-started>=2000)fail(16);
  auto &v=lists[find_list(e.object)];e.valid=returned && SUCCEEDED(hr) && v.busy==e.enter;v.busy=0;
  if(!e.valid){fail(5);leave();return;}
  if(e.kind==reset) {
    for(auto &s:samples)if(s.command==v.object && s.reset==v.reset && s.dispatch) {
      if(!s.execute_exit || s.execute_exit>=e.enter || s.retire){fail(6);break;}
      s.retire=e.enter;s.retire_exit=e.exit;
    }
    v.reset=e.enter;v.allocator=e.allocator;v.close=0;
#ifdef MHW_TEMPORAL_PRESENT_STATE
    if(healthy())temporal_present::observer::reset(reinterpret_cast<ID3D12GraphicsCommandList *>(e.object),e.enter,initial);
#endif
  } else {
    for(auto &s:samples)if(s.command==v.object && s.reset==v.reset && s.dispatch) {
      if(!s.dispatch_exit || s.dispatch_exit>=e.enter || s.close || v.close){fail(7);break;}
      s.close=e.enter;
    }
    v.close=e.enter;
  }
  leave();
}
HRESULT reset_call(ID3D12GraphicsCommandList *p,ID3D12CommandAllocator *a,ID3D12PipelineState *s) {
  const auto token=list_before(reset,p,a);HRESULT result=E_FAIL;bool returned=false;
  __try{result=reinterpret_cast<ResetFn>(originals[0])(p,a,s);returned=true;}
  __finally{list_after(token,result,returned,s);}return result;
}
HRESULT close_call(ID3D12GraphicsCommandList *p) {
  const auto token=list_before(close,p,nullptr);HRESULT result=E_FAIL;bool returned=false;
  __try{result=reinterpret_cast<CloseFn>(originals[1])(p);returned=true;
#ifdef MHW_TEMPORAL_FIXTURE
    if(token!=UINT_MAX && before_close_return)before_close_return();
#endif
  }
  __finally{list_after(token,result,returned);}return result;
}
HRESULT WINAPI reset_hook(ID3D12GraphicsCommandList *p,ID3D12CommandAllocator *a,ID3D12PipelineState *s){return reset_call(p,a,s);}
HRESULT WINAPI close_hook(ID3D12GraphicsCommandList *p){return close_call(p);}
unsigned execute_before(ID3D12CommandQueue *q,unsigned n,ID3D12CommandList *const *data) noexcept {
  if(!enter())return UINT_MAX;
  Lock lock;const auto object=reinterpret_cast<std::uintptr_t>(q);
  if(queue_for(object)==UINT_MAX || !healthy()){leave();return UINT_MAX;}
  if(!n || n>batch_limit || n>submitted_limit-submitted_count){fail(2);leave();return UINT_MAX;}
  std::array<std::uintptr_t,batch_limit> batch{};
  if(!pb::copy_memory(reinterpret_cast<std::uintptr_t>(data),batch.data(),n*sizeof(std::uintptr_t))){fail(8);leave();return UINT_MAX;}
  const auto token=reserve(execute,object);if(token==UINT_MAX){leave();return token;}
  auto &e=events[token];e.first=submitted_count;e.count=n;
  for(unsigned j=0;j<n;++j) {
    submitted[submitted_count++]=batch[j];const auto i=find_list(batch[j]);if(i==UINT_MAX)continue;
    auto &v=lists[i];if(v.busy){fail(4);continue;}v.busy=e.enter;
    for(const auto &s:samples)if(s.dispatch && s.command==v.object && s.reset==v.reset)
      if(!v.close || s.close!=v.close || s.close>=e.enter || s.execute || s.retire)fail(9);
  }
  return token;
}
HRESULT private_signal(ID3D12CommandQueue *queue,ID3D12Fence *fence) noexcept {
  __try {
#ifdef MHW_TEMPORAL_FIXTURE
    if(signal_failure)return E_FAIL;
#endif
    return queue->Signal(fence,1);
  }__except(EXCEPTION_EXECUTE_HANDLER){return E_FAIL;}
}
std::uint64_t fence_value(ID3D12Fence *fence) noexcept {
  __try{return fence->GetCompletedValue();}__except(EXCEPTION_EXECUTE_HANDLER){return UINT64_MAX;}
}
bool signal_one(unsigned i,ID3D12CommandQueue *queue) noexcept {
  ID3D12Fence *fence{};
  {Lock lock;auto &s=samples[i];if(!healthy() || s.signal_attempted)return false;
    s.signal_attempted=true;++s.signal_attempts;s.signal=++order;fence=s.fence;}
  const auto result=private_signal(queue,fence);
  {Lock lock;auto &s=samples[i];s.signal_exit=++order;s.signal_result=static_cast<unsigned>(result);
    s.signaled=SUCCEEDED(result);if(!s.signaled)fail(10);}
  return SUCCEEDED(result);
}
void execute_after(unsigned token,ID3D12CommandList *const *data,bool returned) noexcept {
  if(token==UINT_MAX)return;
  std::array<bool,capacity> signal{};ID3D12CommandQueue *queue{};
  {
    Lock lock;auto &e=events[token];e.exit=++order;e.exit_tick=GetTickCount64();e.returned=returned;e.result=0;
    if(e.exit_tick-started>=2000)fail(16);
    std::array<std::uintptr_t,batch_limit> batch{};
    e.valid=returned && pb::copy_memory(reinterpret_cast<std::uintptr_t>(data),batch.data(),e.count*sizeof(std::uintptr_t));
    for(unsigned j=0;j<e.count;++j) {
      const auto p=submitted[e.first+j];e.valid=e.valid && batch[j]==p;const auto index=find_list(p);if(index==UINT_MAX)continue;
      auto &v=lists[index];e.valid=e.valid && v.busy==e.enter;v.busy=0;
      for(unsigned i=0;i<capacity;++i){auto &s=samples[i];if(s.dispatch && s.command==p && s.reset==v.reset) {
        if(s.execute || !s.close || s.close>=e.enter){e.valid=false;continue;}
        s.execute=e.enter;s.execute_exit=e.exit;s.queue=e.object;signal[i]=true;
      }}
    }
    if(!e.valid)fail(11);
    queue=reinterpret_cast<ID3D12CommandQueue *>(e.object);
  }
  for(unsigned i=0;i<capacity;++i)if(signal[i] && healthy())signal_one(i,queue);
  leave();
}
void execute_call(ID3D12CommandQueue *q,unsigned n,ID3D12CommandList *const *data) {
  const auto token=execute_before(q,n,data);bool returned=false;
  __try{reinterpret_cast<ExecuteFn>(originals[2])(q,n,data);returned=true;}
  __finally{execute_after(token,data,returned);}
}
void WINAPI execute_hook(ID3D12CommandQueue *q,UINT n,ID3D12CommandList *const *data){execute_call(q,n,data);}
bool prepare(const Config &c) noexcept {
  if(configured)return false;configured=true;cfg=c;
  auto d=reinterpret_cast<ID3D12Device *>(cfg.device);
  ID3D12CommandAllocator *a{};ID3D12GraphicsCommandList *l{};ID3D12CommandQueue *q{};
  D3D12_COMMAND_QUEUE_DESC desc{};desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;bool ok=false;
  __try {
    ok=SUCCEEDED(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a))) &&
       SUCCEEDED(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,a,nullptr,IID_PPV_ARGS(&l))) && SUCCEEDED(l->Close()) &&
       SUCCEEDED(d->CreateCommandQueue(&desc,IID_PPV_ARGS(&q)));
    if(ok)ok=pb::system_method(reinterpret_cast<std::uintptr_t>(l),10,targets[0]) &&
      pb::system_method(reinterpret_cast<std::uintptr_t>(l),9,targets[1]) && pb::system_method(reinterpret_cast<std::uintptr_t>(q),10,targets[2]);
    for(unsigned i=0;ok && i<capacity;++i) {
      ok=SUCCEEDED(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&samples[i].fence)));
      if(ok){++pins;samples[i].fence_object=reinterpret_cast<std::uintptr_t>(samples[i].fence);}
    }
  }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
  const bool x=drop(q),y=drop(l),z=drop(a);return ok && x && y && z;
}
}
bool install(const Config &c,std::string &reason) {
  if(!prepare(c)){reason="submission-api-discovery";return false;}
  const std::array<void *,3> hooks={reinterpret_cast<void *>(reset_hook),reinterpret_cast<void *>(close_hook),reinterpret_cast<void *>(execute_hook)};
  for(unsigned i=0;i<3;++i) {
    for(unsigned j=0;j<i;++j)if(targets[i]==targets[j]){reason="submission-target-alias";return false;}
    const auto result=MH_CreateHook(reinterpret_cast<void *>(targets[i]),hooks[i],&originals[i]);
    if(result!=MH_OK){reason="submission-create-"+std::to_string(i)+"-"+std::to_string(result);return false;}
  }
  for(unsigned i=0;i<3;++i){const auto result=MH_EnableHook(reinterpret_cast<void *>(targets[i]));
    if(result!=MH_OK){reason="submission-enable-"+std::to_string(i)+"-"+std::to_string(result);return false;}enabled[i]=true;}
  return true;
}
void begin(std::uint64_t tick) noexcept {started=tick;accepting.store(true,std::memory_order_release);}
bool active() noexcept {return configured;}
void enable_copies(bool colors) noexcept {copies_enabled=true;colors_enabled=colors;}
void stop() noexcept {accepting.store(false,std::memory_order_release);Lock lock;if(!stopped_tick)stopped_tick=GetTickCount64();}
unsigned recorded_events() noexcept {Lock lock;return event_count;}
bool admission_open() noexcept {return accepting.load(std::memory_order_acquire);}
std::uint64_t cutoff_tick() noexcept {Lock lock;return stopped_tick;}
bool warmed() noexcept {return !configured || GetTickCount64()-started>=warmup_ms;}
bool healthy() noexcept {return fault.load(std::memory_order_acquire)==0;}
bool settled() noexcept {return pending.load(std::memory_order_acquire)==0;}
unsigned observation_begin(ID3D12GraphicsCommandList *command,MarkerFailure &reason,unsigned kind,RecordingStamp *stamp) noexcept {
  reason=MarkerFailure::none;
  if(!configured){reason=MarkerFailure::not_configured;return UINT_MAX;}
  if(!enter()){reason=MarkerFailure::admission_closed;return UINT_MAX;}
  Lock lock;const auto at=find_list(reinterpret_cast<std::uintptr_t>(command));
  // A missing warmup Reset is an explicit unqualified marker, not a guessed generation.
  if(at==UINT_MAX){reason=MarkerFailure::unknown_list;leave();return UINT_MAX;}auto &v=lists[at];
  if(!v.reset)reason=MarkerFailure::missing_reset;
  else if(v.close)reason=MarkerFailure::list_closed;
  else if(v.busy)reason=MarkerFailure::list_busy;
  else if(!v.allocator)reason=MarkerFailure::missing_allocator;
  if(reason!=MarkerFailure::none){leave();return UINT_MAX;}
  const auto token=reserve(kind,v.object);if(token==UINT_MAX){reason=MarkerFailure::event_budget_or_deadline;leave();return token;}
  v.busy=events[token].enter;if(stamp)*stamp={v.reset,v.busy,v.object,v.allocator};return token;
}
unsigned native_copy_begin(ID3D12GraphicsCommandList *command,MarkerFailure &reason) noexcept {
  return observation_begin(command,reason,native_copy,nullptr);
}
unsigned followup_begin(unsigned receipt,ID3D12GraphicsCommandList *command,std::uint64_t generation) noexcept {
  {Lock lock;if(receipt>=capacity)return UINT_MAX;const auto &s=samples[receipt];
    if(!s.dispatch_exit||!s.inline_ok||s.command!=reinterpret_cast<std::uintptr_t>(command)||s.reset!=generation||s.close||s.execute||s.retire)return UINT_MAX;}
  MarkerFailure reason{};RecordingStamp stamp{};const auto token=observation_begin(command,reason,native_copy,&stamp);
  if(token==UINT_MAX)return token;
  if(stamp.reset!=generation){native_copy_end(token,false);return UINT_MAX;}return token;
}
unsigned barrier_begin(ID3D12GraphicsCommandList *command,MarkerFailure &reason,RecordingStamp &stamp) noexcept {
  return observation_begin(command,reason,resource_barrier,&stamp);
}
void observation_end(unsigned token,bool returned,unsigned kind) noexcept {
  if(token==UINT_MAX)return;
  Lock lock;if(token>=event_count || events[token].kind!=kind || events[token].exit){fail(18);return;}
  auto &e=events[token];const auto at=find_list(e.object);
  e.exit=++order;e.exit_tick=GetTickCount64();e.returned=returned;e.result=0;
  e.valid=returned && at!=UINT_MAX && lists[at].busy==e.enter;
  if(at!=UINT_MAX)lists[at].busy=0;
  if(!e.valid || e.exit_tick-started>=2000)fail(18);leave();
}
void native_copy_end(unsigned token,bool returned) noexcept {observation_end(token,returned,native_copy);}
void barrier_end(unsigned token,bool returned) noexcept {observation_end(token,returned,resource_barrier);}
bool mark(unsigned i,std::uint64_t frame,std::uintptr_t packet,ID3D12GraphicsCommandList *command,RecordingStamp *stamp) noexcept {
  if(!configured)return true;
  if(!enter())return false;
  Lock lock;const auto at=find_list(reinterpret_cast<std::uintptr_t>(command));
  if(i>=capacity || at==UINT_MAX){fail(12);leave();return false;}
  auto &v=lists[at];auto &s=samples[i];
  if(s.dispatch || !v.reset || v.close || v.busy || !v.allocator){fail(12);leave();return false;}
  if(depths_enabled && (!s.depth_ok || !s.depth.end || s.depth.frame!=frame || s.depth.packet!=packet ||
     s.depth.command!=v.object || s.depth.reset!=v.reset || s.depth.allocator!=v.allocator)){fail(19);leave();return false;}
  const auto token=reserve(dispatch,v.object);if(token==UINT_MAX){leave();return false;}
  s.frame=frame;s.packet=packet;s.command=v.object;s.allocator=v.allocator;s.reset=v.reset;
  s.event=token;s.dispatch=events[token].enter;v.busy=s.dispatch;
  if(stamp)*stamp={v.reset,v.busy,v.object,v.allocator};return true;
}
void mark_end(unsigned i,bool returned) noexcept {
  if(!configured)return;
  Lock lock;if(i>=capacity || samples[i].event==UINT_MAX)return;
  auto &s=samples[i];auto &e=events[s.event];if(e.exit)return;
  auto &v=lists[find_list(s.command)];e.exit=++order;e.exit_tick=GetTickCount64();e.returned=returned;e.result=0;e.valid=returned && v.busy==e.enter;
  if(e.exit_tick-started>=2000)fail(16);
  v.busy=0;s.dispatch_exit=e.exit;if(!e.valid)fail(13);leave();
}
void enable_depth_copies() noexcept {depths_enabled=true;}
bool depth_begin(unsigned i,unsigned token,std::uint64_t frame,std::uintptr_t packet,std::uintptr_t resource,std::uintptr_t identity,motion::Site &out) noexcept {
  Lock lock;if(!depths_enabled || !healthy() || !accepting.load(std::memory_order_acquire) || i>=capacity || token>=event_count)return false;
  auto &s=samples[i];const auto &e=events[token];const auto at=find_list(e.object);
  if(e.kind!=native_copy || e.exit || at==UINT_MAX || s.depth.begin || s.dispatch || !frame || !packet || !resource || !identity)return false;
  const auto &v=lists[at];if(v.busy!=e.enter || !v.reset || v.close || !v.allocator)return false;
  s.depth={i,frame,v.reset,e.enter,++order,0,packet,v.object,v.allocator,resource,identity};out=s.depth;return true;
}
std::uint64_t depth_end(unsigned i,bool ok) noexcept {
  Lock lock;if(i>=capacity || !samples[i].depth.begin || samples[i].depth.end){fail(19);return 0;}
  auto &s=samples[i];s.depth.end=++order;s.depth_ok=ok;if(!ok)fail(19);return s.depth.end;
}
bool depth_completion(unsigned i,motion::Completion &out) noexcept {
  Lock lock;if(!depths_enabled || !healthy() || !hooks_disabled || !settled() || retained || i>=capacity)return false;
  const auto &s=samples[i];const auto &p=s.depth;
  if(!s.completed || !s.retire_exit || !s.depth_ok || !s.forward || !p.end || p.end>=s.dispatch ||
     p.frame!=s.frame || p.packet!=s.packet || p.command!=s.command || p.reset!=s.reset || p.allocator!=s.allocator)return false;
  out={p,s.forward,s.dispatch_exit,s.close,s.execute,s.execute_exit,s.signal,s.signal_exit,s.completed_order,s.retire,s.retire_exit,s.queue,s.fence_object};return true;
}
bool copy_begin(unsigned i,std::uintptr_t resource,std::uintptr_t identity,motion::Site &site,bool color) noexcept {
  Lock lock;if(!configured || !copies_enabled || !accepting.load(std::memory_order_acquire) || !healthy() || i>=capacity)return false;
  auto &s=samples[i];auto &c=color?s.color:s.copy;
  if(!s.dispatch || s.dispatch_exit || c.begin || s.forward || !resource || !identity)return false;
  if(color && (!colors_enabled || !s.copy_ok || !s.copy.end || resource==s.copy.resource || identity==s.copy.identity))return false;
  c.receipt=i;c.frame=s.frame;c.packet=s.packet;c.command=s.command;c.allocator=s.allocator;
  c.resource=resource;c.identity=identity;c.reset=s.reset;c.marker=s.dispatch;c.begin=++order;site=c;return true;
}
std::uint64_t copy_end(unsigned i,bool ok,bool color) noexcept {
  Lock lock;if(i>=capacity){fail(17);return 0;}auto &s=samples[i];auto &c=color?s.color:s.copy;
  if(!c.begin || c.end){fail(17);return 0;}
  c.end=++order;(color?s.color_ok:s.copy_ok)=ok;if(!ok)fail(17);return c.end;
}
void dispatch_forward(unsigned i) noexcept {
  if(!configured)return;Lock lock;if(i>=capacity || !samples[i].dispatch)return;
  auto &s=samples[i];if(s.forward || s.dispatch_exit){fail(17);return;}s.forward=++order;
  if(copies_enabled && (!s.copy_ok || !s.copy.end || s.copy.end>=s.forward))fail(17);
  if(colors_enabled && (!s.color_ok || !s.color.end || s.copy.end>=s.color.begin || s.color.end>=s.forward))fail(17);
}
bool copy_completion(unsigned i,motion::Completion &out,bool color) noexcept {
  Lock lock;if(!copies_enabled || !healthy() || !hooks_disabled || !settled() || retained || i>=capacity)return false;
  const auto &s=samples[i];if(!s.completed || !s.retire_exit || !s.copy_ok || !s.forward || (colors_enabled && !s.color_ok) || (color && !colors_enabled))return false;
  out={color?s.color:s.copy,s.forward,s.dispatch_exit,s.close,s.execute,s.execute_exit,s.signal,s.signal_exit,s.completed_order,s.retire,s.retire_exit,s.queue,s.fence_object};
  return true;
}
bool inline_begin(unsigned i,std::uint64_t frame,ID3D12GraphicsCommandList *list,std::uint64_t generation) noexcept {
  Lock lock;if(!configured || !healthy() || !accepting.load(std::memory_order_acquire) || i>=capacity)return false;
  auto &s=samples[i];const auto at=find_list(reinterpret_cast<std::uintptr_t>(list));
  if(at==UINT_MAX || !depths_enabled || !copies_enabled || !colors_enabled || !s.depth_ok || !s.copy_ok || !s.color_ok ||
     s.frame!=frame || s.command!=reinterpret_cast<std::uintptr_t>(list) || s.reset!=generation || !s.forward || s.dispatch_exit || s.inline_begin)return false;
  const auto &v=lists[at];if(v.reset!=generation || v.close || v.busy!=s.dispatch || v.allocator!=s.allocator)return false;
  for(const auto *p:{&s.depth,&s.copy,&s.color})if(!p->end || p->end>=s.forward || p->frame!=frame || p->command!=s.command ||
     p->reset!=generation || p->allocator!=s.allocator || p->packet!=s.packet)return false;
  if(s.depth.end>=s.dispatch || s.copy.end>=s.color.begin)return false;
  s.inline_begin=++order;return true;
}
void inline_end(unsigned i,bool ok) noexcept {
  Lock lock;if(i>=capacity || !samples[i].inline_begin || samples[i].inline_end){fail(20);return;}
  auto &s=samples[i];s.inline_end=++order;s.inline_ok=ok;if(!ok)fail(20);
}
void poll() noexcept {
  if(!configured || !healthy())return;
  if(GetTickCount64()-started>=2000){fail(16);return;}
  for(unsigned i=0;i<capacity;++i) {
    ID3D12Fence *fence{};
    {Lock lock;if(!samples[i].signaled || samples[i].completed)continue;fence=samples[i].fence;}
    const auto value=fence_value(fence);
    Lock lock;auto &s=samples[i];if(value==UINT64_MAX || value>1){fail(14);return;}
    if(value==1){s.completed_tick=GetTickCount64();if(s.completed_tick-started>=2000){fail(16);return;}
      s.completed=true;s.completed_value=value;s.completed_order=++order;}
  }
}
bool complete() noexcept {
  if(!configured)return true;Lock lock;if(!healthy())return false;
  for(const auto &s:samples)if(!s.completed || !s.retire_exit || (copies_enabled && !s.copy_ok) || (colors_enabled && !s.color_ok))return false;return true;
}
bool completed_receipt(unsigned i,ID3D12Fence **held) noexcept {
  if(held)*held=nullptr;
  Lock lock;if(!configured||!healthy()||i>=capacity)return false;
  const auto &s=samples[i];
  if(!s.completed||s.completed_value!=1||!s.retire_exit||!s.dispatch_exit||!s.signal_exit||!s.fence)return false;
  if(held){s.fence->AddRef();*held=s.fence;}return true;
}
bool disabled() noexcept {return !enabled[0]&&!enabled[1]&&!enabled[2];}
bool finish(ID3D12Fence **completed_fence,unsigned receipt) noexcept {
  if(completed_fence)*completed_fence=nullptr;
  if(!configured)return true;stop();bool ok=true;
  for(unsigned i=0;i<3;++i)if(enabled[i]){const auto r=MH_DisableHook(reinterpret_cast<void *>(targets[i]));
    if(r==MH_OK || r==MH_ERROR_DISABLED)enabled[i]=false;else ok=false;}
  hooks_disabled=!enabled[0] && !enabled[1] && !enabled[2];
  const auto until=GetTickCount64()+2000;while(!settled() && GetTickCount64()<until)Sleep(1);
  if(!settled())return false;
  Lock lock;
  for(const auto &s:samples)if(s.signal_attempted && !s.completed)retained=true;
  if(retained)return false;
  if(completed_fence){
    if(receipt>=capacity)return false;const auto &s=samples[receipt];
    if(!healthy() || !s.inline_ok || !s.inline_end || !s.completed || !s.retire_exit || !s.fence || !s.dispatch_exit || s.inline_end>=s.dispatch_exit)return false;
    s.fence->AddRef();*completed_fence=s.fence;
  }
  // Pixel-copy owners retain their own source/list/allocator/buffer references
  // for process lifetime. This module only releases its private fence/journal
  // references after every attempted Signal has observed completion.
  for(auto &s:samples)if(s.fence){if(drop(s.fence)){s.fence=nullptr;++released;}else {ok=false;fail(1);}}
  for(auto &v:lists)if(v.held){if(drop(v.held)){v.held=nullptr;++released;}else {ok=false;fail(1);}}
  for(auto &v:queues)if(v.held){if(drop(v.held)){v.held=nullptr;++released;}else {ok=false;fail(1);}}
  for(auto &v:allocators)if(v.held){if(drop(v.held)){v.held=nullptr;++released;}else {ok=false;fail(1);}}
  bool all=true;for(const auto &s:samples)all=all && s.completed && s.retire_exit && (!copies_enabled || s.copy_ok) && (!colors_enabled || s.color_ok);
  return ok && healthy() && all && pins==released;
}
void write(std::ostream &o) {
  Lock lock;o<<"{\"started_tick\":"<<started<<",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()
   <<",\"hooks_disabled\":"<<hooks_disabled<<",\"admission_closed\":"<<(!accepting.load(std::memory_order_acquire))<<",\"stopped_tick\":"<<stopped_tick<<",\"pins\":"<<pins<<",\"released\":"<<released<<",\"retained\":"<<retained<<",\"copies_enabled\":"<<copies_enabled<<",\"colors_enabled\":"<<colors_enabled<<",\"depths_enabled\":"<<depths_enabled<<",\"samples\":[";
  for(unsigned i=0;i<capacity;++i){if(i)o<<',';const auto &s=samples[i];
    o<<"{\"receipt\":"<<i<<",\"frame\":"<<s.frame<<",\"packet\":"<<s.packet<<",\"command\":"<<s.command<<",\"allocator\":"<<s.allocator
     <<",\"reset\":"<<s.reset<<",\"dispatch\":"<<s.dispatch<<",\"dispatch_exit\":"<<s.dispatch_exit<<",\"close\":"<<s.close
     <<",\"execute\":"<<s.execute<<",\"execute_exit\":"<<s.execute_exit<<",\"queue\":"<<s.queue<<",\"fence\":"<<s.fence_object
     <<",\"signal\":"<<s.signal<<",\"signal_exit\":"<<s.signal_exit<<",\"signal_attempts\":"<<s.signal_attempts<<",\"signal_result\":"<<s.signal_result
     <<",\"signaled\":"<<s.signaled<<",\"completed\":"<<s.completed<<",\"completed_value\":"<<s.completed_value<<",\"completed_order\":"<<s.completed_order
     <<",\"completed_tick\":"<<s.completed_tick
     <<",\"dispatch_forward\":"<<s.forward<<",\"copy_begin\":"<<s.copy.begin<<",\"copy_end\":"<<s.copy.end<<",\"copy_resource\":"<<s.copy.resource<<",\"copy_identity\":"<<s.copy.identity<<",\"copy_ok\":"<<s.copy_ok
     <<",\"color_begin\":"<<s.color.begin<<",\"color_end\":"<<s.color.end<<",\"color_resource\":"<<s.color.resource<<",\"color_identity\":"<<s.color.identity<<",\"color_ok\":"<<s.color_ok
     <<",\"depth_begin\":"<<s.depth.begin<<",\"depth_end\":"<<s.depth.end<<",\"depth_marker\":"<<s.depth.marker<<",\"depth_resource\":"<<s.depth.resource<<",\"depth_identity\":"<<s.depth.identity<<",\"depth_ok\":"<<s.depth_ok
     <<",\"retire\":"<<s.retire<<",\"retire_exit\":"<<s.retire_exit
     <<",\"inline_begin\":"<<s.inline_begin<<",\"inline_end\":"<<s.inline_end<<",\"inline_ok\":"<<s.inline_ok<<'}';}
  o<<"],\"lists\":[";for(unsigned i=0;i<list_count;++i){if(i)o<<',';o<<"{\"object\":"<<lists[i].object<<",\"identity\":"<<lists[i].identity<<'}';}
  o<<"],\"queues\":[";for(unsigned i=0;i<queue_count;++i){if(i)o<<',';o<<"{\"object\":"<<queues[i].object<<",\"identity\":"<<queues[i].identity<<'}';}
  o<<"],\"allocators\":[";for(unsigned i=0;i<allocator_count;++i){if(i)o<<',';o<<"{\"object\":"<<allocators[i].object<<",\"identity\":"<<allocators[i].identity<<'}';}
  o<<"],\"events\":[";for(unsigned i=0;i<event_count;++i){if(i)o<<',';const auto &e=events[i];
    o<<"{\"kind\":"<<e.kind<<",\"thread\":"<<e.thread<<",\"object\":"<<e.object<<",\"allocator\":"<<e.allocator<<",\"enter\":"<<e.enter
     <<",\"exit\":"<<e.exit<<",\"enter_tick\":"<<e.enter_tick<<",\"exit_tick\":"<<e.exit_tick
     <<",\"returned\":"<<e.returned<<",\"valid\":"<<e.valid<<",\"result\":"<<e.result<<",\"lists\":[";
    for(unsigned j=0;j<e.count;++j){if(j)o<<',';o<<submitted[e.first+j];}o<<"]}";}
  o<<"]}";
}
#ifdef MHW_TEMPORAL_FIXTURE
bool fixture_start(const Config &c){if(!prepare(c))return false;for(unsigned i=0;i<3;++i)originals[i]=reinterpret_cast<void *>(targets[i]);begin(GetTickCount64());return true;}
HRESULT fixture_reset(ID3D12GraphicsCommandList *p,ID3D12CommandAllocator *a){return reset_call(p,a,nullptr);}
HRESULT fixture_close(ID3D12GraphicsCommandList *p){return close_call(p);}
void fixture_execute(ID3D12CommandQueue *q,unsigned n,ID3D12CommandList *const *p){execute_call(q,n,p);}
void fixture_signal_failure() noexcept {signal_failure=true;}
void fixture_before_close_return(void (*callback)()) noexcept {before_close_return=callback;}
#endif
}
