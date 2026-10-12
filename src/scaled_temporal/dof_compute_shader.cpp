#include "dof_compute_shader.hpp"
#include <windows.h>
#include <bcrypt.h>
#include "../../third_party/dxc_hash/DxilHash.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
namespace mhw::scaled_temporal {
namespace {
using Word=std::uint32_t;
void require(bool v,const char *why){if(!v)throw std::runtime_error(why);}
std::string digest(std::span<const std::byte> bytes){
 std::array<unsigned char,32> hash{};
 require(bytes.size()<=65536&&BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
  reinterpret_cast<PUCHAR>(const_cast<std::byte *>(bytes.data())),static_cast<ULONG>(bytes.size()),hash.data(),32)>=0,"compute SHA256");
 constexpr char hex[]="0123456789abcdef";std::string out;for(auto x:hash){out+=hex[x>>4];out+=hex[x&15];}return out;
}
// DXBC SM5 operand tokens for the emitted code: 4-component temporaries
// (r<n>, then the register number), CBScreen (cb1[i], then slot and index),
// immediates and the null operand. Components x=0, y=1, z=2, w=3.
constexpr Word X=0,Y=1,Z=2,W=3;
constexpr Word mask(Word m){return 0x00100002|(m<<4);}  // destination r<n>.mask (x1 y2 z4 w8)
constexpr Word swizzle(Word a,Word b,Word c,Word d){return 0x00100006|((a|b<<2|c<<4|d<<6)<<4);}
constexpr Word one(Word c){return 0x0010000a|(c<<4);}   // r<n>.c
constexpr Word negated(Word operand){return operand|0x80000000U;}  // then 0x41 (negate), then the register
constexpr Word cb_swizzle(Word a,Word b,Word c,Word d){return 0x00208006|((a|b<<2|c<<4|d<<6)<<4);}
constexpr Word cb_one(Word c){return 0x0020800a|(c<<4);}
constexpr Word null_operand=0x0000d000,imm1=0x00004001,imm4=0x00004002,negate=0x41;
// Opcodes (D3D10_SB/D3D11_SB_OPCODE_TYPE); breakc tests non-zero (bit 18).
constexpr Word BREAKC_NZ=0x00040003,DIV=0x0e,ENDLOOP=0x16,FTOU=0x1c,IADD=0x1e,IMUL=0x26,LOOP=0x30,MAD=0x32,MOV=0x36,
 MUL=0x38,UDIV=0x4e,UGE=0x50,UMAX=0x53,UMIN=0x54,UTOF=0x56;

// One native colour load, replaced. The native shader reads the render-size
// scene at the integer render texel c (iViewSize lattice); the reconstructed
// scene is output-size. Render texel c covers output [c*S/s,(c+1)*S/s) per
// axis (S = fScreenSize, s = iViewSize, both from the bound CBScreen), so the
// equivalent value is the area-weighted average of the output texels in that
// interval. Bounds and overlaps are exact integers in units of 1/s: texel k
// overlaps min((k+1)s,(c+1)S)-max(ks,cS), divided by S. At 2:1 this is the
// 2x2 box; Quality and Balanced touch two or three texels per axis, and the
// truncated half of an odd scene (3413 -> 1706) up to four. The loops are
// bounded to four texels per axis. r6..r12 are new temporaries; the native
// destination is written last, so an ld that aliases coord and dest is safe.
// Native alpha is untouched (the native ld writes .xyz only).
void emit_area_load(std::vector<Word> &out,Word dest,Word coord){
 const auto op=[&](Word code,std::initializer_list<Word> v){out.push_back(code|static_cast<Word>(1+v.size())<<24);out.insert(out.end(),v);};
 op(FTOU,{mask(4|8),6,cb_swizzle(X,X,X,Y),1,1});                     // r6.zw = S (fScreenSize)
 op(IMUL,{null_operand,mask(1|2),9,swizzle(X,Y,X,X),coord,swizzle(Z,W,Z,Z),6}); // r9.xy = A = c*S
 op(IADD,{mask(4|8),9,swizzle(X,X,X,Y),9,swizzle(Z,Z,Z,W),6});       // r9.zw = B = A+S
 op(UDIV,{mask(1|2),10,null_operand,swizzle(X,Y,X,X),9,cb_swizzle(Z,W,Z,Z),1,2}); // r10.xy = first texel A/s
 op(IADD,{mask(1|2),11,swizzle(Z,W,Z,Z),9,cb_swizzle(Z,W,Z,Z),1,2});
 op(IADD,{mask(1|2),11,swizzle(X,Y,X,X),11,imm4,0xffffffffU,0xffffffffU,0,0});
 op(UDIV,{mask(4|8),10,null_operand,swizzle(X,X,X,Y),11,cb_swizzle(Z,Z,Z,W),1,2}); // r10.zw = end ceil(B/s)
 op(IADD,{mask(1|2),11,swizzle(X,Y,X,X),10,imm4,4,4,0,0});
 op(UMIN,{mask(4|8),10,swizzle(Z,Z,Z,W),10,swizzle(X,X,X,Y),11});    // at most four texels per axis
 op(MOV,{mask(1|2|4),7,imm4,0,0,0,0});                                 // r7.xyz: the weighted sum
 op(MOV,{mask(4|8),8,imm4,0,0,0,0});                                   // r8 = (kx, ky, 0, mip 0)
 op(MOV,{mask(2),8,one(Y),10});
 op(LOOP,{});
  op(UGE,{mask(1),11,one(Y),8,one(W),10});
  op(BREAKC_NZ,{one(X),11});
  op(IMUL,{null_operand,mask(1),11,one(Y),8,cb_one(W),1,2});          // ky*s
  op(IADD,{mask(2),11,one(X),11,cb_one(W),1,2});                       // (ky+1)*s
  op(UMAX,{mask(1),11,one(X),11,one(Y),9});
  op(UMIN,{mask(2),11,one(Y),11,one(W),9});
  op(IADD,{mask(1),11,one(Y),11,negated(one(X)),negate,11});          // the overlap
  op(UTOF,{mask(1),11,one(X),11});
  op(DIV,{mask(1),11,one(X),11,cb_one(Y),1,1});                        // r11.x = y weight
  op(MOV,{mask(1),8,one(X),10});
  op(LOOP,{});
   op(UGE,{mask(2),11,one(X),8,one(Z),10});
   op(BREAKC_NZ,{one(Y),11});
   op(IMUL,{null_operand,mask(2),11,one(X),8,cb_one(Z),1,2});
   op(IADD,{mask(4),11,one(Y),11,cb_one(Z),1,2});
   op(UMAX,{mask(2),11,one(Y),11,one(X),9});
   op(UMIN,{mask(4),11,one(Z),11,one(Z),9});
   op(IADD,{mask(2),11,one(Z),11,negated(one(Y)),negate,11});
   op(UTOF,{mask(2),11,one(Y),11});
   op(DIV,{mask(2),11,one(Y),11,cb_one(X),1,1});                       // x weight
   op(MUL,{mask(2),11,one(Y),11,one(X),11});                           // r11.y = x weight * y weight
   out.insert(out.end(),{0x8900002d,0x800000c2,0x00155543,mask(1|2|4),12,0x00100e46,8,0x00107e46,1}); // ld r12.xyz, r8.xyzw, t1.xyzw
   op(MAD,{mask(1|2|4),7,swizzle(X,Y,Z,X),12,swizzle(Y,Y,Y,Y),11,swizzle(X,Y,Z,X),7});
   op(IADD,{mask(1),8,one(X),8,imm1,1});
  op(ENDLOOP,{});
  op(IADD,{mask(2),8,one(Y),8,imm1,1});
 op(ENDLOOP,{});
 op(MOV,{mask(1|2|4),dest,swizzle(X,Y,Z,X),7});
}
std::vector<std::byte> prepare(std::span<const std::byte> input){
 require(input.size()==7584&&digest(input)=="b7afd17a818940e9f2da3830ad2e208767930f4decd1572b4fb64b164c85a532","known DOF preparation shader");
 const auto atword=[&](std::size_t offset){require(offset+4<=input.size(),"container bounds");Word w{};std::memcpy(&w,input.data()+offset,4);return w;};
 std::vector<std::vector<Word>> chunks;unsigned programs=0;
 for(unsigned c=0;c<atword(28);++c){const auto at=atword(32+4*c),tag=atword(at),size=atword(at+4);
  require(size%4==0&&std::uint64_t(at)+8+size<=input.size(),"chunk bounds");
  if(tag==0x54415453)continue; // Drop stale STAT; preserve RDEF and signatures.
  std::vector<Word> native(size/4);std::memcpy(native.data(),input.data()+at+8,size);
  if(tag==0x58454853){++programs;require(native.size()==736&&native[0]==0x50050&&native[1]==736,"CS program");
   std::vector<Word> result{native[0],0};unsigned edits=0;
   for(std::size_t i=2;i<native.size();){const auto n=(native[i]>>24)&127U;require(n&&n<=native.size()-i,"instruction bounds");
    std::vector<Word> ins(native.begin()+i,native.begin()+i+n);
    if(i==40){require(ins==std::vector<Word>{0x02000068,6},"temp declaration");ins[1]=13;++edits;}
    // CBScreen is declared CB1[3]: fScreenSize is cb1[1].xy, iViewSize cb1[2].zw.
    if(i==7)require(ins==std::vector<Word>{0x04000059,0x00208e46,1,3},"CBScreen declaration");
    if(i==553||i==600||i==645||i==688){
     const Word dest=i==553?2:i==688?0:3,coord=i==553||i==688?0:3;
     require(ins==std::vector<Word>{0x8900002d,0x800000c2,0x00155543,0x00100072,dest,0x00100e46,coord,0x00107e46,1},"color load");
     emit_area_load(result,dest,coord);++edits;i+=n;continue;
    }
    result.insert(result.end(),ins.begin(),ins.end());i+=n;
   }
   require(edits==5,"all four color reads adapted");result[1]=static_cast<Word>(result.size());native=std::move(result);
  }
  native.insert(native.begin(),{tag,static_cast<Word>(native.size()*4)});chunks.push_back(std::move(native));
 }
 require(programs==1,"one compute program");std::vector<Word> out(8+chunks.size());
 out[0]=0x43425844;out[5]=1;out[7]=static_cast<Word>(chunks.size());
 for(std::size_t i=0;i<chunks.size();++i){out[8+i]=static_cast<Word>(out.size()*4);out.insert(out.end(),chunks[i].begin(),chunks[i].end());}
 out[6]=static_cast<Word>(out.size()*4);auto bytes=reinterpret_cast<BYTE *>(out.data());
 ComputeHashRetail(bytes+20,static_cast<UINT32>(out.size()*4-20),bytes+4);
 std::vector<std::byte> result(out.size()*4);std::memcpy(result.data(),out.data(),result.size());return result;
}

std::vector<std::byte> synchronized_reduction(std::span<const std::byte> input){
 require(input.size()==6524&&digest(input)=="98c622bab8c0e113eeb4062dcc78fec129441854bcf67f61db6ed6239ca68e3c","known horizontal reduction");
 const auto word=[&](std::size_t at){require(at+4<=input.size(),"reduction container bounds");Word w{};std::memcpy(&w,input.data()+at,4);return w;};
 std::vector<std::vector<Word>> chunks;unsigned programs=0;
 for(unsigned c=0;c<word(28);++c){const auto at=word(32+4*c),tag=word(at),size=word(at+4);
  require(size%4==0&&std::uint64_t(at)+8+size<=input.size(),"reduction chunk bounds");
  if(tag==0x54415453)continue;
  std::vector<Word> data(size/4);std::memcpy(data.data(),input.data()+at+8,size);
  if(tag==0x58454853){++programs;require(data.size()==511&&data[0]==0x50050&&data[1]==511,"horizontal CS program");
   require(data[376]==0x010018be&&data[449]==0x01000015&&data[450]==0x0304001f&&data[451]==0x0010001a&&data[452]==0,"horizontal final branch");
   // All 128 merge threads must finish their group-shared atomics before the
   // first 16 threads load the combined results. Insert outside both branches.
   data.insert(data.begin()+450,0x010018be);data[1]=static_cast<Word>(data.size());
  }
  data.insert(data.begin(),{tag,static_cast<Word>(data.size()*4)});chunks.push_back(std::move(data));
 }
 require(programs==1,"one horizontal reduction program");std::vector<Word> out(8+chunks.size());
 out[0]=0x43425844;out[5]=1;out[7]=static_cast<Word>(chunks.size());
 for(std::size_t i=0;i<chunks.size();++i){out[8+i]=static_cast<Word>(out.size()*4);out.insert(out.end(),chunks[i].begin(),chunks[i].end());}
 out[6]=static_cast<Word>(out.size()*4);auto *bytes=reinterpret_cast<BYTE *>(out.data());
 ComputeHashRetail(bytes+20,static_cast<UINT32>(out.size()*4-20),bytes+4);
 std::vector<std::byte> result(out.size()*4);std::memcpy(result.data(),out.data(),result.size());return result;
}

}
bool dof_compute_identity(std::span<const std::byte> bytes,unsigned pass){
 constexpr const char *hashes[]{
 "98c622bab8c0e113eeb4062dcc78fec129441854bcf67f61db6ed6239ca68e3c",
 "d2c5eca59598bd0ebdd67cd2d5e3858e79862a50a8e076423b7c6a3f498f6245",
 "4f54cf139cc8222b235670375faa7a8f0970bf51a699c30b82357bfa8f6001d8",
 "b7afd17a818940e9f2da3830ad2e208767930f4decd1572b4fb64b164c85a532",
 "c4c5caf1ae7f1cdfc6e2762ef954f51ed65b64056cbb02778c963977f84bec7f",
 "61b739ad7861bffa528e0f9a1b58958765549cd0aac1d8e6a2b5366cc11ac6db"};
 return pass<6&&digest(bytes)==hashes[pass];
}
std::vector<std::byte> make_dof_compute_shader(std::span<const std::byte> bytes,unsigned pass){
 require(pass==0||pass==3,"adapted compute pass");
 return pass==0?synchronized_reduction(bytes):prepare(bytes);
}
}
