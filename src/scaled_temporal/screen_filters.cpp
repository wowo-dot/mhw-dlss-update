#include "screen_filters.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <stdexcept>
#include <string>

namespace mhw::scaled_temporal {
namespace {
// The filter vertex shader shared by depth color, motion and color correction.
constexpr std::string_view filter_vs="abddc634c053c157f295842964b023610ff64a7505d4b253edbe1612680ea937";
// Word bits: 0-3 kind (0 CB, 1 sampler, 2 SRV), 4-11 owner index, 12-23 global
// binding ID, 24-31 register. tBaseMap is global ID 193 in every entry.
constexpr std::array<ScreenFilter,6> filters{{
    {"DepthColor","c0944737c37bec2c5f24759b70bd14b16e89dbb3870aeab662dd7a611cc1f9c1",5568,filter_vs,752,0,
        {663553,790546,17059874,45104,16826432,54276176},6,2,1,0},
    {"ColorCorrect","c82c33171b6336e7a01361bb9ad0133423f2b7f4acc12fd8cdef8f880177e493",3808,filter_vs,752,0,
        {1060865,17440785,34258977,790578,17985602,36241490,16826464,53022832},8,1,1,0},
    {"ColorCorrectCubeSimple","4a65b6c4ebb078c887e7eeb8086557434ff1e53d66ffc7d3e558c8557364c4f9",1232,filter_vs,752,0,
        {663553,790546,19443746},3,1,0,1},
    {"ColorCorrectCubeFixed","0cbdb031296afd4824ed2cf275f6d495a3ca7f9701f0623c469abd37e7ae2255",1684,filter_vs,752,0,
        {663553,790546,19443746,36225074,53006400},5,1,0,1},
    {"ColorCorrectCubeTable","bc5544b68c102e253aac1bad2c71776f1d18484c04adf1539e9c9967145f4a0c",4080,filter_vs,752,0,
        {1064961,17440785,790562,17059890,36220994,53002322,70869090,45168,51126400,69783696},10,1,0,1},
    {"ColorCorrectCubeSophia","29525bf1e2138e1df13a601f4a77c5210d7b89a2c8923c55d2faaefa9dc33384",4124,filter_vs,752,0,
        {1064961,17440785,790562,17059890,36220994,53002322,45152,51126384,69783680},9,1,0,1},
}};
std::string sha256(std::span<const std::byte> bytes){
    std::array<unsigned char,32> hash{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<std::byte *>(bytes.data())),
        static_cast<ULONG>(bytes.size()),hash.data(),static_cast<ULONG>(hash.size()))<0)throw std::runtime_error("screen filter hash");
    constexpr char hex[]="0123456789abcdef";std::string s;for(auto b:hash){s+=hex[b>>4];s+=hex[b&15];}return s;
}
}
std::span<const ScreenFilter> screen_filters() noexcept{return filters;}
const ScreenFilter *screen_filter(std::string_view ps) noexcept{
    for(const auto &f:filters)if(f.ps_sha256==ps)return &f;
    return nullptr;
}
const ScreenFilter *screen_filter(std::span<const std::byte> ps,std::span<const std::byte> vs){
    const auto *f=screen_filter(sha256(ps));
    return f&&ps.size()==f->ps_size&&vs.size()==f->vs_size&&sha256(vs)==f->vs_sha256?f:nullptr;
}
unsigned screen_filter_textures(const ScreenFilter &f) noexcept{
    unsigned mask{};for(unsigned i=0;i<f.word_count;++i)if((f.words[i]&15)==2)mask|=1U<<(f.words[i]>>24);
    return mask;
}
std::vector<std::byte> make_screen_filter_shader(std::span<const std::byte> native,FilterGeometry g){
    const auto *f=screen_filter(sha256(native));
    if(!f||native.size()!=f->ps_size)throw std::runtime_error("unapproved screen filter shader");
    ScreenShaderEdits edits;auto adapted=make_screen_shader(native,g,f->scene_register,edits);
    if(edits.position_reads!=f->position_reads||edits.scene_samples!=f->scene_samples||edits.scene_loads!=f->scene_loads)
        throw std::runtime_error("screen filter edits differ from the approved recipe");
    return adapted;
}
}
