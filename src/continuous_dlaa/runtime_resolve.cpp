// The TAA dispatch (hook 9): admitted with every input proven, it records
// DLSS (or the unfiltered copy) in place of the game's TAA, and stages frame
// generation's inputs.
#include "runtime_state.hpp"

namespace mhw::continuous_dlaa::detail {
bool dispatch_before(ID3D12GraphicsCommandList *list,UINT x,UINT y,UINT z,std::uintptr_t caller){
    const bool prepared_output=
        current_kind==1&&sr::full_contract(consumer.source);
    if(injected||current_kind!=1||(!accepting.load()&&!prepared_output))return false;
    const auto skip=[](unsigned why){last_skip=why;++skips;Lock lock;++dispatch_reasons[why];return false;};
    if(++dispatch_count!=1||list!=current_list||caller!=cfg.image+0x259cfca)return skip(1);
    const auto &s=consumer.source;tb::Packet packet;tb::ShaderRecipe shader;std::uintptr_t current_packet{},root{},cached{},pipeline{};
    for(unsigned role:{0U,4U}){const auto &owner=s.inputs[role];tb::Owner actual;if(!tb::read_owner(reader,owner.object,actual))return skip(2);actual.id=owner.id;if(actual!=owner)return skip(2);}
    if(!reader.get(current_recording+0x158,current_packet)||current_packet!=consumer.packet.object||!tb::packet(reader,current_packet,packet)||packet!=consumer.packet||!tb::packet_matches(s,packet,x,y,z)||
        !tb::shader_recipe(reader,s,shader)||shader!=consumer.shader||!reader.get(cfg.backend+0x2a0,root)||!reader.get(current_recording+0x118,cached)||root!=cached||!reader.get(current_recording+0x108,pipeline))return skip(4);
    temporal_present::Bindings state;if(!obs::current(list,state)||state.compute.signature!=root||state.pipeline!=pipeline)return skip(5);
    if(!state_of(s.inputs[0].resource,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)||!state_of(s.inputs[4].resource,D3D12_RESOURCE_STATE_UNORDERED_ACCESS)){
        Lock lock;const auto *record=find(list);
        const bool admitted=(accepting.load()||prepared_output)&&record&&!record->closed&&record->generation==state.generation&&
            state_admission.attempt(reader,s,consumer.sequence,list,record->generation,consumer.packet.object,GetTickCount64(),std::span(barriers).first(barrier_count));
        if(!admitted){trace_states(list,false);last_skip=6;++skips;++dispatch_reasons[6];return false;}
        tb::Packet again;std::uintptr_t active_packet{};
        if(!tb::packet(reader,consumer.packet.object,again)||again!=consumer.packet||!reader.get(current_recording+0x158,active_packet)||active_packet!=again.object){last_skip=4;++skips;++dispatch_reasons[4];return false;}
    }
    std::array<std::byte,native_taa_shader_bytes> bytecode{};
    const unsigned variant=native_shader_index(shader.technique,shader.size,include_menu_shader);
    const bool shape=single_native_output(s,shader,include_menu_shader);
    const bool copied=shape&&pb::copy_memory(shader.bytecode,bytecode.data(),shader.size);
    const auto tested=std::span<const std::byte>(bytecode).first(copied?shader.size:0U);
    Lock lock;const auto *l=find(list);
    const auto locked_skip=[](unsigned why){last_skip=why;++skips;++dispatch_reasons[why];return false;};
    if(!accepting.load()&&!prepared_output)return false;
    ++resolve_telemetry.shader_checks;
    const bool previously_verified=variant<unknown_native_shader&&shader_cache.verified[variant];
    const bool matches=copied&&shader_cache.matches(variant,tested,[](std::span<const std::byte> data){return pb::bytes_sha256(data.data(),static_cast<unsigned>(data.size()));});
    if(!matches){
        if(!shader_rejection.present){
            shader_rejection.capture(s,shader,shape,copied,matches,previously_verified,tested,reader);
            shader_rejection.packet=consumer.packet;shader_rejection.sequence=consumer.sequence;
            shader_rejection.generation=state.generation;shader_rejection.list=reinterpret_cast<std::uintptr_t>(list);}
        ++resolve_telemetry.shader_rejected;fail(29);return locked_skip(9);}
    if(!l||l->closed||l->generation!=state.generation)return locked_skip(5);
    obs::Predicate native_predicate;
    if(!obs::predicate_current(list,native_predicate)||native_predicate.buffer)return locked_skip(5);
    const auto unfiltered=[&](unsigned why,bool allow_full=true){
        const bool owns_private=!lease.list||(lease.list==list&&lease.generation==l->generation&&lease.frame==s.frame&&!lease.evaluated);
        if(prepared_output&&allow_full&&owns_private){
            // The native post chain already owns a full-output contract. A
            // missing temporal input changes reconstruction quality, not the
            // dimensions consumed by recording workers that may run first.
            if(!lease.list){
                recording_pins.collect(fence->GetCompletedValue());
                if(!recording_pins.retain(list,l->generation,consumer.resources)){fail(1);return locked_skip(8);}
                lease={list,l->generation,s.frame,false,false};find(list)->injected_generation=l->generation;
                ++spatial_recordings;
            }
            if(!sr::ready(list,l->generation,s,current_recording)){fail(46);return locked_skip(11);}
            copy_input(list,reinterpret_cast<ID3D12Resource *>(s.inputs[0].resource),0,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            fill_edge(list,reinterpret_cast<ID3D12Resource *>(s.inputs[0].resource),0,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
            last_skip=why;++skips;++dispatch_reasons[why];reconstruction.suspend();prepared_resolve={state,{},false,true};return true;
        }
        const ResourcePins colors{consumer.resources[0],consumer.resources[2],consumer.resources[0],consumer.resources[2]};
        if(!temporal_present::can_copy_unfiltered(list,colors[0].Get(),colors[1].Get(),runtime.width,runtime.height,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)||
           !unfiltered_lifetime.retain(list,l->generation,colors)){fail(33);return locked_skip(11);}
        last_skip=why;++skips;++dispatch_reasons[why];reconstruction.suspend();prepared_resolve={state,{},false};return true;
    };
    if(reconstruction.failed()||!accepting.load())return unfiltered(0);
    for(unsigned role:{1U,2U,3U,5U,6U}){const auto &owner=s.inputs[role];tb::Owner actual;
        if(!tb::read_owner(reader,owner.object,actual))return unfiltered(2);actual.id=owner.id;if(actual!=owner)return unfiltered(2);}
    const DepthEligibilityTrace::Identity depth_at{GetTickCount64(),s.frame,s.epoch,consumer.sequence,consumer.packet.object,
        reinterpret_cast<std::uintptr_t>(list),l->generation,s.view,s.context,s.bank,s.async};
    DepthReadObservation depth_reads{reader};tb::SceneDepth depth;
    const bool depth_readable=tb::scene_depth(depth_reads.reader(),s,depth);
    const bool depth_equal=depth_readable&&depth==consumer.depth;
    depth_eligibility.scene(depth_at,consumer.depth,depth,depth_readable,depth_equal,depth_reads);
    if(!depth_destination.check(depth_at,consumer.depth,depth,depth_readable))return unfiltered(3);
    if(!state_of(s.inputs[5].resource,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)){trace_states(list,true);return unfiltered(6);}
    auto *depth_resource=consumer.resources[3].Get();
    const bool depth_description=depth_resource&&reinterpret_cast<std::uintptr_t>(depth_resource)==consumer.depth.texture.resource&&
        depth_copy_description(depth_resource->GetDesc(),runtime.width,runtime.height,true)&&
        depth_copy_description(raw[1]->GetDesc(),runtime.width,runtime.height,false);
    const auto depth_state=depth_consumer_admission.attempt(reader,s,consumer.depth.texture,consumer.depth_object,consumer.depth_packet,
        consumer.packet.object,std::span(barriers).first(barrier_count),consumer.sequence,reinterpret_cast<std::uintptr_t>(list),
        l->generation,GetTickCount64(),!lease.list&&consumer.depth_sequence!=0,depth_description,identity_extent());
    if(depth_state){
        // This recording owns all private inputs and their completion signal.
        // The old draw/copy callbacks no longer inject scaled depth work.
        recording_pins.collect(fence->GetCompletedValue());
        if(!recording_pins.retain(list,l->generation,consumer.resources)){fail(1);return locked_skip(8);}
        lease={list,l->generation,s.frame,true,false};find(list)->injected_generation=l->generation;
        if(!record_consumer_depth(list,depth_resource,raw[1].Get(),runtime.width,runtime.height,
            static_cast<D3D12_RESOURCE_STATES>(depth_state),reinterpret_cast<DepthBarrierFn>(originals[8]),reinterpret_cast<DepthCopyFn>(originals[10]))){
            fail(44);return locked_skip(11);}
        ++depth_count;depth_consumer_admission.copied();
        // GBuffer4's state was proven above (state_of, TAA's own barriers).
        sr::full_post_states(s,depth_state,reinterpret_cast<std::uintptr_t>(depth_resource),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,s.inputs[5].resource);
    }
    const bool qualified_depth=lease.depth
        &&depth_state!=0
        ;
    depth_eligibility.lease(depth_at,reinterpret_cast<std::uintptr_t>(lease.list),lease.generation,lease.frame,qualified_depth,lease.evaluated);
    depth_schedule_trace.observe(reader,s,consumer.depth,consumer.depth_object,consumer.depth_packet,consumer.depth_sequence,
        consumer.sequence,reinterpret_cast<std::uintptr_t>(list),l->generation,consumer.packet.object,GetTickCount64(),
        lease.list!=list||lease.generation!=l->generation||lease.frame!=s.frame||!qualified_depth||lease.evaluated,std::span(barriers).first(barrier_count));
    // Render scale 1.0: record the native depth schedule (observation only).
    if(identity_extent())identity_depth_survey.observe(reader,s,consumer.depth.texture,consumer.depth_object,consumer.depth_packet,
        consumer.packet.object,consumer.sequence,consumer.depth_sequence,reinterpret_cast<std::uintptr_t>(list),l->generation,GetTickCount64(),
        lease.list!=list||lease.generation!=l->generation||lease.frame!=s.frame||!qualified_depth||lease.evaluated);
    if(lease.list!=list||lease.generation!=l->generation||lease.frame!=s.frame||!qualified_depth||lease.evaluated){
        {DepthTraceLock trace_lock;scaled_depth_trace.missing({GetTickCount64(),s.frame,s.epoch,consumer.sequence,consumer.depth_sequence,
            consumer.packet.object,reinterpret_cast<std::uintptr_t>(list),l->generation},
            {reinterpret_cast<std::uintptr_t>(lease.list),lease.generation,lease.frame,qualified_depth,lease.evaluated});}
        return unfiltered(7);}
    if(!recording_pins.matches(list,l->generation,consumer.resources))return unfiltered(8);
    const auto decision=history_decision(previous,consumer.projection.history,runtime_history_policy);
    // A skipped frame creates a real frame gap. Never resume with retained
    // history if a suspension did not yet create such a reset boundary.
    if(!decision.valid||(reconstruction.waiting()&&!decision.reset))return unfiltered(10);
    if(!temporal_present::can_copy_unfiltered(list,raw[0].Get(),reinterpret_cast<ID3D12Resource *>(s.inputs[4].resource),runtime.width,runtime.height,D3D12_RESOURCE_STATE_COPY_DEST)){
        fail(30);return locked_skip(11);}
    if(!sr::ready(list,l->generation,s,current_recording)){
        {DepthTraceLock trace_lock;++scaled_depth_trace.readiness_rejected;}return unfiltered(7,false);}
    prepared_resolve={state,decision,true,prepared_output};
    copy_input(list,reinterpret_cast<ID3D12Resource *>(s.inputs[0].resource),0,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    copy_input(list,reinterpret_cast<ID3D12Resource *>(s.inputs[5].resource),2,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    fill_edge(list,reinterpret_cast<ID3D12Resource *>(s.inputs[0].resource),0,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    fill_edge(list,reinterpret_cast<ID3D12Resource *>(s.inputs[5].resource),2,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    return true;
}
ResolveResult dispatch_replace(ID3D12GraphicsCommandList *list){
    Lock lock;const auto &state=prepared_resolve.bindings;const auto &decision=prepared_resolve.history;
    if(!prepared_resolve.evaluate){
        if(!obs::matches_current(list,state)){fail(12);return {};}
        if(prepared_resolve.full_output){
            auto result=scaled_temporal::record_resolve(*pass,*spatial,list,reinterpret_cast<ID3D12Resource *>(consumer.source.inputs[4].resource),
                state,{},true,0,false);
            sr::resolved(result.output_written(),false);
            if(!result.output_written()||!obs::matches_current(list,state)){fail(46);return {ResolveOutcome::unrecoverable};}
            lease.evaluated=true;return result;
        }
        const bool copied=temporal_present::copy_unfiltered(list,consumer.resources[0].Get(),consumer.resources[2].Get(),
            runtime.width,runtime.height,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        if(!copied||!obs::matches_current(list,state)){fail(34);return {ResolveOutcome::unrecoverable};}
        return {ResolveOutcome::unfiltered};
    }
    if(!obs::matches_current(list,state)||lease.list!=list||lease.generation!=state.generation){fail(12);return {};}
    const auto bits=consumer.projection.source.projection.jitter_bits;
    dlss_sdk::PixelJitter mapped;const auto viewport=consumer.projection.source.projection.viewport_bits;
    if(!dlss_sdk::projection_jitter(scaled_geometry,{std::bit_cast<float>(bits[0]),std::bit_cast<float>(bits[1])},
        std::bit_cast<float>(viewport[0]),std::bit_cast<float>(viewport[1]),mapped)){sr::resolved(false);fail(41);return {};}
    const dlss_sdk::HistoryFrame frame{consumer.source.frame,consumer.source.epoch,mapped.x,mapped.y};
    // Once claimed, even an explicit stop between preparation and replacement
    // must write the output. It must never resume the native TAA calculation.
    auto result=scaled_temporal::record_resolve(*pass,*spatial,list,reinterpret_cast<ID3D12Resource *>(consumer.source.inputs[4].resource),
        state,frame,decision.reset,decision.milliseconds,accepting.load()&&!reconstruction.failed());
    sr::resolved(result.output_written(),result.outcome==ResolveOutcome::reconstructed);
    if(!obs::matches_current(list,state)){fail(31);result.outcome=ResolveOutcome::unrecoverable;return result;}
    if(result.output_written())lease.evaluated=true;
    if(result.outcome!=ResolveOutcome::reconstructed){
        if(result.outcome==ResolveOutcome::recovered){reconstruction.latch(14);activation::latch_failure();}
        else if(result.outcome!=ResolveOutcome::unfiltered)fail(30);
        return result;
    }
    // Stage this frame's converted depth/motion and camera; the replaced final
    // screen draw of the same frame records them with the HUD-less colour.
    {frame_generation::game::SlotCamera camera;const auto &h=consumer.projection.history;camera.view=h.view_matrix;
     for(unsigned i=0;i<16;++i)camera.projection[i]=std::bit_cast<float>(h.projection[i]);
     camera.jitter_x=frame.jitter_x;camera.jitter_y=frame.jitter_y;camera.reset=decision.reset;
     const auto images=pass->private_images();
     const bool outer=injected;injected=true;  // our staging copy is not native work
     frame_generation::game::stage(list,consumer.source.frame,consumer.source.epoch,images[1],images[2],camera);
     injected=outer;}
    history_telemetry.record(previous,consumer.projection.history,decision);
    history_depth=consumer.resources[3];previous=consumer.projection.history;lease.evaluated=true;++frames;last_skip=0;if(decision.reset)++reset_count;
    reconstruction.complete();
    return result;
}
// stream_route final-scene observer: our barriers, copies and dispatch must
// bypass this runtime's own hooks exactly like the DLSS replacement's work.
void fg_final_scene(ID3D12GraphicsCommandList *list,const temporal_present::Bindings &roots,ID3D12Resource *scene,
    std::uint64_t frame,std::uint64_t epoch,D3D12_RESOURCE_STATES scene_state) noexcept {
    const bool outer=injected;injected=true;
    frame_generation::game::record(list,roots,scene,frame,epoch,scene_state);
    injected=outer;
}
ResolveResult replace_scoped(ID3D12GraphicsCommandList *list){
    ResolveResult result;injected=true;__try{result=dispatch_replace(list);}__finally{injected=false;}return result;
}
void dispatch_call(ID3D12GraphicsCommandList *list,UINT x,UINT y,UINT z,std::uintptr_t caller){
    const bool candidate=!injected&&current_kind==1&&list==current_list&&caller==cfg.image+0x259cfca;
    if(candidate){Lock lock;resolve_telemetry.begin();}
    const bool admitted=dispatch_before(list,x,y,z,caller);
    const auto result=dispatch_resolve(admitted,[&]{return replace_scoped(list);},
        [&]{reinterpret_cast<DispatchFn>(originals[9])(list,x,y,z);});
    if(candidate){Lock lock;resolve_telemetry.end(result);}
}
void WINAPI dispatch_hook(ID3D12GraphicsCommandList *list,UINT x,UINT y,UINT z){perf::hook_timing::Scope timing(perf::hook_timing::dispatch);Guard guard;dispatch_call(list,x,y,z,reinterpret_cast<std::uintptr_t>(_ReturnAddress()));}
}  // namespace mhw::continuous_dlaa::detail
