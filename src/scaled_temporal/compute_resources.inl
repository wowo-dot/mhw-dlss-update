// Included in stream_route's anonymous namespace after owner/stamp helpers.
// Native prepared queues own these textures. Inspect synchronously, AddRef
// before GetDesc, then recheck the captured owner/recipe. Diagnostic refs are
// held until all route callbacks are disabled; no extra GPU work is recorded.
struct ComputeGpuSample {
 struct Resource {OutputDependencies::Texture owner;std::array<std::uint64_t,12> desc{};ComPtr<ID3D12Resource> hold;};
 struct Pass {
  unsigned error{},calls{};bool entered{},returned{};
  // Entry tick, list, Reset generation, native recording context, API tick.
  std::array<std::uint64_t,5> at{};
  std::array<std::uint64_t,2> pipeline{}; // compute root and PSO from API observer
  // Failure step, backend, recording, list, inspected, known, valid,
  // generation, renderer frame. Zero step means no entry rejection.
  std::array<std::uint64_t,9> entry{};
  std::array<unsigned,2> recipe{}; // complete reread, difference mask
  unsigned stable_difference{}; // excludes only scheduling list at packet +8
  std::array<unsigned,12> observed_prefix{};
  std::array<std::array<unsigned,2>,16> states{};
  unsigned use_count{};std::array<std::array<unsigned,2>,16> uses{}; // word, resource index
 };
 unsigned error{},count{};std::array<Resource,16> resources;std::array<Pass,6> passes;
};
std::array<ComputeGpuSample,2> compute_gpu_samples;
unsigned compute_gpu_count{};
std::atomic<bool> compute_gpu_pending{};
ComputeResourceTrace compute_gpu_states;
OutputDependencies::Compute compute_gpu_scratch;
thread_local unsigned compute_gpu_scope=UINT_MAX;
bool compute_recipe_stable(const OutputDependencies::Compute &d){
 return OutputDependencies::compute_matches(d,compute_gpu_scratch,[](std::uint64_t at,void *out,std::size_t n){
  return pb::copy_memory(static_cast<std::uintptr_t>(at),out,static_cast<unsigned>(n));});
}
bool compute_recipe_observed(const OutputDependencies::Compute &d,ComputeGpuSample::Pass &pass){
 const bool read=OutputDependencies::compute_reread(d,compute_gpu_scratch,[](std::uint64_t at,void *out,std::size_t n){
  return pb::copy_memory(static_cast<std::uintptr_t>(at),out,static_cast<unsigned>(n));});
 pass.recipe={read?1U:0U,read?OutputDependencies::compute_difference(d,compute_gpu_scratch):0U};
 pass.stable_difference=read?OutputDependencies::compute_recording_difference(d,compute_gpu_scratch):0U;
 if(read)pass.observed_prefix=compute_gpu_scratch.prefix;
 return read&&!pass.stable_difference;
}
bool compute_texture_stable(const OutputDependencies::Texture &t){
 const auto &v=t.fields;std::uint64_t primary{},secondary{},resource{};unsigned extent{},engine{},format{};
 return field(v[0],0x60,primary)&&primary==v[1]&&field(v[0],0xf0,secondary)&&secondary==v[2]&&
  field(secondary?secondary:primary,0x20,resource)&&resource==v[3]&&field(v[0],0x88,extent)&&extent==v[4]&&
  field(v[0],0x90,engine)&&engine==v[5]&&field(v[0],0x94,format)&&format==v[6];
}
// SEH boundary contains no destructible C++ locals. The caller immediately
// adopts the successful reference. Failure releases acquired references here.
bool compute_describe(std::uintptr_t candidate,ID3D12Resource **out,D3D12_RESOURCE_DESC *desc,unsigned *planes) noexcept {
 ID3D12Resource *held{};ID3D12Device *device{};bool ok=false;
 __try {
  auto *r=reinterpret_cast<ID3D12Resource *>(candidate);
  if(SUCCEEDED(r->QueryInterface(IID_PPV_ARGS(&held)))&&held==r&&
     SUCCEEDED(held->GetDevice(IID_PPV_ARGS(&device)))&&reinterpret_cast<std::uintptr_t>(device)==cfg.device){
   *desc=held->GetDesc();D3D12_FEATURE_DATA_FORMAT_INFO info{desc->Format,0};
   ok=SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO,&info,sizeof(info)));
   if(ok)*planes=info.PlaneCount;
  }
 }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
 if(device)device->Release();if(!ok&&held){held->Release();held=nullptr;}*out=held;return ok;
}
bool compute_gpu_any_pending(){
 for(unsigned s=0;s<compute_gpu_count;++s)if(!compute_gpu_samples[s].error)
  for(const auto &p:compute_gpu_samples[s].passes)if(!p.returned&&!p.error)return true;
 return false;
}
void compute_gpu_capture(){
 if(compute_gpu_count==compute_dependencies.size())return;
 while(compute_gpu_count<compute_dependencies.size()){
  const auto index=compute_gpu_count++;auto &q=compute_gpu_samples[index];const auto &s=compute_dependencies.sample(index);
  if(s.command_count!=8||s.compute_count!=6||s.count!=1){q.error=1;continue;}
  bool ok=true;
  for(unsigned p=0;p<6&&ok;++p){const auto &d=s.computes[p];auto &pass=q.passes[p];
   if((s.commands[p+1][2]&31)!=6||s.commands[p+1][3]!=p||!compute_recipe_stable(d)){ok=false;break;}
   for(unsigned j=0;j<d.bindings.range[0];++j){const auto word=d.bindings.words[j],kind=word&15,owner=(word>>4)&255;
    if(kind!=2&&kind!=3)continue;
    if(pass.use_count==pass.uses.size()){ok=false;break;}
    const auto &t=d.textures[owner];unsigned r{};
    for(;r<q.count;++r)if(q.resources[r].owner.fields[3]==t.fields[3])break;
    if(r==q.count){
     if(q.count==q.resources.size()||!compute_texture_stable(t)){ok=false;break;}
     auto &v=q.resources[q.count++];v.owner=t;std::uintptr_t method{};
     for(unsigned slot:{0U,1U,2U,7U,10U})if(!pb::system_method(t.fields[3],slot,method))ok=false;
     if(!ok)break;
     D3D12_RESOURCE_DESC desc{};unsigned planes{};ID3D12Resource *held{};
     if(!compute_describe(t.fields[3],&held,&desc,&planes)){ok=false;break;}v.hold.Attach(held);
     v.desc={unsigned(desc.Dimension),desc.Alignment,desc.Width,desc.Height,desc.DepthOrArraySize,desc.MipLevels,
      unsigned(desc.Format),desc.SampleDesc.Count,desc.SampleDesc.Quality,unsigned(desc.Layout),unsigned(desc.Flags),planes};
     if(!compute_texture_stable(t)||desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||!desc.Width||desc.Width>16384||
        !desc.Height||desc.Height>16384||desc.DepthOrArraySize!=1||!desc.MipLevels||desc.MipLevels>16||
        desc.SampleDesc.Count!=1||desc.SampleDesc.Quality||!planes||!compute_gpu_states.watch(t.fields[3])){ok=false;break;}
    }
    if(!compute_texture_stable(t)){ok=false;break;}
    pass.uses[pass.use_count++]={word,r};
   }
   if(ok&&!compute_recipe_stable(d))ok=false;
  }
  if(!ok)q.error=2;
 }
 compute_gpu_pending.store(compute_gpu_any_pending());
}
bool compute_gpu_complete(){
 if(compute_gpu_count!=2)return false;
 for(const auto &s:compute_gpu_samples)if(!s.error)for(const auto &p:s.passes)if(!p.returned&&!p.error)return false;
 return true;
}
void compute_gpu_barrier(ID3D12GraphicsCommandList *list,std::uint64_t generation,const D3D12_RESOURCE_BARRIER &b){
 if(!compute_gpu_pending.load()||compute_gpu_states.stopped()||compute_gpu_complete())return;
 for(unsigned s=0;s<compute_gpu_count;++s)for(unsigned r=0;r<compute_gpu_samples[s].count;++r){
  bool pending_pass=false;for(const auto &p:compute_gpu_samples[s].passes)if(!p.returned&&!p.error)pending_pass=true;
  if(compute_gpu_samples[s].error||!pending_pass)continue;
  const auto &v=compute_gpu_samples[s].resources[r];const auto id=v.owner.fields[3];
  // Deduplicate resources shared by the two captured frames.
  bool duplicate=false;for(unsigned prior=0;prior<s;++prior){bool active_prior=false;
   for(const auto &p:compute_gpu_samples[prior].passes)if(!p.returned&&!p.error)active_prior=true;
   if(compute_gpu_samples[prior].error||!active_prior)continue;
   for(unsigned k=0;k<compute_gpu_samples[prior].count;++k)if(compute_gpu_samples[prior].resources[k].owner.fields[3]==id)duplicate=true;
  }
  if(duplicate||!v.hold)continue;
  const bool transition=b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION&&reinterpret_cast<std::uintptr_t>(b.Transition.pResource)==id;
  const bool alias=b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING&&(!b.Aliasing.pResourceBefore||!b.Aliasing.pResourceAfter||
   reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore)==id||reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter)==id);
  if(!transition&&!alias)continue;
  const bool full=transition&&!b.Flags&&(b.Transition.Subresource==UINT_MAX||(b.Transition.Subresource==0&&v.desc[4]==1&&v.desc[5]==1&&v.desc[11]==1));
  compute_gpu_states.observe({reinterpret_cast<std::uintptr_t>(list),generation,id,unsigned(b.Type),unsigned(b.Flags),
   transition?b.Transition.Subresource:UINT_MAX,transition?unsigned(b.Transition.StateBefore):0,transition?unsigned(b.Transition.StateAfter):0,0},full);
 }
}
void compute_gpu_write(std::ostream &o){
 const auto a=[&](const auto &v){o<<'[';bool comma=false;for(auto x:v){if(comma)o<<',';o<<+x;comma=true;}o<<']';};
 o<<"{\"schema\":3,\"policy\":\"native-compute-stable-recipe-v3\",\"diagnostic_only\":true,\"gpu_work_added\":false,\"pixel_footprint_verified\":false,\"descriptor_contents_verified\":false,\"lost\":"<<compute_gpu_states.lost()<<",\"stopped\":"<<compute_gpu_states.stopped()<<",\"samples\":[";
 for(unsigned i=0;i<compute_gpu_count;++i){if(i)o<<',';const auto &s=compute_gpu_samples[i];const auto &cpu=compute_dependencies.sample(i);
  o<<"{\"at\":";a(cpu.at);o<<",\"error\":"<<s.error<<",\"resources\":[";
  for(unsigned j=0;j<s.count;++j){if(j)o<<',';const auto &r=s.resources[j];o<<"{\"owner\":";a(r.owner.fields);o<<",\"description\":";a(r.desc);o<<",\"held\":"<<bool(r.hold)<<'}';}o<<"],\"passes\":[";
  for(unsigned j=0;j<6;++j){if(j)o<<',';const auto &p=s.passes[j];o<<"{\"packet\":"<<cpu.computes[j].packet<<",\"error\":"<<p.error<<",\"entered\":"<<p.entered<<",\"returned\":"<<p.returned<<",\"calls\":"<<p.calls<<",\"at\":";a(p.at);o<<",\"pipeline\":";a(p.pipeline);o<<",\"entry\":";a(p.entry);o<<",\"recipe\":";a(p.recipe);o<<",\"observed_prefix\":";a(p.observed_prefix);o<<",\"uses\":[";
   for(unsigned k=0;k<p.use_count;++k){if(k)o<<',';a(p.uses[k]);}o<<"],\"states\":[";for(unsigned k=0;k<p.use_count;++k){if(k)o<<',';a(p.states[k]);}o<<"],\"stable_difference\":"<<p.stable_difference<<'}';
  }o<<"]}";
 }o<<"],\"barriers\":[";bool comma=false;
 for(const auto &e:compute_gpu_states.events()){if(comma)o<<',';a(std::array<std::uint64_t,9>{e.list,e.generation,e.resource,e.kind,e.flags,e.subresource,e.before,e.after,e.result});comma=true;}o<<"]}";
}
