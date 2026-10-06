#include "post_queue.hpp"
#ifdef MHW_AA_SDK
#include "sdk_runtime.hpp"
#include "../ngx_startup/startup.hpp"
#endif
#ifdef MHW_AA_PROJECTION
#include "projection_link.hpp"
#include "../temporal_bindings/jitter.hpp"
#endif
#ifdef MHW_AA_OWNED_PASS
#include "owned_runtime.hpp"
#endif
#ifdef MHW_AA_DEFERRED
#include "deferred_runtime.hpp"
#endif
#include "../projection_binding/memory.hpp"
#ifdef MHW_AA_INPUT_READBACK
#include "input_readback.hpp"
#endif
#include <windows.h>
#include <MinHook.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
extern "C" void aa_schedule_entry();
extern "C" void mhw_binding_forward(void *,mhw::projection_binding::Frame *);
namespace mhw::projection_binding {bool discover(std::uintptr_t,Runtime &,std::string &);}
namespace {
namespace aa=mhw::aa_control;namespace pb=mhw::projection_binding;
#ifdef MHW_AA_SDK
constexpr const char *version="0.7.0",*experiment="independent-aa-sdk",*event_prefix="Local\\MhwAaSdk-v1-";
#elif defined(MHW_AA_PROJECTION)
constexpr const char *version="0.6.1",*experiment="independent-aa-projection-bindings",*event_prefix="Local\\MhwAaProjection-v1-";
#elif defined(MHW_AA_INPUT_READBACK)
constexpr const char *version="0.5.0",*experiment="independent-aa-input-readback",*event_prefix="Local\\MhwAaInputs-v1-";
#elif defined(MHW_AA_GPU_TRACE)
constexpr const char *version="0.4.0",*experiment="independent-aa-gpu-trace",*event_prefix="Local\\MhwAaGpu-v1-";
#elif defined(MHW_AA_DEFERRED)
constexpr const char *version="0.3.0",*experiment="independent-aa-deferred-marker",*event_prefix="Local\\MhwAaDeferred-v1-";
#elif defined(MHW_AA_OWNED_PASS)
constexpr const char *version="0.2.0",*experiment="independent-aa-owned-in-place",*event_prefix="Local\\MhwAaOwned-v1-";
#else
constexpr const char *version="0.1.0",*experiment="independent-aa-post-queue-boundary",*event_prefix="Local\\MhwAaBoundary-v1-";
#endif
constexpr unsigned sample_limit=8;
HMODULE module{};std::uintptr_t image{};void *original{};
std::atomic<bool> accepting{},busy{};std::atomic<unsigned> pending{},fault{},count{},visits{};
struct Sample {aa::PostQueue before,after;std::uint64_t tick{};unsigned thread{},projection_async{};bool entered{},returned{},complete{},same_frame{};};
std::array<Sample,sample_limit> samples;
bool enabled{};
std::uint64_t first_disable_tick{};
#ifdef MHW_AA_PROJECTION
namespace jitter=mhw::temporal_bindings::jitter;
bool jitter_prepared{},jitter_drained{true};
#endif
bool read(void *,std::uintptr_t at,std::span<std::byte> out) {
    return mhw::bridge_probe::read_bounded_scalar(nullptr,at,out)==mhw::bridge_probe::ScalarReadResult::success;
}
const mhw::temporal_bindings::Reader reader{nullptr,read};
void fail(unsigned why){unsigned zero{};fault.compare_exchange_strong(zero,why);accepting=false;
#ifdef MHW_AA_PROJECTION
    jitter::stop();
#endif
}
unsigned before(const pb::Frame &frame){
    if(!accepting||frame.caller!=image+0x228e92d)return sample_limit;
#ifdef MHW_AA_PROJECTION
    if(!jitter::healthy()){fail(8);return sample_limit;}
    // Initial calls may precede the selected projection baseline. Never enqueue
    // an unassociated frame or skip within the admitted sequence.
    if(!jitter::wanted(count.load()))return sample_limit;
#endif
    ++visits;
    if(busy.exchange(true)){fail(1);return sample_limit;}
    const auto index=count.load();
    if(index>=sample_limit){accepting=false;busy=false;return sample_limit;}
    auto &s=samples[index];aa::PostQueue again;
    s.tick=GetTickCount64();s.thread=GetCurrentThreadId();
    s.entered=aa::read_post_queue(reader,image,frame.gpr[1],frame.gpr[2],frame.gpr[3],s.before)&&
        aa::read_post_queue(reader,image,frame.gpr[1],frame.gpr[2],frame.gpr[3],again)&&again==s.before;
    if(!s.entered){fail(2);++count;busy=false;return sample_limit;}
#ifdef MHW_AA_PROJECTION
    unsigned char async_mode{};mhw::temporal_bindings::Snapshot projection;
    if(!pb::read(s.before.renderer+0x93,async_mode)||!aa::projection_link(s.before,async_mode,projection)||
       !jitter::acknowledge(index,projection)){fail(8);++count;busy=false;return sample_limit;}
    s.projection_async=async_mode;
#endif
#ifdef MHW_AA_OWNED_PASS
    if(!aa::owned::insert(index,s.before)){fail(4);++count;busy=false;return sample_limit;}
#endif
    return index;
}
void after(unsigned index,bool returned){
    if(index>=sample_limit)return;
    auto &s=samples[index];aa::PostQueue again;const auto &b=s.before;
#ifdef MHW_AA_OWNED_PASS
    if(!aa::owned::remove(index,returned))fail(5);
#endif
    s.returned=returned;
    s.complete=returned&&aa::read_post_queue(reader,image,b.manager,b.context,b.view,s.after)&&
        aa::read_post_queue(reader,image,b.manager,b.context,b.view,again)&&again==s.after;
    s.same_frame=s.complete&&aa::same_post_frame(b,s.after);
    if(!s.complete||!s.same_frame)fail(3);
    if(++count==sample_limit)accepting=false;
    busy=false;
}
void dispatch(pb::Frame *frame){
    const auto index=before(*frame);bool returned=false;
    __try {mhw_binding_forward(original,frame);returned=true;}
    __finally {after(index,returned);}
}
std::wstring wide(const std::string &s){const auto n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);
    if(!n)return {};std::wstring w(static_cast<std::size_t>(n),L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),w.data(),n);return w;}
std::string quote(const std::string &s){std::ostringstream out;out<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')out<<'\\'<<c;
    else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c);else out<<c;}return out.str()+'"';}
struct Handle {HANDLE value{};~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
void queue_json(std::ostream &out,const aa::PostQueue &q){
    out<<"{\"manager\":"<<q.manager<<",\"context\":"<<q.context<<",\"view\":"<<q.view<<",\"renderer\":"<<q.renderer
       <<",\"frame\":"<<q.frame<<",\"epoch\":"<<q.epoch<<",\"bank\":"<<q.bank<<",\"count\":"<<q.count<<",\"packets\":"<<q.packet_count
       <<",\"enabled\":"<<unsigned(q.enabled)<<",\"collecting\":"<<unsigned(q.collecting)<<",\"taa\":"<<unsigned(q.taa)<<",\"fxaa\":"<<unsigned(q.fxaa)<<",\"rectangle\":[";
    for(unsigned i=0;i<4;++i)out<<(i?",":"")<<q.rectangle[i];
    out<<"],\"scratch\":["<<q.scratch[0]<<','<<q.scratch[1]<<"],\"attachments\":["<<q.attachments[0]<<','<<q.attachments[1]<<"],\"nodes\":[";
    for(unsigned i=0;i<q.count&&i<aa::post_queue_limit;++i){const auto &n=q.nodes[i];out<<(i?",":"")<<"{\"descriptor\":"<<n.descriptor
        <<",\"unit\":"<<n.unit<<",\"vtable\":"<<n.vtable<<",\"phase\":"<<n.phase<<",\"order\":"<<n.order<<",\"input\":"<<n.input<<",\"output\":"<<n.output
        <<",\"flags\":["<<unsigned(n.flags[0])<<','<<unsigned(n.flags[1])<<','<<unsigned(n.flags[2])<<"]}";}
    out<<"]}";
}
bool finish(){
    accepting=false;
#ifdef MHW_AA_PROJECTION
    jitter::stop();
#endif
    const auto deadline=GetTickCount64()+2000;
    while(pending&&GetTickCount64()<deadline){
#ifdef MHW_AA_DEFERRED
        aa::deferred::finished(); // Poll while admitted scheduler work drains.
#endif
        Sleep(1);
    }
#ifdef MHW_AA_DEFERRED
    if(pending||busy)return false;
    const auto consumer_deadline=GetTickCount64()+2000;
    while(!aa::deferred::finished()&&GetTickCount64()<consumer_deadline)Sleep(1);
    const bool consumed=aa::deferred::finished()&&!aa::deferred::fault();
    // Nothing can enlarge the journal during the following MinHook operations.
    // Keep the actual completion result: closing observation is not success.
    if(!aa::deferred::close_observation())return false;
#endif
    if(!first_disable_tick)first_disable_tick=GetTickCount64();
    if(enabled){if(MH_DisableHook(reinterpret_cast<void *>(image+0x24f1fc0))!=MH_OK)return false;enabled=false;}
    const auto drain_deadline=GetTickCount64()+2000;
    while(pending&&GetTickCount64()<drain_deadline)Sleep(1);
#ifdef MHW_AA_PROJECTION
    if(jitter_prepared){
        const bool complete=jitter::finish();jitter_drained=jitter::settled()&&jitter::hooks_disabled();
        if(!complete)fail(8);
        if(!jitter_drained)return false;
    }
#endif
#ifdef MHW_AA_DEFERRED
    if(pending||busy)return false;
    const bool consumer_drained=aa::deferred::stop();
    if(!consumed||aa::deferred::fault())fail(6);
    if(!consumer_drained)return false;
#endif
    // Original trampoline and pinned DLL stay alive even after capture closes.
    return !pending&&!busy
#ifdef MHW_AA_OWNED_PASS
        &&aa::owned::detached()
#endif
        ;
}
DWORD WINAPI worker(void *) noexcept {
    HMODULE pinned{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&worker),&pinned))return 1;
    try {
        wchar_t dll_buffer[32768]{},host_buffer[32768]{};
        auto n=GetModuleFileNameW(module,dll_buffer,static_cast<DWORD>(std::size(dll_buffer)));
        auto h=GetModuleFileNameW(nullptr,host_buffer,static_cast<DWORD>(std::size(host_buffer)));
        if(!n||n>=std::size(dll_buffer)||!h||h>=std::size(host_buffer))return 2;
        const std::filesystem::path dll(dll_buffer),host(host_buffer);auto sidecar=dll;sidecar+=L".output-dir";
        if(!std::filesystem::exists(sidecar)||std::filesystem::file_size(sidecar)>4096)return 3;
        std::ifstream file(sidecar);std::string folder;std::getline(file,folder);if(!folder.empty()&&folder.back()=='\r')folder.pop_back();
        if(!std::filesystem::path(wide(folder)).is_absolute())return 4;
        const auto output=std::filesystem::weakly_canonical(std::filesystem::path(wide(folder)));
        if(!std::filesystem::is_directory(output)||output==std::filesystem::weakly_canonical(host.parent_path()))return 4;
        image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));std::string reason;
        if(!pb::host_identity(image,reason))return 5;
        FILETIME ct{},et{},kt{},ut{};if(!GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut))return 6;
        const auto creation=(std::uint64_t(ct.dwHighDateTime)<<32)|ct.dwLowDateTime;const auto pid=GetCurrentProcessId();
        const auto name="aa-boundary-"+std::to_string(pid)+"-"+std::to_string(creation);const auto path=output/(name+".jsonl");
        {Handle fresh{CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr)};if(fresh.value==INVALID_HANDLE_VALUE)return 7;}
        std::ofstream log(path,std::ios::binary|std::ios::app);if(!log)return 8;
        log<<std::boolalpha<<"{\"event\":\"start\",\"schema\":1,\"version\":"<<quote(version)<<",\"pid\":"<<pid<<",\"creation\":"<<creation<<",\"base\":"<<image
           <<",\"host_sha256\":"<<quote(pb::host_sha)<<",\"host_version\":\"0.0.0.0\",\"dll_sha256\":"<<quote(pb::file_sha256(dll.wstring()))
           <<",\"experiment\":"<<quote(experiment)<<",\"sample_limit\":8"
#ifdef MHW_AA_PROJECTION
           <<",\"write_scope\":\"scoped-post-queue-and-one-empty-packet\",\"aa_option_writes\":0,\"jitter_writes\":12,\"pass_phase\":15,\"pass_order\":2,\"in_place\":true,\"projection_receipts\":8,\"jitter_stimuli\":6,\"binding_snapshots\":8,\"shutdown_order\":\"drain-before-disable-v1\""
#elif defined(MHW_AA_DEFERRED)
           <<",\"write_scope\":\"scoped-post-queue-and-one-empty-packet\",\"aa_option_writes\":0,\"jitter_writes\":0,\"pass_phase\":15,\"pass_order\":2,\"in_place\":true"
#elif defined(MHW_AA_OWNED_PASS)
           <<",\"write_scope\":\"scoped-post-queue-only\",\"aa_option_writes\":0,\"jitter_writes\":0,\"pass_phase\":15,\"pass_order\":2,\"in_place\":true"
#else
           <<",\"game_data_writes\":0"
#endif
#ifdef MHW_AA_GPU_TRACE
           <<",\"private_queue_signals\":8,\"gpu_state_trace\":true"
#endif
           <<",\"private_gpu_commands\":"
#ifdef MHW_AA_SDK
           <<-1<<",\"readback_receipts\":[0,3,7],\"read_point\":\"first-paired-transition-after-marker\",\"sdk_evaluations\":8,\"sdk_output\":\"private-only\""
#elif defined(MHW_AA_INPUT_READBACK)
           <<27<<",\"readback_receipts\":[0,3,7],\"read_point\":\"first-paired-transition-after-marker\""
#else
           <<0
#endif
           <<",\"sdk_used\":"
#ifdef MHW_AA_SDK
           <<true
#else
           <<false
#endif
           <<"}\n"<<std::flush;
        const auto prefix=std::string(event_prefix)+std::to_string(pid)+"-"+std::to_string(creation)+"-";
        Handle stop{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"0").c_str())};const bool collision=GetLastError()==ERROR_ALREADY_EXISTS;
        Handle arm{CreateEventW(nullptr,FALSE,FALSE,wide(prefix+"1").c_str())};
        if(!stop.value||!arm.value||collision||GetLastError()==ERROR_ALREADY_EXISTS)return 9;
        bool reserved=pb::reserve_startup(image,reason);
#ifdef MHW_AA_SDK
        if(reserved){
            reserved=mhw::ngx_startup::run(module,version,true);
            if(!reserved)reason="AA SDK startup transaction";
            else {std::ifstream config(output/L"sdk-runtime-dir.txt");std::string sdk_path;std::getline(config,sdk_path);
                if(!sdk_path.empty()&&sdk_path.back()=='\r')sdk_path.pop_back();
                aa::sdk_runtime::configure(std::filesystem::canonical(wide(sdk_path)));}
        }
#endif
        log<<"{\"event\":\"ready\",\"reserved\":"<<reserved<<",\"reason\":"<<quote(reason)<<",\"hooks_installed\":false}\n"<<std::flush;
        if(!reserved)return 10;
        const HANDLE events[]{stop.value,arm.value};
        if(WaitForMultipleObjects(2,events,FALSE,1800000)!=WAIT_OBJECT_0+1){log<<"{\"event\":\"stopped\",\"armed\":false}\n"<<std::flush;return 0;}
        pb::Runtime runtime;
        bool prepared=pb::discover(image,runtime,reason)&&
            pb::verify_code(image,0x24f1fc0,835,"1ec618caf8d1d455cc12fc12c57889b73e1073074faf04e82364fba10d01e190")&&
            pb::verify_code(image,0x228e770,528,"33ae7df95058ac2e1ce8d920ff89c62d664068a975931bb10afb95ea88f00b97");
#ifdef MHW_AA_OWNED_PASS
        prepared=prepared&&pb::verify_code(image,0x24f1620,808,"199d3cb4f03566dcc21aaf72c55e562f568595e3e3954f5987890bdb2c33eaa7");
#ifdef MHW_AA_DEFERRED
        prepared=prepared&&aa::deferred::prepare(image,runtime);
        aa::owned::configure(image,runtime.device,aa::deferred::publish);
#else
        aa::owned::configure(image,runtime.device);
#endif
#endif
#ifdef MHW_AA_PROJECTION
        if(prepared){
            std::uintptr_t renderer{},backend{};
            prepared=pb::read(image+0x51c4480,renderer)&&renderer&&pb::read(renderer+0x78,backend)&&backend&&
                jitter::prepare({image,renderer,backend,runtime.device},reason);
            jitter_prepared=prepared;
        }
#endif
        if(!prepared&&reason.empty())reason="post-queue-code-gate";
        log<<"{\"event\":\"prepared\",\"ok\":"<<prepared<<",\"reason\":"<<quote(reason)<<",\"graphics_api\":\"D3D12\",\"adapter_low\":"<<runtime.luid_low
           <<",\"adapter_high\":"<<runtime.luid_high<<",\"device\":"<<runtime.device<<",\"backend\":"<<runtime.backend<<",\"swapchain_format\":"<<runtime.format<<",\"width\":"<<runtime.width<<",\"height\":"<<runtime.height<<"}\n"<<std::flush;
        if(!prepared){finish();return 11;}
        if(MH_CreateHook(reinterpret_cast<void *>(image+0x24f1fc0),reinterpret_cast<void *>(aa_schedule_entry),&original)!=MH_OK){finish();return 12;}
#ifdef MHW_AA_DEFERRED
        if(!aa::deferred::enable())return 12;
#endif
        accepting=true;
        if(MH_EnableHook(reinterpret_cast<void *>(image+0x24f1fc0))!=MH_OK){accepting=false;finish();return 13;}enabled=true;
#ifdef MHW_AA_PROJECTION
        if(!jitter::begin(GetTickCount64(),reason)){finish();return 13;}
#endif
        log<<"{\"event\":\"armed\",\"hooks\":"
#ifdef MHW_AA_PROJECTION
           <<25
#elif defined(MHW_AA_GPU_TRACE)
           <<6
#elif defined(MHW_AA_DEFERRED)
           <<2
#else
           <<1
#endif
           <<"}\n"<<std::flush;
        const auto deadline=GetTickCount64()+5000;
        while(accepting&&GetTickCount64()<deadline)if(WaitForSingleObject(stop.value,20)!=WAIT_TIMEOUT)break;
        const bool drained=finish();
#ifdef MHW_AA_INPUT_READBACK
        if(drained&&!fault){
            const auto image_prefix=output/("aa-input-"+std::to_string(pid)+"-"+std::to_string(creation));
            if(!aa::input_readback::save(image_prefix.wstring()))fail(7);
#ifdef MHW_AA_SDK
            if(!aa::sdk_runtime::save((output/("aa-sdk-"+std::to_string(pid)+"-"+std::to_string(creation))).wstring()))fail(9);
#endif
        }
#endif
        if(drained)for(unsigned i=0;i<count&&i<sample_limit;++i){const auto &s=samples[i];log<<"{\"event\":\"sample\",\"index\":"<<i<<",\"tick\":"<<s.tick<<",\"thread\":"<<s.thread
            <<",\"entered\":"<<s.entered<<",\"returned\":"<<s.returned<<",\"complete\":"<<s.complete<<",\"same_frame\":"<<s.same_frame<<",\"before\":";
            queue_json(log,s.before);log<<",\"after\":";queue_json(log,s.after);
#ifdef MHW_AA_PROJECTION
            log<<",\"projection_async\":"<<s.projection_async;
#endif
#ifdef MHW_AA_OWNED_PASS
            log<<",\"owned\":";aa::owned::write(log,i,queue_json);
#endif
            log<<"}\n";}
#ifdef MHW_AA_DEFERRED
        if(drained)aa::deferred::write(log);
#endif
#ifdef MHW_AA_INPUT_READBACK
        if(drained)aa::input_readback::write(log);
#endif
#ifdef MHW_AA_PROJECTION
        if(drained){log<<"{\"event\":\"projection\",\"jitter\":";jitter::write(log);log<<"}\n";}
#endif
#ifdef MHW_AA_SDK
        if(drained)aa::sdk_runtime::write(log);
#endif
        log<<"{\"event\":\"stopped\",\"armed\":true,\"samples\":"<<count.load()<<",\"visits\":"<<visits.load()<<",\"fault\":"<<fault.load()
           <<",\"pending\":"<<pending.load()<<",\"drained\":"<<drained<<",\"hooks_disabled\":"<<(!enabled
#ifdef MHW_AA_DEFERRED
           &&aa::deferred::disabled()
#endif
#ifdef MHW_AA_PROJECTION
           &&jitter_drained
#endif
           )<<",\"dll_pinned\":true,\"first_disable_tick\":"<<first_disable_tick
#ifdef MHW_AA_OWNED_PASS
           <<",\"owned_descriptors_detached\":"<<aa::owned::detached()
#endif
           <<"}\n"<<std::flush;
        return drained&&!fault&&count==sample_limit?0:14;
    }catch(...){finish();return 15;}
}
}
extern "C" void mhw_aa_schedule(mhw::projection_binding::Frame *frame){
    ++pending;__try{dispatch(frame);}__finally{--pending;}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){module=instance;DisableThreadLibraryCalls(instance);const auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;
}
