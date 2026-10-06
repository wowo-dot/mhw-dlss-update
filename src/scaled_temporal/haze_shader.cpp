#include "haze_shader.hpp"
#include <windows.h>
#include <bcrypt.h>
#include "../../third_party/dxc_hash/DxilHash.h"
#include <algorithm>
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
Word word(std::span<const std::byte> bytes,std::size_t at){
    require(at<=bytes.size()&&bytes.size()-at>=4,"haze word bounds");
    Word result{};std::memcpy(&result,bytes.data()+at,4);return result;
}
bool known(std::span<const std::byte> input){
    constexpr std::array<unsigned char,32> expected{0x4f,0x89,0x0f,0xfa,0x89,0xea,0x59,0xa2,0x18,0xdb,0x7e,0x19,0x1d,0x8a,0xab,0x48,0xe9,0xd5,0xfe,0x4b,0xe2,0x00,0xc5,0x0a,0x60,0x6b,0x22,0x59,0x4b,0xb5,0x7f,0x69};
    std::array<unsigned char,32> hash{};
    return input.size()==14596&&BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        reinterpret_cast<PUCHAR>(const_cast<std::byte *>(input.data())),static_cast<ULONG>(input.size()),
        hash.data(),static_cast<ULONG>(hash.size()))>=0&&hash==expected;
}
// These are DXBC token encodings, not shader source. The hash above gates the
// entire program before these exact instruction offsets can be used.
void append(std::vector<Word> &out,std::initializer_list<Word> tokens){out.insert(out.end(),tokens);}
std::vector<Word> adapt(std::span<const Word> native,FilterGeometry g){
    require(native.size()==2604&&native[0]==0x50&&native[1]==native.size(),"haze ps_5_0 program");
    const Word ix=std::bit_cast<Word>(float(g.output_width)/g.render_width);
    const Word iy=std::bit_cast<Word>(float(g.output_height)/g.render_height);
    std::vector<Word> out{native[0],0};out.reserve(native.size()+50);
    unsigned edits=0;
    for(std::size_t at=2;at<native.size();){
        const auto n=(native[at]>>24)&127U;require(n&&n<=native.size()-at,"haze instruction bounds");
        std::vector<Word> ins(native.begin()+at,native.begin()+at+n);
        if(at==46){require(ins==std::vector<Word>{0x02000068,6},"haze temporary declaration");ins[1]=7;++edits;}
        if(at==487||at==942||at==1585||at==2135||at==2580){
            require(ins==std::vector<Word>{0x8b000045,0x800000c2,0x00155543,0x00100072,0,
                0x00100046,1,0x00107e46,0,0x00106000,0},"haze final color sample");
            // r1.xy remains the native clamped/refraction coordinate for depth
            // and all other calculations; only the t0 operand uses new r6.xy.
            append(out,{0x0a000038,0x00100032,6,0x00100046,1,0x00004002,ix,iy,0,0});
            ins[6]=6;++edits;
        }
        out.insert(out.end(),ins.begin(),ins.end());at+=n;
    }
    require(edits==6,"haze all edits accounted");out[1]=static_cast<Word>(out.size());return out;
}
}
std::vector<std::byte> make_haze_shader(std::span<const std::byte> native,FilterGeometry g){
    require(valid_filter_geometry(g),"haze geometry");require(known(native),"unrecognized native haze shader");
    require(word(native,0)==fourcc('D','X','B','C')&&word(native,20)==1&&word(native,24)==native.size(),"haze container");
    const auto count=word(native,28);require(count>0&&count<=64,"haze chunk count");
    std::vector<std::vector<Word>> chunks;unsigned programs=0;
    for(unsigned i=0;i<count;++i){
        const auto at=word(native,32+4*i),size=word(native,at+4),tag=word(native,at);
        require(size%4==0&&std::uint64_t(at)+8+size<=native.size(),"haze chunk bounds");
        // Statistics describe the old instruction/temp counts. They are not
        // required for execution; retain native reflection and both signatures.
        if(tag==fourcc('S','T','A','T'))continue;
        std::vector<Word> data(size/4);std::memcpy(data.data(),native.data()+at+8,size);
        if(tag==fourcc('S','H','E','X')){data=adapt(data,g);++programs;}
        data.insert(data.begin(),{tag,static_cast<Word>(data.size()*4)});chunks.push_back(std::move(data));
    }
    require(programs==1,"haze one shader program");
    std::vector<Word> out(8+chunks.size());out[0]=fourcc('D','X','B','C');out[5]=1;out[7]=static_cast<Word>(chunks.size());
    for(std::size_t i=0;i<chunks.size();++i){out[8+i]=static_cast<Word>(out.size()*4);out.insert(out.end(),chunks[i].begin(),chunks[i].end());}
    out[6]=static_cast<Word>(out.size()*4);
    auto bytes=reinterpret_cast<BYTE *>(out.data());
    ComputeHashRetail(bytes+20,static_cast<UINT32>(out.size()*4-20),bytes+4);
    std::vector<std::byte> result(out.size()*4);std::memcpy(result.data(),out.data(),result.size());return result;
}
}
