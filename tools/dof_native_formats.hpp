#pragma once
#include <DirectXPackedVector.h>
#include <cstdlib>
// Offline fixture formats observed by v1.25.37. This does not infer native views.
namespace dof_formats {
using namespace DirectX;
using namespace DirectX::PackedVector;
inline constexpr std::array<DXGI_FORMAT,11> formats{DXGI_FORMAT_R32_FLOAT,
 DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT,
 DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R16G16B16A16_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT,
 DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R8_UNORM,DXGI_FORMAT_R32G32B32A32_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT};
inline std::uint32_t pack(const std::array<float,4> &v){
 XMFLOAT3PK p;XMStoreFloat3PK(&p,XMVectorSet(v[0],v[1],v[2],v[3]));return p.v;
}
inline std::array<float,4> unpack(std::uint32_t word){
 XMFLOAT3PK p(word);XMFLOAT4 f;XMStoreFloat4(&f,XMLoadFloat3PK(&p));return {f.x,f.y,f.z,1.F};
}
inline std::array<float,4> quantize(const std::array<float,4> &v){return unpack(pack(v));}
// A shader store can choose the adjacent representable value rather than the
// SDK CPU packer's nearest rounding. Accept only the bracketing encodings of
// the unquantized oracle value, not an arbitrary absolute error tolerance.
inline bool brackets(float actual,float expected,unsigned channel){
 if(!std::isfinite(actual)||!std::isfinite(expected)||actual<0||expected<0||channel>2)return false;
 std::array<float,4> v{expected,expected,expected,1};const auto shift=channel==0?0:channel==1?11:22;
 const unsigned mask=channel==2?1023:2047,word=pack(v),nearest=(word>>shift)&mask;
 const auto q=unpack(word)[channel];const auto lower=nearest-(q>expected?1U:0U),upper=nearest+(q<expected?1U:0U);
 v={actual,actual,actual,1};const auto encoded=(pack(v)>>shift)&mask;
 return unpack(pack(v))[channel]==actual&&encoded>=lower&&encoded<=upper;
}
// MHW_DOF_POOLED=1 reproduces the 1920x1080 DOF textures: the CoC backed by
// its TYPELESS family and engine extents carrying flag bit 31.
inline bool pooled(){char value[2]{};std::size_t n{};return !getenv_s(&n,value,"MHW_DOF_POOLED")&&n==2&&value[0]=='1';}
// Views of a TYPELESS resource use the typed member the game's views use.
inline DXGI_FORMAT view(DXGI_FORMAT f){return f==DXGI_FORMAT_R16G16B16A16_TYPELESS?DXGI_FORMAT_R16G16B16A16_FLOAT:f;}
inline std::vector<float> decode(const std::vector<float> &raw,DXGI_FORMAT format){
 const auto bytes=std::as_bytes(std::span(raw));
 if(format==DXGI_FORMAT_R11G11B10_FLOAT){
  std::vector<float> out(raw.size()*4);
  for(std::size_t i=0;i<raw.size();++i){std::uint32_t word{};std::memcpy(&word,bytes.data()+4*i,4);
   const auto v=unpack(word);std::copy(v.begin(),v.end(),out.begin()+4*i);}return out;
 }
 if(format==DXGI_FORMAT_R16G16B16A16_FLOAT||format==DXGI_FORMAT_R16G16B16A16_TYPELESS){
  std::vector<float> out(raw.size()*2);
  for(std::size_t i=0;i<out.size();++i){HALF h{};std::memcpy(&h,bytes.data()+2*i,2);out[i]=XMConvertHalfToFloat(h);}return out;
 }
 return raw;
}
}
