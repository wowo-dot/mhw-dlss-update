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
// v0.26.0: a frame's scene ends with the identity plan's final copy (the grade
// output into the view's attachment), recorded under the route lock.
void scene_prepare(const Plan &p) noexcept {
 if(!scene_trace.wants_frame())return;
 const auto &q=final_quads[p.stamp.frame%final_quads.size()];
 if(!q.valid||q.stamp.frame!=p.stamp.frame||q.stamp.epoch!=p.stamp.epoch||q.stamp.context!=p.stamp.context||q.stamp.view!=p.stamp.view||
    q.destination.resource!=reinterpret_cast<std::uintptr_t>(p.resources[3].Get()))return;
 // The admitted plan owns these resources already; this diagnostic only
 // copies identities. It never assumes the target is a swap-chain buffer.
 if(scene_trace.begin({p.stamp.frame,p.stamp.epoch,p.stamp.context,p.stamp.view,
  p.final_copy,q.source.resource,q.destination.resource,width,height,q.destination.gpu_format,p.stamp.bank}))
  scene_trace.choice(p.stamp.frame,p.stamp.epoch,static_cast<unsigned>(p.handoff.choice()));
}
void scene_api(ID3D12GraphicsCommandList *list,bool indexed,std::array<std::uint64_t,5> args,std::uintptr_t caller) noexcept {
 if(injecting)return;Lock lock;
 std::uint64_t frame{},epoch{};
 if(!accepting.load()||!field(cfg.renderer,0x1f210,frame)||!field(cfg.renderer,0x1f208,epoch))return;
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
 const auto match=scene_trace.match(e.resource);
 if(match==SceneTrace::unmatched){++capture->unmatched;return;}
 // A request-time GetBuffer label is presentation metadata only; it never
 // joins the followed final-target path or the closure gate.
 e.presentation_label=match==SceneTrace::presentation;e.diagnostic=e.presentation_label;
 e.rotation=rotation;
 if(!observed){reject(8);return;}
 const auto &r=e.raster_observation;
 // The preserved output_known contract is unchanged. A complete one-RTV OM
 // observation with a readable depth attachment gets its own diagnostic
 // qualification; it never authorizes replacement or rendering changes.
 const bool depth_output=!raster_read&&r[0]==1&&r[1]==1&&r[2]==1&&r[3]==1&&r[4]==1&&r[5]==1&&
  r[6]==roots.generation&&r[7]==r[6]&&(r[8]&15U)==15U&&!(r[8]&32U)&&r[10]==1&&r[11]==1&&
  r[13]==1&&r[14]==1&&r[15]&&r[16]==1&&r[17]==1&&r[18];
 if(!raster_read&&!depth_output){reject(9);return;}
 const std::uintptr_t api_rtv=raster_read?raster.render_target:static_cast<std::uintptr_t>(r[15]);
 e.rtv=api_rtv;
 unsigned depth_format{};
 if(depth_output){
  e.output_scope=1;e.diagnostic=true;e.dsv=r[18];
  const auto output_read=[&](std::uintptr_t address,unsigned offset,unsigned bit,bool format=false){
   std::uintptr_t value{};unsigned narrow{};
   if(!(format?field(address,offset,narrow):field(address,offset,value)))return false;
   e.output_native[bit]=format?narrow:value;e.output_native_read_mask|=1U<<bit;return true;};
  const auto &o=e.output_native;
  // Existing tested attachment/cache offsets: backend+0x10 handle, attachment
  // +0x40 depth view, view+0x60 backend, +0x74 format, state+0x98 handles[9],
  // state+0x140 cached depth resource. No rotating table is traversed.
  if(!cache_read(state,0x98,native_rtv,8)||native_rtv!=api_rtv||!output_read(backend,0x10,0)||o[0]!=api_rtv){reject(16);return;}
  if(!output_read(attachment,0x40,1)||!o[1]||!output_read(o[1],0x60,2)||!o[2]||
     !output_read(o[2],0x10,3)||!output_read(o[2],0x18,4)||!o[4]||!output_read(o[1],0x74,5,true)||
     !output_read(state,0x98+8*8,6)||!output_read(state,0x140,7)||
     o[3]!=e.dsv||o[6]!=e.dsv||o[7]!=o[4]){reject(17);return;}
  if(!cache_read(state,0x110,root,9)||root!=roots.graphics.signature){reject(18);return;}
  if(!cache_read(state,0x108,pipeline,10)||pipeline!=roots.pipeline){reject(19);return;}
  depth_format=static_cast<unsigned>(e.output_native[5]);
 }else{
  if(!cache_read(state,0x98,native_rtv,8)||native_rtv!=raster.render_target){reject(10);return;}
  if(!cache_read(state,0x110,root,9)||root!=roots.graphics.signature){reject(11);return;}
  if(!cache_read(state,0x108,pipeline,10)||pipeline!=roots.pipeline){reject(12);return;}
 }
 // The exact final packet belongs to the preserved followed path only.
 if(e.diagnostic&&e.packet==capture->identity.packet){reject(20);return;}
 e.native_cache={state,command,packet,attachment,active_attachment,flags,view,backend,native_rtv,root,pipeline};
 e.generation=roots.generation;e.root=root;e.pipeline=pipeline;e.api_state_matched=true;
 pb::NativePipeline facts;
 e.pipeline_read=pb::snapshot_native_pipeline(cfg.image,e.packet,facts,depth_format);
 if(e.pipeline_read){e.shader=facts.shader_handle;e.blend=facts.blend[2];e.depth=facts.depth[0];
  for(unsigned i=0;i<10;++i)e.pipeline_facts[i]=facts.blend[2+i];
  for(unsigned i=0;i<3;++i)e.pipeline_facts[10+i]=facts.depth[i];
  e.pipeline_facts[13]=facts.dsv_format;}
 if(!e.diagnostic&&e.packet==capture->identity.packet)e.stage=2;
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
 const auto from=scene_trace.match(src),to=scene_trace.match(dst);
 if(!from&&!to)return {};
 // Followed copies keep the preserved v0.6.4 path/closure contract. A copy
 // touching only presentation labels is diagnostic and never extends it.
 e.diagnostic=from!=SceneTrace::followed&&to!=SceneTrace::followed;e.presentation_label=e.diagnostic;
 e.resource=from==SceneTrace::followed||(e.diagnostic&&from)?src:dst;
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
 using M=SceneTrace::Match;
 const auto match=[](ID3D12Resource *resource){return scene_trace.match(reinterpret_cast<std::uintptr_t>(resource));};
 // Followed scope wins; presentation-only barriers are diagnostic metadata.
 // Null alias/UAV barriers stay in the preserved followed scope as before.
 M scope=SceneTrace::unmatched;ID3D12Resource *label{};
 const auto consider=[&](ID3D12Resource *resource){const auto m=match(resource);
  if(m==SceneTrace::followed||(m==SceneTrace::presentation&&scope==SceneTrace::unmatched)){scope=m;label=resource;}};
 if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)consider(b.Transition.pResource);
 else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING){
  if(!b.Aliasing.pResourceBefore||!b.Aliasing.pResourceAfter)scope=SceneTrace::followed;
  else{consider(b.Aliasing.pResourceBefore);consider(b.Aliasing.pResourceAfter);}
 }else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV){
  if(!b.UAV.pResource)scope=SceneTrace::followed;else consider(b.UAV.pResource);
 }
 if(scope==SceneTrace::unmatched)return;
 const bool transition=b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
 SceneTrace::Event e;e.kind=transition?2U:3U;e.qpc=scene_qpc();e.thread=GetCurrentThreadId();
 e.list=reinterpret_cast<std::uintptr_t>(list);e.generation=generation;e.resource=capture->identity.target;e.flags=b.Flags;e.returned=true;
 e.native_frame=frame;e.native_epoch=epoch;
 e.diagnostic=e.presentation_label=scope==SceneTrace::presentation;
 if(e.presentation_label)e.resource=reinterpret_cast<std::uintptr_t>(label);
 if(transition){e.resource=reinterpret_cast<std::uintptr_t>(b.Transition.pResource);e.subresource=b.Transition.Subresource;e.before=b.Transition.StateBefore;e.after=b.Transition.StateAfter;}
 e.args[0]=static_cast<unsigned>(b.Type);(void)scene_trace.event(capture->identity.frame,capture->identity.epoch,e);
}
