#include "../src/scaled_temporal/frame_handoff.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
using H=mhw::scaled_temporal::FrameHandoff;
using R=H::Result;
void check(bool value){if(!value){std::cerr<<"frame handoff check failed\n";std::exit(1);}}
int main(){
    // Every partition of a six-node GPU chain into contiguous native command
    // recordings, and every CPU ordering of those recordings (1,631 cases).
    const std::array<std::uintptr_t,6> packets{11,12,13,14,15,16};
    unsigned permutations{};
    for(unsigned cuts=0;cuts<32;++cuts){
        std::array<unsigned,6> group{};unsigned groups=1;
        for(unsigned n=1;n<6;++n){if(cuts&(1U<<(n-1)))++groups;group[n]=groups-1;}
        std::array<unsigned,6> order{};for(unsigned n=0;n<groups;++n)order[n]=n;
        do{
            H h;check(h.prepare({1,2,3,4},packets)==R::ok);
            std::array<H::Recording,6> batch{};
            for(unsigned g=0;g<groups;++g)batch[g]={100+g,20+g};
            for(unsigned ordinal=0;ordinal<groups;++ordinal)for(unsigned n=0;n<6;++n)
                if(group[n]==order[ordinal])check(h.record(n,packets[n],batch[group[n]])==R::ok);
            check(h.recorded()==6);check(h.choose(H::Choice::reconstructed)==R::ok);
            check(h.submitted(1,std::span(batch).first(groups),1)==R::ok);
            check(!h.retired(1));
            for(unsigned g=0;g<groups;++g)check(h.reset(batch[g])==R::ok);
            check(!h.retired(0)&&h.retired(1)&&!h.retired(UINT64_MAX));++permutations;
        }while(std::next_permutation(order.begin(),order.begin()+groups));
    }
    check(permutations==1631);
    H h;check(h.prepare({2,3,4,5},packets)==R::ok);
    const H::Recording a{100,1},b{200,1};
    check(h.record(3,packets[3],b)==R::ok);
    check(h.record(2,packets[2],b)==R::recording_order);
    check(h.record(3,packets[3],b)==R::duplicate);
    check(h.record(0,packets[0]+1,a)==R::identity);
    check(h.record(0,packets[0],a)==R::ok);
    check(h.record(1,packets[1],{100,2})==R::identity);
    check(h.submitted(1,std::span(&a,1),1)==R::undecided);
    check(h.choose(H::Choice::native)==R::ok);
    check(h.choose(H::Choice::reconstructed)==R::undecided);
    check(h.submitted(1,std::span(&b,1),1)==R::submission_order);
    check(h.submitted_count()==0&&h.uses()[3].signal==0);
    check(h.reset(a)==R::retirement);
    check(h.record(1,packets[1],a)==R::ok);check(h.record(2,packets[2],a)==R::ok);
    check(h.submitted(1,std::span(&a,1),1)==R::ok);
    check(h.record(4,packets[4],a)==R::already_submitted);
    check(h.submitted(1,std::span(&a,1),2)==R::already_submitted);
    check(h.record(4,packets[4],b)==R::ok);check(h.record(5,packets[5],b)==R::ok);
    check(h.submitted(2,std::span(&b,1),2)==R::queue_change);
    check(h.submitted(1,std::span(&b,1),1)==R::signal);
    check(h.submitted(1,std::span(&b,1),UINT64_MAX)==R::signal);
    check(h.submitted(1,std::span(&b,1),2)==R::ok);
    check(h.reset(a)==R::ok&&h.reset(b)==R::ok);
    check(!h.retired(1)&&h.retired(2));
    H invalid;const std::array<std::uintptr_t,2> duplicates{1,1};
    check(invalid.prepare({1,0,2,3},duplicates)==R::duplicate);
    check(invalid.count()==0);
    // Maximum late indexed chain, recorded on four workers out of CPU order.
    std::array<std::uintptr_t,H::capacity> late{};for(unsigned i=0;i<late.size();++i)late[i]=1000+i;
    H expanded;check(expanded.prepare({7,8,9,10},late)==R::ok);
    std::array<H::Recording,4> workers{{{101,3},{102,3},{103,3},{104,3}}};
    static_assert(H::capacity%4==0);constexpr unsigned per=H::capacity/4;
    for(unsigned worker:{3U,1U,2U,0U})for(unsigned i=per*worker;i<per*(worker+1);++i)check(expanded.record(i,late[i],workers[worker])==R::ok);
    check(expanded.choose(H::Choice::reconstructed)==R::ok);unsigned end{};
    auto reversed=workers;std::reverse(reversed.begin(),reversed.end());
    check(expanded.validate_submission(1,reversed,5,end)==R::submission_order&&expanded.submitted_count()==0);
    check(expanded.submitted(1,workers,5)==R::ok&&!expanded.retired(5));
    for(auto worker:workers)check(expanded.reset(worker)==R::ok);check(!expanded.retired(4)&&expanded.retired(5));
    std::cout<<"PASS "<<permutations<<" CPU-order/partition cases and negative submission/lifetime cases\n";
}
