#include "scaled_effect_fixture.hpp"
#include "perf_env.hpp"
#include "../src/scaled_temporal/stream_route.cpp"
#ifdef MHW_STREAM_RUNTIME_FIXTURE
#include "../src/continuous_dlaa/runtime.cpp"
namespace cd=mhw::continuous_dlaa;
void WINAPI execute_forward(ID3D12CommandQueue *queue,UINT count,ID3D12CommandList *const *lists){queue->ExecuteCommandLists(count,lists);}
#endif
#include <sstream>
#include "scaled_stream_plan_fixture.hpp"
namespace fx=mhw::fixtures::effects;
namespace route=mhw::scaled_temporal::stream_route;
namespace obs=mhw::temporal_present::observer;
template<class T,std::size_t N>void put(std::array<std::byte,N> &a,unsigned offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
void WINAPI copy_forward(ID3D12GraphicsCommandList *l,const D3D12_TEXTURE_COPY_LOCATION *d,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION *s,const D3D12_BOX *box){l->CopyTextureRegion(d,x,y,z,s,box);}
#include "scaled_indexed_fixture.hpp"
#include "scaled_late_candidates_fixture.hpp"
#include "scaled_identity_stream_fixture.hpp"
#ifdef MHW_STREAM_RUNTIME_FIXTURE
#include "scaled_return_fixture.hpp"
#endif
// v0.26.0: the identity route (the only route): the identity streams, the
// plan's discovery with full_post, late indexed draws (the silhouettes), and
// the runtime's return and shutdown cases.
int wmain(int argc,wchar_t **argv){try{
    apply_perf_env();
    using namespace fx;require(argc==6,"usage: warp|hardware shader-prefix output-dir case luid-low");
    const bool warp=std::wstring_view(argv[1])==L"warp";const std::string mode=std::filesystem::path(argv[4]).string();
    const std::filesystem::path out=argv[3];require(!std::filesystem::exists(out),"preserve output");std::filesystem::create_directories(out);
    Fixture f(warp,LUID{std::stoul(argv[5]),0},argv[2]);
    require(MH_Initialize()==MH_OK,"hook init");std::string reason;
#ifdef MHW_STREAM_RUNTIME_FIXTURE
    if(mode=="return-clobber"||mode=="fault-shutdown"){scaled_return_fixture::run(f,mode);return 0;}
#endif
    const auto resolver=[](std::uintptr_t p,unsigned slot,std::uintptr_t &target)noexcept{target=(*reinterpret_cast<std::uintptr_t **>(p))[slot];return target!=0;};
    require(obs::install(f.device.Get(),resolver,reason,true,ViewportFeed::observe,true),reason.c_str());obs::begin();
    ViewportFeed::expected=f.list.Get();
    route::cfg.device=reinterpret_cast<std::uintptr_t>(f.device.Get());route::originals[2]=reinterpret_cast<void *>(copy_forward);
    route::width=128;route::height=64;route::render_w=64;route::render_h=32;route::begin();
    if(mode=="identity"||mode=="identity_split"||mode=="identity_native"){identity_stream_fixture::run(f,mode);require(obs::finish(),"identity observer cleanup");
        std::ofstream report(out/"route.json");report<<std::boolalpha;route::write(report);report<<'\n';route::write_summary(report);report<<'\n';
        require(bool(report),"route report");return 0;}
    if(mode=="indexed-ownership"){check_indexed_ownership(f);require(obs::finish(),"indexed observer cleanup");return 0;}
    if(mode=="late-candidates"){check_late_candidates(f);require(obs::finish(),"late observer cleanup");return 0;}
    if(mode=="compute_descriptions"){
        for(const auto format:{DXGI_FORMAT_R32_TYPELESS,DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R8_UNORM}){
            D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=128;d.Height=64;d.DepthOrArraySize=d.MipLevels=1;d.Format=format;d.SampleDesc.Count=1;
            auto r=resource(f.device.Get(),d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
            ID3D12Resource *held{};D3D12_RESOURCE_DESC actual{};unsigned planes{};
            require(route::compute_describe(reinterpret_cast<std::uintptr_t>(r.Get()),&held,&actual,&planes),"compute description acquired");
            require(held==r.Get()&&actual.Format==format&&actual.Width==128&&actual.Height==64&&planes==1,"actual texture and plane description");held->Release();
            route::cfg.device=0;
            require(!route::compute_describe(reinterpret_cast<std::uintptr_t>(r.Get()),&held,&actual,&planes)&&!held,"wrong device rejected and reference released");
            route::cfg.device=reinterpret_cast<std::uintptr_t>(f.device.Get());
        }
        require(obs::finish(),"description observer cleanup");f.messages();
        std::cout<<"PASS four actual texture formats and wrong-device rejection\n";return 0;
    }
    if(mode=="plan"){require(stream_plan_fixture::check_plan(f)==11,"eleven malformed identity graphs");require(obs::finish(),"plan observer cleanup");f.messages();return 0;}
    throw std::runtime_error("unknown case "+mode);
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
