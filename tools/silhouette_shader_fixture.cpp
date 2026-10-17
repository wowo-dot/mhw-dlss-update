// Uses private captured shaders supplied by path; no game process or assets
// are modified. GPU mask/depth oracles distinguish coordinate adaptation from
// simply enlarging the viewport. All objects live through the completion fence.
#include "../src/scaled_temporal/silhouette_shader.hpp"
#include "../src/scaled_temporal/silhouette_draw.hpp"
#include "../src/temporal_present/observer.hpp"
#include "../tests/fake_astera.hpp"
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
namespace {
using Microsoft::WRL::ComPtr;
namespace sf=mhw::scaled_temporal;
void require(bool b,const char *why){if(!b)throw std::runtime_error(why);}
void hr(HRESULT h,const char *why){if(FAILED(h))throw std::runtime_error(std::string(why)+" HRESULT="+std::to_string(static_cast<unsigned long>(h)));}
std::vector<std::byte> read(const std::filesystem::path &p){
 auto n=std::filesystem::file_size(p);require(n>=52&&n<=131072,"shader size");std::vector<std::byte> b(static_cast<std::size_t>(n));
 std::ifstream f(p,std::ios::binary);require(bool(f.read(reinterpret_cast<char *>(b.data()),static_cast<std::streamsize>(n))),"read shader");return b;
}
void save(const std::filesystem::path &p,const void *b,std::size_t n){require(!std::filesystem::exists(p),"preserve artifacts");
 std::ofstream f(p,std::ios::binary);require(bool(f.write(static_cast<const char *>(b),static_cast<std::streamsize>(n))),"write artifact");}
ComPtr<ID3D11ShaderReflection> reflect(std::span<const std::byte> b){ComPtr<ID3D11ShaderReflection> r;hr(D3DReflect(b.data(),b.size(),IID_PPV_ARGS(&r)),"reflection");return r;}
ComPtr<ID3DBlob> vertex(ID3D11ShaderReflection *r,float z){
 D3D11_SHADER_DESC d{};hr(r->GetDesc(&d),"shader desc");std::ostringstream s;s<<"struct V {float4 p:SV_POSITION;";
 for(unsigned i=0;i<d.InputParameters;++i){D3D11_SIGNATURE_PARAMETER_DESC p{};hr(r->GetInputParameterDesc(i,&p),"input signature");
  if(p.SystemValueType!=D3D_NAME_UNDEFINED)continue;
  s<<(p.ComponentType==D3D_REGISTER_COMPONENT_UINT32?"uint4":"float4")<<" a"<<i<<":"<<p.SemanticName<<p.SemanticIndex<<";";
 }
 s<<"};V main(uint i:SV_VertexID){V v=(V)0;float2 c=float2((i&1)?1:-1,(i&2)?-1:1);v.p=float4(c,"<<z<<",1);";
 for(unsigned i=0;i<d.InputParameters;++i){D3D11_SIGNATURE_PARAMETER_DESC p{};hr(r->GetInputParameterDesc(i,&p),"input signature");
  if(p.SystemValueType!=D3D_NAME_UNDEFINED)continue;std::string name=p.SemanticName;
  s<<"v.a"<<i<<"=";
  if(name=="WorldPosition"||name=="WorldNormal")s<<"float4(0,0,1,1)";
  else if(name=="WorldTangent")s<<"float4(1,0,0,1)";
  else if(name=="WorldBiNormal")s<<"float4(0,1,0,1)";
  else s<<(p.ComponentType==D3D_REGISTER_COMPONENT_UINT32?"uint4(0,0,0,0)":"float4(.5,.5,.5,.5)");s<<";";
 }
 s<<"return v;}";auto text=s.str();ComPtr<ID3DBlob> b,e;
 const auto result=D3DCompile(text.data(),text.size(),"silhouette fixture",nullptr,nullptr,"main","vs_5_0",
  D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_WARNINGS_ARE_ERRORS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&b,&e);
 if(FAILED(result)&&e)std::cerr<<static_cast<const char *>(e->GetBufferPointer());hr(result,"fixture VS");return b;
}
D3D12_RESOURCE_DESC texture(unsigned w,unsigned h,DXGI_FORMAT format){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
 d.Width=w;d.Height=h;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Format=format;return d;}
ComPtr<ID3D12Resource> resource(ID3D12Device *device,D3D12_RESOURCE_DESC d,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){
 D3D12_HEAP_PROPERTIES h{};h.Type=type;ComPtr<ID3D12Resource> r;D3D12_CLEAR_VALUE clear{};clear.Format=d.Format;
 hr(device->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,state,(d.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)?&clear:nullptr,IID_PPV_ARGS(&r)),"resource");return r;
}
ComPtr<ID3D12Resource> buffer(ID3D12Device *device,UINT64 bytes,D3D12_HEAP_TYPE type){D3D12_RESOURCE_DESC d{};
 d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=bytes;d.Height=1;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
 return resource(device,d,type,type==D3D12_HEAP_TYPE_UPLOAD?D3D12_RESOURCE_STATE_GENERIC_READ:D3D12_RESOURCE_STATE_COPY_DEST);
}
void upload(ID3D12Resource *r,const void *data,std::size_t n){void *p{};D3D12_RANGE none{};hr(r->Map(0,&none,&p),"upload map");std::memcpy(p,data,n);r->Unmap(0,nullptr);}
void barrier(ID3D12GraphicsCommandList *l,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){D3D12_RESOURCE_BARRIER x{};x.Transition={r,UINT_MAX,a,b};l->ResourceBarrier(1,&x);}
struct Pixels {
 ComPtr<ID3D12Resource> data;D3D12_PLACED_SUBRESOURCE_FOOTPRINT foot{};UINT64 bytes{},row{};unsigned height{};
 Pixels(ID3D12Device *d,const D3D12_RESOURCE_DESC &desc,D3D12_HEAP_TYPE type):height(desc.Height){
  d->GetCopyableFootprints(&desc,0,1,0,&foot,nullptr,&row,&bytes);data=buffer(d,bytes,type);
 }
 void write(std::span<const float> f){require(f.size_bytes()==row*height,"pixel upload size");void *p{};D3D12_RANGE none{};hr(data->Map(0,&none,&p),"pixel map");
  for(unsigned y=0;y<height;++y)std::memcpy(static_cast<std::byte *>(p)+SIZE_T(y)*foot.Footprint.RowPitch,f.data()+y*row/4,static_cast<std::size_t>(row));data->Unmap(0,nullptr);
 }
 void copy(ID3D12GraphicsCommandList *l,ID3D12Resource *r,bool up){D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=data.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=foot;
  b.pResource=r;b.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;if(up)l->CopyTextureRegion(&b,0,0,0,&a,nullptr);else l->CopyTextureRegion(&a,0,0,0,&b,nullptr);
 }
 std::vector<float> read(){void *p{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};hr(data->Map(0,&range,&p),"pixel readback");std::vector<float> f(static_cast<std::size_t>(row)*height/4);
  for(unsigned y=0;y<height;++y)std::memcpy(f.data()+y*row/4,static_cast<std::byte *>(p)+SIZE_T(y)*foot.Footprint.RowPitch,static_cast<std::size_t>(row));D3D12_RANGE none{};data->Unmap(0,&none);return f;
 }
};
struct Fixture {
 ComPtr<ID3D12Device> device;ComPtr<ID3D12InfoQueue> messages;ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;
 ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;ComPtr<ID3D12RootSignature> root;UINT64 sequence{},generation{};
 Fixture(){
  ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug layer");debug->EnableDebugLayer();
  ComPtr<ID3D12Debug1> validation;hr(debug.As(&validation),"GPU validation");validation->SetEnableGPUBasedValidation(TRUE);
  ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> warp;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)),"WARP");
  hr(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");hr(device.As(&messages),"info queue");
  D3D12_COMMAND_QUEUE_DESC q{};hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"allocator");
  hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(list->Close(),"initial close");hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
  // The loader installs the observer's registered detours; the observer checks them.
  mhw::test::fake_astera::start();std::vector<mhw::astera::Hook> hooks;mhw::temporal_present::observer::astera_hooks(hooks);
  require(mhw::test::fake_astera::install_methods(hooks,{.command_list=list.Get()}),"loader installs the observer's detours");std::string why;
  const auto resolver=[](std::uintptr_t object,unsigned slot,std::uintptr_t &target)noexcept {target=(*reinterpret_cast<std::uintptr_t **>(object))[slot];return target!=0;};
  require(mhw::temporal_present::observer::install(device.Get(),resolver,why,true,nullptr,true,true),why.c_str());mhw::temporal_present::observer::begin();
  std::array<D3D12_DESCRIPTOR_RANGE,2> ranges{{{D3D12_DESCRIPTOR_RANGE_TYPE_CBV,16,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,32,0,0,0}}};
  std::array<D3D12_ROOT_PARAMETER,2> parameters{};for(unsigned i=0;i<2;++i){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[i].DescriptorTable={1,&ranges[i]};parameters[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
  std::array<D3D12_STATIC_SAMPLER_DESC,16> samplers{};for(unsigned i=0;i<samplers.size();++i){auto &s=samplers[i];s.Filter=i==2?D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT:D3D12_FILTER_MIN_MAG_MIP_POINT;
   s.AddressU=s.AddressV=s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;s.MaxLOD=D3D12_FLOAT32_MAX;s.ShaderRegister=i;s.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
  D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=2;rd.pParameters=parameters.data();rd.NumStaticSamplers=static_cast<UINT>(samplers.size());rd.pStaticSamplers=samplers.data();rd.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  ComPtr<ID3DBlob> b,e;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&b,&e),"serialize root");hr(device->CreateRootSignature(0,b->GetBufferPointer(),b->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
 }
 ~Fixture(){mhw::temporal_present::observer::finish();}
 void finish(){hr(list->Close(),"close");ID3D12CommandList *ls[]{list.Get()};queue->ExecuteCommandLists(1,ls);hr(queue->Signal(fence.Get(),++sequence),"signal");
  HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"fence event");const auto result=fence->SetEventOnCompletion(sequence,event);
  if(FAILED(result)){CloseHandle(event);hr(result,"fence completion");}const auto wait=WaitForSingleObject(event,30000);CloseHandle(event);require(wait==WAIT_OBJECT_0,"GPU completion");
 }
 void review(){const auto count=messages->GetNumStoredMessages();for(UINT64 i=0;i<count;++i){SIZE_T n{};hr(messages->GetMessage(i,nullptr,&n),"message size");std::vector<std::byte> b(n);auto *m=reinterpret_cast<D3D12_MESSAGE *>(b.data());hr(messages->GetMessage(i,m,&n),"message");
   if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING){std::cerr<<m->pDescription<<'\n';messages->ClearStoredMessages();throw std::runtime_error("D3D12 warning/error");}}
  messages->ClearStoredMessages();
 }
 bool candidate{};
 float intensity{.1F};  // fMaterialHiddenSurfaceDrawIntensity
 std::vector<float> draw(std::span<const std::byte> ps,sf::FilterGeometry g,bool full,float z,unsigned scenario,bool adapted=false,bool full_choice=true){
  hr(allocator->Reset(),"reset allocator");hr(list->Reset(allocator.Get(),nullptr),"reset list");mhw::temporal_present::observer::reset(list.Get(),++generation,nullptr);auto reflected=reflect(ps);auto vs=vertex(reflected.Get(),z);
  const auto w=g.output_width,h=g.output_height;
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=48;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ComPtr<ID3D12DescriptorHeap> heap,rtv;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"heap");hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=3;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"RTVs");
  const auto step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  std::vector<float> constants(16*16384,0);
  // Fields by reflected name, wherever each shader binds its buffers: linear
  // depth = 100/z, CBScreen at output allocation/render view, the captured
  // hidden-surface material values, opaque transparency and base colour.
  {D3D11_SHADER_DESC sd{};hr(reflected->GetDesc(&sd),"PS desc");
   for(unsigned i=0;i<sd.ConstantBuffers;++i){auto *buffer_reflection=reflected->GetConstantBufferByIndex(i);D3D11_SHADER_BUFFER_DESC bd{};
    if(FAILED(buffer_reflection->GetDesc(&bd))||bd.Type!=D3D_CT_CBUFFER)continue;D3D11_SHADER_INPUT_BIND_DESC bind{};
    if(FAILED(reflected->GetResourceBindingDescByName(bd.Name,&bind))||bind.BindPoint>=16)continue;
    for(unsigned v=0;v<bd.Variables;++v){D3D11_SHADER_VARIABLE_DESC vd{};hr(buffer_reflection->GetVariableByIndex(v)->GetDesc(&vd),"variable");
     require(vd.StartOffset+16<=65536,"constant offset");float *at=constants.data()+bind.BindPoint*16384+vd.StartOffset/4;const std::string n=vd.Name;
     if(n=="fZToLinear"){at[0]=1;at[1]=1;at[2]=100;}
     else if(n=="fScreenInverseSize"){at[0]=1.F/w;at[1]=1.F/h;}
     else if(n=="iViewSize"){at[0]=std::bit_cast<float>(g.render_width);at[1]=std::bit_cast<float>(g.render_height);}
     else if(n=="fGlobalTransparency")at[0]=1;
     else if(n=="bMaterialCustomLighting")at[0]=std::bit_cast<float>(1U);
     else if(n=="iMaterialCustomLightingGBufferIdMask")at[0]=std::bit_cast<float>(7680U);
     else if(n=="fMaterialHiddenSurfaceDrawIntensity")at[0]=intensity;
     else if(n.starts_with("fBaseMapFactor"))std::fill(at,at+4,1.F);
    }}}
  auto constant_buffer=buffer(device.Get(),constants.size()*4,D3D12_HEAP_TYPE_UPLOAD);upload(constant_buffer.Get(),constants.data(),constants.size()*4);
  for(unsigned i=0;i<16;++i){D3D12_CONSTANT_BUFFER_VIEW_DESC c{constant_buffer->GetGPUVirtualAddress()+UINT64(i)*65536,65536};device->CreateConstantBufferView(&c,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(i)*step});}
  D3D11_SHADER_DESC reflected_desc{};hr(reflected->GetDesc(&reflected_desc),"PS desc");
  for(unsigned i=0;i<32;++i){D3D12_SHADER_RESOURCE_VIEW_DESC s{};s.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;s.Texture2D.MipLevels=1;s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   for(unsigned j=0;j<reflected_desc.BoundResources;++j){D3D11_SHADER_INPUT_BIND_DESC b{};hr(reflected->GetResourceBindingDesc(j,&b),"binding");if(b.BindPoint!=i||b.Type==D3D_SIT_CBUFFER||b.Type==D3D_SIT_SAMPLER)continue;
    if(b.Dimension==D3D_SRV_DIMENSION_BUFFER){s.Format=DXGI_FORMAT_UNKNOWN;s.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;s.Buffer.NumElements=4;s.Buffer.StructureByteStride=(i==1||i==2)?24:i==3?4:16;}
    else if(b.Dimension==D3D_SRV_DIMENSION_TEXTURECUBE){s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURECUBE;s.TextureCube.MipLevels=1;}
    else if(b.Dimension==D3D_SRV_DIMENSION_TEXTURE2DARRAY){s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;s.Texture2DArray.MipLevels=1;s.Texture2DArray.ArraySize=1;}
   }
   device->CreateShaderResourceView(nullptr,&s,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(16+i)*step});
  }
  // Every bound 2-D texture gets data by name: depth pattern, G-buffer ID
  // mask pattern, flat normal, or white (albedo, alpha and other maps).
  std::vector<ComPtr<ID3D12Resource>> inputs;std::vector<Pixels> uploads;uploads.reserve(32);
  for(unsigned j=0;j<reflected_desc.BoundResources;++j){D3D11_SHADER_INPUT_BIND_DESC b{};hr(reflected->GetResourceBindingDesc(j,&b),"binding");
   if(b.Type!=D3D_SIT_TEXTURE||b.Dimension!=D3D_SRV_DIMENSION_TEXTURE2D||b.BindPoint>=32)continue;const std::string n=b.Name;
   const unsigned role=n=="tVolumeBlendDepthMap"||n=="tWaterReflectionDepth"?0:n=="tGBuffer1"?1:n.find("NormalMap")!=std::string::npos?3:2;
   auto d=texture(w,h,DXGI_FORMAT_R32G32B32A32_FLOAT);inputs.push_back(resource(device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST));uploads.emplace_back(device.Get(),d,D3D12_HEAP_TYPE_UPLOAD);
   std::vector<float> pixels(std::size_t(w)*h*4,0);
   for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){const auto at=(std::size_t(y)*w+x)*4;const bool active=x<g.render_width&&y<g.render_height;
    if(role==0)pixels[at]=active?(scenario==1&&((x/5+y/3)%2)? .1F:.9F):.1F;
    if(role==1)pixels[at+3]=active&&((x/7+y/5)%3!=0)?88.F/255.F:0;
    if(role==2)std::fill(pixels.begin()+at,pixels.begin()+at+4,1.F);
    if(role==3){pixels[at]=pixels[at+1]=.501961F;pixels[at+2]=pixels[at+3]=1;}
   }
   uploads.back().write(pixels);uploads.back().copy(list.Get(),inputs.back().Get(),true);barrier(list.Get(),inputs.back().Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   D3D12_SHADER_RESOURCE_VIEW_DESC s{};s.Format=d.Format;s.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;s.Texture2D.MipLevels=1;s.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   device->CreateShaderResourceView(inputs.back().Get(),&s,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(16+b.BindPoint)*step});
  }
  std::array<ComPtr<ID3D12Resource>,3> outputs;std::array<DXGI_FORMAT,3> formats{DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32G32_FLOAT};
  const auto rtv_step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for(unsigned i=0;i<3;++i){auto d=texture(w,h,formats[i]);d.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;outputs[i]=resource(device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);
   D3D12_CPU_DESCRIPTOR_HANDLE handle{rtv->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(i)*rtv_step};device->CreateRenderTargetView(outputs[i].Get(),nullptr,handle);const float clear[4]{};list->ClearRenderTargetView(handle,clear,0,nullptr);}
  Pixels result(device.Get(),outputs[0]->GetDesc(),D3D12_HEAP_TYPE_READBACK);
  D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=root.Get();p.VS={vs->GetBufferPointer(),vs->GetBufferSize()};p.PS={ps.data(),ps.size()};p.SampleMask=UINT_MAX;
  p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
  p.NumRenderTargets=3;std::copy(formats.begin(),formats.end(),p.RTVFormats);p.SampleDesc.Count=1;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  for(auto &b:p.BlendState.RenderTarget){b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.RenderTargetWriteMask=15;}
  ComPtr<ID3D12PipelineState> pipeline;hr(device->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&pipeline)),"PSO");
  std::array<unsigned,6> indices{0,1,2,2,1,3};auto ib=buffer(device.Get(),sizeof(indices),D3D12_HEAP_TYPE_UPLOAD);upload(ib.Get(),indices.data(),sizeof(indices));D3D12_INDEX_BUFFER_VIEW iv{ib->GetGPUVirtualAddress(),sizeof(indices),DXGI_FORMAT_R32_UINT};
  ID3D12DescriptorHeap *hs[]{heap.Get()};list->SetDescriptorHeaps(1,hs);list->SetGraphicsRootSignature(root.Get());list->SetGraphicsRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());
  list->SetGraphicsRootDescriptorTable(1,{heap->GetGPUDescriptorHandleForHeapStart().ptr+UINT64(16)*step});list->SetPipelineState(pipeline.Get());
  const D3D12_VIEWPORT v{0,0,float(full?w:g.render_width),float(full?h:g.render_height),0,1};const D3D12_RECT scissor{0,0,LONG(full?w:g.render_width),LONG(full?h:g.render_height)};
  const auto target_handle=rtv->GetCPUDescriptorHandleForHeapStart();
  list->RSSetViewports(1,&v);list->RSSetScissorRects(1,&scissor);list->OMSetRenderTargets(3,&target_handle,TRUE,nullptr);
  auto vb=buffer(device.Get(),64,D3D12_HEAP_TYPE_UPLOAD);const D3D12_VERTEX_BUFFER_VIEW vv{vb->GetGPUVirtualAddress(),64,16};
  list->IASetVertexBuffers(0,1,&vv);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->IASetIndexBuffer(&iv);
  std::unique_ptr<sf::SilhouetteDraw> correction;ComPtr<ID3D12Resource> selector;
  if(adapted){
   sf::SilhouetteRecipe recipe{g,p,candidate};correction=std::make_unique<sf::SilhouetteDraw>(device.Get(),recipe);
   selector=buffer(device.Get(),8,D3D12_HEAP_TYPE_UPLOAD);const std::uint64_t choice=full_choice?1:0;upload(selector.Get(),&choice,8);
   const D3D12_VIEWPORT native_view{0,0,float(g.render_width),float(g.render_height),0,1};
   const D3D12_RECT native_clip{0,0,LONG(g.render_width),LONG(g.render_height)};
   list->RSSetViewports(1,&native_view);list->RSSetScissorRects(1,&native_clip);
   mhw::temporal_present::Bindings before;namespace obs=mhw::temporal_present::observer;obs::RasterBindings raster_before;
   require(obs::inspect(list.Get(),before)&&obs::raster_current(list.Get(),raster_before),"pre-draw API bindings");
   list->SetPredication(selector.Get(),0,D3D12_PREDICATION_OP_NOT_EQUAL_ZERO);list->DrawIndexedInstanced(6,1,0,0,0);
   list->SetPredication(selector.Get(),0,D3D12_PREDICATION_OP_EQUAL_ZERO);
   require(correction->record(list.Get(),pipeline.Get(),native_view,native_clip,6,0,0),"actual private silhouette draw");
   list->SetPredication(nullptr,0,D3D12_PREDICATION_OP_EQUAL_ZERO);
   obs::RasterBindings after;obs::Predicate predicate;
   require(obs::matches_current(list.Get(),before)&&obs::raster_current(list.Get(),after)&&
    after.vertex_mask==raster_before.vertex_mask&&after.index_known==raster_before.index_known&&
    !std::memcmp(after.vertex_buffers.data(),raster_before.vertex_buffers.data(),sizeof(after.vertex_buffers))&&
    !std::memcmp(&after.index_buffer,&raster_before.index_buffer,sizeof(after.index_buffer))&&
    !std::memcmp(&after.viewport,&raster_before.viewport,sizeof(after.viewport))&&!std::memcmp(&after.scissor,&raster_before.scissor,sizeof(after.scissor))&&
    after.topology==raster_before.topology&&after.render_target==raster_before.render_target&&after.output_known==raster_before.output_known&&
    obs::predicate_current(list.Get(),predicate)&&!predicate.buffer,"exact PSO/RS/IA/root/heaps/predicate restoration");
  }else list->DrawIndexedInstanced(6,1,0,0,0);
  barrier(list.Get(),outputs[0].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);result.copy(list.Get(),outputs[0].Get(),false);finish();auto pixels=result.read();review();return pixels;
 }
};
// One shader: reflection preserved, 1x identity, 2x mask/depth/position
// preservation against the native render-size draw (two occluder patterns,
// two depths), wrong-coordinate negative control and the selector's native branch.
struct Verdict {unsigned pixel_checks{},negative_checks{};};
Verdict verify(Fixture &fixture,std::span<const std::byte> original,bool candidate,const std::filesystem::path &prefix,bool keep){
 Verdict v;const sf::FilterGeometry g{64,36,128,72};fixture.candidate=candidate;
 auto adapted=candidate?sf::adapt_silhouette_shader(original,g):sf::make_silhouette_shader(original,g);
 auto x=reflect(original),y=reflect(adapted);D3D11_SHADER_DESC xd{},yd{};hr(x->GetDesc(&xd),"native desc");hr(y->GetDesc(&yd),"adapted desc");
 require(xd.BoundResources==yd.BoundResources&&xd.InputParameters==yd.InputParameters&&xd.OutputParameters==yd.OutputParameters,"preserved reflection");
 if(keep){ComPtr<ID3DBlob> asm_blob;hr(D3DDisassemble(adapted.data(),adapted.size(),0,nullptr,&asm_blob),"adapted disassembly");
  save(prefix.wstring()+L".dxbc",adapted.data(),adapted.size());save(prefix.wstring()+L".asm",asm_blob->GetBufferPointer(),asm_blob->GetBufferSize());}
 bool rejected=false;try{sf::adapt_silhouette_shader(original,{0,18,64,36});}catch(const std::exception &){rejected=true;}require(rejected,"bad geometry rejected");++v.negative_checks;
 if(!candidate){std::vector<std::byte> altered(original.begin(),original.end());altered.back()^=std::byte{1};require(!sf::known_silhouette_shader(altered),"corrupted shader rejected");++v.negative_checks;}
 // Native and adapted at 1x are identical.
 const sf::FilterGeometry one{64,36,64,36};
 const auto native_one=fixture.draw(original,one,true,.1F,0),adapted_one=fixture.draw(original,one,true,.1F,0,true);
 require(native_one.size()==adapted_one.size(),"identity size");
 for(std::size_t j=0;j<native_one.size();++j){require(std::isfinite(native_one[j])&&std::isfinite(adapted_one[j])&&std::abs(native_one[j]-adapted_one[j])<.00001F,"identity shader equivalence");++v.pixel_checks;}
 // At 2x every output pixel equals the native render-size pixel it covers, at
 // the captured hidden-surface intensity and at a player's 0.02 (v0.28.6).
 // Each intensity must exercise the shader on its own: visible and hidden
 // pixels, and an enlarged viewport without adaptation must differ (position
 // dependence). The opacity scales with the intensity, so do the thresholds.
 for(const float strength:{.1F,.02F}){fixture.intensity=strength;const float visible=strength*.1F;unsigned lit=0,dark=0,wrong_pixels=0;
 for(const float z:{.1F,.9F})for(unsigned scenario=0;scenario<2;++scenario){
  auto native=fixture.draw(original,g,false,z,scenario);auto fixed=fixture.draw(original,g,true,z,scenario,true);auto wrong=fixture.draw(original,g,true,z,scenario);
  for(unsigned py=0;py<g.render_height;++py)for(unsigned px=0;px<g.render_width;++px){const auto source=(std::size_t(py)*g.output_width+px)*4;
   for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){const auto target=(std::size_t(py*2+dy)*g.output_width+px*2+dx)*4;
    for(unsigned c=0;c<4;++c){require(std::isfinite(fixed[target+c])&&std::abs(fixed[target+c]-native[source+c])<.00002F,"2x mask/depth preservation");++v.pixel_checks;}
    if(fixed[target+3]>visible)++lit;else ++dark;if(std::abs(wrong[target+3]-fixed[target+3])>visible)++wrong_pixels;
   }}
 }
 require(lit>100&&dark>100&&wrong_pixels>100,"oracle and wrong-coordinate negative control");++v.negative_checks;}
 fixture.intensity=.1F;
 const auto native_selected=fixture.draw(original,g,false,.1F,0,true,false),native_control=fixture.draw(original,g,false,.1F,0);
 require(native_selected==native_control,"GPU selector native branch exact equivalence");++v.negative_checks;
 return v;
}
std::string hex(const sf::ShaderHash &h){static const char *d="0123456789abcdef";std::string s;for(auto b:h){s+=d[b>>4];s+=d[b&15];}return s;}
}
int wmain(int argc,wchar_t **argv){try{
 require(argc>=3,"usage: silhouette_shader_fixture output-directory [--candidates] PS-path|@list ...");const std::filesystem::path output=argv[1];
 require(!std::filesystem::exists(output),"fresh output directory");std::filesystem::create_directories(output);
 bool candidate=false;std::vector<std::filesystem::path> paths;
 for(int i=2;i<argc;++i){const std::wstring a=argv[i];
  if(a==L"--candidates"){candidate=true;continue;}
  if(a.starts_with(L"@")){std::wifstream list{std::filesystem::path(a.substr(1))};std::wstring line;
   while(std::getline(list,line)){while(!line.empty()&&(line.back()==L'\r'||line.back()==L' '))line.pop_back();if(!line.empty())paths.emplace_back(line);}continue;}
  paths.emplace_back(a);}
 require(!paths.empty(),"no shaders");
 // Candidate runs record a verdict per shader; allowlist runs stop at the first failure.
 Fixture fixture;unsigned variants=0,failed=0,pixel_checks=0,negative_checks=0;std::ostringstream shaders;
 for(std::size_t i=0;i<paths.size();++i){const auto original=read(paths[i]);sf::ShaderHash hash;require(sf::shader_sha256(original,hash),"shader hash");
  if(!candidate)require(sf::known_silhouette_shader(original),"recognized shader");
  std::string reason;Verdict v;
  try{v=verify(fixture,original,candidate,output/std::to_wstring(i),!candidate);}
  catch(const std::exception &e){if(!candidate)throw;reason=e.what();}
  if(reason.empty()){++variants;pixel_checks+=v.pixel_checks;negative_checks+=v.negative_checks;}else ++failed;
  if(i)shaders<<',';shaders<<"{\"sha256\":\""<<hex(hash)<<"\",\"size\":"<<original.size()<<",\"passed\":"<<(reason.empty()?"true":"false")
   <<",\"reason\":\""<<reason<<"\",\"pixel_checks\":"<<v.pixel_checks<<'}';
  std::cerr<<(i+1)<<'/'<<paths.size()<<' '<<hex(hash).substr(0,8)<<' '<<(reason.empty()?"passed":reason)<<'\n';
 }
 std::ostringstream report;report<<"{\"passed\":"<<(failed?"false":"true")<<",\"candidates\":"<<(candidate?"true":"false")<<",\"adapter\":\"WARP\",\"gpu_validation\":true"
  <<",\"allowlist\":"<<sf::known_silhouette_shader_count()<<",\"variants\":"<<variants<<",\"failed\":"<<failed<<",\"pixel_checks\":"<<pixel_checks
  <<",\"indexed_private_draw\":true,\"conditional_branches\":true,\"binding_restore\":true,\"negative_checks\":"<<negative_checks<<",\"shaders\":["<<shaders.str()<<"]}\n";
 auto text=report.str();save(output/L"result.json",text.data(),text.size());
 std::cout<<"{\"passed\":"<<(failed?"false":"true")<<",\"variants\":"<<variants<<",\"failed\":"<<failed<<",\"pixel_checks\":"<<pixel_checks<<"}\n";
 return candidate||!failed?0:1;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
