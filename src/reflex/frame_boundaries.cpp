#include "frame_boundaries.hpp"
#include "../projection_binding/memory.hpp"
#include <windows.h>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <MinHook.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <ostream>

extern "C" {
void mhw_reflex_boundary_forward(void*, mhw::reflex::boundaries::Frame*);
void reflex_boundary_update(); void reflex_boundary_advance();
void reflex_boundary_publish(); void reflex_boundary_render();
void reflex_boundary_native_present(); void reflex_boundary_execute();
void reflex_boundary_present();
void reflex_boundary_keyboard(); void reflex_boundary_mouse();
void reflex_boundary_devices(); void reflex_boundary_jobs();
void reflex_boundary_submit(); void reflex_boundary_device_poll();
}
namespace mhw::reflex::boundaries {
namespace {
namespace pb = mhw::projection_binding;
constexpr unsigned open = 0x80000000U;
struct Gate { unsigned rva, bytes; const char* sha; unsigned kind; };
constexpr Gate gates[]{
    {0x1af2b60,4864,"9f6f5b3ac1a521ddd59351e41cc66c17dfc453ccd1b969afd4d59787bfef795d",update},
    {0x2293280,489,"576813bea02244e8970c592cf2f2a10e74335cb893c1402f560cf3eb4fa9ad64",advance},
    {0x229a080,1428,"961cf9969c28abc6b5b63ba9fb476978555eb188fdc173940d7ed81fc566a4e0",publish},
    {0x229b020,2420,"a4a09901e108fe38dabfb6d9df3a52f9d5ccd68a12b8b8e6f7a9ea671cbf5e90",render},
    {0x23a9ce0,1447,"aec3a83be592d010103ac1aa576648a043f30c8691a0b4405e04df8c4bb49f3c",native_present}
#ifdef MHW_REFLEX_INPUT_TRACE
    ,{0x485d90,606,"22a594985eff78bbbe182a98c59ee6ef8d6514b8d9c07b34183902dc736570cd",keyboard}
    ,{0x489800,1626,"5fee2e4f12148a329c4cad0b327c91bf6f7ef4090d485c33447c54f5a2f48398",mouse}
    ,{0x1b168c0,700,"919becfc62d4fac31843de4bf06eaffa2888fe89de7870cd4eeb893cb00ced34",devices}
    ,{0x11da7a0,177,"77f1dcb493093ec6923959c706275f8665f8af55cb14a20a3b4f42a272fae527",jobs}
    ,{0x23a7f50,69,"1183e1807b99ec2ada753fa147af8c59b1f99f29bbbf6176ed7479fd48e09a46",submit}
    ,{0x22a5040,1296,"2c2964093037a1ede0f3fddcbea44ff0c9029df1e66b12717c6468bdcdb04424",device_poll}
#endif
};
constexpr std::array<void(*)(), kinds> entries{
    reflex_boundary_update, reflex_boundary_advance, reflex_boundary_publish,
    reflex_boundary_render, reflex_boundary_native_present,
    reflex_boundary_execute, reflex_boundary_present
#ifdef MHW_REFLEX_INPUT_TRACE
    ,reflex_boundary_keyboard, reflex_boundary_mouse, reflex_boundary_devices,
    reflex_boundary_jobs, reflex_boundary_submit, reflex_boundary_device_poll
#endif
};
struct Stamp { std::uint64_t epoch{}, temporal{}; unsigned bank{}; bool valid{}; };
#ifdef MHW_REFLEX_INPUT_TRACE
struct Context { unsigned workers{}, pending{}; bool valid{}; };
struct Job { std::uintptr_t object{}, function{}; unsigned completed{}; };
struct Plan {
    std::array<Job,64> jobs{}; unsigned size{}; bool valid{}; std::uint64_t order{};
    std::atomic<bool> ready{};
};
std::array<Plan,2> plans{};
std::uintptr_t engine{}, keyboard_object{}, mouse_object{}, devices_object{};
#endif
struct Record {
    std::uint64_t enter{}, exit{}, qpc_enter{}, qpc_exit{};
    std::uintptr_t object{}, caller{};
    std::array<std::uintptr_t, 3> args{};
    Stamp before{}, after{};
#ifdef MHW_REFLEX_INPUT_TRACE
    Context context_before{}, context_after{};
    bool qualified{};
#endif
    std::uint64_t returned{};
    unsigned kind{}, thread{}, last_error{}, exception{};
    std::array<void*, 12> stack{};
    unsigned stack_count{};
    std::atomic<bool> ready{};
};
std::unique_ptr<Record[]> records;
std::array<void*, kinds> originals{};
std::array<std::uintptr_t, kinds> targets{};
std::array<std::array<std::uint64_t, 4>, kinds> heads{};
std::array<bool, kinds> created{}, enabled{};
std::array<std::atomic<unsigned>, kinds> visits{};
std::atomic<unsigned> gate{}, count{}, dropped{}, exceptions{};
std::atomic<std::uint64_t> order{};
std::uintptr_t image{}, renderer{}, backend{}, device{}, swapchain{};
std::uint64_t began{}, frequency{};
DXGI_SWAP_CHAIN_DESC swap_desc{};
LUID luid{};
Microsoft::WRL::ComPtr<ID3D12Device> device_pin;
Microsoft::WRL::ComPtr<IDXGISwapChain> swap_pin;
bool used{}, restored{};

bool method(std::uintptr_t object, unsigned slot, std::uintptr_t& target) noexcept {
    std::uintptr_t table{}; MEMORY_BASIC_INFORMATION m{};
    if (!pb::read(object, table) || !table || !pb::read(table+slot*8ULL, target) || !target ||
        VirtualQuery(reinterpret_cast<void*>(target), &m, sizeof(m)) != sizeof(m)) return false;
    const auto access = m.Protect & 255;
    return m.State == MEM_COMMIT && m.Type == MEM_IMAGE &&
        !(m.Protect & (PAGE_GUARD|PAGE_NOACCESS)) &&
        (access == PAGE_EXECUTE_READ || access == PAGE_EXECUTE_READWRITE || access == PAGE_EXECUTE_WRITECOPY);
}
bool system(std::uintptr_t object, unsigned slot, std::uintptr_t& target, const wchar_t* module) noexcept {
    if (!method(object, slot, target)) return false;
    MEMORY_BASIC_INFORMATION m{};
    return VirtualQuery(reinterpret_cast<void*>(target), &m, sizeof(m)) == sizeof(m) &&
        m.AllocationBase == GetModuleHandleW(module);
}
bool discover_com() noexcept {
    // Borrowed objects are validated on the worker before scoped COM ownership.
    std::uintptr_t ignored{};
    if (!system(device,0,ignored,L"D3D12Core.dll") || !system(device,2,ignored,L"D3D12Core.dll") ||
        !system(swapchain,0,ignored,L"dxgi.dll") || !system(swapchain,2,ignored,L"dxgi.dll") ||
        !system(swapchain,8,targets[present],L"dxgi.dll")) return false;
    __try {
        if (FAILED(reinterpret_cast<IUnknown*>(device)->QueryInterface(IID_PPV_ARGS(&device_pin))) ||
            reinterpret_cast<std::uintptr_t>(device_pin.Get()) != device ||
            FAILED(reinterpret_cast<IUnknown*>(swapchain)->QueryInterface(IID_PPV_ARGS(&swap_pin))) ||
            reinterpret_cast<std::uintptr_t>(swap_pin.Get()) != swapchain ||
            FAILED(swap_pin->GetDesc(&swap_desc))) return false;
        luid = device_pin->GetAdapterLuid();
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
    return true;
}
Stamp stamp() noexcept {
    Stamp s; std::uint64_t again{}; unsigned bank{};
    s.valid = pb::read(renderer+0x1f208,s.epoch) && pb::read(renderer+0x1f210,s.temporal) &&
        pb::read(renderer+0x1f200,s.bank) && s.bank<2 && pb::read(renderer+0x1f208,again) &&
        pb::read(renderer+0x1f200,bank) && again==s.epoch && bank==s.bank;
    return s;
}
#ifdef MHW_REFLEX_INPUT_TRACE
Context context() noexcept {
    Context c; std::uintptr_t current{};
    c.valid=pb::read(image+0x51c4148,current) && current==engine && pb::read(engine+0xcc,c.workers);
    for(unsigned n=0;n<4;++n) {
        unsigned char active{};
        c.valid=pb::read(engine+0xafda9+n,active) && active<2 && c.valid;
        if(active)c.pending|=1U<<n;
    }
    return c;
}
void plan(Plan& p,std::uint64_t order_value) noexcept {
    p.valid=true;p.order=order_value;
    // Four interleaved worker queues, sixteen bounded slots each. Capture
    // routing/lifetime metadata only, never any pointed-to input/job payload.
    for(unsigned n=0;n<16;++n)for(unsigned q=0;q<4;++q) {
        Job j; unsigned char completed{};
        const auto at=engine+0xafdb0+0x78ULL*n+0x18ULL*q;
        const bool ok=pb::read(at,j.object) && pb::read(at+8,j.function) && pb::read(at+16,completed);
        p.valid=ok && completed<2 && p.valid;
        j.completed=completed;
        if(ok && j.function)p.jobs[p.size++]=j;
    }
    p.ready.store(true,std::memory_order_release);
}
#endif
bool enter() noexcept {
    auto v = gate.load(std::memory_order_acquire);
    while (v & open) {
        if ((v & ~open) >= 4096) { stop(); return false; }
        if (gate.compare_exchange_weak(v,v+1,std::memory_order_acq_rel)) return true;
    }
    return false;
}
Record* begin(unsigned kind, const Frame& f) noexcept {
    if (kind >= kinds || !enter()) return nullptr;
    if (GetTickCount64()-began >= duration_ms) { stop(); gate.fetch_sub(1); return nullptr; }
    const auto index = count.fetch_add(1);
    if (index >= capacity) { dropped.fetch_add(1); stop(); gate.fetch_sub(1); return nullptr; }
    auto& r = records[index]; LARGE_INTEGER qpc{}; QueryPerformanceCounter(&qpc);
    r.qpc_enter = static_cast<std::uint64_t>(qpc.QuadPart); r.enter = order.fetch_add(1)+1;
    r.kind = kind; r.thread = GetCurrentThreadId(); r.object = f.gpr[1]; r.caller = f.caller;
    r.args = {f.gpr[2],f.gpr[3],f.gpr[4]}; r.before = stamp();
#ifdef MHW_REFLEX_INPUT_TRACE
    r.context_before=context();
    r.qualified=kind==keyboard?r.object==keyboard_object:kind==mouse?r.object==mouse_object:
        (kind==devices || kind==device_poll)?r.object==devices_object:kind==update?r.object==engine:
        (kind==advance || kind==publish || kind==render)?r.object==renderer:
        (kind==submit || kind==native_present)?r.object==backend:kind==present?r.object==swapchain:true;
    if(kind==keyboard || kind==mouse || kind==devices || kind==device_poll)r.args={};
#endif
    const auto visit=visits[kind].fetch_add(1);
    if (visit < 2)
        r.stack_count = CaptureStackBackTrace(2,static_cast<DWORD>(r.stack.size()),r.stack.data(),nullptr);
#ifdef MHW_REFLEX_INPUT_TRACE
    if(kind==publish && visit<plans.size())plan(plans[visit],r.enter);
#endif
    return &r;
}
void end(Record* r, const Frame& f, DWORD error, bool completed) noexcept {
    if (!r) return;
    LARGE_INTEGER qpc{}; QueryPerformanceCounter(&qpc);
    r->qpc_exit = static_cast<std::uint64_t>(qpc.QuadPart); r->exit = order.fetch_add(1)+1;
    r->after = stamp(); r->returned = f.gpr[0]; r->last_error = error;
#ifdef MHW_REFLEX_INPUT_TRACE
    r->context_after=context();
    if(r->kind==keyboard || r->kind==mouse || r->kind==devices || r->kind==device_poll)r->returned=0;
#endif
    if (!completed) { r->exception = 1; exceptions.fetch_add(1); stop(); }
    r->ready.store(true,std::memory_order_release); gate.fetch_sub(1,std::memory_order_release);
}
bool head_matches(unsigned i) noexcept {
    for (unsigned n=0;n<4;++n) {
        std::uint64_t v{};
        if (!pb::read(targets[i]+8ULL*n,v) || v != heads[i][n]) return false;
    }
    return true;
}
}
bool reserve(std::uintptr_t base, std::string& reason) {
    std::array<std::uintptr_t,10> points{};
    for (unsigned i=0;i<points.size();++i) points[i]=base+gates[i%std::size(gates)].rva;
    return pb::reserve_pool(points,reason);
}
bool install(std::uintptr_t base, std::string& reason) {
    if (used) { reason="already-used"; return false; } used=true; image=base;
    for (unsigned i=0;i<std::size(gates);++i) {
        const auto& g=gates[i];
        if (!pb::verify_code(base,g.rva,g.bytes,g.sha)) { reason="code-gate-"+std::to_string(i); return false; }
        targets[g.kind]=base+g.rva;
    }
    unsigned char api{}; LARGE_INTEGER f{};
    if (!pb::read(base+0x51cc080,api) || api!=1 || !pb::read(base+0x51c4480,renderer) || !renderer ||
        !pb::read(base+0x51c7050,backend) || !backend || !pb::read(backend+0x18,device) || !device ||
        !pb::read(backend+0x1470,swapchain) || !swapchain || !QueryPerformanceFrequency(&f)) {
        reason="renderer-anchors"; return false;
    }
    frequency=static_cast<std::uint64_t>(f.QuadPart);
#ifdef MHW_REFLEX_INPUT_TRACE
    std::uintptr_t linked{}, target{};
    if(!pb::read(base+0x51c4148,engine) || !engine || !pb::read(engine+0xaf7b0,linked) || linked!=renderer ||
        !pb::read(engine+0xaf7d0,keyboard_object) || !keyboard_object ||
        !method(keyboard_object,6,target) || target!=targets[keyboard] ||
        !pb::read(engine+0xaf7c8,mouse_object) || !mouse_object ||
        !method(mouse_object,6,target) || target!=targets[mouse] ||
        !pb::read(engine+0xaf8e0,devices_object) || !devices_object ||
        !method(devices_object,6,target) || target!=targets[devices] || !context().valid) {
        reason="input-subsystem-identity";return false;
    }
#endif
    if (!discover_com()) { reason="system-com-identity"; return false; }
    // Owned unsubmitted queue discovers the concrete ExecuteCommandLists method.
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC d{};
    if (FAILED(device_pin->CreateCommandQueue(&d,IID_PPV_ARGS(&queue))) ||
        !system(reinterpret_cast<std::uintptr_t>(queue.Get()),10,targets[execute],L"D3D12Core.dll")) {
        reason="execute-method"; return false;
    }
    records=std::make_unique<Record[]>(capacity);
    for (unsigned i=0;i<kinds;++i) {
        for (unsigned j=0;j<i;++j) if (targets[i]==targets[j]) { reason="aliased-target"; return false; }
        for (unsigned n=0;n<4;++n) if (!pb::read(targets[i]+8ULL*n,heads[i][n])) { reason="head-read"; return false; }
        const auto result=MH_CreateHook(reinterpret_cast<void*>(targets[i]),reinterpret_cast<void*>(entries[i]),&originals[i]);
        if (result!=MH_OK) { reason="create-hook-"+std::to_string(i)+"-"+std::to_string(result); return false; }
        created[i]=true;
    }
    // Queue the complete set and suspend other threads only once.
    for (unsigned i=0;i<kinds;++i) if (MH_QueueEnableHook(reinterpret_cast<void*>(targets[i]))!=MH_OK) {
        reason="queue-enable"; return false;
    }
    for (auto& e:enabled) e=true; // Include partial ApplyQueued failure in cleanup.
    if (MH_ApplyQueued()!=MH_OK) { reason="apply-enable"; return false; }
    began=GetTickCount64(); gate.store(open,std::memory_order_release); return true;
}
void stop() noexcept { gate.fetch_and(~open,std::memory_order_acq_rel); }
bool finish() noexcept {
    stop(); const auto until=GetTickCount64()+2000;
    while ((gate.load()&~open) && GetTickCount64()<until) Sleep(1);
    bool ok=(gate.load()&~open)==0;
    for (unsigned i=0;i<kinds;++i) if (created[i]) {
        const auto result=MH_QueueDisableHook(reinterpret_cast<void*>(targets[i]));
        ok=(result==MH_OK || result==MH_ERROR_DISABLED) && ok;
    }
    ok=(MH_ApplyQueued()==MH_OK) && ok;
    for (unsigned i=0;i<kinds;++i) if (created[i]) {
        const bool same=head_matches(i); enabled[i]=!same; ok=same && ok;
    }
    for (const auto& g:gates) if (created[g.kind]) {
        ok=pb::verify_code(image,g.rva,g.bytes,g.sha) && ok;
    }
    restored=ok;
    // Forwarding calls are not necessarily inside the recording gate. Keep
    // originals/trampolines/DLL alive even after hook removal; never MH_Remove.
    if (ok) { swap_pin.Reset(); device_pin.Reset(); }
    return ok;
}
void describe(std::ostream& log) {
    log<<"{\"event\":\"armed\",\"graphics_api\":\"D3D12\",\"renderer\":"<<renderer<<",\"backend\":"<<backend
        <<",\"device\":"<<device<<",\"swapchain\":"<<swapchain<<",\"format\":"<<swap_desc.BufferDesc.Format
        <<",\"width\":"<<swap_desc.BufferDesc.Width<<",\"height\":"<<swap_desc.BufferDesc.Height
        <<",\"buffers\":"<<swap_desc.BufferCount<<",\"adapter_luid_low\":"<<luid.LowPart<<",\"adapter_luid_high\":"<<luid.HighPart
        <<",\"qpc_frequency\":"<<frequency<<",\"tick\":"<<began<<",\"duration_ms\":"<<duration_ms<<",\"capacity\":"<<capacity
        <<",\"reflex_sleep\":false,\"sdk_loaded\":false,\"resolution_changed\":false,\"targets\":[";
    for(unsigned i=0;i<kinds;++i) { if(i)log<<','; log<<targets[i]; } log<<']';
#ifdef MHW_REFLEX_INPUT_TRACE
    log<<",\"engine\":"<<engine<<",\"keyboard_object\":"<<keyboard_object<<",\"mouse_object\":"<<mouse_object
        <<",\"devices_object\":"<<devices_object<<",\"input_values_recorded\":false,\"original_head_sha256\":[";
    for(unsigned i=0;i<kinds;++i) {
        if(i)log<<',';
        log<<'"'<<pb::bytes_sha256(heads[i].data(),32)<<'"';
    }
    log<<"],\"native_gates\":[";
    for(unsigned i=0;i<std::size(gates);++i) {
        if(i)log<<',';const auto& g=gates[i];
        log<<"{\"kind\":"<<g.kind<<",\"rva\":"<<g.rva<<",\"bytes\":"<<g.bytes<<",\"sha256\":\""<<g.sha<<"\"}";
    }
    log<<']';
#endif
    log<<"}\n"<<std::flush;
}
void write(std::ostream& log) {
    const auto size=(std::min)(count.load(),capacity);
    for(unsigned i=0;records && i<size;++i) {
        const auto& r=records[i]; if(!r.ready.load(std::memory_order_acquire))continue;
        log<<"{\"event\":\"boundary\",\"kind\":"<<r.kind<<",\"thread\":"<<r.thread<<",\"enter\":"<<r.enter<<",\"exit\":"<<r.exit
            <<",\"qpc_enter\":"<<r.qpc_enter<<",\"qpc_exit\":"<<r.qpc_exit<<",\"object\":"<<r.object<<",\"caller\":"<<r.caller
            <<",\"args\":["<<r.args[0]<<','<<r.args[1]<<','<<r.args[2]<<"],\"before\":["<<r.before.epoch<<','<<r.before.temporal<<','<<r.before.bank<<','<<r.before.valid
            <<"],\"after\":["<<r.after.epoch<<','<<r.after.temporal<<','<<r.after.bank<<','<<r.after.valid
            <<"],\"returned\":"<<r.returned<<",\"last_error\":"<<r.last_error<<",\"exception\":"<<r.exception<<",\"stack\":[";
        for(unsigned j=0;j<r.stack_count;++j) { if(j)log<<','; log<<reinterpret_cast<std::uintptr_t>(r.stack[j]); } log<<']';
#ifdef MHW_REFLEX_INPUT_TRACE
        const auto context_json=[&log](const Context& c) { log<<'['<<c.workers<<','<<c.pending<<','<<c.valid<<']'; };
        log<<",\"context_before\":";context_json(r.context_before);
        log<<",\"context_after\":";context_json(r.context_after);log<<",\"qualified\":"<<r.qualified;
#endif
        log<<"}\n";
    }
#ifdef MHW_REFLEX_INPUT_TRACE
    for(const auto& p:plans)if(p.ready.load(std::memory_order_acquire)) {
        log<<"{\"event\":\"worker_plan\",\"order\":"<<p.order<<",\"valid\":"<<p.valid<<",\"jobs\":[";
        for(unsigned n=0;n<p.size;++n) {
            if(n)log<<',';const auto& j=p.jobs[n];log<<'['<<j.object<<','<<j.function<<','<<j.completed<<']';
        }
        log<<"]}\n";
    }
#endif
    log<<"{\"event\":\"stopped\",\"records\":"<<size<<",\"drops\":"<<dropped.load()<<",\"exceptions\":"<<exceptions.load()
        <<",\"pending\":"<<(gate.load()&~open)<<",\"native_bytes_restored\":"<<restored<<",\"visits\":[";
    for(unsigned i=0;i<kinds;++i) { if(i)log<<','; log<<visits[i].load(); }
    log<<"],\"trampolines_retained\":true,\"dll_pinned\":true,\"game_reflex_implemented\":false}\n"<<std::flush;
}
}
extern "C" void mhw_reflex_boundary_dispatch(unsigned kind, mhw::reflex::boundaries::Frame* frame) noexcept {
    using namespace mhw::reflex::boundaries;
    // SEH cleanup preserves the game's exception behavior and Win32 error.
    const auto prior=GetLastError(); auto* record=begin(kind,*frame); SetLastError(prior);
    bool completed=false;
    __try { mhw_reflex_boundary_forward(originals[kind],frame); completed=true; }
    __finally { const auto error=GetLastError(); end(record,*frame,error,completed); SetLastError(error); }
}
