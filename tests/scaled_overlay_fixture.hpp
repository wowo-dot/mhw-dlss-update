#pragma once
namespace overlay_fixture {
using namespace mhw::fixtures::effects;
void run(Fixture &f,std::span<const std::byte> vs,std::span<const std::byte> ps,bool padded=false){
    namespace obs=mhw::temporal_present::observer;
    require(sf::overlay_pixel_shader(ps),"captured UV-only shader admitted");
    const auto compile=[](const char *source){ComPtr<ID3DBlob> b,e;hr(D3DCompile(source,std::strlen(source),nullptr,nullptr,nullptr,"main","ps_5_0",0,0,&b,&e),"contract shader");return b;};
    for(const char *source:{"float4 main(float4 p:SV_Position):SV_Target{return p;}","float4 main(float2 u:TEXCOORD1):SV_Target{return float4(u,0,1);}","float main(float2 u:TEXCOORD0):SV_Depth{return u.x;}"}){
        auto b=compile(source);require(!sf::overlay_pixel_shader({static_cast<const std::byte *>(b->GetBufferPointer()),b->GetBufferSize()}),"screen-position/other semantic/output rejected");}
    const sf::FilterGeometry g{64,32,128,64};auto vertices=mhw::fixtures::effects::vertices(sf::EffectStage::motion_filter,g,false);
    require(sf::overlay_coverage(std::span(vertices.values).first(16)),"native full-clip coverage");
    auto bad=vertices;bad.values[0]=0;require(!sf::overlay_coverage(std::span(bad.values).first(16)),"partial overlay refused");
    auto r=recipe(sf::EffectStage::motion_filter,g,f.root.Get(),vs,ps,std::span(vertices.values).first(16));
    r.blend.RenderTarget[0].BlendEnable=TRUE;r.blend.RenderTarget[0].SrcBlend=D3D12_BLEND_DEST_COLOR;
    const std::array<D3D12_INPUT_ELEMENT_DESC,2> padded_elements{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TexCoord",0,DXGI_FORMAT_R32G32_FLOAT,0,20,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
    if(padded)r.input_layout=padded_elements;
    auto pso=pipeline(f.device.Get(),r,ps);auto vb=buffer(f.device.Get(),padded?168:64,D3D12_HEAP_TYPE_UPLOAD);
    std::array<std::byte,168> raw{};
    if(padded){for(unsigned i=0;i<4;++i){std::memcpy(raw.data()+(i+2)*28,vertices.values.data()+4*i,8);std::memcpy(raw.data()+(i+2)*28+20,vertices.values.data()+4*i+2,8);}upload(vb.Get(),raw);}
    else upload(vb.Get(),std::as_bytes(std::span(vertices.values)));
    std::array<float,256> constants{};constants[79]=32;constants[80]=1.F/64;constants[192]=.1F;constants[193]=1.5F;constants[194]=.4F;constants[196]=.4F;constants[197]=.5F;constants[198]=.6F;
    auto cb=buffer(f.device.Get(),sizeof(constants),D3D12_HEAP_TYPE_UPLOAD);upload(cb.Get(),std::as_bytes(std::span(constants)));
    auto td=texture(128,64,DXGI_FORMAT_R11G11B10_FLOAT);td.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    auto target=resource(f.device.Get(),td,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_RENDER_TARGET);Pixels pixels(f.device.Get(),td,D3D12_HEAP_TYPE_READBACK);
    ComPtr<ID3D12DescriptorHeap> heap;D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=1;hr(f.device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"overlay rtv");
    const auto handle=heap->GetCPUDescriptorHandleForHeapStart();f.device->CreateRenderTargetView(target.Get(),nullptr,handle);
    std::vector<unsigned> reference;
    for(unsigned pass=0;pass<3;++pass){
        hr(f.allocator->Reset(),"overlay allocator");hr(f.list->Reset(f.allocator.Get(),nullptr),"overlay list");obs::reset(f.list.Get(),pass+1,nullptr);
        auto *list=f.list.Get();list->SetGraphicsRootSignature(f.root.Get());list->SetPipelineState(pso.Get());for(unsigned i=0;i<4;++i)list->SetGraphicsRootConstantBufferView(i,cb->GetGPUVirtualAddress()+256*i);
        list->OMSetRenderTargets(1,&handle,FALSE,nullptr);const float clear[]{64,32,16,1};list->ClearRenderTargetView(handle,clear,0,nullptr);
        const D3D12_VERTEX_BUFFER_VIEW view{vb->GetGPUVirtualAddress(),padded?168U:64U,padded?28U:16U};list->IASetPrimitiveTopology(vertices.topology);list->IASetVertexBuffers(0,1,&view);
        const D3D12_VIEWPORT viewport{0,0,pass==0?128.F:64.F,pass==0?64.F:32.F,0,1};const D3D12_RECT scissor{0,0,pass==0?128:64,pass==0?64:32};list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);
        sf::EffectBindings b{list,pso.Get(),f.root.Get(),target.Get(),vb.Get(),view,viewport,scissor,vertices.topology,4,padded?2U:0U};
        if(pass==1){require(sf::overlay_raster(b,g),"overlay extent admission");sf::record_overlay(b,g);obs::RasterBindings actual;require(obs::raster_current(list,actual)&&!std::memcmp(&actual.viewport,&viewport,sizeof(viewport))&&!std::memcmp(&actual.scissor,&scissor,sizeof(scissor)),"native raster restored");}
        else list->DrawInstanced(4,1,padded?2:0,0);
        barrier(list,target.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);pixels.copy(list,target.Get(),false);barrier(list,target.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
        hr(list->Close(),"overlay close");ID3D12CommandList *batch[]{list};f.queue->ExecuteCommandLists(1,batch);f.wait();auto actual=pixels.read();
        if(pass==0)reference=actual;else if(pass==1)require(actual==reference,"expanded overlay equals independent full-size native draw at every pixel");else require(actual!=reference,"quarter-sized control reproduces the sizing error");
    }
    f.messages();std::cout<<"PASS captured overlay, full-size blend equality, quarter-size negative control and raster restoration\n";
}
}

