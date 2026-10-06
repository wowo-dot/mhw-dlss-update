#include "scaled_effect_fixture.hpp"
#include "../src/scaled_temporal/output_route.hpp"
#include "../src/temporal_bindings/submission.hpp"
#include <sstream>
namespace fx=mhw::fixtures::effects;
namespace route=mhw::scaled_temporal::output_route;
namespace obs=mhw::temporal_present::observer;
namespace {
unsigned markers{},ended{};bool marker_open{};
}
// The production journal's exclusion/retirement protocol is exercised by
// jowned_inline. Here the real output route and GPU are isolated from game ABI.
namespace mhw::temporal_bindings::submission {
unsigned followup_begin(unsigned i,ID3D12GraphicsCommandList *list,std::uint64_t g) noexcept {
    temporal_present::Bindings s;if(i!=2||marker_open||!temporal_present::observer::inspect(list,s)||s.generation!=g)return UINT_MAX;
    marker_open=true;return markers++;
}
void native_copy_end(unsigned i,bool ok) noexcept {fx::require(marker_open&&ok&&i==ended,"route marker closure");marker_open=false;++ended;}
}
template<class T,std::size_t N>void put(std::array<std::byte,N> &a,unsigned offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
void WINAPI copy_forward(ID3D12GraphicsCommandList *l,const D3D12_TEXTURE_COPY_LOCATION *d,UINT x,UINT y,UINT z,const D3D12_TEXTURE_COPY_LOCATION *s,const D3D12_BOX *box){l->CopyTextureRegion(d,x,y,z,s,box);}
int wmain(int argc,wchar_t **argv){try{
    using namespace fx;require(argc==6,"usage: warp|hardware shader-prefix output-dir case luid-low");
    const bool warp=std::wstring_view(argv[1])==L"warp";const std::string mode=std::filesystem::path(argv[4]).string();
    const std::filesystem::path out=argv[3];require(!std::filesystem::exists(out),"preserve output");std::filesystem::create_directories(out);
    Fixture f(warp,LUID{std::stoul(argv[5]),0},argv[2]);require(MH_Initialize()==MH_OK,"hook init");std::string reason;
    const auto resolver=[](std::uintptr_t p,unsigned slot,std::uintptr_t &target)noexcept{target=(*reinterpret_cast<std::uintptr_t **>(p))[slot];return target!=0;};
    require(obs::install(f.device.Get(),resolver,reason,true,ViewportFeed::observe),reason.c_str());obs::begin();
    ViewportFeed::expected=f.list.Get();
    route::fixture_start(f.device.Get(),reinterpret_cast<void *>(copy_forward));
    const sf::FilterGeometry g{64,32,128,64};
    std::array<sf::EffectVertices,3> v;std::array<sf::EffectRecipe,3> recipe_values;
    std::array<ComPtr<ID3D12PipelineState>,3> native;std::array<ComPtr<ID3D12Resource>,3> vb,outputs,cb;
    std::array<ComPtr<ID3D12DescriptorHeap>,3> srv,rtv;
    auto td=texture(128,64,DXGI_FORMAT_R11G11B10_FLOAT);td.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    auto source=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);
    auto destination=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
    Pixels input(f.device.Get(),td,D3D12_HEAP_TYPE_UPLOAD),copied(f.device.Get(),td,D3D12_HEAP_TYPE_READBACK);
    std::vector<unsigned> pixels(128*64);for(unsigned i=0;i<pixels.size();++i)pixels[i]=0x781e03c0U+(i%31);input.write(std::as_bytes(std::span(pixels)));
    mhw::temporal_bindings::Snapshot snapshot;snapshot.frame=3;snapshot.rectangle={0,0,64,32};
    for(unsigned stage=0;stage<3;++stage){
        v[stage]=vertices(static_cast<sf::EffectStage>(stage),g,false);
        auto &r=recipe_values[stage];r=recipe(static_cast<sf::EffectStage>(stage),g,f.root.Get(),f.vs[stage],f.ps[stage],std::span(v[stage].values).first(v[stage].count*4));
        native[stage]=pipeline(f.device.Get(),r,r.pixel_shader);vb[stage]=buffer(f.device.Get(),64,D3D12_HEAP_TYPE_UPLOAD);upload(vb[stage].Get(),std::as_bytes(std::span(v[stage].values)));
        outputs[stage]=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
        D3D12_DESCRIPTOR_HEAP_DESC h{};h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;h.NumDescriptors=1;hr(f.device->CreateDescriptorHeap(&h,IID_PPV_ARGS(&rtv[stage])),"RTV");f.device->CreateRenderTargetView(outputs[stage].Get(),nullptr,rtv[stage]->GetCPUDescriptorHandleForHeapStart());
        h.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;h.NumDescriptors=5;h.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;hr(f.device->CreateDescriptorHeap(&h,IID_PPV_ARGS(&srv[stage])),"SRV");
        const auto step=f.device->GetDescriptorHandleIncrementSize(h.Type);
        constexpr DXGI_FORMAT formats[]{DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R32_UINT,DXGI_FORMAT_R32G32_FLOAT,DXGI_FORMAT_R32G32B32A32_FLOAT};
        for(unsigned i=0;i<5;++i){D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=formats[i];d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Texture2D.MipLevels=1;
            if(stage==1&&i==1){d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE3D;d.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;d.Texture3D.MipLevels=1;}
            f.device->CreateShaderResourceView(i?nullptr:stage?outputs[stage-1].Get():destination.Get(),&d,{srv[stage]->GetCPUDescriptorHandleForHeapStart().ptr+i*SIZE_T(step)});}
        std::array<float,256> c{};if(stage==0){c[42]=c[46]=1;c[68]=128;c[69]=64;c[70]=1.F/128;c[71]=1.F/64;c[78]=64;c[79]=32;c[195]=.125F;c[196]=c[197]=1;c[198]=.01F;c[200]=63.5F/128;c[201]=31.5F/64;const unsigned n=8;std::memcpy(c.data()+194,&n,4);}
        if(stage==1){const unsigned n=1;std::memcpy(c.data()+192,&n,4);}cb[stage]=buffer(f.device.Get(),sizeof(c),D3D12_HEAP_TYPE_UPLOAD);upload(cb[stage].Get(),std::as_bytes(std::span(c)));
        bool replaced=true;require(route::draw(stage,r,{},0,replaced)&&!replaced,"warmup preparation never draws");
        mhw::temporal_bindings::Owner owner;owner.resource=reinterpret_cast<std::uintptr_t>(outputs[stage].Get());owner.width=128;owner.height=64;
        require(route::publish_draw(2,stage,snapshot,owner),"publish stage");
    }
    std::array<std::byte,80> packet{};std::array<std::byte,0x160> recording{};
    put(recording,0x158,reinterpret_cast<std::uintptr_t>(packet.data()));put(recording,0x20,reinterpret_cast<std::uintptr_t>(f.list.Get()));
    require(route::publish_copy(2,snapshot,reinterpret_cast<std::uintptr_t>(packet.data()),source.Get(),destination.Get()),"publish copy");
    hr(f.allocator->Reset(),"allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"list");obs::reset(f.list.Get(),1,nullptr);
    require(!route::ready(f.list.Get(),1,4,reinterpret_cast<std::uintptr_t>(recording.data())),"reject foreign frame");
    require(route::ready(f.list.Get(),mode=="generation"?2:1,3,reinterpret_cast<std::uintptr_t>(recording.data())),"claim frame");route::resolved(true);
    input.copy(f.list.Get(),source.Get(),true);
    const auto transition=[&](ID3D12Resource *r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){D3D12_RESOURCE_BARRIER b{};b.Transition={r,UINT_MAX,before,after};f.list->ResourceBarrier(1,&b);if(mode!="states")route::barriers(f.list.Get(),1,&b);};
    transition(source.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);transition(destination.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
    if(mode=="packet")packet[16]=std::byte{1};
    const D3D12_BOX box{0,0,0,mode=="box"?63U:64U,32,1};const bool expanded=route::fixture_copy(f.list.Get(),source.Get(),destination.Get(),box,mode!="caller");
    if(mode!="success"&&mode!="duplicate"){
        require(!expanded&&!route::healthy()&&markers==0,"invalid copy rejected before recording");hr(f.list->Close(),"discard close");hr(f.list->Reset(f.allocator.Get(),nullptr),"discard reset");hr(f.list->Close(),"discarded");
        require(!route::finish(nullptr),"failed route cannot claim retirement");require(obs::finish(),"observer cleanup");f.messages();std::cout<<"PASS rejection "<<mode<<'\n';return 0;
    }
    require(expanded&&markers==1,"expanded copy");
    if(mode=="duplicate"){
        require(!route::fixture_copy(f.list.Get(),source.Get(),destination.Get(),box,true)&&!route::healthy()&&markers==1&&ended==1,"duplicate before completed chain is rejected");
        hr(f.list->Close(),"discard duplicate close");hr(f.list->Reset(f.allocator.Get(),nullptr),"discard duplicate reset");hr(f.list->Close(),"discarded duplicate");
        require(!route::finish(nullptr),"duplicate cannot claim retirement");require(obs::finish(),"duplicate observer cleanup");f.messages();std::cout<<"PASS rejection duplicate\n";return 0;
    }
    transition(destination.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);copied.copy(f.list.Get(),destination.Get(),false);transition(destination.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    for(unsigned stage=0;stage<3;++stage){
        transition(outputs[stage].Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_RENDER_TARGET);
        auto *list=f.list.Get();ID3D12DescriptorHeap *heap[]{srv[stage].Get()};list->SetDescriptorHeaps(1,heap);list->SetGraphicsRootSignature(f.root.Get());list->SetPipelineState(native[stage].Get());
        for(unsigned i=0;i<4;++i)list->SetGraphicsRootConstantBufferView(i,cb[stage]->GetGPUVirtualAddress()+256*i);list->SetGraphicsRootDescriptorTable(4,srv[stage]->GetGPUDescriptorHandleForHeapStart());
        const auto handle=rtv[stage]->GetCPUDescriptorHandleForHeapStart();list->OMSetRenderTargets(1,&handle,FALSE,nullptr);
        const D3D12_VERTEX_BUFFER_VIEW view{vb[stage]->GetGPUVirtualAddress(),64,16};list->IASetPrimitiveTopology(v[stage].topology);list->IASetVertexBuffers(0,1,&view);
        const D3D12_VIEWPORT viewport{0,0,stage==2?128.F:64.F,stage==2?64.F:32.F,0,1};const D3D12_RECT scissor{0,0,stage==2?128:64,stage==2?64:32};list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);
        sf::EffectBindings bindings{list,native[stage].Get(),f.root.Get(),outputs[stage].Get(),vb[stage].Get(),view,viewport,scissor,v[stage].topology,v[stage].count,0};
        bool replaced=false;require(route::draw(6+stage,recipe_values[stage],bindings,handle.ptr,replaced)&&replaced,"production route draw");
        transition(outputs[stage].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    require(route::complete()&&markers==4&&ended==4&&!marker_open,"complete recording route");
    const auto native_late_copy=[&]{
        require(!route::fixture_copy(f.list.Get(),source.Get(),destination.Get(),box,true),"completed route must forward later copy");
        require(route::complete()&&markers==4&&ended==4&&!marker_open,"completed route must not fault on later copy");
        D3D12_TEXTURE_COPY_LOCATION s{},d{};s.pResource=source.Get();s.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;d=s;d.pResource=destination.Get();
        copy_forward(f.list.Get(),&d,0,0,0,&s,&box);
    };
    // Same resources can be copied again after the selected four operations,
    // before Close as well as after Reset. Neither copy belongs to the probe.
    transition(destination.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
    native_late_copy();transition(destination.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if(ViewportFeed::calls!=9)std::cerr<<"viewport listener calls="<<ViewportFeed::calls<<'\n';
    require(ViewportFeed::calls==9,"one listener feed per native/expanded/restored viewport");require(!route::finish(nullptr),"no premature retirement");
    hr(f.list->Close(),"close");ID3D12CommandList *lists[]{f.list.Get()};f.queue->ExecuteCommandLists(1,lists);f.wait();require(copied.read()==pixels,"all 8192 expanded-copy pixels exact");
    hr(f.allocator->Reset(),"reuse allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"reuse list");obs::reset(f.list.Get(),2,nullptr);
    Pixels overwrite(f.device.Get(),td,D3D12_HEAP_TYPE_UPLOAD);std::vector<unsigned> zeros(pixels.size());overwrite.write(std::as_bytes(std::span(zeros)));
    transition(destination.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);overwrite.copy(f.list.Get(),destination.Get(),true);
    native_late_copy();transition(destination.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);copied.copy(f.list.Get(),destination.Get(),false);
    hr(f.list->Close(),"reuse close");f.queue->ExecuteCommandLists(1,lists);f.wait();
    auto expected=zeros;for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)expected[y*128+x]=pixels[y*128+x];
    require(copied.read()==expected,"later native copy preserves half-size box and untouched pixels");
    hr(f.allocator->Reset(),"retire allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"retire list");hr(f.list->Close(),"retire close");require(route::finish(f.fence.Get()),"retire production route");f.messages();require(obs::finish(),"observer cleanup");
    std::ofstream log(out/"route.json");log<<std::boolalpha;route::write(log);require(bool(log),"route report");std::cout<<"PASS production copy and three draws, 8192 exact copy pixels, clean GPU validation and retirement\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
