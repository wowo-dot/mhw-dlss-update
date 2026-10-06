#include "silhouette_shader.hpp"
#include <windows.h>
#include <bcrypt.h>
#include "../../third_party/dxc_hash/DxilHash.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace mhw::scaled_temporal {
namespace {
using Word=std::uint32_t;
constexpr Word fourcc(char a,char b,char c,char d) noexcept {
 return Word(a)|(Word(b)<<8)|(Word(c)<<16)|(Word(d)<<24);
}
void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
Word word(std::span<const std::byte> b,std::size_t at){
 require(at<=b.size()&&b.size()-at>=4,"silhouette word bounds");
 Word result{};std::memcpy(&result,b.data()+at,4);return result;
}
struct Reference {unsigned operand,index;Word token;};
struct Variant {
 std::array<unsigned char,32> hash;
 unsigned bytes,words,temps_at,temps;
 std::array<Reference,8> reads;
};
// Only identities and verified operand locations are retained, never native
// shader code. These five programs use the same hidden-surface depth/mask test.
constexpr Variant variants[]{
 {{0xbe,0x17,0x4b,0x5f,0x4d,0x68,0xae,0x60,0x08,0x1a,0xa5,0xff,0xb1,0x8e,0x3a,0xb2,0x44,0xa4,0x81,0xae,0xda,0x17,0x6d,0xad,0xf4,0x4a,0x2f,0xdd,0xf2,0x80,0x07,0x2d},56348,10731,223,28,{{{493,494,0x10100a},{532,533,0x10102a},{541,542,0x10102a},{549,550,0x10102a},{637,638,0x101046},{741,742,0x101046},{780,782,0x8010102a},{1162,1163,0x101046}}}},
 {{0x66,0xb9,0x4b,0xe1,0x55,0x86,0x57,0xb6,0x23,0x0d,0xca,0x31,0xb2,0x1b,0xfd,0xa5,0x83,0x69,0x92,0x89,0xd3,0x3f,0x89,0x8f,0x71,0x1d,0xfc,0x39,0x80,0xc2,0x70,0x77},60812,11585,233,29,{{{668,669,0x10100a},{707,708,0x10102a},{716,717,0x10102a},{724,725,0x10102a},{812,813,0x101046},{916,917,0x101046},{963,965,0x8010102a},{2015,2016,0x101046}}}},
 {{0xed,0xb7,0xdf,0x78,0x7b,0x52,0x91,0x8e,0x4c,0x4f,0xba,0xff,0x35,0x1e,0xa9,0x7f,0xfa,0xe3,0xad,0x68,0x6b,0x64,0x52,0xb0,0x5d,0x1f,0x55,0x9d,0x58,0xb4,0xf6,0x20},66600,12550,272,30,{{{2128,2129,0x10100a},{2167,2168,0x10102a},{2176,2177,0x10102a},{2184,2185,0x10102a},{2272,2273,0x101046},{2376,2377,0x101046},{2423,2425,0x8010102a},{2965,2966,0x101406}}}},
 {{0x75,0x0c,0x2b,0x00,0x16,0x7f,0xbd,0x6e,0x79,0x1d,0xbf,0xac,0xe9,0xb2,0x95,0xd6,0xac,0x9c,0xbe,0x3e,0xb7,0x36,0x48,0x56,0x0f,0x9e,0x84,0x78,0x6e,0x96,0x4e,0xec},68632,12859,247,30,{{{2573,2574,0x10100a},{2612,2613,0x10102a},{2621,2622,0x10102a},{2629,2630,0x10102a},{2717,2718,0x101046},{2821,2822,0x101406},{2860,2862,0x8010102a},{3281,3282,0x101106}}}},
 {{0x5f,0x2e,0x23,0x63,0xc0,0x83,0x9c,0x93,0x98,0x2e,0x20,0xb9,0xff,0xb5,0x7d,0xf1,0xcf,0x03,0xc4,0xa4,0xec,0x89,0x8f,0xa8,0x55,0x83,0xe1,0x54,0x17,0x2e,0x2e,0xa1},56384,10789,223,30,{{{561,562,0x10100a},{600,601,0x10102a},{609,610,0x10102a},{617,618,0x10102a},{705,706,0x101046},{809,810,0x101406},{856,858,0x8010102a},{1208,1209,0x101046}}}}
};
const Variant *identify(std::span<const std::byte> input) noexcept {
 if(input.size()<52||input.size()>131072)return nullptr;
 std::array<unsigned char,32> hash{};
 if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
    reinterpret_cast<PUCHAR>(const_cast<std::byte *>(input.data())),static_cast<ULONG>(input.size()),
    hash.data(),static_cast<ULONG>(hash.size()))<0)return nullptr;
 for(const auto &v:variants)if(input.size()==v.bytes&&hash==v.hash)return &v;
 return nullptr;
}
std::vector<Word> adapt(std::span<const Word> p,const Variant &v,FilterGeometry g){
 require(p.size()==v.words&&p[0]==0x50&&p[1]==p.size(),"silhouette ps_5_0 program");
 require(p[v.temps_at]==0x02000068&&p[v.temps_at+1]==v.temps,"silhouette temporary declaration");
 std::vector<Word> edited(p.begin(),p.end());edited[v.temps_at+1]=v.temps+1;
 for(const auto &r:v.reads){
  require(r.operand>v.temps_at+1&&r.index<p.size()&&p[r.operand]==r.token&&p[r.index]==0,"silhouette position operand");
  // Input register -> new temporary. Swizzle and extended source modifier
  // (including the negated z read) survive unchanged.
  edited[r.operand]&=~0x00001000U;edited[r.index]=v.temps;
 }
 unsigned matched=0;
 for(std::size_t at=2;at<p.size();){
  const bool custom=(p[at]&0x7ff)==53;
  require(!custom||p.size()-at>=2,"silhouette custom data header");
  const auto n=custom?p[at+1]:(p[at]>>24)&127U;
  require(n>=1&&n<=p.size()-at&&(!custom||n>=2),"silhouette instruction bounds");
  for(const auto &r:v.reads)if(r.operand>at&&r.index<at+n){require(!custom,"silhouette operand in custom data");++matched;}
  at+=n;
 }
 require(matched==v.reads.size(),"silhouette all reads accounted");
 const Word sx=std::bit_cast<Word>(float(g.render_width)/g.output_width);
 const Word sy=std::bit_cast<Word>(float(g.render_height)/g.output_height);
 const std::array<Word,15> injected{
  // xyz only: native declaration does not make SV_POSITION.w available.
  0x05000036,0x00100072,v.temps,0x00101246,0,
  0x0a000038,0x00100032,v.temps,0x00100046,v.temps,0x00004002,sx,sy,0,0};
 edited.insert(edited.begin()+v.temps_at+2,injected.begin(),injected.end());
 edited[1]=static_cast<Word>(edited.size());return edited;
}
}
bool known_silhouette_shader(std::span<const std::byte> b) noexcept {return identify(b)!=nullptr;}
std::vector<std::byte> make_silhouette_shader(std::span<const std::byte> native,FilterGeometry g){
 require(valid_filter_geometry(g),"silhouette geometry");const auto *v=identify(native);
 require(v!=nullptr,"unrecognized native silhouette shader");
 require(word(native,0)==fourcc('D','X','B','C')&&word(native,20)==1&&word(native,24)==native.size(),"silhouette container");
 const auto count=word(native,28);require(count>0&&count<=64,"silhouette chunk count");
 std::vector<std::vector<Word>> chunks;unsigned programs=0;
 for(unsigned i=0;i<count;++i){
  const auto at=word(native,32+4*i),size=word(native,at+4),tag=word(native,at);
  require(at>=32+4*count&&size%4==0&&std::uint64_t(at)+8+size<=native.size(),"silhouette chunk bounds");
  // Retain all signatures/reflection. Old STAT data describes the unadapted
  // instruction count and is optional for execution.
  if(tag==fourcc('S','T','A','T'))continue;
  std::vector<Word> data(size/4);std::memcpy(data.data(),native.data()+at+8,size);
  if(tag==fourcc('S','H','E','X')||tag==fourcc('S','H','D','R')){data=adapt(data,*v,g);++programs;}
  data.insert(data.begin(),{tag,static_cast<Word>(data.size()*4)});chunks.push_back(std::move(data));
 }
 require(programs==1,"silhouette one program");
 std::vector<Word> out(8+chunks.size());out[0]=fourcc('D','X','B','C');out[5]=1;out[7]=static_cast<Word>(chunks.size());
 for(std::size_t i=0;i<chunks.size();++i){out[8+i]=static_cast<Word>(out.size()*4);out.insert(out.end(),chunks[i].begin(),chunks[i].end());}
 out[6]=static_cast<Word>(out.size()*4);
 auto *bytes=reinterpret_cast<BYTE *>(out.data());ComputeHashRetail(bytes+20,static_cast<UINT32>(out.size()*4-20),bytes+4);
 std::vector<std::byte> result(out.size()*4);std::memcpy(result.data(),out.data(),result.size());return result;
}
}
