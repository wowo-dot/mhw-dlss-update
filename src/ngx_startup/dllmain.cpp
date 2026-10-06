#include "startup.hpp"
#include <windows.h>
namespace {
HMODULE module{};
DWORD WINAPI worker(void *) noexcept {return mhw::ngx_startup::run(module,"1.12.1")?0:1;}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);const auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
    return TRUE;
}
