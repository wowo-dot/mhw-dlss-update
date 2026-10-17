#include "memory.hpp"
#include <d3d12.h>
#include <bit>
#include <cmath>
#include <cstring>
namespace mhw::projection_binding {
// The pinned root builders write these Windows SDK layouts directly.
static_assert(sizeof(D3D12_ROOT_PARAMETER1)==32 && offsetof(D3D12_ROOT_PARAMETER1,Descriptor)==8 &&
  offsetof(D3D12_ROOT_PARAMETER1,ShaderVisibility)==24 && sizeof(D3D12_ROOT_PARAMETER)==32);
static_assert(D3D12_ROOT_PARAMETER_TYPE_CBV==2 && D3D12_SHADER_VISIBILITY_VERTEX==1 && D3D12_SHADER_VISIBILITY_PIXEL==5);
bool matrix_layout(const std::array<unsigned,5> &ids,unsigned words) noexcept {
  if(!words || words>max_constants/4)return false;
  for(unsigned i=0;i<ids.size();++i) {
    const auto offset=(ids[i]>>12)&0x3ff;
    if(ids[i]==UINT32_MAX || ((ids[i]>>8)&15)!=15 || offset+16>words)return false;
    for(unsigned j=0;j<i;++j) {const auto prior=(ids[j]>>12)&0x3ff;if(!(prior+16<=offset || offset+16<=prior))return false;}
  }
  return true;
}
bool matrix_ids_same(std::uintptr_t base,const Source &s) noexcept {
  if(!s.matrices.valid)return false;
  for(unsigned i=0;i<matrix_id_rvas.size();++i) {unsigned id{};
    if(!read(base+matrix_id_rvas[i],id) || id!=s.matrices.ids[i])return false;}
  return true;
}
bool capture_matrices(std::uintptr_t base,Source &s) noexcept {
  auto &m=s.matrices;
  for(unsigned i=0;i<m.ids.size();++i)if(!read(base+matrix_id_rvas[i],m.ids[i]))return false;
  if(s.bytes!=s.projection.words*4 || !matrix_layout({m.ids[0],m.ids[1],m.ids[2],s.projection.projection_id,s.projection.unjittered_id},s.projection.words))return false;
  for(unsigned i=0;i<m.ids.size();++i) {
    std::memcpy(m.bits[i].data(),s.data.data()+4*((m.ids[i]>>12)&0x3ff),64);
    for(const auto b:m.bits[i])if(!std::isfinite(std::bit_cast<float>(b)))return false;
  }
  m.valid=true;if(!matrix_ids_same(base,s)){m.valid=false;return false;}return true;
}
}
