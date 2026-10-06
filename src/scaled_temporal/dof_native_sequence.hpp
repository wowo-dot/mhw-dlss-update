#pragma once
#include "output_dependencies.hpp"
#include "dof_compute_shader.hpp"
#include <cstring>

namespace mhw::scaled_temporal {
// Six native dispatches followed by the already-qualified composition draw.
// Native views/constants/states remain bound. No descriptor is reconstructed.
struct DofNativeSequence {
 std::array<OutputDependencies::Compute,6> passes{};
 std::array<OutputDependencies::Texture,9> textures{};
 std::array<unsigned,9> resources{};
 unsigned count{},key{},stage{UINT32_MAX},entered{},returned{},dispatched{};
 std::array<unsigned,2> cache{UINT32_MAX,UINT32_MAX};
 // depth, scene, TileH, TileV, Neighbor, CoC, Image, packed blur, alpha
 static constexpr int reads[6][3]={{0,-1,-1},{2,-1,-1},{3,-1,-1},{0,1,4},{5,6,4},{7,-1,-1}};
 static constexpr int writes[6][2]={{2,-1},{3,-1},{4,-1},{5,6},{7,8},{6,-1}};
 static constexpr unsigned sizes[9][2]={{2560,1440},{2560,1440},{128,1440},{128,72},{128,72},{1280,720},{1280,720},{1280,720},{1280,720}};
 static constexpr unsigned formats[9]={41,26,26,26,26,10,26,42,61};
 static constexpr unsigned engine_formats[9]={16,34,34,34,34,2,34,38,21};
 static constexpr unsigned groups[6][3]={{8,1440,1},{128,5,1},{16,9,1},{80,45,1},{80,45,1},{80,45,1}};
 static constexpr unsigned words[6][9]={{282626,4243475,45088,16826416,52383808},
  {1470466,1474579,16826400},{1912834,1916947},
  {274433,282642,17068066,35594290,3989571,18718803,45152,16826480,52383872},
  {274433,2035730,17063970,35594290,2043971,18825299,16826464,52383856},
  {1937410,1941523}};
 static constexpr unsigned counts[6]={5,3,2,9,8,2},techniques[6]={43866,1382,15479,42573,4322,41772};
 bool qualify(std::uint64_t scene_owner,std::span<const std::uint64_t,5> composite){
  if(count!=6||!scene_owner)return false;
  textures={};
  std::array<const OutputDependencies::Constant *,4> constants{};
  for(unsigned p=0;p<6;++p){const auto &d=passes[p];
   if(!d.packet||!d.shader.size||d.shader.size>d.shader.bytes.size()||d.constant_count>3||d.technique!=techniques[p]||d.owner_count!=counts[p]||
      d.bindings.range!=std::array<unsigned short,2>{static_cast<unsigned short>(counts[p]),0}||
      !std::equal(words[p],words[p]+counts[p],d.bindings.words.begin())||
      !dof_compute_identity(std::as_bytes(std::span(d.shader.bytes).first(d.shader.size)),p))return false;
   if(p&&d.index!=passes[p-1].index+1)return false;
   for(unsigned i=0;i<3;++i)if(d.prefix[6+i]!=groups[p][i])return false;
   for(unsigned i=0;i<d.owner_count;++i)if(d.modes[i]||!d.objects[i])return false;
   for(unsigned j=0;j<counts[p];++j){const unsigned word=d.bindings.words[j],kind=word&15,index=(word>>4)&255,reg=word>>24;
    if(index>=d.owner_count||d.ids[index]!=((word>>12)&4095))return false;
    if(kind!=2&&kind!=3)continue;
    if(reg>=(kind==2?3U:2U))return false;
    const auto role=kind==2?reads[p][reg]:writes[p][reg];if(role<0||!d.views[j][0]||!d.views[j][1])return false;
    const auto &t=d.textures[index];const auto &v=t.fields;
    if(!d.texture_used[index]||v[0]!=d.objects[index]||!v[3]||v[4]!=(sizes[role][0]|(sizes[role][1]<<16))||v[5]!=engine_formats[role])return false;
    if(textures[role].fields[0]&&textures[role]!=t)return false;textures[role]=t;
   }
   for(unsigned i=0;i<d.constant_count;++i){const auto &c=d.constants[i];const auto reg=c.word>>24;
    if(reg>=4||reg==2||c.size!=(reg==0?1072U:reg==1?112U:96U))return false;
    if(constants[reg]&&(c.size!=constants[reg]->size||c.bytes!=constants[reg]->bytes))return false;constants[reg]=&c;
   }
  }
  for(unsigned i=0;i<9;++i){if(!textures[i].fields[0])return false;
   for(unsigned j=0;j<i;++j)if(textures[i].fields[0]==textures[j].fields[0]||textures[i].fields[3]==textures[j].fields[3])return false;}
  constexpr unsigned slots[]{5,6,1,4,8};
  for(unsigned i=0;i<5;++i)if(textures[slots[i]].fields[0]!=composite[i])return false;
  if(textures[1].fields[0]!=scene_owner||!constants[0]||!constants[1]||!constants[3])return false;
  std::array<float,28> screen{};std::memcpy(screen.data(),constants[1]->bytes.data(),112);
  const auto raw=[&](unsigned i){unsigned value{};std::memcpy(&value,&screen[i],4);return value;};
  if(screen[4]!=2560||screen[5]!=1440||screen[6]!=1.F/2560||screen[7]!=1.F/1440||screen[8]||screen[9]||
     raw(10)!=1280||raw(11)!=720||screen[12]||screen[13]||screen[14]!=1280||screen[15]!=720||
     screen[16]!=1.F/1280||screen[17]!=1.F/720||screen[26])return false;
  std::array<unsigned,24> blur{};std::memcpy(blur.data(),constants[3]->bytes.data(),96);
  return blur[11]==1&&blur[12]==1&&blur[13]==0x3f800000&&blur[14]==0x3f800000;
 }
};
}
