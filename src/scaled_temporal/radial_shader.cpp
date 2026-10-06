#include "radial_shader.hpp"
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
    require(at<=bytes.size()&&bytes.size()-at>=4,"radial word bounds");
    Word result{};std::memcpy(&result,bytes.data()+at,4);return result;
}
bool known(std::span<const std::byte> input){
    constexpr std::array<unsigned char,32> expected{0x90,0xf3,0xf4,0xd5,0x59,0x43,0x57,0xe5,0x35,0x0a,0xcf,0x3d,0x5b,0xda,0x57,0xdb,0x46,0x41,0xb5,0x0e,0x6a,0xaa,0x57,0x87,0xd3,0xae,0x34,0x14,0x63,0x86,0x07,0x41};
    std::array<unsigned char,32> hash{};
    return input.size()==6444&&BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,
        reinterpret_cast<PUCHAR>(const_cast<std::byte *>(input.data())),static_cast<ULONG>(input.size()),
        hash.data(),static_cast<ULONG>(hash.size()))>=0&&hash==expected;
}
// These are DXBC token encodings, not shader source. The hash above gates the
// entire program before these exact instruction offsets can be used.
void append(std::vector<Word> &out,std::initializer_list<Word> tokens){out.insert(out.end(),tokens);}
std::vector<Word> adapt(std::span<const Word> native,FilterGeometry g){
    require(native.size()==646&&native[0]==0x50&&native[1]==native.size(),"radial ps_5_0 program");
    const Word sx=std::bit_cast<Word>(float(g.render_width)/g.output_width);
    const Word sy=std::bit_cast<Word>(float(g.render_height)/g.output_height);
    const Word ix=std::bit_cast<Word>(float(g.output_width)/g.render_width);
    const Word iy=std::bit_cast<Word>(float(g.output_height)/g.render_height);
    std::vector<Word> out{native[0],0};out.reserve(native.size()+50);unsigned edits=0;
    for(std::size_t at=2;at<native.size();){
        const auto n=(native[at]>>24)&127U;require(n&&n<=native.size()-at,"radial instruction bounds");
        std::vector<Word> ins(native.begin()+at,native.begin()+at+n);
        if(at==53){require(ins==std::vector<Word>{0x02000068,7},"radial temporary declaration");ins[1]=9;++edits;}
        if(at==139){
            require(ins==std::vector<Word>{0x0c000032,0x00100032,1,0x80101046,0x41,0,0x00208ae6,1,1,0x00208046,3,0},"radial position use");
            // Scale the inverse size while retaining native SV_POSITION.
            // The bound CB remains untouched.
            append(out,{0x0b000038,0x00100032,7,0x00208ae6,1,1,0x00004002,sx,sy,0,0});
            ins={0x0b000032,0x00100032,1,0x80101046,0x41,0,0x00100046,7,0x00208046,3,0};edits+=2;
        }
        if(at==450||at==461||at==495||at==507){
            require(n==11&&ins[0]==0x8b000045&&ins[1]==0x800000c2&&ins[2]==0x00155543&&
                ins[4]==6&&ins[7]==0x00107e46&&ins[8]==0&&ins[9]==0x00106000&&ins[10]==0&&
                ins[3]==(at==450?0x00100012U:at==461?0x00100022U:at==495?0x00100042U:0x00100072U)&&
                ins[5]==((at==461||at==507)?0x00100ae6U:0x00100046U)&&ins[6]==(at==507?4U:5U),"radial t0 sample");
            // Preserve native radial math, clamps, chromatic offsets and t1/t2.
            // Convert only the final color coordinate after native clamping.
            append(out,{0x0a000038,0x00100032,8,ins[5],ins[6],0x00004002,ix,iy,0,0});
            ins[5]=0x00100046;ins[6]=8;++edits;
        }
        out.insert(out.end(),ins.begin(),ins.end());at+=n;
    }
    require(edits==7,"radial all edits accounted");out[1]=static_cast<Word>(out.size());return out;
}
}
std::vector<std::byte> make_radial_shader(std::span<const std::byte> native,FilterGeometry g){
    require(valid_filter_geometry(g),"radial geometry");require(known(native),"unrecognized native radial shader");
    require(word(native,0)==fourcc('D','X','B','C')&&word(native,20)==1&&word(native,24)==native.size(),"radial container");
    const auto count=word(native,28);require(count>0&&count<=64,"radial chunk count");
    std::vector<std::vector<Word>> chunks;unsigned programs=0;
    for(unsigned i=0;i<count;++i){
        const auto at=word(native,32+4*i),size=word(native,at+4),tag=word(native,at);
        require(size%4==0&&std::uint64_t(at)+8+size<=native.size(),"radial chunk bounds");
        // Statistics describe the old instruction/temp counts. They are not
        // required for execution; retain native reflection and both signatures.
        if(tag==fourcc('S','T','A','T'))continue;
        std::vector<Word> data(size/4);std::memcpy(data.data(),native.data()+at+8,size);
        if(tag==fourcc('S','H','E','X')){data=adapt(data,g);++programs;}
        data.insert(data.begin(),{tag,static_cast<Word>(data.size()*4)});chunks.push_back(std::move(data));
    }
    require(programs==1,"radial one shader program");
    std::vector<Word> out(8+chunks.size());out[0]=fourcc('D','X','B','C');out[5]=1;out[7]=static_cast<Word>(chunks.size());
    for(std::size_t i=0;i<chunks.size();++i){out[8+i]=static_cast<Word>(out.size()*4);out.insert(out.end(),chunks[i].begin(),chunks[i].end());}
    out[6]=static_cast<Word>(out.size()*4);
    auto bytes=reinterpret_cast<BYTE *>(out.data());
    ComputeHashRetail(bytes+20,static_cast<UINT32>(out.size()*4-20),bytes+4);
    std::vector<std::byte> result(out.size()*4);std::memcpy(result.data(),out.data(),result.size());return result;
}
std::array<DependencyVertex,4> dependency_vertices(FilterGeometry g,bool copy){
    require(valid_filter_geometry(g),"dependency geometry");
    const float u=copy?1.F:float(g.render_width)/g.output_width;
    const float v=copy?1.F:float(g.render_height)/g.output_height;
    return {{{{-1,1},{0,0},{0,0}},{{-1,-1},{0,v},{0,1}},{{1,1},{u,0},{1,0}},{{1,-1},{u,v},{1,1}}}};
}

}
