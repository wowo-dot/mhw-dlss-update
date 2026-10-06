#include "memory.hpp"
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <algorithm>
#include <cstring>
namespace mhw::projection_binding {
namespace {
std::array<ID3D12Resource *,max_resources> pins{};
unsigned pin_count{};
bool address(std::uintptr_t p,unsigned n) noexcept {return p && p<=UINTPTR_MAX-n;}
template<class T> bool field(std::uintptr_t p,unsigned off,T &v) noexcept {
  return address(p,off+sizeof(T)) && copy_memory(p+off,&v,sizeof(T));
}
bool view(std::uintptr_t p,bool depth,ViewBinding &v) noexcept {
  v.owner=p;if(!p)return true;
  if(!field(p,0x60,v.backend) || !v.backend || !field(v.backend,0x10,v.handle) || !v.handle ||
     !field(v.backend,0x18,v.resource) || !v.resource)return false;
  if(depth)return field(p,0x74,v.format);
  unsigned char rotation{};
  if(!field(v.backend,0x20,v.format) || !field(v.backend,0x28,rotation))return false;
  v.rotating=rotation;
  // No length for the alternative handle table is established. Preserve the
  // discriminator and fail closed instead of guessing a rotation bound.
  if(rotation) {field(v.backend,0x2c,v.rotation_index);return false;}
  return true;
}
bool native_pipeline(std::uintptr_t image,std::uintptr_t packet,NativePipeline &p,unsigned depth_format) noexcept {
  if(!field(packet,0x20,p.graphics) || !field(packet,0x28,p.render) ||
     !field(p.graphics,0,p.graphics_flags) || !field(p.graphics,8,p.shader_handle) ||
     !field(p.graphics,0x18,p.group) || !field(p.graphics,0x20,p.input_layout))return false;
  if(p.input_layout && (!field(p.input_layout,0x10,p.input_elements) || !field(p.input_layout,0x18,p.input_count) ||
       p.input_count>32 || (p.input_count && !p.input_elements)))return false;
  unsigned char variant{};
  if(!field(p.render,4,variant) || !field(p.render,0x18,p.raster_owner) ||
     !field(p.render,0x10,p.depth_owner) || !field(p.render,8,p.blend_owner))return false;
  p.raster_variant=variant&1;
  p.dsv_format=depth_format==53?55:depth_format==44?45:depth_format==39?40:depth_format;
  if(!depth_format) {
    std::uintptr_t renderer{},owner{};
    if(!field(image,0x51c73d0,renderer) || !field(renderer,0x2480,owner) || !field(owner,0,p.depth_owner))return false;
  }
  if(!field(p.depth_owner,0x60,p.depth_state) || !field(p.blend_owner,0x98,p.blend_state) ||
     !field(p.raster_owner,0x68+8*p.raster_variant,p.raster_state))return false;
  return field(p.depth_state,0x10,p.depth) && field(p.blend_state,0x10,p.blend) && field(p.raster_state,0x10,p.raster);
}
bool methods(std::uintptr_t p,std::initializer_list<unsigned> slots) noexcept {
  for(const auto s:slots) {std::uintptr_t target{};if(!system_method(p,s,target))return false;}return true;
}
HRESULT query(std::uintptr_t p,REFIID iid,void **out) noexcept {
  if(!methods(p,{0,2}))return E_NOINTERFACE;
  __try{return reinterpret_cast<IUnknown *>(p)->QueryInterface(iid,out);}
  __except(EXCEPTION_EXECUTE_HANDLER){return E_FAIL;}
}
bool release(IUnknown *p) noexcept {
  if(!p)return true;
  if(!methods(reinterpret_cast<std::uintptr_t>(p),{2}))return false;
  __try{p->Release();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
template<class T> struct OwnedInterface {
  T *p{};
  ~OwnedInterface(){reset();}
  bool reset() noexcept {auto old=p;p=nullptr;return release(old);}
  std::uintptr_t address() const noexcept {return reinterpret_cast<std::uintptr_t>(p);}
  void **out() noexcept {return reinterpret_cast<void **>(&p);}
  T *detach() noexcept {auto old=p;p=nullptr;return old;}
};
bool desc(ID3D12Resource *p,ResourceDescription &s) noexcept {
  D3D12_RESOURCE_DESC d{};
  __try{d=p->GetDesc();}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
  s.dimension=d.Dimension;s.width=d.Width;s.height=d.Height;s.depth_or_array=d.DepthOrArraySize;
  s.mips=d.MipLevels;s.format=d.Format;s.samples=d.SampleDesc.Count;s.quality=d.SampleDesc.Quality;
  s.layout=d.Layout;s.flags=d.Flags;s.alignment=d.Alignment;
  return d.Dimension==D3D12_RESOURCE_DIMENSION_TEXTURE2D && d.Width && d.Height && d.DepthOrArraySize && d.MipLevels && d.SampleDesc.Count;
}
HRESULT owning_device(ID3D12Resource *p,void **out) noexcept {
  __try{return p->GetDevice(__uuidof(ID3D12Device),out);}
  __except(EXCEPTION_EXECUTE_HANDLER){return E_FAIL;}
}
bool resource(std::uintptr_t p,std::uintptr_t expected,ResourceDescription &s,ResourceQuery &q,bool retain) noexcept {
  OwnedInterface<ID3D12Resource> r,roundtrip;
  OwnedInterface<IUnknown> identity,repeat,roundtrip_identity;
  OwnedInterface<ID3D12Device> device;
  bool ok=false;q.candidate=p;
  // Keep every HRESULT and returned interface separately. COM identity is
  // canonical IUnknown equality, not equality of different interface addresses.
  do {
    q.checkpoint=1;q.results[0]=static_cast<unsigned>(query(p,__uuidof(ID3D12Resource),r.out()));q.resource=r.address();
    if(FAILED(static_cast<HRESULT>(q.results[0])) || !r.p)break;
    q.checkpoint=2;if(r.address()!=p)break;
    q.checkpoint=3;if(!methods(p,{0,2,7,10}))break;
    q.checkpoint=4;q.results[1]=static_cast<unsigned>(owning_device(r.p,device.out()));q.device=device.address();
    if(FAILED(static_cast<HRESULT>(q.results[1])) || !device.p)break;
    q.checkpoint=5;if(device.address()!=expected)break;
    q.checkpoint=6;q.results[2]=static_cast<unsigned>(query(p,__uuidof(IUnknown),identity.out()));q.identity=identity.address();
    if(FAILED(static_cast<HRESULT>(q.results[2])) || !identity.p)break;
    q.checkpoint=7;q.results[3]=static_cast<unsigned>(query(identity.address(),__uuidof(IUnknown),repeat.out()));q.identity_repeat=repeat.address();
    if(FAILED(static_cast<HRESULT>(q.results[3])) || !repeat.p)break;
    q.checkpoint=8;if(identity.p!=repeat.p)break;
    q.checkpoint=9;q.results[4]=static_cast<unsigned>(query(identity.address(),__uuidof(ID3D12Resource),roundtrip.out()));q.roundtrip=roundtrip.address();
    if(FAILED(static_cast<HRESULT>(q.results[4])) || !roundtrip.p)break;
    q.checkpoint=10;if(roundtrip.address()!=p)break;
    q.checkpoint=11;q.results[5]=static_cast<unsigned>(query(roundtrip.address(),__uuidof(IUnknown),roundtrip_identity.out()));q.roundtrip_identity=roundtrip_identity.address();
    if(FAILED(static_cast<HRESULT>(q.results[5])) || !roundtrip_identity.p)break;
    q.checkpoint=12;if(identity.p!=roundtrip_identity.p)break;
    q.checkpoint=13;if(!desc(r.p,s))break;
    s.resource=p;s.identity=identity.address();s.device=expected;ok=true;
  } while(false);
  const bool released_device=device.reset(),released_identity=identity.reset(),released_repeat=repeat.reset();
  const bool released_roundtrip=roundtrip.reset(),released_roundtrip_identity=roundtrip_identity.reset();
  q.temporaries_released=released_device && released_identity && released_repeat && released_roundtrip && released_roundtrip_identity;
  if(!q.temporaries_released){q.checkpoint=14;ok=false;}
  if(ok && retain && pin_count<max_resources)pins[pin_count++]=r.detach();
  else {
    if(retain && ok){q.checkpoint=14;ok=false;}
    const bool released=r.reset();q.temporaries_released=q.temporaries_released && released;
    if(!released){q.checkpoint=14;ok=false;}
  }
  if(ok)q.checkpoint=15;
  s.valid=q.valid=ok;return ok;
}
}
static_assert(sizeof(D3D12_RESOURCE_BARRIER)==32 && sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC)==656);
static_assert(sizeof(D3D12_INPUT_ELEMENT_DESC)==32); // 32 elements fit in the bounded byte array.
bool snapshot_native_pipeline(std::uintptr_t image,std::uintptr_t packet,NativePipeline &p,unsigned depth_format) noexcept {
  p={};return native_pipeline(image,packet,p,depth_format);
}
bool snapshot_attachments(std::uintptr_t image,const Draw &d,AttachmentSnapshot &s) noexcept {
  s={};
  if(!field(d.executor,8,s.state) || !field(s.state,0x20,s.command) || s.command!=d.command ||
     !field(d.packet,0x10,s.attachment) || !field(d.packet,0x38,s.incoming_pso) ||
     !field(d.context,0x10,s.active_attachment) || !field(s.attachment,0x48,s.attachment_flags))return false;
  s.color_count=s.attachment_flags&7;
  for(unsigned i=0;i<s.color_count;++i) {
    std::uintptr_t p{};if(!field(s.attachment,8*i,p) || !view(p,false,s.views[i]))return false;
  }
  std::uintptr_t p{};
  if(!field(s.attachment,0x40,p) || !view(p,true,s.views[8]) || !field(s.state,0x98,s.cached) ||
     !field(s.state,0x140,s.cached_depth_resource) || !field(s.state,0xe0,s.viewport) || !field(s.state,0xf0,s.scissor))return false;
  unsigned char enabled{};
  if(!field(s.state,0x100,enabled) || enabled>1)return false;
  s.scissor_enabled=enabled;
  if(!native_pipeline(image,d.packet,s.pipeline,s.views[8].format))return false;
  s.valid=true;return true;
}
bool attachment_inputs_equal(const AttachmentSnapshot &a,const AttachmentSnapshot &b) noexcept {
  return a.valid && b.valid && a.state==b.state && a.command==b.command && a.attachment==b.attachment &&
    a.incoming_pso==b.incoming_pso && a.attachment_flags==b.attachment_flags && a.color_count==b.color_count &&
    a.views==b.views && a.pipeline==b.pipeline;
}
bool attachment_cache_matches(const AttachmentSnapshot &s) noexcept {
  if(!s.valid || s.active_attachment!=s.attachment)return false;
  for(unsigned i=0;i<9;++i)if(s.views[i].handle!=s.cached[i])return false;
  return !s.views[8].owner || s.cached_depth_resource==s.views[8].resource;
}
bool describe_attachments(AttachmentCapture &c,std::uintptr_t device) noexcept {
  if(c.described || pin_count)return false;
  for(const auto &v:c.ready.views) {
    if(!v.resource)continue;
    bool found=false;for(unsigned i=0;i<c.resource_count;++i)if(c.resources[i].resource==v.resource)found=true;
    if(found)continue;
    if(c.resource_count>=max_resources || c.resource_check_counts[0]>=max_resources)return false;
    auto &q=c.resource_checks[0][c.resource_check_counts[0]++];
    if(!resource(v.resource,device,c.resources[c.resource_count],q,true))return false;
    ++c.resource_count;
  }
  c.described=c.resource_count!=0;return c.described;
}
bool recheck_resources(AttachmentCapture &c,unsigned phase) noexcept {
  if(phase<1 || phase>2 || c.resource_check_counts[phase] || !c.described || pin_count!=c.resource_count)return false;
  for(unsigned i=0;i<c.resource_count;++i) {
    ResourceDescription now{};const auto &previous=c.resources[i];
    auto &q=c.resource_checks[phase][c.resource_check_counts[phase]++];
    if(reinterpret_cast<std::uintptr_t>(pins[i])!=previous.resource ||
       !resource(previous.resource,previous.device,now,q,false) || now!=previous)return false;
  }
  return true;
}
bool release_attachment_resources() noexcept {
  bool ok=true;for(unsigned i=0;i<pin_count;++i) {if(!release(pins[i]))ok=false;pins[i]=nullptr;}
  pin_count=0;return ok;
}
bool capture_output_binding(const Frame &f,OutputBinding &o) noexcept {
  if(f.gpr[2]>8 || f.gpr[4]>1)return false;
  o.count=static_cast<unsigned>(f.gpr[2]);o.contiguous=static_cast<unsigned>(f.gpr[4]);
  // Pinned native path uses an explicit handle array. No stride guessing.
  if(o.contiguous)return false;
  if(o.count && !copy_memory(f.gpr[3],o.colors.data(),o.count*8))return false;
  o.depth_present=f.stack[0]!=0;
  return !o.depth_present || read(f.stack[0],o.depth);
}
bool output_binding_matches(const OutputBinding &o,const AttachmentSnapshot &s) noexcept {
  if(o.count!=s.color_count || o.contiguous || o.depth_present!=(s.views[8].owner!=0) || o.depth!=s.views[8].handle)return false;
  for(unsigned i=0;i<8;++i)if(o.colors[i]!=s.views[i].handle)return false;
  return true;
}
bool capture_barriers(const Frame &f,std::array<BarrierRecord,max_barriers> &out,unsigned &count) noexcept {
  if(!f.gpr[2] || f.gpr[2]>max_barriers)return false;
  count=static_cast<unsigned>(f.gpr[2]);
  std::array<D3D12_RESOURCE_BARRIER,max_barriers> a{};
  if(!copy_memory(f.gpr[3],a.data(),count*sizeof(a[0])))return false;
  for(unsigned i=0;i<count;++i) {
    const auto &b=a[i];auto &r=out[i];r.type=b.Type;r.flags=b.Flags;
    if(b.Flags!=D3D12_RESOURCE_BARRIER_FLAG_NONE)return false;
    if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
      r.resource=reinterpret_cast<std::uintptr_t>(b.Transition.pResource);r.subresource=b.Transition.Subresource;
      r.before=b.Transition.StateBefore;r.after=b.Transition.StateAfter;if(!r.resource)return false;
    } else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_UAV)r.resource=reinterpret_cast<std::uintptr_t>(b.UAV.pResource);
    else if(b.Type==D3D12_RESOURCE_BARRIER_TYPE_ALIASING) {
      r.resource=reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceBefore);r.other=reinterpret_cast<std::uintptr_t>(b.Aliasing.pResourceAfter);
    } else return false;
  }
  return true;
}
bool capture_pipeline_creation(const Frame &f,const Draw &d,const Capture &c,PipelineCreation &p) noexcept {
  if(p.seen)return false;p.seen=true;p.output_address=f.gpr[4];p.descriptor_address=f.gpr[2];GUID iid{};
  D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
  if(!copy_memory(f.gpr[3],&iid,sizeof(iid)) || iid!=__uuidof(ID3D12PipelineState) || !p.output_address ||
     !copy_memory(f.gpr[2],&desc,sizeof(desc)))return false;
  p.iid_valid=true;std::memcpy(p.descriptor.data(),&desc,sizeof(desc));
  p.root=reinterpret_cast<std::uintptr_t>(desc.pRootSignature);p.color_count=desc.NumRenderTargets;
  p.dsv_format=desc.DSVFormat;p.samples=desc.SampleDesc.Count;p.quality=desc.SampleDesc.Quality;
  p.input_count=desc.InputLayout.NumElements;if(p.input_count>32 || p.color_count>8)return false;
  for(unsigned i=0;i<8;++i)p.formats[i]=desc.RTVFormats[i];
  if(p.input_count && !copy_memory(reinterpret_cast<std::uintptr_t>(desc.InputLayout.pInputElementDescs),p.input_elements.data(),p.input_count*sizeof(D3D12_INPUT_ELEMENT_DESC)))return false;
  for(unsigned i=0;i<p.input_count;++i) {
    D3D12_INPUT_ELEMENT_DESC el{};std::memcpy(&el,p.input_elements.data()+i*sizeof(el),sizeof(el));
    bool end=false;for(unsigned j=0;j<64;++j) {
      if(!field(reinterpret_cast<std::uintptr_t>(el.SemanticName),j,p.semantics[i][j]))return false;
      if(!p.semantics[i][j]){end=true;break;}
      if(p.semantics[i][j]<32 || p.semantics[i][j]>=127)return false;
    }
    if(!end)return false;
  }
  const std::array<D3D12_SHADER_BYTECODE,5> shaders={desc.VS,desc.PS,desc.GS,desc.HS,desc.DS};
  std::array<std::byte,256> chunk{};
  for(unsigned i=0;i<5;++i) {
    const auto &s=shaders[i];if(s.BytecodeLength>max_shader)return false;
    p.sizes[i]=static_cast<unsigned>(s.BytecodeLength);p.data[i]=reinterpret_cast<std::uintptr_t>(s.pShaderBytecode);
    if(!d.blobs[i]) {if(s.BytecodeLength || s.pShaderBytecode)return false;continue;}
    if(d.blobs[i]>c.blob_count)return false;const auto &blob=c.blobs[d.blobs[i]-1];
    if(s.BytecodeLength!=blob.size)return false;
    for(unsigned j=0;j<blob.size;j+=256) {
      const auto n=(std::min)(256U,blob.size-j);
      if(!address(p.data[i],j+n) || !copy_memory(p.data[i]+j,chunk.data(),n) || std::memcmp(chunk.data(),blob.bytes.data()+j,n))return false;
    }
  }
  p.bytecode_matches=true;return true;
}
bool finish_pipeline_creation(const Frame &f,const Draw &d,const Capture &c,PipelineCreation &p) noexcept {
  p.returned=true;p.result=static_cast<unsigned>(f.gpr[0]);
  p.succeeded=SUCCEEDED(static_cast<HRESULT>(p.result)) && read(p.output_address,p.pso) && p.pso;
  Frame before{};const GUID iid=__uuidof(ID3D12PipelineState);PipelineCreation now{};
  before.gpr[2]=p.descriptor_address;before.gpr[3]=reinterpret_cast<std::uintptr_t>(&iid);before.gpr[4]=p.output_address;
  p.stable=capture_pipeline_creation(before,d,c,now) && p.descriptor==now.descriptor &&
    p.input_elements==now.input_elements && p.semantics==now.semantics;
  return p.succeeded && p.stable;
}
}
