#include "scaled_effect_fixture.hpp"
#include "perf_env.hpp"
#include "../src/scaled_temporal/radial_shader.hpp"
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
#include "scaled_stream_haze_fixture.hpp"
#include "scaled_overlay_fixture.hpp"
template<class T,std::size_t N>void put(std::array<std::byte,N> &a,unsigned offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
void WINAPI copy_forward(ID3D12GraphicsCommandList *l,const D3D12_TEXTURE_COPY_LOCATION *d,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION *s,const D3D12_BOX *box){l->CopyTextureRegion(d,x,y,z,s,box);}
#include "scaled_indexed_fixture.hpp"
#include "scaled_late_candidates_fixture.hpp"
#include "scaled_radial_occlusion_fixture.hpp"
#include "scaled_identity_stream_fixture.hpp"
#ifdef MHW_STREAM_RUNTIME_FIXTURE
#include "scaled_return_fixture.hpp"
#endif
int wmain(int argc,wchar_t **argv){try{
    apply_perf_env();
    using namespace fx;require(argc==6||argc==7||argc==8,"usage: warp|hardware shader-prefix output-dir case luid-low [dependency-dir | haze-vs haze-ps]");
    const bool warp=std::wstring_view(argv[1])==L"warp";const std::string mode=std::filesystem::path(argv[4]).string();
    const std::filesystem::path out=argv[3];require(!std::filesystem::exists(out),"preserve output");std::filesystem::create_directories(out);
    Fixture f(warp,LUID{std::stoul(argv[5]),0},argv[2]);
    if(mode=="auxiliary"){check_haze_auxiliary_state(f);return 0;}
    require(MH_Initialize()==MH_OK,"hook init");std::string reason;
#ifdef MHW_STREAM_RUNTIME_FIXTURE
    if(mode=="return-clobber"||mode=="fault-shutdown"){scaled_return_fixture::run(f,mode);return 0;}
#endif
    const auto resolver=[](std::uintptr_t p,unsigned slot,std::uintptr_t &target)noexcept{target=(*reinterpret_cast<std::uintptr_t **>(p))[slot];return target!=0;};
    require(obs::install(f.device.Get(),resolver,reason,true,ViewportFeed::observe,true),reason.c_str());obs::begin();
    ViewportFeed::expected=f.list.Get();
    route::cfg.device=reinterpret_cast<std::uintptr_t>(f.device.Get());route::originals[2]=reinterpret_cast<void *>(copy_forward);
    route::width=128;route::height=64;route::render_w=64;route::render_h=32;route::begin();
    if(mode=="identity"||mode=="identity_split"||mode=="identity_native"){identity_stream_fixture::run(f,mode);require(obs::finish(),"identity observer cleanup");return 0;}
    if(mode=="indexed-ownership"){check_indexed_ownership(f);require(obs::finish(),"indexed observer cleanup");return 0;}
    if(mode=="late-candidates"){check_late_candidates(f);require(obs::finish(),"late observer cleanup");return 0;}
    if(mode=="radial-occlusion"){check_radial_occlusion(f);require(obs::finish(),"occlusion observer cleanup");return 0;}
#include "compute_depth_pixels_fixture.inl"
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
    if(mode=="dof_plan"){require(argc==7,"DOF shader directory");require(stream_plan_fixture::check_plan(f,{},{},{},{},{},argv[6])==14,"base graph rejects");require(obs::finish(),"DOF observer cleanup");f.messages();return 0;}
    if(mode=="dependency_plan"){require(argc==7,"dependency shader directory");require(stream_plan_fixture::check_plan(f,{},{},{},{},argv[6])==14,"base graph rejects");require(obs::finish(),"dependency observer cleanup");f.messages();return 0;}
    if(mode=="overlay_pixels"||mode=="overlay_pixels_padded"){require(argc==8,"overlay shader paths");overlay_fixture::run(f,read(argv[6]),read(argv[7]),mode=="overlay_pixels_padded");require(obs::finish(),"overlay observer cleanup");return 0;}
    if(mode=="haze"){
        require(argc==8,"haze shader paths");const auto hv=read(argv[6]),hp=read(argv[7]);
        require(stream_plan_fixture::check_plan(f,hv,hp)==14,"fourteen malformed graph cases");
        std::ofstream report(out/"haze.json");report<<std::boolalpha;route::haze_write(report);report<<'\n';require(bool(report),"haze fixture report");
        require(obs::finish(),"haze observer cleanup");f.messages();return 0;
    }
    require(stream_plan_fixture::check_plan(f)==14,"fourteen malformed graph cases");
    const bool multi_overlay=mode=="overlay_multi"||mode=="overlay_multi_native";
    const bool padded_overlay=multi_overlay||mode=="overlay_padded"||mode=="overlay_padded_native";
    const bool split_overlay=padded_overlay||mode=="overlay_inherited"||mode=="overlay_inherited_native";
    const bool overlay_mode=mode=="overlay_reordered"||mode=="overlay_native"||split_overlay;
    const bool native_choice=mode=="overlay_multi_native"||mode=="overlay_padded_native"||mode=="reordered_native"||mode=="unclosed"||mode=="overlay_native"||mode=="overlay_inherited_native";
    const bool spatial_choice=mode=="reordered_spatial",stop_choice=mode=="reordered_stop";
    const bool split_copy=mode=="copy_reordered";
    const bool four_lists=split_copy||split_overlay;
    const bool unscoped=mode=="unscoped_reordered"||overlay_mode||split_copy;
    const bool reordered=mode=="reordered"||native_choice||spatial_choice||stop_choice||unscoped;
    const bool aliased=mode=="mixed_alias",mixed=mode=="mixed"||aliased;require(!mixed||argc==8,"mixed route needs haze shaders");
    const auto haze_vs=mixed?read(argv[6]):std::vector<std::byte>{},haze_ps=mixed?read(argv[7]):std::vector<std::byte>{};
    std::unique_ptr<StreamHazeFixture> haze;if(mixed)haze=std::make_unique<StreamHazeFixture>(f);
    require(!overlay_mode||argc==8,"overlay shader paths");
    const auto overlay_vs=overlay_mode?read(argv[6]):std::vector<std::byte>{},overlay_ps=overlay_mode?read(argv[7]):std::vector<std::byte>{};
    if(overlay_mode){require(sf::overlay_pixel_shader(overlay_ps),"captured overlay shader eligibility");
        route::final_quads={};
        require(stream_plan_fixture::check_plan(f,{},{},overlay_ps,overlay_vs)==14,"overlay graph discovery and rejection");}
    const unsigned stages=multi_overlay?6U:overlay_mode?5U:mixed?4U:3U;
    const std::array<D3D12_INPUT_ELEMENT_DESC,2> padded_elements{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TexCoord",0,DXGI_FORMAT_R32G32_FLOAT,0,20,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
    const sf::FilterGeometry g{64,32,128,64};
    std::array<sf::EffectVertices,6> v;std::array<sf::EffectRecipe,6> recipe_values;
    std::array<ComPtr<ID3D12PipelineState>,6> native;std::array<ComPtr<ID3D12Resource>,6> vb,outputs,cb;
    const auto make_vertex_buffer=[&](unsigned stage){
        auto result=buffer(f.device.Get(),stage>=4&&padded_overlay?112:64,D3D12_HEAP_TYPE_UPLOAD);
        if(stage>=4&&padded_overlay){std::array<std::byte,112> raw{};for(unsigned i=0;i<4;++i){std::memcpy(raw.data()+i*28,v[stage].values.data()+4*i,8);std::memcpy(raw.data()+i*28+20,v[stage].values.data()+4*i+2,8);}upload(result.Get(),raw);}
        else upload(result.Get(),std::as_bytes(std::span(v[stage].values)));
        return result;
    };
    std::array<ComPtr<ID3D12DescriptorHeap>,6> srv,rtv;
    std::array<std::byte,0x90> overlay_set{};
    auto td=texture(128,64,DXGI_FORMAT_R11G11B10_FLOAT);td.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    auto source=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
    auto destination=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
    Pixels input(f.device.Get(),td,D3D12_HEAP_TYPE_UPLOAD),poison(f.device.Get(),td,D3D12_HEAP_TYPE_UPLOAD),copied(f.device.Get(),td,D3D12_HEAP_TYPE_READBACK),final_pixels(f.device.Get(),td,D3D12_HEAP_TYPE_READBACK);
    const std::vector<unsigned> sentinel(128*64,0x98000001U);poison.write(std::as_bytes(std::span(sentinel)));
    std::vector<unsigned> baseline_pixels;
    std::array<ComPtr<ID3D12CommandAllocator>,4> allocators{f.allocator};
    std::array<ComPtr<ID3D12GraphicsCommandList>,4> commands{f.list};
    if(reordered)for(unsigned i=1;i<(four_lists?4U:3U);++i){
        hr(f.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocators[i])),"split allocator");
        hr(f.device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocators[i].Get(),nullptr,IID_PPV_ARGS(&commands[i])),"split list");
        hr(commands[i]->Close(),"split initial close");
    }
#ifdef MHW_STREAM_RUNTIME_FIXTURE
    cd::cfg.device=reinterpret_cast<std::uintptr_t>(f.device.Get());cd::fence=f.fence;cd::signal_value=f.sequence;cd::accepting=true;
    cd::originals[7]=reinterpret_cast<void *>(execute_forward);cd::list_count=four_lists?4U:reordered?3U:1U;
#endif
    std::vector<unsigned> pixels(128*64);for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x)
        pixels[y*128+x]=(15U*64+(x*3+y*5)%64)|((14U*64+(x*7+y*11)%64)<<11)|((16U*32+(x+y*3)%32)<<22);
    input.write(std::as_bytes(std::span(pixels)));
    mhw::temporal_bindings::Snapshot snapshot;snapshot.frame=3;snapshot.rectangle={0,0,64,32};
    for(unsigned stage=0;stage<stages;++stage){
        if(overlay_mode&&stage==3)continue;
        v[stage]=stage==3?sf::EffectVertices{{{-1,1,0,0,-1,-3,0,1,3,1,1,0}},3,D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP}:vertices(static_cast<sf::EffectStage>(stage),g,false);
        auto &r=recipe_values[stage];r=recipe(static_cast<sf::EffectStage>(stage>=4?0:stage),g,f.root.Get(),stage>=4?overlay_vs:stage==3?haze_vs:f.vs[stage],stage>=4?overlay_ps:stage==3?haze_ps:f.ps[stage],std::span(v[stage].values).first(v[stage].count*4));
        if(stage>=4){r.blend.RenderTarget[0].BlendEnable=TRUE;r.blend.RenderTarget[0].SrcBlend=D3D12_BLEND_DEST_COLOR;}
        if(stage==5){r.blend.RenderTarget[0].SrcBlend=D3D12_BLEND_ONE;r.blend.RenderTarget[0].DestBlend=D3D12_BLEND_ONE;}
        if(stage>=4&&padded_overlay)r.input_layout=padded_elements;
        native[stage]=pipeline(f.device.Get(),r,r.pixel_shader);vb[stage]=make_vertex_buffer(stage);
        outputs[stage]=stage>=4?outputs[0]:aliased&&stage==3?destination:resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
        D3D12_DESCRIPTOR_HEAP_DESC h{};h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;h.NumDescriptors=1;hr(f.device->CreateDescriptorHeap(&h,IID_PPV_ARGS(&rtv[stage])),"RTV");f.device->CreateRenderTargetView(outputs[stage].Get(),nullptr,rtv[stage]->GetCPUDescriptorHandleForHeapStart());
        h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;h.NumDescriptors=5;h.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;hr(f.device->CreateDescriptorHeap(&h,IID_PPV_ARGS(&srv[stage])),"SRV");
        const auto step=f.device->GetDescriptorHandleIncrementSize(h.Type);
        constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32G32_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT};
        for(unsigned i=0;i<5;++i){D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=formats[i];d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Texture2D.MipLevels=1;
            if(stage==1&&i==1){d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.Texture3D.MipLevels=1;}
            if(stage==3&&i){d.Format=i==1?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R8G8B8A8_UNORM;if(i==2){d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;d.Texture3D.MipLevels=1;}}
            f.device->CreateShaderResourceView(stage>=4?nullptr:stage==3?(i&&i<4?haze->inputs[i-1].Get():i?nullptr:outputs[0].Get()):i?nullptr:stage?outputs[stage-1].Get():destination.Get(),&d,{srv[stage]->GetCPUDescriptorHandleForHeapStart().ptr+i*SIZE_T(step)});}
        std::array<float,320> c{};if(stage==0){c[42]=c[46]=1;c[68]=128;c[69]=64;c[70]=1.F/128;c[71]=1.F/64;c[78]=64;c[79]=32;c[195]=.125F;c[196]=c[197]=1;c[198]=.01F;c[200]=63.5F/128;c[201]=31.5F/64;const unsigned n=8;std::memcpy(c.data()+194,&n,4);}
        if(stage==1){const unsigned n=1;std::memcpy(c.data()+192,&n,4);}if(stage==3){for(unsigned i=0;i<4;++i){c[48+5*i]=1;c[80+5*i]=1;}
         c[132]=128;c[133]=64;c[134]=1.F/128;c[135]=1.F/64;c[142]=64;c[143]=32;
         c[193]=.05F;c[194]=.8F;c[195]=.2F;c[196]=.4F;c[200]=.3F;c[201]=.5F;c[202]=.2F;c[203]=1.3F;}
        if(stage>=4){c[79]=32;c[80]=1.F/64;c[192]=.1F;c[193]=1.5F;c[194]=.4F;c[196]=.4F;c[197]=.5F;c[198]=.6F;}
        cb[stage]=buffer(f.device.Get(),sizeof(c),D3D12_HEAP_TYPE_UPLOAD);upload(cb[stage].Get(),std::as_bytes(std::span(c)));
        if(stage>=4)continue;
        auto &cache=route::pipelines[stage];cache.effect=std::make_unique<sf::Effect>(f.device.Get(),r,true);cache.signature=route::signature(r);cache.root=f.root;

    }
    if(haze)haze->upload(f);
#ifdef MHW_STREAM_RUNTIME_FIXTURE
    cd::signal_value=f.sequence;
#endif
    std::array<std::byte,80> packet{};std::array<std::byte,0x160> recording{};std::array<std::byte,48> temporal{};
    put(temporal,0,6U);put(temporal,0x10,std::uintptr_t{1234});
    put(recording,0x158,reinterpret_cast<std::uintptr_t>(packet.data()));put(recording,0x20,reinterpret_cast<std::uintptr_t>(f.list.Get()));
    const auto fixture_copy=[&](ID3D12GraphicsCommandList *list,const D3D12_BOX &box,bool caller=true){D3D12_TEXTURE_COPY_LOCATION a{},b{};a.pResource=source.Get();a.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;b=a;b.pResource=destination.Get();
        route::copy_slot=static_cast<unsigned>(snapshot.frame%32);route::copy_recording=reinterpret_cast<std::uintptr_t>(recording.data());route::copy_calls=0;
        return route::copy_override(list,&b,0,0,0,&a,&box,caller?0x259caf0:0);};
    const unsigned iterations=(mode=="success"||mixed||reordered)?96:1;
    for(unsigned frame=1;frame<=iterations;++frame){
    route::begin();
    snapshot.frame=frame;route::publish(snapshot,{});auto &plan=route::plans[frame%32];plan.built=true;plan.has_haze=mixed&&(frame%2==0);
    require(mhw::temporal_bindings::packet(route::reader,reinterpret_cast<std::uintptr_t>(temporal.data()),plan.temporal),"fixture packet");
    plan.copy=reinterpret_cast<std::uintptr_t>(packet.data());plan.resources[0]=source;plan.resources[1]=destination;
    for(unsigned stage=0;stage<(mixed?4U:3U);++stage){plan.resources[stage+2]=outputs[stage];plan.draws[stage].signature=route::pipelines[stage].signature;}
    if(overlay_mode){plan.overlay_count=multi_overlay?2U:1U;plan.ordered_count=3+plan.overlay_count;
        plan.order=multi_overlay?std::array<unsigned,12>{0,4,5,1,2}:std::array<unsigned,12>{0,4,1,2};
        for(unsigned stage=4;stage<4+plan.overlay_count;++stage){plan.targets[stage]=2;
            plan.draws[stage].draw.shader_set=reinterpret_cast<std::uintptr_t>(overlay_set.data());++route::overlay_planned;
            if(padded_overlay){auto &rec=plan.draws[stage];rec.vertex_stride=28;rec.before.pipeline.input_count=2;require(sf::overlay_vertex_layout(padded_elements,28,rec.overlay_layout),"padded fixture layout");}}}
    if(multi_overlay)++route::overlay_shared_prepared;
    if(plan.has_haze)haze->prepare(plan);
    std::array<std::uintptr_t,sf::FrameHandoff::capacity> prepared_packets{plan.temporal.object,plan.copy};
    for(unsigned ordinal=0;ordinal<route::effect_count(plan);++ordinal){const auto stage=route::effect_stage(plan,ordinal);
        plan.draws[stage].draw.packet=10000+stage;prepared_packets[2+ordinal]=plan.draws[stage].draw.packet;}
    require(route::prepare_selector(plan)&&plan.handoff.prepare({frame,1,1,2},std::span(prepared_packets).first(2+route::effect_count(plan)))==sf::FrameHandoff::Result::ok,"fixture prepared frame");
    plan.prepared=plan.claimed=true;++route::claimed;++route::prepared_frames;if(plan.has_haze)++route::haze_claimed;
    route::watch(static_cast<unsigned>(frame%32),plan);
    if(mixed){D3D12_SHADER_RESOURCE_VIEW_DESC grade_input{};grade_input.Format=DXGI_FORMAT_R11G11B10_FLOAT;
     grade_input.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;grade_input.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;grade_input.Texture2D.MipLevels=1;
     f.device->CreateShaderResourceView(outputs[plan.has_haze?3:0].Get(),&grade_input,srv[1]->GetCPUDescriptorHandleForHeapStart());}
    const unsigned list_count=four_lists?4U:reordered&&frame>1?3U:1U;
    for(unsigned i=0;i<list_count;++i){hr(allocators[i]->Reset(),"allocator");hr(commands[i]->Reset(allocators[i].Get(),nullptr),"list");obs::reset(commands[i].Get(),frame,nullptr);}
    if(frame==1){
        obs::Predicate predicate;require(obs::predicate_current(f.list.Get(),predicate)&&!predicate.buffer,"Reset disables predication");
        f.list->SetPredication(plan.selector.Get(),0,D3D12_PREDICATION_OP_NOT_EQUAL_ZERO);
        require(obs::predicate_current(f.list.Get(),predicate)&&predicate.buffer==reinterpret_cast<std::uintptr_t>(plan.selector.Get())&&predicate.operation==D3D12_PREDICATION_OP_NOT_EQUAL_ZERO,"native predicate observed");
        rejects([&]{route::PredicateScope scope(f.list.Get());});
        require(obs::predicate_current(f.list.Get(),predicate)&&predicate.buffer==reinterpret_cast<std::uintptr_t>(plan.selector.Get()),"rejection preserves existing native predicate");
        f.list->ClearState(nullptr);
        require(obs::predicate_current(f.list.Get(),predicate)&&!predicate.buffer,"ClearState disables predication");
    }
#ifdef MHW_STREAM_RUNTIME_FIXTURE
    for(unsigned i=0;i<list_count;++i)cd::lists[i]={commands[i],frame,0,0,false,false};
#endif
    ID3D12GraphicsCommandList *current=f.list.Get();
    const auto transition=[&](ID3D12Resource *r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){D3D12_RESOURCE_BARRIER b{};b.Transition={r,UINT_MAX,before,after};current->ResourceBarrier(1,&b);
        // The extra regression emits transitions before any owned callback,
        // including workers recorded in reverse CPU order on separate lists.
        const auto saved_api=route::api_slot,saved_copy=route::copy_slot,saved_resolve=route::resolve_slot;
        route::api_slot=unscoped?UINT_MAX:frame%32;route::copy_slot=route::resolve_slot=UINT_MAX;
        if(mode!="states")route::barriers(current,1,&b);
        route::api_slot=saved_api;route::copy_slot=saved_copy;route::resolve_slot=saved_resolve;};
    const D3D12_BOX box{0,0,0,mode=="box"?63U:64U,32,1};
    const auto record_resolve=[&]{
    current=f.list.Get();ViewportFeed::expected=current;
    if(stop_choice)route::stop();
    auto foreign=snapshot;++foreign.frame;require(!route::ready(f.list.Get(),frame,foreign,reinterpret_cast<std::uintptr_t>(recording.data())),"reject foreign frame");
    if(native_choice){
        require(route::observe_resolve(plan.temporal.object,f.list.Get(),frame),"native resolve recording observed");
        route::finish_resolve(plan.temporal.object,true);
    }else{require(route::ready(f.list.Get(),mode=="generation"?frame+1:frame,snapshot,reinterpret_cast<std::uintptr_t>(recording.data())),"claim frame");route::resolved(true,!spatial_choice&&!stop_choice);}
#ifdef MHW_STREAM_RUNTIME_FIXTURE
    if(!native_choice){
        const cd::ResourcePins retained{source,destination,outputs[0],outputs[1]};
        require(cd::recording_pins.retain(f.list.Get(),frame,retained),"runtime private input retention");
        cd::lease={f.list.Get(),frame,frame,true,true};cd::lists[0].injected_generation=frame;
    }
#endif
    input.copy(f.list.Get(),source.Get(),true);
    };
    const auto record_copy=[&]{
    current=commands[split_copy?1:0].Get();ViewportFeed::expected=current;
    put(recording,0x20,reinterpret_cast<std::uintptr_t>(current));
    transition(source.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(destination.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
    poison.copy(current,destination.Get(),true);
    if(mode=="packet")packet[16]=std::byte{1};
    if(split_copy&&frame==1){const auto address=reinterpret_cast<std::uintptr_t>(current);
        const auto a=route::recording_states.get(address,frame,reinterpret_cast<std::uintptr_t>(source.Get()));
        const auto b=route::recording_states.get(address,frame,reinterpret_cast<std::uintptr_t>(destination.Get()));
        const auto *track=route::track_recording(plan,current,frame,false);
        require(!track&&a.known&&a.value==D3D12_RESOURCE_STATE_COPY_SOURCE&&b.known&&b.value==D3D12_RESOURCE_STATE_COPY_DEST,"copy-first journal knows states before plan enrollment");
        std::ofstream receipt(out/"copy-first-proof.json");receipt<<"{\"plan_recording_present\":false,\"source_known\":true,\"destination_known\":true,\"source_state\":"<<a.value<<",\"destination_state\":"<<b.value<<"}\n";
        require(bool(receipt),"copy-first proof saved");}
    const bool expanded=fixture_copy(current,box,mode!="caller");
    if(mode!="success"&&!mixed&&!reordered&&mode!="duplicate"){
        require(!expanded&&!route::healthy()&&route::copies==0,"invalid copy rejected before recording");hr(f.list->Close(),"discard close");hr(f.list->Reset(f.allocator.Get(),nullptr),"discard reset");hr(f.list->Close(),"discarded");
        require(obs::finish(),"observer cleanup");f.messages();std::cout<<"PASS rejection "<<mode<<'\n';return 0;
    }
    require(expanded&&route::copies==frame,"expanded copy");
    obs::Predicate after_copy;require(obs::predicate_current(current,after_copy)&&!after_copy.buffer,"copy restores disabled predicate");
    if(mode=="duplicate"){
        require(!fixture_copy(current,box)&&!route::healthy()&&route::copies==1,"duplicate before completed chain is rejected");
        hr(f.list->Close(),"discard duplicate close");hr(f.list->Reset(f.allocator.Get(),nullptr),"discard duplicate reset");hr(f.list->Close(),"discarded duplicate");
        require(obs::finish(),"duplicate observer cleanup");f.messages();std::cout<<"PASS rejection duplicate\n";return 0;
    }
    transition(destination.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);copied.copy(current,destination.Get(),false);transition(destination.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    return -1;
    };
    const auto record_stage=[&](unsigned stage,unsigned list_index){
        current=commands[list_index].Get();ViewportFeed::expected=current;
        // An in-place overlay can use the preceding native draw's RT state
        // without a redundant transition in its own command list.
        if(!(split_overlay&&stage>=4))transition(outputs[stage].Get(),stage>=4||(aliased&&stage==3)?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_RENDER_TARGET);
        auto *list=current;ID3D12DescriptorHeap *heap[]{srv[stage].Get()};list->SetDescriptorHeaps(1,heap);list->SetGraphicsRootSignature(f.root.Get());list->SetPipelineState(native[stage].Get());
        for(unsigned i=0;i<4;++i)list->SetGraphicsRootConstantBufferView(i,cb[stage]->GetGPUVirtualAddress()+(stage==3?(i==0?0: i==3?768:512):256*i));list->SetGraphicsRootDescriptorTable(4,srv[stage]->GetGPUDescriptorHandleForHeapStart());
        const auto handle=rtv[stage]->GetCPUDescriptorHandleForHeapStart();list->OMSetRenderTargets(1,&handle,FALSE,nullptr);
        const D3D12_VERTEX_BUFFER_VIEW view{vb[stage]->GetGPUVirtualAddress(),stage>=4&&padded_overlay?112U:64U,stage>=4&&padded_overlay?28U:16U};list->IASetPrimitiveTopology(v[stage].topology);list->IASetVertexBuffers(0,1,&view);
        const D3D12_VIEWPORT viewport{0,0,stage==2?128.F:64.F,stage==2?64.F:32.F,0,1};const D3D12_RECT scissor{0,0,stage==2?128:64,stage==2?64:32};list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);
        sf::EffectBindings bindings{list,native[stage].Get(),f.root.Get(),outputs[stage].Get(),vb[stage].Get(),view,viewport,scissor,v[stage].topology,v[stage].count,0};
        bool replaced=false;
        if(stage==0&&frame==1){
            auto *track=route::track_recording(plan,list,frame,true);require(track&&track->known[2],"draw-state fixture admission");
            track->known[2]=false;
            rejects([&]{route::record_effect(frame%32,stage,recipe_values[stage],bindings,handle.ptr,replaced);});
            require(route::draw_state_trace.latest().mask==2,"missing transition is distinguished");track->known[2]=true;
            const auto saved=track->states[2];track->states[2]=D3D12_RESOURCE_STATE_COPY_DEST;
            rejects([&]{route::record_effect(frame%32,stage,recipe_values[stage],bindings,handle.ptr,replaced);});
            require(route::draw_state_trace.latest().mask==4,"wrong state is distinguished");track->states[2]=saved;
            rejects([&]{route::record_effect(frame%32,stage,recipe_values[stage],bindings,handle.ptr+1,replaced);});
            require(route::draw_state_trace.latest().mask==64,"different output view is distinguished");
            std::ofstream report(out/"draw-state.json");route::draw_state_trace.write(report);report<<'\n';require(bool(report),"draw-state receipt saved");
            require(!replaced,"diagnostic guards issue no commands");
        }
        if(stage==3&&frame==2){
            const auto before=route::draws;
            rejects([&]{route::record_effect(frame%32,1,recipe_values[1],bindings,handle.ptr,replaced);});
            auto *track=route::track_recording(plan,f.list.Get(),frame,false);require(track!=nullptr,"fixture recording state");
            const auto original_state=track->states[2];track->states[2]=D3D12_RESOURCE_STATE_COPY_DEST;
            rejects([&]{route::record_effect(frame%32,stage,recipe_values[stage],bindings,handle.ptr,replaced);});track->states[2]=original_state;
            stream_plan_fixture::put(haze->graphics+0x40,haze->set);
            rejects([&]{route::record_effect(frame%32,stage,recipe_values[stage],bindings,handle.ptr,replaced);});
            stream_plan_fixture::put(haze->graphics+0x40,haze->color_owner);
            require(!replaced&&route::draws==before,"order, input state and stale bindings reject before commands");
        }
        if(split_overlay&&stage==4&&frame==1){
            auto *track=route::track_recording(plan,list,frame,true);
            require(track&&!track->known[2],"overlay has no local output-state transition");
            track->known[2]=true;track->states[2]=D3D12_RESOURCE_STATE_COPY_DEST;
            rejects([&]{route::record_effect(frame%32,stage,recipe_values[stage],bindings,handle.ptr,replaced);});
            require(route::draw_state_trace.latest().mask==4&&!replaced,"overlay rejects contradictory state before recording");
            track->known[2]=false;track->states[2]=0;
            rejects([&]{route::record_effect(frame%32,stage,recipe_values[stage],bindings,handle.ptr+1,replaced);});
            require(route::draw_state_trace.latest().mask==64&&!replaced,"overlay rejects mismatched native RTV before recording");
            std::ofstream receipt(out/"inherited-overlay-proof.json");
            receipt<<"{\"stage\":4,\"local_state_known\":false,\"native_draw_and_resources_unchanged\":true,\"earlier_gpu_list_leaves_render_target\":true}\n";
            require(bool(receipt),"inherited overlay proof saved");
        }
        require(route::record_effect(frame%32,stage,recipe_values[stage],bindings,handle.ptr,replaced)&&replaced,"production route draw");
        obs::Predicate after_draw;require(obs::predicate_current(list,after_draw)&&!after_draw.buffer,"effect restores disabled predicate");
        if(!(split_overlay&&(stage==0||(multi_overlay&&stage==4))))transition(outputs[stage].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    };
    const auto finish_recording=[&]{
    // Cleanup belongs to the last GPU recording, regardless of its CPU order.
    transition(outputs[2].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
    final_pixels.copy(current,outputs[2].Get(),false);
    transition(outputs[2].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    transition(source.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
    transition(destination.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
    for(unsigned ordinal=0;ordinal<route::effect_count(plan);++ordinal){const auto stage=route::effect_stage(plan,ordinal);
        if(stage<4&&(!aliased||stage!=3))transition(outputs[stage].Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);}
    };
    if(split_overlay){
        std::array<unsigned,4> order{0,1,2,3};for(unsigned n=0;n<(frame-1)%24;++n)std::next_permutation(order.begin(),order.end());
        for(auto index:order){if(index==0){record_resolve();require(record_copy()==-1,"source recording accepted");record_stage(0,0);}else if(index==1){record_stage(4,1);if(multi_overlay)record_stage(5,1);}else record_stage(index-1,index);if(index==3)finish_recording();}
    }else if(split_copy){
        std::array<unsigned,4> order{0,1,2,3};for(unsigned n=0;n<(frame-1)%24;++n)std::next_permutation(order.begin(),order.end());
        for(auto index:order){if(index==0)record_resolve();else if(index==1){require(record_copy()==-1,"copy-first recording accepted");record_stage(0,1);}else record_stage(index-1,index);if(index==3)finish_recording();}
    }else if(list_count==1){
        record_resolve();if(record_copy()==0)return 0;
        for(unsigned ordinal=0;ordinal<route::effect_count(plan);++ordinal)record_stage(route::effect_stage(plan,ordinal),0);
        finish_recording();
    }else{
        constexpr unsigned orders[6][3]{{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
        for(const auto index:orders[(frame-2)%6]){
            if(index==0){record_resolve();require(record_copy()==-1,"valid split source");record_stage(0,0);if(overlay_mode)record_stage(4,0);}
            else record_stage(index,index);
            if(index==2)finish_recording();
        }
    }
    require(route::complete==frame&&route::copies==frame&&route::draws==3*frame+(mixed?frame/2:0)+(overlay_mode?frame*plan.overlay_count:0),"complete recording route");
    route::copy_slot=UINT_MAX;
    D3D12_TEXTURE_COPY_LOCATION later_source{},later_destination{};later_source.pResource=source.Get();later_destination.pResource=destination.Get();
    require(!route::copy_override(f.list.Get(),&later_destination,0,0,0,&later_source,&box,0x259caf0)&&route::healthy(),"later unowned copies no longer overridden");
    std::array<ID3D12CommandList *,4> batch{};std::array<std::array<std::uint64_t,2>,4> identities{};
    for(unsigned i=0;i<list_count;++i){hr(commands[i]->Close(),"close");batch[i]=commands[i].Get();identities[i]={reinterpret_cast<std::uintptr_t>(commands[i].Get()),frame};}
#ifdef MHW_STREAM_RUNTIME_FIXTURE
    for(unsigned i=0;i<list_count;++i)cd::lists[i].closed=true;
#endif
    const auto queue_id=reinterpret_cast<std::uintptr_t>(f.queue.Get());
    if(list_count>=3){auto reversed=identities;std::swap(reversed[0],reversed[2]);
        require(!route::validate_submission(queue_id,std::span(reversed).first(list_count),f.sequence+1),"reverse GPU route rejected before execution");}
    for(unsigned first=0;first<list_count;){
        const auto count=list_count>=3&&frame%2?1U:list_count;
        const auto ids=std::span(identities).subspan(first,count);
        require(route::validate_submission(queue_id,ids,f.sequence+1),"GPU route validated before execution");
#ifdef MHW_STREAM_RUNTIME_FIXTURE
        if(mode=="unclosed"){
            // The real list is closed so forwarding is legal. Remove only the
            // observed receipt and require rejection before any owned signal.
            cd::lists[0].closed=false;const auto before=cd::signal_value;
            cd::execute_hook(f.queue.Get(),count,batch.data()+first);
            require(cd::fault==19&&!route::healthy()&&cd::signal_value==before,"missing Close receipt rejects route-only submission");
            f.wait();require(obs::finish(),"rejected submission observer cleanup");f.messages();
            std::cout<<"PASS missing Close receipt rejected before ownership submission\n";return 0;
        }
        cd::execute_hook(f.queue.Get(),count,batch.data()+first);
        require(cd::healthy()&&cd::signal_value==f.sequence+1,"actual runtime route-only/SDK submission signal");
        f.sequence=cd::signal_value;const auto event=CreateEventW(nullptr,FALSE,FALSE,nullptr);require(event!=nullptr,"runtime completion event");
        hr(f.fence->SetEventOnCompletion(f.sequence,event),"runtime completion receipt");const auto wait=WaitForSingleObject(event,30000);CloseHandle(event);require(wait==WAIT_OBJECT_0,"runtime GPU completion");
        require(!cd::lease.list,"private lease retires at its submission, before later consumers");first+=count;
#else
        f.queue->ExecuteCommandLists(count,batch.data()+first);
        route::observe_submission(queue_id,GetTickCount64(),ids);f.wait();
        require(route::submitted_batch(queue_id,ids,f.sequence),"submission completes frame ownership");first+=count;
#endif
    }
    route::collect(f.fence->GetCompletedValue());require(route::lifetime.live()==list_count,"GPU completion alone retains recording");
    const auto transfer_pixels=copied.read();
    if(native_choice){for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x)require(transfer_pixels[y*128+x]==(y<32&&x<64?pixels[y*128+x]:sentinel[y*128+x]),"native copy extent and untouched padding exact");}
    else require(transfer_pixels==pixels,"all expanded-copy pixels exact");
    const auto actual_pixels=final_pixels.read();
    if(frame==1){baseline_pixels=actual_pixels;auto unique=actual_pixels;std::sort(unique.begin(),unique.end());unique.erase(std::unique(unique.begin(),unique.end()),unique.end());
        require(unique.size()>32,"final image contains meaningful nonconstant signal");std::cout<<"Final reference has "<<unique.size()<<" distinct pixels\n";}
    if(reordered&&actual_pixels!=baseline_pixels){unsigned differing{};for(unsigned i=0;i<actual_pixels.size();++i)if(actual_pixels[i]!=baseline_pixels[i])++differing;
        std::cerr<<"Frame "<<frame<<": "<<differing<<" final pixels differ\n";f.messages();}
    if(reordered)require(actual_pixels==baseline_pixels,"final output identical across CPU orders and split GPU batches");
    for(unsigned i=0;i<list_count;++i){hr(allocators[i]->Reset(),"retire allocator");hr(commands[i]->Reset(allocators[i].Get(),nullptr),"retire list");hr(commands[i]->Close(),"retire close");
#ifdef MHW_STREAM_RUNTIME_FIXTURE
        require(cd::recording_pins.reset(commands[i].Get(),frame),"runtime reset receipt");
#endif
        require(route::reset(commands[i].Get(),frame),"successful native reset");}
#ifdef MHW_STREAM_RUNTIME_FIXTURE
    cd::recording_pins.collect(f.fence->GetCompletedValue());require(!cd::recording_pins.live(),"runtime private resources retired");
#endif
    route::collect(f.fence->GetCompletedValue());require(!route::lifetime.live(),"completed and retired resources released");
    // Rotate native vertex objects beyond the original effect's 16-use cap.
    for(unsigned stage=0;stage<stages;++stage){if(overlay_mode&&stage==3)continue;vb[stage]=make_vertex_buffer(stage);}
    }
    if(padded_overlay)require(route::overlay_extended_draws==(multi_overlay?192U:96U)&&route::overlay_extended_layout==std::array<unsigned,6>{28,2,2,0,16,20},"padded overlay draw receipts");
    if(multi_overlay)require(route::overlay_shared_prepared==96&&route::overlay_shared_completed==96,"same-key overlay frame accounting");
    const auto expected_recordings=four_lists?384U:reordered?286U:96U;
    require(route::lifetime.retained()==expected_recordings&&route::lifetime.released()==expected_recordings&&route::lifetime.peak()==(four_lists?4U:reordered?3U:1U),"bounded 96-frame lifetime");
    if(reordered)require(route::early_draws>0&&route::multilist_frames==(four_lists?96U:95U)&&route::submitted_frames==96,"early worker recording covered");
    if(unscoped)require(route::state_unscoped>0&&route::state_imports>0,"unscoped transitions imported within their exact recording");
    for(auto &cache:route::pipelines)if(cache.effect)cache.effect->retire(f.fence.Get(),f.sequence,true);
    f.messages();require(obs::finish(),"observer cleanup");
    std::ofstream log(out/"route.json");log<<std::boolalpha;route::write(log);require(bool(log),"route report");
    std::cout<<"PASS 96 continuous frames, "<<route::draws<<" native effects, 786432 exact pixels, changing vertex resources, GPU validation and retirement\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
