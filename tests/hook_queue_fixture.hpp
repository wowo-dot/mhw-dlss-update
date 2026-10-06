#pragma once
#include <windows.h>
#include <MinHook.h>
#include <array>
#include <cstring>
#include <stdexcept>

namespace mhw::fixtures {
// Two private executable stubs model another component's queued enable/disable.
// Observer setup and cleanup must not apply either operation on its behalf.
class QueuedHookIsolation {
    using Call=int(*)();
    unsigned char *code_{};
    std::array<bool,2> created_{};
    static int first() {return 3;}
    static int second() {return 4;}
    static void check(bool value,const char *message){if(!value)throw std::runtime_error(message);}
    int call(unsigned i) const {return reinterpret_cast<Call>(code_+32*i)();}
public:
    QueuedHookIsolation() {
        code_=static_cast<unsigned char *>(VirtualAlloc(nullptr,64,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        check(code_!=nullptr,"queue fixture allocation");
        std::memset(code_,0x90,64);
        for(unsigned i=0;i<2;++i){const unsigned value=i+1;code_[32*i]=0xb8;std::memcpy(code_+32*i+1,&value,4);code_[32*i+5]=0xc3;}
        DWORD old{};check(VirtualProtect(code_,64,PAGE_EXECUTE_READ,&old)!=0,"queue fixture protection");
        check(FlushInstructionCache(GetCurrentProcess(),code_,64)!=0,"queue fixture flush");
        const std::array<void *,2> callbacks{reinterpret_cast<void *>(first),reinterpret_cast<void *>(second)};
        for(unsigned i=0;i<2;++i){void *original{};check(MH_CreateHook(code_+32*i,callbacks[i],&original)==MH_OK,"queue fixture hook");created_[i]=true;}
        check(MH_EnableHook(code_+32)==MH_OK,"queue fixture initial enable");
        check(MH_QueueEnableHook(code_)==MH_OK&&MH_QueueDisableHook(code_+32)==MH_OK,"queue fixture pending operations");
        unchanged();
    }
    ~QueuedHookIsolation(){
        for(unsigned i=0;i<2;++i)if(created_[i])MH_RemoveHook(code_+32*i);
        if(code_)VirtualFree(code_,0,MEM_RELEASE);
    }
    QueuedHookIsolation(const QueuedHookIsolation &)=delete;
    QueuedHookIsolation &operator=(const QueuedHookIsolation &)=delete;
    void unchanged() const {check(call(0)==1&&call(1)==4,"observer applied another owner's queued hooks");}
    void apply_and_check() const {
        unchanged();check(MH_ApplyQueued()==MH_OK,"queue fixture explicit apply");
        check(call(0)==3&&call(1)==2,"queued operations survived observer lifecycle");
    }
};
}
