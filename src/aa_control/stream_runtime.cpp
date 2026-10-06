#include "stream_runtime.hpp"
#include "stream_pass.hpp"
#include "stream_lifetime.hpp"
#include "owned_runtime.hpp"
#include "deferred_contract.hpp"
#include "projection_link.hpp"
#include "gpu_state.hpp"
#include "../continuous_dlaa/history.hpp"
#include "../projection_binding/memory.hpp"
#include "../ngx_startup/startup.hpp"
#include "../temporal_present/observer.hpp"
#include <MinHook.h>
#include <wrl/client.h>
#include <atomic>
#include <algorithm>
#include <bit>
#include <cstring>
#include <iomanip>
#include <stdexcept>
extern "C" void aa_schedule_entry();
extern "C" void aa_marker_entry();
extern "C" void temporal_jitter_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::projection_binding {bool discover(std::uintptr_t,Runtime &,std::string &);}
namespace mhw::aa_control::stream {
namespace {
namespace pb=projection_binding;namespace tb=temporal_bindings;namespace obs=temporal_present::observer;namespace cd=continuous_dlaa;
using Microsoft::WRL::ComPtr;using tb::NativeFrame;
constexpr unsigned capacity=32,none=UINT_MAX;
tb::Config cfg;pb::Runtime runtime;
SRWLOCK mutex=SRWLOCK_INIT;
struct Lock {Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
std::atomic<bool> accepting{};std::atomic<unsigned> pending{},fault{};
std::atomic<std::uint64_t> frames{},skips{},injections{},restorations{},published{},consumed{},released{};
std::atomic_flag projection_busy=ATOMIC_FLAG_INIT,scheduler_busy=ATOMIC_FLAG_INIT;
std::array<std::uintptr_t,7> targets{};std::array<void *,7> originals{};std::array<std::atomic<bool>,7> enabled{};
// Scheduler, empty-packet consumer, projection; Reset, Close, Execute, Barrier.
using Barrier=void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,const D3D12_RESOURCE_BARRIER *);
struct ResourceState {
    ID3D12Resource *resource{};gpu_trace::State state,previous,invalid_before;
    D3D12_RESOURCE_BARRIER latest{},first_invalid{};std::uint64_t invalid_order{};
};
struct InputState {bool tracked{};ResourceState value;};
struct Projection {pb::Source source;cd::HistoryStamp history;std::uint64_t candidate{};bool valid{};};
struct Slot {
    deferred::Marker marker;StreamLifetime life;Projection projection;PostQueue source;
    std::array<tb::Owner,3> owners;std::array<ComPtr<ID3D12Resource>,3> resources;
    ID3D12GraphicsCommandList *list{};std::uintptr_t recording{},entry{};
    std::uint64_t identity{};unsigned native_nodes{};bool consuming{};std::array<std::byte,3> padding{};
    std::array<InputState,3> marker_states;std::uint64_t marker_order{};
};
std::array<Slot,capacity> slots;std::uint64_t identities{};
struct List {
    ComPtr<ID3D12GraphicsCommandList> value;std::uint64_t generation{},order{};
    unsigned count{},selected{none},injected{none},busy{};bool closed{};
    std::array<ResourceState,512> states;
};
std::array<List,128> lists;unsigned list_count{},peak_slots{};std::uint64_t generations{},signal_value{};
ComPtr<ID3D12Fence> fence;ComPtr<ID3D12CommandQueue> last_queue;
std::unique_ptr<StreamPass> pass;unsigned pending_recording=none;
cd::HistoryStamp previous;cd::HistoryTelemetry history;
ComPtr<ID3D12Resource> history_depth;
thread_local unsigned projection_slot=none,schedule_slot=none,consumer_slot=none;
thread_local std::array<std::byte,0x2400> parameters;
thread_local std::uintptr_t backend_context{};
thread_local ID3D12GraphicsCommandList *private_list{};
struct QueueRejection {
    bool present{},recycled{};unsigned slot{};std::uint64_t tick{};
    PostQueue previous,rejected;owned::Admission admission;
};
PostQueue last_admitted_queue;
QueueRejection first_queue_rejection;
struct Layouts {
    std::array<std::uint64_t,2> admissions{},returns{},evaluations{};
    std::uint64_t haze_entries{},haze_exits{};unsigned previous_return_count{};
    owned::ReturnSnapshot first_haze,first_base_after_haze,first_failed_return;
    bool failed_return{};
} layouts;
struct BoundaryDecision {unsigned relevant{},invalid_states{};bool color{},motion{};
    bool accepted() const noexcept{return relevant==2&&color&&motion&&!invalid_states;}};
struct BoundarySnapshot {
    bool present{};unsigned slot{},native_nodes{},count{};std::uint64_t tick{},frame{},epoch{},generation{},marker_order{},begin_order{},end_order{};
    std::uintptr_t list{};std::array<std::uintptr_t,3> resources{};
    BoundaryDecision decision;std::array<InputState,3> marker,before,after;
    std::array<D3D12_RESOURCE_BARRIER,1024> batch{};
};
BoundarySnapshot first_base_boundary,first_haze_boundary,first_boundary_rejection;
void fail(unsigned n) noexcept {unsigned zero{};fault.compare_exchange_strong(zero,n);accepting=false;}
void reject_queue(unsigned slot,const PostQueue &q,bool recycled,const owned::Admission &admission){
    Lock lock;
    if(!first_queue_rejection.present){first_queue_rejection={true,recycled,slot,GetTickCount64(),last_admitted_queue,q,admission};}
    fail(8);
}
bool read(void *,std::uintptr_t at,std::span<std::byte> out){return pb::copy_memory(at,out.data(),static_cast<unsigned>(out.size()));}
const tb::Reader reader{nullptr,read};
List *find(ID3D12GraphicsCommandList *p){for(unsigned i=0;i<list_count;++i)if(lists[i].value.Get()==p)return &lists[i];return nullptr;}
std::array<InputState,3> input_states(const List &l,const Slot &s){
    std::array<InputState,3> result{};
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<l.count;++j)if(l.states[j].resource==s.resources[i].Get()){
        result[i]={true,l.states[j]};break;}
    return result;
}
bool child(ID3D12DeviceChild *p){std::uintptr_t method{};if(!p||!pb::system_method(reinterpret_cast<std::uintptr_t>(p),7,method))return false;
    ComPtr<ID3D12Device> device;return SUCCEEDED(p->GetDevice(IID_PPV_ARGS(&device)))&&reinterpret_cast<std::uintptr_t>(device.Get())==cfg.device;}
void collect(){
    if(!fence||fault.load())return;const auto completed=fence->GetCompletedValue();if(completed==UINT64_MAX){fail(30);return;}
    unsigned live{};for(auto &s:slots){if(s.life.recyclable(completed)){s={};++released;}live+=s.life.stage!=StreamLifetime::free;}
    if(live>peak_slots)peak_slots=live;
}
bool owners_match(const Slot &s){for(unsigned i=0;i<3;++i){tb::Owner a,b;if(!tb::read_owner(reader,s.owners[i].object,a)||!tb::read_owner(reader,s.owners[i].object,b)||a!=b||a!=s.owners[i])return false;}return true;}
bool inputs(Slot &s){
    const auto &a=s.projection.source.anchor;std::uintptr_t manager{},color{},gbuffer{},motion{};tb::Snapshot anchor;tb::SceneDepth depth;
    anchor.context=a.context;anchor.view=a.view;
    if(!pb::read(cfg.image+0x51cc238,manager)||!manager||!pb::read(manager+0xc0,color)||!tb::read_owner(reader,color,s.owners[0])||
       !tb::scene_depth(reader,anchor,depth)||!pb::read(a.view+0x620,gbuffer)||!gbuffer||!pb::read(gbuffer+0x78,motion)||!tb::read_owner(reader,motion,s.owners[2]))return false;
    s.owners[1]=depth.texture;constexpr unsigned formats[]{26,41,42};
    for(unsigned i=0;i<3;++i){auto *r=reinterpret_cast<ID3D12Resource *>(s.owners[i].resource);if(!child(r))return false;const auto d=r->GetDesc();
        if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.Width!=runtime.width||d.Height!=runtime.height||static_cast<unsigned>(d.Format)!=formats[i]||d.MipLevels!=1||d.DepthOrArraySize!=1||d.SampleDesc.Count!=1||d.SampleDesc.Quality)return false;
        for(unsigned j=0;j<i;++j)if(s.resources[j].Get()==r)return false;s.resources[i]=r;}
    return owners_match(s);
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

bool projection_before(const NativeFrame &f){
    if(!accepting||f.call.caller!=cfg.image+0x228e834)return false;
    if(projection_busy.test_and_set()){fail(1);return false;}
    Projection candidate;pb::Provider provider_state;unsigned char taa{},fxaa{};
    auto &a=candidate.source.anchor;
    if(!anchor(candidate.source,f)||!provider(provider_state)||!pb::read(a.table+0xe9e4,taa)||taa||!pb::read(a.table+0xe9e5,fxaa)||fxaa||
       !pb::read(a.table+0xe6a0+8*(a.frame%20),candidate.candidate)||!pb::jitter_finite(candidate.candidate)){
        ++skips;projection_busy.clear();return false;}
    unsigned selected=none;
    {Lock lock;collect();for(unsigned i=0;i<capacity;++i){auto &slot=slots[i];
        // A projection which never reached this post queue owns no commands.
        // Expire it only after later source frames, without reusing live packets.
        if(slot.life.stage==StreamLifetime::projected&&slot.life.frame+4<a.frame){slot={};++skips;}
        if(slot.life.stage==StreamLifetime::free&&selected==none)selected=i;}
        if(selected!=none){auto &slot=slots[selected];slot.life.reserve(a.frame);slot.identity=++identities;slot.projection=candidate;}}
    if(selected==none){fail(2);projection_busy.clear();return false;}
    auto &slot=slots[selected];
    if(!inputs(slot)){Lock lock;slot={};++skips;projection_busy.clear();return false;}
    projection_slot=selected;
    if(!pb::jitter_exchange(candidate.source.camera+0x38,0,candidate.candidate)){fail(3);projection_slot=none;projection_busy.clear();return false;}
    ++injections;return true;
}
void projection_after(bool admitted,bool returned,NativeFrame &f){
    if(!admitted)return;auto &slot=slots[projection_slot];auto &projection=slot.projection;auto &s=projection.source;
    s.projection=bridge_probe::capture_projection_snapshot(cfg.image,s.anchor,bridge_probe::read_bounded_scalar,nullptr);
    const auto &p=s.projection;
    const bool restored=pb::jitter_exchange(s.camera+0x38,projection.candidate,0);
    if(restored)++restorations;else fail(4);
    const auto bits=std::uint64_t(p.jitter_bits[0])|(std::uint64_t(p.jitter_bits[1])<<32);
    bool valid=returned&&(f.call.gpr[0]&255)&&restored&&p.valid&&p.matrix_relation&&p.manager==s.camera&&p.vtable==s.camera_type&&bits==projection.candidate&&
        p.tagged_constants==p.constants&&p.words&&p.words<=pb::max_constants/4;
    if(valid){s.bytes=p.words*4;std::array<std::byte,pb::max_constants> again{};pb::Provider after;
        valid=pb::copy_memory(p.constants,s.data.data(),s.bytes)&&pb::copy_memory(p.constants,again.data(),s.bytes)&&
            std::memcmp(s.data.data(),again.data(),s.bytes)==0&&pb::capture_matrices(cfg.image,s)&&provider(after);}
    if(valid){LARGE_INTEGER tick{},frequency{};QueryPerformanceCounter(&tick);QueryPerformanceFrequency(&frequency);
        auto &h=projection.history;h.frame=s.anchor.frame;h.epoch=s.anchor.epoch;h.camera=s.camera;h.view=s.anchor.view;
        h.microseconds=static_cast<std::uint64_t>((tick.QuadPart/frequency.QuadPart)*1000000+(tick.QuadPart%frequency.QuadPart)*1000000/frequency.QuadPart);
        h.viewport=p.viewport_bits;h.projection=p.unjittered_bits;for(unsigned i=0;i<16;++i)h.view_matrix[i]=std::bit_cast<float>(s.matrices.bits[1][i]);
        Lock lock;projection.valid=true;
    }else fail(5);
    projection_slot=none;projection_busy.clear();
}

bool writable(std::uintptr_t address,std::size_t bytes) noexcept {
    MEMORY_BASIC_INFORMATION m{};if(!address||address>UINTPTR_MAX-bytes||VirtualQuery(reinterpret_cast<void *>(address),&m,sizeof(m))!=sizeof(m)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto p=m.Protect&255;const auto base=reinterpret_cast<std::uintptr_t>(m.BaseAddress);
    return (p==PAGE_READWRITE||p==PAGE_WRITECOPY)&&address>=base&&bytes<=m.RegionSize&&address-base<=m.RegionSize-bytes;
}
bool queue_marker(Slot &s){Lock lock;return s.life.queue();}
bool enqueue(Slot &s,unsigned i) noexcept {
    const auto context=s.source.context;std::uintptr_t entries{},returned{},found{},cached{};unsigned count{},limit{},capacity_value{},phase{},offset{};
    if(!pb::read(context+0x2b78,entries)||!pb::read(context+0x2b80,count)||count!=s.source.packet_count||!pb::read(context+0x2b84,limit)||!pb::read(context+0x2b88,capacity_value)||
       !pb::read(context+0x154,phase)||!pb::read(context+0x15c,offset)||!pb::read(context+0x1a8,cached)||cached||!deferred::admit(count,limit,capacity_value,79,0)||
       entries>UINTPTR_MAX-16ULL*(count+1)||!writable(entries+16ULL*count,16)||!writable(context+0x154,12)||!writable(context+0x2b80,4))return false;
    s.entry=entries+16ULL*count;s.marker.generation=i+1;
    if(!queue_marker(s))return false;
    __try {*reinterpret_cast<unsigned *>(context+0x154)=79;*reinterpret_cast<unsigned *>(context+0x15c)=offset&31U;
        returned=reinterpret_cast<std::uintptr_t(*)(std::uintptr_t,deferred::Marker *)>(cfg.image+0x23cabf0)(context,&s.marker);
    } __finally {*reinterpret_cast<unsigned *>(context+0x154)=phase;*reinterpret_cast<unsigned *>(context+0x15c)=offset;}
    unsigned actual_phase{},actual_offset{},after{},key{};
    return pb::read(context+0x154,actual_phase)&&phase==actual_phase&&pb::read(context+0x15c,actual_offset)&&offset==actual_offset&&pb::read(context+0x1a8,cached)&&!cached&&
        returned==s.entry&&pb::read(s.entry,key)&&key==((15U<<27)|2U)&&pb::read(s.entry+8,found)&&found==reinterpret_cast<std::uintptr_t>(&s.marker)&&
        pb::read(context+0x2b80,after)&&after==count+1&&deferred::valid(s.marker,i);
}
bool publish(unsigned,const PostQueue &q,const tb::Owner &color) noexcept {
    if(schedule_slot==none)return false;auto &s=slots[schedule_slot];
    if(!same_post_frame(s.source,q)||s.owners[0]!=color||!owners_match(s))return false;s.source=q;
    if(!enqueue(s,schedule_slot))return false;++published;return true;
}
bool schedule_before(const pb::Frame &f){
    if(!accepting||f.caller!=cfg.image+0x228e92d)return false;
    if(scheduler_busy.test_and_set()){fail(6);return false;}PostQueue q,again;unsigned selected=none;
    if(read_post_queue(reader,cfg.image,f.gpr[1],f.gpr[2],f.gpr[3],q)&&read_post_queue(reader,cfg.image,f.gpr[1],f.gpr[2],f.gpr[3],again)&&q==again){
        Lock lock;for(unsigned i=0;i<capacity;++i)if(slots[i].life.stage==StreamLifetime::projected&&slots[i].life.frame==q.frame){if(selected!=none){fail(7);break;}selected=i;}
    }
    if(selected==none){++skips;scheduler_busy.clear();return false;}
    auto &s=slots[selected];const auto &a=s.projection.source.anchor;unsigned char async{};tb::Snapshot linked;
    if(!pb::read(cfg.renderer+0x93,async)||!projection_link(q,async,linked)||linked.frame!=a.frame||linked.epoch!=a.epoch||linked.bank!=a.bank||linked.async!=a.async_mode||linked.view!=a.view||linked.context!=a.context||!s.projection.valid){fail(7);scheduler_busy.clear();return false;}
    s.source=q;s.native_nodes=q.count;schedule_slot=selected;
    owned::Admission admission;const bool recycled=owned::recycle(0,&admission);
    if(!recycled||!owned::insert(0,q,&admission)){reject_queue(selected,q,recycled,admission);schedule_slot=none;scheduler_busy.clear();return false;}
    {Lock lock;last_admitted_queue=q;++layouts.admissions[q.count==5];}
    return true;
}
void schedule_after(bool admitted,bool returned){if(!admitted)return;
    const auto i=schedule_slot;const bool ok=owned::remove(0,returned);
    {Lock lock;slots[i].life.producer_returned=ok;const auto n=slots[i].native_nodes;
        if(!ok){if(!layouts.failed_return){layouts.first_failed_return=owned::returned_snapshot(0);layouts.failed_return=true;}fail(9);}
        else {
            ++layouts.returns[n==5];
            if(n==5&&!layouts.first_haze.complete)layouts.first_haze=owned::returned_snapshot(0);
            if(n==5&&layouts.previous_return_count==4)++layouts.haze_entries;
            if(n==4&&layouts.previous_return_count==5){++layouts.haze_exits;
                if(!layouts.first_base_after_haze.complete)layouts.first_base_after_haze=owned::returned_snapshot(0);}
            layouts.previous_return_count=n;
        }}
    schedule_slot=none;scheduler_busy.clear();
}
bool consumer_before(const pb::Frame &f){
    unsigned selected=none;{Lock lock;for(unsigned i=0;i<capacity;++i)if(f.gpr[3]==reinterpret_cast<std::uintptr_t>(&slots[i].marker)){selected=i;break;}
        if(selected==none)return false;auto &s=slots[selected];if(s.life.stage!=StreamLifetime::queued||s.consuming){fail(10);return false;}s.consuming=true;}
    auto &s=slots[selected];consumer_slot=selected;backend_context=f.gpr[2];std::uintptr_t backend{},list{};
    if(!deferred::valid(s.marker,selected)||!pb::read(backend_context+8,backend)||backend!=cfg.backend||!pb::read(f.gpr[1]+8,s.recording)||!s.recording||
       !pb::read(s.recording+0x20,list)||!owners_match(s)||!pb::copy_memory(backend_context+0x1860,parameters.data(),static_cast<unsigned>(parameters.size()))){fail(11);return true;}
    s.list=reinterpret_cast<ID3D12GraphicsCommandList *>(list);return true;
}
void consumer_after(bool admitted,bool returned){if(!admitted)return;auto &s=slots[consumer_slot];
    std::array<std::byte,0x2400> after;std::uintptr_t current{},begin{},end{},list{};
    bool ok=returned&&!fault&&pb::copy_memory(backend_context+0x1860,after.data(),static_cast<unsigned>(after.size()))&&after==parameters&&owners_match(s)&&
        pb::read(s.recording+0x158,current)&&current==reinterpret_cast<std::uintptr_t>(&s.marker)&&pb::read(s.recording+0x160,begin)&&pb::read(s.recording+0x168,end)&&begin==end&&
        pb::read(s.recording+0x20,list)&&list==reinterpret_cast<std::uintptr_t>(s.list);
    {Lock lock;auto *l=find(s.list);ok=ok&&l&&!l->closed&&!l->busy&&l->selected==none&&s.life.mark(l->generation);
        if(ok){s.life.consumer_returned=true;l->selected=consumer_slot;s.marker_states=input_states(*l,s);s.marker_order=l->order;++consumed;}else fail(12);}
    consumer_slot=none;
}
ResourceState *state(List &l,ID3D12Resource *r){for(unsigned i=0;i<l.count;++i)if(l.states[i].resource==r)return &l.states[i];
    if(l.count==l.states.size())return nullptr;auto &s=l.states[l.count++];s={};s.resource=r;return &s;}
void apply_resource(ResourceState &s,const D3D12_RESOURCE_BARRIER &b,std::uint64_t order){
    s.previous=s.state;s.latest=b;
    if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)s.state.transition(b.Transition.StateBefore,b.Transition.StateAfter,b.Flags,b.Transition.Subresource,order);
    else s.state.invalidate(gpu_trace::Issue::alias,order);
    if(!s.invalid_order&&s.state.issue!=gpu_trace::Issue::none&&s.state.issue!=gpu_trace::Issue::unknown){
        s.invalid_order=order;s.invalid_before=s.previous;s.first_invalid=b;}
}
void apply(List &l,const D3D12_RESOURCE_BARRIER &b){const auto order=++l.order;
    if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION){auto *s=state(l,b.Transition.pResource);if(!s){fail(13);return;}apply_resource(*s,b,order);}
    else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING){for(unsigned i=0;i<l.count;++i)if(!b.Aliasing.pResourceBefore||!b.Aliasing.pResourceAfter||l.states[i].resource==b.Aliasing.pResourceBefore||l.states[i].resource==b.Aliasing.pResourceAfter)apply_resource(l.states[i],b,order);}
    else if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_UAV)fail(13);
}
bool evaluate(List &l,unsigned i){auto &s=slots[i];if(pending_recording!=none||!owners_match(s)||!s.life.record()){fail(14);return false;}
    temporal_present::Bindings bindings;if(!obs::current(l.value.Get(),bindings)||bindings.generation!=l.generation){fail(15);return false;}
    auto next=s.projection.history;next.depth=reinterpret_cast<std::uintptr_t>(s.resources[1].Get());const auto decision=cd::history_decision(previous,next);
    if(!decision.valid){fail(16);return false;}
    pending_recording=i;l.injected=i;const auto bits=s.projection.source.projection.jitter_bits;
    const dlss_sdk::HistoryFrame h{next.frame,next.epoch,std::bit_cast<float>(bits[0])*.5F,std::bit_cast<float>(bits[1])*-.5F};
    const bool ok=pass->record(l.value.Get(),reinterpret_cast<Barrier>(originals[6]),{s.resources[0].Get(),s.resources[1].Get(),s.resources[2].Get()},bindings,h,decision.reset,decision.milliseconds,dlss_sdk::RecordFault::none,obs::matches_current);
    if(!ok||!obs::matches_current(l.value.Get(),bindings)){fail(17);return false;}
    history.record(previous,next,decision);previous=next;history_depth=s.resources[1];++frames;++layouts.evaluations[s.native_nodes==5];return true;
}
BoundaryDecision boundary_decision(const Slot &s,std::span<const D3D12_RESOURCE_BARRIER> batch,const std::array<InputState,3> &after){
    BoundaryDecision result;
    for(const auto &b:batch){bool hit=b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)for(const auto &r:s.resources)hit=hit||b.Transition.pResource==r.Get();
        if(!hit)continue;++result.relevant;
        if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION||b.Flags||b.Transition.Subresource!=UINT_MAX||b.Transition.StateBefore!=D3D12_RESOURCE_STATE_RENDER_TARGET||b.Transition.StateAfter!=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)continue;
        result.color=result.color||b.Transition.pResource==s.resources[0].Get();result.motion=result.motion||b.Transition.pResource==s.resources[2].Get();
    }
    for(unsigned j=0;j<3;++j){const auto &a=after[j];if(!a.tracked||!a.value.state.known||a.value.state.issue!=gpu_trace::Issue::none||a.value.state.value!=128)result.invalid_states|=1U<<j;}
    return result;
}
void capture_boundary(BoundarySnapshot &out,const List &l,unsigned i,std::uint64_t begin,
                      const std::array<InputState,3> &before,const std::array<InputState,3> &after,
                      std::span<const D3D12_RESOURCE_BARRIER> batch,const BoundaryDecision &decision){
    if(out.present)return;const auto &s=slots[i];out.present=true;out.slot=i;out.tick=GetTickCount64();out.frame=s.life.frame;out.epoch=s.source.epoch;
    out.native_nodes=s.native_nodes;out.list=reinterpret_cast<std::uintptr_t>(l.value.Get());out.generation=l.generation;
    out.marker_order=s.marker_order;out.begin_order=begin;out.end_order=l.order;out.count=static_cast<unsigned>(batch.size());
    out.decision=decision;out.marker=s.marker_states;out.before=before;out.after=after;
    for(unsigned j=0;j<3;++j)out.resources[j]=reinterpret_cast<std::uintptr_t>(s.resources[j].Get());
    std::copy(batch.begin(),batch.end(),out.batch.begin());
}
void barrier_after(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *data){
    if(!enabled[6]||fault.load())return;std::array<D3D12_RESOURCE_BARRIER,1024> copy;
    if(n>copy.size()||!pb::copy_memory(reinterpret_cast<std::uintptr_t>(data),copy.data(),n*sizeof(copy[0]))){fail(18);return;}
    Lock lock;auto *l=find(list);if(!l||l->closed)return;
    const auto i=l->selected;const auto begin=l->order;
    const auto before=i==none?std::array<InputState,3>{}:input_states(*l,slots[i]);
    for(unsigned j=0;j<n;++j)apply(*l,copy[j]);if(fault||i==none)return;
    const auto after=input_states(*l,slots[i]);const auto batch=std::span(copy).first(n);
    const auto decision=boundary_decision(slots[i],batch,after);if(!decision.relevant)return;
    auto &receipt=!decision.accepted()?first_boundary_rejection:slots[i].native_nodes==5?first_haze_boundary:first_base_boundary;
    capture_boundary(receipt,*l,i,begin,before,after,batch,decision);
    l->selected=none;if(!decision.accepted()){fail(19);return;}evaluate(*l,i);
}
void WINAPI barrier_hook(ID3D12GraphicsCommandList *list,UINT n,const D3D12_RESOURCE_BARRIER *data){
    if(private_list==list){reinterpret_cast<Barrier>(originals[6])(list,n,data);return;}
    ++pending;__try {reinterpret_cast<Barrier>(originals[6])(list,n,data);private_list=list;barrier_after(list,n,data);}
    __finally {private_list=nullptr;--pending;}
}
HRESULT reset_call(ID3D12GraphicsCommandList *list,ID3D12CommandAllocator *allocator,ID3D12PipelineState *pipeline){
    const auto result=reinterpret_cast<HRESULT(WINAPI *)(ID3D12GraphicsCommandList *,ID3D12CommandAllocator *,ID3D12PipelineState *)>(originals[3])(list,allocator,pipeline);
    if(FAILED(result)||!child(list)||list->GetType()!=D3D12_COMMAND_LIST_TYPE_DIRECT)return result;
    Lock lock;auto *l=find(list);if(!l){if(!enabled[3])return result;if(list_count==lists.size()){fail(20);return result;}l=&lists[list_count++];l->value=list;}
    if(l->selected!=none)fail(21);
    if(l->injected!=none){auto &s=slots[l->injected];if(s.list!=list||!s.life.reset(l->generation))fail(21);}
    l->generation=++generations;l->order=0;l->count=0;l->selected=l->injected=none;l->closed=false;
    collect();obs::reset(list,l->generation,pipeline);return result;
}
HRESULT WINAPI reset_hook(ID3D12GraphicsCommandList *list,ID3D12CommandAllocator *allocator,ID3D12PipelineState *pipeline){
    ++pending;HRESULT result{};__try {result=reset_call(list,allocator,pipeline);}__finally {--pending;}return result;
}
HRESULT close_call(ID3D12GraphicsCommandList *list){const auto result=reinterpret_cast<HRESULT(WINAPI *)(ID3D12GraphicsCommandList *)>(originals[4])(list);
    Lock lock;if(auto *l=find(list)){if(l->selected!=none)fail(22);if(SUCCEEDED(result))l->closed=true;else if(l->injected!=none)fail(22);}return result;}
HRESULT WINAPI close_hook(ID3D12GraphicsCommandList *list){++pending;HRESULT result{};__try {result=close_call(list);}__finally {--pending;}return result;}
void execute_call(ID3D12CommandQueue *queue,UINT count,ID3D12CommandList *const *submitted){
    std::array<ID3D12CommandList *,128> copy{};unsigned selected=none;std::uint64_t value{};
    if(count<=copy.size()&&pb::copy_memory(reinterpret_cast<std::uintptr_t>(submitted),copy.data(),count*sizeof(copy[0]))){Lock lock;
        for(unsigned i=0;i<count;++i){auto *l=find(reinterpret_cast<ID3D12GraphicsCommandList *>(copy[i]));if(!l||l->injected==none)continue;
            auto &s=slots[l->injected];if(selected!=none||l->injected!=pending_recording||!l->closed||s.life.stage!=StreamLifetime::recorded||s.life.generation!=l->generation){fail(23);continue;}selected=l->injected;}
        if(selected!=none){if(!child(queue)||queue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT)fail(24);
            else if(last_queue&&last_queue.Get()!=queue&&FAILED(queue->Wait(fence.Get(),signal_value)))fail(24);
            else {value=++signal_value;last_queue=queue;
                auto &s=slots[selected];if(!s.life.submit(s.life.generation,value)){fail(26);value=0;}}}
    }else if(accepting)fail(25);
    reinterpret_cast<void(WINAPI *)(ID3D12CommandQueue *,UINT,ID3D12CommandList *const *)>(originals[5])(queue,count,submitted);
    if(selected!=none&&value){const auto result=queue->Signal(fence.Get(),value);Lock lock;auto &s=slots[selected];
        // Reset may race with the returning Execute/Signal calls. Its separate
        // retirement proof must survive; collection still waits for this receipt.
        if(FAILED(result)||!s.life.confirm(value)){fail(26);return;}pending_recording=none;}
}
void WINAPI execute_hook(ID3D12CommandQueue *queue,UINT count,ID3D12CommandList *const *submitted){++pending;__try {execute_call(queue,count,submitted);}__finally {--pending;}}
void native_call(unsigned kind,pb::Frame *f){bool admitted=false,returned=false;
    if(kind==0)admitted=schedule_before(*f);else if(kind==1)admitted=consumer_before(*f);else admitted=projection_before(*reinterpret_cast<NativeFrame *>(f));
    __try {mhw_binding_forward(originals[kind],f);returned=true;}
    __finally {if(kind==0)schedule_after(admitted,returned);else if(kind==1)consumer_after(admitted,returned);else projection_after(admitted,returned,*reinterpret_cast<NativeFrame *>(f));}
}
}
void native(unsigned kind,pb::Frame *f){++pending;__try {native_call(kind,f);}__finally {--pending;}}
bool install(std::uintptr_t image,const std::filesystem::path &sdk,std::string &reason){
    try {
        if(pass||!pb::discover(image,runtime,reason))return false;
        struct Gate {unsigned rva,bytes;const char *sha;};
        constexpr Gate gates[]{
            {0x24f1fc0,835,"1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190"},
            {0x228e770,528,"33ae7df95058ac2e1ce8d920ff89c62d664068a975931bb10afb95ea88f00b97"},
            {0x24f1620,808,"199d3cb4f03566dcc21aaf72c55e562f568595e3e3954f5987890bdb2c33eaa7"},
            {0x24c7760,2256,"d1e1c91d7cd6ff65cf5fee677a998692fc4e9ead2bedcb7c0a79f88fbe752701"},
            {0x23cabf0,99,"5c63fd6c587c250e0fb09f02c2cf776fb44ed90944e90d1b98c80a700036d433"},
            {0x229cee0,7756,"11f7cc4d48598967f439659c87142839c4ad3fb900cfafb7fab3631655691af3"},
            {0x23d2370,1047,"0cf82d2b8b6259fbb5cad0c7468a780bb7026d9a20f720e9bbd8b5cd501fffc3"},
            {0x23c3480,265,"e1dfbdd44786bac3a9b62e900b3fb4a844dcd4c839425a3753899087e44e0245"},
            {0x22986b3,1125,"80408f28b51d0fc50cfc0bab29eb961cdec4bf20da559b675fd12c7169029765"},
            {0x228f3b0,2000,"df1bf81584401fe529bc5fbbc3e19010e8959c2d13745eca6f87897183148373"}};
        for(const auto &g:gates)if(!pb::verify_code(image,g.rva,g.bytes,g.sha))throw std::runtime_error("AA stream code gate");
        cfg={image,0,runtime.backend,runtime.device};if(!pb::read(image+0x51c4480,cfg.renderer)||!cfg.renderer||runtime.width!=2560||runtime.height!=1440)throw std::runtime_error("AA stream renderer/extent");
        auto *device=reinterpret_cast<ID3D12Device *>(cfg.device);const auto *initialization=ngx_startup::borrowed_initialization();if(!initialization)throw std::runtime_error("AA stream startup ownership");
        pass=std::make_unique<StreamPass>(device,runtime.width,runtime.height,sdk,*initialization);
        if(FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))throw std::runtime_error("AA stream fence");
        ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC q{};
        if(FAILED(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator)))||FAILED(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)))||FAILED(list->Close())||FAILED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue))))throw std::runtime_error("AA stream API discovery");
        targets[0]=image+0x24f1fc0;targets[1]=image+0x259e1e0;targets[2]=image+0x228f3b0;
        constexpr unsigned api[]{10,9,10,26};for(unsigned i=3;i<7;++i)if(!pb::system_method(reinterpret_cast<std::uintptr_t>(i==5?static_cast<IUnknown *>(queue.Get()):static_cast<IUnknown *>(list.Get())),api[i-3],targets[i]))throw std::runtime_error("AA stream API gate");
        const std::array<void *,7> detours{reinterpret_cast<void *>(aa_schedule_entry),reinterpret_cast<void *>(aa_marker_entry),reinterpret_cast<void *>(temporal_jitter_entry),reinterpret_cast<void *>(reset_hook),reinterpret_cast<void *>(close_hook),reinterpret_cast<void *>(execute_hook),reinterpret_cast<void *>(barrier_hook)};
        if(!obs::install(device,pb::system_method,reason))return false;
        for(unsigned i=0;i<7;++i){if(MH_CreateHook(reinterpret_cast<void *>(targets[i]),detours[i],&originals[i])!=MH_OK)throw std::runtime_error("AA stream hook creation");
            if(MH_QueueEnableHook(reinterpret_cast<void *>(targets[i]))!=MH_OK)throw std::runtime_error("AA stream queue enable");enabled[i]=true;}
        owned::configure(image,runtime.device,publish);obs::begin();if(MH_ApplyQueued()!=MH_OK)throw std::runtime_error("AA stream enable");
        // Observe Reset generations before allowing any projection stimulus.
        Sleep(150);accepting=true;return true;
    }catch(const std::exception &e){reason=e.what();stop();return false;}
}
bool healthy() noexcept{return !fault.load()&&obs::healthy();}
void stop() noexcept{accepting=false;}
void finish() noexcept{
    stop();const auto until=GetTickCount64()+3000;
    // Keep submission/Reset observers until issued work drains; no new jitter.
    while(GetTickCount64()<until){bool done{};{Lock lock;collect();done=pending_recording==none;for(const auto &s:slots)if(s.life.stage>=StreamLifetime::queued)done=false;}if(done&&!pending)break;Sleep(1);}
    for(unsigned i=0;i<7;++i)if(enabled[i]&&MH_QueueDisableHook(reinterpret_cast<void *>(targets[i]))!=MH_OK)fail(27);
    if(MH_ApplyQueued()==MH_OK){for(auto &e:enabled)e=false;}else fail(27);
    const auto drain=GetTickCount64()+2000;while(pending&&GetTickCount64()<drain)Sleep(1);
    if(pending||projection_busy.test()||scheduler_busy.test()||!owned::detached())fail(28);
    if(!obs::finish())fail(29);
    // Private SDK/list resources remain pinned at stop. Uncertain recordings
    // never cause unsafe release; process teardown owns their final cleanup.
}
void write_state(std::ostream &out,const gpu_trace::State &s){
    out<<"{\"known\":"<<s.known<<",\"value\":"<<s.value<<",\"transitions\":"<<s.transitions<<",\"issue\":"<<static_cast<unsigned>(s.issue)<<",\"last\":"<<s.last<<'}';
}
void write_barrier(std::ostream &out,const D3D12_RESOURCE_BARRIER &b){
    out<<"{\"type\":"<<static_cast<unsigned>(b.Type)<<",\"flags\":"<<static_cast<unsigned>(b.Flags);
    if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)out<<",\"resource\":"<<reinterpret_cast<std::uintptr_t>(b.Transition.pResource)<<",\"subresource\":"<<b.Transition.Subresource<<",\"before\":"<<static_cast<unsigned>(b.Transition.StateBefore)<<",\"after\":"<<static_cast<unsigned>(b.Transition.StateAfter);
    else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING)out<<",\"before_resource\":"<<reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore)<<",\"after_resource\":"<<reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter);
    else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV)out<<",\"resource\":"<<reinterpret_cast<std::uintptr_t>(b.UAV.pResource);
    out<<'}';
}
void write_inputs(std::ostream &out,const std::array<InputState,3> &a){
    out<<'[';for(unsigned i=0;i<3;++i){if(i)out<<',';const auto &v=a[i].value;
        out<<"{\"tracked\":"<<a[i].tracked<<",\"resource\":"<<reinterpret_cast<std::uintptr_t>(v.resource)<<",\"state\":";write_state(out,v.state);
        out<<",\"previous\":";write_state(out,v.previous);out<<",\"latest\":";if(v.state.last)write_barrier(out,v.latest);else out<<"null";
        out<<",\"invalid_order\":"<<v.invalid_order<<",\"invalid_before\":";write_state(out,v.invalid_before);
        out<<",\"first_invalid\":";if(v.invalid_order)write_barrier(out,v.first_invalid);else out<<"null";out<<'}';}out<<']';
}
void write_boundary(std::ostream &out,const BoundarySnapshot &s){
    if(!s.present){out<<"null";return;}
    out<<"{\"schema\":1,\"slot\":"<<s.slot<<",\"tick\":"<<s.tick<<",\"frame\":"<<s.frame<<",\"epoch\":"<<s.epoch<<",\"native_nodes\":"<<s.native_nodes
       <<",\"list\":"<<s.list<<",\"generation\":"<<s.generation<<",\"marker_order\":"<<s.marker_order<<",\"begin_order\":"<<s.begin_order<<",\"end_order\":"<<s.end_order
       <<",\"resources\":["<<s.resources[0]<<','<<s.resources[1]<<','<<s.resources[2]<<"],\"relevant\":"<<s.decision.relevant<<",\"color\":"<<s.decision.color
       <<",\"motion\":"<<s.decision.motion<<",\"invalid_states\":"<<s.decision.invalid_states<<",\"accepted\":"<<s.decision.accepted()<<",\"marker\":";
    write_inputs(out,s.marker);out<<",\"before\":";write_inputs(out,s.before);out<<",\"after\":";write_inputs(out,s.after);
    out<<",\"count\":"<<s.count<<",\"batch\":[";for(unsigned i=0;i<s.count;++i){if(i)out<<',';write_barrier(out,s.batch[i]);}out<<"]}";
}
void write(std::ostream &out){Lock lock;collect();bool disabled=true;for(bool e:enabled)disabled=disabled&&!e;
    unsigned live{},awaiting_gpu{},awaiting_reset{},awaiting_submit{};const auto completed=fence?fence->GetCompletedValue():0;
    for(const auto &s:slots)if(s.life.stage!=StreamLifetime::free){++live;if(s.life.stage>=StreamLifetime::queued){awaiting_submit+=!s.life.signal;awaiting_reset+=!s.life.retired;awaiting_gpu+=!s.life.signal||completed==UINT64_MAX||completed<s.life.signal;}}
    out<<std::boolalpha<<"{\"event\":\"continuous\",\"schema\":1,\"active\":"<<accepting.load()<<",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"frames\":"<<frames.load()
       <<",\"native_aa_required\":\"off\",\"native_aa_writes\":0,\"jitter_injections\":"<<injections.load()<<",\"jitter_restorations\":"<<restorations.load()<<",\"published\":"<<published.load()<<",\"consumed\":"<<consumed.load()
       <<",\"released\":"<<released.load()<<",\"skips\":"<<skips.load()<<",\"signal_value\":"<<signal_value<<",\"completed_value\":"<<completed<<",\"pending_private_recording\":"<<(pending_recording!=none)
       <<",\"hooks_disabled\":"<<disabled<<",\"bindings_hooks_disabled\":"<<obs::hooks_disabled()<<",\"bindings_healthy\":"<<obs::healthy()<<",\"descriptors_detached\":"<<owned::detached()
       <<",\"live_slots\":"<<live<<",\"peak_slots\":"<<peak_slots<<",\"awaiting_gpu\":"<<awaiting_gpu<<",\"awaiting_reset\":"<<awaiting_reset<<",\"awaiting_submit\":"<<awaiting_submit
       <<",\"list_count\":"<<list_count<<",\"width\":"<<runtime.width<<",\"height\":"<<runtime.height<<",\"format\":"<<runtime.format<<",\"luid_low\":"<<runtime.luid_low<<",\"luid_high\":"<<runtime.luid_high<<",\"device\":"<<runtime.device<<",\"backend\":"<<runtime.backend<<",\"history\":";
    history.write(out);out<<",\"pass\":";if(pass)pass->write(out);else out<<"null";
    out<<",\"queue_rejection\":";
    const auto &r=first_queue_rejection;
    if(!r.present)out<<"null";
    else {out<<"{\"schema\":1,\"slot\":"<<r.slot<<",\"tick\":"<<r.tick<<",\"recycled\":"<<r.recycled<<",\"previous_admitted\":";
        write_post_queue(out,r.previous);out<<",\"rejected\":";write_post_queue(out,r.rejected);
        out<<",\"admission\":";owned::write_admission(out,r.admission,write_post_queue);out<<'}';}
    out<<",\"scheduler_layouts\":{\"policy\":\"base-or-haze-v1\",\"base_admissions\":"<<layouts.admissions[0]
       <<",\"haze_admissions\":"<<layouts.admissions[1]<<",\"base_returns\":"<<layouts.returns[0]<<",\"haze_returns\":"<<layouts.returns[1]
       <<",\"base_evaluations\":"<<layouts.evaluations[0]<<",\"haze_evaluations\":"<<layouts.evaluations[1]
       <<",\"haze_entries\":"<<layouts.haze_entries<<",\"haze_exits\":"<<layouts.haze_exits<<",\"first_haze\":";
    if(layouts.first_haze.complete)owned::write_return(out,layouts.first_haze);else out<<"null";
    out<<",\"first_base_after_haze\":";
    if(layouts.first_base_after_haze.complete)owned::write_return(out,layouts.first_base_after_haze);else out<<"null";
    out<<",\"first_failed_return\":";
    if(layouts.failed_return)owned::write_return(out,layouts.first_failed_return);else out<<"null";
    out<<"},\"input_boundaries\":{\"policy\":\"exact-color-motion-v1\",\"first_base\":";write_boundary(out,first_base_boundary);
    out<<",\"first_haze\":";write_boundary(out,first_haze_boundary);out<<",\"first_rejection\":";write_boundary(out,first_boundary_rejection);
    out<<"}}\n"<<std::flush;
}
}
extern "C" void mhw_aa_schedule(mhw::projection_binding::Frame *f){mhw::aa_control::stream::native(0,f);}
extern "C" void mhw_aa_marker(mhw::projection_binding::Frame *f){mhw::aa_control::stream::native(1,f);}
extern "C" void mhw_temporal_jitter(mhw::temporal_bindings::NativeFrame *f){mhw::aa_control::stream::native(2,&f->call);}
