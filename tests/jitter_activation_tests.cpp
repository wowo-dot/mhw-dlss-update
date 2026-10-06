#include "recorder.hpp"
#include <windows.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include <bit>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace ja=mhw::jitter_activation;
namespace {
std::vector<std::byte> renderer(0x1f570),table(0xe740),view(0x80),backend(0x42720);
alignas(16) std::array<unsigned,64> constants{};
std::array<std::byte,0x18> metadata{};
std::uintptr_t metadata_pointer{},image{},cam{},context{};
std::atomic<unsigned> original_calls{},bad_error{};
std::atomic<bool> release_original{};
std::mutex publish;
std::string mode;
struct Allocation { void *p{}; ~Allocation(){if(p)VirtualFree(p,0,MEM_RELEASE);} } image_memory,camera_memory;
void check(bool ok,const char *why) { if(!ok)throw std::runtime_error(why); }
template<class T> void put(std::uintptr_t p,unsigned at,T value){std::memcpy(reinterpret_cast<void *>(p+at),&value,sizeof(value));}
template<class T> T get(std::uintptr_t p,unsigned at){T v{};std::memcpy(&v,reinterpret_cast<void *>(p+at),sizeof(v));return v;}
unsigned bits(float f){return std::bit_cast<unsigned>(f);}
void advance(unsigned n){
  const auto r=reinterpret_cast<std::uintptr_t>(renderer.data());
  put(r,0x1f210,std::uint64_t{10+n});put(r,0x1f208,std::uint64_t{10+n});put(cam,0x40,10+n);
}
void setup(){
  image_memory.p=VirtualAlloc(nullptr,0x55d3000,MEM_RESERVE,PAGE_NOACCESS);check(image_memory.p,"reserve image");
  image=reinterpret_cast<std::uintptr_t>(image_memory.p);
  for(auto offset:{0x51c4000U,0x4be7000U})check(VirtualAlloc(reinterpret_cast<void *>(image+offset),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit slots");
  camera_memory.p=VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);check(camera_memory.p,"camera page");
  cam=reinterpret_cast<std::uintptr_t>(camera_memory.p);
  const auto r=reinterpret_cast<std::uintptr_t>(renderer.data()),t=reinterpret_cast<std::uintptr_t>(table.data());context=r+0x710;
  put(image,0x51c4480,r);put(image,0x51c4368,t);put(image,0x51c4400,cam);
  put(r,0x78,reinterpret_cast<std::uintptr_t>(backend.data()));
  put<unsigned char>(r,0xa5,1); // Observed Off/TAA state: raw switch true, sizing disabled, no wrapper.
  put(cam,0,image+0x350d188);put<unsigned char>(cam,0x70,1);
  put(r,0x1f450,reinterpret_cast<std::uintptr_t>(view.data()));advance(0);
  put(context,0x230,bits(2560.F));put(context,0x234,bits(1440.F));
  metadata_pointer=reinterpret_cast<std::uintptr_t>(metadata.data());put(metadata_pointer,0x10,64U);
  put(context,0x2b18,reinterpret_cast<std::uintptr_t>(&metadata_pointer));
  put(context,0x710,reinterpret_cast<std::uintptr_t>(constants.data())|5);
  put(image,0x4be75f4,0U);put(image,0x4be7604,0xf00U);put(image,0x4be763c,0x10f00U);
  for(unsigned i=0;i<20;++i){put(t,0xe6a0+8*i,bits(static_cast<float>(i+1)/100));put(t,0xe6a4+8*i,bits(-0.25F));}
}
}
extern "C" bool activation_fixture(std::uintptr_t,std::uintptr_t);
extern "C" bool activation_fixture_body(std::uintptr_t camera,std::uintptr_t ctx){
  const auto call=++original_calls;
  if(camera!=cam+0x50 || ctx!=context || GetLastError()!=0x1111)++bad_error;
  if((mode=="concurrent" || mode=="stop_pending") && call==1)while(!release_original.load())SwitchToThread();
  if(!get<unsigned char>(camera,0x20)){SetLastError(0x2222);return false;}
  const auto jitter=get<std::uint64_t>(cam,0x38);
  if(mode=="exception" && jitter) { SetLastError(0x3333);RaiseException(0xe0424242,0,0,nullptr); }
  std::lock_guard lock(publish);
  constants.fill(0);
  const auto x=std::bit_cast<float>(static_cast<unsigned>(jitter)),y=std::bit_cast<float>(static_cast<unsigned>(jitter>>32));
  for(unsigned i=0;i<4;++i)constants[16+i*4+i]=bits(1.F);
  for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){
    const auto n=std::bit_cast<float>(constants[16+4*row+col]),factor=std::bit_cast<float>(constants[16+4*row+3]);
    constants[4*row+col]=bits(n+(col==0?factor*x/2560.F:col==1?factor*y/1440.F:0.F));
  }
  if(mode=="baseline_mismatch")constants[0]=bits(2.F);
  if(mode=="conflict" && jitter)put(cam,0x38,std::uint64_t{0x3e99999a3e99999a});
  if(mode=="changed_camera")put(image,0x51c4400,std::uintptr_t{0});
  if(mode=="wrapper_during_stimulus" && jitter)put(reinterpret_cast<std::uintptr_t>(backend.data()),0x42700,std::uintptr_t{0x12345678});
  SetLastError(0x2222);return true;
}
bool invoke_exception(){
  __try {SetLastError(0x1111);activation_fixture(cam+0x50,context);return false;}
  __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
}
void invoke(){SetLastError(0x1111);check(activation_fixture(cam+0x50,context),"bool return");check(GetLastError()==0x2222,"LastError return");}
int main(int argc,char **argv){
  try{
    mode=argc>1?argv[1]:"normal";setup();
    std::string why;check(!ja::host_identity(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),why),"foreign host accepted");
    if(mode=="install_failure"){
      check(!ja::install_owned(cam,image),"non-executable target accepted");
      const auto s=ja::install_state();
      check(s.stage==2 && s.status==MH_ERROR_NOT_EXECUTABLE && !s.hook_created && !s.hook_enabled,"installation failure details");
      check(std::string(ja::install_status_name())=="MH_ERROR_NOT_EXECUTABLE","status name");
      check(ja::disable(),"empty disable");std::cout<<"pass: "<<mode<<'\n';return 0;
    }
    check(ja::install_owned(reinterpret_cast<std::uintptr_t>(&activation_fixture),image),"hook install");
    check(ja::install_state().stage==3 && ja::install_state().status==MH_OK && ja::install_state().hook_enabled,"successful installation details");
    check(ja::arm(mode=="deadline"?GetTickCount64()-ja::window_ms-1:0),"arm");check(!ja::arm(),"rearm");
    if(mode=="normal"){
      for(unsigned i=0;i<3;++i){advance(i);invoke();check(get<std::uint64_t>(cam,0x38)==0,"camera not restored");}
      const auto s=ja::state();check(s.stop==ja::Stop::complete && s.reserved==3 && !s.pending && s.injections==1 && s.restorations==1,"three-stage bound");
      for(unsigned i=0;i<3;++i){ja::Call r;check(ja::copy_call(i,r) && !r.rejection && r.returned && r.result && r.restored && r.projection.valid && r.projection.matrix_relation && r.injected==(i==1),"sample acceptance");}
      invoke();check(ja::state().injections==1 && original_calls==4,"one injection/exact forwarding");
    }else if(mode=="deadline"){
      invoke();check(ja::state().stop==ja::Stop::deadline && ja::state().reserved==0,"deadline");
    }else if(mode=="wrapper_present" || mode=="sizing_enabled" || mode=="backend_absent"){
      if(mode=="wrapper_present")put(reinterpret_cast<std::uintptr_t>(backend.data()),0x42700,std::uintptr_t{0x12345678});
      if(mode=="sizing_enabled")put(reinterpret_cast<std::uintptr_t>(renderer.data()),0xb4,1U);
      if(mode=="backend_absent")put(reinterpret_cast<std::uintptr_t>(renderer.data()),0x78,std::uintptr_t{0});
      invoke();ja::Call r;
      check(ja::state().stop==ja::Stop::invalid && ja::state().injections==0 && ja::copy_call(0,r) && r.admission_checkpoint==6,"provider exclusion admission");
      if(mode=="wrapper_present")check(r.provider_before.wrapper==0x12345678 && r.provider_before.use_dlss==1,"rejected values not retained");
    }else if(mode=="wrapper_during_stimulus"){
      invoke();advance(1);invoke();ja::Call r;
      check(ja::state().stop==ja::Stop::invalid && ja::state().restorations==1 && get<std::uint64_t>(cam,0x38)==0 && ja::copy_call(1,r) && r.rejection==9,"changed provider restore");
    }else if(mode=="baseline_mismatch" || mode=="changed_camera"){
      invoke();check(ja::state().stop==ja::Stop::invalid && ja::state().injections==0,"bad baseline permitted activation");
    }else if(mode=="same_frame"){
      invoke();invoke();check(ja::state().reserved==1 && !ja::state().injections,"same frame consumed twice");
      advance(1);invoke();advance(2);invoke();check(ja::state().stop==ja::Stop::complete,"later frames failed");
    }else if(mode=="stop_pending"){
      std::thread a([]{invoke();});while(original_calls.load()<1)SwitchToThread();
      ja::stop();check(!ja::settled(),"pending original incorrectly drained");release_original=true;a.join();
      check(ja::settled() && ja::state().stop==ja::Stop::requested && !ja::state().injections,"pending stop");
      advance(1);invoke();check(ja::state().reserved==1 && !ja::state().injections,"post-stop admission");
    }else if(mode=="invalid"){
      put<unsigned char>(cam,0x70,0);SetLastError(0x1111);check(!activation_fixture(cam+0x50,context),"false return preservation");
      check(ja::state().stop==ja::Stop::invalid && ja::state().injections==0,"invalid admission");
    }else if(mode=="concurrent"){
      std::thread a([]{invoke();});while(original_calls.load()<1)SwitchToThread();
      std::thread b([]{invoke();});while(original_calls.load()<2)SwitchToThread();release_original=true;a.join();b.join();
      check(ja::state().stop==ja::Stop::invalid && ja::state().dropped==1 && !ja::state().injections && original_calls==2,"concurrent admission");
    }else if(mode=="exception" || mode=="conflict" || mode=="readonly"){
      invoke();advance(1);
      if(mode=="exception"){
        check(invoke_exception(),"native exception swallowed");
        check(ja::state().restorations==1 && get<std::uint64_t>(cam,0x38)==0,"unwind restore");
      }else{
        DWORD old{};if(mode=="readonly")check(VirtualProtect(camera_memory.p,4096,PAGE_READONLY,&old)!=FALSE,"protect camera");
        invoke();
        if(mode=="readonly")check(ja::state().injections==0 && get<std::uint64_t>(cam,0x38)==0,"readonly mutation");
        else check(!ja::state().restorations && get<std::uint64_t>(cam,0x38)==0x3e99999a3e99999aULL,"concurrent value overwritten");
      }
      ja::Call r;check(ja::state().stop==ja::Stop::invalid && !ja::state().pending && ja::copy_call(1,r) && r.rejection,"failed mutation evidence");
    }else throw std::runtime_error("unknown mode");
    check(!bad_error,"arguments/incoming LastError");check(ja::remove_owned(),"restore hook bytes");
    std::cout<<"pass: "<<mode<<'\n';return 0;
  }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}

