#include "observer.hpp"
#include <MinHook.h>
#include <wrl/client.h>
#include <atomic>
#include <cstring>
#include <ostream>
#include <intrin.h>

namespace mhw::temporal_present::observer {
namespace {
constexpr unsigned raster_hooks=hook_count+5,predicate_hooks=raster_hooks+1,max_hooks=predicate_hooks+1;
constexpr std::array<unsigned,max_hooks> methods{11,25,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,20,21,22,44,46,55,43};
struct Entry {ID3D12GraphicsCommandList *list{};Bindings state;RasterBindings raster;Predicate predicate;};
#ifdef MHW_FG_SCENE_PROBE
struct OutputObservation {
    bool seen{},rtv_argument{},rtv_read{},depth_argument{},depth_read{};
    unsigned count{},contiguous{};std::uintptr_t rtv{},depth{};
};
// Same bounded index as entries. No COM references or resource calls.
std::array<OutputObservation,list_limit> output_observations;
#endif
struct Sample {bool captured{};std::uint64_t frame{};std::uintptr_t list{},expected_root{},expected_pipeline{};Bindings state;};
std::array<Entry,list_limit> entries;
std::array<Sample,sample_limit> samples;
std::array<std::uintptr_t,max_hooks> targets;
std::array<void *,max_hooks> originals;
std::array<bool,max_hooks> created,enabled;
unsigned entry_count{},installed_hooks=hook_count;
std::atomic<unsigned> pending{},fault{};
std::atomic<bool> accepting{};
bool installed{},drained{};
ViewportListener viewport_listener{};
std::atomic<std::uint64_t> viewport_notifications{};
SRWLOCK mutex=SRWLOCK_INIT;
struct Lock {Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
void fail(unsigned f) noexcept {unsigned zero{};fault.compare_exchange_strong(zero,f);accepting.store(false,std::memory_order_release);}
bool enter() noexcept {
    if(!accepting.load(std::memory_order_acquire))return false;
    pending.fetch_add(1,std::memory_order_acq_rel);
    if(!accepting.load(std::memory_order_acquire)){pending.fetch_sub(1,std::memory_order_release);return false;}
    return true;
}
Entry *find(ID3D12GraphicsCommandList *p) noexcept {for(unsigned i=0;i<entry_count;++i)if(entries[i].list==p)return &entries[i];return nullptr;}
bool copy(const void *source,void *dest,std::size_t bytes) noexcept {
    if(!source&&bytes)return false;
    __try{if(bytes)std::memcpy(dest,source,bytes);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Updates take place after successful forwarding. No lock spans a driver call.
void after(unsigned which,ID3D12GraphicsCommandList *list,bool admitted,bool returned,
           std::uintptr_t a=0,std::uintptr_t b=0,std::uintptr_t c=0,[[maybe_unused]] std::uintptr_t d=0) noexcept {
    if(!admitted)return;
    {Lock lock;if(auto *entry=find(list)){
        auto &s=entry->state;
        if(!returned){s.invalidate();entry->raster={};entry->predicate={};}
        else if(which==0){s.reset(s.generation,a);entry->raster={};entry->raster.generation=s.generation;entry->predicate={0,0,D3D12_PREDICATION_OP_EQUAL_ZERO,true};} // ClearState preserves Reset generation.
        else if(which==1)s.pipeline=a;
        else if(which==2){s.invalidate();entry->predicate={};} // Bundle mutations are intentionally unsupported.
        else if(which==23)entry->predicate={a,b,static_cast<D3D12_PREDICATION_OP>(c),true};
        else if(which==24){entry->raster.index_known=a&&copy(reinterpret_cast<const void *>(a),&entry->raster.index_buffer,sizeof(entry->raster.index_buffer));}
        else if(which==3){std::array<std::uintptr_t,2> heaps{};
            if(a>2||!copy(reinterpret_cast<const void *>(b),heaps.data(),a*sizeof(std::uintptr_t)))s.invalidate();
            else s.descriptor_heaps(std::span(heaps).first(a));
        }else if(which==4||which==5)s.root(which==5,a);
        else if(which>=6&&which<=7)s.descriptor(which==7,static_cast<unsigned>(a),ArgumentKind::table,b);
        else if(which>=8&&which<=9){const auto value=static_cast<std::uint32_t>(b);s.constants(which==9,static_cast<unsigned>(a),static_cast<unsigned>(c),std::span(&value,1));}
        else if(which>=10&&which<=11){std::array<std::uint32_t,64> values{};const auto count=static_cast<unsigned>(b>>32),offset=static_cast<unsigned>(b);
            if(count>64||!copy(reinterpret_cast<const void *>(c),values.data(),std::size_t(count)*4))s.invalidate();
            else s.constants(which==11,static_cast<unsigned>(a),offset,std::span(values).first(count));
        }else if(which<18){constexpr ArgumentKind kinds[]{ArgumentKind::cbv,ArgumentKind::srv,ArgumentKind::uav};
            s.descriptor((which&1)!=0,static_cast<unsigned>(a),kinds[(which-12)/2],b);
        }else{
            auto &r=entry->raster;
            if(which==18){r.topology=static_cast<D3D_PRIMITIVE_TOPOLOGY>(a);r.topology_known=true;}
            if(which==19)r.viewport_known=a==1&&copy(reinterpret_cast<const void *>(b),&r.viewport,sizeof(r.viewport));
            if(which==20)r.scissor_known=a==1&&copy(reinterpret_cast<const void *>(b),&r.scissor,sizeof(r.scissor));
            if(which==21){
                if(a>=r.vertex_buffers.size()||b>r.vertex_buffers.size()-a){r.vertex_known=false;r.vertex_mask=0;}
                else{for(unsigned i=0;i<b;++i){const auto slot=static_cast<unsigned>(a)+i;
                    if(c&&copy(reinterpret_cast<const void *>(c+i*sizeof(D3D12_VERTEX_BUFFER_VIEW)),&r.vertex_buffers[slot],sizeof(D3D12_VERTEX_BUFFER_VIEW)))r.vertex_mask|=1U<<slot;
                    else r.vertex_mask&=~(1U<<slot);}
                    if(!a){r.vertex_known=b>0&&(r.vertex_mask&1U);if(r.vertex_known)r.vertex_buffer=r.vertex_buffers[0];}}
            }
            // c is the depth descriptor pointer, not the contiguous-range flag.
            // Keep the existing rendering admission contract unchanged.
            if(which==22){r.output_known=a==1&&!c&&copy(reinterpret_cast<const void *>(b),&r.render_target,sizeof(r.render_target));
#ifdef MHW_FG_SCENE_PROBE
                auto &o=output_observations[static_cast<std::size_t>(entry-entries.data())];o={};
                o.seen=true;o.count=static_cast<unsigned>(a);o.contiguous=static_cast<unsigned>(d);
                o.rtv_argument=b!=0;o.depth_argument=c!=0;
                if(r.output_known){o.rtv_read=true;o.rtv=r.render_target;}
                else if(a&&b){std::uintptr_t value{};o.rtv_read=copy(reinterpret_cast<const void *>(b),&value,sizeof(value));if(o.rtv_read)o.rtv=value;}
                if(c){std::uintptr_t value{};o.depth_read=copy(reinterpret_cast<const void *>(c),&value,sizeof(value));if(o.depth_read)o.depth=value;}
#endif
            }
        }
#ifdef MHW_FG_SCENE_PROBE
        if(!returned||which==0)output_observations[static_cast<std::size_t>(entry-entries.data())]={};
#endif
    }}
    pending.fetch_sub(1,std::memory_order_release);
}
template<unsigned I>void WINAPI pointer_call(ID3D12GraphicsCommandList *list,void *value){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,void *)>(originals[I])(list,value);returned=true;}
    __finally{after(I,list,admitted,returned,reinterpret_cast<std::uintptr_t>(value));}
}
void WINAPI predicate_call(ID3D12GraphicsCommandList *list,ID3D12Resource *buffer,UINT64 offset,D3D12_PREDICATION_OP operation){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,ID3D12Resource *,UINT64,D3D12_PREDICATION_OP)>(originals[23])(list,buffer,offset,operation);returned=true;}
    __finally{after(23,list,admitted,returned,reinterpret_cast<std::uintptr_t>(buffer),offset,operation);}
}
void WINAPI heaps_call(ID3D12GraphicsCommandList *list,UINT count,ID3D12DescriptorHeap *const *heaps){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,ID3D12DescriptorHeap *const *)>(originals[3])(list,count,heaps);returned=true;}
    __finally{after(3,list,admitted,returned,count,reinterpret_cast<std::uintptr_t>(heaps));}
}
template<unsigned I>void WINAPI table_call(ID3D12GraphicsCommandList *list,UINT index,D3D12_GPU_DESCRIPTOR_HANDLE value){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,D3D12_GPU_DESCRIPTOR_HANDLE)>(originals[I])(list,index,value);returned=true;}
    __finally{after(I,list,admitted,returned,index,value.ptr);}
}
template<unsigned I>void WINAPI constant_call(ID3D12GraphicsCommandList *list,UINT index,UINT value,UINT offset){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,UINT,UINT)>(originals[I])(list,index,value,offset);returned=true;}
    __finally{after(I,list,admitted,returned,index,value,offset);}
}
template<unsigned I>void WINAPI constants_call(ID3D12GraphicsCommandList *list,UINT index,UINT count,const void *values,UINT offset){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,UINT,const void *,UINT)>(originals[I])(list,index,count,values,offset);returned=true;}
    __finally{after(I,list,admitted,returned,index,(std::uintptr_t(count)<<32)|offset,reinterpret_cast<std::uintptr_t>(values));}
}
template<unsigned I>void WINAPI address_call(ID3D12GraphicsCommandList *list,UINT index,D3D12_GPU_VIRTUAL_ADDRESS value){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,D3D12_GPU_VIRTUAL_ADDRESS)>(originals[I])(list,index,value);returned=true;}
    __finally{after(I,list,admitted,returned,index,value);}
}
void WINAPI topology_call(ID3D12GraphicsCommandList *list,D3D12_PRIMITIVE_TOPOLOGY topology){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,D3D12_PRIMITIVE_TOPOLOGY)>(originals[18])(list,topology);returned=true;}
    __finally{after(18,list,admitted,returned,topology);}
}
template<unsigned I,class T>void WINAPI raster_call(ID3D12GraphicsCommandList *list,UINT count,const T *values){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,const T *)>(originals[I])(list,count,values);returned=true;
        if constexpr(I==19)if(admitted&&viewport_listener){++viewport_notifications;viewport_listener(list,count,values,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}}
    __finally{after(I,list,admitted,returned,count,reinterpret_cast<std::uintptr_t>(values));}
}
void WINAPI vertices_call(ID3D12GraphicsCommandList *list,UINT first,UINT count,const D3D12_VERTEX_BUFFER_VIEW *values){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,UINT,const D3D12_VERTEX_BUFFER_VIEW *)>(originals[21])(list,first,count,values);returned=true;}
    __finally{after(21,list,admitted,returned,first,count,reinterpret_cast<std::uintptr_t>(values));}
}
void WINAPI outputs_call(ID3D12GraphicsCommandList *list,UINT count,const D3D12_CPU_DESCRIPTOR_HANDLE *values,BOOL contiguous,const D3D12_CPU_DESCRIPTOR_HANDLE *depth){
    const bool admitted=enter();bool returned=false;
    __try{reinterpret_cast<void(WINAPI *)(ID3D12GraphicsCommandList *,UINT,const D3D12_CPU_DESCRIPTOR_HANDLE *,BOOL,const D3D12_CPU_DESCRIPTOR_HANDLE *)>(originals[22])(list,count,values,contiguous,depth);returned=true;}
    __finally{after(22,list,admitted,returned,count,reinterpret_cast<std::uintptr_t>(values),reinterpret_cast<std::uintptr_t>(depth),static_cast<unsigned>(contiguous));}
}
void root_json(std::ostream &o,const RootBindings &r){
    o<<"{\"signature\":"<<r.signature<<",\"arguments\":[";bool comma=false;
    for(unsigned i=0;i<r.arguments.size();++i){const auto &a=r.arguments[i];if(a.kind==ArgumentKind::unset)continue;
        if(comma)o<<',';comma=true;o<<"{\"index\":"<<i<<",\"kind\":"<<static_cast<unsigned>(a.kind)<<",\"address\":"<<a.address<<",\"mask\":"<<a.mask<<",\"constants\":[";bool item=false;
        for(unsigned j=0;j<64;++j)if(a.mask&(std::uint64_t(1)<<j)){if(item)o<<',';item=true;o<<'['<<j<<','<<a.values[j]<<']';}o<<"]}";
    }o<<"]}";
}
}
bool install(ID3D12Device *device,Resolver resolve,std::string &reason,bool raster,ViewportListener listener,bool predicate,bool indexed){
    if(installed||!device||!resolve||(!raster&&(listener||predicate||indexed))||(indexed&&!predicate)){reason="bindings-install-contract";return false;}installed=true;viewport_listener=listener;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)))||
       FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)))||FAILED(list->Close())){
        reason="bindings-discovery-list";return false;}
    installed_hooks=indexed?max_hooks:predicate?predicate_hooks:raster?raster_hooks:hook_count;
    const std::array<void *,max_hooks> detours{
        reinterpret_cast<void *>(pointer_call<0>),reinterpret_cast<void *>(pointer_call<1>),reinterpret_cast<void *>(pointer_call<2>),reinterpret_cast<void *>(heaps_call),
        reinterpret_cast<void *>(pointer_call<4>),reinterpret_cast<void *>(pointer_call<5>),reinterpret_cast<void *>(table_call<6>),reinterpret_cast<void *>(table_call<7>),
        reinterpret_cast<void *>(constant_call<8>),reinterpret_cast<void *>(constant_call<9>),reinterpret_cast<void *>(constants_call<10>),reinterpret_cast<void *>(constants_call<11>),
        reinterpret_cast<void *>(address_call<12>),reinterpret_cast<void *>(address_call<13>),reinterpret_cast<void *>(address_call<14>),reinterpret_cast<void *>(address_call<15>),reinterpret_cast<void *>(address_call<16>),reinterpret_cast<void *>(address_call<17>),
        reinterpret_cast<void *>(topology_call),reinterpret_cast<void *>(raster_call<19,D3D12_VIEWPORT>),reinterpret_cast<void *>(raster_call<20,D3D12_RECT>),reinterpret_cast<void *>(vertices_call),reinterpret_cast<void *>(outputs_call),reinterpret_cast<void *>(predicate_call),reinterpret_cast<void *>(pointer_call<24>)};
    for(unsigned i=0;i<installed_hooks;++i){
        if(!resolve(reinterpret_cast<std::uintptr_t>(list.Get()),methods[i],targets[i])){reason="bindings-system-method-"+std::to_string(methods[i]);return false;}
        for(unsigned j=0;j<i;++j)if(targets[i]==targets[j]){reason="bindings-method-alias";return false;}
        const auto status=MH_CreateHook(reinterpret_cast<void *>(targets[i]),detours[i],&originals[i]);
        if(status!=MH_OK){reason="bindings-create-"+std::to_string(methods[i])+"-"+std::to_string(status);return false;}created[i]=true;
    }
    // MinHook's queue is shared with the native temporal activation owner.
    // Its hooks must remain pending until all original-code gates have passed.
    for(unsigned i=0;i<installed_hooks;++i){
        const auto status=MH_EnableHook(reinterpret_cast<void *>(targets[i]));
        if(status!=MH_OK){reason="bindings-enable-"+std::to_string(methods[i])+"-"+std::to_string(status);return false;}
        enabled[i]=true;
    }
    return true;
}
void begin() noexcept {accepting.store(true,std::memory_order_release);}
void reset(ID3D12GraphicsCommandList *list,std::uint64_t generation,ID3D12PipelineState *pipeline) noexcept {
    if(!accepting.load(std::memory_order_acquire))return;Lock lock;
    auto *e=find(list);if(!e){if(entry_count==list_limit){fail(1);return;}e=&entries[entry_count++];e->list=list;}
    e->state.reset(generation,reinterpret_cast<std::uintptr_t>(pipeline));e->raster={};e->raster.generation=generation;e->predicate={0,0,D3D12_PREDICATION_OP_EQUAL_ZERO,true};
#ifdef MHW_FG_SCENE_PROBE
    output_observations[static_cast<std::size_t>(e-entries.data())]={};
#endif
}
bool capture(unsigned i,std::uint64_t frame,ID3D12GraphicsCommandList *list,std::uintptr_t root,std::uintptr_t pipeline) noexcept {
    Lock lock;if(i>=sample_limit||samples[i].captured||!healthy()||!accepting.load())return false;
    const auto *e=find(list);if(!e||!e->state.ready()||e->state.compute.signature!=root||e->state.pipeline!=pipeline||!frame){fail(2);return false;}
    samples[i]={true,frame,reinterpret_cast<std::uintptr_t>(list),root,pipeline,e->state};return true;
}
bool sample(unsigned i,Bindings &s) noexcept {Lock lock;if(i>=sample_limit||!samples[i].captured)return false;s=samples[i].state;return true;}
bool matches_current(ID3D12GraphicsCommandList *list,const Bindings &state) noexcept {
    Lock lock;const auto *e=find(list);return healthy()&&e&&e->state==state;
}
bool current(ID3D12GraphicsCommandList *list,Bindings &state) noexcept {
    Lock lock;const auto *e=find(list);if(!healthy()||!e||!e->state.ready())return false;state=e->state;return true;
}
bool inspect(ID3D12GraphicsCommandList *list,Bindings &state) noexcept {
    Lock lock;const auto *e=find(list);if(!e)return false;state=e->state;return true;
}
bool raster_current(ID3D12GraphicsCommandList *list,RasterBindings &state) noexcept {
    Lock lock;const auto *e=find(list);if(installed_hooks<raster_hooks||!accepting.load()||!healthy()||!e||!e->state.known||!e->state.valid||!e->raster.ready())return false;state=e->raster;return true;
}
#ifdef MHW_FG_SCENE_PROBE
bool raster_inspect(ID3D12GraphicsCommandList *list,std::array<std::uint64_t,19> &values) noexcept {
    Lock lock;values={};const auto *e=find(list);if(!e)return false;
    const auto &r=e->raster;const auto &o=output_observations[static_cast<std::size_t>(e-entries.data())];
    const unsigned mask=(r.vertex_known?1U:0U)|(r.viewport_known?2U:0U)|(r.scissor_known?4U:0U)|
        (r.topology_known?8U:0U)|(r.index_known?16U:0U)|(r.output_known?32U:0U);
    values={1,installed_hooks>=raster_hooks,accepting.load(),healthy(),e->state.known,e->state.valid,
        e->state.generation,r.generation,mask,r.vertex_mask,o.seen,o.count,o.contiguous,
        o.rtv_argument,o.rtv_read,o.rtv,o.depth_argument,o.depth_read,o.depth};return true;
}
#endif
bool predicate_current(ID3D12GraphicsCommandList *list,Predicate &state) noexcept {
    Lock lock;const auto *e=find(list);if(installed_hooks<predicate_hooks||!accepting.load()||!healthy()||!e||!e->state.known||!e->state.valid||!e->predicate.known)return false;state=e->predicate;return true;
}
bool hooks_disabled() noexcept {Lock lock;for(const auto v:enabled)if(v)return false;return true;}
void stop() noexcept {accepting.store(false,std::memory_order_release);}
bool settled() noexcept {return pending.load(std::memory_order_acquire)==0;}
bool healthy() noexcept {return fault.load(std::memory_order_acquire)==0;}
bool finish() noexcept {
    if(!installed)return true;
    stop();const auto start=GetTickCount64();while(!settled()&&GetTickCount64()-start<2000)Sleep(1);drained=settled();bool ok=drained;
    for(unsigned i=0;i<installed_hooks;++i)if(created[i]){
        const auto status=MH_DisableHook(reinterpret_cast<void *>(targets[i]));
        if(status==MH_OK||status==MH_ERROR_DISABLED)enabled[i]=false;else ok=false;
    }
    return ok&&healthy()&&settled();
}
void write(std::ostream &o){Lock lock;bool disabled=true;for(const auto e:enabled)disabled=disabled&&!e;
    o<<"{\"schema\":1,\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()<<",\"hooks_disabled\":"<<disabled<<",\"drained\":"<<drained<<",\"list_count\":"<<entry_count<<",\"hook_count\":"<<installed_hooks
     <<",\"hook_control\":\"owned-targets-only\",\"viewport_listener\":"<<(viewport_listener!=nullptr)<<",\"viewport_notifications\":"<<viewport_notifications.load()<<",\"records\":[";bool comma=false;
    for(unsigned i=0;i<sample_limit;++i){const auto &v=samples[i];if(!v.captured)continue;if(comma)o<<',';comma=true;
        o<<"{\"receipt\":"<<i<<",\"frame\":"<<v.frame<<",\"command\":"<<v.list<<",\"generation\":"<<v.state.generation<<",\"ready\":"<<v.state.ready()<<",\"pipeline\":"<<v.state.pipeline
         <<",\"expected_pipeline\":"<<v.expected_pipeline<<",\"expected_root\":"<<v.expected_root<<",\"heaps\":[";
        for(unsigned j=0;j<v.state.heap_count;++j){if(j)o<<',';o<<v.state.heaps[j];}o<<"],\"compute\":";root_json(o,v.state.compute);o<<",\"graphics\":";root_json(o,v.state.graphics);o<<'}';
    }o<<"]}";
}
}
