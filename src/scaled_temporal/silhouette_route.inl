// All access serialized by the route lock. No native bytecode is modified.
std::array<std::shared_ptr<SilhouetteCpu>,32> silhouette_cpu_cache;
struct SilhouettePipeline {
 std::shared_ptr<SilhouetteCpu> recipe;std::unique_ptr<SilhouetteDraw> draw;
};
std::array<SilhouettePipeline,32> silhouette_pipelines;
std::uint64_t silhouette_recorded{},silhouette_native{},silhouette_corrected{},silhouette_restored{};
std::array<std::uint64_t,12> silhouette_rejections{};
unsigned silhouette_cache_count{};
bool silhouette_code_stable(const SilhouetteCpu &r){
 for(unsigned i=0;i<2;++i){const auto &s=r.identity[i];std::uintptr_t entry{},desc{},data{};unsigned size{};
  if(!field(r.set,0x50+8*i,entry)||entry!=s.entry||!field(entry,0x10,desc)||desc!=s.descriptor||
     !field(desc,0,size)||size!=s.size||!field(desc,8,data)||data!=s.data)return false;
  std::array<std::byte,1024> block{};
  for(unsigned at=0;at<size;at+=1024){const auto n=(std::min)(1024U,size-at);
   if(!pb::copy_memory(data+at,block.data(),n)||std::memcmp(block.data(),r.bytes[i].data()+at,n))return false;}
 }return true;
}
bool silhouette_recipe_equal(pb::NativePipeline a,pb::NativePipeline b){a.render=b.render=0;a.graphics=b.graphics=0;return a==b;}
std::shared_ptr<SilhouetteCpu> silhouette_cpu(std::uintptr_t packet){
 pb::NativePipeline native;std::uintptr_t set{};
 if(!pb::snapshot_native_pipeline(cfg.image,packet,native)||!field(native.shader_handle,0,set)||!set||native.input_count>32)return {};
 for(const auto &cached:silhouette_cpu_cache)if(cached&&cached->set==set&&silhouette_recipe_equal(cached->native,native)){
  if(!layout(cached->layout,true)||!silhouette_code_stable(*cached))return {};return cached;}
 unsigned empty=UINT_MAX;for(unsigned i=0;i<silhouette_cpu_cache.size();++i)if(!silhouette_cpu_cache[i]){empty=i;break;}
 if(empty==UINT_MAX)return {};
 auto r=std::make_shared<SilhouetteCpu>();r->native=native;r->set=set;r->layout.before.pipeline=native;
 if(!layout(r->layout,false))return {};
 for(unsigned i=0;i<2;++i){auto &s=r->identity[i];
  if(!field(set,0x50+8*i,s.entry)||!field(s.entry,0x10,s.descriptor)||!field(s.descriptor,0,s.size)||s.size<52||s.size>131072||!field(s.descriptor,8,s.data))return {};
  r->bytes[i].resize(s.size);if(!pb::copy_memory(s.data,r->bytes[i].data(),s.size)||!pb::dxbc_valid(r->bytes[i].data(),s.size))return {};
 }
 if(!known_silhouette_shader(r->bytes[1]))return {};
 silhouette_cpu_cache[empty]=r;return r;
}
IndexedScope indexed_before(const pb::Frame &f) noexcept {
 auto scope=indexed_diagnostic_before(f);
 try{Lock lock;if(fault.load()||abandoned)return scope;
  unsigned slot=UINT_MAX,position{};
  for(unsigned i=0;i<plans.size();++i){const auto &p=plans[i];if(!p.built)continue;
   for(unsigned j=0;j<p.late_count;++j)if(!(p.late_executed&(1U<<j))&&p.late_packets[j]==f.gpr[3]){
    if(slot!=UINT_MAX){fail(14);return scope;}slot=i;position=j;}}
  if(slot==UINT_MAX)return scope;
  auto &p=plans[slot];const auto &r=p.late[position];p.late_executed|=1U<<position;
  if(!p.prepared)return scope;
  scope.slot=slot;scope.position=position;scope.calls=0;scope.replaced=false;
  scope.draw.packet=f.gpr[3];scope.draw.executor=f.gpr[1];scope.draw.context=f.gpr[2];scope.draw.graphics=r.bindings[2];
  if(r.recipe)scope.draw.shader_set=r.recipe->set;
  if(!field(f.gpr[1],8,scope.state)||!field(scope.state,0x20,scope.draw.command))fail(14);
 }catch(...){Lock lock;fail(14);}return scope;
}
bool silhouette_bindings(const LatePacket &r,std::array<unsigned,28> &screen,std::array<unsigned,5> &material,
                         std::array<ComPtr<ID3D12Resource>,2> &textures){
 const auto set=r.recipe->set,graphics=r.bindings[2];std::uintptr_t words{},ids{},modes{};unsigned count{};std::array<unsigned short,2> range{};
 if(!field(set,0,words)||!field(set,0x80,ids)||!field(set,0x88,count)||count>64||!field(graphics,0x10,modes)||
    !field(set,0x14,range)||range[0]>128||unsigned(range[0])+range[1]>4096)return false;
 std::array<unsigned,128> binding{};unsigned found{};
 if(!pb::copy_memory(words+4ULL*range[1],binding.data(),4*range[0]))return false;
 for(unsigned i=0;i<range[0];++i){const auto w=binding[i],kind=w&15,index=(w>>4)&255,reg=w>>24;
  const unsigned wanted=kind==0&&reg==1?0:kind==0&&reg==5?1:kind==2&&reg==0?2:kind==2&&reg==18?3:UINT_MAX;
  if(wanted==UINT_MAX)continue;unsigned id{};unsigned char mode{};std::uintptr_t object{};
  constexpr unsigned expected[]{12,433,159,41};
  if(index>=count||(found&(1U<<wanted))||!field(ids,index*4,id)||id!=expected[wanted]||!field(modes,index,mode)||mode||
     !field(graphics,0x30+8*index,object)||!object)return false;
  found|=1U<<wanted;
  if(wanted<2){if(wanted==0?!pb::copy_memory(object,screen.data(),sizeof(screen)):!pb::copy_memory(object,material.data(),sizeof(material)))return false;}
  else{tb::Owner owner;if(!tb::read_owner(reader,object,owner)||!indexed_description(owner.resource)[1])return false;
   textures[wanted-2]=reinterpret_cast<ID3D12Resource *>(owner.resource);}
 }
 return found==15;
}
bool silhouette_buffers(const LatePacket &r,const obs::RasterBindings &b,std::array<ComPtr<ID3D12Resource>,2> &buffers){
 if(!b.ready()||b.vertex_mask!=1||!b.index_known||b.topology!=D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST||b.index_buffer.Format!=DXGI_FORMAT_R16_UINT)return false;
 for(unsigned i=0;i<2;++i){std::uintptr_t owner{},backend{},resource{},address{};unsigned bytes{},offset{};
  const auto ia=r.bindings[1];unsigned char stride{};
  if(!field(ia,i?0x38:0,owner)||!field(owner,0x60,backend)||!field(backend,0x20,resource)||!field(backend,0x40,address)||
     !field(backend,0x10,bytes)||!field(ia,i?0x40:0x24,offset)||offset>bytes||address>UINTPTR_MAX-offset)return false;
  const auto description=indexed_description(resource);
  if(!description[1]||description[2]!=D3D12_RESOURCE_DIMENSION_BUFFER||description[3]<bytes||description[13]!=address)return false;
  if(i){if(b.index_buffer.BufferLocation!=address+offset||b.index_buffer.SizeInBytes!=bytes-offset||
      (UINT64(r.arguments[0])+r.arguments[1])*2>b.index_buffer.SizeInBytes)return false;}
  else{if(!field(ia,0x20,stride)||(stride!=32&&stride!=36&&stride!=40)||b.vertex_buffer.BufferLocation!=address+offset||
      b.vertex_buffer.SizeInBytes!=bytes-offset||b.vertex_buffer.StrideInBytes!=stride)return false;}
  buffers[i]=reinterpret_cast<ID3D12Resource *>(resource);
 }return true;
}
SilhouettePipeline *silhouette_pipeline(const std::shared_ptr<SilhouetteCpu> &r,ID3D12RootSignature *root){
 for(auto &p:silhouette_pipelines)if(p.draw&&p.recipe==r&&p.draw->root()==root)return &p;
 SilhouettePipeline *slot{};for(auto &p:silhouette_pipelines)if(!p.draw){slot=&p;break;}if(!slot)return nullptr;
 auto &layout_record=r->layout;auto input=layout_record.elements;
 for(unsigned i=0;i<r->native.input_count;++i)input[i].SemanticName=layout_record.semantics[i].data();
 SilhouetteRecipe recipe;recipe.geometry={render_w,render_h,width,height};auto &d=recipe.native;
 d.pRootSignature=root;d.VS={r->bytes[0].data(),r->bytes[0].size()};d.PS={r->bytes[1].data(),r->bytes[1].size()};
 std::memcpy(&d.BlendState,r->native.blend.data(),sizeof(d.BlendState));std::memcpy(&d.RasterizerState,r->native.raster.data(),sizeof(d.RasterizerState));
 std::memcpy(&d.DepthStencilState,r->native.depth.data(),sizeof(d.DepthStencilState));
 d.InputLayout={input.data(),r->native.input_count};d.SampleMask=UINT_MAX;d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
 d.NumRenderTargets=1;d.RTVFormats[0]=DXGI_FORMAT_R11G11B10_FLOAT;d.SampleDesc.Count=1;
 slot->draw=std::make_unique<SilhouetteDraw>(reinterpret_cast<ID3D12Device *>(cfg.device),recipe);slot->recipe=r;++silhouette_cache_count;return slot;
}
bool record_indexed(ID3D12GraphicsCommandList *list,unsigned n,unsigned instances,unsigned first,int base,unsigned instance,std::uintptr_t caller){
 auto &scope=indexed_scope;auto &p=plans[scope.slot];auto &r=p.late[scope.position];
 std::array<std::uintptr_t,4> bindings{};std::array<unsigned,3> arguments{};temporal_present::Bindings roots;
 std::uintptr_t packet{},root{},pipeline{},command{};
 if(!p.prepared||++scope.calls!=1||caller!=cfg.image+0x259d806||instances!=1||instance||reinterpret_cast<std::uintptr_t>(list)!=scope.draw.command||
    !field(p.late_packets[scope.position],0x10,bindings)||bindings!=r.bindings||!field(p.late_packets[scope.position],0x40,arguments)||arguments!=r.arguments||
    arguments!=std::array<unsigned,3>{n,first,std::bit_cast<unsigned>(base)}||!obs::inspect(list,roots)||!roots.known||!roots.valid||
    !field(scope.state,0x158,packet)||packet!=scope.draw.packet||!field(scope.state,0x20,command)||command!=scope.draw.command||
    !field(scope.state,0x110,root)||root!=roots.graphics.signature||!field(scope.state,0x108,pipeline)||pipeline!=roots.pipeline)
  throw std::runtime_error("silhouette indexed packet identity");
 // Even unadapted candidates participate in GPU order and selector lifetime.
 if(!track_recording(p,list,roots.generation,true)||!retain_recording(p,list,roots.generation)||p.handoff.record(handoff_indexed(p,scope.position),scope.draw.packet,
    {reinterpret_cast<std::uintptr_t>(list),roots.generation})!=FrameHandoff::Result::ok)throw std::runtime_error("silhouette recording ownership");
 ++silhouette_recorded;++p.late_recorded;
 const auto native=[&](unsigned reason){++silhouette_native;++silhouette_rejections[reason];completed(p);return false;};
 if(!r.recipe||!n)return native(0);
 pb::AttachmentSnapshot now;obs::RasterBindings raster;
 if(!pb::snapshot_attachments(cfg.image,scope.draw,now)||!pb::attachment_cache_matches(now)||now.pipeline!=r.native||
    now.color_count!=1||now.views[8].owner||now.views[0].resource!=reinterpret_cast<std::uintptr_t>(p.resources[3].Get())||
    now.views[0].format!=DXGI_FORMAT_R11G11B10_FLOAT||!obs::raster_current(list,raster)||!raster.output_known||
    raster.render_target!=now.views[0].handle||raster.generation!=roots.generation)return native(1);
 D3D12_BLEND_DESC blend{};D3D12_DEPTH_STENCIL_DESC depth{};D3D12_RASTERIZER_DESC rs{};
 std::memcpy(&blend,now.pipeline.blend.data(),sizeof(blend));std::memcpy(&depth,now.pipeline.depth.data(),sizeof(depth));std::memcpy(&rs,now.pipeline.raster.data(),sizeof(rs));
 const auto &b=blend.RenderTarget[0];
 if(b.RenderTargetWriteMask!=15||!b.BlendEnable||b.LogicOpEnable||b.SrcBlend!=D3D12_BLEND_SRC_ALPHA||b.DestBlend!=D3D12_BLEND_INV_SRC_ALPHA||
    b.BlendOp!=D3D12_BLEND_OP_ADD||b.SrcBlendAlpha!=D3D12_BLEND_ONE||b.DestBlendAlpha!=D3D12_BLEND_ZERO||b.BlendOpAlpha!=D3D12_BLEND_OP_ADD||
    blend.AlphaToCoverageEnable||depth.DepthEnable||depth.StencilEnable||rs.FillMode!=D3D12_FILL_MODE_SOLID||rs.CullMode!=D3D12_CULL_MODE_BACK||
    !rs.FrontCounterClockwise||!rs.DepthClipEnable||rs.DepthBias||rs.DepthBiasClamp||rs.SlopeScaledDepthBias||rs.MultisampleEnable||rs.AntialiasedLineEnable||
    rs.ForcedSampleCount||rs.ConservativeRaster!=D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF)return native(2);
 for(unsigned i=0;i<3;++i){std::uintptr_t extra{};if(!field(r.recipe->set,0x60+8*i,extra)||extra)return native(3);}
 const auto &v=raster.viewport;const auto &sc=raster.scissor;
 if(v.TopLeftX||v.TopLeftY||v.Width!=render_w||v.Height!=render_h||v.MinDepth||v.MaxDepth!=1||sc.left||sc.top||sc.right!=LONG(render_w)||sc.bottom!=LONG(render_h))return native(4);
 const auto state=recording_states.get(reinterpret_cast<std::uintptr_t>(list),roots.generation,now.views[0].resource);
 if(!state.known||state.value!=D3D12_RESOURCE_STATE_RENDER_TARGET)return native(5);
 std::array<ComPtr<ID3D12Resource>,2> textures,buffers;std::array<unsigned,28> screen{};std::array<unsigned,5> material{};
 if(!silhouette_bindings(r,screen,material,textures)||material[2]!=1||material[3]!=7680||material[4]!=std::bit_cast<unsigned>(.1F)||
    screen[4]!=std::bit_cast<unsigned>(float(width))||screen[5]!=std::bit_cast<unsigned>(float(height))||screen[6]!=std::bit_cast<unsigned>(1.F/width)||screen[7]!=std::bit_cast<unsigned>(1.F/height)||
    screen[10]!=render_w||screen[11]!=render_h||screen[12]||screen[13]||screen[14]!=std::bit_cast<unsigned>(float(render_w))||screen[15]!=std::bit_cast<unsigned>(float(render_h))||
    screen[16]!=std::bit_cast<unsigned>(2.F/width)||screen[17]!=std::bit_cast<unsigned>(2.F/height))return native(6);
 for(unsigned i=0;i<2;++i){const auto d=textures[i]->GetDesc();if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.Width!=width||d.Height!=height||
    d.DepthOrArraySize!=1||d.MipLevels!=1||d.SampleDesc.Count!=1||d.SampleDesc.Quality||d.Format!=(i?DXGI_FORMAT_R8G8B8A8_TYPELESS:DXGI_FORMAT_R32_FLOAT))return native(7);}
 if(!silhouette_buffers(r,raster,buffers)||!layout(r.recipe->layout,true)||!silhouette_code_stable(*r.recipe))return native(8);
 for(auto ptr:{root,pipeline}){std::uintptr_t method{};for(unsigned slot:{0U,2U,7U})if(!pb::system_method(ptr,slot,method))return native(9);}
 auto *cache=silhouette_pipeline(r.recipe,reinterpret_cast<ID3D12RootSignature *>(root));if(!cache)return native(10);
 std::array<IUnknown *,9> pins{reinterpret_cast<IUnknown *>(pipeline),reinterpret_cast<IUnknown *>(root),textures[0].Get(),textures[1].Get(),buffers[0].Get(),buffers[1].Get(),cache->draw->pipeline()};unsigned size=7;
 for(auto heap:roots.heaps)if(heap)pins[size++]=reinterpret_cast<IUnknown *>(heap);
 if(!retain_recording(p,list,roots.generation,std::span(pins).first(size)))throw std::runtime_error("silhouette draw pins");
 injecting=true;
 {PredicateScope predicate(list);predicate.native(p.selector.Get());list->DrawIndexedInstanced(n,instances,first,base,instance);
  predicate.full(p.selector.Get());scope.replaced=cache->draw->record(list,reinterpret_cast<ID3D12PipelineState *>(pipeline),v,sc,n,first,base);}
 injecting=false;obs::RasterBindings after;
 if(!scope.replaced||!obs::matches_current(list,roots)||!obs::raster_current(list,after)||after.vertex_mask!=raster.vertex_mask||after.index_known!=raster.index_known||
    std::memcmp(after.vertex_buffers.data(),raster.vertex_buffers.data(),sizeof(raster.vertex_buffers))||std::memcmp(&after.index_buffer,&raster.index_buffer,sizeof(raster.index_buffer))||
    std::memcmp(&after.viewport,&v,sizeof(v))||std::memcmp(&after.scissor,&sc,sizeof(sc))||after.render_target!=raster.render_target||after.topology!=raster.topology)
  throw std::runtime_error("silhouette state restore");
 ++silhouette_corrected;++silhouette_restored;completed(p);return true;
}
void silhouette_after(bool returned) noexcept {
 if(indexed_scope.slot==UINT_MAX)return;Lock lock;
 if(!returned||indexed_scope.calls!=1)fail(14);
}
void silhouette_write(std::ostream &o){
 o<<"{\"schema\":1,\"policy\":\"indexed-native-coordinate-adapter-v1\",\"planned\":"<<silhouette_planned<<",\"recorded\":"<<silhouette_recorded
  <<",\"native\":"<<silhouette_native<<",\"corrected\":"<<silhouette_corrected<<",\"restored\":"<<silhouette_restored<<",\"pipelines\":"<<silhouette_cache_count<<",\"rejections\":";
 indexed_array(o,silhouette_rejections);o<<'}';
}
