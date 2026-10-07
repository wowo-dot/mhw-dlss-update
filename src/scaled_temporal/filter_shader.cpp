#include "filter_shader.hpp"
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
    require(at<=bytes.size()&&bytes.size()-at>=4,"filter word bounds");
    Word result{};std::memcpy(&result,bytes.data()+at,4);return result;
}
bool known(std::span<const std::byte> input){
    constexpr std::array<unsigned char,32> expected{0x3e,0xed,0x9b,0x57,0xbb,0xbd,0xf3,0x8d,
        0x14,0xd9,0xa9,0x45,0x96,0xfc,0x93,0xa1,0x07,0xaa,0x99,0xea,0xb2,0x75,0xaf,0x71,
        0x18,0xb9,0x5f,0xeb,0xbb,0xc1,0xba,0x18};
    std::array<unsigned char,32> hash{};
    return input.size()==10008&&BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        reinterpret_cast<PUCHAR>(const_cast<std::byte *>(input.data())),static_cast<ULONG>(input.size()),
        hash.data(),static_cast<ULONG>(hash.size()))>=0&&hash==expected;
}
// These are DXBC token encodings, not shader source. The hash above gates the
// entire program before these exact instruction offsets can be used.
void append(std::vector<Word> &out,std::initializer_list<Word> tokens){out.insert(out.end(),tokens);}
std::vector<Word> adapt(std::span<const Word> native,FilterGeometry g){
    require(native.size()==1399&&native[0]==0x50&&native[1]==native.size(),"filter ps_5_0 program");
    const Word sx=std::bit_cast<Word>(float(g.render_width)/g.output_width);
    const Word sy=std::bit_cast<Word>(float(g.render_height)/g.output_height);
    const Word ix=std::bit_cast<Word>(float(g.output_width)/g.render_width);
    const Word iy=std::bit_cast<Word>(float(g.output_height)/g.render_height);
    std::vector<Word> out{native[0],0};out.reserve(native.size()+64);
    unsigned edits=0;
    for(std::size_t at=2;at<native.size();){
        const auto n=(native[at]>>24)&127U;require(n&&n<=native.size()-at,"filter instruction bounds");
        std::vector<Word> ins(native.begin()+at,native.begin()+at+n);
        if(at==48){require(ins==std::vector<Word>{0x02000068,10},"filter temporary declaration");ins[1]=13;++edits;}
        if(at==50){
            // r10.xy = raster position in the original render lattice.
            append(out,{0x0a000038,0x00100032,10,0x00101046,0,0x00004002,sx,sy,0,0});
            // Integer t0 loads use the full-size raster pixel, including alpha.
            append(out,{0x0500001c,0x00100032,12,0x00101046,0});
            append(out,{0x08000036,0x001000c2,12,0x00004002,0,0,0,0});++edits;
        }
        if(at==61){require(ins==std::vector<Word>{0x08000038,0x001000c2,0,0x00101406,0,0x00208ea6,1,1},"filter position use");
            ins[3]=0x00100406;ins[4]=10;++edits;}
        if(at==311||at==501||at==650||at==1067){
            require(n==13&&ins[0]==0x8d000048&&ins[7]==0x00107e46&&ins[8]==0&&
                (ins[5]==0x00100ae6||ins[5]==0x00100046),"filter t0 sample");
            // Preserve native clamps/blur distances in render space. Only the
            // color coordinate is expanded; auxiliary reads use original temps.
            append(out,{0x0a000038,0x00100032,11,ins[5],ins[6],0x00004002,ix,iy,0,0});
            ins[5]=0x00100046;ins[6]=11;++edits;
        }
        if(at==1354||at==1384){
            require(n==9&&ins[0]==0x8900002d&&ins[6]==1&&ins[7]==0x00107e46&&ins[8]==0,
                "filter t0 load");ins[6]=12;++edits;
        }
        out.insert(out.end(),ins.begin(),ins.end());at+=n;
    }
    require(edits==9,"filter all edits accounted");out[1]=static_cast<Word>(out.size());return out;
}
}
bool valid_filter_geometry(FilterGeometry g) noexcept {
    if(g.render_width<32||g.render_height<32||g.render_width>8192||g.render_height>8192||g.output_width>8192||g.output_height>8192||
       std::uint64_t(g.output_width)*g.output_height>16777216)return false;
    const bool identity=g.render_width==g.output_width&&g.render_height==g.output_height;
    // Half, truncated per axis: exact 2:1, or a pillarboxed odd scene
    // (3413x1440 rendered at 1706x720).
    const bool half=g.render_width==g.output_width/2&&g.render_height==g.output_height/2;
    return identity||half;
}
std::vector<std::byte> make_filter_shader(std::span<const std::byte> native,FilterGeometry g){
    require(valid_filter_geometry(g),"filter geometry");require(known(native),"unrecognized native filter shader");
    require(word(native,0)==fourcc('D','X','B','C')&&word(native,20)==1&&word(native,24)==native.size(),"filter container");
    const auto count=word(native,28);require(count>0&&count<=64,"filter chunk count");
    std::vector<std::vector<Word>> chunks;unsigned programs=0;
    for(unsigned i=0;i<count;++i){
        const auto at=word(native,32+4*i),size=word(native,at+4),tag=word(native,at);
        require(size%4==0&&std::uint64_t(at)+8+size<=native.size(),"filter chunk bounds");
        // Statistics describe the old instruction/temp counts. They are not
        // required for execution; retain native reflection and both signatures.
        if(tag==fourcc('S','T','A','T'))continue;
        std::vector<Word> data(size/4);std::memcpy(data.data(),native.data()+at+8,size);
        if(tag==fourcc('S','H','E','X')){data=adapt(data,g);++programs;}
        data.insert(data.begin(),{tag,static_cast<Word>(data.size()*4)});chunks.push_back(std::move(data));
    }
    require(programs==1,"filter one shader program");
    std::vector<Word> out(8+chunks.size());out[0]=fourcc('D','X','B','C');out[5]=1;out[7]=static_cast<Word>(chunks.size());
    for(std::size_t i=0;i<chunks.size();++i){out[8+i]=static_cast<Word>(out.size()*4);out.insert(out.end(),chunks[i].begin(),chunks[i].end());}
    out[6]=static_cast<Word>(out.size()*4);
    auto bytes=reinterpret_cast<BYTE *>(out.data());
    ComputeHashRetail(bytes+20,static_cast<UINT32>(out.size()*4-20),bytes+4);
    std::vector<std::byte> result(out.size()*4);std::memcpy(result.data(),out.data(),result.size());return result;
}
}
