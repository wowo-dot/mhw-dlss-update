// Exercise the real streaming submission callbacks using owned D3D12 objects.
// No game process, native hooks or vendor initialization is used here.
#include "../src/aa_control/stream_runtime.cpp"
#include <dxgi1_4.h>
#include <d3d12sdklayers.h>
#include <iostream>
#include <cstdlib>
#include <sstream>
namespace st=mhw::aa_control::stream;
void check(bool v,const char *why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
void *real_execute{};ID3D12CommandAllocator *racing_allocator{};
void WINAPI execute_then_reset(ID3D12CommandQueue *q,UINT n,ID3D12CommandList *const *lists){
    reinterpret_cast<void(WINAPI *)(ID3D12CommandQueue *,UINT,ID3D12CommandList *const *)>(real_execute)(q,n,lists);
    check(SUCCEEDED(st::reset_hook(static_cast<ID3D12GraphicsCommandList *>(lists[0]),racing_allocator,nullptr)),"Reset before signal return");
}
int main(int argc,char **argv){
    const std::string mode=argc>1?argv[1]:"queues";
    if(mode=="admission-diagnostic"){
        mhw::aa_control::PostQueue q;q.frame=100;q.count=4;
        st::last_admitted_queue=q;++q.frame;
        mhw::aa_control::owned::Admission a;a.step="plan";a.previous_complete=true;
        st::accepting=true;st::reject_queue(3,q,true,a);
        q.frame=999;a.step="recycle-incomplete";st::last_admitted_queue={};
        st::reject_queue(4,q,false,a);
        const auto &r=st::first_queue_rejection;
        check(st::fault==8&&!st::accepting&&r.slot==3&&r.recycled&&r.previous.frame==100&&r.rejected.frame==101,
              "immutable first rejection and previous accepted queue");
        check(std::string(r.admission.step)=="plan"&&r.admission.previous_complete,"owned admission copied before fault");
        std::ostringstream output;st::write(output);const auto text=output.str();
        check(text.find("\"queue_rejection\":{\"schema\":1")!=std::string::npos&&text.find("\"step\":\"plan\"")!=std::string::npos,
              "bounded diagnostic serialized without live native reads");
        std::cout<<text;return 0;
    }
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D12Debug> debug;check(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))),"debug");debug->EnableDebugLayer();
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> adapter;ComPtr<ID3D12Device> device;
    check(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))&&SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))&&SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device))),"WARP");
    st::cfg.device=reinterpret_cast<std::uintptr_t>(device.Get());st::accepting=true;st::enabled[3]=true;
    check(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&st::fence))),"fence");
    std::array<ComPtr<ID3D12CommandQueue>,2> queues;std::array<ComPtr<ID3D12CommandAllocator>,2> allocators;std::array<ComPtr<ID3D12GraphicsCommandList>,2> lists;
    D3D12_COMMAND_QUEUE_DESC q{};
    for(unsigned i=0;i<2;++i)check(SUCCEEDED(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queues[i])))&&SUCCEEDED(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocators[i])))&&SUCCEEDED(device->CreateCommandList(0,q.Type,allocators[i].Get(),nullptr,IID_PPV_ARGS(&lists[i])))&&SUCCEEDED(lists[i]->Close()),"queue/list pool");
    auto table=*reinterpret_cast<void ***>(lists[0].Get());st::originals[3]=table[10];st::originals[4]=table[9];st::originals[5]=(*reinterpret_cast<void ***>(queues[0].Get()))[10];
    if(mode=="boundary-diagnostic"){
        check(SUCCEEDED(st::reset_hook(lists[0].Get(),allocators[0].Get(),nullptr)),"diagnostic Reset");
        auto *l=st::find(lists[0].Get());auto &s=st::slots[0];
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=desc.Height=1;
        desc.DepthOrArraySize=desc.MipLevels=1;desc.Format=DXGI_FORMAT_R32_FLOAT;desc.SampleDesc.Count=1;
        for(auto &r:s.resources)check(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&r))),"diagnostic resources");
        const auto transition=[](ID3D12Resource *r,unsigned a,unsigned b){D3D12_RESOURCE_BARRIER v{};v.Transition={r,UINT_MAX,static_cast<D3D12_RESOURCE_STATES>(a),static_cast<D3D12_RESOURCE_STATES>(b)};return v;};
        std::array<D3D12_RESOURCE_BARRIER,2> batch{transition(s.resources[0].Get(),4,128),transition(s.resources[2].Get(),4,128)};
        std::array<st::InputState,3> after{};for(unsigned i=0;i<3;++i){after[i].tracked=true;after[i].value.resource=s.resources[i].Get();after[i].value.state.transition(4,128,0,UINT_MAX,i+1);}
        check(st::boundary_decision(s,batch,after).accepted(),"original color/motion boundary accepted");
        for(unsigned i=0;i<3;++i){auto bad=after;bad[i].value.state.invalidate(mhw::aa_control::gpu_trace::Issue::conflict,10);
            check(st::boundary_decision(s,batch,bad).invalid_states==(1U<<i)&&!st::boundary_decision(s,batch,bad).accepted(),"individual tracked-state rejection");}
        for(unsigned change=0;change<7;++change){auto bad=batch;
            if(change==0)bad[0].Flags=D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;
            if(change==1)bad[0].Transition.Subresource=0;
            if(change==2)bad[0].Transition.StateBefore=D3D12_RESOURCE_STATE_COMMON;
            if(change==3)bad[0].Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            if(change==4)bad[1]=bad[0];
            if(change==5)bad[0]=transition(s.resources[1].Get(),4,128);
            if(change==6){bad[0]={};bad[0].Type=D3D12_RESOURCE_BARRIER_TYPE_ALIASING;}
            check(!st::boundary_decision(s,bad,after).accepted(),"original strict boundary still rejects altered batch");}
        std::array<D3D12_RESOURCE_BARRIER,3> extra{batch[0],batch[1],{}};extra[2].Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;
        check(st::boundary_decision(s,extra,after).accepted(),"unrelated UAV does not count");
        extra[2]=transition(s.resources[1].Get(),4,128);check(!st::boundary_decision(s,extra,after).accepted(),"extra input transition rejected");
        // Feed the real fault-19 callback a color-only batch. No native or SDK
        // commands are issued by this observer entry point.
        check(s.life.reserve(123)&&s.life.queue()&&s.life.mark(l->generation),"diagnostic marker");s.source.epoch=123;s.native_nodes=5;
        st::apply(*l,transition(s.resources[0].Get(),128,4));st::apply(*l,transition(s.resources[1].Get(),4,128));
        s.marker_states=st::input_states(*l,s);s.marker_order=l->order;l->selected=0;st::enabled[6]=true;
        st::barrier_after(lists[0].Get(),1,batch.data());const auto &receipt=st::first_boundary_rejection;
        check(st::fault==19&&!st::accepting&&l->selected==st::none,"actual barrier callback rejects before evaluation");
        check(receipt.present&&receipt.frame==123&&receipt.native_nodes==5&&receipt.count==1&&receipt.decision.relevant==1&&receipt.decision.color&&!receipt.decision.motion&&receipt.decision.invalid_states==4,
              "precise failing predicates captured");
        check(receipt.marker[0].value.state.value==4&&receipt.before[0].value.state.value==4&&receipt.after[0].value.state.value==128&&!receipt.marker[2].tracked,
              "marker and before/after states captured without seeding unknown motion");
        auto changed=batch[0];changed.Transition.StateBefore=D3D12_RESOURCE_STATE_COMMON;st::apply(*l,changed);
        auto *tracked=st::state(*l,s.resources[0].Get());const auto first_invalid=tracked->invalid_order;
        check(first_invalid>0&&tracked->invalid_before.value==128&&tracked->first_invalid.Transition.StateBefore==0,"first state conflict records its cause");
        st::apply(*l,batch[0]);check(tracked->invalid_order==first_invalid&&tracked->first_invalid.Transition.StateBefore==0,"later transitions preserve first ambiguity");
        s.life.frame=999;batch[0]={};st::capture_boundary(st::first_boundary_rejection,*l,0,99,after,after,batch,{});
        check(receipt.frame==123&&receipt.batch[0].Transition.StateBefore==4&&receipt.after[0].value.state.known,"failure snapshot immutable after source changes");
        st::enabled[6]=false;check(SUCCEEDED(lists[0]->Close()),"close diagnostic list");
        std::ostringstream output;st::write(output);std::cout<<output.str();return 0;
    }
    if(mode=="reset-during-submit"){real_execute=st::originals[5];st::originals[5]=reinterpret_cast<void *>(execute_then_reset);racing_allocator=allocators[1].Get();}
    const auto wait=[&](UINT64 value){const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(event&&SUCCEEDED(st::fence->SetEventOnCompletion(value,event))&&WaitForSingleObject(event,5000)==WAIT_OBJECT_0,"completion");CloseHandle(event);};
    const unsigned count=mode=="queues"?1000U:1U;
    for(unsigned frame=0;frame<count;++frame){const unsigned at=frame%2,index=frame%st::capacity;auto *list=lists[at].Get();
        check(SUCCEEDED(allocators[at]->Reset())&&SUCCEEDED(st::reset_hook(list,allocators[at].Get(),nullptr)),"Reset");auto *l=st::find(list);check(l!=nullptr,"Reset registered list");auto &s=st::slots[index];
        check(s.life.reserve(frame+1)&&s.life.queue()&&s.life.mark(l->generation),"publish/consume");s.life.consumer_returned=true;s.life.producer_returned=true;
        check(s.life.record(),"record");s.list=list;l->injected=index;st::pending_recording=index;
        if(mode=="early-reset"){check(SUCCEEDED(list->Close())&&SUCCEEDED(st::reset_hook(list,allocators[at].Get(),nullptr)),"native early Reset");check(st::fault==21&&!st::accepting,"reject missing submission");return 0;}
        check(SUCCEEDED(st::close_hook(list)),"Close");ID3D12CommandList *batch[]{list};
        ComPtr<ID3D12Fence> gate;
        if(mode=="pending-gpu"){check(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&gate)))&&SUCCEEDED(queues[at]->Wait(gate.Get(),1)),"GPU gate");}
        st::execute_hook(queues[at].Get(),1,batch);check(s.life.stage==mhw::aa_control::StreamLifetime::submitted&&st::pending_recording==st::none,"submission proof");
        if(mode=="pending-gpu"){
            check(SUCCEEDED(st::reset_hook(list,allocators[1].Get(),nullptr)),"Reset with separate allocator");check(st::fence->GetCompletedValue()==0&&s.life.retired&&s.life.stage!=mhw::aa_control::StreamLifetime::free,"retain before GPU");
            check(SUCCEEDED(gate->Signal(1)),"unblock GPU");wait(1);st::collect();check(s.life.stage==mhw::aa_control::StreamLifetime::free&&st::released==1&&!st::fault,"release after both proofs");check(SUCCEEDED(list->Close()),"close empty");return 0;
        }
        wait(frame+1);
        if(mode=="reset-during-submit"){st::collect();check(!st::fault&&st::released==1&&s.life.stage==mhw::aa_control::StreamLifetime::free,"independent retirement and Signal receipts");check(SUCCEEDED(list->Close()),"close empty");return 0;}
        if(mode=="duplicate"){st::execute_hook(queues[at].Get(),1,batch);check(st::fault==23&&!st::accepting,"duplicate rejected");return 0;}
        st::collect();check(s.life.stage!=mhw::aa_control::StreamLifetime::free,"fence does not retire recording");
        check(SUCCEEDED(allocators[at]->Reset())&&SUCCEEDED(st::reset_hook(list,allocators[at].Get(),nullptr)),"retirement");
        check(s.life.stage==mhw::aa_control::StreamLifetime::free&&st::released==frame+1&&!st::fault,"bounded slot reuse");check(SUCCEEDED(list->Close()),"empty close");
    }
    ComPtr<ID3D12InfoQueue> messages;check(SUCCEEDED(device.As(&messages)),"debug messages");
    for(UINT64 i=0;i<messages->GetNumStoredMessages();++i){SIZE_T size{};messages->GetMessage(i,nullptr,&size);std::vector<std::byte> bytes(size);auto *m=reinterpret_cast<D3D12_MESSAGE *>(bytes.data());messages->GetMessage(i,m,&size);if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING){std::cerr<<m->pDescription<<'\n';check(false,"D3D12 diagnostics");}}
    check(st::signal_value==count&&st::fence->GetCompletedValue()==count,"all signals complete");std::cout<<"1,000 two-queue marker lifetimes passed\n";
}
