#include "gpu_trace.hpp"
#include "gpu_state.hpp"
#ifdef MHW_AA_SDK
#include "sdk_runtime.hpp"
#endif
#ifdef MHW_AA_BINDINGS
#include "../temporal_present/observer.hpp"
#endif
#ifdef MHW_AA_PROJECTION
#include "../temporal_bindings/jitter.hpp"
#endif
#ifdef MHW_AA_INPUT_READBACK
#include "input_readback.hpp"
#endif
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <ostream>
namespace mhw::aa_control::gpu_trace {
namespace {
namespace pb=projection_binding;namespace sub=temporal_bindings::submission;
constexpr unsigned request_limit=8,event_limit=2048,barrier_limit=8192,batch_limit=1024;
struct Watch {ID3D12Resource *held{};std::uintptr_t resource{};unsigned format{};};
struct List {std::uintptr_t command{};std::uint64_t generation{};std::array<State,3> states;};
struct Event {sub::RecordingStamp stamp;unsigned token{},first{},count{};bool returned{},unchanged{};};
struct Sample {sub::RecordingStamp stamp;std::uint64_t frame{};std::uintptr_t packet{};std::array<State,3> states;bool captured{};
#ifdef MHW_AA_BINDINGS
    temporal_present::Bindings bindings;bool binding_seen{},binding_ready{},binding_unchanged{};
#endif
#ifdef MHW_AA_PROJECTION
    temporal_bindings::jitter::HistorySample projection;bool projection_matched{};
#endif
#ifdef MHW_AA_INPUT_READBACK
    std::uint64_t read_point{};unsigned read_token{UINT_MAX};std::array<State,3> read_states;bool read_ready{},copy_ok{};
#endif
};
std::array<Watch,3> watched;
std::array<List,128> lists;
std::array<Event,event_limit> events;
std::array<D3D12_RESOURCE_BARRIER,barrier_limit> barriers;
std::array<Sample,request_limit> samples;
std::uintptr_t target{};void *original{};unsigned list_count{},event_count{},barrier_count{},sample_count{},unqualified_calls{};
std::atomic<unsigned> pending{},fault{};std::atomic<bool> accepting{};
bool enabled{},drained{},completed{},journal_released{},watch_released{};
bool collection_closed{},completed_before_close{};
std::uint64_t collection_close_requested{},collection_close_tick{},first_disable_tick{};
unsigned journal_events_at_close{};
#ifdef MHW_AA_SDK
thread_local ID3D12GraphicsCommandList *private_list{};
bool sdk_record(unsigned i,ID3D12GraphicsCommandList *list,Sample &sample) noexcept {
    if(private_list)return false;bool ok{};private_list=list;
    __try {ok=sdk_runtime::record(i,list,reinterpret_cast<input_readback::Barrier>(original),sample.bindings,sample.projection);}
    __finally {private_list=nullptr;}return ok;
}
#endif
#ifdef MHW_AA_BINDINGS
namespace observer=temporal_present::observer;
#endif
SRWLOCK mutex=SRWLOCK_INIT;
struct Lock {Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
void fail(unsigned why) noexcept {unsigned zero{};fault.compare_exchange_strong(zero,why);}
List *recording(const sub::RecordingStamp &s) noexcept {
    for(unsigned i=0;i<list_count;++i)if(lists[i].command==s.command){auto &v=lists[i];
        if(v.generation>s.reset){fail(1);return nullptr;}
        if(v.generation!=s.reset){v.generation=s.reset;v.states={};}return &v;
    }
    if(list_count==lists.size()){fail(2);return nullptr;}
    auto &v=lists[list_count++];v.command=s.command;v.generation=s.reset;return &v;
}
bool selected(std::uintptr_t p) noexcept {for(const auto &v:watched)if(v.resource==p)return true;return false;}
bool relevant(const D3D12_RESOURCE_BARRIER &b) noexcept {
    if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)return selected(reinterpret_cast<std::uintptr_t>(b.Transition.pResource));
    if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING)return !b.Aliasing.pResourceBefore||!b.Aliasing.pResourceAfter||
        selected(reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore))||selected(reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter));
    if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV)return !b.UAV.pResource||selected(reinterpret_cast<std::uintptr_t>(b.UAV.pResource));
    return true; // Unknown kind invalidates coverage instead of being silently skipped.
}
bool extract(unsigned n,const D3D12_RESOURCE_BARRIER *data,std::array<D3D12_RESOURCE_BARRIER,batch_limit> &out,unsigned &count) noexcept {
    if(!n||n>batch_limit)return false;
    std::array<D3D12_RESOURCE_BARRIER,batch_limit> all;
    if(!pb::copy_memory(reinterpret_cast<std::uintptr_t>(data),all.data(),n*sizeof(*data)))return false;
    count=0;for(unsigned i=0;i<n;++i)if(relevant(all[i]))out[count++]=all[i];return true;
}
void apply(List &v,const D3D12_RESOURCE_BARRIER &b,std::uint64_t order) noexcept {
    for(unsigned i=0;i<3;++i){auto &s=v.states[i];const auto address=watched[i].resource;
        if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION&&reinterpret_cast<std::uintptr_t>(b.Transition.pResource)==address)
            s.transition(b.Transition.StateBefore,b.Transition.StateAfter,b.Flags,b.Transition.Subresource,order);
        else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING&&(!b.Aliasing.pResourceBefore||!b.Aliasing.pResourceAfter||
                reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore)==address||reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter)==address))
            s.invalidate(Issue::alias,order);
        else if(b.Type>D3D12_RESOURCE_BARRIER_TYPE_UAV||(b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV&&b.Flags))s.invalidate(Issue::unsupported,order);
    }
}
struct Observation {
    std::array<D3D12_RESOURCE_BARRIER,batch_limit> saved;
    unsigned count{},at{UINT_MAX},token{UINT_MAX};sub::RecordingStamp stamp;
};
#ifdef MHW_AA_INPUT_READBACK
// Select only the FIRST relevant native barrier following this immutable
// marker in this recording. The explicit paired transition resolves motion's
// formerly unknown state without borrowing state from another recording.
unsigned read_point(const Observation &o) noexcept {
    Lock lock;auto *v=recording(o.stamp);if(!v)return UINT_MAX;
    unsigned found=UINT_MAX;
    for(unsigned i=0;i<request_limit;++i){auto &s=samples[i];
        if(!s.captured||s.stamp.command!=o.stamp.command||s.stamp.reset!=o.stamp.reset||s.read_point)continue;
        if(found!=UINT_MAX||s.stamp.enter>=o.stamp.enter){fail(7);return UINT_MAX;}found=i;
    }
    if(found==UINT_MAX)return found;auto &s=samples[found];s.read_point=o.stamp.enter;s.read_token=o.token;s.read_states=v->states;
    bool color{},motion{};
    if(o.count==2)for(unsigned i=0;i<o.count;++i){const auto &b=o.saved[i];
        if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION||b.Flags||b.Transition.Subresource!=UINT_MAX||
           b.Transition.StateBefore!=D3D12_RESOURCE_STATE_RENDER_TARGET||b.Transition.StateAfter!=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)continue;
        color|=b.Transition.pResource==watched[0].held;motion|=b.Transition.pResource==watched[2].held;
    }
    bool ok=color&&motion;
    for(const auto &state:v->states)ok=ok&&state.known&&state.issue==Issue::none&&state.value==128&&state.transitions;
    s.read_ready=ok;if(!ok){fail(7);return UINT_MAX;}
    return found;
}
#endif
void observation_before(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *data,Observation &o) noexcept {
    auto &saved=o.saved;auto &count=o.count;auto &at=o.at;auto &token=o.token;auto &stamp=o.stamp;sub::MarkerFailure why{};
    const bool observing=accepting.load(std::memory_order_acquire)&&healthy();
    if(observing){
        if(!extract(n,data,saved,count))fail(3);
        else if(count){
            token=sub::barrier_begin(list,why,stamp);
            if(token==UINT_MAX){Lock lock;++unqualified_calls;
                // Hooks may first see an already-open recording during warmup.
                // Its barriers cannot seed a later, observed Reset generation.
                if(why!=sub::MarkerFailure::unknown_list&&why!=sub::MarkerFailure::missing_reset)fail(4);
            }else{Lock lock;
                if(event_count==event_limit||count>barrier_limit-barrier_count)fail(2);
                else{at=event_count++;events[at]={stamp,token,barrier_count,count};
                    for(unsigned i=0;i<count;++i)barriers[barrier_count++]=saved[i];}
            }
        }
    }
 }
void observation_after(unsigned n,const D3D12_RESOURCE_BARRIER *data,Observation &o,bool returned) noexcept {
    auto &saved=o.saved;auto &count=o.count;auto &at=o.at;auto &token=o.token;auto &stamp=o.stamp;
        if(token!=UINT_MAX){
            std::array<D3D12_RESOURCE_BARRIER,batch_limit> after;unsigned after_count{};
            bool same=returned&&extract(n,data,after,after_count)&&after_count==count;
            if(same)for(unsigned i=0;i<count;++i){const auto &a=saved[i],&b=after[i];
                if(a.Type!=b.Type||a.Flags!=b.Flags){same=false;break;}
                if(a.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)same=a.Transition.pResource==b.Transition.pResource&&
                    a.Transition.Subresource==b.Transition.Subresource&&a.Transition.StateBefore==b.Transition.StateBefore&&a.Transition.StateAfter==b.Transition.StateAfter;
                else if(a.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING)same=a.Aliasing.pResourceBefore==b.Aliasing.pResourceBefore&&a.Aliasing.pResourceAfter==b.Aliasing.pResourceAfter;
                else if(a.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV)same=a.UAV.pResource==b.UAV.pResource;
                else same=false;
                if(!same)break;
            }
            {Lock lock;if(at!=UINT_MAX){auto &e=events[at];e.returned=returned;e.unchanged=same;
                auto *v=recording(stamp);if(v&&same)for(unsigned i=0;i<count;++i)apply(*v,saved[i],stamp.enter*1025+i+1);
                if(!same)fail(5);}}
#ifdef MHW_AA_INPUT_READBACK
            if(at!=UINT_MAX&&same&&healthy()){
                const auto index=read_point(o);
                if(index!=UINT_MAX){
                    // Keep the submission journal's exclusive recording lease
                    // until private copies and exact native state restoration
                    // finish. The original barrier trampoline avoids recursion.
                    auto *list=reinterpret_cast<ID3D12GraphicsCommandList *>(stamp.command);
                    bool admitted=true;
#ifdef MHW_AA_BINDINGS
                    auto &sample=samples[index];
                    sample.binding_seen=observer::inspect(list,sample.bindings);
                    sample.binding_ready=sample.binding_seen&&sample.bindings.ready()&&sample.bindings.generation==stamp.reset&&
                        observer::capture(index,sample.frame,list,sample.bindings.compute.signature,sample.bindings.pipeline);
                    admitted=sample.binding_ready;
#endif
#ifdef MHW_AA_PROJECTION
                    sample.projection_matched=temporal_bindings::jitter::current_history_sample(index,sample.projection)&&
                        sample.projection.frame==sample.frame;
                    admitted=admitted&&sample.projection_matched;
#endif
                    bool ok=admitted&&(!input_readback::selected(index)||input_readback::record(index,
                        list,reinterpret_cast<input_readback::Barrier>(original)));
#ifdef MHW_AA_SDK
                    if(ok)ok=sdk_record(index,list,sample);
#endif
#ifdef MHW_AA_BINDINGS
                    sample.binding_unchanged=sample.binding_seen&&observer::matches_current(list,sample.bindings);
                    if(!sample.binding_ready||!sample.binding_unchanged)fail(9);
#endif
                    {Lock lock;samples[index].copy_ok=ok;}
                    if(!ok)fail(8);
                }
            }
#endif
            sub::barrier_end(token,returned&&same);
        }
}
void observed(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *data){
    using Fn=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,const D3D12_RESOURCE_BARRIER *);
    Observation o;observation_before(list,n,data,o);bool returned{};
    __try {reinterpret_cast<Fn>(original)(list,n,data);returned=true;}
    __finally {observation_after(n,data,o,returned);}
}
void WINAPI hook(ID3D12GraphicsCommandList *list,UINT n,const D3D12_RESOURCE_BARRIER *data){
#ifdef MHW_AA_SDK
    // Our explicit copy states are restored by SdkPass; SDK-private barriers
    // must not recursively claim the native journal lease already held here.
    if(private_list==list){reinterpret_cast<input_readback::Barrier>(original)(list,n,data);return;}
#endif
    ++pending;__try{observed(list,n,data);}__finally{--pending;}
}
bool pin(std::uintptr_t address,std::uintptr_t device,Watch &out) noexcept {
    std::uintptr_t method{};ID3D12Device *owner{};bool ok{};
    for(const auto slot:{0U,2U,7U,10U})if(!pb::system_method(address,slot,method))return false;
    __try{
        if(SUCCEEDED(reinterpret_cast<IUnknown *>(address)->QueryInterface(IID_PPV_ARGS(&out.held)))&&out.held&&
           reinterpret_cast<std::uintptr_t>(out.held)==address&&SUCCEEDED(out.held->GetDevice(IID_PPV_ARGS(&owner)))&&owner){
            const auto d=out.held->GetDesc();D3D12_FEATURE_DATA_FORMAT_INFO format{d.Format,0};
            ok=reinterpret_cast<std::uintptr_t>(owner)==device&&d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&
                d.Width==2560&&d.Height==1440&&d.MipLevels==1&&d.DepthOrArraySize==1&&d.SampleDesc.Count==1&&
                SUCCEEDED(owner->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO,&format,sizeof(format)))&&format.PlaneCount==1;
            out.resource=address;out.format=d.Format;
        }
    }__finally{if(owner)owner->Release();}return ok;
}
bool discover(std::uintptr_t device) noexcept {
    ID3D12CommandAllocator *allocator{};ID3D12GraphicsCommandList *list{};bool ok{};
    __try{
        auto *d=reinterpret_cast<ID3D12Device *>(device);
        ok=SUCCEEDED(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)))&&
            SUCCEEDED(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator,nullptr,IID_PPV_ARGS(&list)))&&
            SUCCEEDED(list->Close())&&pb::system_method(reinterpret_cast<std::uintptr_t>(list),26,target);
    }__finally{if(list)list->Release();if(allocator)allocator->Release();}return ok;
}
}
bool install(const temporal_bindings::Config &config,const std::array<std::uintptr_t,3> &resources,std::string &reason){
    constexpr std::array<unsigned,3> formats={26,41,42};
    for(unsigned i=0;i<3;++i){
        if(!pin(resources[i],config.device,watched[i])||watched[i].format!=formats[i]){reason="AA-input-pin-format";return false;}
        for(unsigned j=0;j<i;++j)if(resources[i]==resources[j]){reason="AA-input-alias";return false;}
    }
#ifdef MHW_AA_INPUT_READBACK
    if(!input_readback::prepare(reinterpret_cast<ID3D12Device *>(config.device),{watched[0].held,watched[1].held,watched[2].held})){
        reason="AA-readback-prepare";return false;}
#endif
#ifdef MHW_AA_SDK
    if(!sdk_runtime::prepare(reinterpret_cast<ID3D12Device *>(config.device),{watched[0].held,watched[1].held,watched[2].held},reason))return false;
#endif
    if(!discover(config.device)){reason="AA-barrier-target";return false;}
    if(MH_CreateHook(reinterpret_cast<void *>(target),reinterpret_cast<void *>(hook),&original)!=MH_OK){reason="AA-barrier-create";return false;}
#ifdef MHW_AA_BINDINGS
    if(!observer::install(reinterpret_cast<ID3D12Device *>(config.device),pb::system_method,reason))return false;
#endif
    if(!sub::install(config,reason))return false;
    enabled=MH_EnableHook(reinterpret_cast<void *>(target))==MH_OK;if(!enabled)reason="AA-barrier-enable";return enabled;
}
void begin() noexcept {
#ifdef MHW_AA_BINDINGS
    observer::begin();
#endif
    sub::begin(GetTickCount64());accepting=true;
}
bool matches(const std::array<std::uintptr_t,3> &resources) noexcept {
    for(unsigned i=0;i<3;++i)if(resources[i]!=watched[i].resource)return false;return true;
}
bool mark(unsigned i,std::uint64_t frame,std::uintptr_t packet,ID3D12GraphicsCommandList *list) noexcept {
    if(i>=request_limit||!healthy())return false;
    sub::RecordingStamp stamp;if(!sub::mark(i,frame,packet,list,&stamp))return false;
    bool ok{};
    {Lock lock;auto *v=recording(stamp);auto &s=samples[i];
        ok=v&&!s.captured;if(ok){s.stamp=stamp;s.frame=frame;s.packet=packet;s.states=v->states;s.captured=true;++sample_count;}else fail(6);
    }
    sub::mark_end(i,ok);return ok;
}
void poll() noexcept {sub::poll();}
bool healthy() noexcept {return !fault&&sub::healthy()
#ifdef MHW_AA_BINDINGS
    &&observer::healthy()
#endif
    ;}
bool complete() noexcept {return sub::complete();}
bool close_admission() noexcept {
    if(collection_closed)return true;
    if(!collection_close_requested){collection_close_requested=GetTickCount64();completed_before_close=sub::complete();}
    accepting=false;
    // A barrier already admitted can still own a journal lease. Let it finish
    // before closing the journal, so it cannot encounter an artificial cutoff.
    const auto until=GetTickCount64()+2000;
    while(pending&&GetTickCount64()<until)Sleep(1);
    if(pending)return false;
#ifdef MHW_AA_BINDINGS
    observer::stop();
#endif
    sub::stop();
    while(!sub::settled()&&GetTickCount64()<until)Sleep(1);
#ifdef MHW_AA_BINDINGS
    while(!observer::settled()&&GetTickCount64()<until)Sleep(1);
    if(!observer::settled())return false;
#endif
    if(!sub::settled())return false;
    journal_events_at_close=sub::recorded_events();collection_close_tick=GetTickCount64();collection_closed=true;
    return true;
}
bool disabled() noexcept {return !enabled&&sub::disabled()
#ifdef MHW_AA_BINDINGS
    &&observer::hooks_disabled()&&observer::settled()
#endif
    ;}
bool safe_release() noexcept {return drained&&disabled()&&(completed||sample_count==0);}
bool qualified() noexcept {
    if(!healthy()||!completed||!journal_released||!watch_released||sample_count!=request_limit)return false;
#ifdef MHW_AA_INPUT_READBACK
    for(const auto &s:samples)if(!s.read_ready||!s.copy_ok||s.read_point<=s.stamp.enter)return false;
    if(!input_readback::recorded())return false;
#ifdef MHW_AA_BINDINGS
    for(const auto &s:samples)if(!s.binding_ready||!s.binding_unchanged)return false;
#endif
#ifdef MHW_AA_PROJECTION
    for(const auto &s:samples)if(!s.projection_matched)return false;
#endif
#else
    for(const auto &s:samples)for(const auto &v:s.states)if(!v.known||v.issue!=Issue::none||!v.transitions)return false;
#endif
    return true;
}
bool finish() noexcept {
    if(!close_admission())return false;
    if(!first_disable_tick)first_disable_tick=GetTickCount64();
    if(enabled){if(MH_DisableHook(reinterpret_cast<void *>(target))!=MH_OK)return false;enabled=false;}
    const auto until=GetTickCount64()+2000;while(pending&&GetTickCount64()<until)Sleep(1);
    if(pending)return false;
#ifdef MHW_AA_BINDINGS
    observer::finish();
    if(!observer::settled()||!observer::hooks_disabled())return false;
#endif
    completed=sub::complete();
#ifdef MHW_AA_SDK
    if(!sdk_runtime::finish(completed))fail(10);
#endif
    journal_released=sub::finish();drained=sub::settled();
#ifdef MHW_AA_INPUT_READBACK
    input_readback::retire(safe_release()&&completed);
#endif
    if(safe_release()){for(auto &v:watched)if(v.held){v.held->Release();v.held=nullptr;}watch_released=true;}
    return drained&&disabled();
}
void write(std::ostream &out){
    Lock lock;out<<"{\"event\":\"gpu_trace\",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"hooks_disabled\":"<<disabled()
        <<",\"drained\":"<<drained<<",\"qualified\":"<<qualified()<<",\"completed\":"<<completed<<",\"journal_released\":"<<journal_released
        <<",\"watch_released\":"<<watch_released<<",\"unqualified_calls\":"<<unqualified_calls
        <<",\"collection_closed\":"<<collection_closed<<",\"completed_before_close\":"<<completed_before_close
        <<",\"collection_close_requested\":"<<collection_close_requested<<",\"collection_close_tick\":"<<collection_close_tick
        <<",\"journal_events_at_close\":"<<journal_events_at_close<<",\"first_disable_tick\":"<<first_disable_tick<<",\"watched\":[";
    for(unsigned i=0;i<3;++i)out<<(i?",":"")<<watched[i].resource;
    out<<"],\"records\":[";
    for(unsigned i=0;i<request_limit;++i){const auto &s=samples[i];out<<(i?",":"")<<"{\"receipt\":"<<i<<",\"frame\":"<<s.frame<<",\"packet\":"<<s.packet
        <<",\"command\":"<<s.stamp.command<<",\"allocator\":"<<s.stamp.allocator<<",\"reset\":"<<s.stamp.reset<<",\"marker\":"<<s.stamp.enter<<",\"captured\":"<<s.captured<<",\"states\":[";
        for(unsigned j=0;j<3;++j){const auto &v=s.states[j];out<<(j?",":"")<<"{\"known\":"<<v.known<<",\"state\":"<<v.value<<",\"issue\":"<<unsigned(v.issue)
            <<",\"transitions\":"<<v.transitions<<",\"last\":"<<v.last<<'}';}out<<"]";
#ifdef MHW_AA_INPUT_READBACK
        out<<",\"read_point\":"<<s.read_point<<",\"read_token\":"<<s.read_token<<",\"read_ready\":"<<s.read_ready<<",\"copy_ok\":"<<s.copy_ok<<",\"read_states\":[";
        for(unsigned j=0;j<3;++j){const auto &v=s.read_states[j];out<<(j?",":"")<<"{\"known\":"<<v.known<<",\"state\":"<<v.value<<",\"issue\":"<<unsigned(v.issue)
            <<",\"transitions\":"<<v.transitions<<",\"last\":"<<v.last<<'}';}out<<"]";
#endif
#ifdef MHW_AA_BINDINGS
        out<<",\"binding_seen\":"<<s.binding_seen<<",\"binding_ready\":"<<s.binding_ready<<",\"binding_unchanged\":"<<s.binding_unchanged
           <<",\"binding_known\":"<<s.bindings.known<<",\"binding_valid\":"<<s.bindings.valid<<",\"binding_generation\":"<<s.bindings.generation
           <<",\"binding_pipeline\":"<<s.bindings.pipeline<<",\"binding_compute_root\":"<<s.bindings.compute.signature;
#endif
#ifdef MHW_AA_PROJECTION
        out<<",\"projection_matched\":"<<s.projection_matched<<",\"projection_frame\":"<<s.projection.frame<<",\"projection_epoch\":"<<s.projection.epoch
           <<",\"jitter_bits\":["<<s.projection.jitter_bits[0]<<','<<s.projection.jitter_bits[1]<<']';
#endif
        out<<"}";
    }
    out<<"],\"barrier_calls\":[";
    for(unsigned i=0;i<event_count;++i){const auto &e=events[i];out<<(i?",":"")<<"{\"token\":"<<e.token<<",\"command\":"<<e.stamp.command
        <<",\"reset\":"<<e.stamp.reset<<",\"enter\":"<<e.stamp.enter<<",\"returned\":"<<e.returned<<",\"unchanged\":"<<e.unchanged<<",\"barriers\":[";
        for(unsigned j=0;j<e.count;++j){const auto &b=barriers[e.first+j];out<<(j?",":"")<<"{\"type\":"<<b.Type<<",\"flags\":"<<b.Flags;
            if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)out<<",\"resource\":"<<reinterpret_cast<std::uintptr_t>(b.Transition.pResource)<<",\"subresource\":"<<b.Transition.Subresource<<",\"before\":"<<b.Transition.StateBefore<<",\"after\":"<<b.Transition.StateAfter;
            else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING)out<<",\"before\":"<<reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore)<<",\"after\":"<<reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter);
            else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV)out<<",\"resource\":"<<reinterpret_cast<std::uintptr_t>(b.UAV.pResource);
            out<<'}';}out<<"]}";
    }
    out<<"],\"submission\":";sub::write(out);
#ifdef MHW_AA_BINDINGS
    out<<",\"bindings\":";observer::write(out);
#endif
    out<<"}\n";
}
}
