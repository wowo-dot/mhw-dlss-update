// Native resource/view preserving route. Included under the route's lock.
OutputDependencies::Compute dof_scratch;
struct DofPipeline {unsigned pass{};std::uintptr_t set{};ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pipeline;};
std::array<DofPipeline,16> dof_pipelines;
struct DofCounters {
 std::uint64_t qualified{},warmups{},dispatches{},adapted{},completed{},rejected{},restored{};
 std::array<std::uint64_t,6> passes{};
 unsigned reason{},pass{};
} dof_stats;
bool dof_read(OutputDependencies::Compute &d){
 if(!OutputDependencies::compute_reread(d,dof_scratch,[](std::uint64_t at,void *out,std::size_t size){
  return pb::copy_memory(static_cast<std::uintptr_t>(at),out,static_cast<unsigned>(size));}))return false;
 d=dof_scratch;return true;
}
bool dof_stable(const OutputDependencies::Compute &d){
 return OutputDependencies::compute_reread(d,dof_scratch,[](std::uint64_t at,void *out,std::size_t size){
  return pb::copy_memory(static_cast<std::uintptr_t>(at),out,static_cast<unsigned>(size));})&&
  !OutputDependencies::compute_recording_difference(d,dof_scratch);
}
unsigned dof_cache(unsigned pass,std::uintptr_t set){
 for(unsigned i=0;i<dof_pipelines.size();++i)if(dof_pipelines[i].pipeline&&dof_pipelines[i].pass==pass&&dof_pipelines[i].set==set)return i;
 return UINT_MAX;
}
bool dof_warm(Plan &p){
 for(unsigned i=0;i<2;++i){const auto pass=i?3U:0U;p.dof->cache[i]=dof_cache(pass,p.dof->passes[pass].shader_set);
  if(p.dof->cache[i]==UINT_MAX)return false;}
 return true;
}
bool dof_qualify(Plan &p){
 auto &d=*p.dof;
 const auto reject=[&](unsigned reason){++dof_stats.rejected;dof_stats.reason=reason;return false;};
 if(d.count!=6)return reject(1);
 unsigned matching{};
 for(unsigned i=0;i<p.overlay_count;++i)if(p.keys[4+i]==d.key){
  if(p.draws[4+i].dependency!=3)return reject(2);d.stage=4+i;++matching;}
 if(matching!=1)return reject(3);
 const auto &b=p.dependency_bindings[d.stage-4];std::array<std::uint64_t,5> composite{};
 for(unsigned i=0;i<5;++i)composite[i]=b.textures[i].object;
 if(!d.qualify(p.draws[d.stage].dependency_source,composite))return reject(4);
 std::uintptr_t queue{},next{};unsigned key{};
 if(!field(p.stamp.context,0x2b78,queue)||!field(queue,static_cast<unsigned>(16*(d.passes[5].index+1)),key)||key!=d.key||
    !field(queue,static_cast<unsigned>(16*(d.passes[5].index+1)+8),next)||next!=p.draws[d.stage].draw.packet)return reject(5);
 for(unsigned i=0;i<9;++i){const auto &t=d.textures[i];const auto &v=t.fields;std::uintptr_t method{};
  if(!compute_texture_stable(t))return reject(6);
  for(unsigned slot:{0U,1U,2U,7U,10U})if(!pb::system_method(v[3],slot,method))return reject(7);
  ID3D12Resource *raw{};D3D12_RESOURCE_DESC desc{};unsigned planes{};
  if(!compute_describe(v[3],&raw,&desc,&planes))return reject(8);ComPtr<ID3D12Resource> held;held.Attach(raw);
  if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.Width!=DofNativeSequence::sizes[i][0]||
     desc.Height!=DofNativeSequence::sizes[i][1]||unsigned(desc.Format)!=DofNativeSequence::formats[i]||desc.MipLevels!=1||
     desc.DepthOrArraySize!=1||desc.SampleDesc.Count!=1||desc.SampleDesc.Quality||planes!=1||
     (desc.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)||
     (i>=2&&!(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))||!compute_texture_stable(t))return reject(9);
  unsigned index{};for(;index<resource_count(p);++index)if(p.resources[index]==held)break;
  if(index==resource_count(p)){if(index==resource_capacity)return reject(10);p.resources[index]=std::move(held);++p.extra_resources;}
  d.resources[i]=index;
 }
 for(const auto &pass:d.passes)if(!dof_stable(pass))return reject(11);
 ++dof_stats.qualified;return true;
}
void dof_write(std::ostream &o){
 o<<"{\"schema\":1,\"policy\":\"native-six-compute-preserved-views-v1\",\"qualified\":"<<dof_stats.qualified
  <<",\"warmups\":"<<dof_stats.warmups<<",\"dispatches\":"<<dof_stats.dispatches<<",\"adapted\":"<<dof_stats.adapted
  <<",\"completed\":"<<dof_stats.completed<<",\"rejected\":"<<dof_stats.rejected<<",\"restored\":"<<dof_stats.restored
  <<",\"reason\":"<<dof_stats.reason<<",\"pass\":"<<dof_stats.pass<<",\"passes\":[";
 for(unsigned i=0;i<6;++i){if(i)o<<',';o<<dof_stats.passes[i];}o<<"]}";
}
