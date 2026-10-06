#include "../src/projection_binding/recorder.hpp"
#include <windows.h>
#include <iostream>
#include <cstdlib>
namespace pb=mhw::projection_binding;
extern "C" std::uint64_t post_queue_fixture_call(std::uintptr_t);
extern "C" void mhw_binding_forward(void *,pb::Frame *);
unsigned calls{},entered{},exited{};bool throwing{};
void check(bool value,const char *message){if(!value){std::cerr<<message<<'\n';std::exit(1);}}
std::uint64_t original(std::uintptr_t a,std::uintptr_t b,std::uintptr_t c,std::uintptr_t d){
    check(a==0x1122&&b==0x3344&&c==0x5566&&d==0x7788,"forwarded registers");++calls;
    if(throwing)RaiseException(0xe0424242,0,0,nullptr);
    return 0x123456789abcdef0;
}
extern "C" void mhw_aa_schedule(pb::Frame *frame){
    ++entered;
    check(frame->gpr[1]==0x1122&&frame->gpr[2]==0x3344&&frame->gpr[3]==0x5566&&frame->caller,"captured arguments and caller");
    __try{mhw_binding_forward(reinterpret_cast<void *>(original),frame);}__finally{++exited;}
}
extern "C" void mhw_binding_dispatch(unsigned,pb::Frame *){std::abort();}
void exception_test(){
    bool caught=false;
    __try{post_queue_fixture_call(0x9090);}__except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){caught=true;}
    check(caught,"native exception propagated through both unwind frames");
}
int main(){
    for(unsigned i=0;i<1000;++i)check(post_queue_fixture_call(0x9090)==0x123456789abcdef0,"return value preserved");
    throwing=true;exception_test();
    check(calls==1001&&entered==1001&&exited==1001,"exact forwarding and exception cleanup");
    std::cout<<"Post queue ABI: 1000 forwards, native return and exception cleanup passed\n";
}
