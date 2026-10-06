#include "../src/scaled_temporal/overlay_extent.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace sf=mhw::scaled_temporal;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
int main(){try{
    // Deliberately not the old packed layout: UV first, unused color, XYZ last.
    const std::array<D3D12_INPUT_ELEMENT_DESC,3> elements{{
        {"TexCoord",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
    constexpr std::array<float,16> expected{-1,1,0,0,-1,-1,0,1,1,1,1,0,1,-1,1,1};
    sf::OverlayVertexLayout layout;std::array<float,16> decoded{};
    require(sf::overlay_vertex_layout(elements,28,layout),"28-byte native layout");
    std::array<std::byte,112> raw;raw.fill(std::byte{0xff}); // ignored Z/padding include NaNs
    for(unsigned i=0;i<4;++i){std::memcpy(raw.data()+28*i+16,expected.data()+4*i,8);std::memcpy(raw.data()+28*i,expected.data()+4*i+2,8);}
    require(sf::overlay_vertices(raw,layout,decoded)&&decoded==expected,"decode actual offsets; ignored attributes untouched");
    require(!sf::overlay_vertices(std::span(raw).first(111),layout,decoded),"truncated vertex data");
    float bad=.5F;std::memcpy(raw.data()+16,&bad,4);require(!sf::overlay_vertices(raw,layout,decoded),"partial coverage rejected");
    bad=std::numeric_limits<float>::quiet_NaN();std::memcpy(raw.data(),&bad,4);require(!sf::overlay_vertices(raw,layout,decoded),"non-finite used input rejected");
    auto negative=[&](auto edit){auto e=elements;edit(e);sf::OverlayVertexLayout out{77};require(!sf::overlay_vertex_layout(e,28,out)&&out.stride==0,"invalid layout rejected without output");};
    negative([](auto &e){e[2].InputSlot=1;});negative([](auto &e){e[0].InputSlotClass=D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;});
    negative([](auto &e){e[0].InstanceDataStepRate=1;});negative([](auto &e){e[2].Format=DXGI_FORMAT_R32G32_UINT;});
    negative([](auto &e){e[2].AlignedByteOffset=20;});negative([](auto &e){e[2].AlignedByteOffset=4;});
    negative([](auto &e){e[0].SemanticIndex=1;});negative([](auto &e){e[1]=e[0];});
    negative([](auto &e){e[2].AlignedByteOffset=17;});negative([](auto &e){e[2].SemanticName=nullptr;});
    auto append=elements;append[1].AlignedByteOffset=D3D12_APPEND_ALIGNED_ELEMENT;append[2].AlignedByteOffset=D3D12_APPEND_ALIGNED_ELEMENT;
    require(sf::overlay_vertex_layout(append,28,layout)&&layout.position==12,"append offsets resolved from actual preceding elements");
    append[1].Format=DXGI_FORMAT_UNKNOWN;require(!sf::overlay_vertex_layout(append,28,layout),"unknown append stride refused");
    for(unsigned stride:{0U,8U,256U,UINT_MAX})require(!sf::overlay_vertex_layout(elements,stride,layout),"stride bound");
    const std::array<D3D12_INPUT_ELEMENT_DESC,2> packed{{
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
    require(sf::overlay_vertex_layout(packed,16,layout)&&sf::overlay_vertices(std::as_bytes(std::span(expected)),layout,decoded)&&decoded==expected,"packed layout regression");
    std::cout<<"PASS native overlay layout decoding and bounds\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
