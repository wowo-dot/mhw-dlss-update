#include "../src/scaled_temporal/color_dependencies.hpp"
#include "../src/scaled_temporal/color_chain.hpp"
#include <iostream>
#include <stdexcept>
using namespace mhw::scaled_temporal::color_dependencies;
void require(bool v){if(!v)throw std::runtime_error("dependency plan test");}
int main(){try{
 using mhw::scaled_temporal::ColorLink;using mhw::scaled_temporal::color_chain;
 std::array<ColorLink,3> links{{{0x78001000,10,20},{0x78002000,20,30},{0x78003000,30,40}}};
 std::array<unsigned,8> path{};unsigned count{};
 require(color_chain(links,10,40,path,count)&&count==3&&path[2]==2);
 require(color_chain(links,10,10,path,count)&&count==0);
 auto broken=links;broken[1].source=10;require(!color_chain(broken,10,40,path,count)&&!count);
 broken=links;broken[1].destination=10;require(!color_chain(broken,10,40,path,count));
 broken=links;broken[1].key=links[0].key;require(!color_chain(broken,10,40,path,count));
 broken=links;broken[2].destination=0;require(!color_chain(broken,10,40,path,count));
 require(!color_chain(links,10,50,path,count));
 std::array<ColorLink,9> chain{};for(unsigned i=0;i<9;++i)chain[i]={0x78000100+i,i+1ULL,i+2ULL};
 require(color_chain(std::span(chain).first(8),1,9,path,count)&&count==8);
 require(!color_chain(chain,1,10,path,count)&&count==0);
 const std::array<Resource,5> resources{{{10,Domain::scene},{20,Domain::unknown},{30,Domain::auxiliary},{40,Domain::auxiliary},{50,Domain::unknown}}};
 std::array<Draw,2> draws{{{20,{10},1},{10,{20,30,40},3}}};Plan p;
 require(build(resources,draws,10,p)&&p.count==2);
 require(p.steps[0].inputs[0].producer==-1&&p.steps[1].inputs[0].producer==0);
 require(p.steps[1].inputs[0].domain==Domain::scene&&p.steps[1].inputs[1].domain==Domain::auxiliary&&p.steps[1].inputs[2].domain==Domain::auxiliary);
 const auto rejected=[&](auto r,auto d,std::uint64_t final=10){p.count=99;require(!build(r,d,final,p)&&!p.count);};
 auto bad=draws;std::swap(bad[0],bad[1]);rejected(resources,bad);
 bad=draws;bad[0].reads[0]=20;rejected(resources,bad);
 bad=draws;bad[0].output=30;rejected(resources,bad);
 bad=draws;bad[1].reads[0]=50;rejected(resources,bad);
 bad=draws;bad[1].reads[0]=999;rejected(resources,bad);
 bad=draws;bad[1].output=999;rejected(resources,bad);
 bad=draws;bad[1].count=9;rejected(resources,bad);
 bad=draws;bad[0].count=0;rejected(resources,bad);
 bad=draws;bad[0].reads[0]=30;rejected(resources,bad);
 auto rr=resources;rr[1].id=10;rejected(rr,draws);
 rr=resources;rr[0].id=0;rejected(rr,draws);
 rr=resources;rr[0].domain=Domain::unknown;rejected(rr,draws);
 rejected(resources,draws,20);
 const std::array<Draw,4> pingpong{{{20,{10},1},{10,{20,30},2},{20,{10},1},{10,{20,40},2}}};
 require(build(resources,pingpong,10,p)&&p.steps[2].inputs[0].producer==1&&p.steps[3].inputs[0].producer==2);
 std::array<Draw,17> too_many{};rejected(resources,too_many);
 std::array<Resource,33> too_many_resources{};rejected(too_many_resources,draws);
 std::cout<<"PASS dependency versions, auxiliary domains, ordering, aliases and rejection rollback\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
