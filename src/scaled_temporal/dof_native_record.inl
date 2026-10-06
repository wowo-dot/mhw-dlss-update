struct DofScope {unsigned slot{UINT_MAX},pass{},calls{};std::uintptr_t state{},list{};std::uint64_t generation{};};
thread_local DofScope dof_scope;
bool dof_active(){for(const auto &p:plans)if(p.prepared&&p.dof&&p.dof->returned!=63)return true;return false;}
void dof_before(std::uintptr_t executor,std::uintptr_t context,std::uintptr_t packet){
 if(fault.load()||abandoned)return;
 unsigned selected=UINT_MAX,pass{};
 for(unsigned i=0;i<plans.size();++i){const auto &p=plans[i];if(!p.built||!p.dof)continue;
  for(unsigned j=0;j<6;++j)if(!(p.dof->entered&(1U<<j))&&p.dof->passes[j].packet==packet){
   if(selected!=UINT_MAX){fail(14);return;}selected=i;pass=j;}}
 if(selected==UINT_MAX)return;
 auto &p=plans[selected];p.dof->entered|=1U<<pass;
 std::uintptr_t backend{},state{},list{};temporal_present::Bindings bindings;
 if(dof_scope.slot!=UINT_MAX||!field(context,8,backend)||backend!=cfg.backend||
    !field(executor,8,state)||!field(state,0x20,list)||!list||
    !obs::inspect(reinterpret_cast<ID3D12GraphicsCommandList *>(list),bindings)||!bindings.known||!bindings.valid||!bindings.generation||
    !dof_stable(p.dof->passes[pass])){dof_stats.reason=20;dof_stats.pass=pass;if(p.prepared)fail(14);return;}
 dof_scope={selected,pass,0,state,list,bindings.generation};
}
bool dof_dispatch(ID3D12GraphicsCommandList *list,unsigned x,unsigned y,unsigned z,std::uintptr_t caller)noexcept{
 if(dof_scope.slot==UINT_MAX)return false;Lock lock;
 auto &p=plans[dof_scope.slot];auto &d=*p.dof;const auto pass=dof_scope.pass;const auto &recipe=d.passes[pass];
 unsigned step=21;bool replaced=false;
 try{
  temporal_present::Bindings roots;std::uintptr_t packet{};obs::Predicate predicate;
  if(++dof_scope.calls!=1||caller!=cfg.image+0x259cfca||reinterpret_cast<std::uintptr_t>(list)!=dof_scope.list||
     !field(dof_scope.state,0x158,packet)||packet!=recipe.packet||!obs::inspect(list,roots)||!roots.ready()||
     roots.generation!=dof_scope.generation||std::array<unsigned,3>{x,y,z}!=std::array<unsigned,3>{recipe.prefix[6],recipe.prefix[7],recipe.prefix[8]}||
     !dof_stable(recipe)||!obs::predicate_current(list,predicate)||!predicate.known||predicate.buffer)throw std::runtime_error("DOF native dispatch identity");
  step=22;const bool adapt=pass==0||pass==3;unsigned cache=UINT_MAX;
  if(adapt){
   cache=dof_cache(pass,recipe.shader_set);
   if(cache==UINT_MAX){
    for(unsigned i=0;i<dof_pipelines.size();++i)if(!dof_pipelines[i].pipeline){cache=i;break;}
    if(cache==UINT_MAX)throw std::runtime_error("DOF pipeline capacity");
    auto &v=dof_pipelines[cache];const auto shader=make_dof_compute_shader(std::as_bytes(std::span(recipe.shader.bytes).first(recipe.shader.size)),pass);
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};desc.pRootSignature=reinterpret_cast<ID3D12RootSignature *>(roots.compute.signature);desc.CS={shader.data(),shader.size()};
    auto *device=reinterpret_cast<ID3D12Device *>(cfg.device);
    if(FAILED(device->CreateComputePipelineState(&desc,IID_PPV_ARGS(&v.pipeline))))throw std::runtime_error("DOF adapted pipeline");
    v.root=desc.pRootSignature;v.set=recipe.shader_set;v.pass=pass;
   }
   if(dof_pipelines[cache].root.Get()!=reinterpret_cast<ID3D12RootSignature *>(roots.compute.signature))throw std::runtime_error("DOF native root changed");
  }
  if(!p.prepared){++dof_stats.warmups;return false;}
  step=23;auto *track=track_recording(p,list,roots.generation,true);if(!track)throw std::runtime_error("DOF recording capacity");
  // The unmodified native dispatch itself establishes the view contract.
  // Unknown local state may be inherited; contradictory local state rejects.
  // No barriers or resource states are changed by this route.
  for(const auto role:DofNativeSequence::reads[pass])if(role>=0){const auto i=d.resources[role];
   if(track->known[i]&&!(track->states[i]&D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))throw std::runtime_error("DOF native read state");}
  for(const auto role:DofNativeSequence::writes[pass])if(role>=0){const auto i=d.resources[role];
   if(track->known[i]&&track->states[i]!=D3D12_RESOURCE_STATE_UNORDERED_ACCESS)throw std::runtime_error("DOF native write state");}
  step=24;std::array<IUnknown *,5> pins{};unsigned n{};
  pins[n++]=reinterpret_cast<ID3D12RootSignature *>(roots.compute.signature);pins[n++]=reinterpret_cast<ID3D12PipelineState *>(roots.pipeline);
  for(auto heap:roots.heaps)if(heap)pins[n++]=reinterpret_cast<ID3D12DescriptorHeap *>(heap);
  if(adapt)pins[n++]=dof_pipelines[cache].pipeline.Get();
  if(!retain_recording(p,list,roots.generation,std::span(pins).first(n))||
     p.handoff.record(handoff_compute(p,pass),recipe.packet,{reinterpret_cast<std::uintptr_t>(list),roots.generation})!=FrameHandoff::Result::ok)
   throw std::runtime_error("DOF recording retention/order");
  step=25;
  if(adapt){
   injecting=true;
   {PredicateScope choice(list);choice.native(p.selector.Get());list->Dispatch(x,y,z);
    choice.full(p.selector.Get());list->SetPipelineState(dof_pipelines[cache].pipeline.Get());list->Dispatch(x,y,z);
    list->SetPipelineState(reinterpret_cast<ID3D12PipelineState *>(roots.pipeline));}
   injecting=false;replaced=true;
   if(!obs::matches_current(list,roots)||!obs::predicate_current(list,predicate)||predicate.buffer)throw std::runtime_error("DOF binding restore");
   ++dof_stats.adapted;++dof_stats.restored;
  }
  d.dispatched|=1U<<pass;++dof_stats.dispatches;++dof_stats.passes[pass];completed(p);
 }catch(const std::exception &e){injecting=false;dof_stats.reason=step;dof_stats.pass=pass;failure=e.what();if(p.prepared)fail(14);}
 return replaced;
}
void dof_after(bool returned){
 if(dof_scope.slot==UINT_MAX)return;auto &p=plans[dof_scope.slot];const auto pass=dof_scope.pass;
 if(!returned||dof_scope.calls!=1){dof_stats.reason=26;dof_stats.pass=pass;if(p.prepared)fail(14);}
 else {p.dof->returned|=1U<<pass;if(p.prepared&&p.dof->returned==63)++dof_stats.completed;}
 dof_scope={};
}
