#pragma once
#include "output_dependencies.hpp"
#include "dof_compute_shader.hpp"
#include <bit>
#include <cstring>

namespace mhw::scaled_temporal {
// Texture sizes and dispatch counts of the native DOF at a given screen size.
// The shaders use fixed 20-pixel tiles (pass 0 divides its thread index by 20)
// and work at half the screen size; group sizes are 320x1, 1x320, 8x8 and
// 16x16 (dof-compute-bundle-result.md). At 2560x1440 this gives the captured
// 128x1440, 128x72 and 1280x720 textures and groups {8,1440}, {128,5}, {16,9}
// and {80,45}. A partial tile is assumed to round up (not yet observed: every
// common 16:9 size divides by 20). With DLSS Performance the half size is also
// the render size.
struct DofLayout {
 std::array<std::array<unsigned,2>,9> sizes{};
 std::array<std::array<unsigned,3>,6> groups{};
};
constexpr DofLayout dof_layout(unsigned width,unsigned height){
 const auto up=[](unsigned v,unsigned d){return (v+d-1)/d;};
 const unsigned tw=up(width,20),th=up(height,20),hw=width/2,hh=height/2;
 DofLayout l{};
 l.sizes={{{width,height},{width,height},{tw,height},{tw,th},{tw,th},{hw,hh},{hw,hh},{hw,hh},{hw,hh}}};
 l.groups={{{up(tw,16),height,1},{tw,up(height,320),1},{up(tw,8),up(th,8),1},{up(hw,16),up(hh,16),1},{up(hw,16),up(hh,16),1},{up(hw,16),up(hh,16),1}}};
 return l;
}
// The game may back a DOF texture with the TYPELESS member of the format's
// family (seen at 1920x1080: the CoC texture is R16G16B16A16_TYPELESS where
// 1440p had R16G16B16A16_FLOAT). The route only uses the game's own typed
// views, so either resource format is accepted. Returns 0 when the format has
// no typeless family (R11G11B10_FLOAT).
constexpr unsigned dof_typeless(unsigned typed){
 switch(typed){
  case 2:return 1;   // R32G32B32A32_FLOAT
  case 10:return 9;  // R16G16B16A16_FLOAT
  case 16:return 15; // R32G32_FLOAT
  case 34:return 33; // R16G16_FLOAT
  case 41:case 42:return 39; // R32_FLOAT, R32_UINT
  case 54:case 56:return 53; // R16_FLOAT, R16_UNORM
  case 61:return 60; // R8_UNORM
  default:return 0;
 }
}
constexpr bool dof_format_matches(unsigned actual,unsigned typed){return actual==typed||(dof_typeless(typed)&&actual==dof_typeless(typed));}
// The game computes the reciprocals on the CPU; allow one unit in the last place.
inline bool dof_near(float a,float b){std::int32_t x{},y{};std::memcpy(&x,&a,4);std::memcpy(&y,&b,4);return x==y||x==y+1||x+1==y;}

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
 static constexpr unsigned formats[9]={41,26,26,26,26,10,26,42,61};
 static constexpr unsigned engine_formats[9]={16,34,34,34,34,2,34,38,21};
 static constexpr unsigned words[6][9]={{282626,4243475,45088,16826416,52383808},
  {1470466,1474579,16826400},{1912834,1916947},
  {274433,282642,17068066,35594290,3989571,18718803,45152,16826480,52383872},
  {274433,2035730,17063970,35594290,2043971,18825299,16826464,52383856},
  {1937410,1941523}};
 static constexpr unsigned counts[6]={5,3,2,9,8,2},techniques[6]={43866,1382,15479,42573,4322,41772};
 DofLayout layout{};
 // Diagnostics of the last qualify: the first structural check that failed
 // (failed_at, 0 = none; see bad() below), every size/dispatch/constant
 // mismatch {check, index, observed, expected} (check 1 texture size,
 // 2 groups, 3 screen constant; the plan adds 4 resource format and
 // 5 resource size), and the roles whose engine extent carries flag bits.
 unsigned failed_at{},miss_count{},flagged{};
 std::array<std::array<unsigned,4>,8> misses{};
 void note(unsigned check,unsigned index,unsigned observed,unsigned expected){
  if(miss_count<misses.size())misses[miss_count]={check,index,observed,expected};++miss_count;}
 bool qualify(unsigned width,unsigned height,std::uint64_t scene_owner,std::span<const std::uint64_t,5> composite){
  failed_at=miss_count=flagged=0;misses={};
  // Structural codes: 1 shape, 10+p pass recipe, 20+p dispatch order, 30+p
  // owners, 40+p binding ids, 50+p binding register/role/views, 60+p texture
  // owner/format, 70+p role conflict, 80+p constants, 90 missing texture,
  // 91 shared texture, 92 composition inputs, 93 scene/constants, 94 screen
  // zero words, 95 blur constants.
  const auto bad=[&](unsigned code){failed_at=code;return false;};
  // An odd scene (3413x1440 pillarboxed) takes the truncated half; the size
  // and constant checks below record the game's actual sizes if it differs.
  if(count!=6||!scene_owner||!width||!height)return bad(1);
  layout=dof_layout(width,height);
  textures={};
  std::array<const OutputDependencies::Constant *,4> constants{};
  for(unsigned p=0;p<6;++p){const auto &d=passes[p];
   if(!d.packet||!d.shader.size||d.shader.size>d.shader.bytes.size()||d.constant_count>3||d.technique!=techniques[p]||d.owner_count!=counts[p]||
      d.bindings.range!=std::array<unsigned short,2>{static_cast<unsigned short>(counts[p]),0}||
      !std::equal(words[p],words[p]+counts[p],d.bindings.words.begin())||
      !dof_compute_identity(std::as_bytes(std::span(d.shader.bytes).first(d.shader.size)),p))return bad(10+p);
   if(p&&d.index!=passes[p-1].index+1)return bad(20+p);
   for(unsigned i=0;i<3;++i)if(d.prefix[6+i]!=layout.groups[p][i])note(2,p*3+i,d.prefix[6+i],layout.groups[p][i]);
   for(unsigned i=0;i<d.owner_count;++i)if(d.modes[i]||!d.objects[i])return bad(30+p);
   for(unsigned j=0;j<counts[p];++j){const unsigned word=d.bindings.words[j],kind=word&15,index=(word>>4)&255,reg=word>>24;
    if(index>=d.owner_count||d.ids[index]!=((word>>12)&4095))return bad(40+p);
    if(kind!=2&&kind!=3)continue;
    if(reg>=(kind==2?3U:2U))return bad(50+p);
    const auto role=kind==2?reads[p][reg]:writes[p][reg];if(role<0||!d.views[j][0]||!d.views[j][1])return bad(50+p);
    const auto &t=d.textures[index];const auto &v=t.fields;
    if(!d.texture_used[index]||v[0]!=d.objects[index]||!v[3]||v[5]!=engine_formats[role])return bad(60+p);
    // Owner+0x88 packs a 16-bit width, a 14-bit height and two flag bits
    // (bit 31 accompanies a secondary backing, seen at 1920x1080).
    const unsigned extent=static_cast<unsigned>(v[4]),packed=layout.sizes[role][0]|(layout.sizes[role][1]<<16);
    if(extent>>30)flagged|=1U<<role;
    if((extent&0x3fffffffU)!=packed)note(1,static_cast<unsigned>(role),extent,packed);
    if(textures[role].fields[0]&&textures[role]!=t)return bad(70+p);textures[role]=t;
   }
   for(unsigned i=0;i<d.constant_count;++i){const auto &c=d.constants[i];const auto reg=c.word>>24;
    if(reg>=4||reg==2||c.size!=(reg==0?1072U:reg==1?112U:96U))return bad(80+p);
    if(constants[reg]&&(c.size!=constants[reg]->size||c.bytes!=constants[reg]->bytes))return bad(80+p);constants[reg]=&c;
   }
  }
  for(unsigned i=0;i<9;++i){if(!textures[i].fields[0])return bad(90);
   for(unsigned j=0;j<i;++j)if(textures[i].fields[0]==textures[j].fields[0]||textures[i].fields[3]==textures[j].fields[3])return bad(91);}
  constexpr unsigned slots[]{5,6,1,4,8};
  for(unsigned i=0;i<5;++i)if(textures[slots[i]].fields[0]!=composite[i])return bad(92);
  if(textures[1].fields[0]!=scene_owner||!constants[0]||!constants[1]||!constants[3])return bad(93);
  std::array<float,28> screen{};std::memcpy(screen.data(),constants[1]->bytes.data(),112);
  const auto raw=[&](unsigned i){unsigned value{};std::memcpy(&value,&screen[i],4);return value;};
  // CBScreen: fScreenSize, its inverse, iViewSize (integers) and the half size
  // with its inverse; the remaining checked words are zero.
  const float w=static_cast<float>(width),h=static_cast<float>(height),hw=static_cast<float>(width/2),hh=static_cast<float>(height/2);
  const std::array<float,8> sizes_f{w,h,1.F/w,1.F/h,hw,hh,1.F/hw,1.F/hh};constexpr unsigned at[]{4,5,6,7,14,15,16,17};
  for(unsigned i=0;i<8;++i)if(i<2||i==4||i==5?screen[at[i]]!=sizes_f[i]:!dof_near(screen[at[i]],sizes_f[i]))note(3,at[i],raw(at[i]),std::bit_cast<unsigned>(sizes_f[i]));
  if(raw(10)!=width/2)note(3,10,raw(10),width/2);
  if(raw(11)!=height/2)note(3,11,raw(11),height/2);
  if(screen[8]||screen[9]||screen[12]||screen[13]||screen[26])return bad(94);
  if(miss_count)return false;
  std::array<unsigned,24> blur{};std::memcpy(blur.data(),constants[3]->bytes.data(),96);
  return blur[11]==1&&blur[12]==1&&blur[13]==0x3f800000&&blur[14]==0x3f800000?true:bad(95);
 }
};
}
