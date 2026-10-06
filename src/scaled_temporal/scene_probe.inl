// Included only by the opt-in FG scene-boundary probe. Uses existing native
// executor/API hooks. No resource QI/AddRef/GetDesc, GPU commands or new hooks.
using SceneTrace=mhw::frame_generation::SceneTrace;
SceneTrace scene_trace;
struct SceneScope {
 std::uintptr_t executor{},context{},packet{};
 std::uint64_t frame{},epoch{};
 unsigned event{SceneTrace::none};bool replaced{};
};
thread_local SceneScope scene_scope;
std::uint64_t scene_qpc() noexcept {LARGE_INTEGER q{};QueryPerformanceCounter(&q);return static_cast<std::uint64_t>(q.QuadPart);}
SceneScope scene_enter(const pb::Frame &f) noexcept {return {f.gpr[1],f.gpr[2],f.gpr[3],0,0,SceneTrace::none,false};}
void scene_exit(bool returned) noexcept {
 Lock lock;scene_trace.returned(scene_scope.frame,scene_scope.epoch,scene_scope.event,returned,scene_scope.replaced);
}
void scene_prepare(const Plan &p) noexcept {
 if(!scene_trace.wants_frame())return;
 const auto &q=final_quads[p.stamp.frame%final_quads.size()];
 if(!q.valid||q.packet!=p.draws[2].draw.packet||q.stamp.frame!=p.stamp.frame||q.stamp.epoch!=p.stamp.epoch||
    q.stamp.context!=p.stamp.context||q.stamp.view!=p.stamp.view)return;
 // The admitted plan owns these resources already; this diagnostic only
 // copies identities. It never assumes the target is a swap-chain buffer.
 if(scene_trace.begin({p.stamp.frame,p.stamp.epoch,p.stamp.context,p.stamp.view,
  p.draws[2].draw.packet,q.source.resource,p.draws[2].output.resource,width,height,p.draws[2].output.gpu_format,p.stamp.bank}))
  scene_trace.choice(p.stamp.frame,p.stamp.epoch,static_cast<unsigned>(p.handoff.choice()));
}
void scene_api(ID3D12GraphicsCommandList *list,bool indexed,std::array<std::uint64_t,5> args,std::uintptr_t caller) noexcept {
 if(injecting)return;Lock lock;
 std::uint64_t frame{},epoch{};
 if(!accepting.load()||!field(cfg.renderer,0x1f210,frame)||!field(cfg.renderer,0x1f208,epoch))return;
 // The existing route already identifies the exact final packet and its
 // immutable plan. Start here, not at preparation of a later native frame.
 if(api_slot!=UINT_MAX&&api_stage==2&&plans[api_slot].prepared&&
    scene_scope.packet==plans[api_slot].draws[2].draw.packet)scene_prepare(plans[api_slot]);
 auto *capture=scene_trace.recording();if(!capture)return;
 // Vendor/private draws are outside the two qualified game executor scopes.
 if(caller<cfg.image||caller>=cfg.image+0x55d3000)return;
 SceneTrace::Event e;e.kind=indexed?1U:0U;e.qpc=scene_qpc();e.thread=GetCurrentThreadId();e.caller=caller;
 e.native_frame=frame;e.native_epoch=epoch;
 e.packet=scene_scope.packet;e.list=reinterpret_cast<std::uintptr_t>(list);e.args=args;
 std::uintptr_t state{},command{},packet{},attachment{},active_attachment{},view{},backend{},native_rtv{},root{},pipeline{};
 unsigned char rotation{};unsigned flags{};
 temporal_present::Bindings roots;obs::RasterBindings raster;
 // These API labels can explain a rejected native scope; they never establish
 // an RTV resource identity or a completed draw. At most two entries/reason.
 const bool observed=obs::inspect(list,roots)&&roots.known&&roots.valid&&roots.generation;
 const bool raster_read=obs::raster_current(list,raster)&&raster.output_known;
 (void)obs::raster_inspect(list,e.raster_observation);
 if(observed){e.generation=roots.generation;e.root=roots.graphics.signature;e.pipeline=roots.pipeline;}
 if(raster_read)e.rtv=raster.render_target;
 const auto reject=[&](unsigned reason){++capture->unreadable;
  e.native_cache={state,command,packet,attachment,active_attachment,flags,view,backend,native_rtv,root,pipeline};
  scene_trace.reject(reason,e);};
 const auto cache_read=[&]<class T>(std::uintptr_t address,unsigned offset,T &value,unsigned bit){
  T temporary{};if(!field(address,offset,temporary))return false;
  value=temporary;e.native_cache_read_mask|=1U<<bit;return true;};
 if(!scene_scope.executor||!scene_scope.packet){++capture->outside_scope;scene_trace.reject(0,e);return;}
 // Deliberately do not follow the unbounded rotating-handle table. Preserve
 // its discriminator and native backing label; API/cache agreement is separate.
 if(!cache_read(scene_scope.executor,8,state,0)){reject(1);return;}
 if(!cache_read(state,0x20,command,1)||command!=e.list){reject(2);return;}
 if(!cache_read(state,0x158,packet,2)||packet!=e.packet){reject(3);return;}
 if(!cache_read(e.packet,0x10,attachment,3)||!cache_read(scene_scope.context,0x10,active_attachment,4)||attachment!=active_attachment){reject(4);return;}
 if(!cache_read(attachment,0x48,flags,5)||(flags&7U)!=1){reject(5);return;}
 if(!cache_read(attachment,0,view,6)||!cache_read(view,0x60,backend,7)||!field(backend,0x18,e.resource)||!e.resource){reject(6);return;}
 if(!field(backend,0x28,rotation)){reject(7);return;}
 if(!scene_trace.tracked(e.resource)){++capture->unmatched;return;}
 e.rotation=rotation;
 if(!observed){reject(8);return;}
 if(!raster_read){reject(9);return;}
 if(!cache_read(state,0x98,native_rtv,8)||native_rtv!=raster.render_target){reject(10);return;}
 if(!cache_read(state,0x110,root,9)||root!=roots.graphics.signature){reject(11);return;}
 if(!cache_read(state,0x108,pipeline,10)||pipeline!=roots.pipeline){reject(12);return;}
 e.native_cache={state,command,packet,attachment,active_attachment,flags,view,backend,native_rtv,root,pipeline};
 e.generation=roots.generation;e.rtv=raster.render_target;e.root=root;e.pipeline=pipeline;e.api_state_matched=true;
 pb::NativePipeline facts;
 e.pipeline_read=pb::snapshot_native_pipeline(cfg.image,e.packet,facts);
 if(e.pipeline_read){e.shader=facts.shader_handle;e.blend=facts.blend[2];e.depth=facts.depth[0];}
 if(e.packet==capture->identity.packet)e.stage=2;
 scene_scope.frame=capture->identity.frame;scene_scope.epoch=capture->identity.epoch;
 scene_scope.event=scene_trace.event(scene_scope.frame,scene_scope.epoch,e);
}
struct SceneCopyReceipt {std::uint64_t frame{},epoch{};unsigned index{SceneTrace::none};};
SceneCopyReceipt scene_copy(ID3D12GraphicsCommandList *list,const D3D12_TEXTURE_COPY_LOCATION *destination,
 UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION *source,const D3D12_BOX *box,std::uintptr_t caller) noexcept {
 if(injecting)return {};Lock lock;
 auto *capture=scene_trace.recording();if(!capture||!accepting.load()||caller<cfg.image||caller>=cfg.image+0x55d3000)return {};
 SceneTrace::Event e;e.kind=4;e.qpc=scene_qpc();e.thread=GetCurrentThreadId();e.caller=caller;e.list=reinterpret_cast<std::uintptr_t>(list);
 if(!field(cfg.renderer,0x1f210,e.native_frame)||!field(cfg.renderer,0x1f208,e.native_epoch))return {};
 D3D12_TEXTURE_COPY_LOCATION d{},s{};D3D12_BOX b{};
 if(!destination||!source||!pb::copy_memory(reinterpret_cast<std::uintptr_t>(destination),&d,sizeof(d))||
    !pb::copy_memory(reinterpret_cast<std::uintptr_t>(source),&s,sizeof(s))){scene_trace.reject(13,e);return {};}
 const auto src=reinterpret_cast<std::uintptr_t>(s.pResource),dst=reinterpret_cast<std::uintptr_t>(d.pResource);
 if(!scene_trace.tracked(src)&&!scene_trace.tracked(dst))return {};
 e.resource=scene_trace.tracked(src)?src:dst;
 if(!src||!dst||s.Type!=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX||d.Type!=s.Type||
    (box&&(!pb::copy_memory(reinterpret_cast<std::uintptr_t>(box),&b,sizeof(b))||
     b.left>=b.right||b.top>=b.bottom||b.front>=b.back))){scene_trace.reject(13,e);return {};}
 temporal_present::Bindings bindings;
 if(!obs::inspect(list,bindings)||!bindings.known||!bindings.valid||!bindings.generation){scene_trace.reject(14,e);return {};}
 e.generation=bindings.generation;
 e.copy={src,dst,static_cast<unsigned>(s.Type),static_cast<unsigned>(d.Type),s.SubresourceIndex,d.SubresourceIndex,
  x,y,z,box!=nullptr,b.left,b.top,b.front,b.right,b.bottom,b.back};
 return {capture->identity.frame,capture->identity.epoch,scene_trace.event(capture->identity.frame,capture->identity.epoch,e)};
}
void scene_copy_returned(SceneCopyReceipt receipt,bool replaced) noexcept {
 Lock lock;scene_trace.returned(receipt.frame,receipt.epoch,receipt.index,true,replaced);
}
void scene_barrier(ID3D12GraphicsCommandList *list,std::uint64_t generation,const D3D12_RESOURCE_BARRIER &b) noexcept {
 std::uint64_t frame{},epoch{};
 if(!field(cfg.renderer,0x1f210,frame)||!field(cfg.renderer,0x1f208,epoch))return;
 auto *capture=scene_trace.recording();if(!capture)return;
 const auto tracked=[](ID3D12Resource *resource){return scene_trace.tracked(reinterpret_cast<std::uintptr_t>(resource));};
 const bool transition=b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION&&tracked(b.Transition.pResource);
 const bool alias=b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING&&
   (!b.Aliasing.pResourceBefore||!b.Aliasing.pResourceAfter||tracked(b.Aliasing.pResourceBefore)||tracked(b.Aliasing.pResourceAfter));
 const bool uav=b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV&&(!b.UAV.pResource||tracked(b.UAV.pResource));
 if(!transition&&!alias&&!uav)return;
 SceneTrace::Event e;e.kind=transition?2U:3U;e.qpc=scene_qpc();e.thread=GetCurrentThreadId();
 e.list=reinterpret_cast<std::uintptr_t>(list);e.generation=generation;e.resource=capture->identity.target;e.flags=b.Flags;e.returned=true;
 e.native_frame=frame;e.native_epoch=epoch;
 if(transition){e.resource=reinterpret_cast<std::uintptr_t>(b.Transition.pResource);e.subresource=b.Transition.Subresource;e.before=b.Transition.StateBefore;e.after=b.Transition.StateAfter;}
 e.args[0]=static_cast<unsigned>(b.Type);(void)scene_trace.event(capture->identity.frame,capture->identity.epoch,e);
}
