#include "deferred_runtime.hpp"
#include "deferred_contract.hpp"
#ifdef MHW_AA_GPU_TRACE
#include "gpu_trace.hpp"
#endif
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <d3d12.h>
#include <MinHook.h>
#include <atomic>
#include <ostream>
#include <cstring>
extern "C" void aa_marker_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::aa_control::deferred {
namespace {
namespace pb=projection_binding;namespace tb=temporal_bindings;
constexpr unsigned capacity=8,consumer=0x259e1e0;
struct Input {tb::Owner owner;ID3D12Resource *pin{};D3D12_RESOURCE_DESC desc{};};
struct Slot {
    Marker marker;
    PostQueue source;
    std::array<Input,3> inputs;
    std::array<std::byte,0x2400> parameters;
    std::atomic<unsigned> stage{}; // 0 empty, 1 publishing, 2 published, 3 consuming, 4 complete, 5 failed
    std::uintptr_t entry{},executor{},backend_context{},recording{},list{},cached_batch{};
    std::uint64_t publish_tick{},consume_tick{},current_frame{};
    unsigned key{},old_phase{},old_offset{},limit{},queue_capacity{},calls{},thread{},reason{};
    bool enqueued{},scope_restored{},owners_match{},parameters_same{},native_returned{},list_valid{},pins_released{};
};
std::array<Slot,capacity> slots;
std::uintptr_t image{},device{},backend{};void *original{};
std::atomic<unsigned> pending{},failure{};bool enabled{};
bool read(void *,std::uintptr_t at,std::span<std::byte> out){return pb::copy_memory(at,out.data(),static_cast<unsigned>(out.size()));}
const tb::Reader reader{nullptr,read};
void fail(unsigned reason){unsigned zero{};failure.compare_exchange_strong(zero,reason);}
bool writable(std::uintptr_t address,std::size_t bytes) noexcept {
    MEMORY_BASIC_INFORMATION m{};
    if(!address||address>UINTPTR_MAX-bytes||VirtualQuery(reinterpret_cast<void *>(address),&m,sizeof(m))!=sizeof(m)||
       m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto p=m.Protect&255;const auto base=reinterpret_cast<std::uintptr_t>(m.BaseAddress);
    return (p==PAGE_READWRITE||p==PAGE_WRITECOPY)&&address>=base&&bytes<=m.RegionSize&&address-base<=m.RegionSize-bytes;
}
bool pin(Input &input,unsigned role) noexcept {
    std::uintptr_t method{};ID3D12Device *owner{};bool ok{};
    if(!pb::system_method(input.owner.resource,0,method)||!pb::system_method(input.owner.resource,2,method)||
       !pb::system_method(input.owner.resource,7,method)||!pb::system_method(input.owner.resource,10,method))return false;
    __try {
        if(SUCCEEDED(reinterpret_cast<IUnknown *>(input.owner.resource)->QueryInterface(IID_PPV_ARGS(&input.pin)))&&input.pin&&
           reinterpret_cast<std::uintptr_t>(input.pin)==input.owner.resource&&SUCCEEDED(input.pin->GetDevice(IID_PPV_ARGS(&owner)))&&owner){
            input.desc=input.pin->GetDesc();const auto &d=input.desc;
            ok=reinterpret_cast<std::uintptr_t>(owner)==device&&d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&
                d.Width==2560&&d.Height==1440&&d.DepthOrArraySize==1&&d.MipLevels==1&&d.SampleDesc.Count==1&&d.SampleDesc.Quality==0&&
                (role!=0||d.Format==DXGI_FORMAT_R11G11B10_FLOAT);
        }
    } __finally {if(owner)owner->Release();}
    return ok;
}
bool owners(Slot &s) noexcept {
    for(const auto &input:s.inputs){tb::Owner a,b;
        if(!tb::read_owner(reader,input.owner.object,a)||!tb::read_owner(reader,input.owner.object,b)||a!=b||a!=input.owner)return false;
    }
    return true;
}
bool list_identity(std::uintptr_t address) noexcept {
    std::uintptr_t method{};ID3D12GraphicsCommandList *held{};ID3D12Device *owner{};bool ok{};
    if(!pb::system_method(address,0,method)||!pb::system_method(address,2,method)||
       !pb::system_method(address,7,method)||!pb::system_method(address,8,method))return false;
    __try {
        if(SUCCEEDED(reinterpret_cast<IUnknown *>(address)->QueryInterface(IID_PPV_ARGS(&held)))&&held&&
           reinterpret_cast<std::uintptr_t>(held)==address&&SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&owner)))&&owner)
            ok=reinterpret_cast<std::uintptr_t>(owner)==device&&held->GetType()==D3D12_COMMAND_LIST_TYPE_DIRECT;
    } __finally {if(owner)owner->Release();if(held)held->Release();}
    return ok;
}
bool enqueue(Slot &s,unsigned index) noexcept {
    const auto context=s.source.context;std::uintptr_t entries{},returned{},found{};unsigned count{};
    if(!pb::read(context+0x2b78,entries)||!pb::read(context+0x2b80,count)||count!=s.source.packet_count||
       !pb::read(context+0x2b84,s.limit)||!pb::read(context+0x2b88,s.queue_capacity)||
       !pb::read(context+0x154,s.old_phase)||!pb::read(context+0x15c,s.old_offset)||
       !pb::read(context+0x1a8,s.cached_batch)||s.cached_batch||!admit(count,s.limit,s.queue_capacity,79,0)||
       entries>UINTPTR_MAX-16ULL*(count+1)||!writable(entries+16ULL*count,16)||!writable(context+0x154,12)||
       !writable(context+0x2b80,4))return false;
    s.entry=entries+16ULL*count;s.marker.generation=index+1;s.publish_tick=GetTickCount64();
    // The native pass callback does not set these for us. Scope only the key;
    // never install this packet as a coalescible native parameter batch (+1a8).
    __try {
        *reinterpret_cast<unsigned *>(context+0x154)=79;
        *reinterpret_cast<unsigned *>(context+0x15c)=s.old_offset&31U;
        s.stage.store(2,std::memory_order_release);
        returned=reinterpret_cast<std::uintptr_t(*)(std::uintptr_t,Marker *)>(image+0x23cabf0)(context,&s.marker);
        s.enqueued=true;
    } __finally {
        *reinterpret_cast<unsigned *>(context+0x154)=s.old_phase;
        *reinterpret_cast<unsigned *>(context+0x15c)=s.old_offset;
    }
    unsigned phase{},offset{},after{};std::uintptr_t cached{};
    s.scope_restored=pb::read(context+0x154,phase)&&phase==s.old_phase&&pb::read(context+0x15c,offset)&&offset==s.old_offset&&
        pb::read(context+0x1a8,cached)&&cached==s.cached_batch;
    return s.scope_restored&&returned==s.entry&&pb::read(s.entry,s.key)&&s.key==((15U<<27)|2U)&&
        pb::read(s.entry+8,found)&&found==reinterpret_cast<std::uintptr_t>(&s.marker)&&
        pb::read(context+0x2b80,after)&&after==count+1&&valid(s.marker,index);
}
unsigned before(const pb::Frame &f) noexcept {
    for(unsigned i=0;i<capacity;++i){auto &s=slots[i];
        if(f.gpr[3]!=reinterpret_cast<std::uintptr_t>(&s.marker))continue;
        unsigned expected=2;
        if(!s.stage.compare_exchange_strong(expected,3,std::memory_order_acq_rel)){fail(3);return capacity;}
        ++s.calls;s.consume_tick=GetTickCount64();s.thread=GetCurrentThreadId();
        s.executor=f.gpr[1];s.backend_context=f.gpr[2];std::uintptr_t actual_backend{};
        if(!valid(s.marker,i)||!pb::read(s.backend_context+8,actual_backend)||actual_backend!=backend||
           !pb::read(s.executor+8,s.recording)||!s.recording||!pb::read(s.recording+0x20,s.list)||
           !(s.list_valid=list_identity(s.list))||!(s.owners_match=owners(s))||
           !pb::copy_memory(s.backend_context+0x1860,s.parameters.data(),static_cast<unsigned>(s.parameters.size()))||
           !pb::read(s.source.renderer+0x1f210,s.current_frame)){s.reason=4;fail(4);}
        return i;
    }
    return capacity;
}
void after(unsigned i,bool returned) noexcept {
    if(i>=capacity)return;auto &s=slots[i];s.native_returned=returned;
    std::array<std::byte,0x2400> parameters;std::uintptr_t current{},begin{},end{},list{};
    s.parameters_same=pb::copy_memory(s.backend_context+0x1860,parameters.data(),static_cast<unsigned>(parameters.size()))&&parameters==s.parameters;
    bool ok=returned&&!s.reason&&valid(s.marker,i)&&s.parameters_same&&owners(s)&&
        pb::read(s.recording+0x158,current)&&current==reinterpret_cast<std::uintptr_t>(&s.marker)&&
        pb::read(s.recording+0x160,begin)&&pb::read(s.recording+0x168,end)&&begin==end&&
        pb::read(s.recording+0x20,list)&&list==s.list;
#ifdef MHW_AA_GPU_TRACE
    // Native preparation/barrier flushing is finished. This is the insertion
    // point for future private GPU commands; today's marker only records state.
    if(ok)ok=gpu_trace::mark(i,s.source.frame,reinterpret_cast<std::uintptr_t>(&s.marker),reinterpret_cast<ID3D12GraphicsCommandList *>(s.list));
#endif
    if(!ok){s.reason=5;fail(5);}s.stage.store(ok?4U:5U,std::memory_order_release);
}
void dispatch(pb::Frame *f){
    const auto i=before(*f);bool returned{};
    __try {mhw_binding_forward(original,f);returned=true;}
    __finally {after(i,returned);}
}
}
bool prepare(std::uintptr_t base,const pb::Runtime &runtime) noexcept {
    image=base;device=runtime.device;backend=runtime.backend;
    if(!pb::verify_code(base,0x23cabf0,99,"5c63fd6c587c250e0fb09f02c2cf776fb44ed90944e90d1b98c80a700036d433")||
       !pb::verify_code(base,0x229cee0,7756,"11f7cc4d48598967f439659c87142839c4ad3fb900cfafb7fab3631655691af3")||
       !pb::verify_code(base,0x23d2370,1047,"0cf82d2b8b6259fbb5cad0c7468a780bb7026d9a20f720e9bbd8b5cd501fffc3")||
       !pb::verify_code(base,0x23c3480,265,"e1dfbdd44786bac3a9b62e900b3fb4a844dcd4c839425a3753899087e44e0245")||
       !pb::verify_code(base,0x22986b3,1125,"80408f28b51d0fc50cfc0bab29eb961cdec4bf20da559b675fd12c7169029765"))return false;
    if(MH_CreateHook(reinterpret_cast<void *>(base+consumer),reinterpret_cast<void *>(aa_marker_entry),&original)!=MH_OK)return false;
#ifdef MHW_AA_GPU_TRACE
    std::uintptr_t renderer{},view{},manager{},color{},gbuffer{},motion{};tb::Owner color_owner,motion_owner;tb::SceneDepth depth;tb::Snapshot anchor;
    if(!pb::read(base+0x51c4480,renderer)||!renderer||!pb::read(renderer+0x1f450,view)||!view||
       !pb::read(base+0x51cc238,manager)||!manager||!pb::read(manager+0xc0,color)||!color||
       !tb::read_owner(reader,color,color_owner)||!pb::read(view+0x620,gbuffer)||!gbuffer||
       !pb::read(gbuffer+0x78,motion)||!tb::read_owner(reader,motion,motion_owner))return false;
    anchor.context=renderer+0x710;anchor.view=view;if(!tb::scene_depth(reader,anchor,depth))return false;
    const std::array<std::uintptr_t,3> resources={color_owner.resource,depth.texture.resource,motion_owner.resource};
    std::string reason;
    if(!gpu_trace::install({base,renderer,backend,device},resources,reason)){gpu_trace::finish();return false;}
#endif
    return true;
}
bool enable() noexcept {
    enabled=MH_EnableHook(reinterpret_cast<void *>(image+consumer))==MH_OK;
#ifdef MHW_AA_GPU_TRACE
    if(enabled){gpu_trace::begin();Sleep(tb::submission::warmup_ms);}
    else gpu_trace::finish();
#endif
    return enabled;
}
bool publish(unsigned i,const PostQueue &q,const tb::Owner &color) noexcept {
    if(i>=capacity||failure)return false;auto &s=slots[i];unsigned zero{};
    if(!s.stage.compare_exchange_strong(zero,1)){fail(1);return false;}
    s.source=q;s.inputs[0].owner=color;
    tb::Snapshot anchor;anchor.context=q.context;anchor.view=q.view;tb::SceneDepth depth;
    std::uintptr_t gbuffer{},motion{};
    bool ok=tb::scene_depth(reader,anchor,depth)&&pb::read(q.view+0x620,gbuffer)&&gbuffer&&
        pb::read(gbuffer+0x78,motion)&&tb::read_owner(reader,motion,s.inputs[2].owner);
    s.inputs[1].owner=depth.texture;
    for(unsigned j=0;ok&&j<3;++j){ok=pin(s.inputs[j],j);
        for(unsigned k=0;k<j;++k)if(s.inputs[j].owner.resource==s.inputs[k].owner.resource)ok=false;
    }
#ifdef MHW_AA_GPU_TRACE
    if(ok)ok=gpu_trace::matches({s.inputs[0].owner.resource,s.inputs[1].owner.resource,s.inputs[2].owner.resource});
#endif
    if(ok)ok=owners(s)&&enqueue(s,i);
    if(!ok){s.reason=2;s.stage=5;fail(2);}return ok;
}
bool finished() noexcept {
#ifdef MHW_AA_GPU_TRACE
    gpu_trace::poll();if(!gpu_trace::healthy())fail(8);
#endif
    if(failure)return true;
    for(const auto &s:slots)if(s.stage.load(std::memory_order_acquire)!=4)return false;
#ifdef MHW_AA_GPU_TRACE
    return gpu_trace::complete();
#else
    return true;
#endif
}
bool close_observation() noexcept {
#ifdef MHW_AA_GPU_TRACE
    return gpu_trace::close_admission();
#else
    return true;
#endif
}
bool stop() noexcept {
    if(!close_observation())return false;
    if(enabled){if(MH_DisableHook(reinterpret_cast<void *>(image+consumer))!=MH_OK)return false;enabled=false;}
    const auto end=GetTickCount64()+2000;while(pending&&GetTickCount64()<end)Sleep(1);
    if(pending)return false;
#ifdef MHW_AA_GPU_TRACE
    if(!gpu_trace::finish())return false;
    if(!gpu_trace::qualified())fail(9);
    // In this version, sampled input references also follow the observed GPU
    // fence plus list-retirement chain. An incomplete trace retains them.
    if(!gpu_trace::safe_release())return true;
#endif
    // No private GPU command references these pins. All diagnostic callbacks
    // have drained. Packet/slot memory itself lives until process termination.
    for(auto &s:slots){for(auto &input:s.inputs)if(input.pin){input.pin->Release();input.pin=nullptr;}s.pins_released=true;}
    return true;
}
unsigned fault() noexcept {return failure.load();}
bool disabled() noexcept {return !enabled
#ifdef MHW_AA_GPU_TRACE
    &&gpu_trace::disabled()
#endif
    ;}
void write(std::ostream &out){
    out<<"{\"event\":\"deferred\",\"fault\":"<<failure.load()<<",\"pending\":"<<pending.load()
       <<",\"hook_disabled\":"<<!enabled<<",\"gpu_states_verified\":false,\"private_gpu_commands\":0,\"requests\":[";
    for(unsigned i=0;i<capacity;++i){const auto &s=slots[i];out<<(i?",":"")<<"{\"index\":"<<i<<",\"stage\":"<<s.stage.load()<<",\"reason\":"<<s.reason
        <<",\"generation\":"<<s.marker.generation<<",\"packet\":"<<reinterpret_cast<std::uintptr_t>(&s.marker)<<",\"entry_at_publish\":"<<s.entry
        <<",\"key\":"<<s.key<<",\"queue_limit\":"<<s.limit<<",\"queue_capacity\":"<<s.queue_capacity<<",\"old_phase\":"<<s.old_phase<<",\"old_offset\":"<<s.old_offset
        <<",\"cached_batch\":"<<s.cached_batch<<",\"enqueued\":"<<s.enqueued<<",\"scope_restored\":"<<s.scope_restored
        <<",\"source_frame\":"<<s.source.frame<<",\"source_epoch\":"<<s.source.epoch<<",\"source_bank\":"<<s.source.bank<<",\"renderer_frame_at_consume\":"<<s.current_frame
        <<",\"publish_tick\":"<<s.publish_tick<<",\"consume_tick\":"<<s.consume_tick<<",\"calls\":"<<s.calls<<",\"thread\":"<<s.thread
        <<",\"executor\":"<<s.executor<<",\"backend_context\":"<<s.backend_context<<",\"recording\":"<<s.recording<<",\"list\":"<<s.list
        <<",\"list_valid\":"<<s.list_valid<<",\"owners_match\":"<<s.owners_match<<",\"parameters_same\":"<<s.parameters_same
        <<",\"native_returned\":"<<s.native_returned<<",\"pins_released\":"<<s.pins_released<<",\"inputs\":[";
        for(unsigned j=0;j<3;++j){const auto &v=s.inputs[j];const auto &d=v.desc;out<<(j?",":"")<<"{\"role\":"<<j
            <<",\"owner\":"<<v.owner.object<<",\"backing\":"<<v.owner.backing<<",\"resource\":"<<v.owner.resource
            <<",\"format\":"<<d.Format<<",\"dimension\":"<<d.Dimension<<",\"width\":"<<d.Width<<",\"height\":"<<d.Height
            <<",\"flags\":"<<d.Flags<<",\"mips\":"<<d.MipLevels<<",\"array\":"<<d.DepthOrArraySize<<",\"samples\":"<<d.SampleDesc.Count<<'}';}
        out<<"]}";
    }out<<"]}\n";
#ifdef MHW_AA_GPU_TRACE
    gpu_trace::write(out);
#endif
}
void entry(pb::Frame *frame){++pending;__try{dispatch(frame);}__finally{--pending;}}
}
extern "C" void mhw_aa_marker(mhw::projection_binding::Frame *frame){mhw::aa_control::deferred::entry(frame);}
