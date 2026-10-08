#include "../src/scaled_temporal/effects.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace sf=mhw::scaled_temporal;
void check(bool v,const char *why){if(!v)throw std::runtime_error(why);}
template<class F>void rejects(F f){bool rejected=false;try{f();}catch(const std::exception &){rejected=true;}check(rejected,"expected invalid geometry");}
int main(int argc,char **argv){try{
    if(argc==2){
        std::ifstream input(argv[1]);check(bool(input),"capture geometry input");unsigned rows{},stage{},w{},h{},ow{},oh{},count{},topology{};
        while(input>>stage){
            check(bool(input>>w>>h>>ow>>oh>>count>>topology),"capture geometry header");
            check(stage<4&&++rows<=64,"capture geometry bounds");
            const auto actual=sf::effect_vertices(static_cast<sf::EffectStage>(stage),{w,h,ow,oh},false);
            check(count==actual.count,"capture primitive count");
            check(topology==static_cast<unsigned>(actual.topology),"production topology differs from captured native draw");
            for(unsigned i=0;i<count*4;++i){double expected{};check(bool(input>>expected)&&std::isfinite(expected),"capture vertex value");
                check(std::abs(actual.values[i]-expected)<2e-6,"production vertices differ from captured native order/UV");}
        }
        check(input.eof()&&rows>0,"complete capture geometry file");
        std::cout<<"PASS: "<<rows<<" captured native vertex arrays and topologies match production geometry\n";return 0;
    }
    check(argc==1,"usage: scaled_effect_vertices_tests [capture-geometry.txt]");
    unsigned cases{};
    for(auto g:std::array<sf::FilterGeometry,4>{{{1280,720,2560,1440},{64,32,128,64},{2560,1440,2560,1440},{65,33,130,66}}}){
        for(auto stage:{sf::EffectStage::motion_filter,sf::EffectStage::color_grade,sf::EffectStage::final_screen,sf::EffectStage::haze})for(bool full:{false,true}){
            const auto v=sf::effect_vertices(stage,g,full);const bool haze=stage==sf::EffectStage::haze;const bool triangle=stage==sf::EffectStage::final_screen||haze;
            check(v.count==(triangle?3U:4U),"primitive count");check(v.topology==D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,"native topology");
            for(unsigned i=0;i<4*v.count;++i)check(std::isfinite(v.values[i]),"finite vertices");
            const double x=double(full?g.output_width:g.render_width)/g.output_width,y=double(full?g.output_height:g.render_height)/g.output_height;
            if(haze){
                check(v.values[0]==-1&&v.values[1]==1&&v.values[4]==-1&&v.values[5]==-3&&v.values[8]==3&&v.values[9]==1,"haze triangle");
                check(std::abs(v.values[10]*.5-double(g.render_width)/g.output_width)<1e-7&&std::abs(v.values[7]*.5-double(g.render_height)/g.output_height)<1e-7,"haze native UV footprint retained");
            }else if(triangle){
                check(v.values[0]==-1&&v.values[1]==1&&v.values[4]==-1&&v.values[5]==-3&&v.values[8]==3&&v.values[9]==1,"covering triangle");
                // Interpolate at the lower-right clip corner, halfway along
                // each long edge: the native sampling limit is extent minus
                // half a texel, not a second stretch of the half-size image.
                check(std::abs(double(v.values[10])*.5-(x-.5/g.output_width))<1e-7,"final source x half texel");
                check(std::abs(double(v.values[7])*.5-(y-.5/g.output_height))<1e-7,"final source y half texel");
            }else if(stage==sf::EffectStage::color_grade){
                check(v.values[0]==-1&&v.values[1]==1&&v.values[4]==1&&v.values[5]==1&&v.values[8]==-1&&v.values[9]==-1&&v.values[12]==1&&v.values[13]==-1,"native color-grade strip order");
                check(std::abs(v.values[6]-x)<1e-7&&std::abs(v.values[14]-x)<1e-7&&std::abs(v.values[11]-y)<1e-7&&std::abs(v.values[15]-y)<1e-7,"color-grade UV convention");
            }else{
                check(v.values[0]==-1&&v.values[1]==1&&v.values[4]==-1&&v.values[5]==-1&&v.values[8]==1&&v.values[9]==1&&v.values[12]==1&&v.values[13]==-1,"full output quad");
                const double u=stage==sf::EffectStage::motion_filter?1:x,t=stage==sf::EffectStage::motion_filter?1:y;
                check(std::abs(v.values[10]-u)<1e-7&&std::abs(v.values[14]-u)<1e-7&&std::abs(v.values[7]-t)<1e-7&&std::abs(v.values[15]-t)<1e-7,"UV convention");
            }++cases;
        }
    }
    for(const auto bad:std::array<sf::FilterGeometry,7>{{{0,720,2560,1440},{1280,0,2560,1440},{1280,720,1920,1080},{1280,720,2560,720},{2560,1440,1280,720},{UINT_MAX,720,2560,1440},{17,9,34,18}}})
        for(auto stage:{sf::EffectStage::motion_filter,sf::EffectStage::color_grade,sf::EffectStage::final_screen,sf::EffectStage::haze})rejects([&]{sf::effect_vertices(stage,bad,true);});
    rejects([]{sf::effect_vertices(static_cast<sf::EffectStage>(8),{64,32,128,64},true);});
    // uDepthColorFilter (game-41 capture) draws haze's native triangle and keeps
    // its render-space UVs at full output; only its color sample expands.
    for(auto g:std::array<sf::FilterGeometry,3>{{{1280,720,2560,1440},{64,32,128,64},{1706,720,3413,1440}}})for(bool full:{false,true}){
        const auto d=sf::effect_vertices(sf::EffectStage::screen_filter,g,full),h=sf::effect_vertices(sf::EffectStage::haze,g,false);
        check(d.count==3&&d.topology==h.topology&&d.values==h.values,"screen filters use the native haze triangle");++cases;}
    {const auto d=sf::effect_vertices(sf::EffectStage::screen_filter,{1280,720,2560,1440},false);
     const std::array<float,12> captured{-1,1,0,0,-1,-3,0,1,3,1,1,0};
     for(unsigned i=0;i<12;++i)check(d.values[i]==captured[i],"screen filter (depth color) matches the captured native vertices");}
    // A pillarboxed odd scene renders at the truncated half (game-20).
    check(sf::valid_filter_geometry({1706,720,3413,1440})&&!sf::valid_filter_geometry({1707,720,3413,1440})&&!sf::valid_filter_geometry({1705,720,3413,1440}),"truncated half");
    std::cout<<"PASS: "<<cases<<" geometry cases, 29 rejected configurations\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
