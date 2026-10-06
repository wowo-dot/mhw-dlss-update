// Exercise production ASM and activation against owned native-shaped objects.
#include "../src/continuous_dlaa/activation.cpp"
#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <sstream>
namespace aa=mhw::continuous_dlaa::activation;
extern "C" {
unsigned activation_table_fixture(std::uintptr_t,std::uintptr_t);
unsigned activation_sharpen_fixture(std::uintptr_t,std::uintptr_t);
void activation_table_site();void activation_table_resume();
void activation_sharpen_site();void activation_sharpen_resume();
}
void check(bool yes,const char *message){if(!yes){std::cerr<<message<<'\n';std::exit(1);}}
template<class T>void put(std::uintptr_t p,T v){std::memcpy(reinterpret_cast<void *>(p),&v,sizeof(v));}
template<class T>T get(std::uintptr_t p){T v{};check(aa::pb::read(p,v),"read owned fixture");return v;}
struct Block{
    std::uintptr_t p;explicit Block(std::size_t n):p(reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,n,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))){check(p!=0,"allocate");}
    ~Block(){VirtualFree(reinterpret_cast<void *>(p),0,MEM_RELEASE);}
};
using Call=std::uint64_t(*)(std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t);
std::uintptr_t unit{},table{},ctx{},view{},other_view{},worker_ctx{},queue{},registry{},competitor{},dummy{},camera{};
unsigned choice{},action{},taa_calls{},native_calls{},jitter_calls{},unrelated_calls{},registration_calls{};
unsigned completions{},expected_completions{};bool queue_tail_completed{};
namespace mhw::scaled_temporal::stream_route {
void post_queue_complete(std::uintptr_t manager,std::uintptr_t context,std::uintptr_t incoming,std::uintptr_t taa) noexcept {
    check(manager==queue&&context==ctx&&incoming==view&&taa==unit,"completion preserves incoming queue identity across native volatile registers");
    check(queue_tail_completed&&aa::owned_context(context)&&aa::queued_taa==unit,"completion only after native queue tail in owning frame");
    queue_tail_completed=false;++completions;check(completions==expected_completions,"no early, nested or exceptional completion");
}
}
constexpr std::uint64_t answer=0x987654321abcdef0ULL;
void native_register(std::uintptr_t layer,unsigned stage,unsigned order){
    ++registration_calls;auto count=get<unsigned>(queue+0x3c);check(count<16,"native helper capacity");
    put(layer+0x140,stage);put(layer+0x144,order);put(queue+0x40+8*count,layer+0x140);put(queue+0x3c,count+1);
}
std::uint64_t native_taa(std::uintptr_t layer,std::uintptr_t context,std::uintptr_t,std::uintptr_t){
    check(layer==unit,"TAA layer");
    if(get<unsigned char>(queue+0x39)){native_register(layer,15,2);return answer;}
    if(!aa::queued_taa||context!=ctx||action==8){
        ++native_calls;check(activation_table_fixture(table,context)==(choice&1)&&activation_sharpen_fixture(unit,context)==1,"unowned execute retains native comparisons");return answer;
    }
    ++taa_calls;
    if(action==6)RaiseException(0xe0424242,0,0,nullptr);
    if(action==7)aa::stop();
    if(action==9){action=8;reinterpret_cast<Call>(mhw_aa_taa_entry)(unit,ctx,7,8);action=9;}
    if(action==17){action=12;reinterpret_cast<Call>(mhw_aa_queue_entry)(queue,ctx,view,0x12345678);action=17;}
    check(activation_table_fixture(table,ctx)==1,"owned queue enables native temporal preparation");
    check(activation_sharpen_fixture(unit,ctx)==0,"owned queue uses unsharpened current output");
    unsigned other_table=9,other_sharpen=9;
    std::thread unrelated([&]{other_table=activation_table_fixture(table,ctx);other_sharpen=activation_sharpen_fixture(unit,ctx);});unrelated.join();
    check(other_table==(choice&1)&&other_sharpen==1,"unrelated thread cannot inherit comparison scope");
    return answer;
}
std::uint64_t native_queue(std::uintptr_t q,std::uintptr_t context,std::uintptr_t incoming,std::uintptr_t marker){
    queue_tail_completed=false;
    check(q==queue&&context==ctx&&incoming==view&&marker==0x12345678,"queue ABI preserved");
    put(queue+0x39,static_cast<unsigned char>(0));
    if(action==10)RaiseException(0xe0424242,0,0,nullptr);
    if(action==11){action=12;reinterpret_cast<Call>(mhw_aa_queue_entry)(queue,ctx,view,marker);action=11;}
    auto count=get<unsigned>(queue+0x3c);
    if(count>16)return answer; // Deliberate corrupt-count fixture: no native OOB.
    auto entries=reinterpret_cast<std::uintptr_t *>(queue+0x40);
    std::sort(entries,entries+count,[](auto a,auto b){return (get<unsigned>(a)<<27|get<unsigned>(a+4))<(get<unsigned>(b)<<27|get<unsigned>(b+4));});
    for(unsigned i=0;i<count;++i){auto entry=entries[i];if(!get<unsigned char>(entry+0x20)){
        auto owner=get<std::uintptr_t>(entry+8);
        if(owner==unit)reinterpret_cast<Call>(mhw_aa_taa_entry)(unit,ctx,7,8);
        else ++unrelated_calls;
    }}
    queue_tail_completed=true;
    if(aa::queued_taa&&aa::requested.load()&&!aa::fault.load()&&aa::owned_context(context))++expected_completions;
    return answer;
}
std::uint64_t native_jitter(std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t){++jitter_calls;return answer;}
std::uint64_t native_frame(std::uintptr_t cam,std::uintptr_t context,std::uintptr_t incoming,std::uintptr_t display){
    check(cam==camera&&context==ctx&&incoming==view&&display==0,"camera ABI");
    check(aa::owns_projection(ctx,view)==(aa::frame_owned&&aa::requested.load()&&!aa::fault.load()),"projection requires owned frame");
    check(!aa::owns_projection(worker_ctx,view)&&!aa::owns_projection(ctx,other_view),"projection scope rejects foreign identity");
    bool foreign_projection=true;
    std::thread projection_worker([&]{foreign_projection=aa::owns_projection(ctx,view);});projection_worker.join();
    check(!foreign_projection,"projection ownership is thread local");
    check(activation_table_fixture(table,ctx)==(choice&1)&&activation_sharpen_fixture(unit,ctx)==1,"outer frame alone grants no comparison authority");
    put(ctx+0x1d0,action==4?other_view:(action==5?std::uintptr_t{}:view));
    put(queue+0x39,static_cast<unsigned char>(1));put(queue+0x3c,0U);
    if(action==3)RaiseException(0xe0424242,0,0,nullptr);
    // Native worker's prefilter precedes callback. Off never reaches Execute.
    std::thread worker([]{if(get<unsigned>(unit+0x14)&2)reinterpret_cast<Call>(mhw_aa_taa_entry)(unit,worker_ctx,7,8);});worker.join();
    if(action==13){native_register(competitor,15,1);}
    if(action==14){for(unsigned i=0;i<16;++i)put(queue+0x40+8*i,dummy);put(queue+0x3c,16U);}
    if(action==15){native_register(unit,15,2);native_register(unit,15,2);}
    if(action==16)put(queue+0x3c,17U);
    if(action==1)aa::stop();
    if(action==2){choice=3;put(table+0xe9e4,static_cast<std::uint16_t>(0x101));}
    return reinterpret_cast<Call>(mhw_aa_queue_entry)(queue,ctx,view,0x12345678);
}
void run(){check(!aa::owns_projection(ctx,view),"no projection ownership before entry");check(reinterpret_cast<Call>(mhw_aa_frame_entry)(camera,ctx,view,0)==answer,"original queue/frame return preserved");check(aa::drained()&&!aa::queued_taa&&!mhw_aa_thread,"all scopes drain");check(!aa::owns_projection(ctx,view),"no projection ownership after return");}
void exception_frame(unsigned expected_fault){bool caught=false;
    __try{reinterpret_cast<Call>(mhw_aa_frame_entry)(camera,ctx,view,0);}
    __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){caught=true;}
    check(caught&&aa::fault==expected_fault&&aa::drained()&&!aa::queued_taa&&!mhw_aa_thread,"native exception propagated and scopes cleaned");
}
void jump(std::uintptr_t where,void *fn){
    const unsigned char opcode[]{0x48,0xb8};std::memcpy(reinterpret_cast<void *>(where),opcode,2);put(where+2,reinterpret_cast<std::uintptr_t>(fn));
    put(where+10,static_cast<std::uint16_t>(0xe0ff));DWORD old{};check(VirtualProtect(reinterpret_cast<void *>(where),12,PAGE_EXECUTE_READ,&old)!=0,"protect owned stub");
    check(FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void *>(where),12)!=0,"flush stub");
}
std::string receipt(){std::ostringstream s;s<<std::boolalpha;aa::write(s);return s.str();}
int main(){
    Block image(0x55d3000),r(0x20000),backend(0x43000),cam(0x100),v(0x100),alternate(0x100),options(0xea00),layer(0x200),reg(0x4b00),q(0xd0),worker(0x1000),comp(0x200),d(0x30);
    aa::image=image.p;aa::renderer=r.p;ctx=r.p+0x710;table=options.p;unit=layer.p;view=v.p;other_view=alternate.p;camera=cam.p;
    registry=reg.p;queue=q.p;worker_ctx=worker.p;competitor=comp.p;dummy=d.p;
    put(image.p+0x51cc080,static_cast<unsigned char>(1));put(image.p+0x51c4480,r.p);put(image.p+0x51c4400,camera);
    put(image.p+0x51c4368,table);put(camera,image.p+0x350d188);put(r.p+0x1f450,view);put(r.p+0x78,backend.p);
    put(image.p+0x51238c8,registry);put(registry,image.p+0x33fcde8);put(registry+0x15d0,unit);
    put(image.p+0x51cc238,queue);put(queue+0x38,static_cast<unsigned char>(1));put(queue+0xc0,0x1122334455667788ULL);
    put(unit,image.p+0x351bda8);put(unit+0xc,8U);put(unit+0x14,0x10U);put(unit+0x48,1U);put(unit+0x1c,1U);
    put(ctx+0x152,static_cast<std::uint16_t>(1));put(unit+0x148,unit);put(unit+0x187,static_cast<unsigned char>(1));
    put(competitor+0x148,competitor);put(dummy+0x20,static_cast<unsigned char>(1));
    jump(image.p+0x24c9240,reinterpret_cast<void *>(native_register));
    aa::originals[0]=reinterpret_cast<void *>(native_frame);aa::originals[1]=reinterpret_cast<void *>(native_taa);aa::originals[7]=reinterpret_cast<void *>(native_queue);
    for(unsigned i=2;i<5;++i)aa::originals[i]=reinterpret_cast<void *>(native_jitter);
    check(MH_Initialize()==MH_OK,"MinHook init");
    check(MH_CreateHook(reinterpret_cast<void *>(activation_table_site),reinterpret_cast<void *>(mhw_aa_table_entry),&mhw_aa_table_original)==MH_OK,"table hook");
    check(MH_CreateHook(reinterpret_cast<void *>(activation_sharpen_site),reinterpret_cast<void *>(mhw_aa_sharpen_entry),&mhw_aa_sharpen_original)==MH_OK,"sharpen hook");
    mhw_aa_table_resume=reinterpret_cast<void *>(activation_table_resume);mhw_aa_sharpen_resume=reinterpret_cast<void *>(activation_sharpen_resume);
    check(MH_EnableHook(MH_ALL_HOOKS)==MH_OK,"enable hooks");
    for(choice=0;choice<4;++choice){
        const auto bytes=static_cast<std::uint16_t>((choice&1)|((choice>>1)<<8));put(table+0xe9e4,bytes);put(unit+0x14,0x10U|((choice&1)<<1));
        put(ctx+0x1d0,choice&1?other_view:std::uintptr_t{});aa::requested=true;run();
        check(get<std::uint16_t>(table+0xe9e4)==bytes&&get<unsigned>(unit+0x14)==(0x10U|((choice&1)<<1)),"preferences and native flags unchanged");
    }
    check(taa_calls==4&&aa::queue_inserted==2&&aa::queue_existing==2&&registration_calls==4,"missing worker registrations inserted once; existing queue entries reused");
    check(completions==4&&completions==expected_completions,"all four choices complete after native queue");
    choice=0;put(table+0xe9e4,static_cast<std::uint16_t>(0));put(unit+0x14,0x10U);
    for(action=4;action<=5;++action)run();check(taa_calls==4,"other/null bound view not activated");
    action=0;
    for(unsigned gate=0;gate<5;++gate){
        put(unit+0xc,gate==0?7U:8U);put(unit+0x14,gate==1?0U:0x10U);put(unit+0x48,gate==2?0U:1U);put(unit+0x1c,gate==3?0U:1U);put(unit+0x40,gate==4?1U:0U);run();
    }
    check(taa_calls==4&&aa::queue_reasons[2]==5,"native state/render/mask/display/category filters retained");
    put(unit+0xc,8U);put(unit+0x14,0x10U);put(unit+0x48,1U);put(unit+0x1c,1U);put(unit+0x40,0U);
    for(auto vt:{0x351c1a8U,0x354dd78U,0x354df48U}){put(competitor,image.p+vt);action=13;run();}
    check(aa::competing==3&&unrelated_calls==0,"competing AA removed before native resource routing");
    put(competitor,image.p+0x111);run();check(unrelated_calls==1,"unrelated native stage retained");
    auto baseline=taa_calls;
    for(action=14;action<=16;++action)run();
    check(taa_calls==baseline&&get<std::uint64_t>(queue+0xc0)==0x1122334455667788ULL,"full/duplicate/corrupt queues rejected without scratch corruption");
    action=0;put(unit+0x30,unit);run();put(unit+0x30,std::uintptr_t{});check(taa_calls==baseline,"registry cycle rejected");
    put(unit+0x148,std::uintptr_t{});run();put(unit+0x148,unit);check(taa_calls==baseline,"descriptor owner validated");
    action=11;run();check(aa::queue_reasons[5]==1,"nested queue authority suspended");
    action=17;run();check(aa::queue_reasons[5]==2,"nested queue inside Execute suspends and restores comparison authority");
    action=1;run();check(!aa::requested&&aa::healthy(),"stop inside camera drains already-owned queue");
    baseline=taa_calls;action=0;run();check(taa_calls==baseline,"next frame is native after stop");
    aa::requested=true;action=2;run();check(get<std::uint16_t>(table+0xe9e4)==0x101,"native preference update survives scope exit");
    action=7;run();check(!aa::requested&&aa::healthy(),"stop inside Execute retains branch authority to return");
    aa::requested=true;reinterpret_cast<Call>(mhw_aa_jitter2_entry)(unit,0,0,0);check(jitter_calls==0&&aa::jitter_suppressed==1,"native jitter suppressed");
    aa::stop();reinterpret_cast<Call>(mhw_aa_jitter2_entry)(unit,0,0,0);check(jitter_calls==1,"native jitter restored after stop");
    auto healthy=receipt();
    aa::requested=true;aa::latch_failure();baseline=taa_calls;auto original_native=native_calls;auto original_unrelated=unrelated_calls;
    put(image.p+0x51238c8,std::uintptr_t{}); // Unfiltered ownership needs no temporal registry.
    for(choice=0;choice<4;++choice){put(table+0xe9e4,static_cast<std::uint16_t>((choice&1)|((choice>>1)<<8)));put(unit+0x14,0x10U|((choice&1)<<1));
        for(auto vt:{0x351c1a8U,0x354dd78U,0x354df48U}){put(competitor,image.p+vt);action=13;run();}
    }
    check(taa_calls==baseline&&native_calls==original_native&&unrelated_calls==original_unrelated&&aa::queue_unfiltered==12,"latched failure removes all native AA without executing TAA or needing its registry");
    choice=0;put(table+0xe9e4,static_cast<std::uint16_t>(0));put(unit+0x14,0x10U);put(competitor,image.p+0x111);run();
    check(unrelated_calls==original_unrelated+1&&aa::queue_unfiltered==13,"latched path retains unrelated postprocessing");
    reinterpret_cast<Call>(mhw_aa_jitter2_entry)(unit,0,0,0);check(jitter_calls==1,"failure retains native jitter suppression");
    aa::stop();auto latched=receipt();
    // Only the test resets this process-global latch, to exercise old faults.
    aa::failure_latched=false;put(image.p+0x51238c8,registry);
    aa::requested=true;action=3;exception_frame(2);auto frame_exception=receipt();
    aa::fault=0;aa::requested=true;action=6;exception_frame(5);auto taa_exception=receipt();
    aa::fault=0;aa::requested=true;action=10;exception_frame(7);auto queue_exception=receipt();
    aa::fault=0;aa::requested=true;action=9;run();check(aa::fault==5,"reentrant Execute latches fault");auto reentrant=receipt();
    aa::stop();aa::targets[5]=reinterpret_cast<std::uintptr_t>(activation_table_site);
    aa::targets[6]=reinterpret_cast<std::uintptr_t>(activation_sharpen_site);aa::enabled[5]=aa::enabled[6]=true;
    // A callback can re-enter after a drain check and before queued removal.
    // It forwards after stop; no owned camera scope remains.
    aa::pending=1;
    check(aa::queue_disable(),"forwarding callback does not block stopped hook removal");
    aa::disabled(false);check(aa::enabled[5]&&aa::enabled[6],"failed apply cannot claim removal");
    check(MH_ApplyQueued()==MH_OK,"apply stopped activation removal");aa::disabled(true);
    check(!aa::enabled[5]&&!aa::enabled[6],"queued activation hooks retired");aa::pending=0;
    check(MH_EnableHook(reinterpret_cast<void *>(activation_table_site))==MH_OK&&MH_EnableHook(reinterpret_cast<void *>(activation_sharpen_site))==MH_OK,"reenable fixture hooks");
    aa::enabled[5]=aa::enabled[6]=true;
    InterlockedExchange64(&mhw_aa_context,static_cast<LONG64>(ctx));
    check(!aa::queue_disable(),"owned camera scope must finish before removal");
    check(MH_ApplyQueued()==MH_OK,"unrelated empty apply succeeds");aa::disabled(true);
    check(aa::enabled[5]&&aa::enabled[6],"empty apply cannot falsely retire unqueued hooks");
    InterlockedExchange64(&mhw_aa_context,0);
    aa::requested=true;check(!aa::queue_disable(),"live activation cannot be removed");aa::stop();
    // One invalid target must not make successfully queued targets look live,
    // or make the unqueued target look disabled.
    aa::enabled[0]=true;aa::targets[0]=image.p+0x100;
    check(!aa::queue_disable(),"partial queue failure reported");
    check(MH_ApplyQueued()==MH_OK,"apply partial queue");aa::disabled(true);
    check(aa::enabled[0]&&!aa::enabled[5]&&!aa::enabled[6],"per-hook removal accounting");
    check(MH_DisableHook(MH_ALL_HOOKS)==MH_OK&&MH_Uninitialize()==MH_OK,"fixture hooks removed");
    check(completions==expected_completions,"queue completion counts include nested, stop and exception cases");
    for(const auto &item:{std::pair{"healthy",healthy},std::pair{"latched",latched},std::pair{"exception",frame_exception},std::pair{"taa-exception",taa_exception},std::pair{"queue-exception",queue_exception},std::pair{"reentrant",reentrant}})
        std::cout<<"{\"case\":\""<<item.first<<"\",\"activation\":"<<item.second<<"}\n";
    std::cout<<"Native activation: worker prefilter/join, main postqueue, four choices, duplicates/capacity, unrelated stages/threads, retained native gates, stop/reentry/exception cleanup passed\n";
}
