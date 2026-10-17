// Exercise the actual continuous CPU callbacks against owned mock renderer
// memory; no process inspection, game code, or SDK initialization is used.
#include "../src/continuous_dlaa/runtime.cpp"
#include <iostream>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
namespace tb=mhw::temporal_bindings;
namespace cd=mhw::continuous_dlaa;
std::string mode;
void check(bool v,const char *s){if(!v){std::cerr<<s<<'\n';std::exit(1);}}
template<class T>void put(std::uintptr_t p,T v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
template<class T>T get(std::uintptr_t p){T v;std::memcpy(&v,reinterpret_cast<void *>(p),sizeof(v));return v;}
struct Block {std::uintptr_t p;explicit Block(std::size_t n):p(reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))){check(p!=0,"memory");}~Block(){VirtualFree(reinterpret_cast<void *>(p),0,MEM_RELEASE);}};
#include "temporal_jitter_fixture.hpp"
namespace mhw::temporal_bindings::jitter {
void stop() noexcept {cd::stop();}
void dispatch(NativeFrame *f){cd::native(4,f);}
}
void exception_call(tb::NativeFrame *f){
    bool caught=false;__try{cd::native(4,f);}__except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){caught=true;}
    check(caught,"expected native exception forwarded");
}
void queue_fixture(){
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D12Debug> debug;check(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))),"debug");debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> adapter;ComPtr<ID3D12Device> device;
    check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))&&SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"WARP device");
    cd::cfg.device=reinterpret_cast<std::uintptr_t>(device.Get());cd::accepting=true;
    check(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&cd::fence))),"private fence");
    std::array<ComPtr<ID3D12CommandQueue>,2> queues;std::array<ComPtr<ID3D12CommandAllocator>,2> allocators;std::array<ComPtr<ID3D12GraphicsCommandList>,2> lists;
    D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    for(unsigned i=0;i<2;++i){check(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queues[i])))&&SUCCEEDED(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocators[i])))&&SUCCEEDED(device->CreateCommandList(0,q.Type,allocators[i].Get(),nullptr,IID_PPV_ARGS(&lists[i])))&&SUCCEEDED(lists[i]->Close()),"queue/list pool");}
    auto table=*reinterpret_cast<void ***>(lists[0].Get());cd::originals[5]=table[10];cd::originals[6]=table[9];cd::originals[7]=(*reinterpret_cast<void ***>(queues[0].Get()))[10];
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=256;desc.Height=1;
    desc.DepthOrArraySize=desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> resource;
    check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&resource))),"owned queue fixture resource");
    const cd::ResourcePins pins{resource,resource,resource,resource};
    if(mode=="released-lists"||mode=="held-lists"){
        // Lists the creator releases are pruned from the registry (and the
        // observer); lists it still holds are never evicted (fault 15).
        const bool held=mode=="held-lists";std::vector<ComPtr<ID3D12GraphicsCommandList>> kept;
        for(unsigned n=0;n<300;++n){ComPtr<ID3D12GraphicsCommandList> l;
            check(SUCCEEDED(device->CreateCommandList(0,q.Type,allocators[0].Get(),nullptr,IID_PPV_ARGS(&l)))&&SUCCEEDED(l->Close()),"transient list");
            check(SUCCEEDED(cd::reset_hook(l.Get(),allocators[0].Get(),nullptr))&&SUCCEEDED(cd::close_hook(l.Get())),"transient Reset and Close");
            if(held){kept.push_back(l);if(cd::fault.load())break;}
        }
        if(held){check(cd::fault==15&&!cd::accepting&&kept.size()==129&&cd::lists_pruned==0,"held lists are never pruned");
            std::cout<<"Held lists fail closed at 129\n";return;}
        // Full at 128: the 129th and the 257th prune every released list.
        check(cd::healthy()&&cd::lists_pruned==256&&cd::live_lists()==44&&cd::list_count==44&&cd::lists_peak==128,"released lists pruned");
        check(cd::recording_pins.live()==0&&cd::lease.list==nullptr,"nothing retained for pruned lists");
        std::cout<<"Released lists pruned: "<<cd::lists_pruned<<"\n";return;
    }
    if(mode=="pending-retirement"){
        ComPtr<ID3D12Fence> gate;check(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate))),"gate");
        auto *list=lists[0].Get();check(SUCCEEDED(cd::reset_hook(list,allocators[0].Get(),nullptr)),"initial Reset");
        auto *entry=cd::find(list);check(cd::recording_pins.retain(list,entry->generation,pins),"pending ownership");
        cd::lease={list,entry->generation,1,true,true};entry->injected_generation=entry->generation;
        check(SUCCEEDED(cd::close_hook(list))&&SUCCEEDED(queues[0]->Wait(gate.Get(),1)),"hold GPU before recording");
        ID3D12CommandList *batch[]{list};cd::execute_hook(queues[0].Get(),1,batch);
        // Reset can use a different allocator while the old one is still busy.
        check(SUCCEEDED(cd::reset_hook(list,allocators[1].Get(),nullptr)),"Reset before GPU completion");
        check(cd::fence->GetCompletedValue()==0&&cd::recording_pins.live()==1,"pending GPU reference survives Reset");
        check(SUCCEEDED(gate->Signal(1)),"release queued work");
        const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(event&&SUCCEEDED(cd::fence->SetEventOnCompletion(1,event))&&WaitForSingleObject(event,5000)==WAIT_OBJECT_0,"pending completion");CloseHandle(event);
        cd::recording_pins.collect(cd::fence->GetCompletedValue());
        check(cd::recording_pins.live()==0&&cd::recording_pins.released()==1&&cd::healthy(),"release only after both receipts");
        check(SUCCEEDED(list->Close()),"fixture close");std::cout<<"Pending GPU/Reset retirement passed\n";return;
    }
    const unsigned count=mode=="queues"?1000:1;
    for(unsigned frame=0;frame<count;++frame){const auto i=frame%2;auto *list=lists[i].Get();
        check(SUCCEEDED(allocators[i]->Reset())&&SUCCEEDED(cd::reset_hook(list,allocators[i].Get(),nullptr)),"tracked Reset");
        auto *entry=cd::find(list);check(entry&&entry->generation&&!entry->closed,"known open generation");
        check(cd::recording_pins.retain(list,entry->generation,pins),"recording ownership before commands");
        cd::lease={list,entry->generation,frame+1,true,true};entry->injected_generation=entry->generation;
        if(mode=="early-reset"){
            check(SUCCEEDED(list->Close()),"discard close");check(SUCCEEDED(cd::reset_hook(list,allocators[i].Get(),nullptr)),"native Reset forwards");
            check(cd::fault==16&&!cd::accepting,"early reset disables admission");return;}
        check(SUCCEEDED(cd::close_hook(list)),"tracked Close");ID3D12CommandList *batch[]{list};cd::execute_hook(queues[i].Get(),1,batch);
        check(cd::signal_value==frame+1&&!cd::lease.list&&entry->signal==frame+1&&entry->submitted,"private submission receipt");
        const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(event!=nullptr&&SUCCEEDED(cd::fence->SetEventOnCompletion(frame+1,event))&&WaitForSingleObject(event,5000)==WAIT_OBJECT_0,"queue completion");CloseHandle(event);
        if(mode=="duplicate"){
            cd::execute_hook(queues[i].Get(),1,batch);check(cd::fault==18&&!cd::accepting&&cd::signal_value==1,"duplicate generation rejected");
            check(SUCCEEDED(queues[i]->Signal(cd::fence.Get(),2)),"duplicate native drain");
            const auto drain=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(SUCCEEDED(cd::fence->SetEventOnCompletion(2,drain))&&WaitForSingleObject(drain,5000)==WAIT_OBJECT_0,"drain");CloseHandle(drain);
            check(SUCCEEDED(allocators[i]->Reset())&&SUCCEEDED(cd::reset_hook(list,allocators[i].Get(),nullptr)),"post-fault Reset forwards");
            std::ostringstream status;cd::write(status);check(cd::recording_pins.live()==1&&cd::recording_pins.released()==0,"fault freezes retirement even after Reset and fence");
            check(SUCCEEDED(list->Close()),"fault fixture close");return;}
    }
    check(cd::healthy()&&cd::list_count==2&&cd::pending.zero(),"bounded registry after 1000 submissions");
    ComPtr<ID3D12InfoQueue> messages;check(SUCCEEDED(device.As(&messages)),"validation messages");
    for(UINT64 i=0;i<messages->GetNumStoredMessagesAllowedByRetrievalFilter();++i){SIZE_T size{};messages->GetMessage(i,nullptr,&size);std::vector<std::byte> bytes(size);auto *m=reinterpret_cast<D3D12_MESSAGE *>(bytes.data());messages->GetMessage(i,m,&size);check(m->Severity>D3D12_MESSAGE_SEVERITY_WARNING,"clean queue validation");}
    std::cout<<"Continuous submission fixture passed: 1000 generations, two queues, bounded registry\n";
}
void request_fixture(){
    Block packet(128);put(packet.p,6U);put(packet.p+0x10,std::uintptr_t{1234});
    put(packet.p+0x18,160U);put(packet.p+0x1c,90U);put(packet.p+0x20,1U);
    cd::accepting=true;
    const auto stage=[&](std::uint64_t frame,std::uintptr_t object){
        cd::producer={};auto &t=cd::producer;t.source.frame=frame;t.source.epoch=frame+100;
        t.source.view=100;t.source.context=200;t.source.layer=300;t.source.bank=1;t.source.async=1;
        t.depth.texture.resource=400;t.projection.history.microseconds=frame*10000;
        t.packet={object,1234,0,6,160,90,1,0};};
    const auto publish=[&](std::uint64_t frame,std::uintptr_t object){stage(frame,object);cd::Lock lock;cd::publish_taa();};
    tb::NativeFrame f{};f.call.gpr[3]=packet.p;f.call.caller=123456;
    publish(5,packet.p);check(cd::requests[5].live&&cd::sequence==1,"first publication");
    if(mode=="requests-consumed"||mode=="requests-changed"){
        if(mode=="requests-changed")put(packet.p+0x18,159U);
        check(!cd::consumer_before(f,false),"owned consumer has no command list");
        check(!cd::requests[5].live&&cd::request_telemetry.consumers[0].matched==1,"matching lookup retires original slot");
        check(cd::request_telemetry.consumers[0].changed==(mode=="requests-changed"?1U:0U),"packet comparison recorded");
        publish(37,packet.p+64);check(cd::healthy()&&cd::requests[5].source.frame==37,"retired slot reusable");
    }else if(mode=="requests-unmatched"){
        f.call.gpr[3]=packet.p+64;check(!cd::consumer_before(f,false),"unknown consumer rejected");
        check(cd::requests[5].live&&cd::request_telemetry.consumers[0].unmatched==1,"unmatched does not retire other request");
    }else if(mode=="requests-reentrant"){
        cd::current_kind=1;check(!cd::consumer_before(f,false),"reentrant consumer rejected");
        check(cd::fault==9&&cd::request_telemetry.consumers[0].reentrant==1,"reentrant fault unchanged");
    }else if(mode=="requests-depth"){
        auto &d=cd::depths[0];d.live=true;d.object=packet.p;d.sequence=2;d.stamp.frame=5;d.stamp.epoch=105;
        d.stamp.context=200;d.stamp.view=100;cd::publishing=1;
        check(!cd::consumer_before(f,true)&&d.live,"depth blocked while publishing");
        check(cd::request_telemetry.consumers[1].first_blocked.matches==1,"depth match snapshot");
        cd::publishing=0;f.call.gpr[3]=packet.p+64;check(!cd::consumer_before(f,true),"unmatched depth");
    }else {
        if(mode=="requests-blocked"){
            cd::publishing=2;
            std::thread worker([&]{for(unsigned i=0;i<10000;++i)check(!cd::consumer_before(f,false),"publishing bypass");});worker.join();
            cd::publishing=0;check(cd::requests[5].live&&cd::requests[5].bypasses==10000,"blocked consumer leaves slot live");
            const auto &c=cd::request_telemetry.consumers[0];check(c.blocked==10000&&c.first_blocked.ordinal==1&&c.last_blocked.ordinal==10000,"bounded first/latest bypasses");
        }else if(mode=="requests-multiple"){
            publish(6,packet.p);check(!cd::consumer_before(f,false),"multiple lookup has no command list");
            check(cd::requests[5].live&&!cd::requests[6].live,"existing newest-match retirement preserved");
            check(cd::request_telemetry.consumers[0].multiple==1&&cd::request_telemetry.consumers[0].first_multiple.matches==2,"multiple matches recorded");
        }
        const auto before_sequence=cd::sequence;publish(mode=="requests-same-frame"?5:37,packet.p+64);
        check(cd::fault==6&&!cd::accepting&&cd::sequence==before_sequence,"collision still fails closed without publication");
        const auto &c=cd::request_telemetry.first_collision;
        check(c.old_request.frame==5&&c.new_request.frame==(mode=="requests-same-frame"?5U:37U)&&c.slot==5,"old/new collision frames");
        check(cd::requests[5].packet.object==packet.p&&c.new_request.sequence==0,"old slot preserved; incoming unpublished");
        if(mode=="requests-blocked")check(c.old_request.bypasses==10000&&c.old_request.last_bypass_publishers==2,"collision retains slot-specific bypass evidence");
    }
    const auto &m=cd::request_telemetry;
    check(m.publication_attempts==m.published+m.collisions,"publication partition");
    for(const auto &c:m.consumers)check(c.calls==c.blocked+c.unmatched+c.matched+c.reentrant,"consumer partition");
    std::ostringstream out;m.write(out);check(out.str().size()<16384,"bounded serialized metadata");
    std::cout<<out.str()<<'\n';
}
unsigned forwarded_resolves{};
unsigned forwarded_consumers{};
HANDLE forward_enter{},forward_leave{};
std::uintptr_t expected_forward_packet{};
void WINAPI forward_consumer(std::uintptr_t,std::uintptr_t,std::uintptr_t packet){
    ++forwarded_consumers;check(packet==expected_forward_packet,"native consumer packet preserved");
    if(mode=="forwarded-overlap"||mode=="forwarded-early-reuse"){
        check(cd::requests[5].live&&cd::requests[5].forwarding,"request remains reserved inside native callback");
        check(cd::forwarded_telemetry.pending==1,"completion not fabricated before native return");
        SetEvent(forward_enter);check(WaitForSingleObject(forward_leave,5000)==WAIT_OBJECT_0,"release native consumer");
    }
    if(mode=="forwarded-stop")cd::stop();
    if(mode=="forwarded-exception")RaiseException(0xe0424242,0,0,nullptr);
    if(mode=="forwarded-changed-after")put(packet+0x18,159U);
    if(mode=="forwarded-stale-token"){
        cd::Lock lock;auto &t=cd::requests[5];++t.sequence;t.packet.object+=64;t.forwarding=false;
    }
    if(mode=="forwarded-reentrant"&&forwarded_consumers==1){
        tb::NativeFrame f{};f.call.gpr[3]=packet;cd::native(1,&f);
    }
}
void forwarded_exception_call(tb::NativeFrame *f){
    bool caught=false;__try{cd::native(1,f);}__except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){caught=true;}
    check(caught,"native consumer exception propagated");
}
void forwarded_request_fixture(){
    Block packet(128);expected_forward_packet=packet.p;
    for(unsigned offset:{0U,64U}){
        put(packet.p+offset,6U);put(packet.p+offset+0x10,std::uintptr_t{1234});
        put(packet.p+offset+0x18,160U);put(packet.p+offset+0x1c,90U);put(packet.p+offset+0x20,1U);
    }
    cd::runtime.width=2560;cd::runtime.height=1440;cd::accepting=true;
    cd::originals[1]=reinterpret_cast<void *>(&forward_consumer);
    const auto publish=[&](std::uint64_t frame,std::uintptr_t object){
        cd::Lock lock;cd::producer={};auto &t=cd::producer;
        t.source.frame=frame;t.source.epoch=frame+100;t.source.view=100;t.source.context=200;t.source.layer=300;
        t.source.bank=t.source.async=1;t.source.state=1234;t.source.rectangle={0,0,2560,1440};
        t.depth.texture.resource=400;t.projection.history.microseconds=frame*10000;
        t.packet={object,1234,0,6,160,90,1,0};cd::publish_taa();
    };
    publish(5,packet.p);cd::publishing=1;
    if(mode=="forwarded-changed-before")put(packet.p+0x18,159U);
    if(mode=="forwarded-ambiguous")publish(6,packet.p);
    if(mode=="forwarded-unmatched")expected_forward_packet+=64;
    tb::NativeFrame f{};f.call.gpr[3]=expected_forward_packet;f.call.caller=123456;
    if(mode=="forwarded-overlap"||mode=="forwarded-early-reuse"){
        forward_enter=CreateEventW(nullptr,FALSE,FALSE,nullptr);forward_leave=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        check(forward_enter&&forward_leave,"owned overlap events");
        std::thread worker([&]{cd::native(1,&f);});
        check(WaitForSingleObject(forward_enter,5000)==WAIT_OBJECT_0,"consumer forwarding while producer runs");
        publish(mode=="forwarded-overlap"?6:37,packet.p+64);
        if(mode=="forwarded-early-reuse")check(cd::fault==6&&cd::requests[5].source.frame==5,"in-flight request cannot be overwritten");
        SetEvent(forward_leave);worker.join();CloseHandle(forward_enter);CloseHandle(forward_leave);
    }else if(mode=="forwarded-exception")forwarded_exception_call(&f);
    else cd::native(1,&f);
    cd::publishing=0;
    check(cd::pending.zero()&&!cd::native_forwarding&&!cd::forwarded_telemetry.pending,"native return/SEH tracking drained");
    const bool untracked=mode=="forwarded-changed-before"||mode=="forwarded-ambiguous"||mode=="forwarded-unmatched";
    const bool failed=mode=="forwarded-exception"||mode=="forwarded-changed-after"||mode=="forwarded-stale-token";
    auto &m=cd::forwarded_telemetry;
    if(untracked){check(m.started==0&&cd::requests[5].live,"unproven request stays live");publish(37,packet.p+64);check(cd::fault==6,"unproven retirement cannot hide collision");}
    else if(failed){check(m.failed==1&&!m.retired&&cd::requests[5].live,"failed native completion preserves request");
        check(cd::fault==(mode=="forwarded-exception"?10U:32U),"native completion fault");
        if(mode=="forwarded-stale-token")check(cd::requests[5].packet.object==packet.p+64,"stale token does not erase new identity");}
    else {check(m.retired==1&&!m.failed&&!cd::requests[5].live,"proven native consumer retires exact request");
        if(mode=="forwarded-wrap"||mode=="forwarded-overlap"){
            publish(37,packet.p+64);check(cd::healthy()&&cd::requests[5].source.frame==37,"32-frame slot reuse succeeds after native consumption");}
        if(mode=="forwarded-stop")check(!cd::accepting&&cd::healthy(),"explicit stop still completes request bookkeeping");
        if(mode=="forwarded-reentrant")check(cd::fault==9,"nested consumer fails closed");}
    check(forwarded_consumers==(mode=="forwarded-reentrant"?2U:1U),"native consumer forwarded exactly once per call");
    check(!cd::frames&&!cd::depth_count&&!cd::resolve_telemetry.claimed,"forward retirement injects no GPU commands");
    std::cout<<"{\"fault\":"<<cd::fault.load()<<",\"native_calls\":"<<forwarded_consumers<<",\"requests\":";
    cd::request_telemetry.write(std::cout);std::cout<<",\"forwarded_requests\":";m.write(std::cout);std::cout<<"}\n";
}
void WINAPI forward_resolve(ID3D12GraphicsCommandList *list,UINT x,UINT y,UINT z){
    check(list==reinterpret_cast<ID3D12GraphicsCommandList *>(0x1000)&&x==160&&y==90&&z==1,"original dispatch arguments preserved");
    ++forwarded_resolves;
}
void resolve_forward_fixture(){
    auto *list=reinterpret_cast<ID3D12GraphicsCommandList *>(0x1000);
    cd::originals[9]=reinterpret_cast<void *>(&forward_resolve);cd::current_list=list;cd::cfg.image=0x10000000;
    const auto caller=cd::cfg.image+0x259cfca;
    cd::accepting=true;cd::current_kind=0;cd::dispatch_call(list,160,90,1,caller);
    cd::current_kind=1;cd::injected=true;cd::dispatch_call(list,160,90,1,caller);cd::injected=false;
    cd::accepting=false;cd::dispatch_call(list,160,90,1,caller);
    cd::accepting=true;cd::dispatch_call(list,160,90,1,caller+1);
    cd::dispatch_count=0;cd::dispatch_call(list,160,90,1,caller); // missing owned source metadata
    check(forwarded_resolves==5,"unrelated/stopped/unqualified dispatches forward once");
    check(cd::resolve_telemetry.candidates==2&&cd::resolve_telemetry.unclaimed_native==2&&
          !cd::resolve_telemetry.pending&&!cd::resolve_telemetry.claimed&&!cd::frames,"no false bypass/evaluation receipts");
    check(cd::last_skip==2&&cd::healthy(),"existing invalid-owner admission stays closed");
    std::cout<<"Actual dispatch callback preserves unclaimed work and reports it separately\n";
}
#include "continuous_unfiltered_fixture.hpp"
#include "continuous_state_fixture.hpp"
#include "continuous_native_state_fixture.hpp"
#include "continuous_native_admission_fixture.hpp"
#include "continuous_depth_eligibility_fixture.hpp"
unsigned __declspec(noinline) shutdown_detour(){return 99;}
void shutdown_fixture(){
    Block code(4096);const std::array<unsigned char,8> original{0xb8,42,0,0,0,0xc3,0x90,0x90};
    std::memcpy(reinterpret_cast<void *>(code.p),original.data(),original.size());DWORD old{};
    check(VirtualProtect(reinterpret_cast<void *>(code.p),4096,PAGE_EXECUTE_READ,&old)!=0,"owned shutdown stub");
    check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(code.p),4096)!=0,"flush shutdown stub");
    check(MH_Initialize()==MH_OK,"shutdown MinHook");void *trampoline{};
    check(MH_CreateHook(reinterpret_cast<void *>(code.p),reinterpret_cast<void *>(shutdown_detour),&trampoline)==MH_OK&&
          MH_EnableHook(reinterpret_cast<void *>(code.p))==MH_OK,"install owned shutdown hook");
    check(reinterpret_cast<unsigned(*)()>(code.p)()==99,"owned detour active");
    // The runtime's own game hooks (OwnedHook; MinHook without AL).
    const auto own=[](unsigned i,std::uintptr_t target){
        cd::targets[i]=target;cd::enabled[i]=true;auto &h=cd::game_hooks[i];
        h.target=reinterpret_cast<void *>(target);h.minhook=true;h.live=true;
    };
    own(0,code.p);
    const bool partial=mode=="shutdown-partial";
    if(partial)own(1,code.p+32); // Deliberately unregistered target.
    cd::accepting=true;cd::finish();
    check(!cd::enabled[0]&&cd::enabled[1]==partial&&cd::fault==(partial?25U:0U),"shutdown records only successful removals");
    check(std::memcmp(reinterpret_cast<void *>(code.p),original.data(),original.size())==0&&
          reinterpret_cast<unsigned(*)()>(code.p)()==42,"native bytes and behavior restored");
    check(MH_Uninitialize()==MH_OK,"shutdown fixture cleanup");
    std::cout<<"Shutdown removal accounting and actual hook restoration passed: "<<mode<<'\n';
}
int main(int argc,char **argv){
    mode=argc==2?argv[1]:"continuous";
    if(mode.starts_with("shutdown-")){shutdown_fixture();return 0;}
    if(mode=="state-trace"){state_trace_fixture();return 0;}
    if(mode=="native-state-trace"){native_state_fixture();return 0;}
    if(mode=="native-state-admission"){native_admission_fixture();return 0;}
    if(mode=="depth-eligibility"||mode=="depth-destination"){depth_eligibility_fixture();return 0;}
    if(mode=="unfiltered-copy"){unfiltered_fixture();return 0;}
    if(mode.starts_with("forwarded-")){forwarded_request_fixture();return 0;}
    if(mode=="resolve-forward"){resolve_forward_fixture();return 0;}
    if(mode.starts_with("requests-")){request_fixture();return 0;}
    if(mode=="queues"||mode=="early-reset"||mode=="duplicate"||mode=="pending-retirement"||mode=="released-lists"||mode=="held-lists"){queue_fixture();return 0;}
    Block image(0x55d3000),renderer(0x20000),backend(0x43000),view(0x1000);
    cd::cfg={image.p,renderer.p,backend.p,1234};
    put(image.p+0x51c4480,renderer.p);put(renderer.p+0x78,backend.p);put(backend.p+0x18,std::uintptr_t{1234});put(renderer.p+0x1f450,view.p);
    JitterFixture fixture(cd::cfg,2560,1440);cd::originals[4]=reinterpret_cast<void *>(JitterFixture::forward);cd::accepting=true;
    if(mode=="reconstruction"){
        cd::reconstruction.suspend();cd::reconstruction.suspend();fixture.stage(5);auto f=fixture.frame();cd::native(4,&f);
        check(cd::projections[5].valid&&cd::projections[5].source.projection.jitter_bits[0]==0&&cd::zero_jitter_projections==1,"waiting publishes unjittered matrices");
        check(cd::reconstruction.complete()&&cd::reconstruction.jitter(),"input recovery permits jitter again");
        fixture.stage(6);f=fixture.frame();cd::native(4,&f);check(cd::projections[6].valid&&cd::projections[6].source.projection.jitter_bits[0]!=0,"resumed projection uses jitter");
        cd::reconstruction.latch(14);cd::reconstruction.latch(99);fixture.stage(7);f=fixture.frame();cd::native(4,&f);
        check(!cd::reconstruction.complete()&&!cd::reconstruction.jitter()&&!cd::projections[7].valid&&cd::jitter_count==2&&cd::restorations==2&&cd::healthy(),"latched failure cannot resume or inject jitter");
        std::cout<<std::boolalpha<<"{\"reconstruction\":";cd::reconstruction.write(std::cout);std::cout<<"}\n";return 0;
    }
    const unsigned count=mode=="continuous"?1000:2;
    for(unsigned i=0;i<count;++i){fixture.stage(5+i);auto f=fixture.frame();
        if(mode=="jitter_exception"&&i==1)exception_call(&f);else cd::native(4,&f);
        check(get<std::uint64_t>(fixture.camera.p+0x38)==0,"scoped camera restoration");
        check(cd::pending.load()==0&&!cd::projection_busy.test(),"callback drainage");
        if(mode=="continuous"){
            const auto &p=cd::projections[(5+i)%cd::projections.size()];check(p.valid&&p.history.frame==5+i&&p.source.matrices.valid,"published matching matrices");
            check(p.source.projection.jitter_bits[0]==std::bit_cast<unsigned>(.375F),"matching injected jitter");}
    }
    check(cd::jitter_count==count&&cd::restorations==count,"all injections restored");
    if(mode=="continuous")check(cd::healthy()&&cd::accepting,"no fixed frame cutoff");
    else if(mode=="jitter_stop")check(!cd::accepting&&cd::healthy(),"stop restores in-flight projection");
    else check(!cd::healthy()&&!cd::accepting,"invalid input fails closed");
    std::cout<<"Continuous callback fixture passed: "<<mode<<", "<<count<<" projections\n";
}
