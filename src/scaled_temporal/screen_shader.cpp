#include "screen_shader.hpp"
#include <windows.h>
#include "../../third_party/dxc_hash/DxilHash.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <map>
#include <stdexcept>

namespace mhw::scaled_temporal {
namespace {
using Word=std::uint32_t;
constexpr Word fourcc(char a,char b,char c,char d) noexcept {return Word(a)|(Word(b)<<8)|(Word(c)<<16)|(Word(d)<<24);}
void require(bool value,const char *why){if(!value)throw std::runtime_error(why);}
Word word(std::span<const std::byte> bytes,std::size_t at){
    require(at<=bytes.size()&&bytes.size()-at>=4,"screen shader word bounds");
    Word result{};std::memcpy(&result,bytes.data()+at,4);return result;
}
// SM4/SM5 opcodes used below (d3d11TokenizedProgramFormat).
enum : unsigned {
    op_break=2,op_breakc=3,op_call=4,op_callc=5,op_case=6,op_continue=7,op_continuec=8,op_default=10,
    op_else=18,op_endif=21,op_endloop=22,op_endswitch=23,op_ftoi=27,op_if=31,op_label=44,op_ld=45,op_ld_ms=46,
    op_loop=48,op_customdata=53,op_mov=54,op_mul=56,op_resinfo=61,op_ret=62,op_retc=63,
    op_sample=69,op_sample_c=70,op_sample_c_lz=71,op_sample_l=72,op_sample_d=73,op_sample_b=74,op_switch=76,
    op_lod=108,op_gather4=109,op_samplepos=110,op_sample_info=111,op_interface_call=119,
    op_gather4_c=126,op_gather4_po=127,op_gather4_po_c=128,
    op_eval_snapped=203,op_eval_sample_index=204,op_eval_centroid=205,
};
enum : unsigned {type_temp=0,type_input=1,type_immediate32=4,type_immediate64=5,type_resource=7};
bool declaration(unsigned op){return op==op_customdata||(op>=88&&op<=106)||(op>=143&&op<=162);}
bool control_flow(unsigned op){
    switch(op){case op_break:case op_breakc:case op_call:case op_callc:case op_case:case op_continue:case op_continuec:
        case op_default:case op_else:case op_endif:case op_endloop:case op_endswitch:case op_if:case op_label:case op_loop:
        case op_ret:case op_retc:case op_switch:return true;default:return false;}
}
struct Operand {std::size_t at{},count{};Word token{};unsigned type{},dims{},components{},mode{},selection{},index{};bool simple{};};
// One operand at ins[at] (with its extended tokens, immediates and indices).
std::size_t parse(std::span<const Word> ins,std::size_t at,Operand &o){
    require(at<ins.size(),"screen shader operand bounds");
    o={};o.at=at;o.token=ins[at];std::size_t n=1;
    if(o.token>>31){Word ext{};do{require(at+n<ins.size(),"screen shader extended operand");ext=ins[at+n++];}while(ext>>31);}
    o.type=(o.token>>12)&255;o.components=o.token&3;o.mode=(o.token>>2)&3;o.selection=(o.token>>4)&255;o.dims=(o.token>>20)&3;
    if(o.type==type_immediate32)n+=o.components==1?1:o.components==2?4:0;
    if(o.type==type_immediate64)n+=o.components==1?2:o.components==2?8:0;
    o.simple=true;
    for(unsigned d=0;d<o.dims;++d){
        const unsigned rep=(o.token>>(22+3*d))&7;if(rep)o.simple=false;
        if(rep==0||rep==3){require(at+n<ins.size(),"screen shader index");if(d==0)o.index=ins[at+n];++n;}
        if(rep==1||rep==4)n+=2;
        if(rep>=2){require(rep<=4,"screen shader index representation");Operand relative;n+=parse(ins,at+n,relative);}
    }
    if(o.dims!=1)o.simple=o.simple&&o.dims==0;
    require(at+n<=ins.size(),"screen shader operand length");o.count=n;return n;
}
unsigned swizzle_component(const Operand &o,unsigned lane){
    if(o.components!=2)return lane;
    if(o.mode==1)return (o.selection>>(2*lane))&3;
    if(o.mode==2)return o.selection&3;
    return lane;
}
// Program-order provenance of temp components for the scene-load pattern.
enum Tag : unsigned char {unknown,raster_x,raster_y,zero};
struct Tracker {
    std::map<unsigned,std::array<Tag,4>> temps;
    std::array<Tag,4> &of(unsigned r){auto i=temps.find(r);if(i==temps.end())i=temps.emplace(r,std::array<Tag,4>{}).first;return i->second;}
};
// Same component count, selection mode and selection; temp type; 1-D imm32
// index; no extension.
Word temp_token(Word source){return (source&0x00000fffU)|(1U<<20);}
Word bits(float v){return std::bit_cast<Word>(v);}
std::vector<Word> adapt(std::span<const Word> program,FilterGeometry g,unsigned scene,ScreenShaderEdits &edits){
    require(program.size()>=2&&(program[0]>>16)==0&&(program[0]&0xffff)==0x50&&program[1]==program.size(),"screen shader ps_5_0 program");
    // Pass 1: declarations, the raster-position register and the temp count.
    unsigned position=UINT_MAX,position_mask{},temps{};std::size_t temps_at=0,body=0;bool scene_declared=false;
    for(std::size_t at=2;at<program.size();){
        const Word t=program[at];const unsigned op=t&0x7ff;std::size_t n=(t>>24)&127;
        if(op==op_customdata){require(at+1<program.size(),"screen shader custom data");n=program[at+1];}
        require(n&&n<=program.size()-at,"screen shader instruction bounds");
        if(!declaration(op)){body=at;break;}
        if(op==100&&n==4&&program[at+3]==1){Operand o;parse(program.subspan(at,n),1,o);
            require(o.type==type_input&&o.simple&&position==UINT_MAX,"screen shader position declaration");position=o.index;position_mask=o.selection&15;}
        if(op==104){require(n==2&&!temps_at,"screen shader temps declaration");temps=program[at+1];temps_at=at;}
        if(op==88){Operand o;parse(program.subspan(at,n),1,o);
            if(o.type==type_resource&&o.index==scene){require(((t>>11)&31)==3,"scene register is not a 2-D texture");scene_declared=true;}}
        at+=n;
    }
    require(body&&(scene_declared||scene==UINT_MAX),"screen shader body or scene declaration");
    // Pass 2: plan every edit on the native token stream.
    struct Instruction {std::size_t at,n;std::vector<Word> words;std::vector<Word> before;};
    std::vector<Instruction> code;Tracker track;bool raster_full=false,lattice=false,expand=false;
    const unsigned lattice_reg=temps,full_reg=temps+1,uv_reg=temps+2;
    for(std::size_t at=body;at<program.size();){
        const Word t=program[at];const unsigned op=t&0x7ff;std::size_t n=(t>>24)&127;
        if(op==op_customdata){require(at+1<program.size(),"screen shader custom data");n=program[at+1];}
        require(n&&n<=program.size()-at,"screen shader instruction bounds");
        require(!declaration(op)||op==op_customdata,"screen shader declaration after code");
        Instruction ins{at,n,std::vector<Word>(program.begin()+at,program.begin()+at+n),{}};
        if(op==op_customdata||op==op_interface_call){require(op==op_customdata,"screen shader interfaces");code.push_back(std::move(ins));at+=n;continue;}
        std::size_t first=1;if(t>>31){Word ext{};do{require(first<n,"screen shader extended opcode");ext=ins.words[first++];
            // Sample controls (immediate texel offsets) only on non-scene reads; checked below.
        }while(ext>>31);}
        std::vector<Operand> ops;for(std::size_t k=first;k<n;){Operand o;k+=parse(ins.words,k,o);ops.push_back(o);}
        const auto touches_scene=[&]{for(const auto &o:ops)if(o.type==type_resource&&o.index==scene)return true;return false;}();
        const auto offsets=[&]{if(!(t>>31))return false;std::size_t k=1;Word ext{};
            do{ext=ins.words[k++];if((ext&63)==1&&(ext&0x1ffe00))return true;}while(ext>>31);return false;}();
        const bool sample=op==op_sample||op==op_sample_l||op==op_sample_b||op==op_sample_c||op==op_sample_c_lz||op==op_gather4||op==op_gather4_c;
        for(const auto &o:ops)if(o.type==type_resource&&o.simple&&o.index<edits.access.size()){
            unsigned char kind=sample?access_sample:access_other;
            if(op==op_ld&&ops.size()==3&&ops[1].type==type_temp&&ops[1].simple&&ops[1].components==2){
                const auto &tags=track.of(ops[1].index);
                if(tags[swizzle_component(ops[1],0)]==raster_x&&tags[swizzle_component(ops[1],1)]==raster_y&&tags[swizzle_component(ops[1],3)]==zero)kind=access_raster;}
            edits.access[o.index]|=kind;}
        // Rule 3 (scene loads) is decided on the native provenance, before rule 1 rewrites.
        Operand load_address{};bool scene_load=false;
        if(touches_scene){
            require(!offsets,"scene read with texel offsets");
            if(op==op_ld){
                require(ops.size()==3&&ops[2].type==type_resource&&ops[2].index==scene&&ops[1].type==type_temp&&ops[1].simple&&ops[1].components==2,"scene load operands");
                auto &tags=track.of(ops[1].index);
                require(tags[swizzle_component(ops[1],0)]==raster_x&&tags[swizzle_component(ops[1],1)]==raster_y&&
                        tags[swizzle_component(ops[1],2)]==zero&&tags[swizzle_component(ops[1],3)]==zero,"scene load is not at the raster position");
                load_address=ops[1];scene_load=true;
            }else require(sample&&ops.size()>=4&&ops[2].type==type_resource&&ops[2].index==scene,"unsupported scene access");
        }
        require(!(op==op_ld_ms||op==op_resinfo||op==op_sample_info||op==op_lod||op==op_sample_d||op==op_gather4_po||op==op_gather4_po_c||op==op_samplepos)||!touches_scene,"unsupported scene access");
        // Provenance update for this instruction's destinations.
        // Destinations are the mask-mode temp operands (sources use swizzle or
        // select-1). Instructions with two destinations update both; any other
        // write, and any control flow, forgets the pattern.
        if(control_flow(op))track.temps.clear();
        else for(const auto &o:ops)if(o.type==type_temp&&o.components==2&&o.mode==0){
            if(!o.simple){track.temps.clear();continue;}
            auto &tags=track.of(o.index);const unsigned mask=o.selection&15;const bool only=ops.size()==2&&&o==&ops.front();
            for(unsigned c=0;c<4;++c)if(mask&(1U<<c)){
                Tag tag=unknown;
                if(only&&op==op_ftoi&&ops[1].type==type_input&&ops[1].simple&&ops[1].index==position&&!(ops[1].token>>31)){
                    const auto source=swizzle_component(ops[1],c);if(source==c&&c<2)tag=c?raster_y:raster_x;}
                if(only&&op==op_mov&&!(t&(1U<<13))&&ops[1].type==type_immediate32&&!(ops[1].token>>31)){
                    const auto value=ops[1].components==1?ins.words[ops[1].at+1]:ins.words[ops[1].at+1+c];if(!value)tag=zero;}
                tags[c]=tag;
            }
        }
        if(op==op_eval_snapped||op==op_eval_sample_index||op==op_eval_centroid)
            for(const auto &o:ops)require(!(o.type==type_input&&o.index==position),"position read by eval");
        // Rewrite operands back to front so earlier offsets stay valid.
        std::vector<Word> words=ins.words;
        for(auto o=ops.rbegin();o!=ops.rend();++o){
            if(o->type==type_input&&o->index==position&&o->dims){
                // Plain, or with one operand-modifier extension (neg/abs, e.g. -v0.z)
                // that stays attached; 1-D immediate index.
                const bool extended=o->token>>31;
                const bool modifier=extended&&o->count==3&&(words[o->at+1]&0x3fU)==1&&!(words[o->at+1]>>31);
                require(o->simple&&(!extended||modifier)&&o->count==(extended?3U:2U),"position read form");
                words[o->at]=temp_token(o->token)|(o->token&0x80000000U);words[o->at+o->count-1]=lattice_reg;lattice=true;++edits.position_reads;
            }
        }
        if(scene_load){
            const std::array<Word,2> full{0x00100e46U,full_reg};  // rF.xyzw
            words.erase(words.begin()+static_cast<std::ptrdiff_t>(load_address.at),words.begin()+static_cast<std::ptrdiff_t>(load_address.at+load_address.count));
            words.insert(words.begin()+static_cast<std::ptrdiff_t>(load_address.at),full.begin(),full.end());raster_full=true;++edits.scene_loads;
        }else if(touches_scene&&sample){
            // mul rU.xy, <address as rewritten>, l(ix,iy,0,0); then sample at rU.xyxx.
            Operand address;parse(words,ops[1].at,address);
            std::vector<Word> mul{0,0x00100032U,uv_reg};
            mul.insert(mul.end(),words.begin()+static_cast<std::ptrdiff_t>(address.at),words.begin()+static_cast<std::ptrdiff_t>(address.at+address.count));
            mul.insert(mul.end(),{0x00004002U,bits(float(g.output_width)/g.render_width),bits(float(g.output_height)/g.render_height),0,0});
            mul[0]=op_mul|(static_cast<Word>(mul.size())<<24);ins.before.insert(ins.before.end(),mul.begin(),mul.end());
            const std::array<Word,2> uv{0x00100046U,uv_reg};  // rU.xyxx
            words.erase(words.begin()+static_cast<std::ptrdiff_t>(address.at),words.begin()+static_cast<std::ptrdiff_t>(address.at+address.count));
            words.insert(words.begin()+static_cast<std::ptrdiff_t>(address.at),uv.begin(),uv.end());expand=true;++edits.scene_samples;
        }
        require(words.size()<=127,"screen shader instruction length");
        words[0]=(words[0]&~(127U<<24))|(static_cast<Word>(words.size())<<24);ins.words=std::move(words);
        code.push_back(std::move(ins));at+=n;
    }
    require(position!=UINT_MAX||(!lattice&&!raster_full),"screen shader position");
    // Emit: declarations (temps raised), prologue, rewritten body.
    const unsigned added=(lattice||raster_full||expand)?3U:0U;
    std::vector<Word> out{program[0],0};out.reserve(program.size()+64);
    for(std::size_t at=2;at<body;){
        const Word t=program[at];std::size_t n=(t>>24)&127;if((t&0x7ff)==op_customdata)n=program[at+1];
        std::vector<Word> d(program.begin()+at,program.begin()+at+n);if(at==temps_at)d[1]+=added;
        out.insert(out.end(),d.begin(),d.end());at+=n;
    }
    if(added&&!temps_at)out.insert(out.end(),{0x02000068U,added});
    if(lattice){
        // rL.xy = raster position on the render lattice; rL.z/w = position z/w,
        // only the components the input declares (v0.xyz has no readable w).
        out.insert(out.end(),{0x0a000038U,0x00100032U,lattice_reg,0x00101046U,position,0x00004002U,
            bits(float(g.render_width)/g.output_width),bits(float(g.render_height)/g.output_height),0,0});
        // Source swizzle .zwzw: lane z reads z, lane w reads w.
        if(position_mask&12)out.insert(out.end(),{0x05000036U,0x00100002U|((position_mask&12U)<<4),lattice_reg,0x00101ee6U,position});
    }
    if(raster_full)out.insert(out.end(),{0x0500001bU,0x00100032U,full_reg,0x00101046U,position,0x08000036U,0x001000c2U,full_reg,0x00004002U,0,0,0,0});
    for(const auto &i:code){out.insert(out.end(),i.before.begin(),i.before.end());out.insert(out.end(),i.words.begin(),i.words.end());}
    out[1]=static_cast<Word>(out.size());return out;
}
}
std::vector<std::byte> make_screen_shader(std::span<const std::byte> native,FilterGeometry g,unsigned scene_register,ScreenShaderEdits &edits){
    require(valid_filter_geometry(g),"screen shader geometry");edits={};
    require(word(native,0)==fourcc('D','X','B','C')&&word(native,20)==1&&word(native,24)==native.size(),"screen shader container");
    const auto count=word(native,28);require(count>0&&count<=64,"screen shader chunk count");
    std::vector<std::vector<Word>> chunks;unsigned programs=0;
    for(unsigned i=0;i<count;++i){
        const auto at=word(native,32+4*i),size=word(native,at+4),tag=word(native,at);
        require(size%4==0&&std::uint64_t(at)+8+size<=native.size(),"screen shader chunk bounds");
        // Statistics describe the old instruction/temp counts; reflection and
        // both signatures are retained unchanged.
        if(tag==fourcc('S','T','A','T'))continue;
        std::vector<Word> data(size/4);std::memcpy(data.data(),native.data()+at+8,size);
        if(tag==fourcc('S','H','E','X')){data=adapt(data,g,scene_register,edits);++programs;}
        require(tag!=fourcc('S','H','D','R'),"screen shader SM4 program");
        data.insert(data.begin(),{tag,static_cast<Word>(data.size()*4)});chunks.push_back(std::move(data));
    }
    require(programs==1,"screen shader one program");
    std::vector<Word> out(8+chunks.size());out[0]=fourcc('D','X','B','C');out[5]=1;out[7]=static_cast<Word>(chunks.size());
    for(std::size_t i=0;i<chunks.size();++i){out[8+i]=static_cast<Word>(out.size()*4);out.insert(out.end(),chunks[i].begin(),chunks[i].end());}
    out[6]=static_cast<Word>(out.size()*4);
    auto bytes=reinterpret_cast<BYTE *>(out.data());
    ComputeHashRetail(bytes+20,static_cast<UINT32>(out.size()*4-20),bytes+4);
    std::vector<std::byte> result(out.size()*4);std::memcpy(result.data(),out.data(),result.size());return result;
}
}
