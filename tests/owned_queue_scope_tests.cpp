#include "../src/aa_control/owned_runtime.hpp"
#include "../src/projection_binding/memory.hpp"
#include <windows.h>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <algorithm>
namespace aa=mhw::aa_control;
namespace mhw::projection_binding {
bool copy_memory(std::uintptr_t p,void *out,unsigned n) noexcept {std::memcpy(out,reinterpret_cast<void *>(p),n);return true;}
bool system_method(std::uintptr_t,unsigned,std::uintptr_t &) noexcept{return false;}
}
void check(bool ok,const char *why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
alignas(16) std::array<std::byte,0xd0> memory;
aa::OwnedPlan fixture(unsigned count=4){
    memory={};memory[0x39]=std::byte{1};auto *n=reinterpret_cast<unsigned *>(memory.data()+0x3c);*n=count;
    auto *ptrs=reinterpret_cast<std::uintptr_t *>(memory.data()+0x40);
    aa::OwnedPlan p;p.descriptor=99;p.before.manager=reinterpret_cast<std::uintptr_t>(memory.data());p.before.count=count;
    for(unsigned i=0;i<count+1;++i){ptrs[i]=100+i;if(i<count)p.before.nodes[i].descriptor=ptrs[i];}
    return p;
}
void throw_native(const aa::OwnedPlan &p){
    check(aa::owned::fixture_attach(p),"attach before exception");
    __try {RaiseException(0xe0424242,0,0,nullptr);}
    __finally {check(aa::owned::fixture_detach(),"detach through native exception");}
}
void unwind_case(const aa::OwnedPlan &p){
    bool caught=false;
    __try{throw_native(p);}__except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){caught=true;}
    check(caught,"native exception preserved");
}
int main(){
    auto p=fixture();const auto original=memory;
    check(aa::owned::fixture_attach(p)&&!aa::owned::detached(),"descriptor inserted");
    aa::owned::Admission diagnostic;
    check(!aa::owned::recycle(0,&diagnostic)&&std::string(diagnostic.step)=="recycle-attached"&&
          diagnostic.previous_inserted&&diagnostic.attached_count==1,"recycle rejection preserves live ownership evidence");
    auto *ptrs=reinterpret_cast<std::uintptr_t *>(memory.data()+0x40);
    check(*reinterpret_cast<unsigned *>(memory.data()+0x3c)==5&&ptrs[4]==99,"count and slot published");
    std::swap(ptrs[0],ptrs[4]);std::swap(ptrs[2],ptrs[3]);
    check(aa::owned::fixture_detach()&&aa::owned::detached()&&memory==original,"exact unsorted slots and stale tail restored");
    check(!aa::owned::recycle(0,&diagnostic)&&std::string(diagnostic.step)=="recycle-incomplete"&&
          diagnostic.previous_removed&&!diagnostic.previous_complete,"detachment alone does not certify callback completion");
    p=fixture();unwind_case(p);check(memory==original&&aa::owned::detached(),"exception leaves queue exact");
    p=fixture();ptrs[1]=777;auto changed=memory;
    diagnostic={};check(!aa::owned::fixture_attach(p,&diagnostic)&&memory==changed,"changed queue rejected without writes");
    check(std::string(diagnostic.step)=="attach-entry"&&diagnostic.attach_entries[1]==777,"record colliding native entry");
    p=fixture();check(aa::owned::fixture_attach(p),"second attach");ptrs[3]=777;changed=memory;
    check(!aa::owned::fixture_detach()&&memory==changed&&!aa::owned::detached(),"foreign mutation not overwritten");
    ptrs[3]=103;check(aa::owned::fixture_detach()&&aa::owned::detached(),"retained node can be removed after evidence restored");
    p=fixture();memory[0x39]=std::byte{0};changed=memory;diagnostic={};
    check(!aa::owned::fixture_attach(p,&diagnostic)&&memory==changed&&std::string(diagnostic.step)=="attach-phase","phase rejection is observable without mutation");
    p=fixture();*reinterpret_cast<unsigned *>(memory.data()+0x3c)=3;changed=memory;diagnostic={};
    check(!aa::owned::fixture_attach(p,&diagnostic)&&memory==changed&&diagnostic.attach_count==3&&std::string(diagnostic.step)=="attach-count","count rejection retains observed count");
    p=fixture();p.before.manager=0;diagnostic={};
    check(!aa::owned::fixture_attach(p,&diagnostic)&&std::string(diagnostic.step)=="attach-unwritable","invalid attachment memory rejected before access");
    diagnostic={};check(aa::owned::recycle(1,&diagnostic),"fresh descriptor can be recycled");
    aa::owned::configure(0x140000000,0);aa::PostQueue invalid;
    check(!aa::owned::insert(1,invalid,&diagnostic)&&std::string(diagnostic.step)=="plan"&&diagnostic.descriptor,"plan refusal records owned descriptor before any native read");
    std::ostringstream out;out<<std::boolalpha;aa::owned::write_admission(out,diagnostic,aa::write_post_queue);
    check(out.str().find("\"step\":\"plan\"")!=std::string::npos&&out.str().find("\"second_queue\":{")!=std::string::npos,"diagnostic snapshot serializes bounded queues");
    std::array<unsigned,6> order{0,1,2,3,4,5};unsigned permutations{};
    do {
        p=fixture(5);const auto before=memory;diagnostic={};
        check(aa::owned::fixture_attach(p,&diagnostic)&&*reinterpret_cast<unsigned *>(memory.data()+0x3c)==6,"five native nodes plus owned descriptor");
        const std::array<std::uintptr_t,6> values{100,101,102,103,104,99};
        for(unsigned i=0;i<6;++i)ptrs[i]=values[order[i]];
        check(aa::owned::fixture_detach()&&memory==before&&aa::owned::detached(),"all six-entry permutations restore native ordering and unused tail");
        ++permutations;
    }while(std::next_permutation(order.begin(),order.end()));
    check(permutations==720,"all haze descriptor orderings");
    p=fixture(5);const auto five_original=memory;unwind_case(p);check(memory==five_original,"five-node exception restoration");
    for(unsigned i=0;i<6;++i){p=fixture(5);check(aa::owned::fixture_attach(p),"haze collision setup");const auto original_entry=ptrs[i];
        ptrs[i]=888;const auto foreign=memory;check(!aa::owned::fixture_detach()&&memory==foreign,"foreign entries in six-node queue retained");
        ptrs[i]=original_entry;check(aa::owned::fixture_detach(),"haze collision evidence restored");}
    std::cout<<"Owned queue scope: insertion, sorting, exact restoration, unwind and collision refusal passed\n";
}
