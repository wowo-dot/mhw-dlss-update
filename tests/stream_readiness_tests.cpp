#include "../src/scaled_temporal/readiness_trace.hpp"
#include "../src/scaled_temporal/effect_order.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
using Trace=mhw::scaled_temporal::ReadinessTrace;
void check(bool ok,const char *why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
Trace::Snapshot sample(unsigned reason=0){
    Trace::Snapshot v;v.at={100,9,9,0x1000,2,0x2000};v.request={0x3000,0x4000,1,0};
    v.plan={9,9,9,1,1,0,0,0x5000,0x3000,0x4000,1,0};v.accepting=v.stamp_equal=true;
    if(reason==Trace::inactive)v.accepting=false;
    if(reason==Trace::faulted)v.fault=8;
    if(reason==Trace::active_route)v.active={8,0x6000,2,0x7000,1,1,3,8,8};
    if(reason==Trace::arguments)v.at[4]=0;
    if(reason==Trace::unbuilt)v.plan[4]=0;
    if(reason==Trace::claimed)v.plan[5]=1;
    if(reason==Trace::executed)v.plan[6]=3;
    if(reason==Trace::stamp){v.stamp_equal=false;v.plan[1]=8;}
    if(reason==Trace::pipeline_missing){v.pipeline_missing_mask=2;v.stage=1;}
    if(reason==Trace::pipeline_signature){v.pipeline_signature_mask=4;v.stage=2;}
    return v;
}
void emit(const std::string &name,const Trace &t){std::cout<<std::boolalpha<<"{\"case\":\""<<name<<"\",\"trace\":";t.write(std::cout);std::cout<<"}\n";}
int main(){
    // game-38: a build that failed after it counted haze and 8 overlays read
    // stages past the 4-entry haze table (a garbage draw index).
    for(const bool haze:{false,true})for(unsigned overlays=0;overlays<=8;++overlays){
        std::array<bool,12> seen{};const unsigned count=(haze?4U:3U)+overlays;
        for(unsigned ordinal=0;ordinal<count;++ordinal){const auto stage=mhw::scaled_temporal::unordered_effect_stage(haze,ordinal);
            check(stage<seen.size()&&!seen[stage]&&(haze||stage!=3),"unordered stages are distinct plan stages");seen[stage]=true;}
    }
    {Trace t;emit("empty",t);}
    for(unsigned reason=0;reason<Trace::reason_count;++reason){Trace t;unsigned captures{};
        t.note(static_cast<Trace::Reason>(reason),[&]{++captures;return sample(reason);});
        check(captures==1,"each first decision captures its evidence");emit("reason-"+std::to_string(reason),t);
    }
    {Trace t;unsigned captures{};
        for(unsigned i=0;i<10000;++i)t.note(Trace::accepted,[&]{++captures;return sample();});
        check(captures==1,"successful frames never recapture after the baseline");emit("success-counts",t);
    }
    {Trace t;unsigned captures{};
        for(unsigned i=0;i<20;++i){t.note(Trace::accepted,[&]{++captures;return sample();});
            t.note(Trace::executed,[&]{++captures;auto v=sample(Trace::executed);v.at[0]=100+i;return v;});}
        check(captures==21,"bounded rejection capture only");std::ostringstream output;output<<std::boolalpha;t.write(output);
        const auto text=output.str();check(text.size()<8192,"bounded serialized evidence");
        check(text.find("\"ordinal\":2,\"rejection\":1")!=std::string::npos,"first rejection retained");
        check(text.find("\"ordinal\":40,\"rejection\":20")!=std::string::npos,"latest rejection retained");
        check(text.find("\"rejection\":19,")==std::string::npos,"middle rejection evicted from latest");emit("bounded",t);
    }
}
