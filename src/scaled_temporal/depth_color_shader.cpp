#include "depth_color_shader.hpp"
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
    require(at<=bytes.size()&&bytes.size()-at>=4,"depth color word bounds");
    Word result{};std::memcpy(&result,bytes.data()+at,4);return result;
}
bool known(std::span<const std::byte> input){
    constexpr std::array<unsigned char,32> expected{0xc0,0x94,0x47,0x37,0xc3,0x7b,0xec,0x2c,0x5f,0x24,0x75,0x9b,0x70,0xbd,0x14,0xb1,
        0x6e,0x89,0xdb,0xb3,0x87,0x0a,0xea,0xb6,0x62,0xdd,0x7a,0x61,0x1c,0xc1,0xf9,0xc1};
    std::array<unsigned char,32> hash{};
    return input.size()==5568&&BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        reinterpret_cast<PUCHAR>(const_cast<std::byte *>(input.data())),static_cast<ULONG>(input.size()),
        hash.data(),static_cast<ULONG>(hash.size()))>=0&&hash==expected;
}
// These are DXBC token encodings, not shader source. The hash above gates the
// entire program before these exact instruction offsets can be used.
void append(std::vector<Word> &out,std::initializer_list<Word> tokens){out.insert(out.end(),tokens);}
std::vector<Word> adapt(std::span<const Word> native,FilterGeometry g){
    require(native.size()==387&&native[0]==0x50&&native[1]==native.size(),"depth color ps_5_0 program");
    const Word sx=std::bit_cast<Word>(float(g.render_width)/g.output_width);
    const Word sy=std::bit_cast<Word>(float(g.render_height)/g.output_height);
    const Word ix=std::bit_cast<Word>(float(g.output_width)/g.render_width);
    const Word iy=std::bit_cast<Word>(float(g.output_height)/g.render_height);
    std::vector<Word> out{native[0],0};out.reserve(native.size()+24);
    unsigned edits=0;
    for(std::size_t at=2;at<native.size();){
        const auto n=(native[at]>>24)&127U;require(n&&n<=native.size()-at,"depth color instruction bounds");
        std::vector<Word> ins(native.begin()+at,native.begin()+at+n);
        if(at==36){require(ins==std::vector<Word>{0x02000068,3},"depth color temporary declaration");ins[1]=5;++edits;}
        if(at==38){
            require(ins==std::vector<Word>{0x8b000045,0x800000c2,0x00155543,0x00100072,0,0x00101046,1,0x00107e46,0,0x00106000,0},
                "depth color scene sample");
            // r3.xy = raster position on the render lattice (depth load, view ray).
            append(out,{0x0a000038,0x00100032,3,0x00101046,0,0x00004002,sx,sy,0,0});
            // r4.xy = the native render-space UV expanded to the full-output color.
            append(out,{0x0a000038,0x00100032,4,0x00101046,1,0x00004002,ix,iy,0,0});
            ins[5]=0x00100046;ins[6]=4;++edits;
        }
        if(at==49){require(ins==std::vector<Word>{0x0500001b,0x00100032,1,0x00101046,0},"depth color depth texel");
            ins[3]=0x00100046;ins[4]=3;++edits;}
        if(at==71){require(ins==std::vector<Word>{0x09000000,0x00100032,1,0x00101046,0,0x80208046,0x41,1,3},"depth color view position");
            ins[3]=0x00100046;ins[4]=3;++edits;}
        out.insert(out.end(),ins.begin(),ins.end());at+=n;
    }
    require(edits==4,"depth color all edits accounted");out[1]=static_cast<Word>(out.size());return out;
}
}
std::vector<std::byte> make_depth_color_shader(std::span<const std::byte> native,FilterGeometry g){
    require(valid_filter_geometry(g),"depth color geometry");require(known(native),"unrecognized native depth color shader");
    require(word(native,0)==fourcc('D','X','B','C')&&word(native,20)==1&&word(native,24)==native.size(),"depth color container");
    const auto count=word(native,28);require(count>0&&count<=64,"depth color chunk count");
    std::vector<std::vector<Word>> chunks;unsigned programs=0;
    for(unsigned i=0;i<count;++i){
        const auto at=word(native,32+4*i),size=word(native,at+4),tag=word(native,at);
        require(size%4==0&&std::uint64_t(at)+8+size<=native.size(),"depth color chunk bounds");
        // Statistics describe the old instruction/temp counts. They are not
        // required for execution; retain native reflection and both signatures.
        if(tag==fourcc('S','T','A','T'))continue;
        std::vector<Word> data(size/4);std::memcpy(data.data(),native.data()+at+8,size);
        if(tag==fourcc('S','H','E','X')){data=adapt(data,g);++programs;}
        data.insert(data.begin(),{tag,static_cast<Word>(data.size()*4)});chunks.push_back(std::move(data));
    }
    require(programs==1,"depth color one shader program");
    std::vector<Word> out(8+chunks.size());out[0]=fourcc('D','X','B','C');out[5]=1;out[7]=static_cast<Word>(chunks.size());
    for(std::size_t i=0;i<chunks.size();++i){out[8+i]=static_cast<Word>(out.size()*4);out.insert(out.end(),chunks[i].begin(),chunks[i].end());}
    out[6]=static_cast<Word>(out.size()*4);
    auto bytes=reinterpret_cast<BYTE *>(out.data());
    ComputeHashRetail(bytes+20,static_cast<UINT32>(out.size()*4-20),bytes+4);
    std::vector<std::byte> result(out.size()*4);std::memcpy(result.data(),out.data(),result.size());return result;
}
}
