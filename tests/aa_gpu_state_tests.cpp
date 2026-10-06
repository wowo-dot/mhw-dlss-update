#include "../src/aa_control/gpu_state.hpp"
#include <cstdlib>
#include <iostream>
namespace g=mhw::aa_control::gpu_trace;
void check(bool v,const char *why){if(!v){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
    g::State s;check(!s.known&&s.issue==g::Issue::unknown,"initial state unknown");
    s.transition(4,192,0,UINT32_MAX,100);check(s.known&&s.value==192&&s.transitions==1,"first explicit transition anchors state");
    s.transition(192,8,0,0,101);check(s.known&&s.value==8&&s.transitions==2,"single-plane subresource zero accepted");
    s.transition(4,192,0,0,102);check(!s.known&&s.issue==g::Issue::conflict,"state mismatch rejected");
    s.transition(192,4,0,0,103);check(!s.known,"later barrier cannot repair ambiguous history");
    s={};check(!s.known&&!s.transitions&&!s.last,"reset generation starts unknown");
    s.transition(4,192,1,0,1);check(!s.known&&s.issue==g::Issue::split,"split begin not guessed");
    s.transition(4,192,2,0,2);check(!s.known,"split end not guessed");
    s={};s.transition(4,192,0,1,1);check(s.issue==g::Issue::subresource,"nonexistent plane rejected");
    s={};s.transition(4,192,0,0,2);s.transition(192,4,0,0,1);check(s.issue==g::Issue::order,"out-of-order callback rejected");
    s={};s.transition(4,192,0,0,1);s.invalidate(g::Issue::alias,2);s.transition(192,4,0,0,3);check(!s.known,"alias cannot carry content/state certainty");
    std::cout<<"AA state uncertainty, generation reset and transitions passed\n";
}
