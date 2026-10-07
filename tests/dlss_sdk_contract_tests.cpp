#include "../src/dlss_sdk/contract.hpp"
#include "../src/dlss_sdk/history.hpp"
#include "../src/dlss_sdk/geometry.hpp"
#include <vector>
#include <limits>
#include <iostream>
int main(){
    using mhw::dlss_sdk::Frame;using mhw::dlss_sdk::validate;
    std::vector<float> color(32*32*4,1),depth(32*32,.25F),motion(32*32*2,0);std::string why;
    Frame f{32,32,color,depth,motion};if(!validate(f,why))return 1;
    const auto reject=[&](){return !validate(f,why) && !why.empty();};
    f.width=31;if(!reject())return 2;f.width=32;
    f.motion=std::span(motion).first(motion.size()-1);if(!reject())return 3;f.motion=motion;
    color[0]=std::numeric_limits<float>::quiet_NaN();if(!reject())return 4;color[0]=1;
    depth[3]=-1;if(!reject())return 5;depth[3]=.25F;
    motion[8]=std::numeric_limits<float>::infinity();if(!reject())return 6;motion[8]=0;
    color[3]=0;if(!reject())return 7;color[3]=1;
    if(!validate(f,why) || !why.empty())return 8;
    using mhw::dlss_sdk::HistoryPlan;using mhw::dlss_sdk::valid_history;
    const HistoryPlan good{{{100,900,0,0},{101,901,.1875F,.21875F},{102,902,0,0}}};
    if(!valid_history(good))return 9;
    for(unsigned i=0;i<3;++i){
        auto bad=good;bad[i].frame+=1;if(valid_history(bad))return 10;
        bad=good;bad[i].epoch+=1;if(valid_history(bad))return 11;
        for(float value:{.5001F,-.5001F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
            bad=good;bad[i].jitter_x=value;if(valid_history(bad))return 12;
            bad=good;bad[i].jitter_y=value;if(valid_history(bad))return 13;
        }
    }
    auto wrap=good;wrap[0].frame=UINT64_MAX;wrap[1].frame=0;wrap[2].frame=1;if(valid_history(wrap))return 14;
    wrap=good;wrap[0].epoch=UINT64_MAX;wrap[1].epoch=0;wrap[2].epoch=1;if(valid_history(wrap))return 15;
    std::array<mhw::dlss_sdk::HistoryFrame,10> longer;
    for(unsigned i=0;i<longer.size();++i)longer[i]={100+i,900+i,0,0};
    if(!valid_history(std::span(longer).first(9)))return 16;
    for(unsigned n:{0U,1U,2U,4U,8U,10U})if(valid_history(std::span(longer).first(n)))return 17;
    for(unsigned i=0;i<9;++i){auto bad=longer;++bad[i].epoch;if(valid_history(std::span(bad).first(9)))return 18;}
    using namespace mhw::dlss_sdk;
    const Geometry upscale{{2560,1440},{1280,720},{2560,1440},ReconstructionMode::performance};
    if(!valid_geometry(upscale)||!valid_geometry(Geometry::dlaa(2560,1440)))return 19;
    auto bad=upscale;bad.allocation={1279,720};if(valid_geometry(bad))return 20;
    bad=upscale;bad.render={1280,721};if(valid_geometry(bad))return 21;
    bad=upscale;bad.output={640,360};if(valid_geometry(bad))return 22;
    bad=upscale;bad.mode=ReconstructionMode::dlaa;if(valid_geometry(bad))return 23;
    bad=upscale;bad.render=bad.output;if(valid_geometry(bad))return 24;
    bad=upscale;bad.mode=static_cast<ReconstructionMode>(99);if(valid_geometry(bad))return 25;
    // 4K, 5120x1440 (32:9) and 5120x2880 fit the pixel ceiling; 8K does not.
    for(Extent e: {Extent{3840,2160},{5120,1440},{5120,2880}})if(!valid_extent(e))return 32;
    if(valid_extent({7680,4320})||valid_extent({4097,4096}))return 33;
    for(Extent e: {Extent{0,720},{31,720},{8193,32},{8192,8192},{UINT32_MAX,UINT32_MAX}}){
        bad=upscale;bad.allocation=e;if(valid_geometry(bad))return 26;
        bad=upscale;bad.render=e;if(valid_geometry(bad))return 27;
        bad=upscale;bad.output=e;if(valid_geometry(bad))return 28;
    }
    if(!optimal_matches(upscale,{1280,720},{640,360},{2560,1440}))return 29;
    if(optimal_matches(upscale,{1279,720},{640,360},{2560,1440})||
       optimal_matches(upscale,{1280,720},{1281,720},{2560,1440})||
       optimal_matches(upscale,{1280,720},{640,360},{1279,720})||
       optimal_matches(upscale,{1280,720},{640,360},{2561,1440})||
       optimal_matches(upscale,{1280,720},{0,0},{2560,1440}))return 30;
    PixelJitter mapped;
    if(!projection_jitter(upscale,{.375F,-.4375F},2560,1440,mapped)||mapped.x!=.09375F||mapped.y!=.109375F)return 31;
    if(!projection_jitter(upscale,{.375F,-.4375F},1280,720,mapped)||mapped.x!=.1875F||mapped.y!=.21875F)return 32;
    if(!projection_jitter(Geometry::dlaa(2560,1440),{.375F,-.4375F},2560,1440,mapped)||mapped.x!=.1875F||mapped.y!=.21875F)return 33;
    for(const auto e:{Extent{1280,1440},Extent{2560,720},Extent{0,0},Extent{640,360}})
        if(projection_jitter(upscale,{.375F,-.4375F},static_cast<float>(e.width),static_cast<float>(e.height),mapped))return 34;
    for(float v:{.5001F,-.5001F,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()})
        if(projection_jitter(upscale,{v,0},1280,720,mapped)||projection_jitter(upscale,{0,v},1280,720,mapped))return 35;
    if(projection_jitter(upscale,{0,0},std::numeric_limits<float>::quiet_NaN(),720,mapped))return 36;
    if(!projection_jitter(upscale,{0,0},1280,720,mapped)||mapped.x!=0||mapped.y!=0)return 37;
    std::cout<<"DLAA contracts, separate geometry and projection-basis jitter passed\n";
}
