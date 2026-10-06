// Included after native owner/binding helpers. The graph follows resource
// identities and native command order; no area, effect name or event enables it.
std::array<std::uint64_t,3> dependency_draws{};
unsigned dependency_rejection{};
unsigned dependency_stage{UINT_MAX};
std::array<unsigned,6> dependency_description{};
DependencyBindingTrace dependency_binding_trace;
bool dependency_binding_contract(const Record &r,const HazeBindings &b){
 if(r.dependency==3){
  constexpr std::array<unsigned,9> words{274433,17055761,2035746,17063986,33845314,52371538,69791842,16826480,52383872};
  if(b.count!=9||b.ranges[0]!=std::array<unsigned short,2>{0,0}||b.ranges[1]!=std::array<unsigned short,2>{9,0}||
     !std::equal(words.begin(),words.end(),b.words[1].begin())||b.indices!=std::array<unsigned,5>{2,3,4,5,6})return false;
  for(unsigned i=0;i<b.count;++i)if(b.modes[i]||!b.objects[i])return false;
  return true;
 }
 const bool copy=r.dependency==1;
 constexpr std::array<unsigned,3> copy_words{1921025,790546,52301856};
 constexpr std::array<unsigned,10> radial_words{663553,17481745,37736481,790578,17985602,35754066,16826464,54517872,69357696,88076432};
 if(b.count!=(copy?3U:10U)||b.ranges[0]!=std::array<unsigned short,2>{0,0}||
    b.ranges[1]!=std::array<unsigned short,2>{static_cast<unsigned short>(copy?3:10),0})return false;
 if(copy?!std::equal(copy_words.begin(),copy_words.end(),b.words[1].begin()):
          !std::equal(radial_words.begin(),radial_words.end(),b.words[1].begin()))return false;
 for(unsigned i=0;i<b.count;++i)if(b.modes[i]||!b.objects[i])return false;
 return b.indices[0]==(copy?1U:3U)&&(copy||(b.indices[1]==4&&b.indices[2]==5));
}
bool dependency_bindings_read(const Record &r,HazeBindings &b){
 return haze_bindings(r.draw.shader_set,r.packet_bindings[2],b,r.dependency==1?1U:r.dependency==3?31U:7U)&&dependency_binding_contract(r,b);
}
BindingCheck dependency_binding_check(const Plan &p,unsigned stage,const HazeBindings *expected=nullptr){
 BindingCheck c;c.observed=true;const auto &r=p.draws[stage];const auto &before=expected?*expected:p.dependency_bindings[stage-4];HazeBindings now{};
 c.readable=haze_bindings(r.draw.shader_set,r.packet_bindings[2],now,r.dependency==1?1U:r.dependency==3?31U:7U,&c.read);
 c.contract=c.readable&&dependency_binding_contract(r,now);c.equal=c.readable&&now==before;
 // Compare initialized field values even when reading stopped early. In that
 // case later zero values are incomplete evidence, not proven mutations.
 c.difference(0,0,before.count,now.count);
 for(unsigned i=0;i<64;++i){c.difference(1,i,before.ids[i],now.ids[i]);c.difference(2,i,before.modes[i],now.modes[i]);c.difference(3,i,before.objects[i],now.objects[i]);}
 for(unsigned s=0;s<2;++s){
  for(unsigned i=0;i<2;++i)c.difference(4,2*s+i,before.ranges[s][i],now.ranges[s][i]);
  for(unsigned i=0;i<8;++i)c.difference(5,8*s+i,before.metadata[s][i],now.metadata[s][i]);
  for(unsigned i=0;i<128;++i)c.difference(6,128*s+i,before.words[s][i],now.words[s][i]);
 }
 for(unsigned i=0;i<5;++i){const auto values=[](const tb::Owner &o){return std::array<std::uint64_t,13>{o.id,o.kind,o.flags,o.width,o.height,o.format,o.gpu_format,o.object,o.type,o.primary,o.secondary,o.backing,o.resource};};
  const auto a=values(before.textures[i]),b=values(now.textures[i]);for(unsigned j=0;j<a.size();++j)c.difference(7,13*i+j,a[j],b[j]);
  c.difference(8,i,before.views[i],now.views[i]);c.difference(9,i,before.descriptors[i],now.descriptors[i]);c.difference(10,i,before.indices[i],now.indices[i]);
 }
 return c;
}
bool dependency_bindings_stable(const Plan &p,unsigned stage){
 const auto &r=p.draws[stage];const auto check=dependency_binding_check(p,stage);
 dependency_binding_trace.draw({GetTickCount64(),p.stamp.frame,p.stamp.epoch,stage,r.dependency,r.draw.packet,r.packet_bindings[2],r.draw.shader_set},r.binding_entry,check);
 return check.accepted();
}
bool dependency_texture(const tb::Owner &o,unsigned role,ComPtr<ID3D12Resource> &out){
 // role 0 scene, 1 copied scene, 2 independent occlusion, 3 independent mask.
 std::uintptr_t method{};for(unsigned slot:{0U,2U,7U,10U})if(!pb::system_method(o.resource,slot,method))return false;
 // Native draw entry owns the current binding. Acquire our reference before
 // inspecting it, then require the owner and binding snapshot to stay stable.
 ComPtr<ID3D12Resource> held=reinterpret_cast<ID3D12Resource *>(o.resource);auto *r=held.Get();ComPtr<ID3D12Device> device;
 if(FAILED(r->GetDevice(IID_PPV_ARGS(&device)))||reinterpret_cast<std::uintptr_t>(device.Get())!=cfg.device)return false;
 const auto d=r->GetDesc();dependency_description={role,o.format,unsigned(d.Format),unsigned(d.Dimension),static_cast<unsigned>(d.Width),d.Height};
 if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.Width!=o.width||d.Height!=o.height||
    !d.Width||d.Width>16384||!d.Height||d.Height>16384||d.DepthOrArraySize!=1||!d.MipLevels||d.MipLevels>16||
    d.SampleDesc.Count!=1||d.SampleDesc.Quality||(d.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))return false;
 if(role<2){
  if(o.format!=(role?6U:34U)||d.Format!=(role?DXGI_FORMAT_R10G10B10A2_UNORM:DXGI_FORMAT_R11G11B10_FLOAT)||
     d.Width!=width||d.Height!=height||d.MipLevels!=1||!(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))return false;
 }else if(role>=4){
  // DOF auxiliaries retain native formats, extents, SRVs and coordinates.
  // Admit only actual normalized/float color storage; no integer, depth or
  // typeless interpretation is inferred from the engine's UINT_MAX field.
  const unsigned slot=role-4;constexpr unsigned engine[]{2,34,34,34,21};
  if(slot>=5||slot==2||o.format!=engine[slot]||d.MipLevels!=1)return false;
  switch(d.Format){
   case DXGI_FORMAT_R32G32B32A32_FLOAT:case DXGI_FORMAT_R16G16B16A16_FLOAT:
   case DXGI_FORMAT_R32G32_FLOAT:case DXGI_FORMAT_R16G16_FLOAT:
   case DXGI_FORMAT_R11G11B10_FLOAT:case DXGI_FORMAT_R32_FLOAT:case DXGI_FORMAT_R16_FLOAT:
   case DXGI_FORMAT_R8_UNORM:case DXGI_FORMAT_R16_UNORM:break;
   default:return false;
  }
  D3D12_FEATURE_DATA_FORMAT_SUPPORT support{d.Format};
  if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)))||
     !(support.Support1&D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE))return false;
 }else{
  // Sampled auxiliary textures keep their native dimensions and coordinates.
  // Only normalized/float-compatible formats are admitted; never integer SRVs.
  if(role==2){if(o.format!=7||(d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))return false;}
  else {
   // The native radial helper binds its format-7 default when no custom mask
   // exists. This is the same normalized texture accepted for occlusion;
   // retain its actual SRV, dimensions and coordinates, including t1/t2 aliasing.
   if(o.format==7){if(d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)return false;}
   else if(o.format!=23)return false;
   switch(d.Format){
    case DXGI_FORMAT_R8_UNORM:case DXGI_FORMAT_R8G8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM:case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_BC1_UNORM:case DXGI_FORMAT_BC1_UNORM_SRGB:case DXGI_FORMAT_BC2_UNORM:case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:case DXGI_FORMAT_BC3_UNORM_SRGB:case DXGI_FORMAT_BC4_UNORM:case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC7_UNORM:case DXGI_FORMAT_BC7_UNORM_SRGB:break;
    default:return false;
   }
  }
  D3D12_FEATURE_DATA_FORMAT_SUPPORT support{d.Format};
  if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)))||
     !(support.Support1&D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE))return false;
 }
 tb::Owner again;if(!tb::read_owner(reader,o.object,again)||again!=o)return false;out=std::move(held);return true;
}
bool add_dependency_resource(Plan &p,const tb::Owner &o,unsigned role,unsigned &index){
 ComPtr<ID3D12Resource> held;if(!dependency_texture(o,role,held))return false;
 for(unsigned i=0;i<resource_count(p);++i)if(p.resources[i]==held){index=i;return true;}
 if(resource_count(p)==resource_capacity)return false;
 index=resource_count(p);p.resources[index]=std::move(held);++p.extra_resources;return true;
}
bool discover_dependencies(Plan &p){
 dependency_rejection=0;dependency_stage=UINT_MAX;
 // Read every consumer before resolving temporary RTV owners: the first draw
 // advertises a scene descriptor but actually targets the second draw's t0.
 for(unsigned i=0;i<p.overlay_count;++i){dependency_stage=4+i;auto &r=p.draws[4+i];r.dependency=0;r.read_count=0;
  dependency_rejection=1;
  if(!source(r)||!pb::snapshot_native_pipeline(cfg.image,r.draw.packet,r.before.pipeline)||
     !field(r.before.pipeline.shader_handle,0,r.draw.shader_set)||!shader(r.shaders[0],r.draw.shader_set,0x50,1)||
     !shader(r.shaders[1],r.draw.shader_set,0x58,0))return false;
  const auto ps=pb::bytes_sha256(r.shaders[1].bytes.data(),r.shaders[1].size),vs=pb::bytes_sha256(r.shaders[0].bytes.data(),r.shaders[0].size);
  if(ps=="be6127ba48008790fd0e93558b8248cb8acd7c0577899bf50d28e5618ae37b79"&&
     vs=="730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd")r.dependency=1;
  else if(ps=="90f3f4d5594357e5350acf3d5bda57db4641b50e6aaa5787d3ae341463860741"&&
     vs=="b980e376575d82610809066f45c6ecd6efb090bbee76968d9ee5203d2459a1d2")r.dependency=2;
  else if(ps=="9f04832cb0a0ff800169964919f7ebecba353e7e3c147c2b314e76af7372e3aa"&&
     vs=="730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd")r.dependency=3;
  // Nonzero-source descriptors are admitted only through a qualified color
  // consumer/producer. A generic overlay cannot silently stand in for one.
  if(r.dependency_source&&r.dependency!=3)return false;
  if(!r.dependency)continue;
  dependency_rejection=2;
  if(!dependency_bindings_read(r,p.dependency_bindings[i]))return false;
 }
 for(unsigned i=0;i<p.overlay_count;++i){dependency_stage=4+i;auto &r=p.draws[4+i];if(!r.dependency)continue;
  dependency_rejection=3;auto &b=p.dependency_bindings[i];r.read_count=r.dependency==1?1:r.dependency==3?5:3;
  if(r.dependency==3&&(!r.dependency_source||b.textures[2].object!=r.dependency_source))return false;
  for(unsigned j=0;j<r.read_count;++j)if(!add_dependency_resource(p,b.textures[j],r.dependency==1?0:r.dependency==3?(j==2?0:j+4):j+1,r.reads[j]))return false;
  dependency_rejection=4;std::uintptr_t view{},backend{},actual{};
  if(!field(r.packet_bindings[0],0,view)||!field(view,0x60,backend)||!field(backend,0x18,actual))return false;
  if(r.dependency==1){
   // Resolve a genuine temporary through a later consumer's retained owner.
   tb::Owner temporary;bool found=false;
   for(unsigned k=0;k<p.overlay_count;++k)if(p.draws[4+k].dependency==2){const auto &candidate=p.dependency_bindings[k].textures[0];
    if(candidate.resource==actual){if(found&&candidate!=temporary)return false;temporary=candidate;found=true;}}
   if(!found||actual==r.output.resource)return false;r.output=temporary;
  }else if(actual!=r.output.resource)return false;
  if(!add_dependency_resource(p,r.output,r.dependency==1?1:0,p.targets[4+i]))return false;
  for(unsigned j=0;j<r.read_count;++j)if(r.reads[j]==p.targets[4+i])return false;
 }
 dependency_rejection=0;dependency_stage=UINT_MAX;return true;
}
bool dependency_order(const Plan &p){
 using namespace color_dependencies;
 for(unsigned start=0;start<p.ordered_count;){const auto base=p.order[start++];if(base>=4)return false;
  auto end=start;while(end<p.ordered_count&&p.order[end]>=4)++end;
  std::array<Draw,8> nodes{};unsigned count{};
  for(unsigned i=start;i<end;++i){const auto stage=p.order[i];const auto &r=p.draws[stage];if(!r.dependency)continue;
   auto &d=nodes[count++];d.output=r.output.resource;d.count=r.read_count;
   for(unsigned j=0;j<r.read_count;++j)d.reads[j]=reinterpret_cast<std::uintptr_t>(p.resources[r.reads[j]].Get());
  }
  if(count){
   std::array<Resource,32> resources{};unsigned n{};const auto scene=p.draws[base].output.resource;
   const auto final=(base==3||(base==0&&!p.has_haze))?p.grade_input_resource:scene;
   const auto add=[&](std::uint64_t id,Domain domain){for(unsigned i=0;i<n;++i)if(resources[i].id==id)return;
    if(n<resources.size())resources[n++]={id,domain};};
   add(scene,Domain::scene);for(unsigned i=0;i<count;++i)add(nodes[i].output,Domain::unknown);
   for(unsigned i=start;i<end;++i){const auto &r=p.draws[p.order[i]];if(!r.dependency)continue;
    for(unsigned j=0;j<r.read_count;++j){const auto id=reinterpret_cast<std::uintptr_t>(p.resources[r.reads[j]].Get());
     // DOF t2 and other consumers' t0 must acquire scene provenance from a
     // preceding producer. The remaining reads retain native auxiliary inputs.
     const bool auxiliary=j!=(r.dependency==3?2U:0U);
     add(id,auxiliary?Domain::auxiliary:Domain::unknown);
     if(auxiliary){for(unsigned k=0;k<count;++k)if(nodes[k].output==id)return false;}
    }
   }
   color_dependencies::Plan graph;if(!build(std::span(resources).first(n),std::span(nodes).first(count),final,graph))return false;
   unsigned step{};for(unsigned i=start;i<end;++i){const auto &r=p.draws[p.order[i]];if(!r.dependency)continue;
    if(graph.steps[step++].inputs[r.dependency==3?2:0].domain!=Domain::scene)return false;
   }
  }
  start=end;
 }
 return true;
}
#include "dependency_refresh.inl"
