// Native haze inputs remain bound by the original, code-gated draw executor.
// We alter neither its root arguments nor its descriptor contents. Qualify and
// retain the underlying textures at enqueue, then recheck the owner/view table
// at DrawInstanced. This is NOT independent GPU descriptor-content readback.
unsigned haze_admission_rejection{};
std::array<D3D12_RESOURCE_DESC,3> haze_aux_descriptions{};
bool haze_texture(const tb::Owner &o,unsigned slot,ComPtr<ID3D12Resource> &out){
 std::uintptr_t method{};for(unsigned v:{0U,2U,7U,10U})if(!pb::system_method(o.resource,v,method))return false;
 auto *r=reinterpret_cast<ID3D12Resource *>(o.resource);ComPtr<ID3D12Device> device;
 if(FAILED(r->GetDevice(IID_PPV_ARGS(&device)))||reinterpret_cast<std::uintptr_t>(device.Get())!=cfg.device)return false;
 const auto d=r->GetDesc();haze_aux_descriptions[slot-1]=d;
 if(d.Width!=o.width||d.Height!=o.height||!d.MipLevels||d.MipLevels>16||d.SampleDesc.Count!=1||d.SampleDesc.Quality||
    (d.Flags&D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))return false;
 if(slot==1){
  if(o.format!=16||o.width!=width||o.height!=height||d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||
     d.DepthOrArraySize!=1||d.MipLevels!=1||(d.Format!=DXGI_FORMAT_R32_FLOAT&&d.Format!=DXGI_FORMAT_R32_TYPELESS))return false;
 }else{
  if(o.format!=7||(d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&d.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB&&d.Format!=DXGI_FORMAT_R8G8B8A8_TYPELESS))return false;
  if(slot==2){if(o.kind!=1||d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE3D||d.Width!=32||d.Height!=32||d.DepthOrArraySize!=32)return false;}
  else if(d.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||d.DepthOrArraySize!=1||d.Width>16384||d.Height>16384)return false;
 }
 tb::Owner again;if(!tb::read_owner(reader,o.object,again)||again!=o)return false;out=r;return true;
}
bool qualify_haze(Plan &p){
 haze_admission_rejection=1;auto &r=p.draws[3];auto &b=p.haze;
 if(!haze_bindings(r.draw.shader_set,r.packet_bindings[2],b))return false;
 haze_admission_rejection=2;
 constexpr std::array<unsigned,9> words{1060865,17440785,790562,17059890,35762242,52531282,45152,16826480,52543616};
 if(b.count!=9||b.ranges[0]!=std::array<unsigned short,2>{0,0}||b.ranges[1]!=std::array<unsigned short,2>{9,0}||
    !std::equal(words.begin(),words.end(),b.words[1].begin())||b.indices!=std::array<unsigned,5>{2,3,4,5,0})return false;
 for(unsigned i=0;i<b.count;++i)if(b.modes[i]||!b.objects[i])return false;
 haze_admission_rejection=3;
 if(b.textures[0]!=p.draws[0].output||b.textures[0].resource==r.output.resource)return false;
 for(unsigned i=1;i<4;++i){
  haze_admission_rejection=3+i;
  if(b.textures[i].resource==r.output.resource||!haze_texture(b.textures[i],i,p.resources[5+i]))return false;
 }
 haze_admission_rejection=0;return true;
}
bool haze_bindings_stable(const Plan &p){
 HazeBindings now{};const auto &r=p.draws[3];
 return haze_bindings(r.draw.shader_set,r.packet_bindings[2],now)&&now==p.haze;
}

