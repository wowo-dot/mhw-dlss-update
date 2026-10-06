// Production callback and forwarding ASM against owned native-shaped memory.
#include "../src/render_resolution/runtime.cpp"
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
namespace rr=mhw::render_resolution;
void check(bool ok,const char *why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
template<class T>void put(std::uintptr_t at,T value){std::memcpy(reinterpret_cast<void *>(at),&value,sizeof(value));}
template<class T>T get(std::uintptr_t at){T v{};check(rr::pb::read(at,v),"fixture read");return v;}
struct Block {
    std::uintptr_t p{};
    explicit Block(std::size_t size):p(reinterpret_cast<std::uintptr_t>(VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE))){check(p!=0,"fixture allocation");}
    ~Block(){VirtualFree(reinterpret_cast<void *>(p),0,MEM_RELEASE);}
};
unsigned action{},native_calls{};
float native_expected=1.F;
constexpr std::uint64_t answer=0x987654321abcdef0ULL;
std::uint64_t native_update(std::uintptr_t r,std::uintptr_t b,std::uintptr_t c,std::uintptr_t d){
    check(r==rr::renderer&&b==2&&c==3&&d==4,"forwarded arguments");
    check(get<float>(r+0x1f0)==native_expected,"native updater sees its own request");++native_calls;
    if(action==1)RaiseException(0xe0424242,0,0,nullptr);
    if(action==2){put(r+0x1f0,.75F);native_expected=.75F;}
    return answer;
}
void native_setter(std::uintptr_t r,float scale){
    if(action==3)RaiseException(0xe0424242,0,0,nullptr);
    const auto p=rr::plan({get<unsigned>(r+0x198),get<unsigned>(r+0x19c)},scale);
    check(p.valid,"fixture setter plan");put(r+0x1f0,p.quantized);
}
void step(){
    rr::pb::Frame f{};f.caller=rr::expected_return;f.gpr={0,rr::renderer,2,3,4,5,6};
    rr::dispatch(&f);check(f.gpr[0]==answer,"native return register preserved");
    const float desired=get<float>(rr::renderer+0x1f0);const auto v=get<std::uintptr_t>(rr::renderer+0x1f450);
    put(rr::renderer+0x1f8,get<float>(rr::renderer+0x1f4));put(rr::renderer+0x1f4,desired);
    put(rr::renderer+0x208,desired);put(v+0x6b0,desired);put(v+0x6b8,desired);
    put(rr::renderer+0x1f210,get<std::uint64_t>(rr::renderer+0x1f210)+1);
}
void exception_step(){bool caught=false;
    __try {step();}
    __except(GetExceptionCode()==0xe0424242?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){caught=true;}
    check(caught,"native exception propagated");
}
int main(){
    const auto p=rr::plan({2560,1440},.5F);check(p.valid&&p.render==rr::Extent{1280,720}&&p.quantized==.5F,"half-size plan");
    const auto q=rr::plan({2560,1440},2.F/3.F);check(q.valid&&q.render==rr::Extent{1696,954},"native 16-pixel quantization");
    check(rr::plan({3840,2160},.5F).render==rr::Extent{1920,1080},"4K geometry without allocation claim");
    check(rr::plan({5120,1440},.5F).render==rr::Extent{2560,720},"ultrawide geometry");
    check(!rr::plan({65,64},.5F).valid,"reject native clamp disagreement");
    for(float scale:{0.F,.49F,1.01F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})check(!rr::plan({2560,1440},scale).valid,"invalid scale");
    check(!rr::plan({0,1440},.5F).valid&&!rr::plan({8193,1440},.5F).valid,"invalid display extent");
    check(rr::matching_basis({2560,1440},1,{2560,1440}),"matching native override dimensions");
    check(!rr::matching_basis({2560,1440},1,{1920,1080})&&!rr::matching_basis({2560,1440},2,{2560,1440}),"reject different or unknown sizing basis");
    rr::Ownership owner;float restore{};check(!owner.before(1.F,restore)&&restore==1.F,"unowned request untouched");
    owner.after(1.F,.5F);check(owner.before(.5F,restore)&&restore==1.F&&!owner.owned,"restore ours before native policy");
    owner.after(1.F,.5F);check(!owner.before(.75F,restore)&&restore==.75F&&!owner.owned,"foreign request wins");
    Block image(0x5300000),renderer(0x800000),view(0x1000),backend(0x43000),config(0x3000),depth(0x1000),texture(0x1000);
    rr::image=image.p;rr::renderer=renderer.p;rr::expected_return=image.p+0x22982e7;
    rr::discovered.backend=backend.p;rr::target=p;rr::setter=native_setter;rr::original=reinterpret_cast<void *>(native_update);rr::requested=true;
    put(image.p+0x51c4480,renderer.p);put(image.p+0x51cc080,static_cast<unsigned char>(1));put(image.p+0x51c5320,config.p);
    put(renderer.p+0x78,backend.p);put(renderer.p+0x1f450,view.p);put(renderer.p+0x198,2560U);put(renderer.p+0x19c,1440U);put(renderer.p+0x1f210,std::uint64_t{1});
    for(auto offset:{0x1f0,0x1f4,0x1f8,0x200,0x204,0x208})put(renderer.p+offset,1.F);
    for(auto offset:{0x6b0,0x6b4,0x6b8})put(view.p+offset,1.F);
    put(view.p+0x628,depth.p);put(depth.p+0xa0,texture.p);put(texture.p+0x88,2560U|(1440U<<16));
    rr::Snapshot captured{};
    check(rr::snapshot(captured)&&captured.width_override==0&&captured.basis_width==2560,"display width route admitted");
    put(config.p+0x2cda,static_cast<unsigned char>(1));put(config.p+0x2cd0,2560U);put(config.p+0x2cd4,1440U);
    check(rr::snapshot(captured)&&captured.width_override==1&&captured.basis_width==2560,"live-shaped config width route admitted");
    put(config.p+0x2cd0,1920U);check(!rr::snapshot(captured),"reject alternate width mismatch");put(config.p+0x2cd0,2560U);
    put(config.p+0x2cd4,1080U);check(!rr::snapshot(captured),"reject alternate height mismatch");put(config.p+0x2cd4,1440U);
    put(config.p+0x2cda,static_cast<unsigned char>(2));check(!rr::snapshot(captured),"reject unknown override flag");put(config.p+0x2cda,static_cast<unsigned char>(1));
    using Call=std::uint64_t(*)(std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t);
    check(reinterpret_cast<Call>(mhw_resolution_entry)(renderer.p,2,3,4)==answer,"production ASM forwards unmatched caller");
    check(rr::visits==0&&!rr::ownership.owned,"unqualified caller does not acquire scale");
    for(unsigned i=0;i<32;++i)step();check(rr::overrides==0&&rr::baseline.applied==1.F,"baseline has no writes");
    for(unsigned i=0;i<100;++i)step();
    check(rr::overrides==100&&rr::pre_restores==99&&rr::scaled_observed==99&&rr::ownership.owned,"continuous override preserves native cached request");
    rr::InputStamp stamp{};
    check(rr::active_ready()&&rr::input_stamp(view.p,stamp)&&stamp.render_width==1280&&stamp.render_height==720&&stamp.active_scale==.5F&&stamp.content_scale==1.F,"temporal capture sees applied scale");
    check(!rr::input_stamp(view.p+8,stamp),"temporal stamp rejects different view");
    check(get<float>(renderer.p+0x200)==1.F&&get<float>(renderer.p+0x204)==1.F&&rr::reduced.depth_width==2560,"allocation scale unchanged");
    action=2;step();action=0;check(rr::ownership.native==.75F,"native updater new request retained");
    step();rr::stop();step();check(!rr::ownership.owned&&!rr::restored&&get<float>(renderer.p+0x1f0)==.75F,"stop restores latest native request");
    step();check(rr::restored&&rr::restoration.applied==.75F&&rr::restoration.view_scale==.75F,"restoration waits for native application");
    check(!rr::active_ready()&&!rr::input_stamp(view.p,stamp),"temporal stamp closes at stop");
    // A separate native request between invocations must not be overwritten by
    // stale restoration. Resume only inside this owned-memory fixture.
    rr::requested=true;rr::restored=false;step();put(renderer.p+0x1f0,1.F);native_expected=1.F;step();
    check(rr::foreign_requests==1&&rr::ownership.native==1.F,"external native request wins");
    action=1;exception_step();action=0;
    check(rr::fault==7&&rr::pending==0&&!rr::ownership.owned&&!rr::requested&&get<float>(renderer.p+0x1f0)==1.F,"exception restores request and drains callback");
    step();step();check(rr::restored&&rr::restoration.applied==1.F,"exception recovery lets native apply original scale");
    rr::requested=true;rr::restored=false;rr::fault=0;action=3;exception_step();action=0;
    check(rr::fault==7&&rr::pending==0&&!rr::requested,"setter exception drains without holding telemetry lock");
    step();check(rr::restored&&!rr::ownership.owned&&get<float>(renderer.p+0x1f0)==1.F,"setter exception retains native rendering");
    std::ostringstream out;rr::write(out);check(out.str().find("\"dlss_evaluations\":0")!=std::string::npos,"diagnostic scope explicit");
    std::cout<<"Render-resolution geometry, ownership, native update, ABI, stop and exception checks passed.\n";
}
