#include "../src/scaled_temporal/recording_states.hpp"
#include <iostream>
#include <stdexcept>
#include <memory>
using mhw::scaled_temporal::RecordingStates;
void require(bool v){if(!v)throw std::runtime_error("recording state contract");}
int main(){try{
    RecordingStates<2,2> t;using R=decltype(t)::Result;
    require(!t.get(1,1,3).known);
    require(t.observe(1,1,3,true,0,4)==R::ok&&t.get(1,1,3).known&&t.get(1,1,3).value==4);
    require(!t.get(2,1,3).known&&!t.get(1,2,3).known);
    require(t.observe(1,2,3,true,4,8)==R::identity);
    require(t.observe(1,1,3,true,8,16)==R::contradiction&&!t.get(1,1,3).known);
    require(t.observe(1,1,3,true,16,4)==R::ok);
    require(t.observe(1,1,3,false,0,0)==R::ok&&!t.get(1,1,3).known);
    require(t.observe(1,1,3,true,8,4)==R::ok);
    require(t.observe(1,1,4,true,0,4)==R::ok);
    require(t.observe(1,1,5,true,0,4)==R::capacity);
    require(t.observe(2,8,3,true,0,8)==R::ok&&t.get(1,1,3).value==4&&t.get(2,8,3).value==8);
    require(t.observe(3,9,3,true,0,4)==R::capacity);
    require(!t.reset(1,2)&&t.get(1,1,3).known);
    require(t.reset(1,1)&&!t.get(1,1,3).known&&t.get(2,8,3).known);
    require(t.observe(1,2,3,true,0,16)==R::ok&&t.get(1,2,3).value==16&&!t.get(1,1,3).known);
    require(t.observe(0,1,1,true,0,4)==R::identity&&t.observe(1,0,1,true,0,4)==R::identity&&t.observe(1,1,0,true,0,4)==R::identity);
    RecordingStates<2,2,std::shared_ptr<int>> held;auto owner=std::make_shared<int>(7);std::weak_ptr<int> weak=owner;
    require(held.observe(1,1,3,true,0,4,owner)==decltype(held)::Result::ok);owner.reset();require(!weak.expired());
    require(!held.reset(1,2)&&!weak.expired());require(held.reset(1,1)&&weak.expired());
    owner=std::make_shared<int>(8);weak=owner;require(held.observe(1,2,3,true,0,4,owner)==decltype(held)::Result::ok);owner.reset();held.clear();require(weak.expired());
    std::cout<<"PASS generation isolation, reset, invalidation, contradictions and bounded capacity\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
