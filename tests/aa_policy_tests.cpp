#include "../src/aa_control/policy.hpp"
#include <iostream>
#include <stdexcept>
using namespace mhw::aa_control;
void check(bool value){if(!value)throw std::runtime_error("AA policy contract");}
int main(){try{
    check(parse_mode("native")==Mode::native&&parse_mode("off")==Mode::off&&parse_mode("dlaa")==Mode::dlaa);
    for(auto invalid:{"dlss","fxaa","taa","DLAA","","dlaa\n","dlaa extra"})check(!parse_mode(invalid));
    Policy p;p.select(Mode::dlaa);
    auto d=p.begin(false);check(d.path==Path::unfiltered&&!d.jitter&&p.state()==State::waiting);
    d=p.begin(true);check(d.path==Path::reconstruct&&d.jitter&&d.reset_history);p.complete(true);
    for(unsigned i=0;i<1000;++i){d=p.begin(true);check(!d.reset_history);p.complete(true);}
    d=p.begin(false);check(!d.jitter&&d.path==Path::unfiltered);
    d=p.begin(true);check(d.reset_history);p.complete(true);
    d=p.begin(true);p.complete(false);check(p.state()==State::failed);
    for(unsigned i=0;i<1000;++i){d=p.begin(i%2==0);check(d.path==Path::unfiltered&&!d.jitter&&p.state()==State::failed);}
    p.select(Mode::dlaa);d=p.begin(true);check(d.reset_history);p.complete(true);
    p.begin(true);d=p.begin(true);check(d.path==Path::unfiltered&&p.state()==State::failed);
    p.select(Mode::off);d=p.begin(true);check(d.path==Path::unfiltered&&!d.jitter);
    p.select(Mode::native);d=p.begin(true);check(d.path==Path::native&&!d.jitter);
    for(auto path:{Path::native,Path::unfiltered,Path::reconstruct})for(bool success:{false,true})for(bool recovery:{false,true}){
        unsigned native{},sdk{},copy{};
        const bool result=route(Decision{path},[&]{++sdk;return success;},[&]{++copy;return recovery;},[&]{++native;});
        check(native==(path==Path::native?1U:0U));check(sdk==(path==Path::reconstruct?1U:0U));
        check(copy==(path==Path::unfiltered||(path==Path::reconstruct&&!success)?1U:0U));
        check(result==(path==Path::native||(path==Path::reconstruct&&success)||recovery));
    }
    std::cout<<"Independent AA selection, suspension, latched failure and exclusive routing passed\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
