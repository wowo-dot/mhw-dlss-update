#include "../src/reflex/frame_boundaries.hpp"
#include <windows.h>
#include <cmath>
#include <iostream>
#include <stdexcept>

using Frame=mhw::reflex::boundaries::Frame;
extern "C" {
void mhw_reflex_boundary_forward(void*,Frame*);
std::uint64_t reflex_boundary_update(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
double reflex_boundary_advance(std::uint64_t,double,std::uint64_t,double);
std::uint64_t reflex_boundary_publish(std::uint64_t);
std::uint64_t reflex_boundary_keyboard(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
std::uint64_t reflex_boundary_mouse(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
std::uint64_t reflex_boundary_devices(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
std::uint64_t reflex_boundary_jobs(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
std::uint64_t reflex_boundary_submit(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
std::uint64_t reflex_boundary_device_poll(std::uint64_t,std::uint64_t,std::uint64_t,std::uint64_t);
}
namespace {
std::uint64_t integer(std::uint64_t a,std::uint64_t b,std::uint64_t c,std::uint64_t d) {
    if(GetLastError()!=731)throw std::runtime_error("input last-error");
    SetLastError(173); return a^(b*3)^(c*7)^(d*13);
}
double mixed(std::uint64_t a,double b,std::uint64_t c,double d) {
    if(GetLastError()!=731)throw std::runtime_error("mixed input last-error");
    SetLastError(173); return static_cast<double>(a)+b*3+static_cast<double>(c)*7+d*13;
}
std::uint64_t exceptional(std::uint64_t) { RaiseException(0xe1234567,0,0,nullptr); return 0; }
unsigned calls{},cleanups{};
bool exception_test() noexcept {
    __try { reflex_boundary_publish(17); }
    __except(GetExceptionCode()==0xe1234567 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) { return true; }
    return false;
}
}
extern "C" void mhw_reflex_boundary_dispatch(unsigned kind,Frame* f) noexcept {
    const auto prior=GetLastError(); ++calls; SetLastError(prior);
    void* target=kind==0 || kind>=7?reinterpret_cast<void*>(&integer):kind==1?reinterpret_cast<void*>(&mixed):reinterpret_cast<void*>(&exceptional);
    __try { mhw_reflex_boundary_forward(target,f); }
    __finally { const auto error=GetLastError(); ++cleanups; SetLastError(error); }
}
int main() {
    for(unsigned i=0;i<256;++i) {
        const auto a=0x1122334455667788ULL+i,b=0xfedcba9876543210ULL-i,c=7ULL*i,d=13ULL*i;
        SetLastError(731);
        if(reflex_boundary_update(a,b,c,d)!=(a^(b*3)^(c*7)^(d*13)) || GetLastError()!=173)return 1;
        const auto x=static_cast<double>(i)+.125,y=-static_cast<double>(i)-.25;
        SetLastError(731);
        const auto actual=reflex_boundary_advance(i,x,i+1,y);
        if(actual!=static_cast<double>(i)+x*3+static_cast<double>(i+1)*7+y*13 || GetLastError()!=173)return 2;
    }
    const auto added={&reflex_boundary_keyboard,&reflex_boundary_mouse,&reflex_boundary_devices,
        &reflex_boundary_jobs,&reflex_boundary_submit,&reflex_boundary_device_poll};
    for(auto entry:added)for(unsigned i=0;i<256;++i) {
        const auto a=0x1122334455667788ULL+i,b=0xfedcba9876543210ULL-i,c=7ULL*i,d=13ULL*i;
        SetLastError(731);
        if(entry(a,b,c,d)!=(a^(b*3)^(c*7)^(d*13)) || GetLastError()!=173)return 4;
    }
    if(!exception_test() || calls!=2049 || cleanups!=2049)return 3;
    std::cout<<"2048 integer/vector ABI cases and exception propagation/cleanup passed\n";
}
