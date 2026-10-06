#include "sdk_runtime.hpp"
#include "../ngx_startup/startup.hpp"
#include "../temporal_bindings/submission.hpp"
#include <atomic>
namespace mhw::aa_control::sdk_runtime {
namespace {
std::filesystem::path runtime;
std::unique_ptr<SdkPass> pass;
std::atomic_flag busy=ATOMIC_FLAG_INIT;
bool finished{},saved{};
}
void configure(const std::filesystem::path &path){runtime=path;}
bool prepare(ID3D12Device *device,const std::array<ID3D12Resource *,3> &inputs,std::string &reason) noexcept {
    try {
        const auto *initialization=ngx_startup::borrowed_initialization();
        if(pass||!initialization||runtime.empty()){reason="AA SDK startup ownership";return false;}
        pass=std::make_unique<SdkPass>(device,inputs,runtime,*initialization);return true;
    }catch(const std::exception &e){reason=e.what();return false;}
}
bool record(unsigned i,ID3D12GraphicsCommandList *list,input_readback::Barrier barrier,
            const temporal_present::Bindings &bindings,temporal_bindings::jitter::HistorySample history) noexcept {
    if(!pass||busy.test_and_set())return false;
    // Bounded diagnostic synchronization: reuse private images/history only
    // after the previous native submission is demonstrably complete/retired.
    // This works across queues without assuming a global Execute order.
    namespace sub=temporal_bindings::submission;
    bool prior=i==0;const auto until=GetTickCount64()+250;
    while(!prior&&sub::healthy()&&GetTickCount64()<until){sub::poll();prior=sub::completed_receipt(i-1);if(!prior)Sleep(1);}
    const bool ok=prior&&pass->record(i,list,barrier,bindings,history,prior);
    busy.clear();return ok;
}
bool finish(bool all_completed_retired) noexcept {
    if(!pass)return true;if(finished)return true;
    ID3D12Fence *fence{};
    if(!all_completed_retired||!temporal_bindings::submission::completed_receipt(7,&fence))return false;
    finished=pass->finish(fence,true);fence->Release();return finished;
}
bool save(const std::wstring &prefix){saved=pass&&finished&&pass->save(prefix);return saved;}
void write(std::ostream &out){if(pass){pass->write(out);if(saved)pass.reset();}}
}
