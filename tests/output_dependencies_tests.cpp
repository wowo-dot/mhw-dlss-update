#include "../src/scaled_temporal/output_dependencies.hpp"
#include "../src/scaled_temporal/output_diagnostics.hpp"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
using Capture=mhw::scaled_temporal::OutputDependencies;
void check(bool b,const char *why){if(!b){std::cerr<<why<<'\n';std::exit(1);}}
struct Memory {
 std::vector<std::byte> bytes=std::vector<std::byte>(1048576);std::uint64_t next=0x5000;unsigned reads{};
 std::uint64_t take(unsigned n){auto p=next;next+=(n+15)&~15U;return p;}
 template<class T>void put(std::uint64_t at,const T &v){check(at+sizeof(v)<=bytes.size(),"fixture bounds");std::memcpy(bytes.data()+at,&v,sizeof(v));}
 bool read(std::uint64_t at,void *out,std::size_t n){++reads;if(at>bytes.size()||n>bytes.size()-at)return false;std::memcpy(out,bytes.data()+at,n);return true;}
};
int main(){
 Memory m;const std::uint64_t context=0x1000,queue=m.take(128),filter=m.take(32),arena=m.take(512);
 std::array<std::uint64_t,2> objects{},backends{},sets{},shader_descriptors{};
 for(unsigned i=0;i<2;++i){objects[i]=m.take(0x100);backends[i]=m.take(0x40);m.put(objects[i]+0x60,backends[i]);
  m.put(objects[i]+0x88,2560U+(1440U<<16));m.put(objects[i]+0x90,34U);m.put(backends[i]+0x20,std::uint64_t{0xa000+8*i});}
 // Draw 0 writes auxiliary resource A008; draw 1 samples it and writes scene A000.
 for(unsigned i=0;i<2;++i){const auto state=m.take(16),packet=m.take(0x48),attachment=m.take(0x60),view=m.take(0x80),backend=m.take(0x40),
    ia=m.take(0x30),graphics=m.take(0x50),handle=m.take(8),set=m.take(0x90),ids=m.take(8),modes=m.take(8),words=m.take(8),layout=m.take(0x20);
  sets[i]=set;m.put(queue+32*i,0x78001004U);m.put(queue+32*i+8,state);m.put(state,11U);
  m.put(queue+32*i+16,0x78001004U);m.put(queue+32*i+24,packet);
  m.put(packet+0x10,attachment);m.put(packet+0x18,ia);m.put(packet+0x20,graphics);m.put(packet+0x40,4U);m.put(packet+0x44,i?0U:12U);
  m.put(attachment,view);m.put(attachment+0x48,1U);m.put(attachment+0x4c,std::array<int,4>{0,0,1280,720});
  m.put(view+0x60,backend);m.put(backend+0x18,i?0xa000ULL:0xa008ULL);m.put(backend+0x20,i?26U:24U);
  m.put(ia+0x20,static_cast<unsigned char>(16));m.put(graphics+8,handle);m.put(handle,set);m.put(graphics+0x20,layout);
  m.put(set+0x88,1U);m.put(set+0x80,ids);m.put(graphics+0x10,modes);m.put(graphics+0x30,objects[i]);
  m.put(set,words);m.put(set+0x14,std::array<unsigned short,2>{1,0});m.put(words,2U);
  for(unsigned stage=0;stage<2;++stage){const auto entry=m.take(0x20),desc=m.take(16),data=m.take(52);
   m.put(set+0x50+8*stage,entry);m.put(entry+0x10,desc);m.put(desc,52U);m.put(desc+8,data);if(stage)shader_descriptors[i]=desc;
   m.put(data,0x43425844U);m.put(data+20,1U);m.put(data+24,52U);m.put(data+28,1U);m.put(data+32,36U);
   m.put(data+36,0x52444853U);m.put(data+40,8U);m.put(data+44,((stage?0U:1U)<<16)|0x50U);m.put(data+48,2U);
  }
 }
 m.put(context+0x2b50,arena);m.put(context+0x2b78,queue);m.put(context+0x2b80,4U);
 m.put(filter,std::array<std::uint64_t,4>{15ULL|(4100ULL<<32),0x9000,0,objects[0]});
 mhw::scaled_temporal::OutputDiagnostics::Graph g;g.at={100,100,100,context,0x2000,0x3000,0,1};g.count=1;
 g.filters[0]={filter,15,4100,0x9000,0x9100,0,objects[0]};g.reason=17;g.command_gate={9,0x78001004,0,UINT32_MAX,0,0,0,2};
 const auto read=[&](std::uint64_t at,void *out,std::size_t n){return m.read(at,out,n);};
 const auto emit=[](const char *name,const Capture &c){std::cout<<"{\"case\":\""<<name<<"\",\"trace\":"<<std::boolalpha;c.write(std::cout);std::cout<<"}\n";};
 auto c=std::make_unique<Capture>();c->observe(g,read,[]{return true;});check(c->size()==1,"stable dependency snapshot");
 const auto &s=c->sample(0);check(s.count==2&&s.draws[0].index==1&&s.draws[1].index==3,"native draw order");
 check(s.draws[0].target[5]==s.draws[1].textures[0].fields[3]&&s.draws[1].target[5]==0xa000,"auxiliary producer consumer edge");emit("dependency",*c);
 auto before=m.reads;c->observe(g,read,[]{return true;});check(m.reads==before,"same frame not resampled");
 g.at[1]+=60;c->observe(g,read,[]{return true;});check(c->size()==2,"two distinct frames");emit("full",*c);
 before=m.reads;g.at[1]+=60;c->observe(g,read,[]{return true;});check(m.reads==before,"capacity stops reading");
 auto bad=std::make_unique<Capture>();bad->observe(g,read,[]{return false;});check(!bad->size(),"changed stamp rejected");emit("changed",*bad);
 auto unequal=std::make_unique<Capture>();unsigned scans{};
 unequal->observe(g,[&](std::uint64_t at,void *out,std::size_t n){if(at==queue&&++scans==2)m.put(backends[1]+0x20,std::uint64_t{0xb008});return read(at,out,n);},[]{return true;});
 check(!unequal->size(),"changed texture rejected");emit("changed-texture",*unequal);m.put(backends[1]+0x20,std::uint64_t{0xa008});
 auto bound=std::make_unique<Capture>();m.put(sets[0]+0x88,65U);bound->observe(g,read,[]{return true;});check(!bound->size(),"owner bounds");emit("owner-bound",*bound);m.put(sets[0]+0x88,1U);
 auto shader=std::make_unique<Capture>();m.put(shader_descriptors[0],16385U);shader->observe(g,read,[]{return true;});check(!shader->size(),"shader bounds");emit("shader-bound",*shader);m.put(shader_descriptors[0],52U);
 auto queue_bound=std::make_unique<Capture>();m.put(context+0x2b80,65535U);queue_bound->observe(g,read,[]{return true;});check(!queue_bound->size(),"queue bounds");emit("queue-bound",*queue_bound);m.put(context+0x2b80,4U);
 auto retries=std::make_unique<Capture>();for(unsigned i=0;i<20;++i){g.at[1]+=60;retries->observe(g,read,[]{return false;});}
 check(retries->attempts()==16&&!retries->size(),"attempt budget");emit("attempt-limit",*retries);
 auto draws=std::make_unique<Capture>();for(unsigned i=4;i<7;++i){m.put(queue+16*i,0x78001004U);m.put(queue+16*i+8,s.draws[0].packet);}
 m.put(context+0x2b80,7U);draws->observe(g,read,[]{return true;});check(!draws->size(),"draw capacity");emit("draw-capacity",*draws);m.put(context+0x2b80,4U);
 {
  auto color=g;color.reason=6;color.detail=4;color.command_gate={};color.link_reads=7;
  color.links[4]=objects[1];color.links[5]=objects[0];color.filters[0][2]=26005;color.filters[0][5]=objects[1];
  const auto native_filter=std::array<std::uint64_t,4>{15ULL|(26005ULL<<32),0x9000,objects[1],objects[0]};
  m.put(filter,native_filter);for(unsigned i=0;i<4;++i)m.put(queue+16*i,0x78006595U);
  auto intermediate=std::make_unique<Capture>();intermediate->observe_intermediate(color,read,[]{return true;},0x9100);
  check(intermediate->size()==1&&intermediate->sample(0).count==2,"intermediate multiple draws captured");
  check(intermediate->sample(0).key==0x78006595U&&intermediate->sample(0).connection==
    std::array<std::uint64_t,3>{filter,objects[1],objects[0]},"intermediate connection retained");
  emit("intermediate",*intermediate);
  for(unsigned variant=0;variant<8;++variant){auto invalid=color;
   switch(variant){case 0:invalid.reason=17;break;case 1:invalid.detail=3;break;
    case 2:invalid.link_reads=3;break;case 3:invalid.links[4]=objects[0];break;
    case 4:invalid.links[5]=0;break;case 5:invalid.filters[0][4]++;break;
    case 6:invalid.count=2;invalid.filters[1]=invalid.filters[0];break;case 7:invalid.count=17;break;}
   auto ignored=std::make_unique<Capture>();const auto reads_before=m.reads;
   ignored->observe_intermediate(invalid,read,[]{return true;},0x9100);
   check(!ignored->attempts()&&m.reads==reads_before,"unrelated or ambiguous color chain ignored");
  }
  auto stale=std::make_unique<Capture>();m.put(filter+16,objects[0]);
  stale->observe_intermediate(color,read,[]{return true;},0x9100);
  check(stale->attempts()==1&&!stale->size(),"stale intermediate descriptor rejected");emit("intermediate-stale",*stale);
  m.put(filter,native_filter);auto moving=std::make_unique<Capture>();
  moving->observe_intermediate(color,read,[]{return false;},0x9100);
  check(!moving->size(),"intermediate changed frame rejected");emit("intermediate-changing",*moving);
  before=m.reads;intermediate->observe_intermediate(color,read,[]{return true;},0x9100);
  check(before==m.reads,"intermediate sample throttled");color.at[1]+=60;
  intermediate->observe_intermediate(color,read,[]{return true;},0x9100);check(intermediate->size()==2,"intermediate bounded repeat");
  color.at[1]+=60;before=m.reads;intermediate->observe_intermediate(color,read,[]{return true;},0x9100);
  check(before==m.reads,"intermediate capacity stops reading");emit("intermediate-full",*intermediate);
  m.put(filter,std::array<std::uint64_t,4>{15ULL|(4100ULL<<32),0x9000,0,objects[0]});
  for(unsigned i=0;i<4;++i)m.put(queue+16*i,0x78001004U);
 }
 #include "output_compute_bundle_fixture.inl"
 auto unreadable=std::make_unique<Capture>();m.put(sets[0],UINT64_MAX-2);unreadable->observe(g,read,[]{return true;});check(!unreadable->size(),"unreadable binding range");emit("unreadable-range",*unreadable);
}
