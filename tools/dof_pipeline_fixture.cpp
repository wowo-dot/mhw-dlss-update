// Offline native DOF pipeline experiment. Private DXBC comes only from argv.
// Synthetic color and explicit fixture views do not claim the game's live contract.
#include "dof_fixture_support.hpp"
#include "../src/scaled_temporal/dof_compute_shader.hpp"
#include "dof_native_formats.hpp"
#include "dof_depth_oracle.hpp"
#include <bcrypt.h>
#include <sstream>
#include "../tests/perf_env.hpp"
#ifdef MHW_DOF_NATIVE_ROUTE_FIXTURE
#include "../tests/dof_native_route_fixture.hpp"
#endif
namespace {
using namespace dof_fixture;
constexpr unsigned side=320;
const std::array<const char *,6> hashes{
 "98c622bab8c0e113eeb4062dcc78fec129441854bcf67f61db6ed6239ca68e3c",
 "d2c5eca59598bd0ebdd67cd2d5e3858e79862a50a8e076423b7c6a3f498f6245",
 "4f54cf139cc8222b235670375faa7a8f0970bf51a699c30b82357bfa8f6001d8",
 "b7afd17a818940e9f2da3830ad2e208767930f4decd1572b4fb64b164c85a532",
 "c4c5caf1ae7f1cdfc6e2762ef954f51ed65b64056cbb02778c963977f84bec7f",
 "61b739ad7861bffa528e0f9a1b58958765549cd0aac1d8e6a2b5366cc11ac6db"};
std::string digest(std::span<const std::byte> bytes){
 std::array<unsigned char,32> hash{};
 require(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<std::byte *>(bytes.data())),
  static_cast<ULONG>(bytes.size()),hash.data(),32)>=0,"SHA256");
 constexpr char hex[]="0123456789abcdef";std::string result;
 for(auto x:hash){result+=hex[x>>4];result+=hex[x&15];}return result;
}
#include "dof_prepare_experiment.hpp"
#include "dof_reduction_experiment.hpp"
// A private 1792-byte CB upload assembled from one captured frame. The three
// original typed allocations occupy offsets 0, 1280 and 1536; padding is zero.
struct Captured {
 std::array<float,448> cb{};
 unsigned width{},height{},view_width{},view_height{};
 explicit Captured(const std::filesystem::path &path){
  const auto bytes=read(path);require(bytes.size()==sizeof(cb),"captured constant pack extent");std::memcpy(cb.data(),bytes.data(),bytes.size());
  auto screen=cb.data()+320;require(screen[4]==2560.F&&screen[5]==1440.F,"captured screen dimensions");
  width=2560;height=1440;
  view_width=std::bit_cast<unsigned>(screen[10]);view_height=std::bit_cast<unsigned>(screen[11]);
  require(width==2560&&height==1440&&view_width==1280&&view_height==720,"captured allocation/view contract");
  require(screen[4]==float(width)&&screen[5]==float(height)&&screen[6]==1.F/width&&screen[7]==1.F/height&&
   screen[8]==0&&screen[9]==0&&screen[12]==0&&screen[13]==0&&screen[14]==float(view_width)&&screen[15]==float(view_height)&&
   screen[16]==1.F/view_width&&screen[17]==1.F/view_height&&screen[26]==0,"captured screen fields");
  const auto blur=cb.data()+384;require(std::bit_cast<unsigned>(blur[11])==1&&std::bit_cast<unsigned>(blur[12])==1&&blur[13]==1&&blur[14]==1,"captured reduction flags");
 }
};
// Opt-in complete R32F input. Pairing to the constant pack is recorded by the
// extraction receipt; this executable also reports both input hashes.
struct CapturedDepth {
 std::vector<float> pixels;
 std::string sha256;
 CapturedDepth(const std::filesystem::path &path,const Captured &c){
  const auto bytes=static_cast<std::size_t>(c.width)*c.height*sizeof(float);
  require(std::filesystem::file_size(path)==bytes,"complete captured depth extent");
  pixels.resize(bytes/sizeof(float));std::ifstream f(path,std::ios::binary);
  require(bool(f.read(reinterpret_cast<char *>(pixels.data()),static_cast<std::streamsize>(bytes))),"read captured depth");
  for(const auto d:pixels)require(std::isfinite(d)&&d>=0&&d<=1,"captured depth value");
  sha256=digest(std::as_bytes(std::span(pixels)));
 }
 float at(unsigned x,unsigned y,const Captured &c,bool clear_outside=false)const{
  return clear_outside&&(x>=c.view_width||y>=c.view_height)?0.F:pixels[static_cast<std::size_t>(y)*c.width+x];
 }
};
std::array<float,4> scene(unsigned x,unsigned y){
 // Exactly representable high-frequency data; a box reference cannot hide a
 // mistaken nearest-neighbor read or an unscaled upper-left-quadrant read.
 return {.125F+float((x*7+y*3)%13)/32.F,.125F+float((x*3+y*11)%17)/32.F,.125F+float((x+y*5)%19)/32.F,1.F};
}
float captured_depth(unsigned x,unsigned y,unsigned vw,unsigned vh,bool poison){
 if(x>=vw||y>=vh)return poison?.001F:.99F;
 const auto tile=(x/20+y/20)%3;return tile==0?.02F:tile==1?.35F:.85F;
}
struct Result {std::array<std::vector<float>,7> stages;std::vector<float> output,depth;};
struct Pipeline : Gpu {
 bool native_formats{},sync_reduction{},gpu_validation{true};
 ComPtr<ID3D12RootSignature> root;
 std::filesystem::path shader_directory;ComPtr<ID3D12PipelineState> original_horizontal;
 std::array<ComPtr<ID3D12PipelineState>,6> compute;
 ComPtr<ID3D12PipelineState> composite,adapted_compute,adapted_composite;
 // Keep isolated-case allocations distinct until the validation experiment
 // ends; GPU validation also owns asynchronous per-resource instrumentation.
 std::vector<ComPtr<IUnknown>> retained;
 explicit Pipeline(bool warp,const std::filesystem::path &dir,const Captured *capture=nullptr,bool actual_formats=false,bool synchronized=false,bool validation=true):Gpu(warp,validation),native_formats(actual_formats),sync_reduction(synchronized),gpu_validation(validation){
  shader_directory=dir;
  require(!native_formats||capture,"native formats require captured dimensions/constants");
  std::array<D3D12_ROOT_PARAMETER,5> params{};
  for(unsigned i=0;i<3;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[i].Descriptor.ShaderRegister=i==2?3:i;}
  std::array<D3D12_DESCRIPTOR_RANGE,2> ranges{{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,5,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,2,0,0,0}}};
  for(unsigned i=0;i<2;++i){params[3+i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[3+i].DescriptorTable={1,&ranges[i]};}
  std::array<D3D12_STATIC_SAMPLER_DESC,2> samplers{};
  for(unsigned i=0;i<2;++i){auto &s=samplers[i];s.Filter=i?D3D12_FILTER_MIN_MAG_MIP_LINEAR:D3D12_FILTER_MIN_MAG_MIP_POINT;
   s.AddressU=s.AddressV=s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.MaxLOD=0;s.ShaderRegister=i;s.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;}
  D3D12_ROOT_SIGNATURE_DESC rd{5,params.data(),2,samplers.data(),D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};ComPtr<ID3DBlob> b,e;
  hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&b,&e),"root serialize");
  hr(device->CreateRootSignature(0,b->GetBufferPointer(),b->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
  constexpr unsigned threads[6][3]={{320,1,1},{1,320,1},{8,8,1},{16,16,1},{16,16,1},{16,16,1}};
  for(unsigned i=0;i<6;++i){auto code=read(dir/(std::string(hashes[i])+".cs.dxbc"));require(digest(code)==hashes[i],"compute shader identity");
   if(i==0){D3D12_COMPUTE_PIPELINE_STATE_DESC original{};original.pRootSignature=root.Get();original.CS={code.data(),code.size()};
    hr(device->CreateComputePipelineState(&original,IID_PPV_ARGS(&original_horizontal)),"native horizontal PSO");}
   if(i==0&&sync_reduction){const auto patched=dof_experiment::synchronized_reduction(code);dof_fixture::reflection(code,patched);code=patched;}
   ComPtr<ID3D11ShaderReflection> reflection;hr(D3DReflect(code.data(),code.size(),IID_PPV_ARGS(&reflection)),"CS reflection");
   UINT x{},y{},z{};reflection->GetThreadGroupSize(&x,&y,&z);require(x==threads[i][0]&&y==threads[i][1]&&z==threads[i][2],"thread group contract");
   D3D11_SHADER_DESC shader{};hr(reflection->GetDesc(&shader),"CS description");
   for(unsigned ci=0;ci<shader.ConstantBuffers;++ci){D3D11_SHADER_BUFFER_DESC c{};hr(reflection->GetConstantBufferByIndex(ci)->GetDesc(&c),"CB description");
    const unsigned size=std::strcmp(c.Name,"CBViewProjection")==0?1072:std::strcmp(c.Name,"CBScreen")==0?112:std::strcmp(c.Name,"CBNewDOFFilter")==0?96:0;
    require(size&&c.Size==size,"reflected constant capture extent");}
   D3D12_COMPUTE_PIPELINE_STATE_DESC d{};d.pRootSignature=root.Get();d.CS={code.data(),code.size()};hr(device->CreateComputePipelineState(&d,IID_PPV_ARGS(&compute[i])),"compute PSO");
   if(capture&&i==3){const auto adapted=dof_experiment::prepare(code);d.CS={adapted.data(),adapted.size()};dof_fixture::reflection(code,adapted);
    hr(device->CreateComputePipelineState(&d,IID_PPV_ARGS(&adapted_compute)),"adapted preparation PSO");}
  }
  auto vs=read(dir/L"730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd.vs.dxbc");
  auto ps=read(dir/L"9f04832cb0a0ff800169964919f7ebecba353e7e3c147c2b314e76af7372e3aa.ps.dxbc");
  require(digest(vs)=="730b7d23bcdca40477ed973c29c8fff8de7ab7244b05d142d5f9458fbfec5fdd"&&digest(ps)=="9f04832cb0a0ff800169964919f7ebecba353e7e3c147c2b314e76af7372e3aa","composition shader identity");
  const std::array<D3D12_INPUT_ELEMENT_DESC,4> layout{{{"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,24,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
  D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};d.pRootSignature=root.Get();d.VS={vs.data(),vs.size()};d.PS={ps.data(),ps.size()};d.InputLayout={layout.data(),4};d.SampleMask=UINT_MAX;d.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;d.NumRenderTargets=1;d.RTVFormats[0]=DXGI_FORMAT_R32G32B32A32_FLOAT;d.SampleDesc.Count=1;
  d.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;d.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
  for(auto &t:d.BlendState.RenderTarget){t.SrcBlend=t.SrcBlendAlpha=D3D12_BLEND_ONE;t.DestBlend=t.DestBlendAlpha=D3D12_BLEND_ZERO;t.BlendOp=t.BlendOpAlpha=D3D12_BLEND_OP_ADD;t.RenderTargetWriteMask=15;}
  d.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_NEVER;d.DepthStencilState.FrontFace=d.DepthStencilState.BackFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_NEVER};
  hr(device->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&composite)),"composition PSO");
  if(capture){const auto adapted=sf::make_dof_shader(ps,{capture->view_width,capture->view_height,capture->width,capture->height});
   dof_fixture::reflection(ps,adapted);d.PS={adapted.data(),adapted.size()};hr(device->CreateGraphicsPipelineState(&d,IID_PPV_ARGS(&adapted_composite)),"adapted composition PSO");}
 }
 // Fault=1 poisons the invalid region of external input allocations. Fault=2
 // feeds composition the earlier Image writer, testing chronological reuse.
 Result run(unsigned scenario,unsigned fault,const Captured *capture=nullptr,unsigned mode=0,const CapturedDepth *depth=nullptr){
  // Captured modes: 0 independent proxy/padded-texture reference; 1 adapter;
  // 2 deliberately wrong original color loads; 3 earlier Image writer;
  // 4 reference with different values outside the valid external footprint.
  const unsigned w=capture?capture->width:side,h=capture?capture->height:side;
  require(!depth||(capture&&native_formats&&depth->pixels.size()==static_cast<std::size_t>(w)*h),"live-depth replay contract");
  const bool reconstructed=capture&&mode>=1&&mode<=3,reference=capture&&!reconstructed;
  if(capture&&mode==3)fault=2;
  // Give each isolated pipeline experiment its own recording storage. In
  // particular no instrumented dispatch is recycled into a later experiment.
  hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf())),"case allocator");
  hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(list.ReleaseAndGetAddressOf())),"case list");
  const unsigned dims[11][2]={{w,h},{w,h},{w/20,h},{w/20,h/20},{w/20,h/20},{w/2,h/2},{w/2,h/2},{w/2,h/2},{w/2,h/2},{w,h},{reference?2*w:w,reference?2*h:h}};
  const unsigned resource_count=capture?11:10;
  std::array<ComPtr<ID3D12Resource>,11> resources;std::vector<Pixels> uploads;uploads.reserve(resource_count);
  std::array<D3D12_RESOURCE_STATES,11> states;states.fill(D3D12_RESOURCE_STATE_COPY_DEST);
  for(unsigned i=0;i<resource_count;++i){auto desc=texture(dims[i][0],dims[i][1],native_formats?dof_formats::formats[i]:i==0?DXGI_FORMAT_R32_FLOAT:i==7?DXGI_FORMAT_R32_UINT:i==8?DXGI_FORMAT_R8_UNORM:DXGI_FORMAT_R32G32B32A32_FLOAT);
   // MHW_DOF_POOLED=1: back the CoC with its TYPELESS family, as the game does at 1920x1080.
   if(native_formats&&i==5&&dof_formats::pooled())desc.Format=DXGI_FORMAT_R16G16B16A16_TYPELESS;
   if(i>=2&&i<10)desc.Flags=i==9?D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET:D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
   resources[i]=resource(device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,states[i]);uploads.emplace_back(device.Get(),desc,D3D12_HEAP_TYPE_UPLOAD);
   std::vector<std::byte> bytes(static_cast<size_t>(uploads.back().row)*dims[i][1]);
   if(capture&&(i<2||i==10))for(unsigned y=0;y<dims[i][1];++y)for(unsigned x=0;x<dims[i][0];++x){
    const auto offset=static_cast<size_t>(uploads.back().row)*y;
    if(i==0){const float d=depth?depth->at(x,y,*capture,mode==4):captured_depth(x,y,capture->view_width,capture->view_height,mode==4);std::memcpy(bytes.data()+offset+4*x,&d,4);}
    else {std::array<float,4> color{8.F,0.F,8.F,1.F};
     if(i==10||reconstructed){if(x<w&&y<h)color=scene(x,y);}
     else if(x<capture->view_width&&y<capture->view_height){color={};for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){auto v=scene(2*x+dx,2*y+dy);for(unsigned k=0;k<4;++k)color[k]+=.25F*v[k];}}
     else color=mode==4&&!depth?std::array<float,4>{0.F,12.F,0.F,1.F}:std::array<float,4>{8.F,0.F,8.F,1.F};
     if(native_formats){const auto packed=dof_formats::pack(color);std::memcpy(bytes.data()+offset+4*x,&packed,4);}
     else std::memcpy(bytes.data()+offset+16*x,color.data(),16);}
   }
   if(!capture&&i<2)for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x){
    const bool poison=fault==1&&(x>=side/2||y>=side/2);float depth_value=scenario==0?.5F:((x/20+y/20)%3==0?.15F:(x/20+y/20)%3==1?.5F:.85F);
    if(i==0){if(poison)depth_value=.99F;std::memcpy(bytes.data()+4*(size_t(y)*side+x),&depth_value,4);}
    else {std::array<float,4> color{.1F+.7F*float(x%23)/22.F,.1F+.7F*float(y%29)/28.F,.2F+.5F*float((x+y)%31)/30.F,1.F};if(poison)color={10.F,0.F,10.F,1.F};std::memcpy(bytes.data()+16*(size_t(y)*side+x),color.data(),16);}
   }
   uploads.back().write(bytes);uploads.back().copy(list.Get(),resources[i].Get(),true);
  }
  const auto state=[&](unsigned i,D3D12_RESOURCE_STATES next){if(states[i]!=next){barrier(list.Get(),resources[i].Get(),states[i],next);states[i]=next;}};
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=49;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  ComPtr<ID3D12DescriptorHeap> heap,rtv;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"descriptors");
  hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtv)),"RTV heap");
  const auto stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  const auto cpu=[&](unsigned n){return D3D12_CPU_DESCRIPTOR_HANDLE{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(n)*stride};};
  const auto gpu=[&](unsigned n){return D3D12_GPU_DESCRIPTOR_HANDLE{heap->GetGPUDescriptorHandleForHeapStart().ptr+UINT64(n)*stride};};
  int reads[7][5]={{0,-1,-1,-1,-1},{2,-1,-1,-1,-1},{3,-1,-1,-1,-1},{0,1,4,-1,-1},{5,6,4,-1,-1},{7,-1,-1,-1,-1},{5,6,1,4,8}};
  if(capture)reads[6][2]=10;
#ifdef MHW_DOF_NATIVE_ROUTE_FIXTURE
  if(mode==1)reads[6][2]=1;
#endif
  constexpr int writes[6][2]={{2,-1},{3,-1},{4,-1},{5,6},{7,8},{6,-1}};
  for(unsigned pass=0;pass<7;++pass){for(unsigned reg=0;reg<5;++reg){const auto index=reads[pass][reg];
    D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Texture2D.MipLevels=1;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Format=index<0?DXGI_FORMAT_R32_FLOAT:dof_formats::view(resources[index]->GetDesc().Format);
    device->CreateShaderResourceView(index<0?nullptr:resources[index].Get(),&d,cpu(pass*7+reg));}
   for(unsigned reg=0;reg<2;++reg){const auto index=pass<6?writes[pass][reg]:-1;D3D12_UNORDERED_ACCESS_VIEW_DESC d{};d.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;d.Format=index<0?DXGI_FORMAT_R32_FLOAT:dof_formats::view(resources[index]->GetDesc().Format);
    device->CreateUnorderedAccessView(index<0?nullptr:resources[index].Get(),nullptr,&d,cpu(pass*7+5+reg));}
  }
  std::array<float,448> cb{};cb[104]=2;cb[105]=1;cb[106]=1;
  auto screen=cb.data()+320;screen[4]=screen[5]=float(side);screen[6]=screen[7]=1.F/side;screen[10]=screen[11]=std::bit_cast<float>(side);
  screen[14]=screen[15]=float(side);screen[16]=screen[17]=1.F/side;
  auto blur=cb.data()+384;blur[0]=-.05F;blur[1]=.1F;blur[2]=1.F/side;blur[3]=blur[4]=.001F;blur[5]=blur[6]=1;blur[7]=.8F;blur[8]=1;blur[9]=.05F;blur[10]=1;
  blur[11]=blur[12]=std::bit_cast<float>(1U);blur[13]=blur[14]=1;
  blur[15]=.15F;blur[16]=1.6F;blur[17]=std::bit_cast<float>(scenario==2?1U:0U);blur[18]=.6F;blur[20]=.2F;blur[21]=.3F;blur[22]=.4F;
  if(capture)cb=capture->cb;
  const auto upload=[&](std::span<const std::byte> bytes){auto b=buffer(device.Get(),bytes.size(),D3D12_HEAP_TYPE_UPLOAD);void *p{};D3D12_RANGE none{};hr(b->Map(0,&none,&p),"buffer map");std::memcpy(p,bytes.data(),bytes.size());b->Unmap(0,nullptr);return b;};
  auto constants=upload(std::as_bytes(std::span(cb)));
  auto pixel_cb=cb;
  if(reference){auto screen_ref=pixel_cb.data()+320;screen_ref[6]*=.5F;screen_ref[7]*=.5F;screen_ref[12]*=2;screen_ref[13]*=2;screen_ref[14]*=2;screen_ref[15]*=2;screen_ref[16]*=.5F;screen_ref[17]*=.5F;}
  auto pixel_constants=upload(std::as_bytes(std::span(pixel_cb)));
  const auto vertices=sf::dependency_vertices({w,h,w,h},false);auto vb=upload(std::as_bytes(std::span(vertices)));
#ifdef MHW_DOF_NATIVE_ROUTE_FIXTURE
  std::unique_ptr<dof_route_fixture::Route> routed;
  if(mode==1){require(capture&&depth&&native_formats&&sync_reduction,"native route fixture inputs");
   routed=std::make_unique<dof_route_fixture::Route>(device.Get(),list.Get(),std::span(resources).first(9),shader_directory,cb);}
#endif
  ID3D12DescriptorHeap *heaps[]{heap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());
  for(unsigned i=0;i<3;++i)list->SetComputeRootConstantBufferView(i,constants->GetGPUVirtualAddress()+(i==0?0:i==1?1280:1536));
  const unsigned groups[6][3]={{(w+319)/320,h,1},{w/20,(h+319)/320,1},{(w/20+7)/8,(h/20+7)/8,1},{(w/2+15)/16,(h/2+15)/16,1},{(w/2+15)/16,(h/2+15)/16,1},{(w/2+15)/16,(h/2+15)/16,1}};
#ifdef MHW_DOF_NATIVE_ROUTE_FIXTURE
  if(routed){for(unsigned p:{0U,3U}){list->SetPipelineState(p?compute[p].Get():original_horizontal.Get());list->SetComputeRootDescriptorTable(3,gpu(p*7));list->SetComputeRootDescriptorTable(4,gpu(p*7+5));routed->warm(p);}routed->prepare();}
#endif
  std::vector<Pixels> snapshots; snapshots.reserve(8);
  for(unsigned pass=0;pass<6;++pass){
   for(auto index:reads[pass])if(index>=0)state(index,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   for(auto index:writes[pass])if(index>=0)state(index,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   list->SetPipelineState(reconstructed&&mode!=2&&pass==3?adapted_compute.Get():compute[pass].Get());list->SetComputeRootDescriptorTable(3,gpu(pass*7));list->SetComputeRootDescriptorTable(4,gpu(pass*7+5));
#ifdef MHW_DOF_NATIVE_ROUTE_FIXTURE
   if(routed){list->SetPipelineState(pass?compute[pass].Get():original_horizontal.Get());routed->dispatch(pass);}
   else
#endif
   if(!(fault==2&&pass==5))list->Dispatch(groups[pass][0],groups[pass][1],groups[pass][2]);
   // Keep each logical writer's image, including Image BEFORE its reuse.
   const unsigned index=pass==3?5:pass==4?8:static_cast<unsigned>(writes[pass][0]);
   state(index,D3D12_RESOURCE_STATE_COPY_SOURCE);snapshots.emplace_back(device.Get(),resources[index]->GetDesc(),D3D12_HEAP_TYPE_READBACK);snapshots.back().copy(list.Get(),resources[index].Get(),false);
   if(pass==3){state(6,D3D12_RESOURCE_STATE_COPY_SOURCE);snapshots.emplace_back(device.Get(),resources[6]->GetDesc(),D3D12_HEAP_TYPE_READBACK);snapshots.back().copy(list.Get(),resources[6].Get(),false);}
  }
  for(auto index:reads[6])state(index,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);state(9,D3D12_RESOURCE_STATE_RENDER_TARGET);
  device->CreateRenderTargetView(resources[9].Get(),nullptr,rtv->GetCPUDescriptorHandleForHeapStart());auto handle=rtv->GetCPUDescriptorHandleForHeapStart();
  list->SetGraphicsRootSignature(root.Get());for(unsigned i=0;i<3;++i)list->SetGraphicsRootConstantBufferView(i,pixel_constants->GetGPUVirtualAddress()+(i==0?0:i==1?1280:1536));
  list->SetGraphicsRootDescriptorTable(3,gpu(42));list->SetGraphicsRootDescriptorTable(4,gpu(47));list->SetPipelineState(reconstructed?adapted_composite.Get():composite.Get());list->OMSetRenderTargets(1,&handle,FALSE,nullptr);
  const D3D12_VIEWPORT view{0,0,float(w),float(h),0,1};const D3D12_RECT scissor{0,0,LONG(w),LONG(h)};const D3D12_VERTEX_BUFFER_VIEW vertex{vb->GetGPUVirtualAddress(),sizeof(vertices),28};
  list->RSSetViewports(1,&view);list->RSSetScissorRects(1,&scissor);list->IASetVertexBuffers(0,1,&vertex);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);list->DrawInstanced(4,1,0,0);
  state(9,D3D12_RESOURCE_STATE_COPY_SOURCE);Pixels final(device.Get(),resources[9]->GetDesc(),D3D12_HEAP_TYPE_READBACK);final.copy(list.Get(),resources[9].Get(),false);
  std::vector<Pixels> depth_readback;
  if(depth){state(0,D3D12_RESOURCE_STATE_COPY_SOURCE);depth_readback.emplace_back(device.Get(),resources[0]->GetDesc(),D3D12_HEAP_TYPE_READBACK);depth_readback.back().copy(list.Get(),resources[0].Get(),false);}
#ifdef MHW_DOF_NATIVE_ROUTE_FIXTURE
  if(routed)routed->choose();
#endif
  hr(list->Close(),"close");ID3D12CommandList *lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);
#ifdef MHW_DOF_NATIVE_ROUTE_FIXTURE
  if(routed)routed->submitted(queue.Get(),sequence+1);
#endif
  wait();messages();
#ifdef MHW_DOF_NATIVE_ROUTE_FIXTURE
  if(routed){routed->retire(allocator.Get(),fence.Get(),sequence);messages();std::cout<<"PASS production six-dispatch route, 7 recipe negatives, exact binding restoration and fence/Reset retirement\n";}
#endif
  Result r;require(snapshots.size()==7,"writer snapshots");for(unsigned i=0;i<7;++i){const auto raw=snapshots[i].read();r.stages[i]=native_formats?dof_formats::decode(raw,snapshots[i].footprint.Footprint.Format):raw;}r.output=final.read();
  if(depth){r.depth=depth_readback.back().read();require(r.depth.size()==depth->pixels.size(),"depth readback extent");
   for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)require(std::bit_cast<unsigned>(r.depth[static_cast<std::size_t>(y)*w+x])==std::bit_cast<unsigned>(depth->at(x,y,*capture,mode==4)),"depth input preserved bit-exactly");
   retained.emplace_back(depth_readback.back().data);}
  for(unsigned i=0;i<resource_count;++i)retained.emplace_back(resources[i]);
  for(auto &pixels:uploads)retained.emplace_back(pixels.data);
  for(auto &pixels:snapshots)retained.emplace_back(pixels.data);
  retained.emplace_back(final.data);retained.emplace_back(heap);retained.emplace_back(rtv);
  retained.emplace_back(pixel_constants);retained.emplace_back(constants);retained.emplace_back(vb);retained.emplace_back(list);retained.emplace_back(allocator);
  return r;
 }
};
double delta(const std::vector<float> &a,const std::vector<float> &b){require(a.size()==b.size(),"pixel sizes");double d=0;for(size_t i=0;i<a.size();++i){require(std::isfinite(a[i])&&std::isfinite(b[i]),"finite output");d=(std::max)(d,std::abs(double(a[i])-b[i]));}return d;}
// Independent CPU reduction of 20 depth pixels into TileMaxH, then 20 rows.
void reduction_oracle(const Result &r,unsigned scenario){
 for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side/20;++x){const float depth=scenario==0?.5F:(x+y/20)%3==0?.15F:(x+y/20)%3==1?.5F:.85F;
  const float linear=1.F/(2.F-depth),radius=std::abs(-.05F+.1F*depth);const auto at=(size_t(y)*(side/20)+x)*4;
  require(std::abs(r.stages[0][at]-linear)<1e-6F&&std::abs(r.stages[0][at+1]-radius)<1e-6F&&std::abs(r.stages[0][at+2]-radius)<1e-6F,"horizontal CPU oracle");
 }
 for(unsigned y=0;y<side/20;++y)for(unsigned x=0;x<side/20;++x){const auto src=(size_t(y*20)*(side/20)+x)*4,dst=(size_t(y)*(side/20)+x)*4;
  for(unsigned k=0;k<3;++k)require(std::abs(r.stages[0][src+k]-r.stages[1][dst+k])<1e-6F,"vertical CPU oracle");}
 if(scenario==0){
  // Zero CoC must compose the unchanged external HDR scene, pixel for pixel.
  for(unsigned y=0;y<side;++y)for(unsigned x=0;x<side;++x){
   const std::array<float,3> rgb{.1F+.7F*float(x%23)/22.F,.1F+.7F*float(y%29)/28.F,.2F+.5F*float((x+y)%31)/30.F};
   for(unsigned k=0;k<3;++k)require(std::abs(r.output[(size_t(y)*side+x)*4+k]-rgb[k])<2e-5F,"zero-blur composition CPU oracle");
  }
  // The native preparation shader averages four integer scene loads.
  for(unsigned y=0;y<side/2;++y)for(unsigned x=0;x<side/2;++x){std::array<float,3> rgb{};
   for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){const unsigned xx=2*x+dx,yy=2*y+dy;
    rgb[0]+=(.1F+.7F*float(xx%23)/22.F)*.25F;rgb[1]+=(.1F+.7F*float(yy%29)/28.F)*.25F;rgb[2]+=(.2F+.5F*float((xx+yy)%31)/30.F)*.25F;}
   for(unsigned k=0;k<3;++k)require(std::abs(r.stages[4][(size_t(y)*(side/2)+x)*4+k]-rgb[k])<2e-6F,"2x2 preparation CPU oracle");
  }
 }
}
#include "dof_captured_cases.inl"
#include "dof_native_cases.inl"
#include "dof_live_depth_cases.inl"
}
int wmain(int argc,wchar_t **argv){try{
 apply_perf_env();
 require(argc>=4&&argc<=9,"usage: warp|hardware private-shader-directory NEW-output-directory [captured-CB-pack [native-formats [captured-depth.r32f [sync-reduction|native-reduction [no-gpu-validation]]]]]");require(std::wstring_view(argv[1])==L"warp"||std::wstring_view(argv[1])==L"hardware","adapter option");
 const std::filesystem::path out=argv[3];require(!std::filesystem::exists(out),"preserve output");std::filesystem::create_directories(out);
 if(argc>=5){const bool native=argc>=6;if(native)require(std::wstring_view(argv[5])==L"native-formats","format option");Captured capture(argv[4]);
  // Reject malformed pixel input before creating a device or recording work.
  if(argc>=7){CapturedDepth depth(argv[6],capture);const bool sync=argc>=8&&std::wstring_view(argv[7])==L"sync-reduction",validation=argc!=9;
   if(argc>=8)require(sync||std::wstring_view(argv[7])==L"native-reduction","reduction option");
   if(!validation)require(std::wstring_view(argv[8])==L"no-gpu-validation","validation option");
   if(sync){const auto original=read(std::filesystem::path(argv[2])/(std::string(hashes[0])+".cs.dxbc"));auto corrupt=original;corrupt.back()^=std::byte{1};
    rejects([&]{dof_experiment::synchronized_reduction(corrupt);});rejects([&]{dof_experiment::synchronized_reduction(std::span(original).first(original.size()-4));});
    const auto patched=dof_experiment::synchronized_reduction(original);save(out/L"horizontal-synchronized.dxbc",patched);disassemble(out/L"horizontal-synchronized.asm",patched);}
   Pipeline pipeline(std::wstring_view(argv[1])==L"warp",argv[2],&capture,true,sync,validation);live_depth_cases(pipeline,capture,depth,out);return 0;}
  Pipeline pipeline(std::wstring_view(argv[1])==L"warp",argv[2],&capture,native);if(native)native_format_cases(pipeline,capture,out);else captured_cases(pipeline,capture,out,argv[2]);return 0;}
 Pipeline p(std::wstring_view(argv[1])==L"warp",argv[2]);
 std::ofstream report(out/L"result.json");report<<"{\"schema\":1,\"synthetic_constants\":true,\"native_format_equivalence\":false,\"live_contract_verified\":false,\"gpu_validation\":true,\"vendor\":"<<p.vendor<<",\"cases\":[";
 for(unsigned scenario=0;scenario<3;++scenario){const auto r=p.run(scenario,0);reduction_oracle(r,scenario);delta(r.output,r.output);
  const auto poisoned=p.run(scenario,1);const double invalid=delta(r.output,poisoned.output);require(invalid>.01,"invalid-footprint sensitivity");
  const auto wrong=p.run(scenario,2);const double stale=delta(r.output,wrong.output);if(scenario)require(stale>.00001,"Image writer version sensitivity");
  for(unsigned i=0;i<r.stages.size();++i){if(i!=5)delta(r.stages[i],r.stages[i]);save(out/("case-"+std::to_string(scenario)+"-writer-"+std::to_string(i)+".bin"),std::as_bytes(std::span(r.stages[i])));}
  save(out/("case-"+std::to_string(scenario)+"-output.bin"),std::as_bytes(std::span(r.output)));
  if(scenario)report<<',';report<<"{\"scenario\":"<<scenario<<",\"cpu_reduction_oracle\":true,\"invalid_footprint_delta\":"<<invalid<<",\"stale_writer_delta\":"<<stale<<",\"output_sha256\":\""<<digest(std::as_bytes(std::span(r.output)))<<"\"}";
  std::cout<<"case "<<scenario<<" footprint="<<invalid<<" stale-writer="<<stale<<std::endl;
 }
 report<<"],\"accepted\":true}";require(bool(report),"report");std::cout<<"PASS six native compute stages and composition; synthetic inputs only\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
