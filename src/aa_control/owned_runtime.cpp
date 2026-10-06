#include "owned_runtime.hpp"
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <d3d12.h>
#include <atomic>
#include <ostream>
#include <cstring>

namespace mhw::aa_control::owned {
namespace {
namespace pb=projection_binding;namespace tb=temporal_bindings;
constexpr unsigned capacity=8;
struct Unit {std::uintptr_t vtable{};};
struct Resource {
    D3D12_RESOURCE_DESC desc{};
    std::uintptr_t identity{},device{};
    bool valid{};
};
struct Slot {
    OwnedDescriptor descriptor;
    Unit unit;
    OwnedPlan plan;
    PostQueue callback,after,restored;
    tb::Owner color;
    Resource resource;
    std::uintptr_t tail{};
    unsigned callbacks{},thread{},reason{},packets_before{},packets_after{};
    bool inserted{},removed{},callback_valid{},neutral{},returned{},complete{},published{};
};
std::array<Slot,capacity> slots;
std::uintptr_t image{},device{};
Publish publisher{};
std::atomic<unsigned> attached{};
thread_local unsigned current=capacity;
bool read(void *,std::uintptr_t at,std::span<std::byte> out){return pb::copy_memory(at,out.data(),static_cast<unsigned>(out.size()));}
const tb::Reader reader{nullptr,read};
bool queue(const PostQueue &anchor,PostQueue &q){
    PostQueue again;
    return read_post_queue(reader,image,anchor.manager,anchor.context,anchor.view,q)&&
        read_post_queue(reader,image,anchor.manager,anchor.context,anchor.view,again)&&again==q;
}
// Borrowed scalar identity is checked before invoking system COM methods. These
// temporary pins protect only the synchronous description query; there are no
// owned GPU commands or deferred references to release at this stage.
Resource describe(std::uintptr_t address) noexcept {
    Resource result;ID3D12Resource *held{};ID3D12Device *owner{};std::uintptr_t method{};
    if(!pb::system_method(address,0,method)||!pb::system_method(address,2,method)||
       !pb::system_method(address,7,method)||!pb::system_method(address,10,method))return result;
    __try {
        if(SUCCEEDED(reinterpret_cast<IUnknown *>(address)->QueryInterface(IID_PPV_ARGS(&held)))&&held){
            result.identity=reinterpret_cast<std::uintptr_t>(held);
            if(result.identity==address&&SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&owner)))&&owner){
                result.device=reinterpret_cast<std::uintptr_t>(owner);result.desc=held->GetDesc();
                const auto &d=result.desc;
                result.valid=result.device==device&&d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D&&
                    d.Width==2560&&d.Height==1440&&d.DepthOrArraySize==1&&d.MipLevels==1&&
                    d.SampleDesc.Count==1&&d.SampleDesc.Quality==0&&d.Format==DXGI_FORMAT_R11G11B10_FLOAT;
            }
        }
    } __finally {if(owner)owner->Release();if(held)held->Release();}
    return result;
}
bool writable(std::uintptr_t address,std::size_t bytes) noexcept {
    MEMORY_BASIC_INFORMATION m{};
    if(VirtualQuery(reinterpret_cast<void *>(address),&m,sizeof(m))!=sizeof(m)||m.State!=MEM_COMMIT||
       (m.Protect&(PAGE_GUARD|PAGE_NOACCESS))||address>UINTPTR_MAX-bytes)return false;
    const auto p=m.Protect&255;
    return (p==PAGE_READWRITE||p==PAGE_WRITECOPY)&&address>=reinterpret_cast<std::uintptr_t>(m.BaseAddress)&&
        address+bytes<=reinterpret_cast<std::uintptr_t>(m.BaseAddress)+m.RegionSize;
}
bool reject(Admission *a,const char *step) noexcept {if(a)a->step=step;return false;}
bool attach(Slot &s,Admission *a=nullptr) noexcept {
    const auto m=s.plan.before.manager;const auto &q=s.plan.before;const auto n=q.count;
    if(n!=4&&n!=5)return reject(a,"attach-layout");
    if(!writable(m+0x3c,4+(n+1)*8))return reject(a,"attach-unwritable");
    __try {
        auto *count=reinterpret_cast<volatile LONG *>(m+0x3c);
        auto *entries=reinterpret_cast<std::uintptr_t *>(m+0x40);
        const auto actual_count=*count;
        if(a)a->attach_count=static_cast<unsigned>(actual_count);
        if(actual_count!=static_cast<LONG>(n))return reject(a,"attach-count");
        const auto collecting=*reinterpret_cast<unsigned char *>(m+0x39);
        if(a)a->attach_collecting=collecting;
        if(collecting!=1)return reject(a,"attach-phase");
        for(unsigned i=0;i<n;++i){const auto entry=entries[i];if(a)a->attach_entries[i]=entry;
            if(entry!=q.nodes[i].descriptor)return reject(a,"attach-entry");}
        if(a)a->attach_entries[n]=entries[n];
        s.tail=entries[n];entries[n]=s.plan.descriptor;
        if(InterlockedCompareExchange(count,static_cast<LONG>(n+1),static_cast<LONG>(n))!=static_cast<LONG>(n)){
            if(entries[n]==s.plan.descriptor)entries[n]=s.tail;return reject(a,"attach-race");}
        s.inserted=true;++attached;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return reject(a,"attach-exception");}
}
bool detach(Slot &s) noexcept {
    if(!s.inserted)return true;
    const auto m=s.plan.before.manager;const auto n=s.plan.before.count;
    if((n!=4&&n!=5)||!writable(m+0x3c,4+(n+1)*8))return false;
    __try {
        auto *count=reinterpret_cast<volatile LONG *>(m+0x3c);
        auto *entries=reinterpret_cast<std::uintptr_t *>(m+0x40);
        if(*count!=static_cast<LONG>(n+1))return false;
        unsigned seen{};
        for(unsigned i=0;i<n+1;++i){
            unsigned k=n+1;
            if(entries[i]==s.plan.descriptor)k=n;
            else for(unsigned j=0;j<n;++j)if(entries[i]==s.plan.before.nodes[j].descriptor)k=j;
            if(k==n+1||(seen&(1U<<k)))return false;seen|=1U<<k;
        }
        for(unsigned i=0;i<n;++i)entries[i]=s.plan.before.nodes[i].descriptor;
        entries[n]=s.tail;InterlockedExchange(count,static_cast<LONG>(n));
        s.removed=true;--attached;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void callback(Unit *unit,std::uintptr_t context) noexcept {
    if(current>=capacity)return;
    auto &s=slots[current];++s.callbacks;
    if(s.callbacks!=1||unit!=&s.unit||context!=s.plan.before.context||GetCurrentThreadId()!=s.thread){s.reason=3;return;}
    tb::Owner again;
    s.callback_valid=queue(s.plan.before,s.callback)&&owned_callback_matches(s.plan,s.callback)&&
        tb::read_owner(reader,s.plan.color,s.color)&&tb::read_owner(reader,s.plan.color,again)&&s.color==again&&
        s.color.width==2560&&s.color.height==1440;
    if(!s.callback_valid){s.reason=4;return;}
    s.packets_before=s.callback.packet_count;
    // Off mode: intentionally leave the in-place color untouched. A normal
    // filter descriptor would advance ping-pong and require a copy; +21 avoids it.
    s.resource=describe(s.color.resource);
    PostQueue final;
    s.neutral=pb::read(context+0x2b80,s.packets_after)&&s.packets_after==s.packets_before&&
        tb::read_owner(reader,s.plan.color,again)&&again==s.color&&queue(s.plan.before,final)&&final==s.callback;
    if(!s.resource.valid||!s.neutral)s.reason=5;
    if(!s.reason&&publisher){
        s.published=publisher(current,s.callback,s.color);
        if(!s.published||!pb::read(context+0x2b80,s.packets_after)||s.packets_after!=s.packets_before+1)s.reason=7;
    }
}
// Only slot +60 is reached by the fully hash-gated native phase. Other slots
// stay null so this unit cannot be mistaken for a registered native layer.
const std::array<std::uintptr_t,13> vtable={0,0,0,0,0,0,0,0,0,0,0,0,reinterpret_cast<std::uintptr_t>(&callback)};
}
void configure(std::uintptr_t host,std::uintptr_t d,Publish p) noexcept {image=host;device=d;publisher=p;}
bool insert(unsigned i,const PostQueue &q,Admission *diagnostic) noexcept {
    if(i>=capacity)return reject(diagnostic,"insert-index");
    if(current!=capacity)return reject(diagnostic,"insert-active");
    auto &s=slots[i];if(s.inserted)return reject(diagnostic,"insert-used");
    s.unit.vtable=reinterpret_cast<std::uintptr_t>(vtable.data());
    s.descriptor.unit=reinterpret_cast<std::uintptr_t>(&s.unit);s.thread=GetCurrentThreadId();
    if(diagnostic)diagnostic->descriptor=reinterpret_cast<std::uintptr_t>(&s.descriptor);
    if(!plan_owned_pass(q,image,reinterpret_cast<std::uintptr_t>(&s.descriptor),s.descriptor.unit,s.unit.vtable,s.plan)){s.reason=1;return reject(diagnostic,"plan");}
    // No descriptor mutation yet. Validate the actual input owner twice before
    // admission. Querying its COM description in our no-write callback is safe
    // to reject without affecting the native color chain.
    tb::Owner local_a,local_b;auto &a=diagnostic?diagnostic->first_owner:local_a;auto &b=diagnostic?diagnostic->second_owner:local_b;
    s.reason=2;
    if(!tb::read_owner(reader,s.plan.color,a))return reject(diagnostic,"owner-first-read");
    if(!tb::read_owner(reader,s.plan.color,b))return reject(diagnostic,"owner-second-read");
    if(a!=b)return reject(diagnostic,"owner-changed");
    if(a.width!=2560||a.height!=1440)return reject(diagnostic,"owner-extent");
    PostQueue local_first,local_second;auto &first=diagnostic?diagnostic->first_queue:local_first;auto &second=diagnostic?diagnostic->second_queue:local_second;
    if(!read_post_queue(reader,image,q.manager,q.context,q.view,first))return reject(diagnostic,"queue-first-read");
    if(!read_post_queue(reader,image,q.manager,q.context,q.view,second))return reject(diagnostic,"queue-second-read");
    if(first!=second)return reject(diagnostic,"queue-changed");
    if(first!=q)return reject(diagnostic,"queue-anchor-changed");
    if(!attach(s,diagnostic))return false;
    s.reason=0;if(diagnostic)diagnostic->step="inserted";
    current=i;return true;
}
bool remove(unsigned i,bool returned) noexcept {
    if(i>=capacity)return false;auto &s=slots[i];s.returned=returned;
    const bool observed=returned&&queue(s.plan.before,s.after)&&owned_return_matches(s.plan,s.after);
    const bool removed=detach(s);current=capacity;
    const bool restored=removed&&queue(s.plan.before,s.restored)&&owned_restored_matches(s.plan,s.restored);
    s.complete=observed&&restored&&s.callbacks==1&&s.callback_valid&&s.neutral&&s.resource.valid&&!s.reason;
    if(!s.complete&&!s.reason)s.reason=6;
    return s.complete;
}
bool detached() noexcept{return attached.load()==0;}
bool recycle(unsigned i,Admission *a) noexcept {
    if(a){a->current_slot=current;a->attached_count=attached.load();}
    if(i>=capacity)return reject(a,"recycle-index");
    auto &s=slots[i];
    if(a){a->previous_inserted=s.inserted;a->previous_removed=s.removed;a->previous_complete=s.complete;}
    if(current!=capacity)return reject(a,"recycle-active");
    if(attached.load())return reject(a,"recycle-attached");
    if(s.inserted&&(!s.removed||!s.complete))return reject(a,"recycle-incomplete");
    s={};if(a)a->step="recycled";return true;
}
#ifdef MHW_AA_OWNED_FIXTURE
bool fixture_attach(const OwnedPlan &p,Admission *a) noexcept {slots[0]={};slots[0].plan=p;return attach(slots[0],a);}
bool fixture_detach() noexcept{return detach(slots[0]);}
#endif
ReturnSnapshot returned_snapshot(unsigned i) noexcept {
    if(i>=capacity)return {};const auto &s=slots[i];
    return {s.plan,s.callback,s.after,s.restored,s.reason,s.callbacks,
            s.inserted,s.removed,s.returned,s.complete,s.callback_valid,s.neutral,s.published};
}
void write_return(std::ostream &out,const ReturnSnapshot &s){
    out<<"{\"reason\":"<<s.reason<<",\"callbacks\":"<<s.callbacks<<",\"inserted\":"<<s.inserted<<",\"removed\":"<<s.removed
       <<",\"returned\":"<<s.returned<<",\"complete\":"<<s.complete<<",\"callback_valid\":"<<s.callback_valid<<",\"neutral\":"<<s.neutral
       <<",\"published\":"<<s.published<<",\"descriptor\":"<<s.plan.descriptor<<",\"unit\":"<<s.plan.unit<<",\"vtable\":"<<s.plan.vtable
       <<",\"color_owner\":"<<s.plan.color<<",\"before\":";
    write_post_queue(out,s.plan.before);out<<",\"assigned\":";write_post_queue(out,s.plan.assigned);
    out<<",\"callback\":";write_post_queue(out,s.callback);out<<",\"after\":";write_post_queue(out,s.after);
    out<<",\"restored\":";write_post_queue(out,s.restored);out<<'}';
}
void write_admission(std::ostream &out,const Admission &a,void (*emit)(std::ostream &,const PostQueue &)){
    out<<"{\"step\":\""<<a.step<<"\",\"current_slot\":"<<a.current_slot<<",\"attached_count\":"<<a.attached_count
       <<",\"previous_inserted\":"<<a.previous_inserted<<",\"previous_removed\":"<<a.previous_removed<<",\"previous_complete\":"<<a.previous_complete
       <<",\"descriptor\":"<<a.descriptor<<",\"attach_count\":"<<a.attach_count<<",\"attach_collecting\":"<<a.attach_collecting<<",\"attach_entries\":[";
    for(unsigned i=0;i<a.attach_entries.size();++i){if(i)out<<',';out<<a.attach_entries[i];}out<<"],\"owners\":[";
    for(const auto *o:{&a.first_owner,&a.second_owner}){if(o==&a.second_owner)out<<',';
        out<<"{\"object\":"<<o->object<<",\"type\":"<<o->type<<",\"primary\":"<<o->primary<<",\"secondary\":"<<o->secondary
           <<",\"backing\":"<<o->backing<<",\"resource\":"<<o->resource<<",\"id\":"<<o->id<<",\"kind\":"<<o->kind
           <<",\"flags\":"<<o->flags<<",\"width\":"<<o->width<<",\"height\":"<<o->height<<",\"format\":"<<o->format<<",\"gpu_format\":"<<o->gpu_format<<'}';}
    out<<"],\"first_queue\":";emit(out,a.first_queue);out<<",\"second_queue\":";emit(out,a.second_queue);out<<'}';
}
void write(std::ostream &out,unsigned i,void (*emit)(std::ostream &,const PostQueue &)){
    if(i>=capacity){out<<"null";return;}const auto &s=slots[i];const auto &d=s.resource.desc;
    out<<"{\"inserted\":"<<s.inserted<<",\"removed\":"<<s.removed<<",\"returned\":"<<s.returned
       <<",\"complete\":"<<s.complete<<",\"reason\":"<<s.reason<<",\"callbacks\":"<<s.callbacks
       <<",\"thread\":"<<s.thread<<",\"descriptor\":"<<s.plan.descriptor<<",\"unit\":"<<s.plan.unit
       <<",\"vtable\":"<<s.plan.vtable<<",\"phase\":15,\"order\":2,\"in_place\":true,\"color_owner\":"<<s.plan.color
       <<",\"callback_valid\":"<<s.callback_valid<<",\"neutral\":"<<s.neutral
       <<",\"callback_count\":"<<s.callback.count<<",\"callback_frame\":"<<s.callback.frame
       <<",\"after_count\":"<<s.after.count<<",\"restored_count\":"<<s.restored.count
       <<",\"packets_before\":"<<s.packets_before<<",\"packets_after\":"<<s.packets_after
       <<",\"published\":"<<s.published
       <<",\"resource\":{\"valid\":"<<s.resource.valid<<",\"identity\":"<<s.resource.identity<<",\"device\":"<<s.resource.device
       <<",\"dimension\":"<<d.Dimension<<",\"width\":"<<d.Width<<",\"height\":"<<d.Height
       <<",\"format\":"<<d.Format<<",\"flags\":"<<d.Flags<<",\"mips\":"<<d.MipLevels
       <<",\"array\":"<<d.DepthOrArraySize<<",\"samples\":"<<d.SampleDesc.Count<<"},\"callback\":";
    emit(out,s.callback);out<<",\"after\":";emit(out,s.after);out<<",\"restored\":";emit(out,s.restored);out<<'}';
}
}
