#include "output_route.hpp"
#include "../projection_binding/memory.hpp"
#include "../temporal_present/observer.hpp"
#include "../temporal_bindings/submission.hpp"
#include <MinHook.h>
#include <wrl/client.h>
#include <atomic>
#include <cstring>
#include <iomanip>
#include <ostream>
#include <intrin.h>
#include <stdexcept>

namespace mhw::scaled_temporal::output_route {
namespace {
namespace pb=projection_binding;namespace tp=temporal_present;namespace tb=temporal_bindings;
using Microsoft::WRL::ComPtr;
using CopyFn=void(WINAPI *)(ID3D12GraphicsCommandList *,const D3D12_TEXTURE_COPY_LOCATION *,UINT,UINT,UINT,const D3D12_TEXTURE_COPY_LOCATION *,const D3D12_BOX *);
tb::Config cfg;ComPtr<ID3D12Device> device;
struct Plan {bool published{};tb::Snapshot stamp;tb::Owner output;};
std::array<Plan,9> plans;
struct Pipeline {std::unique_ptr<Effect> effect;std::string signature;ComPtr<ID3D12RootSignature> root;};
std::array<Pipeline,3> pipelines;
ComPtr<ID3D12Resource> copy_source,copy_destination;
std::uintptr_t copy_packet{},copy_target{};void *copy_original{};
std::uintptr_t recording_state{};std::array<std::byte,64> copy_fields{};
ID3D12GraphicsCommandList *selected_list{};std::uint64_t generation{},selected_frame{};
std::array<unsigned,3> rt_state{};std::array<bool,3> rt_known{};
std::array<unsigned,2> copy_state{};std::array<bool,2> copy_known{};
std::atomic<unsigned> fault{},pending{};bool installed{},disabled{},claimed{},resolve_ok{},copied{},retired{};
unsigned prepared_draws{},replaced_draws{},copy_count{};std::string failure;
unsigned copy_event=UINT_MAX;std::array<unsigned,3> draw_events{UINT_MAX,UINT_MAX,UINT_MAX};
SRWLOCK mutex=SRWLOCK_INIT;
struct Lock {Lock(){AcquireSRWLockExclusive(&mutex);}~Lock(){ReleaseSRWLockExclusive(&mutex);}};
void fail(unsigned n){unsigned zero{};fault.compare_exchange_strong(zero,n);}
std::string signature(const EffectRecipe &r){
    if(r.vertex_shader.size()>262144||r.pixel_shader.size()>262144||r.native_vertices.size()>16||r.input_layout.size()>32)return {};
    std::string s=pb::bytes_sha256(r.vertex_shader.data(),static_cast<unsigned>(r.vertex_shader.size()))+pb::bytes_sha256(r.pixel_shader.data(),static_cast<unsigned>(r.pixel_shader.size()));
    s+=pb::bytes_sha256(&r.blend,sizeof(r.blend))+pb::bytes_sha256(&r.raster,sizeof(r.raster))+pb::bytes_sha256(&r.depth,sizeof(r.depth));
    s+=pb::bytes_sha256(r.native_vertices.data(),static_cast<unsigned>(r.native_vertices.size_bytes()));
    for(const auto &e:r.input_layout){if(!e.SemanticName)return {};s+=e.SemanticName;
        const std::array<unsigned,6> fields{e.SemanticIndex,unsigned(e.Format),e.InputSlot,e.AlignedByteOffset,unsigned(e.InputSlotClass),e.InstanceDataStepRate};s+=pb::bytes_sha256(fields.data(),sizeof(fields));}
    const std::array<unsigned,5> fields{unsigned(r.stage),r.geometry.render_width,r.geometry.render_height,r.geometry.output_width,r.geometry.output_height};
    return s+pb::bytes_sha256(fields.data(),sizeof(fields));
}
bool generation_matches(ID3D12GraphicsCommandList *list){
    tp::Bindings s;return list==selected_list&&tp::observer::inspect(list,s)&&s.known&&s.valid&&s.generation==generation;
}
bool copy_override(ID3D12GraphicsCommandList *list,const D3D12_TEXTURE_COPY_LOCATION *destination,UINT x,UINT y,UINT z,
                   const D3D12_TEXTURE_COPY_LOCATION *source,const D3D12_BOX *box,std::uintptr_t caller){
    Lock lock;if(!claimed||!resolve_ok||fault.load())return false;
    // This route owns one copy and three draws, not the textures' lifetime.
    // Close copy admission as soon as those commands are recorded. Native
    // copies may reuse the resources before polling/retirement removes hooks.
    if(copied&&replaced_draws==3)return false;
    D3D12_TEXTURE_COPY_LOCATION d{},s{};D3D12_BOX original{};
    if(!pb::copy_memory(reinterpret_cast<std::uintptr_t>(destination),&d,sizeof(d))||!pb::copy_memory(reinterpret_cast<std::uintptr_t>(source),&s,sizeof(s)))return false;
    if(d.pResource!=copy_destination.Get()||s.pResource!=copy_source.Get())return false;
    const auto &p=plans[receipt*3];const auto &r=p.stamp.rectangle;
    std::uintptr_t packet{},command{};std::array<std::byte,64> fields{};
    if(caller!=cfg.image+0x259caf0||copied||replaced_draws||!generation_matches(list)||x||y||z||
       !copy_known[0]||!copy_known[1]||(copy_state[0]&D3D12_RESOURCE_STATE_COPY_SOURCE)==0||copy_state[1]!=D3D12_RESOURCE_STATE_COPY_DEST||
       !pb::read(recording_state+0x158,packet)||packet!=copy_packet||!pb::read(recording_state+0x20,command)||command!=reinterpret_cast<std::uintptr_t>(list)||
       !pb::copy_memory(copy_packet+0x10,fields.data(),static_cast<unsigned>(fields.size()))||fields!=copy_fields||
       d.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||s.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||d.SubresourceIndex||s.SubresourceIndex||
       !box||!pb::copy_memory(reinterpret_cast<std::uintptr_t>(box),&original,sizeof(original))||original.left!=r[0]||original.top!=r[1]||
       original.right!=r[2]||original.bottom!=r[3]||original.front||original.back!=1){fail(4);return false;}
    const auto token=tb::submission::followup_begin(receipt,list,generation);if(token==UINT_MAX){fail(5);return false;}
    copy_event=token;
    const D3D12_BOX full{0,0,0,p.output.width,p.output.height,1};
    reinterpret_cast<CopyFn>(copy_original)(list,destination,0,0,0,source,&full);
    tb::submission::native_copy_end(token,true);copied=true;++copy_count;return true;
}
void copy_call(ID3D12GraphicsCommandList *list,const D3D12_TEXTURE_COPY_LOCATION *d,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION *s,const D3D12_BOX *box,std::uintptr_t caller){
    pending.fetch_add(1);bool replaced=false;
    __try{replaced=copy_override(list,d,x,y,z,s,box,caller);if(!replaced)reinterpret_cast<CopyFn>(copy_original)(list,d,x,y,z,s,box);}
    __finally{pending.fetch_sub(1);}
}
void WINAPI copy_hook(ID3D12GraphicsCommandList *list,const D3D12_TEXTURE_COPY_LOCATION *d,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION *s,const D3D12_BOX *box){copy_call(list,d,x,y,z,s,box,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
}
bool install(const tb::Config &c,std::string &reason){
    cfg=c;device=reinterpret_cast<ID3D12Device *>(c.device);
    if(!pb::verify_code(c.image,0x259c650,1214,"299ede85b4e2bbfb25f0ae2c20e003317efc2809c591eb46677ec154495daf88")||
       !pb::verify_code(c.image,0x259e470,512,"7db0a032642249004ebda64a7e8442407851cec8f25208e8966a63990ddade49")||
       !pb::verify_code(c.image,0x24e6e50,910,"d961028c0e34625cd23343327e3ba3ad401c4d439a5c653a9ca0e6893836bc04")){
        reason="scaled-output-code";return false;}
    ComPtr<ID3D12CommandAllocator> a;ComPtr<ID3D12GraphicsCommandList> l;
    if(FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&a)))||
       FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,a.Get(),nullptr,IID_PPV_ARGS(&l)))||FAILED(l->Close())||
       !pb::system_method(reinterpret_cast<std::uintptr_t>(l.Get()),16,copy_target)){reason="scaled-output-copy-method";return false;}
    const auto made=MH_CreateHook(reinterpret_cast<void *>(copy_target),reinterpret_cast<void *>(copy_hook),&copy_original);
    if(made!=MH_OK){reason="scaled-output-copy-create-"+std::to_string(made);return false;}
    if(MH_EnableHook(reinterpret_cast<void *>(copy_target))!=MH_OK){reason="scaled-output-copy-enable";return false;}installed=true;return true;
}
bool publish_draw(unsigned frame,unsigned stage,const tb::Snapshot &s,const tb::Owner &out) noexcept{
    Lock lock;if(frame>=3||stage>=3||plans[3*frame+stage].published){fail(1);return false;}
    plans[3*frame+stage]={true,s,out};return true;
}
bool publish_copy(unsigned frame,const tb::Snapshot &s,std::uintptr_t packet,ID3D12Resource *src,ID3D12Resource *dst) noexcept{
    if(frame!=receipt)return true;Lock lock;
    if(copy_packet||!src||!dst||src==dst||!s.frame){fail(2);return false;}
    if(!pb::copy_memory(packet+0x10,copy_fields.data(),static_cast<unsigned>(copy_fields.size()))){fail(2);return false;}
    copy_packet=packet;copy_source=src;copy_destination=dst;selected_frame=s.frame;return true;
}
bool ready(ID3D12GraphicsCommandList *list,std::uint64_t g,std::uint64_t frame,std::uintptr_t recording) noexcept{
    Lock lock;if(fault.load()||claimed||!copy_packet||frame!=selected_frame||!g)return false;
    for(unsigned i=0;i<3;++i){const auto &p=plans[3*receipt+i];if(!pipelines[i].effect||!p.published||p.stamp.frame!=frame||!p.output.resource)return false;}
    const auto &p=plans[receipt*3];const auto d=copy_source->GetDesc(),out=copy_destination->GetDesc();
    if(p.stamp.rectangle!=std::array<unsigned,4>{0,0,p.output.width/2,p.output.height/2}||
       d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||out.Dimension!=d.Dimension||d.Width!=p.output.width||d.Height!=p.output.height||
       out.Width!=d.Width||out.Height!=d.Height||d.Format!=DXGI_FORMAT_R11G11B10_FLOAT||out.Format!=d.Format||
       d.MipLevels!=1||out.MipLevels!=1||d.DepthOrArraySize!=1||out.DepthOrArraySize!=1||d.SampleDesc.Count!=1||out.SampleDesc.Count!=1)return false;
    if(!recording||recording>UINTPTR_MAX-0x160)return false;
    selected_list=list;generation=g;recording_state=recording;claimed=true;return true;
}
void resolved(bool ok) noexcept{Lock lock;resolve_ok=ok;if(!ok)fail(3);}
bool draw(unsigned index,const EffectRecipe &r,const EffectBindings &b,std::uintptr_t rtv,bool &replaced) noexcept{
    replaced=false;Lock lock;if(index>=9||fault.load())return false;const unsigned stage=index%3;
    try{
        const auto key=signature(r);if(key.empty())throw std::runtime_error("effect recipe bounds");auto &p=pipelines[stage];
        if(!p.effect){p.effect=std::make_unique<Effect>(device.Get(),r);p.signature=key;p.root=r.root;}
        else if(key!=p.signature||p.root.Get()!=r.root)throw std::runtime_error("effect recipe changed");
        ++prepared_draws;if(index/3!=receipt)return true;
        tp::observer::RasterBindings actual;tp::Bindings roots;
        if(!claimed||!resolve_ok||!copied||stage!=replaced_draws||!generation_matches(b.list)||!rt_known[stage]||rt_state[stage]!=D3D12_RESOURCE_STATE_RENDER_TARGET||
           !tp::observer::raster_current(b.list,actual)||!actual.output_known||actual.render_target!=rtv||
           !tp::observer::inspect(b.list,roots)||roots.pipeline!=reinterpret_cast<std::uintptr_t>(b.pipeline)||roots.graphics.signature!=reinterpret_cast<std::uintptr_t>(b.root)){
            fail(6);return false;}
        const auto token=tb::submission::followup_begin(receipt,b.list,generation);if(token==UINT_MAX){fail(5);return false;}
        draw_events[stage]=token;
        const bool ok=p.effect->record(b);const bool restored=tp::observer::matches_current(b.list,roots);
        tp::observer::RasterBindings after;const bool raster=tp::observer::raster_current(b.list,after)&&
            !std::memcmp(&actual.vertex_buffer,&after.vertex_buffer,sizeof(actual.vertex_buffer))&&!std::memcmp(&actual.viewport,&after.viewport,sizeof(actual.viewport))&&
            !std::memcmp(&actual.scissor,&after.scissor,sizeof(actual.scissor))&&actual.topology==after.topology&&actual.render_target==after.render_target&&after.output_known;
        tb::submission::native_copy_end(token,ok&&restored&&raster);replaced=ok;
        if(!ok||!restored||!raster){fail(7);return false;}++replaced_draws;return true;
    }catch(const std::exception &e){failure=e.what();fail(8);return false;}
}
void barriers(ID3D12GraphicsCommandList *list,unsigned n,const D3D12_RESOURCE_BARRIER *data) noexcept{
    Lock lock;if(!claimed||list!=selected_list||!generation_matches(list)||fault.load())return;
    if(n>4096){fail(9);return;}for(unsigned j=0;j<n;++j){D3D12_RESOURCE_BARRIER b{};
        if(!pb::copy_memory(reinterpret_cast<std::uintptr_t>(data+j),&b,sizeof(b))){fail(9);return;}
        if(b.Type!=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)continue;
        for(unsigned i=0;i<2;++i)if(b.Transition.pResource==(i?copy_destination.Get():copy_source.Get())){
            copy_known[i]=b.Flags==D3D12_RESOURCE_BARRIER_FLAG_NONE&&(b.Transition.Subresource==0||b.Transition.Subresource==UINT_MAX);
            copy_state[i]=b.Transition.StateAfter;}
        for(unsigned stage=0;stage<3;++stage)if(reinterpret_cast<std::uintptr_t>(b.Transition.pResource)==plans[3*receipt+stage].output.resource){
            if(b.Flags!=D3D12_RESOURCE_BARRIER_FLAG_NONE||(b.Transition.Subresource!=0&&b.Transition.Subresource!=UINT_MAX)){rt_known[stage]=false;continue;}
            rt_state[stage]=b.Transition.StateAfter;rt_known[stage]=true;}
    }
}
bool healthy() noexcept{return !fault.load();}
bool complete() noexcept{Lock lock;return healthy()&&claimed&&resolve_ok&&copied&&replaced_draws==3;}
bool finish(ID3D12Fence *fence) noexcept{
    if(installed){const auto s=MH_DisableHook(reinterpret_cast<void *>(copy_target));disabled=s==MH_OK||s==MH_ERROR_DISABLED;}else disabled=true;
    if(pending.load())return false;Lock lock;
    try{if(!fence||fault.load()||!resolve_ok||replaced_draws!=3||!copied)return false;
        for(auto &p:pipelines)p.effect->retire(fence,1,true);retired=true;copy_source.Reset();copy_destination.Reset();return disabled;
    }catch(const std::exception &e){failure=e.what();fail(10);return false;}
}
void write(std::ostream &o){Lock lock;o<<"{\"schema\":1,\"receipt\":"<<receipt<<",\"fault\":"<<fault.load()<<",\"pending\":"<<pending.load()
    <<",\"prepared_draws\":"<<prepared_draws<<",\"replaced_draws\":"<<replaced_draws<<",\"copy_count\":"<<copy_count<<",\"claimed\":"<<claimed
    <<",\"resolved\":"<<resolve_ok<<",\"retired\":"<<retired<<",\"hooks_disabled\":"<<disabled<<",\"command\":"<<reinterpret_cast<std::uintptr_t>(selected_list)
    <<",\"generation\":"<<generation<<",\"frame\":"<<selected_frame<<",\"copy_packet\":"<<copy_packet<<",\"copy_event\":"<<copy_event
    <<",\"draw_events\":["<<draw_events[0]<<','<<draw_events[1]<<','<<draw_events[2]<<"],\"failure\":"<<std::quoted(failure)<<'}';}
#ifdef MHW_SCALED_ROUTE_FIXTURE
void fixture_start(ID3D12Device *d,void *copy){device=d;cfg.device=reinterpret_cast<std::uintptr_t>(d);copy_original=copy;}
bool fixture_copy(ID3D12GraphicsCommandList *list,ID3D12Resource *src,ID3D12Resource *dst,const D3D12_BOX &box,bool caller){
    D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=src;a.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;b=a;b.pResource=dst;
    return copy_override(list,&b,0,0,0,&a,&box,caller?0x259caf0:0);
}
#endif
}
