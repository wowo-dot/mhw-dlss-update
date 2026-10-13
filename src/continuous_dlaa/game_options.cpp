#include "game_options.hpp"
#include "../projection_binding/memory.hpp"
#include "../ngx_startup/startup.hpp"
#include <windows.h>
#include <MinHook.h>
#include <xmmintrin.h>
#include <array>
#include <atomic>
#include <ostream>

namespace mhw::continuous_dlaa::game_options {
namespace {
namespace pb=projection_binding;
// The game's functions (clean image; gates in install()).
using AaApply=std::uintptr_t (*)(unsigned char);           // 0x43dcb0: the anti-aliasing option (TAA under its DLSS)
using RsApply=__m128 (*)(char);                            // 0x43eeb0: resolution scaling (High under its DLSS); the scale
using AaSetter=void (*)(std::uintptr_t,char);              // 0x2287a00: the TAA/FXAA bytes at table +0xe9e4
using IniRead=char (*)(std::uintptr_t);                    // 0x4413e0, 0x442120: the saved choice
using RowEnabled=std::uint64_t (*)(int,unsigned char);     // 0x53f580: a row takes input
using PageRefresh=std::uintptr_t (*)(std::uintptr_t,int);  // 0x53f440: builds the visible rows, greys the locked ones
using RowBuild=std::uintptr_t (*)(std::uintptr_t,char,unsigned,int,std::uintptr_t,unsigned,unsigned,std::uintptr_t); // 0x53f190
using RestoreDefaults=std::uintptr_t (*)(std::uintptr_t);  // 0x540720: copies the defaults, except locked rows
using Grey=std::uintptr_t (*)(std::uintptr_t,int);         // 0xb72f00: a row's greyed look (9)
// The renderer table (TAA byte +0xe9e4, FXAA +0xe9e5) and the renderer
// (variable resolution byte +0x7b3b0, 0x229bf30); the saved settings' file
// and section ({"app:\graphics_option.ini", "GraphicsOption"}).
constexpr std::uintptr_t table_global=0x51c4368,renderer_global=0x51c4480,ini_names=0x2f744b0;
// uGUIGraphicsOption, as its gated functions use it: 13 visible rows from
// +11008, 112 bytes each, with the item id at +8 and the row's GUI object at
// +40; the menu's values of the two rows; the list and its first row.
constexpr std::uintptr_t rows_offset=11008,row_size=112,row_item=8,row_object=40,
                         menu_rs=12520,menu_aa=12525,menu_list=10992,list_top=868;
constexpr unsigned rows=13,greyed_look=9;

std::uintptr_t image{};
std::string refusal;
AaApply aa_original{};RsApply rs_original{};AaSetter aa_setter{};Grey grey{};
RowEnabled row_original{};PageRefresh page_original{};RowBuild build_original{};RestoreDefaults defaults_original{};
std::array<std::uintptr_t,6> targets{};
std::atomic<bool> taken_now{};
// The player's choices: the saved ones at install, then every value the game
// applies (its menu, presets, start-up), except restore defaults while taken.
std::atomic<int> player_aa{-1},player_rs{-1};
int saved_aa{-1},saved_rs{-1};
std::atomic<std::uint64_t> aa_applies{},aa_forced{},rs_applies{},rs_forced{},rows_locked{},rows_greyed{},helps{},
    defaults_kept{},take_overs{},give_backs{},call_faults{};
thread_local bool restoring_defaults{};

bool write_byte(std::uintptr_t at,unsigned char value) noexcept {
    __try{*reinterpret_cast<volatile unsigned char *>(at)=value;return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Worker calls into the game, as its graphics menu makes them.
bool set_aa(unsigned char value) noexcept {
    std::uintptr_t table{};
    if(!pb::read(image+table_global,table)||!table)return false;
    __try{aa_setter(table,static_cast<char>(value));return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool apply_rs(unsigned char value) noexcept {
    __try{rs_original(static_cast<char>(value));return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
int saved(std::uintptr_t rva,unsigned char values) noexcept {
    char value{};
    __try{value=reinterpret_cast<IniRead>(image+rva)(image+ini_names);}
    __except(EXCEPTION_EXECUTE_HANDLER){return -1;}
    return static_cast<unsigned char>(value)<values?static_cast<unsigned char>(value):-1;
}

std::uintptr_t aa_apply(unsigned char value){
    ++aa_applies;
    if(value<anti_aliasing_values&&!restoring_defaults)player_aa=value;
    if(taken_now.load()){++aa_forced;return aa_original(anti_aliasing_off);}
    return aa_original(value);
}
__m128 rs_apply(char value){
    ++rs_applies;
    const auto v=static_cast<unsigned char>(value);
    if(v<resolution_scaling_values&&!restoring_defaults)player_rs=v;
    if(taken_now.load()){++rs_forced;return rs_original(static_cast<char>(resolution_scaling_high));}
    return rs_original(value);
}
std::uint64_t row_enabled(int mode,unsigned char item){
    if(taken_now.load()&&locked_row(item)){++rows_locked;return 0;}
    return row_original(mode,item);
}
std::uintptr_t page_refresh(std::uintptr_t menu,int top){
    const auto result=page_original(menu,top);
    // The page's own pass greys the rows the game's DLSS locks; the same
    // rows, after it.
    if(taken_now.load())for(unsigned i=0;i<rows;++i){
        const auto row=menu+rows_offset+row_size*i;unsigned char item{};std::uintptr_t object{};
        if(!pb::read(row+row_item,item)||!locked_row(item)||!pb::read(row+row_object,object)||!object)continue;
        grey(object,greyed_look);++rows_greyed;
    }
    return result;
}
std::uintptr_t row_build(std::uintptr_t row,char item,unsigned label,int help,std::uintptr_t values,unsigned value,unsigned count,
                         std::uintptr_t callback){
    // The game's own help for the row while NVIDIA DLSS is on.
    if(taken_now.load()&&locked_row(static_cast<unsigned char>(item))){++helps;help=locked_help(static_cast<unsigned char>(item),help);}
    return build_original(row,item,label,help,values,value,count,callback);
}
std::uintptr_t restore_defaults(std::uintptr_t menu){
    if(!taken_now.load())return defaults_original(menu);
    // As for the game's DLSS: the locked rows keep the player's values, and
    // what the defaults apply meanwhile is not the player's choice.
    unsigned char rs{},aa{};
    const bool read=pb::read(menu+menu_rs,rs)&&pb::read(menu+menu_aa,aa);
    restoring_defaults=true;
    const auto result=defaults_original(menu);
    restoring_defaults=false;
    std::uintptr_t list{};unsigned top{};
    if(read&&write_byte(menu+menu_rs,rs)&&write_byte(menu+menu_aa,aa)&&pb::read(menu+menu_list,list)&&list&&pb::read(list+list_top,top)){
        ++defaults_kept;page_refresh(menu,static_cast<int>(top));
    }
    return result;
}
}

bool install(std::uintptr_t host,std::string &reason){
    if(image){reason="game options already installed";return false;}
    struct Gate {unsigned rva,bytes;const char *sha;};
    constexpr Gate gates[]{
        {0x43dcb0,354,"4f4ccf7d6e736233fc0c47ce082ab05593116b183ec2884537633b7ba8c6afdd"},
        {0x43eeb0,332,"d2dafa94a77e203e1156c03bd2dd8564450d86804d5644218d51e20949fa0b8f"},
        {0x2287a00,66,"fd224da012d8cd5cd2175b7f67769b5cf6b5b532bfb0b9dfcf43bcd070f38494"},
        // TAA2's and FXAA's per-frame enable from the table bytes (vtable slot 10).
        {0x2393260,247,"a0f23cc4334b1e0e03b98c99894004f23cc0e6ef82d469ace598789081fc9e46"},
        {0x2394140,309,"0ba352e5c5af5fb35e71744e267bdac8bec9d92de9d9f49ddbb1830cf46cc18c"},
        {0x4413e0,169,"24516d839a3428bd0b27df3f98c9edff5b9349564ba4c2eabb0f0587a50a6327"},
        {0x442120,185,"d9e6710d5a91105f22a09054f9e35e006c8cfe6a60c8b6ff79c2b92325643068"},
        {0x53f580,124,"844b65e650c6b9c72ebd7d6f96a1bbf532dd40c38632757d7da3dbdda9486835"},
        {0x53f440,316,"8299ed6141b50713d145dd033628c787c4300965d409e3225ea91ddb95735306"},
        {0x53f190,680,"14444dd15728755ab84854f7b91b32bff3ae0101f959021e5b3ff06658785a4b"},
        {0x540720,824,"6557fffa6347d6e98d060f7a25605c018ee473e301a8acd15028952a36c637d6"},
        {0xb72f00,17,"99fb83c8c457ec4bab9bf6273293cc99dabd28e9be4c289e92c54b57426538f4"}};
    for(const auto &g:gates)if(!pb::verify_code(host,g.rva,g.bytes,g.sha)){reason="game options code gate "+std::to_string(g.rva);refusal=reason;return false;}
    image=host;
    aa_setter=reinterpret_cast<AaSetter>(host+0x2287a00);grey=reinterpret_cast<Grey>(host+0xb72f00);
    saved_aa=saved(0x4413e0,anti_aliasing_values);saved_rs=saved(0x442120,resolution_scaling_values);
    player_aa=saved_aa;player_rs=saved_rs;
    constexpr std::array<unsigned,6> rvas{0x43dcb0,0x43eeb0,0x53f580,0x53f440,0x53f190,0x540720};
    const std::array<void *,6> detours{reinterpret_cast<void *>(aa_apply),reinterpret_cast<void *>(rs_apply),
        reinterpret_cast<void *>(row_enabled),reinterpret_cast<void *>(page_refresh),reinterpret_cast<void *>(row_build),
        reinterpret_cast<void *>(restore_defaults)};
    std::array<void *,6> originals{};
    unsigned created=0;
    for(;created<rvas.size();++created){
        targets[created]=host+rvas[created];
        if(MH_CreateHook(reinterpret_cast<void *>(targets[created]),detours[created],&originals[created])!=MH_OK)break;
    }
    if(created==rvas.size()){
        aa_original=reinterpret_cast<AaApply>(originals[0]);rs_original=reinterpret_cast<RsApply>(originals[1]);
        row_original=reinterpret_cast<RowEnabled>(originals[2]);page_original=reinterpret_cast<PageRefresh>(originals[3]);
        build_original=reinterpret_cast<RowBuild>(originals[4]);defaults_original=reinterpret_cast<RestoreDefaults>(originals[5]);
        // Pass-through until take_over(): enabled now, apart from the
        // runtime's queued hooks.
        unsigned enabled=0;
        for(;enabled<rvas.size();++enabled)if(MH_EnableHook(reinterpret_cast<void *>(targets[enabled]))!=MH_OK)break;
        if(enabled==rvas.size())return true;
        for(unsigned i=0;i<enabled;++i)MH_DisableHook(reinterpret_cast<void *>(targets[i]));
    }
    for(unsigned i=0;i<created;++i)MH_RemoveHook(reinterpret_cast<void *>(targets[i]));
    image=0;reason="game options hooks";refusal=reason;return false;
}
void take_over() noexcept {
    if(!image||taken_now.exchange(true))return;
    ++take_overs;
    // The units follow the table bytes on their next update (0x2393260,
    // 0x2394140); the game's apply also walks the unit list, which stays on
    // the game's own thread.
    if(!set_aa(anti_aliasing_off))++call_faults;
    if(!apply_rs(resolution_scaling_high))++call_faults;
}
void give_back() noexcept {
    if(!image||!taken_now.exchange(false))return;
    ++give_backs;
    const int aa=player_aa.load(),rs=player_rs.load();
    if(aa>=0&&!set_aa(static_cast<unsigned char>(aa)))++call_faults;
    if(rs>=0&&!apply_rs(static_cast<unsigned char>(rs)))++call_faults;
}
bool taken() noexcept {return taken_now.load();}
void write(std::ostream &out){
    std::uintptr_t table{},renderer{};unsigned char taa{},fxaa{},variable{};float scale{};
    if(image&&pb::read(image+table_global,table)&&table){pb::read(table+0xe9e4,taa);pb::read(table+0xe9e5,fxaa);}
    if(image&&pb::read(image+renderer_global,renderer)&&renderer){pb::read(renderer+0x7b3b0,variable);pb::read(renderer+0x1f0,scale);}
    // The game's own DLSS 1 (0x23842a0 at cD3DRender::initForD3D12): its NGX
    // context at backend +0x42700, or why it has none (0x51c6c28: 1 NGX
    // init, 2 SuperSampling, 3 ImageSignalProcessing unavailable), and this
    // mod's decline of it (ngx_startup::game_dlss(): 2 none made, 3 declined).
    std::uintptr_t backend{},context{};unsigned game_ngx{};
    if(image&&pb::read(image+0x51c7050,backend)&&backend)pb::read(backend+0x42700,context);
    if(image)pb::read(image+0x51c6c28,game_ngx);
    out<<"{\"installed\":"<<(image!=0)<<",\"refusal\":\""<<refusal<<"\",\"taken\":"<<taken_now.load()
       <<",\"saved\":["<<saved_aa<<','<<saved_rs<<"],\"player\":["<<player_aa.load()<<','<<player_rs.load()
       <<"],\"table\":["<<unsigned(taa)<<','<<unsigned(fxaa)<<"],\"variable\":"<<unsigned(variable)<<",\"scale\":"<<scale
       <<",\"take_overs\":"<<take_overs.load()<<",\"give_backs\":"<<give_backs.load()<<",\"call_faults\":"<<call_faults.load()
       <<",\"aa\":["<<aa_applies.load()<<','<<aa_forced.load()<<"],\"rs\":["<<rs_applies.load()<<','<<rs_forced.load()
       <<"],\"rows\":{\"locked\":"<<rows_locked.load()<<",\"greyed\":"<<rows_greyed.load()<<",\"helps\":"<<helps.load()
       <<",\"defaults_kept\":"<<defaults_kept.load()<<"},\"game_dlss\":{\"context\":"<<(context!=0)<<",\"refusal\":"<<game_ngx
       <<",\"declined\":"<<ngx_startup::game_dlss()<<"}}";
}
}
