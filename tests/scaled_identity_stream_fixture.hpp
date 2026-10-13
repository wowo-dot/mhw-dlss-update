#pragma once
// The identity route end to end (v0.26.0: render scale 1.0, and every frame
// full_post arms). Per frame the resolve claims the plan (hand-off node 0); the
// TAA copy (node 1) copies the render rectangle under the native predicate and
// the whole texture under the full one; the final copy (node 2) runs
// unchanged. The recordings are validated, executed, completed and retired.
// 96 frames, through all 32 plan slots three times:
// - identity: one list;
// - identity_split: the final copy on a second list, recorded first on odd
//   frames (CPU order) but submitted after the first (GPU order; the reverse
//   batch is refused);
// - identity_native: the game keeps TAA's own resolve, so only the render
//   rectangle is copied.
namespace identity_stream_fixture {
using namespace mhw::fixtures::effects;
namespace route=mhw::scaled_temporal::stream_route;
template<class T,std::size_t N>void put(std::array<std::byte,N> &a,unsigned offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
inline void run(Fixture &f,const std::string &mode){
    const bool split=mode=="identity_split",native=mode=="identity_native";
    auto td=texture(128,64,DXGI_FORMAT_R11G11B10_FLOAT);td.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    // TAA's output and the motion filter's input (the TAA copy), the grade's
    // output and the view's attachment (the final copy).
    auto source=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
    auto destination=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
    auto grade=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
    auto attachment=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
    Pixels input(f.device.Get(),td,D3D12_HEAP_TYPE_UPLOAD),graded(f.device.Get(),td,D3D12_HEAP_TYPE_UPLOAD),poison(f.device.Get(),td,D3D12_HEAP_TYPE_UPLOAD),
           copied(f.device.Get(),td,D3D12_HEAP_TYPE_READBACK),final_pixels(f.device.Get(),td,D3D12_HEAP_TYPE_READBACK);
    std::vector<unsigned> pixels(128*64),shade(128*64);const std::vector<unsigned> sentinel(128*64,0x98000001U);
    for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x){
        pixels[y*128+x]=(15U*64+(x*3+y*5)%64)|((14U*64+(x*7+y*11)%64)<<11)|((16U*32+(x+y*3)%32)<<22);
        shade[y*128+x]=(13U*64+(x*5+y)%64)|((12U*64+(x+y*7)%64)<<11)|((14U*32+(x*3+y)%32)<<22);}
    input.write(std::as_bytes(std::span(pixels)));graded.write(std::as_bytes(std::span(shade)));poison.write(std::as_bytes(std::span(sentinel)));
    std::array<ComPtr<ID3D12CommandAllocator>,2> allocators{f.allocator};std::array<ComPtr<ID3D12GraphicsCommandList>,2> lists{f.list};
    if(split){hr(f.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocators[1])),"second allocator");
        hr(f.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocators[1].Get(),nullptr,IID_PPV_ARGS(&lists[1])),"second list");
        hr(lists[1]->Close(),"second list initial close");}
    const unsigned list_count=split?2U:1U;
    // The engine packets: TAA's (opcode 6), the TAA copy's and the final copy's
    // (their 64 field bytes, at +0x10, are what the plan proves), and the
    // copy executor's recording state (+0x158 the packet, +0x20 the list).
    std::array<std::byte,48> temporal{};std::array<std::byte,80> copy_packet{},final_packet{};std::array<std::byte,0x160> recording{};
    put(temporal,0,6U);put(temporal,0x10,std::uintptr_t{1234});
    mhw::temporal_bindings::Snapshot snapshot;snapshot.rectangle={0,0,64,32};
    const auto queue_id=reinterpret_cast<std::uintptr_t>(f.queue.Get());
    const auto copy=[&](ID3D12GraphicsCommandList *list,unsigned frame,const std::array<std::byte,80> &packet,bool final,
                        ID3D12Resource *from,ID3D12Resource *to,const D3D12_BOX &box){
        put(recording,0x158,reinterpret_cast<std::uintptr_t>(packet.data()));put(recording,0x20,reinterpret_cast<std::uintptr_t>(list));
        route::copy_slot=frame%32;route::copy_recording=reinterpret_cast<std::uintptr_t>(recording.data());route::copy_calls=0;route::copy_final=final;
        D3D12_TEXTURE_COPY_LOCATION s{},d{};s.pResource=from;s.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;d=s;d.pResource=to;
        const bool ok=route::copy_override(list,&d,0,0,0,&s,&box,0x259caf0);route::copy_slot=UINT_MAX;route::copy_final=false;return ok;};
    for(unsigned frame=1;frame<=96;++frame){
        route::begin();snapshot.frame=frame;route::publish(snapshot,{});auto &plan=route::plans[frame%32];
        plan.built=plan.identity=true;
        require(mhw::temporal_bindings::packet(route::reader,reinterpret_cast<std::uintptr_t>(temporal.data()),plan.temporal),"identity TAA packet");
        plan.copy=reinterpret_cast<std::uintptr_t>(copy_packet.data());plan.final_copy=reinterpret_cast<std::uintptr_t>(final_packet.data());
        plan.resources[0]=source;plan.resources[1]=destination;plan.resources[2]=grade;plan.resources[3]=attachment;
        require(route::handoff_count(plan)==3&&route::final_node(plan)==2,"identity hand-off: resolve, TAA copy, final copy");
        const std::array<std::uintptr_t,3> packets{plan.temporal.object,plan.copy,plan.final_copy};
        require(route::prepare_selector(plan)&&plan.handoff.prepare({frame,1,1,2},packets)==sf::FrameHandoff::Result::ok,"identity frame prepared");
        plan.prepared=plan.claimed=true;route::watch(frame%32,plan);
        for(unsigned i=0;i<list_count;++i){hr(allocators[i]->Reset(),"allocator");hr(lists[i]->Reset(allocators[i].Get(),nullptr),"list");
            mhw::temporal_present::observer::reset(lists[i].Get(),frame,nullptr);}
        const auto transition=[&](ID3D12GraphicsCommandList *list,ID3D12Resource *r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
            D3D12_RESOURCE_BARRIER b{};b.Transition={r,UINT_MAX,before,after};list->ResourceBarrier(1,&b);route::barriers(list,1,&b);};
        // The resolve and the TAA copy, on the first list.
        const auto record_resolve_and_copy=[&]{auto *list=lists[0].Get();
            if(native){require(route::observe_resolve(plan.temporal.object,list,frame),"native resolve observed");route::finish_resolve(plan.temporal.object,true);}
            else{require(route::ready(list,frame,snapshot,reinterpret_cast<std::uintptr_t>(recording.data())),"resolve claims the frame");route::resolved(true,true);}
            input.copy(list,source.Get(),true);
            transition(list,source.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
            transition(list,destination.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
            poison.copy(list,destination.Get(),true);
            require(copy(list,frame,copy_packet,false,source.Get(),destination.Get(),{0,0,0,64,32,1}),"TAA copy, native and full");
            {mhw::temporal_present::observer::Predicate predicate;
             require(mhw::temporal_present::observer::predicate_current(list,predicate)&&!predicate.buffer,"TAA copy restores the disabled predicate");}
            transition(list,destination.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);copied.copy(list,destination.Get(),false);
            transition(list,destination.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);
            transition(list,source.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);};
        // The final copy, on the last list.
        const auto record_final=[&]{auto *list=lists[list_count-1].Get();
            graded.copy(list,grade.Get(),true);
            transition(list,grade.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
            transition(list,attachment.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
            require(copy(list,frame,final_packet,true,grade.Get(),attachment.Get(),{0,0,0,128,64,1}),"final copy unchanged");
            transition(list,attachment.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);final_pixels.copy(list,attachment.Get(),false);
            transition(list,attachment.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COMMON);
            transition(list,grade.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);};
        if(split&&frame%2){record_final();record_resolve_and_copy();}else{record_resolve_and_copy();record_final();}
        require(route::complete==frame&&route::identity_final_copies==frame&&route::copies==2*frame&&route::healthy(),"identity frame complete");
        std::array<ID3D12CommandList *,2> batch{};std::array<std::array<std::uint64_t,2>,2> ids{};
        for(unsigned i=0;i<list_count;++i){hr(lists[i]->Close(),"close");batch[i]=lists[i].Get();ids[i]={reinterpret_cast<std::uintptr_t>(lists[i].Get()),frame};}
        if(split){const std::array<std::array<std::uint64_t,2>,2> reversed{ids[1],ids[0]};
            require(!route::validate_submission(queue_id,reversed,f.sequence+1),"the final copy's list before the resolve's is refused");}
        require(route::validate_submission(queue_id,std::span(ids).first(list_count),f.sequence+1),"identity GPU order validated");
        f.queue->ExecuteCommandLists(list_count,batch.data());route::observe_submission(queue_id,GetTickCount64(),std::span(ids).first(list_count));f.wait();
        require(route::submitted_batch(queue_id,std::span(ids).first(list_count),f.sequence),"submission completes the frame");
        route::collect(f.fence->GetCompletedValue());require(route::lifetime.live()==list_count,"completion alone retains the recordings");
        const auto transfer=copied.read();
        if(native){for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x)require(transfer[y*128+x]==(y<32&&x<64?pixels[y*128+x]:sentinel[y*128+x]),"native: the render rectangle only");}
        else require(transfer==pixels,"reconstructed: the whole texture");
        require(final_pixels.read()==shade,"the final copy unchanged");
        for(unsigned i=0;i<list_count;++i){hr(allocators[i]->Reset(),"retire allocator");hr(lists[i]->Reset(allocators[i].Get(),nullptr),"retire list");hr(lists[i]->Close(),"retire close");
            require(route::reset(lists[i].Get(),frame),"native reset");}
        route::collect(f.fence->GetCompletedValue());require(!route::lifetime.live()&&!plan.prepared,"retired and released");
    }
    require(route::lifetime.retained()==96U*list_count&&route::lifetime.released()==96U*list_count&&route::lifetime.peak()==list_count,"bounded 96-frame lifetime");
    require(route::submitted_frames==96&&route::native_frames==(native?96U:0U),"identity accounting");
    f.messages();
    std::cout<<"PASS identity stream ("<<mode<<"): 96 frames, resolve, TAA copy and final copy, "<<(split?"split lists in both CPU orders, ":"")
             <<"GPU order validated, exact pixels, retirement\n";
}
}
