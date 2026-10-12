// Offline GPU check of the adapted DOF preparation pass (pass 3) at every
// DLSS render scale. Private DXBC comes only from argv; nothing proprietary is
// part of this source or the executable.
//
// For each geometry (output W x H, render w x h) the pass runs three ways on
// one synthetic output-size scene:
//  - adapted: the private variant reading the output-size scene;
//  - proxy:   the native shader reading a CPU area downsample of that scene,
//             placed in the render region of an output-size allocation (how
//             the game itself feeds it at a reduced render scale);
//  - native:  the native shader reading the output-size scene unadapted
//             (negative control: it must not match).
// The prepared colour is checked against an independent double-precision CPU
// oracle; the CoC writer must be bit-identical between adapted and proxy runs.
#include "dof_fixture_support.hpp"
#include "../src/scaled_temporal/dof_compute_shader.hpp"
#include <bcrypt.h>
#include <cstdint>
#include <string>

namespace {
using namespace dof_fixture;
constexpr const char *prepare_hash="b7afd17a818940e9f2da3830ad2e208767930f4decd1572b4fb64b164c85a532";
std::string digest(std::span<const std::byte> bytes){
 std::array<unsigned char,32> hash{};
 require(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<std::byte *>(bytes.data())),
  static_cast<ULONG>(bytes.size()),hash.data(),32)>=0,"SHA256");
 constexpr char hex[]="0123456789abcdef";std::string result;
 for(auto x:hash){result+=hex[x>>4];result+=hex[x&15];}return result;
}
struct Geometry {unsigned W,H,w,h;const char *name;};
// Exactly representable high-frequency stimulus (multiples of 1/32).
std::array<float,4> scene(unsigned x,unsigned y){
 return {.125F+float((x*7+y*3)%13)/32.F,.125F+float((x*3+y*11)%17)/32.F,.125F+float((x+y*5)%19)/32.F,1.F};
}
// Render texel c covers output [c*S/s,(c+1)*S/s): exact integer overlaps in
// units of 1/s, as the adapter computes them.
std::array<double,3> area(const Geometry &g,unsigned cx,unsigned cy){
 std::array<double,3> sum{};
 const std::uint64_t ax=std::uint64_t(cx)*g.W,bx=ax+g.W,ay=std::uint64_t(cy)*g.H,by=ay+g.H;
 for(std::uint64_t ky=ay/g.h;ky*g.h<by;++ky){const auto oy=(std::min)((ky+1)*g.h,by)-(std::max)(ky*g.h,ay);
  for(std::uint64_t kx=ax/g.w;kx*g.w<bx;++kx){const auto ox=(std::min)((kx+1)*g.w,bx)-(std::max)(kx*g.w,ax);
   const auto v=scene(static_cast<unsigned>(kx),static_cast<unsigned>(ky));const double weight=double(ox)*double(oy)/(double(g.W)*g.H);
   for(unsigned k=0;k<3;++k)sum[k]+=weight*v[k];}}
 return sum;
}
struct Output {std::vector<float> coc,color;};
struct Fixture : Gpu {
 ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> native,adapted;std::vector<ComPtr<IUnknown>> retained;
 Fixture(bool warp,std::span<const std::byte> code,std::span<const std::byte> variant):Gpu(warp){
  std::array<D3D12_ROOT_PARAMETER,5> params{};
  for(unsigned i=0;i<3;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[i].Descriptor.ShaderRegister=i==2?3:i;}
  std::array<D3D12_DESCRIPTOR_RANGE,2> ranges{{{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,3,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,2,0,0,0}}};
  for(unsigned i=0;i<2;++i){params[3+i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[3+i].DescriptorTable={1,&ranges[i]};}
  D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_NEVER;
  D3D12_ROOT_SIGNATURE_DESC rd{5,params.data(),1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};ComPtr<ID3DBlob> b,e;
  hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&b,&e),"root serialize");
  hr(device->CreateRootSignature(0,b->GetBufferPointer(),b->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
  D3D12_COMPUTE_PIPELINE_STATE_DESC d{};d.pRootSignature=root.Get();d.CS={code.data(),code.size()};
  hr(device->CreateComputePipelineState(&d,IID_PPV_ARGS(&native)),"native PSO");
  d.CS={variant.data(),variant.size()};hr(device->CreateComputePipelineState(&d,IID_PPV_ARGS(&adapted)),"adapted PSO");
 }
 // mode 0 adapted, 1 proxy, 2 native on the output scene. view overrides the
 // CBScreen render size (the hostile-constants case).
 Output run(const Geometry &g,unsigned mode,std::array<unsigned,2> view={}){
  if(!view[0])view={g.w,g.h};
  hr(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(allocator.ReleaseAndGetAddressOf())),"allocator");
  hr(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(list.ReleaseAndGetAddressOf())),"list");
  const unsigned hw=g.W/2,hh=g.H/2;
  const std::array<D3D12_RESOURCE_DESC,5> descs{texture(g.W,g.H,DXGI_FORMAT_R32_FLOAT),texture(g.W,g.H,DXGI_FORMAT_R32G32B32A32_FLOAT),
   texture((g.W+19)/20,(g.H+19)/20,DXGI_FORMAT_R32G32B32A32_FLOAT),texture(hw,hh,DXGI_FORMAT_R32G32B32A32_FLOAT),texture(hw,hh,DXGI_FORMAT_R32G32B32A32_FLOAT)};
  std::array<ComPtr<ID3D12Resource>,5> textures;std::vector<Pixels> uploads;uploads.reserve(3);
  for(unsigned i=0;i<5;++i){auto desc=descs[i];if(i>=3)desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
   textures[i]=resource(device.Get(),desc,D3D12_HEAP_TYPE_DEFAULT,i<3?D3D12_RESOURCE_STATE_COPY_DEST:D3D12_RESOURCE_STATE_UNORDERED_ACCESS);}
  for(unsigned i=0;i<3;++i){uploads.emplace_back(device.Get(),descs[i],D3D12_HEAP_TYPE_UPLOAD);auto &u=uploads.back();
   std::vector<std::byte> bytes(static_cast<std::size_t>(u.row)*descs[i].Height);
   for(unsigned y=0;y<descs[i].Height;++y)for(unsigned x=0;x<descs[i].Width;++x){const auto at=static_cast<std::size_t>(u.row)*y;
    if(i==0){const float depth=.5F;std::memcpy(bytes.data()+at+4*x,&depth,4);}
    else if(i==2){const std::array<float,4> zero{};std::memcpy(bytes.data()+at+16*x,zero.data(),16);}
    else {std::array<float,4> v{1000.F,-1000.F,1000.F,1.F};  // poison outside the proxy's render region
     if(mode!=1)v=scene(x,y);
     else if(x<g.w&&y<g.h){const auto a=area(g,x,y);v={float(a[0]),float(a[1]),float(a[2]),1.F};}
     std::memcpy(bytes.data()+at+16*x,v.data(),16);}
   }
   u.write(bytes);u.copy(list.Get(),textures[i].Get(),true);barrier(list.Get(),textures[i].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  }
  D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,5,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};ComPtr<ID3D12DescriptorHeap> heap;
  hr(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"descriptors");
  const auto stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  const auto cpu=[&](unsigned n){return D3D12_CPU_DESCRIPTOR_HANDLE{heap->GetCPUDescriptorHandleForHeapStart().ptr+SIZE_T(n)*stride};};
  const auto gpu=[&](unsigned n){return D3D12_GPU_DESCRIPTOR_HANDLE{heap->GetGPUDescriptorHandleForHeapStart().ptr+UINT64(n)*stride};};
  for(unsigned i=0;i<3;++i){D3D12_SHADER_RESOURCE_VIEW_DESC v{};v.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;v.Texture2D.MipLevels=1;
   v.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;v.Format=descs[i].Format;device->CreateShaderResourceView(textures[i].Get(),&v,cpu(i));}
  for(unsigned i=0;i<2;++i){D3D12_UNORDERED_ACCESS_VIEW_DESC v{};v.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;v.Format=descs[3+i].Format;
   device->CreateUnorderedAccessView(textures[3+i].Get(),nullptr,&v,cpu(3+i));}
  // cb0 (CBViewProjection) at 0, cb1 (CBScreen) at 1280, cb3 (CBNewDOFFilter) at 1536.
  std::array<float,448> cb{};cb[104]=2;cb[105]=1;cb[106]=1;
  auto screen=cb.data()+320;screen[4]=float(g.W);screen[5]=float(g.H);screen[6]=1.F/g.W;screen[7]=1.F/g.H;
  screen[10]=std::bit_cast<float>(view[0]);screen[11]=std::bit_cast<float>(view[1]);
  screen[14]=float(view[0]);screen[15]=float(view[1]);screen[16]=1.F/view[0];screen[17]=1.F/view[1];
  // The game's flags (dof_native_sequence: blur[11..14]); with a zero bias and
  // unit slope every one of the four colour samples is included.
  auto blur=cb.data()+384;blur[0]=0;blur[1]=1;blur[9]=1;blur[11]=blur[12]=std::bit_cast<float>(1U);blur[13]=blur[14]=1;
  auto constants=buffer(device.Get(),sizeof(cb),D3D12_HEAP_TYPE_UPLOAD);{void *p{};D3D12_RANGE none{};hr(constants->Map(0,&none,&p),"cb map");std::memcpy(p,cb.data(),sizeof(cb));constants->Unmap(0,nullptr);}
  ID3D12DescriptorHeap *heaps[]{heap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());
  for(unsigned i=0;i<3;++i)list->SetComputeRootConstantBufferView(i,constants->GetGPUVirtualAddress()+(i==0?0:i==1?1280:1536));
  list->SetComputeRootDescriptorTable(3,gpu(0));list->SetComputeRootDescriptorTable(4,gpu(3));
  list->SetPipelineState(mode==0?adapted.Get():native.Get());list->Dispatch((hw+15)/16,(hh+15)/16,1);
  std::vector<Pixels> readback;readback.reserve(2);
  for(unsigned i=3;i<5;++i){barrier(list.Get(),textures[i].Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
   readback.emplace_back(device.Get(),descs[i],D3D12_HEAP_TYPE_READBACK);readback.back().copy(list.Get(),textures[i].Get(),false);}
  hr(list->Close(),"close");ID3D12CommandList *lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);wait();messages();
  Output out{readback[0].read(),readback[1].read()};
  for(auto &t:textures)retained.emplace_back(t);for(auto &u:uploads)retained.emplace_back(u.data);for(auto &r:readback)retained.emplace_back(r.data);
  retained.emplace_back(heap);retained.emplace_back(constants);retained.emplace_back(list);retained.emplace_back(allocator);
  return out;
 }
};
// The native preparation: r0 = min(2t, view-2) per axis, then the four texels
// r0 + {0,1}^2, each now the area value of its output footprint, averaged.
double oracle_error(const Geometry &g,const std::vector<float> &color){
 const unsigned hw=g.W/2,hh=g.H/2;double worst=0;
 for(unsigned ty=0;ty<hh;++ty)for(unsigned tx=0;tx<hw;++tx){
  const unsigned x0=(std::min)(2*tx,g.w-2),y0=(std::min)(2*ty,g.h-2);std::array<double,3> expected{};
  for(unsigned dy=0;dy<2;++dy)for(unsigned dx=0;dx<2;++dx){const auto a=area(g,x0+dx,y0+dy);for(unsigned k=0;k<3;++k)expected[k]+=a[k]/4;}
  const auto at=(static_cast<std::size_t>(ty)*hw+tx)*4;
  for(unsigned k=0;k<3;++k){require(std::isfinite(color[at+k]),"finite prepared colour");worst=(std::max)(worst,std::abs(double(color[at+k])-expected[k]));}
  require(color[at+3]==1.F,"native alpha");
 }
 return worst;
}
double difference(const std::vector<float> &a,const std::vector<float> &b){
 require(a.size()==b.size(),"sizes");double d=0;for(std::size_t i=0;i<a.size();++i)d=(std::max)(d,std::abs(double(a[i])-b[i]));return d;
}
}
int wmain(int argc,wchar_t **argv){try{
 require(argc==4,"usage: warp|hardware private-shader-directory NEW-output-directory");
 require(std::wstring_view(argv[1])==L"warp"||std::wstring_view(argv[1])==L"hardware","adapter option");
 const std::filesystem::path out=argv[3];require(!std::filesystem::exists(out),"preserve output");std::filesystem::create_directories(out);
 const auto code=read(std::filesystem::path(argv[2])/(std::string(prepare_hash)+".cs.dxbc"));require(digest(code)==prepare_hash,"preparation shader identity");
 const auto variant=mhw::scaled_temporal::make_dof_compute_shader(code,3);
 reflection(code,variant);save(out/L"prepare-area.dxbc",variant);disassemble(out/L"prepare-area.asm",variant);
 auto corrupt=code;corrupt.back()^=std::byte{1};rejects([&]{mhw::scaled_temporal::make_dof_compute_shader(corrupt,3);});
 Fixture f(std::wstring_view(argv[1])==L"warp",code,variant);
 const Geometry cases[]{{2560,1440,1280,720,"performance-1440p"},{2560,1440,1696,954,"quality-1440p"},{2560,1440,1472,828,"balanced-1440p"},
  {1920,1080,1280,720,"quality-1080p"},{3413,1440,1706,720,"performance-pillarbox-3413"}};
 std::ofstream report(out/L"result.json");report<<"{\"schema\":1,\"synthetic\":true,\"vendor\":"<<f.vendor<<",\"variant_sha256\":\""<<digest(variant)<<"\",\"cases\":[";
 bool first=true;
 for(const auto &g:cases){
  const auto adapted=f.run(g,0),proxy=f.run(g,1),native=f.run(g,2);
  const double error=oracle_error(g,adapted.color),proxy_error=oracle_error(g,proxy.color),agreement=difference(adapted.color,proxy.color);
  const double control=difference(native.color,adapted.color);
  require(difference(adapted.coc,proxy.coc)==0,"CoC writer unchanged");
  // 2:1 with dyadic stimulus is exact; other scales carry float weights.
  const bool half=g.W==2*g.w&&g.H==2*g.h;
  require(half?error==0&&agreement==0:error<2e-6&&agreement<2e-6,"area oracle");
  require(control>.01,"unadapted negative control");
  std::cout<<g.name<<" "<<g.W<<'x'<<g.H<<" <- "<<g.w<<'x'<<g.h<<": oracle "<<error<<", proxy "<<proxy_error<<", adapted vs proxy "<<agreement<<", unadapted "<<control<<std::endl;
  if(!first)report<<',';first=false;
  report<<"{\"name\":\""<<g.name<<"\",\"output\":["<<g.W<<','<<g.H<<"],\"render\":["<<g.w<<','<<g.h<<"],\"oracle_error\":"<<error
   <<",\"proxy_oracle_error\":"<<proxy_error<<",\"adapted_proxy_delta\":"<<agreement<<",\"unadapted_delta\":"<<control<<",\"coc_equal\":true}";
 }
 // Hostile CBScreen (a 1x1 render view): the loops stay bounded (four texels
 // per axis) and the dispatch completes without a device removal.
 (void)f.run(cases[0],0,{1,1});
 report<<"],\"bounded_hostile_view\":true,\"accepted\":true}";require(bool(report),"report");
 std::cout<<"PASS adapted DOF preparation at 5 render scales against the CPU area oracle and the native proxy; bounded on hostile constants\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
