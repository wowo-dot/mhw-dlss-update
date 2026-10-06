// Offline only: private captured shaders are supplied by path, never embedded.
#include "../src/scaled_temporal/radial_shader.hpp"
#include "../src/scaled_temporal/effects.hpp"
#include "../src/scaled_temporal/color_dependencies.hpp"
#include <windows.h>
#include "../third_party/dxc_hash/DxilHash.h"
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
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
using Microsoft::WRL::ComPtr;namespace sf=mhw::scaled_temporal;
void require(bool v,const char *s){if(!v)throw std::runtime_error(s);}
void hr(HRESULT v,const char *s){if(FAILED(v))throw std::runtime_error(std::string(s)+" HRESULT="+std::to_string(static_cast<unsigned>(v)));}
std::vector<std::byte> read(const std::filesystem::path &p){const auto n=std::filesystem::file_size(p);require(n<=262144,"shader bound");std::vector<std::byte>b(static_cast<std::size_t>(n));std::ifstream f(p,std::ios::binary);require(bool(f.read(reinterpret_cast<char *>(b.data()),static_cast<std::streamsize>(n))),"shader read");return b;}
void save(const std::filesystem::path &p,std::span<const std::byte> b){require(!std::filesystem::exists(p),"preserve artifact");std::ofstream f(p,std::ios::binary);require(bool(f.write(reinterpret_cast<const char *>(b.data()),static_cast<std::streamsize>(b.size()))),"artifact write");}
void upload(ID3D12Resource *r,std::span<const std::byte> b){void *p{};D3D12_RANGE none{};hr(r->Map(0,&none,&p),"upload map");std::memcpy(p,b.data(),b.size());r->Unmap(0,nullptr);}
D3D12_RESOURCE_DESC texture(unsigned w,unsigned h,DXGI_FORMAT format){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w;d.Height=h;d.DepthOrArraySize=d.MipLevels=1;d.SampleDesc.Count=1;d.Format=format;return d;}
ComPtr<ID3D12Resource> resource(ID3D12Device *d,D3D12_RESOURCE_DESC desc,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){D3D12_HEAP_PROPERTIES heap{};heap.Type=type;ComPtr<ID3D12Resource> r;D3D12_CLEAR_VALUE clear{desc.Format,{.9F,.1F,.8F,.7F}};hr(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,state,(desc.Flags&D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)?&clear:nullptr,IID_PPV_ARGS(&r)),"resource");return r;}
ComPtr<ID3D12Resource> buffer(ID3D12Device *d,UINT64 bytes,D3D12_HEAP_TYPE type){D3D12_RESOURCE_DESC b{};b.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;b.Width=bytes;b.Height=1;b.DepthOrArraySize=b.MipLevels=1;b.SampleDesc.Count=1;b.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return resource(d,b,type,type==D3D12_HEAP_TYPE_UPLOAD?D3D12_RESOURCE_STATE_GENERIC_READ:D3D12_RESOURCE_STATE_COPY_DEST);}
void barrier(ID3D12GraphicsCommandList *l,ID3D12Resource *r,D3D12_RESOURCE_STATES a,D3D12_RESOURCE_STATES b){D3D12_RESOURCE_BARRIER x{};x.Transition={r,UINT_MAX,a,b};l->ResourceBarrier(1,&x);}
struct Pixels {
 ComPtr<ID3D12Resource> data;D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 row{},bytes{};unsigned rows{};
 Pixels(ID3D12Device *d,const D3D12_RESOURCE_DESC &desc,D3D12_HEAP_TYPE type){d->GetCopyableFootprints(&desc,0,1,0,&footprint,&rows,&row,&bytes);data=buffer(d,bytes,type);}
 void write(std::span<const std::byte> b){require(b.size()==rows*row,"pixel size");void *p{};D3D12_RANGE none{};hr(data->Map(0,&none,&p),"pixel map");for(unsigned y=0;y<rows;++y)std::memcpy(static_cast<std::byte *>(p)+SIZE_T(y)*footprint.Footprint.RowPitch,b.data()+y*row,static_cast<std::size_t>(row));data->Unmap(0,nullptr);}
 void copy(ID3D12GraphicsCommandList *l,ID3D12Resource *r,bool in){D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=data.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;a.PlacedFootprint=footprint;b.pResource=r;b.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;if(in)l->CopyTextureRegion(&b,0,0,0,&a,nullptr);else l->CopyTextureRegion(&a,0,0,0,&b,nullptr);}
 std::vector<unsigned> read(){std::vector<unsigned> b(static_cast<std::size_t>(rows*row/4));void *p{};D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};hr(data->Map(0,&range,&p),"readback");for(unsigned y=0;y<rows;++y)std::memcpy(b.data()+y*row/4,static_cast<std::byte *>(p)+SIZE_T(y)*footprint.Footprint.RowPitch,static_cast<std::size_t>(row));D3D12_RANGE none{};data->Unmap(0,&none);return b;}
};
enum class Mode { reference,adapted,bad_copy,bad_position,bad_auxiliary,bad_color };
// Engineering regression budget for the normalized test pattern, not an SDK
// guarantee. Exact copies, point-filter output and coordinate probes are
// separate controls; this budget applies only to scaled linear-filter output.
// One 1/256-texel fraction step on each axis, contrast .8, four samples,
// largest channel gain .5 and final gain <= .75. This is specific to the
// fixture's bounded constants. It is not a tolerance for arbitrary shaders.
constexpr double linear_float_budget=.8*2/256*4*.5*.75;
constexpr double packed_filter_budget=linear_float_budget+2.0/1023*4*.5*.75;
double unpack_channel(unsigned value,unsigned mantissa){const auto exp=value>>mantissa,frac=value&((1U<<mantissa)-1);return std::ldexp(double(exp?(1U<<mantissa)+frac:frac),(exp?int(exp)-15:1-15)-int(mantissa));}
struct Comparison {unsigned different{},packed_steps{};double maximum{},packed_budget_ratio{};};
Comparison compare(std::span<const unsigned> a,std::span<const unsigned> b,bool packed,bool check_alpha=true){
 require(a.size()==b.size(),"comparison dimensions");Comparison result;
 for(std::size_t i=0;i<a.size();++i){if(a[i]!=b[i])++result.different;
  if(packed){for(unsigned c=0;c<3;++c){const auto shift=c*11,mask=c==2?1023U:2047U,x=(a[i]>>shift)&mask,y=(b[i]>>shift)&mask,m=c==2?5U:6U;require((x>>m)!=31&&(y>>m)!=31,"finite packed output");result.packed_steps=(std::max)(result.packed_steps,x>y?x-y:y-x);const auto delta=std::abs(unpack_channel(x,m)-unpack_channel(y,m));const auto spacing=[&](unsigned v){return std::ldexp(1.0,(std::max)(1,int(v>>m))-15-int(m));};result.maximum=(std::max)(result.maximum,delta);result.packed_budget_ratio=(std::max)(result.packed_budget_ratio,delta/(packed_filter_budget+(std::max)(spacing(x),spacing(y))));}}
  else{const auto x=std::bit_cast<float>(a[i]),y=std::bit_cast<float>(b[i]);require(std::isfinite(x)&&std::isfinite(y),"finite output");result.maximum=(std::max)(result.maximum,std::abs(double(x)-y));if(check_alpha&&i%4==3)require(a[i]==b[i],"alpha unchanged");}
 }
 return result;
}
// Diagnostic only: test arithmetic refactoring as a cause of an oracle delta.
// D3D token specification: dcl_globalFlags bit 11, precise mask bits 19..22.
std::vector<std::byte> disable_refactoring(std::vector<std::byte> bytes){
 std::vector<unsigned> words(bytes.size()/4);std::memcpy(words.data(),bytes.data(),bytes.size());unsigned changed=0;
 for(unsigned i=0;i<words[7];++i){const auto at=words[8+i]/4;if(words[at]!=0x58454853)continue;
  const auto begin=at+2,end=begin+words[at+1]/4;
  for(auto p=begin+2;p<end;){const auto n=(words[p]>>24)&127U;require(n&&p+n<=end,"diagnostic token bounds");const auto op=words[p]&0x7ff;
   if(op==0x6a){words[p]&=~0x800U;++changed;}
   if(op==0x38||op==0x32)words[p]&=~0x780000U;
   p+=n;
  }
 }
 require(changed==1,"diagnostic global flag");std::memcpy(bytes.data(),words.data(),bytes.size());ComputeHashRetail(reinterpret_cast<BYTE *>(bytes.data())+20,static_cast<UINT32>(bytes.size()-20),reinterpret_cast<BYTE *>(bytes.data())+4);return bytes;
}
// Return a chosen loop iteration and t0 site's coordinate in output-normalized
// units. Used only to distinguish shader arithmetic from texture filtering.
std::vector<std::byte> coordinate_shader(std::span<const std::byte> bytes,unsigned sample,unsigned site,float factor){
 std::vector<unsigned> words(bytes.size()/4);std::memcpy(words.data(),bytes.data(),bytes.size());
 std::vector<std::vector<unsigned>> chunks;unsigned sites=0;
 for(unsigned i=0;i<words[7];++i){const auto at=words[8+i]/4,tag=words[at],count=words[at+1]/4;if(tag==0x54415453)continue;
  std::vector<unsigned> data(words.begin()+at+2,words.begin()+at+2+count);
  if(tag==0x58454853){std::vector<unsigned> code{data[0],0};
   for(std::size_t p=2;p<data.size();){const auto n=(data[p]>>24)&127U;require(n&&p+n<=data.size(),"coordinate token bounds");
    if((data[p]&2047)==0x68)data[p+1]=10;
    if(n==11&&data[p]==0x8b000045&&data[p+7]==0x00107e46&&data[p+8]==0){
     if(sites++==site){
     const auto f=std::bit_cast<unsigned>(factor);
     code.insert(code.end(),{0x07000020,0x00100012,9,0x0010001a,4,0x00004001,sample,
      0x0304001f,0x0010000a,9,
      0x0a180038,0x00102032,0,data[p+5],data[p+6],0x00004002,f,f,0,0,
      0x05000036,0x001020c2,0,0x00004001,0,0x0100003e,0x01000015});}
    }
    code.insert(code.end(),data.begin()+p,data.begin()+p+n);p+=n;
   }code[1]=static_cast<unsigned>(code.size());data=std::move(code);
  }
  data.insert(data.begin(),{tag,static_cast<unsigned>(data.size()*4)});chunks.push_back(std::move(data));
 }
 require(sites==4,"coordinate four t0 sites");std::vector<unsigned> out(8+chunks.size());out[0]=0x43425844;out[5]=1;out[7]=static_cast<unsigned>(chunks.size());
 for(std::size_t i=0;i<chunks.size();++i){out[8+i]=static_cast<unsigned>(out.size()*4);out.insert(out.end(),chunks[i].begin(),chunks[i].end());}out[6]=static_cast<unsigned>(out.size()*4);
 auto ptr=reinterpret_cast<BYTE *>(out.data());ComputeHashRetail(ptr+20,static_cast<UINT32>(out.size()*4-20),ptr+4);std::vector<std::byte> result(out.size()*4);std::memcpy(result.data(),out.data(),result.size());return result;
}
struct Fixture {
 ComPtr<ID3D12Device> device;ComPtr<ID3D12InfoQueue> info;ComPtr<ID3D12CommandQueue> queue;
 ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;ComPtr<ID3D12RootSignature> root;
 std::array<std::vector<std::byte>,2> vs,ps;UINT64 sequence{};LUID luid{};unsigned vendor{};
 bool capture_copy{},precise_diagnostic{},runtime_effects{},rgb_write_mask{};int coordinate_sample{-1};unsigned coordinate_site{};std::vector<unsigned> last_copy;
 Fixture(const std::filesystem::path &dir,bool warp,bool point=false){
  ComPtr<ID3D12Debug> debug;hr(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)),"debug");debug->EnableDebugLayer();ComPtr<ID3D12Debug1> gpu;hr(debug.As(&gpu),"GPU validation");gpu->SetEnableGPUBasedValidation(TRUE);
  ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter1> adapter;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");
  if(warp)hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"WARP");else{unsigned matches=0;for(unsigned i=0;;++i){ComPtr<IDXGIAdapter1> candidate;const auto status=factory->EnumAdapters1(i,&candidate);if(status==DXGI_ERROR_NOT_FOUND)break;hr(status,"adapter enumeration");DXGI_ADAPTER_DESC1 desc{};hr(candidate->GetDesc1(&desc),"adapter desc");if(desc.VendorId==0x10de&&!(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){adapter=candidate;++matches;}}require(matches==1,"one NVIDIA adapter");}
  DXGI_ADAPTER_DESC1 ad{};hr(adapter->GetDesc1(&ad),"selected adapter");luid=ad.AdapterLuid;vendor=ad.VendorId;
  hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)),"device");hr(device.As(&info),"info queue");
  D3D12_COMMAND_QUEUE_DESC q{};hr(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");hr(device->CreateCommandAllocator(q.Type,IID_PPV_ARGS(&allocator)),"allocator");hr(device->CreateCommandList(0,q.Type,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"list");hr(list->Close(),"initial close");hr(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");
  D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,3,0,0,0};std::array<D3D12_ROOT_PARAMETER,7> parameters{};
  for(unsigned i=0;i<6;++i){parameters[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[i].Descriptor.ShaderRegister=i;parameters[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
  parameters[6].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[6].DescriptorTable={1,&range};parameters[6].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
  std::array<D3D12_STATIC_SAMPLER_DESC,3> samplers{};for(unsigned i=0;i<3;++i){auto &s=samplers[i];s.Filter=(i==1||(point&&i==0))?D3D12_FILTER_MIN_MAG_MIP_POINT:D3D12_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.ShaderRegister=i;s.MaxLOD=0;s.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;s.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
  D3D12_ROOT_SIGNATURE_DESC rd{7,parameters.data(),3,samplers.data(),D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};ComPtr<ID3DBlob> b,e;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&b,&e),"root serialize");hr(device->CreateRootSignature(0,b->GetBufferPointer(),b->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
  vs[0]=read(dir/L"730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd.vs.dxbc");ps[0]=read(dir/L"be6127ba48008790fd0e93558b8248cb8acd7c0577899bf50d28e5618ae37b79.ps.dxbc");
  vs[1]=read(dir/L"b980e376575d82610809066f45c6ecd6efb090bbee76968d9ee5203d2459a1d2.vs.dxbc");ps[1]=read(dir/L"90f3f4d5594357e5350acf3d5bda57db4641b50e6aaa5787d3ae341463860741.ps.dxbc");
 }
 void wait(){hr(queue->Signal(fence.Get(),++sequence),"signal");HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"event");const auto result=fence->SetEventOnCompletion(sequence,event);const auto status=SUCCEEDED(result)?WaitForSingleObject(event,30000):WAIT_FAILED;CloseHandle(event);hr(result,"fence");require(status==WAIT_OBJECT_0,"GPU wait");hr(device->GetDeviceRemovedReason(),"device health");}
 void messages(){for(UINT64 i=0;i<info->GetNumStoredMessages();++i){SIZE_T n{};hr(info->GetMessage(i,nullptr,&n),"message size");std::vector<std::byte>b(n);auto m=reinterpret_cast<D3D12_MESSAGE *>(b.data());hr(info->GetMessage(i,m,&n),"message");if(m->Severity<=D3D12_MESSAGE_SEVERITY_WARNING){std::cerr<<m->pDescription<<'\n';throw std::runtime_error("GPU validation warning/error");}}}
 unsigned reject_partial_masks(){
  const sf::FilterGeometry g{48,32,96,64};const auto v=sf::dependency_vertices(g,false);
  std::array<float,16> copy{};for(unsigned i=0;i<4;++i){std::copy(v[i].position.begin(),v[i].position.end(),copy.begin()+4*i);std::copy(v[i].scene_uv.begin(),v[i].scene_uv.end(),copy.begin()+4*i+2);}
  const std::array<D3D12_INPUT_ELEMENT_DESC,4> layout{{{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
  unsigned rejected=0;
  for(unsigned stage=0;stage<2;++stage)for(unsigned mask:{0U,1U,3U,7U,8U,15U}){
   sf::EffectRecipe r;r.stage=stage?sf::EffectStage::radial:sf::EffectStage::scene_copy;r.geometry=g;r.root=root.Get();r.vertex_shader=vs[stage];r.pixel_shader=ps[stage];r.input_layout=std::span(layout).first(stage?4:2);
   r.dependency_vertices=stage?std::as_bytes(std::span<const sf::DependencyVertex>(v)):std::as_bytes(std::span<const float>(copy));
   r.raster.FillMode=D3D12_FILL_MODE_SOLID;r.raster.CullMode=D3D12_CULL_MODE_NONE;r.raster.FrontCounterClockwise=TRUE;
   r.blend.IndependentBlendEnable=TRUE;
   for(auto &b:r.blend.RenderTarget){b.SrcBlend=b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlend=b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOp=b.BlendOpAlpha=D3D12_BLEND_OP_ADD;b.RenderTargetWriteMask=15;}
   auto &b=r.blend.RenderTarget[0];b.RenderTargetWriteMask=static_cast<UINT8>(mask);if(stage){b.BlendEnable=TRUE;b.SrcBlend=D3D12_BLEND_SRC_ALPHA;b.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;}
   r.depth.DepthFunc=D3D12_COMPARISON_FUNC_NEVER;r.depth.FrontFace=r.depth.BackFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_NEVER};
   bool accepted=false;try{sf::Effect effect(device.Get(),r);accepted=true;}catch(const std::exception &e){require(std::string(e.what())=="effect native recipe","mask rejected at recipe gate");}
   require(accepted==(mask==15||(stage&&mask==7)),"RGB mask admission");if(!accepted)++rejected;
  }
  return rejected;
 }
 std::vector<unsigned> draw(sf::FilterGeometry g,unsigned scenario,Mode mode,bool packed){
  const unsigned w=g.output_width,h=g.output_height,scale=w/g.render_width;
  const bool reference=mode==Mode::reference;const auto temp_format=packed?DXGI_FORMAT_R10G10B10A2_UNORM:DXGI_FORMAT_R32G32B32A32_FLOAT;
  const bool production=runtime_effects&&mode==Mode::adapted&&packed;
  std::array<std::unique_ptr<sf::Effect>,2> effects;
  hr(allocator->Reset(),"allocator reset");hr(list->Reset(allocator.Get(),nullptr),"list reset");
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=4;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;ComPtr<ID3D12DescriptorHeap> heap,rtvs;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"SRV heap");hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtvs)),"RTV heap");
  const auto srv_step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),rtv_step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  std::array<ComPtr<ID3D12Resource>,4> inputs;std::vector<Pixels> uploads;uploads.reserve(3);
  for(unsigned i:{0U,2U,3U}){const unsigned tw=i==0?w:i==2?1U:16U,th=i==0?h:i==2?1U:16U;auto desc=texture(tw,th,DXGI_FORMAT_R32G32B32A32_FLOAT);inputs[i]=resource(device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);uploads.emplace_back(device.Get(),desc,D3D12_HEAP_TYPE_UPLOAD);std::vector<float> pixels(std::size_t(tw)*th*4);
   for(unsigned y=0;y<th;++y)for(unsigned x=0;x<tw;++x){auto *p=pixels.data()+(std::size_t(y)*tw+x)*4;for(unsigned k=0;k<4;++k)p[k]=i==2?.65F:i==3?.1F+.8F*float((x*5+y*7+k*3)%19)/18.F:.05F+.8F*float((x*3+y*11+k*13)%31)/30.F;}
   uploads.back().write(std::as_bytes(std::span(pixels)));uploads.back().copy(list.Get(),inputs[i].Get(),true);barrier(list.Get(),inputs[i].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  }
  auto temp=texture(reference?w*scale:w,reference?h*scale:h,temp_format);temp.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;inputs[1]=resource(device.Get(),temp,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);
  auto output_desc=texture(w,h,packed?DXGI_FORMAT_R11G11B10_FLOAT:DXGI_FORMAT_R32G32B32A32_FLOAT);output_desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;auto output=resource(device.Get(),output_desc,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);Pixels readback(device.Get(),output_desc,D3D12_HEAP_TYPE_READBACK);
  std::unique_ptr<Pixels> copy_read;if(capture_copy)copy_read=std::make_unique<Pixels>(device.Get(),temp,D3D12_HEAP_TYPE_READBACK);
  for(unsigned i=0;i<4;++i){D3D12_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=inputs[i]->GetDesc().Format;sd.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sd.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;device->CreateShaderResourceView(inputs[i].Get(),&sd,{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(i)*srv_step});}
  const std::array<D3D12_CPU_DESCRIPTOR_HANDLE,2> handles{{rtvs->GetCPUDescriptorHandleForHeapStart(),{rtvs->GetCPUDescriptorHandleForHeapStart().ptr+rtv_step}}};device->CreateRenderTargetView(inputs[1].Get(),nullptr,handles[0]);device->CreateRenderTargetView(output.Get(),nullptr,handles[1]);
  // Independent oracle changes the reference texture layout and CBScreen
  // inverse size. It executes UNMODIFIED native shaders, never adapter output.
  std::array<float,704> constants{};const unsigned offsets[]{0,256,512,768,2048,2304};auto screen=constants.data()+64;
  screen[6]=(reference||mode==Mode::bad_color?1.F/scale:mode==Mode::bad_position?2.F:1.F)/w;screen[7]=(reference||mode==Mode::bad_color?1.F/scale:mode==Mode::bad_position?2.F:1.F)/h;
  auto blur=constants.data()+192;blur[0]=.41F/scale;blur[1]=.57F/scale;blur[2]=1.F;blur[3]=scenario&1?.24F:0.F;
  blur[4]=.22F;blur[5]=.18F;blur[6]=.25F;blur[7]=.7F;blur[8]=.03F;blur[9]=.3F;blur[10]=.7F;blur[11]=std::bit_cast<float>(4U);
  if(scenario&2){blur[12]=.3F;blur[13]=-.2F;blur[14]=.5F;}blur[15]=.75F;
  for(unsigned i=0;i<256;++i)blur[16+i]=.1F+.8F*float((i*7)%37)/36.F;
  auto clamp=constants.data()+512;clamp[0]=.5F/w;clamp[1]=.5F/h;clamp[2]=1.F/scale-.5F/w;clamp[3]=1.F/scale-.5F/h;
  auto functions=constants.data()+576;for(unsigned i=0;i<4;++i)functions[i]=std::bit_cast<float>(unsigned((scenario>>(i+2))&1));
  auto cb=buffer(device.Get(),sizeof(constants),D3D12_HEAP_TYPE_UPLOAD);upload(cb.Get(),std::as_bytes(std::span(constants)));
  std::array<ComPtr<ID3D12Resource>,2> vbs;std::array<ComPtr<ID3D12PipelineState>,2> pipelines;std::array<std::vector<std::byte>,2> pixels=ps;
  if(!reference&&!production&&mode!=Mode::bad_color)pixels[1]=sf::make_radial_shader(ps[1],g);
  if(precise_diagnostic)pixels[1]=disable_refactoring(std::move(pixels[1]));
  if(coordinate_sample>=0)pixels[1]=coordinate_shader(pixels[1],unsigned(coordinate_sample),coordinate_site,reference?float(scale):1.F);
  const std::array<D3D12_INPUT_ELEMENT_DESC,4> layout{{{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
  for(unsigned i=0;i<2;++i){D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=root.Get();p.VS={vs[i].data(),vs[i].size()};p.PS={pixels[i].data(),pixels[i].size()};p.BlendState.RenderTarget[0].RenderTargetWriteMask=15;p.SampleMask=UINT_MAX;p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.InputLayout={layout.data(),i?4U:2U};p.NumRenderTargets=1;p.RTVFormats[0]=i?output_desc.Format:temp.Format;p.SampleDesc.Count=1;
   // The observed native radial pass writes RGB (7) into an RGB-only target.
   // Keep the reference and the production recipe identical, including mask.
   if(i&&rgb_write_mask)p.BlendState.RenderTarget[0].RenderTargetWriteMask=7;
   // Blend the radial result with the existing scene, as an overlay may do.
   if(i&&(scenario&32)&&coordinate_sample<0){auto &b=p.BlendState.RenderTarget[0];b.BlendEnable=TRUE;b.SrcBlend=D3D12_BLEND_SRC_ALPHA;b.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;b.BlendOp=D3D12_BLEND_OP_ADD;b.SrcBlendAlpha=D3D12_BLEND_ONE;b.DestBlendAlpha=D3D12_BLEND_ZERO;b.BlendOpAlpha=D3D12_BLEND_OP_ADD;}
   hr(device->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&pipelines[i])),"pipeline");
   if(production){
    const auto native=sf::dependency_vertices(g,false);std::array<float,16> copy{};
    for(unsigned n=0;n<4;++n){std::copy(native[n].position.begin(),native[n].position.end(),copy.begin()+4*n);std::copy(native[n].scene_uv.begin(),native[n].scene_uv.end(),copy.begin()+4*n+2);}
    sf::EffectRecipe r;r.stage=i?sf::EffectStage::radial:sf::EffectStage::scene_copy;r.geometry=g;r.root=root.Get();r.vertex_shader=vs[i];r.pixel_shader=ps[i];
    r.blend=p.BlendState;r.raster=p.RasterizerState;r.depth=p.DepthStencilState;r.input_layout=std::span(layout).first(i?4:2);
    r.dependency_vertices=i?std::as_bytes(std::span<const sf::DependencyVertex>(native)):std::as_bytes(std::span<const float>(copy));
    effects[i]=std::make_unique<sf::Effect>(device.Get(),r);
   }
   std::array<sf::DependencyVertex,4> v;
   if(reference){const float u=i?1.F/scale:1.F,t=i?1.F/scale:1.F;v={{{{-1,1},{0,0},{0,0}},{{-1,-1},{0,t},{0,1}},{{1,1},{u,0},{1,0}},{{1,-1},{u,t},{1,1}}}};}
   else v=sf::dependency_vertices(g,i==0&&!production);
   if(mode==Mode::bad_auxiliary&&i)for(auto &x:v)for(auto &u:x.auxiliary_uv)u/=scale;
   if(i){vbs[i]=buffer(device.Get(),sizeof(v),D3D12_HEAP_TYPE_UPLOAD);upload(vbs[i].Get(),std::as_bytes(std::span(v)));}
   else{std::array<float,64> packed_vertices{};for(unsigned n=0;n<4;++n){std::copy(v[n].position.begin(),v[n].position.end(),packed_vertices.begin()+48+4*n);std::copy(v[n].scene_uv.begin(),v[n].scene_uv.end(),packed_vertices.begin()+50+4*n);}vbs[i]=buffer(device.Get(),sizeof(packed_vertices),D3D12_HEAP_TYPE_UPLOAD);upload(vbs[i].Get(),std::as_bytes(std::span(packed_vertices)));}
  }
  namespace cd=sf::color_dependencies;const std::array<cd::Resource,4> resources{{{10,cd::Domain::scene},{20,cd::Domain::unknown},{30,cd::Domain::auxiliary},{40,cd::Domain::auxiliary}}};const std::array<cd::Draw,2> draws{{{20,{10},1},{10,{20,30,40},3}}};cd::Plan plan;require(cd::build(resources,draws,10,plan)&&plan.steps[1].inputs[0].producer==0,"scene dependency plan");
  auto *l=list.Get();ID3D12DescriptorHeap *heaps[]{heap.Get()};l->SetDescriptorHeaps(1,heaps);l->SetGraphicsRootSignature(root.Get());for(unsigned i=0;i<6;++i)l->SetGraphicsRootConstantBufferView(i,cb->GetGPUVirtualAddress()+offsets[i]);l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  for(unsigned i=0;i<2;++i){l->SetPipelineState(pipelines[i].Get());l->SetGraphicsRootConstantBufferView(3,cb->GetGPUVirtualAddress()+(i?768:2560));l->SetGraphicsRootDescriptorTable(6,{heap->GetGPUDescriptorHandleForHeapStart().ptr+SIZE_T(i)*srv_step});const D3D12_VERTEX_BUFFER_VIEW vb{vbs[i]->GetGPUVirtualAddress(),i?112U:256U,i?28U:16U};l->IASetVertexBuffers(0,1,&vb);
   const unsigned dw=production||(mode==Mode::bad_copy&&!i)?g.render_width:w,dh=production||(mode==Mode::bad_copy&&!i)?g.render_height:h;const D3D12_VIEWPORT viewport{0,0,float(dw),float(dh),0,1};const D3D12_RECT scissor{0,0,static_cast<LONG>(dw),static_cast<LONG>(dh)};l->RSSetViewports(1,&viewport);l->RSSetScissorRects(1,&scissor);l->OMSetRenderTargets(1,&handles[i],FALSE,nullptr);const float clear[]{.9F,.1F,.8F,.7F};l->ClearRenderTargetView(handles[i],clear,0,nullptr);
   if(production){sf::EffectBindings b{l,pipelines[i].Get(),root.Get(),i?output.Get():inputs[1].Get(),vbs[i].Get(),vb,viewport,scissor,D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,4,i?0U:12U};require(effects[i]->record(b),"production dependency draw");}
   else l->DrawInstanced(4,1,i?0:12,0);
   if(!i){if(copy_read){barrier(l,inputs[1].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);copy_read->copy(l,inputs[1].Get(),false);barrier(l,inputs[1].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}else barrier(l,inputs[1].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}
  }
  barrier(l,output.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);readback.copy(l,output.Get(),false);hr(l->Close(),"close");ID3D12CommandList *batch[]{l};queue->ExecuteCommandLists(1,batch);wait();messages();
  if(copy_read){const auto all=copy_read->read();last_copy.clear();const auto row_words=w*(packed?1U:4U);for(unsigned y=0;y<h;++y)last_copy.insert(last_copy.end(),all.begin()+y*copy_read->row/4,all.begin()+y*copy_read->row/4+row_words);}
  if(production){hr(allocator->Reset(),"production allocator retirement");hr(list->Reset(allocator.Get(),nullptr),"production recording retirement");hr(list->Close(),"production retired close");for(auto &effect:effects)effect->retire(fence.Get(),sequence,true);}
  return readback.read();
 }
};
void reflection(std::span<const std::byte>a,std::span<const std::byte>b){ComPtr<ID3D11ShaderReflection>x,y;hr(D3DReflect(a.data(),a.size(),IID_PPV_ARGS(&x)),"native reflection");hr(D3DReflect(b.data(),b.size(),IID_PPV_ARGS(&y)),"adapted reflection");D3D11_SHADER_DESC xd{},yd{};hr(x->GetDesc(&xd),"native desc");hr(y->GetDesc(&yd),"adapted desc");require(xd.BoundResources==yd.BoundResources&&xd.ConstantBuffers==yd.ConstantBuffers&&xd.InputParameters==yd.InputParameters&&xd.OutputParameters==yd.OutputParameters,"reflection shape");for(unsigned i=0;i<xd.BoundResources;++i){D3D11_SHADER_INPUT_BIND_DESC p{},q{};hr(x->GetResourceBindingDesc(i,&p),"native resource");hr(y->GetResourceBindingDesc(i,&q),"adapted resource");require(std::strcmp(p.Name,q.Name)==0&&p.Type==q.Type&&p.BindPoint==q.BindPoint&&p.BindCount==q.BindCount&&p.Dimension==q.Dimension,"binding unchanged");}}
void run(int argc,wchar_t **argv){require(argc==4||argc==5,"usage: scaled_dependency_fixture warp|gpu shader-directory output-directory [diagnostic|diagnostic-precise|diagnostic-point|diagnostic-coordinates]");const bool warp=std::wstring(argv[1])==L"warp";require(warp||std::wstring(argv[1])==L"gpu","adapter mode");const std::filesystem::path out=argv[3];require(!std::filesystem::exists(out),"preserve output directory");std::filesystem::create_directories(out);Fixture f(argv[2],warp,argc==5&&std::wstring(argv[4])==L"diagnostic-point");unsigned rejected=0,comparisons=0,cases=0,negative=0,total_different=0,max_steps=0,exact_cases=0;double max_error=0;std::ofstream report(out/L"cases.jsonl");
 if(argc==5&&std::wstring(argv[4])==L"diagnostic-coordinates"){
  for(unsigned w:{96U,128U})for(unsigned scenario:{1U,3U,63U})for(unsigned site=0;site<4;++site)for(int sample=0;sample<4;++sample){
   if((scenario&2)?site==3:site!=3)continue;
   f.coordinate_sample=sample;f.coordinate_site=site;const sf::FilterGeometry g{w/2,32,w,64};const auto expected=f.draw(g,scenario,Mode::reference,false),actual=f.draw(g,scenario,Mode::adapted,false);unsigned different=0;double maximum=0;
   for(std::size_t i=0;i<actual.size();++i){if(actual[i]!=expected[i])++different;const auto a=std::bit_cast<float>(actual[i]),b=std::bit_cast<float>(expected[i]);require(std::isfinite(a)&&std::isfinite(b),"finite coordinate");maximum=(std::max)(maximum,std::abs(double(a)-b));}
   report<<"{\"width\":"<<w<<",\"scenario\":"<<scenario<<",\"site\":"<<site<<",\"sample\":"<<sample<<",\"different\":"<<different<<",\"max_error\":"<<maximum<<"}\n"<<std::flush;
  }return;
 }
 f.rgb_write_mask=argc==5&&std::wstring(argv[4])==L"runtime-effects-rgb";
 f.runtime_effects=f.rgb_write_mask||(argc==5&&std::wstring(argv[4])==L"runtime-effects");
 if(f.runtime_effects)rejected+=f.reject_partial_masks();
 if(argc==5&&!f.runtime_effects){const std::wstring mode=argv[4];require(mode==L"diagnostic"||mode==L"diagnostic-precise"||mode==L"diagnostic-point","diagnostic mode");f.capture_copy=true;f.precise_diagnostic=mode==L"diagnostic-precise";
  for(unsigned w:{96U,128U})for(unsigned scenario:{0U,1U,3U,63U})for(bool packed:{false,true}){
   const sf::FilterGeometry g{w/2,32,w,64};const auto expected=f.draw(g,scenario,Mode::reference,packed),copy=f.last_copy,actual=f.draw(g,scenario,Mode::adapted,packed);unsigned different=0;double maximum=0;
   for(std::size_t i=0;i<actual.size();++i){if(actual[i]!=expected[i])++different;if(!packed){const auto a=std::bit_cast<float>(actual[i]),b=std::bit_cast<float>(expected[i]);require(std::isfinite(a)&&std::isfinite(b),"finite diagnostic");maximum=(std::max)(maximum,std::abs(double(a)-b));}}
   const auto name=std::to_wstring(w)+L"-"+std::to_wstring(scenario)+(packed?L"-packed":L"-float");save(out/(name+L"-reference.bin"),std::as_bytes(std::span(expected)));save(out/(name+L"-actual.bin"),std::as_bytes(std::span(actual)));
   report<<"{\"width\":"<<w<<",\"scenario\":"<<scenario<<",\"packed\":"<<(packed?"true":"false")<<",\"copy_exact\":"<<(copy==f.last_copy?"true":"false")<<",\"components\":"<<actual.size()<<",\"different\":"<<different<<",\"max_float_error\":"<<maximum<<"}\n"<<std::flush;
  }return;
 }
 const auto rejects=[&](auto fn){bool caught=false;try{fn();}catch(const std::exception &){caught=true;}require(caught,"negative admission");++rejected;};
 const sf::FilterGeometry scaled{48,32,96,64};for(auto g:{sf::FilterGeometry{0,32,96,64},sf::FilterGeometry{48,32,96,32},sf::FilterGeometry{48,32,144,96}})rejects([&]{sf::make_radial_shader(f.ps[1],g);});
 for(unsigned at:{0U,4U,24U,28U,4000U,6443U}){auto bad=f.ps[1];bad[at]^=std::byte{1};rejects([&]{sf::make_radial_shader(bad,scaled);});}
 f.capture_copy=true;
 for(auto g:{sf::FilterGeometry{96,64,96,64},scaled,sf::FilterGeometry{64,32,128,64}}){auto adapted=sf::make_radial_shader(f.ps[1],g);reflection(f.ps[1],adapted);const auto name=std::to_wstring(g.render_width)+L"-"+std::to_wstring(g.output_width);save(out/(name+L".dxbc"),adapted);ComPtr<ID3DBlob> asm_blob;hr(D3DDisassemble(adapted.data(),adapted.size(),0,nullptr,&asm_blob),"disassembly");save(out/(name+L".asm"),{static_cast<const std::byte *>(asm_blob->GetBufferPointer()),asm_blob->GetBufferSize()});
  for(bool packed:{false,true})for(unsigned scenario=0;scenario<64;++scenario){if(f.runtime_effects&&!packed)continue;const auto expected=f.draw(g,scenario,Mode::reference,packed),copy=f.last_copy,actual=f.draw(g,scenario,Mode::adapted,packed);const auto delta=compare(actual,expected,packed);require(copy==f.last_copy,"exact copy coverage");
   const bool exact_required=warp||g.render_width==g.output_width||g.output_width==128;
   report<<"{\"render_width\":"<<g.render_width<<",\"output_width\":"<<g.output_width<<",\"packed\":"<<(packed?"true":"false")<<",\"scenario\":"<<scenario<<",\"copy_exact\":true,\"different\":"<<delta.different<<",\"max_color_error\":"<<delta.maximum<<",\"max_packed_steps\":"<<delta.packed_steps<<"}\n"<<std::flush;
   require(exact_required?delta.different==0:packed?delta.packed_budget_ratio<=1:delta.maximum<=linear_float_budget,"native oracle error budget");
   ++cases;if(!delta.different)++exact_cases;comparisons+=static_cast<unsigned>(actual.size());total_different+=delta.different;max_steps=(std::max)(max_steps,delta.packed_steps);max_error=(std::max)(max_error,delta.maximum);
  }
 }
 const auto reference=f.draw(scaled,63,Mode::reference,false);for(auto mode:{Mode::bad_copy,Mode::bad_position,Mode::bad_auxiliary,Mode::bad_color}){const auto delta=compare(f.draw(scaled,63,mode,false),reference,false,false);report<<"{\"negative_control\":"<<unsigned(mode)<<",\"max_error\":"<<delta.maximum<<",\"budget\":"<<linear_float_budget<<"}\n"<<std::flush;require(delta.maximum>linear_float_budget*2,"negative coordinate control must exceed numerical budget");++negative;}
 f.messages();hr(f.allocator->Reset(),"retire allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"retire recording");hr(f.list->Close(),"retire close");std::ofstream result(out/L"result.json");result<<"{\"passed\":true,\"warp\":"<<(warp?"true":"false")<<",\"adapter_luid\":["<<f.luid.HighPart<<','<<f.luid.LowPart<<"],\"vendor\":"<<f.vendor<<",\"cases\":"<<cases<<",\"exact_cases\":"<<exact_cases<<",\"draws\":"<<f.sequence*2<<",\"component_comparisons\":"<<comparisons<<",\"different\":"<<total_different<<",\"max_color_error\":"<<max_error<<",\"max_packed_steps\":"<<max_steps<<",\"linear_float_budget\":"<<linear_float_budget<<",\"packed_filter_budget_before_output_quantization\":"<<packed_filter_budget<<",\"negative_controls\":"<<negative<<",\"rejected_inputs\":"<<rejected<<",\"gpu_warnings_errors\":0,\"live_integration\":false}\n";require(bool(result)&&bool(report),"report write");std::cout<<"PASS "<<cases<<" cases ("<<exact_cases<<" exact), "<<comparisons<<" component comparisons, "<<negative<<" negative controls\n";
}
}
int wmain(int argc,wchar_t **argv){try{run(argc,argv);return 0;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
