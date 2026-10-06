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
   const auto emit=[&](std::initializer_list<Word> v){result.insert(result.end(),v);};
   for(std::size_t i=2;i<native.size();){const auto n=(native[i]>>24)&127U;require(n&&n<=native.size()-i,"instruction bounds");
    std::vector<Word> ins(native.begin()+i,native.begin()+i+n);
    if(i==40){require(ins==std::vector<Word>{0x02000068,6},"temp declaration");ins[1]=9;++edits;}
    if(i==553||i==600||i==645||i==688){
     const Word dest=i==553?2:i==688?0:3,coord=i==553||i==688?0:3;
     require(ins==std::vector<Word>{0x8900002d,0x800000c2,0x00155543,0x00100072,dest,0x00100e46,coord,0x00107e46,1},"color load");
     // A render-lattice texel is the 2x2 box average of reconstructed color.
     // r6 preserves integer coordinates even when the original ld aliases its
     // source and destination. r7/r8 retain RGB only; native alpha is untouched.
     emit({0x0a000029,0x00100032,6,0x00100046,coord,0x00004002,1,1,0,0}); // ishl
     emit({0x08000036,0x001000c2,6,0x00004002,0,0,0,0}); // mov zw=0
     const auto load=[&](Word d){emit({0x8900002d,0x800000c2,0x00155543,0x00100072,d,0x00100e46,6,0x00107e46,1});};
     const auto step=[&](Word mask,Word swizzle,Word value){emit({0x0700001e,mask,6,swizzle,6,0x00004001,value});};
     const auto add=[&]{emit({0x07000000,0x00100072,7,0x00100246,7,0x00100246,8});};
     load(7);step(0x00100012,0x0010000a,1);load(8);add();
     step(0x00100022,0x0010001a,1);load(8);add();
     step(0x00100012,0x0010000a,0xffffffff);load(8);add();
     emit({0x07000038,0x00100072,dest,0x00100246,7,0x00004001,0x3e800000});
     ++edits;i+=n;continue;
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
