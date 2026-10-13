#include "../src/scaled_temporal/dof_native_sequence.hpp"
#include <cstdlib>
#include <iostream>
namespace sf=mhw::scaled_temporal;
void check(bool b,const char *why){if(!b){std::cerr<<why<<'\n';std::exit(1);}}
using Sizes=std::array<std::array<unsigned,2>,9>;
using Groups=std::array<std::array<unsigned,3>,6>;
int main(){
 // Captured at 2560x1440 (dof-compute-bundle-result.md).
 constexpr auto captured=sf::dof_layout(2560,1440);
 static_assert(captured.sizes==Sizes{{{2560,1440},{2560,1440},{128,1440},{128,72},{128,72},{1280,720},{1280,720},{1280,720},{1280,720}}});
 static_assert(captured.groups==Groups{{{8,1440,1},{128,5,1},{16,9,1},{80,45,1},{80,45,1},{80,45,1}}});
 // 1920x1080: 96x54 tiles, 960x540 half size; 540/16 and 1080/320 round up.
 constexpr auto hd=sf::dof_layout(1920,1080);
 static_assert(hd.sizes[2]==std::array<unsigned,2>{96,1080}&&hd.sizes[3]==std::array<unsigned,2>{96,54}&&hd.sizes[8]==std::array<unsigned,2>{960,540});
 static_assert(hd.groups==Groups{{{6,1080,1},{96,4,1},{12,7,1},{60,34,1},{60,34,1},{60,34,1}}});
 // Pillarboxed 21:9 in a 5120x1440 window: an odd width takes the truncated
 // half (unconfirmed in game; the plan records the actual sizes).
 static_assert(sf::dof_layout(3413,1440).sizes[2]==std::array<unsigned,2>{171,1440}&&sf::dof_layout(3413,1440).sizes[5]==std::array<unsigned,2>{1706,720});
 // 3840x2160 and 1600x900 (odd half height).
 static_assert(sf::dof_layout(3840,2160).groups[0]==std::array<unsigned,3>{12,2160,1}&&sf::dof_layout(3840,2160).sizes[4]==std::array<unsigned,2>{192,108});
 static_assert(sf::dof_layout(1600,900).sizes[5]==std::array<unsigned,2>{800,450}&&sf::dof_layout(1600,900).groups[3]==std::array<unsigned,3>{50,29,1});
 // A partial tile rounds up.
 static_assert(sf::dof_layout(1024,768).sizes[3]==std::array<unsigned,2>{52,39});
 // TYPELESS family members are accepted (1080p CoC: R16G16B16A16_TYPELESS).
 static_assert(sf::dof_format_matches(10,10)&&sf::dof_format_matches(9,10)&&sf::dof_format_matches(39,41)&&sf::dof_format_matches(39,42)&&sf::dof_format_matches(60,61));
 static_assert(!sf::dof_format_matches(9,26)&&!sf::dof_format_matches(11,10)&&!sf::dof_format_matches(40,41)&&sf::dof_format_matches(26,26)&&!sf::dof_format_matches(0,26));
 // CBNewDOFFilter far/near flags and coefficients (words 11-14): gameplay's
 // 1,1,1.0,1.0, either side off, no blur, other scales; not other booleans,
 // non-finite or negative coefficients.
 constexpr unsigned one=0x3f800000U,half=0x3f000000U,big=0x40200000U;
 static_assert(sf::dof_blur_supported({1,1,one,one})&&sf::dof_blur_supported({1,0,one,one})&&sf::dof_blur_supported({0,1,one,one})&&
  sf::dof_blur_supported({0,0,one,one})&&sf::dof_blur_supported({1,1,big,half})&&sf::dof_blur_supported({1,1,0,0}));
 static_assert(!sf::dof_blur_supported({2,1,one,one})&&!sf::dof_blur_supported({1,0xffffffffU,one,one})&&
  !sf::dof_blur_supported({1,1,0x7f800000U,one})&&!sf::dof_blur_supported({1,1,one,0x7fc00000U})&&!sf::dof_blur_supported({1,1,0xbf800000U,one})&&
  !sf::dof_blur_supported({1,1,one,0x80000000U}));
 const float third=1.F/3;std::int32_t bits{};std::memcpy(&bits,&third,4);
 float next{},far{};const std::int32_t n=bits+1,f=bits+2;std::memcpy(&next,&n,4);std::memcpy(&far,&f,4);
 check(sf::dof_near(third,third)&&sf::dof_near(third,next)&&sf::dof_near(next,third)&&!sf::dof_near(third,far),"one-ULP reciprocal tolerance");
 check(sf::dof_near(1.F/1920,static_cast<float>(1.0/1920)),"float and double reciprocals agree");
 std::cout<<"dof layout tests passed\n";
}
